#include "settingsnavigation.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QStyle>
#include <QTabWidget>
#include <QWidget>
#include <algorithm>
#include <utility>

SettingsNavigation::SettingsNavigation(QTabWidget *tabs, QScrollArea *scrollArea,
                                       QWidget *scope, QLabel *hint,
                                       QList<QWidget*> headerActions)
    : tabs_(tabs), scrollArea_(scrollArea), scope_(scope), hint_(hint),
      headerActions_(std::move(headerActions)) {}

QWidget *SettingsNavigation::current() const {
    return current_.data();
}

QList<QWidget*> SettingsNavigation::candidates() const {
    QList<QWidget*> result;
    QWidget *page = tabs_ ? tabs_->currentWidget() : nullptr;
    if (!page) return result;

    for (QWidget *widget : page->findChildren<QWidget*>()) {
        const bool supported = qobject_cast<QAbstractButton*>(widget)
            || qobject_cast<QComboBox*>(widget)
            || qobject_cast<QSlider*>(widget);
        if (supported && widget->isEnabled() && widget->isVisibleTo(page))
            result.append(widget);
    }
    std::stable_sort(result.begin(), result.end(), [page](QWidget *a, QWidget *b) {
        const QPoint aPosition = a->mapTo(page, QPoint());
        const QPoint bPosition = b->mapTo(page, QPoint());
        return aPosition.y() == bPosition.y()
            ? aPosition.x() < bPosition.x()
            : aPosition.y() < bPosition.y();
    });

    for (QWidget *action : headerActions_) {
        if (action && action->isEnabled() && action->isVisibleTo(scope_))
            result.append(action);
    }
    return result;
}

void SettingsNavigation::refreshStyle(QWidget *widget) {
    if (!widget) return;
    QStyle *style = widget->style();
    style->unpolish(widget);
    style->polish(widget);
    widget->update();
}

void SettingsNavigation::focus(QWidget *widget) {
    const QList<QWidget*> available = candidates();
    QWidget *next = available.contains(widget) ? widget : available.value(0, nullptr);
    if (current_ != next) {
        if (current_) {
            current_->setProperty("controllerFocused", false);
            refreshStyle(current_);
        }
        current_ = next;
        if (next) {
            next->setProperty("controllerFocused", true);
            refreshStyle(next);
            next->setFocus(Qt::OtherFocusReason);
            QWidget *page = tabs_->currentWidget();
            if (scrollArea_ && page && page->isAncestorOf(next))
                scrollArea_->ensureWidgetVisible(next);
        }
    }
    updateHint(next);
}

void SettingsNavigation::move(bool forward) {
    const QList<QWidget*> available = candidates();
    if (available.isEmpty()) {
        focus(nullptr);
        return;
    }
    const int index = available.indexOf(current_.data());
    const int next = index < 0 ? 0
        : (index + (forward ? 1 : -1) + available.size()) % available.size();
    focus(available[next]);
}

void SettingsNavigation::adjust(int direction) {
    if (auto *slider = qobject_cast<QSlider*>(current_.data())) {
        slider->setValue(slider->value() + direction * qMax(1, slider->singleStep()));
    } else if (auto *combo = qobject_cast<QComboBox*>(current_.data())) {
        if (combo->count() > 0)
            combo->setCurrentIndex(qBound(0, combo->currentIndex() + direction,
                                         combo->count() - 1));
    }
    updateHint(current_);
}

void SettingsNavigation::activate() {
    if (auto *button = qobject_cast<QAbstractButton*>(current_.data())) {
        button->click();
    } else if (auto *combo = qobject_cast<QComboBox*>(current_.data())) {
        if (combo->count() > 0)
            combo->setCurrentIndex((combo->currentIndex() + 1) % combo->count());
    }
    updateHint(current_);
}

void SettingsNavigation::syncFocus(QWidget *widget) {
    const QList<QWidget*> available = candidates();
    QWidget *control = widget;
    while (control && !available.contains(control))
        control = control->parentWidget();
    if (control) focus(control);
}

void SettingsNavigation::updateHint(QWidget *widget) {
    if (!hint_) return;
    if (!widget) {
        hint_->setText("No available controls on this tab.");
        return;
    }

    QString name = widget->accessibleName();
    if (name.isEmpty()) {
        if (auto *button = qobject_cast<QAbstractButton*>(widget)) name = button->text();
        else if (auto *combo = qobject_cast<QComboBox*>(widget)) name = combo->currentText();
        else name = "Setting";
    }
    if (auto *slider = qobject_cast<QSlider*>(widget))
        name += QString(": %1").arg(slider->value());
    else if (auto *combo = qobject_cast<QComboBox*>(widget))
        name += QString(": %1").arg(combo->currentText());

    hint_->setText(QString("Selected: %1   ·   D-pad move   ·   Left/Right adjust   ·   A toggle/activate   ·   LB/RB tabs   ·   B back")
                       .arg(name));
}
