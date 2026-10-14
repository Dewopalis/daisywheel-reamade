#pragma once
#include <QString>
#include <QRect>
#include <functional>
#include <memory>
class QWidget;
class KdeInput {
public:
    KdeInput();
    ~KdeInput();
    bool attach(QWidget *window);
    QRect overlayGeometry() const;
    void setOverlayGeometry(const QRect &geometry);
    bool active() const;
    bool verificationTarget() const;
    bool commit(const QString &text);
    bool key(unsigned symbol);
    std::function<void(bool)> activation;
private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
