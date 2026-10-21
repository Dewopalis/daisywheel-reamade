#pragma once

#include <QList>
#include <QPointer>

class QLabel;
class QScrollArea;
class QTabWidget;
class QWidget;

class SettingsNavigation {
public:
    SettingsNavigation(QTabWidget *tabs, QScrollArea *scrollArea, QWidget *scope,
                      QLabel *hint, QList<QWidget*> headerActions);

    QWidget *current() const;
    void focus(QWidget *widget);
    void move(bool forward);
    void adjust(int direction);
    void activate();
    void syncFocus(QWidget *widget);

private:
    QList<QWidget*> candidates() const;
    void updateHint(QWidget *widget);
    static void refreshStyle(QWidget *widget);

    QTabWidget *tabs_;
    QScrollArea *scrollArea_;
    QWidget *scope_;
    QLabel *hint_;
    QList<QWidget*> headerActions_;
    QPointer<QWidget> current_;
};
