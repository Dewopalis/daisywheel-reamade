#include "portalpointer.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QPointer>
#include <QRandomGenerator>
#include <QTimer>
#include <QVariant>

namespace {
constexpr auto portalService="org.freedesktop.portal.Desktop";
constexpr auto portalPath="/org/freedesktop/portal/desktop";
constexpr auto remoteDesktopInterface="org.freedesktop.portal.RemoteDesktop";
constexpr auto requestInterface="org.freedesktop.portal.Request";
constexpr auto sessionInterface="org.freedesktop.portal.Session";
constexpr int requestTimeoutMs=60000;
constexpr int pointerEventTimeoutMs=5000;
constexpr double maxQueuedMotion=300.0;
constexpr int maxQueuedScrollSteps=12;

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
    std::function<void(const QString &)> completed;
    QTimer *timeout=nullptr;
    QString requestPath;
    explicit PortalResponse(std::function<void(uint,const QVariantMap &)> callback,
                            QObject *parent=nullptr)
        : QObject(parent),callback(std::move(callback)) {}
public slots:
    void response(uint code,const QVariantMap &results){
        if(timeout)timeout->stop();
        if(completed)completed(requestPath);
        auto done=std::move(callback);
        if(done)done(code,results);
        deleteLater();
    }
};

class PortalSessionClosed : public QObject {
    Q_OBJECT
public:
    std::function<void()> callback;
    explicit PortalSessionClosed(std::function<void()> callback,QObject *parent=nullptr)
        : QObject(parent),callback(std::move(callback)) {}
public slots:
    void closed(const QVariantMap &details){
        Q_UNUSED(details);
        auto done=std::move(callback);
        if(done)done();
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

void PortalPointer::callRequest(quint64 generation,const QString &method,
                                const QVariantList &arguments,
                                std::function<void(uint,const QVariantMap &)> response){
    const QString requestToken=arguments.isEmpty()?QString():arguments.last().toMap().value("handle_token").toString();
    QString sender=QDBusConnection::sessionBus().baseService();
    if(sender.startsWith(':'))sender.remove(0,1);
    sender.replace(':','_');sender.replace('.','_');
    const QString requestPath=QString("%1/request/%2/%3").arg(portalPath,sender,requestToken);
    auto *signalReceiver=new PortalResponse(
        [this,generation,response=std::move(response)](uint code,const QVariantMap &results){
            if(generation==sessionGeneration&&response)response(code,results);
        },this);
    signalReceiver->requestPath=requestPath;
    signalReceiver->completed=[this](const QString &path){pendingRequests.remove(path);};
    // The portal Response signal has its own signature, so retain its receiver
    // connection until it fires or its timeout cancels the request.
    const bool connected=QDBusConnection::sessionBus().connect(
        portalService,requestPath,requestInterface,"Response",signalReceiver,
        SLOT(response(uint,QVariantMap)));
    if(!connected){
        signalReceiver->deleteLater();
        if(generation==sessionGeneration){
            isRequesting=false;
            shouldRecover=true;
            setStatus("Could not monitor KDE pointer permission response · recovering while LT is held");
            closeSession();
        }
        return;
    }
    pendingRequests.insert(requestPath,signalReceiver);

    auto *timeout=new QTimer(signalReceiver);
    signalReceiver->timeout=timeout;
    timeout->setSingleShot(true);
    timeout->setInterval(requestTimeoutMs);
    connect(timeout,&QTimer::timeout,this,[this,generation,signalReceiver]{
        const QString requestPath=signalReceiver->requestPath;
        pendingRequests.remove(requestPath);
        QDBusConnection::sessionBus().disconnect(
            portalService,requestPath,requestInterface,"Response",signalReceiver,
            SLOT(response(uint,QVariantMap)));
        signalReceiver->callback={};
        signalReceiver->deleteLater();
        if(generation!=sessionGeneration)return;
        QDBusMessage cancel=QDBusMessage::createMethodCall(
            portalService,requestPath,requestInterface,"Close");
        QDBusConnection::sessionBus().asyncCall(cancel);
        isRequesting=false;
        setStatus("KDE pointer permission request timed out · release LT and retry");
        closeSession();
    });
    timeout->start();

    QDBusMessage call=QDBusMessage::createMethodCall(
        portalService,portalPath,remoteDesktopInterface,method);
    call.setArguments(arguments);
    auto *watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call),this);
    QPointer<PortalResponse> receiverGuard(signalReceiver);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,generation,watcher,receiverGuard,requestPath]{
        QDBusPendingReply<QDBusObjectPath> reply=*watcher;
        watcher->deleteLater();
        if(!reply.isError()){
            if(!receiverGuard||!receiverGuard->callback)return;
            const QString actualPath=reply.value().path();
            if(actualPath.isEmpty()||actualPath==receiverGuard->requestPath)return;
            const QString oldPath=receiverGuard->requestPath;
            const bool connected=QDBusConnection::sessionBus().connect(
                portalService,actualPath,requestInterface,"Response",receiverGuard,
                SLOT(response(uint,QVariantMap)));
            if(connected){
                QDBusConnection::sessionBus().disconnect(
                    portalService,oldPath,requestInterface,"Response",receiverGuard,
                    SLOT(response(uint,QVariantMap)));
                pendingRequests.remove(oldPath);
                receiverGuard->requestPath=actualPath;
                pendingRequests.insert(actualPath,receiverGuard);
            }else if(generation==sessionGeneration){
                receiverGuard->timeout->stop();
                receiverGuard->callback={};
                receiverGuard->deleteLater();
                isRequesting=false;
                shouldRecover=true;
                setStatus("Could not monitor KDE pointer permission response · recovering while LT is held");
                closeSession();
            }
            return;
        }
        if(receiverGuard){
            if(receiverGuard->timeout)receiverGuard->timeout->stop();
            QDBusConnection::sessionBus().disconnect(
                portalService,requestPath,requestInterface,"Response",receiverGuard,
                SLOT(response(uint,QVariantMap)));
            receiverGuard->callback={};
            receiverGuard->deleteLater();
        }
        pendingRequests.remove(requestPath);
        if(generation!=sessionGeneration)return;
        isRequesting=false;
        shouldRecover=true;
        setStatus(QString("Pointer request failed: %1 · recovering while LT is held").arg(reply.error().message()));
        closeSession();
    });
}

