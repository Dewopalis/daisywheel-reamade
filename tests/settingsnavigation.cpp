#include "settingsnavigation.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QVBoxLayout>
#include <iostream>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QWidget root;
    QVBoxLayout layout(&root);
    QPushButton help("Help"), back("Back to keyboard");
    QLabel hint;
    QTabWidget tabs;
    layout.addWidget(&help);
    layout.addWidget(&tabs);
    layout.addWidget(&back);
    layout.addWidget(&hint);

    QWidget firstPage;
    QVBoxLayout firstLayout(&firstPage);
    QCheckBox enabled("Enable feature");
    QSlider slider(Qt::Horizontal);
    slider.setRange(0, 20);
    slider.setSingleStep(5);
    slider.setValue(5);
    QComboBox choice;
    choice.addItems({"First", "Second", "Third"});
    firstLayout.addWidget(&enabled);
    firstLayout.addWidget(&slider);
    firstLayout.addWidget(&choice);
    tabs.addTab(&firstPage, "First tab");

    QWidget secondPage;
    QVBoxLayout secondLayout(&secondPage);
    QCheckBox secondOption("Second tab option");
    secondLayout.addWidget(&secondOption);
    tabs.addTab(&secondPage, "Second tab");

    int helpActivated = 0;
    int backActivated = 0;
    QObject::connect(&help, &QPushButton::clicked, [&] { ++helpActivated; });
    QObject::connect(&back, &QPushButton::clicked, [&] { ++backActivated; });
    root.show();
    app.processEvents();

    SettingsNavigation navigation(&tabs, nullptr, &root, &hint, {&help, &back});
    auto check = [](bool condition, const char *message) {
        if (!condition) std::cerr << "FAIL: " << message << '\n';
        return condition;
    };

    navigation.focus(&enabled);
    if (!check(navigation.current() == &enabled && enabled.property("controllerFocused").toBool(),
               "initial selection is focused and highlighted")) return 1;
    navigation.move(true);
    if (!check(navigation.current() == &slider
                   && !enabled.property("controllerFocused").toBool()
                   && slider.property("controllerFocused").toBool(),
               "D-pad moves selection and transfers its highlight")) return 2;
    navigation.adjust(1);
    if (!check(slider.value() == 10 && hint.text().contains("10"),
               "directional input adjusts a slider and reports its value")) return 3;
    navigation.move(true);
    navigation.adjust(1);
    if (!check(choice.currentIndex() == 1, "directional input changes a combo box")) return 4;
    navigation.move(true);
    if (!check(navigation.current() == &help, "Help is reachable after tab controls")) return 5;
    navigation.activate();
    if (!check(helpActivated == 1, "A activates Help")) return 6;
    navigation.move(true);
    if (!check(navigation.current() == &back, "Back to keyboard is reachable")) return 7;
    navigation.activate();
    if (!check(backActivated == 1, "A activates Back to keyboard")) return 8;

    tabs.setCurrentIndex(1);
    navigation.move(true);
    if (!check(navigation.current() == &secondOption,
               "navigation starts at the first control on the selected tab")) return 9;
    navigation.activate();
    if (!check(secondOption.isChecked(), "A toggles a checkbox")) return 10;
    navigation.move(true);
    if (!check(navigation.current() == &help, "header actions remain reachable on each tab")) return 11;

    std::cout << "PASS: settings navigation, adjustment, activation and header actions\n";
    return 0;
}
