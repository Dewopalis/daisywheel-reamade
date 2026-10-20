#pragma once

#include <QObject>
#include <QString>
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
    bool isActive=false;
    bool isRequesting=false;

    void setStatus(const QString &status);
    void createSession();
    void selectPointer();
    void startSession();
    void callRequest(const QString &method, const QVariantList &arguments,
                     std::function<void(uint,const QVariantMap &)> response);
    void closeSession();
};