void PortalPointer::requestAccess(){
    if(isActive){setStatus("Controller mouse ready · hold LT to use");return;}
    if(isRequesting)return;
    shouldRecover=false;
    if(!QDBusConnection::sessionBus().isConnected()){
        shouldRecover=true;
        setStatus("KDE desktop session bus is unavailable · retrying while LT is held");return;
    }
    isRequesting=true;
    const quint64 generation=++sessionGeneration;
    setStatus("Requesting KDE pointer permission…");
    createSession(generation);
}

void PortalPointer::createSession(quint64 generation){
    QVariantMap options;
    options.insert("handle_token",token());
    options.insert("session_handle_token",token());
    callRequest(generation,"CreateSession",{options},[this,generation](uint code,const QVariantMap &results){
        if(code!=0){
            isRequesting=false;
            shouldRecover=true;
            setStatus("KDE did not create a pointer session · retrying while LT is held");
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
        const QString createdSessionPath=sessionHandle;
        auto *closedReceiver=new PortalSessionClosed([this,generation,createdSessionPath]{
            if(generation==sessionGeneration&&sessionHandle==createdSessionPath)sessionClosed();
        },this);
        sessionSignalReceiver=closedReceiver;
        const bool monitoringSession=QDBusConnection::sessionBus().connect(
            portalService,sessionHandle,sessionInterface,"Closed",closedReceiver,
            SLOT(closed(QVariantMap)));
        if(!monitoringSession){
            isRequesting=false;
            setStatus("Could not monitor KDE pointer session");
            closeSession();
            return;
        }
        selectPointer(generation);
    });
}

void PortalPointer::selectPointer(quint64 generation){
    QVariantMap options;
    options.insert("handle_token",token());
    options.insert("types",uint(2)); // POINTER only
    options.insert("persist_mode",uint(2)); // Ask the portal to remember consent
    if(!savedRestoreToken.isEmpty())options.insert("restore_token",savedRestoreToken);
    callRequest(generation,"SelectDevices",{QDBusObjectPath(sessionHandle),options},
                [this,generation](uint code,const QVariantMap &){
        if(code!=0){
            isRequesting=false;
            setStatus("KDE pointer access was not granted");
            closeSession();
            return;
        }
        startSession(generation);
    });
}

void PortalPointer::startSession(quint64 generation){
    QVariantMap options;
    options.insert("handle_token",token());
    callRequest(generation,"Start",{QDBusObjectPath(sessionHandle),QString(),options},
                [this,generation](uint code,const QVariantMap &results){
        if(generation!=sessionGeneration)return;
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
    pendingMotionX=qBound(-maxQueuedMotion,pendingMotionX+dx,maxQueuedMotion);
    pendingMotionY=qBound(-maxQueuedMotion,pendingMotionY+dy,maxQueuedMotion);
    sendPendingMotion(sessionGeneration);
}

void PortalPointer::sendPendingMotion(quint64 generation){
    if(!isActive||motionInFlight||generation!=sessionGeneration
       ||(pendingMotionX==0.0&&pendingMotionY==0.0))return;
    const double dx=pendingMotionX,dy=pendingMotionY;
    pendingMotionX=pendingMotionY=0.0;
    motionInFlight=true;
    const QDBusMessage call=remoteDesktopCall(sessionHandle,"NotifyPointerMotion",
                                               {QVariantMap{},dx,dy});
    auto *watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call,pointerEventTimeoutMs),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher,generation]{
        QDBusPendingReply<> reply=*watcher;
        watcher->deleteLater();
        if(generation!=sessionGeneration)return;
        motionInFlight=false;
        if(reply.isError()){
            failSession(QString("KDE rejected pointer movement: %1").arg(reply.error().message()),generation);
            return;
        }
        sendPendingMotion(generation);
    });
}

