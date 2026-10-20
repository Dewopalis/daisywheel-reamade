#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

class PortalPointer : public QObject {
    Q_OBJECT
public:
    explicit PortalPointer(QString restoreToken, QObject *parent=nullptr);
    ~PortalPointer() override;

    bool active() const { return isActive; }
    bool requesting() const { return isRequesting; }
    QString status() const { return currentStatus; }
    void requestAccess();
    void move(double dx, double dy);
    void button(bool left, bool pressed);
    void scroll(int steps);

    std::function<void(const QString &)> statusChanged;
    std::function<void(const QString &)> restoreTokenChanged;

private:
    QString savedRestoreToken;
    QString sessionHandle;
    QString currentStatus;
    QPointer<QObject> sessionSignalReceiver;
    bool isActive=false;
    bool isRequesting=false;
    bool motionInFlight=false;
    quint64 sessionGeneration=0;
    double pendingMotionX=0.0;
    double pendingMotionY=0.0;
    QHash<QString,QPointer<QObject>> pendingRequests;
    QSet<int> heldButtons;

    void setStatus(const QString &status);
    void createSession(quint64 generation);
    void selectPointer(quint64 generation);
    void startSession(quint64 generation);
    void callRequest(quint64 generation,const QString &method, const QVariantList &arguments,
                     std::function<void(uint,const QVariantMap &)> response);
    void cancelPendingRequests();
    void sendPendingMotion(quint64 generation);
    void sendButton(int button,bool pressed,quint64 generation);
    void failSession(const QString &message,quint64 generation);
    void closeSession();
    void sessionClosed();
};
