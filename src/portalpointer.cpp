#include "portalpointer.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QRandomGenerator>
#include <QVariant>

namespace {
constexpr auto portalService="org.freedesktop.portal.Desktop";
constexpr auto portalPath="/org/freedesktop/portal/desktop";
constexpr auto remoteDesktopInterface="org.freedesktop.portal.RemoteDesktop";
constexpr auto requestInterface="org.freedesktop.portal.Request";
constexpr auto sessionInterface="org.freedesktop.portal.Session";

QString token(){
    return QString::number(QRandomGenerator::global()->generate64(),16);
}

QVariant portalValue(const QVariantMap &results,const QString &key){
    QVariant value=results.value(key);
    while(value.metaType().id()==qMetaTypeId<QDBusVariant>())
        value=value.value<QDBusVariant>().variant();
    return value;
}

QDBusMessage remoteDesktopCall(const QString &session,const QString &method,
                              const QVariantList &arguments){
    // RemoteDesktop methods live on the portal object; the session path is
    // their first argument, not the D-Bus object path receiving the call.
    QDBusMessage call=QDBusMessage::createMethodCall(
        portalService,portalPath,remoteDesktopInterface,method);
    QVariantList fullArguments{QDBusObjectPath(session)};
    fullArguments.append(arguments);
    call.setArguments(fullArguments);
    return call;
}

class PortalResponse : public QObject {
    Q_OBJECT
public:
    std::function<void(uint,const QVariantMap &)> callback;
    explicit PortalResponse(std::function<void(uint,const QVariantMap &)> callback,
                            QObject *parent=nullptr)
        : QObject(parent),callback(std::move(callback)) {}
public slots:
    void response(uint code,const QVariantMap &results){
        auto done=std::move(callback);
        if(done)done(code,results);
        deleteLater();
    }
};
}

PortalPointer::PortalPointer(QString restoreToken,QObject *parent)
    : QObject(parent),savedRestoreToken(std::move(restoreToken)) {}

PortalPointer::~PortalPointer(){closeSession();}

void PortalPointer::setStatus(const QString &status){
    if(currentStatus==status)return;
    currentStatus=status;
    if(statusChanged)statusChanged(currentStatus);
}

void PortalPointer::callRequest(const QString &method,const QVariantList &arguments,
                                std::function<void(uint,const QVariantMap &)> response){
    const QString requestToken=arguments.isEmpty()?QString():arguments.last().toMap().value("handle_token").toString();
    QString sender=QDBusConnection::sessionBus().baseService();
    if(sender.startsWith(':'))sender.remove(0,1);
    sender.replace(':','_');sender.replace('.','_');
    const QString requestPath=QString("%1/request/%2/%3").arg(portalPath,sender,requestToken);
    auto *signalReceiver=new PortalResponse(std::move(response),this);
    const bool connected=QDBusConnection::sessionBus().connect(
        portalService,requestPath,requestInterface,"Response",signalReceiver,
        SLOT(response(uint,QVariantMap)));
    if(!connected){
        signalReceiver->deleteLater();
        isRequesting=false;
        setStatus("Could not monitor KDE pointer permission response");
        return;
    }
    QDBusMessage call=QDBusMessage::createMethodCall(
        portalService,portalPath,remoteDesktopInterface,method);
    call.setArguments(arguments);
    auto *watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher,signalReceiver]{
        QDBusPendingReply<QDBusObjectPath> reply=*watcher;
        watcher->deleteLater();
        if(reply.isError()){
            signalReceiver->deleteLater();
            isRequesting=false;
            setStatus(QString("Pointer permission unavailable: %1").arg(reply.error().message()));
        }
    });
}

void PortalPointer::requestAccess(){
    if(isActive){setStatus("Controller mouse ready · hold LT to use");return;}
    if(isRequesting)return;
    if(!QDBusConnection::sessionBus().isConnected()){
        setStatus("KDE desktop session bus is unavailable");return;
    }
    isRequesting=true;
    setStatus("Requesting KDE pointer permission…");
    createSession();
}

void PortalPointer::createSession(){
    QVariantMap options;
    options.insert("handle_token",token());
    options.insert("session_handle_token",token());
    callRequest("CreateSession",{options},[this](uint code,const QVariantMap &results){
        if(code!=0){
            isRequesting=false;
            setStatus("KDE did not create a pointer session");
            return;
        }
        const QVariant handle=portalValue(results,"session_handle");
        if(handle.canConvert<QDBusObjectPath>())sessionHandle=handle.value<QDBusObjectPath>().path();
        else sessionHandle=handle.toString();
        if(sessionHandle.isEmpty()){
            isRequesting=false;
            setStatus("KDE returned an invalid pointer session");
            return;
        }
        selectPointer();
    });
}

void PortalPointer::selectPointer(){
    QVariantMap options;
    options.insert("handle_token",token());
    options.insert("types",uint(2)); // POINTER only
    options.insert("persist_mode",uint(2)); // Ask the portal to remember consent
    if(!savedRestoreToken.isEmpty())options.insert("restore_token",savedRestoreToken);
    callRequest("SelectDevices",{QDBusObjectPath(sessionHandle),options},
                [this](uint code,const QVariantMap &){
        if(code!=0){
            isRequesting=false;
            setStatus("KDE pointer access was not granted");
            closeSession();
            return;
        }
        startSession();
    });
}

void PortalPointer::startSession(){
    QVariantMap options;
    options.insert("handle_token",token());
    callRequest("Start",{QDBusObjectPath(sessionHandle),QString(),options},
                [this](uint code,const QVariantMap &results){
        isRequesting=false;
        if(code!=0 || !(portalValue(results,"devices").toUInt()&uint(2))){
            setStatus("KDE did not grant pointer control");
            closeSession();
            return;
        }
        isActive=true;
        const QString restore=portalValue(results,"restore_token").toString();
        if(!restore.isEmpty()){
            savedRestoreToken=restore;
            if(restoreTokenChanged)restoreTokenChanged(restore);
        }
        setStatus("Controller mouse ready · hold LT to use");
    });
}

void PortalPointer::move(double dx,double dy){
    if(!isActive || (dx==0.0&&dy==0.0))return;
    const QDBusMessage call=remoteDesktopCall(sessionHandle,"NotifyPointerMotion",
                                               {QVariantMap{},dx,dy});
    auto *watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{
        QDBusPendingReply<> reply=*watcher;
        if(reply.isError()&&isActive){
            isActive=false;
            setStatus(QString("KDE rejected pointer movement: %1").arg(reply.error().message()));
        }
        watcher->deleteLater();
    });
}

void PortalPointer::button(bool left,bool pressed){
    if(!isActive)return;
    const QDBusMessage call=remoteDesktopCall(sessionHandle,"NotifyPointerButton",
                                               {QVariantMap{},int(left?0x110:0x111),uint(pressed?1:0)});
    QDBusConnection::sessionBus().asyncCall(call);
}

void PortalPointer::scroll(int steps){
    if(!isActive || steps==0)return;
    const QDBusMessage call=remoteDesktopCall(sessionHandle,"NotifyPointerAxisDiscrete",
                                               {QVariantMap{},uint(0),int(steps)});
    QDBusConnection::sessionBus().asyncCall(call);
}

void PortalPointer::closeSession(){
    if(sessionHandle.isEmpty())return;
    QDBusMessage call=QDBusMessage::createMethodCall(
        portalService,sessionHandle,sessionInterface,"Close");
    QDBusConnection::sessionBus().asyncCall(call);
    sessionHandle.clear();
    isActive=false;
}

#include "portalpointer.moc"