void PortalPointer::sendButton(int button,quint64 generation){
    if(sessionHandle.isEmpty()||generation!=sessionGeneration)return;
    auto state=buttons.find(button);
    if(state==buttons.end()||state->inFlight||state->desired==state->confirmed)return;
    const bool pressed=state->desired;
    state->inFlight=true;
    if(pressed)state->mayBeDown=true;
    const QDBusMessage call=remoteDesktopCall(sessionHandle,"NotifyPointerButton",
                                               {QVariantMap{},button,uint(pressed?1:0)});
    auto *watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call,pointerEventTimeoutMs),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher,generation,button,pressed]{
        QDBusPendingReply<> reply=*watcher;
        watcher->deleteLater();
        if(generation!=sessionGeneration)return;
        auto state=buttons.find(button);
        if(state==buttons.end())return;
        state->inFlight=false;
        if(reply.isError()){
            // The portal may have received a press even if its reply was lost.
            // Preserve it for closeSession()'s best-effort release.
            state->mayBeDown=true;
            failSession(QString("KDE rejected pointer button input: %1").arg(reply.error().message()),generation);
            return;
        }
        state->confirmed=pressed;
        if(!pressed)state->mayBeDown=false;
        if(state->desired!=state->confirmed)sendButton(button,generation);
    });
}

void PortalPointer::button(bool left,bool pressed){
    if(!isActive)return;
    const int code=left?0x110:0x111;
    auto &state=buttons[code];
    if(state.desired==pressed)return;
    state.desired=pressed;
    sendButton(code,sessionGeneration);
}

void PortalPointer::scroll(int steps){
    if(!isActive||steps==0)return;
    pendingScrollSteps=qBound(-maxQueuedScrollSteps,pendingScrollSteps+steps,maxQueuedScrollSteps);
    sendPendingScroll(sessionGeneration);
}

void PortalPointer::sendPendingScroll(quint64 generation){
    if(!isActive||scrollInFlight||generation!=sessionGeneration||pendingScrollSteps==0)return;
    const int steps=pendingScrollSteps;
    pendingScrollSteps=0;
    const QDBusMessage call=remoteDesktopCall(sessionHandle,"NotifyPointerAxisDiscrete",
                                               {QVariantMap{},uint(0),int(steps)});
    scrollInFlight=true;
    auto *watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call,pointerEventTimeoutMs),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher,generation]{
        QDBusPendingReply<> reply=*watcher;
        watcher->deleteLater();
        if(generation!=sessionGeneration)return;
        scrollInFlight=false;
        if(reply.isError()){
            failSession(QString("KDE rejected pointer scrolling: %1").arg(reply.error().message()),generation);
            return;
        }
        sendPendingScroll(generation);
    });
}

void PortalPointer::failSession(const QString &message,quint64 generation){
    if(generation!=sessionGeneration)return;
    shouldRecover=true;
    setStatus(message+" · recovering while LT is held");
    closeSession();
}

void PortalPointer::cancelPendingRequests(){
    const auto requests=pendingRequests;
    pendingRequests.clear();
    for(auto it=requests.cbegin();it!=requests.cend();++it){
        auto *receiver=qobject_cast<PortalResponse *>(it.value().data());
        if(!receiver)continue;
        QDBusConnection::sessionBus().disconnect(
            portalService,it.key(),requestInterface,"Response",receiver,
            SLOT(response(uint,QVariantMap)));
        if(receiver->timeout)receiver->timeout->stop();
        receiver->callback={};
        receiver->deleteLater();
        QDBusMessage cancel=QDBusMessage::createMethodCall(
            portalService,it.key(),requestInterface,"Close");
        QDBusConnection::sessionBus().asyncCall(cancel);
    }
}

void PortalPointer::closeSession(){
    ++sessionGeneration;
    isRequesting=false;
    cancelPendingRequests();
    pendingMotionX=pendingMotionY=0.0;
    pendingScrollSteps=0;
    motionInFlight=false;
    scrollInFlight=false;
    if(!sessionHandle.isEmpty()){
        if(isActive){
            for(auto it=buttons.cbegin();it!=buttons.cend();++it){
                if(!it->desired&&!it->confirmed&&!it->mayBeDown)continue;
                const QDBusMessage release=remoteDesktopCall(
                    sessionHandle,"NotifyPointerButton",{QVariantMap{},it.key(),uint(0)});
                QDBusConnection::sessionBus().asyncCall(release);
            }
        }
        if(sessionSignalReceiver){
            QDBusConnection::sessionBus().disconnect(
                portalService,sessionHandle,sessionInterface,"Closed",sessionSignalReceiver,
                SLOT(closed(QVariantMap)));
            sessionSignalReceiver->deleteLater();
            sessionSignalReceiver=nullptr;
        }
        QDBusMessage call=QDBusMessage::createMethodCall(
            portalService,sessionHandle,sessionInterface,"Close");
        QDBusConnection::sessionBus().asyncCall(call);
    }
    sessionHandle.clear();
    buttons.clear();
    isActive=false;
}

void PortalPointer::sessionClosed(){
    sessionSignalReceiver=nullptr;
    ++sessionGeneration;
    cancelPendingRequests();
    sessionHandle.clear();
    isRequesting=false;
    isActive=false;
    motionInFlight=false;
    pendingMotionX=pendingMotionY=0.0;
    pendingScrollSteps=0;
    scrollInFlight=false;
    shouldRecover=true;
    buttons.clear();
    setStatus("KDE pointer session ended · recovering while LT is held");
}

#include "portalpointer.moc"
