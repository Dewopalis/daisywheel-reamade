#include "wheel.h"
#include "overlaywidgets.h"
#include "visibility.h"
#include "placement.h"
#include "settingsnavigation.h"
#include "soundeffects.h"
#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWindow>
#include <QSettings>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QGraphicsOpacityEffect>
#include <QTabWidget>
#include <QScrollArea>
#include <QFrame>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QScreen>
#include <QCursor>
#include <QList>
#include <QDir>
#include <QLockFile>
#include <QStandardPaths>
#include <functional>
#include <algorithm>
#include <iostream>
#include <memory>
#include <SDL.h>
#ifdef Q_OS_WIN
#include "windowsinput.h"
#include "windowsfocus.h"
#endif
#ifdef DAISYWHEEL_KDE
#include "kdeinput.h"
#include "appimageinstaller.h"
#include "portalpointer.h"
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

int main(int argc,char **argv){
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("Daisywheel");
    QCoreApplication::setApplicationName("Daisywheel");
#ifdef DAISYWHEEL_KDE
    if(app.arguments().contains("--install-appimage") && !runAppImageInstaller())return 0;
#endif
    const int previewArgument=app.arguments().indexOf("--render-wheel-preview");
    if(previewArgument>=0){
        if(app.arguments().size()<=previewArgument+1)return 1;
        WheelView preview;preview.setMinimumSize(0,0);preview.resize(390,310);
        preview.selected=0;
        return preview.grab().save(app.arguments().at(previewArgument+1))?0:1;
    }
    QSettings settings;
    settings.remove("minimalMode");
    if(app.arguments().contains("--input-target-test")) {
        QPlainTextEdit target;target.setWindowTitle("Daisywheel cross-app verification");
        target.setPlainText("DAISYWHEEL_ISOLATED_PROBE");target.selectAll();
        target.resize(460,180);target.show();target.setFocus();target.activateWindow();
        QObject::connect(&target,&QPlainTextEdit::textChanged,[&]{
            if(target.toPlainText()=="daisywheel-test"){
                std::cout<<"PASS: separate application received committed text\n";app.exit(0);
            }
        });
        QTimer::singleShot(60000,&app,[&]{std::cerr<<"FAIL: external input not received\n";app.exit(4);});
        return app.exec();
    }
    const bool kdeMode=app.arguments().contains("--kde-input-method");
    const bool windowsMode=app.arguments().contains("--windows-input");
    if(kdeMode && windowsMode){std::cerr<<"Choose only one system input backend\n";return 1;}
    const bool systemMode=kdeMode||windowsMode;
    std::unique_ptr<QLockFile> instanceLock;
    if(windowsMode){
        const QString directory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if(!QDir().mkpath(directory)){std::cerr<<"Cannot create application data directory\n";return 1;}
        instanceLock=std::make_unique<QLockFile>(directory+"/windows-input.lock");
        if(!instanceLock->tryLock(0)){std::cerr<<"Windows keyboard is already running or its lock is unavailable\n";return 1;}
    }
#ifdef Q_OS_WIN
    WindowsInput windowsInput;
#else
    if(windowsMode){std::cerr<<"Windows backend unavailable in this build\n";return 1;}
#endif
#ifdef DAISYWHEEL_KDE
    std::unique_ptr<KdeInput> kde;
    std::unique_ptr<PortalPointer> portalPointer;
    if(kdeMode)kde=std::make_unique<KdeInput>();
    if(kdeMode)portalPointer=std::make_unique<PortalPointer>(settings.value("pointerRestoreToken").toString());
#else
    if(kdeMode){std::cerr<<"KDE backend unavailable in this build\n";return 1;}
#endif
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    SDL_SetMainReady();
    bool sdlOk=SDL_Init(SDL_INIT_GAMECONTROLLER)==0;
    if(app.arguments().contains("--controllers")) {
        if(!sdlOk){std::cerr<<SDL_GetError()<<'\n';return 1;}
        int count=SDL_NumJoysticks();
        std::cout<<"SDL devices: "<<count<<'\n';
        for(int i=0;i<count;++i){
            std::cout<<i<<": "<<SDL_JoystickNameForIndex(i)<<" | mapped: "<<SDL_IsGameController(i);
            auto *probe=SDL_IsGameController(i)?SDL_GameControllerOpen(i):nullptr;
            std::cout<<" | accessible: "<<(probe?"yes":"no")<<'\n';
            if(probe)SDL_GameControllerClose(probe);
        }
        SDL_Quit();return count?0:2;
    }
#ifdef Q_OS_WIN
    WindowsOverlay window;window.preserveFocus=windowsMode;
#else
    QWidget window;
#endif
    SoundEffects soundEffects;
    bool soundAvailable=false;
    window.setWindowTitle("Daisywheel — controller prototype");
    app.setQuitOnLastWindowClosed(false);
    window.setWindowFlags(Qt::Window|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    window.setAttribute(Qt::WA_TranslucentBackground);
    auto *box=new QVBoxLayout(&window);
    auto *content=new QWidget;content->setObjectName("panel");
    content->setStyleSheet("QWidget#panel { background: #1d2229; border: 1px solid #536075; border-radius: 10px; }");
    box->addWidget(content);
    box=new QVBoxLayout(content);
    QList<QWidget*> fullModeWidgets;
    auto addControl=[&](QWidget *control){box->addWidget(control);fullModeWidgets.append(control);return control;};
    auto *settingsPanel=new QWidget(content);
    settingsPanel->setMinimumWidth(280);
    settingsPanel->setStyleSheet(
        "QWidget[controllerFocused=\"true\"] { background-color: #29394c; border: 2px solid #70d6e8; border-radius: 5px; }"
        "QAbstractButton[controllerFocused=\"true\"] { padding: 5px; }"
        "QSlider[controllerFocused=\"true\"] { padding: 3px; }"
        "QComboBox[controllerFocused=\"true\"] { padding: 5px 8px; }");
    auto *settingsLayout=new QVBoxLayout(settingsPanel);
    auto *settingsHeader=new QHBoxLayout;
    auto *settingsTitle=new QLabel("Daisywheel settings");
    settingsHeader->addWidget(settingsTitle,1);
    auto *helpButton=new QPushButton("Help");
    helpButton->setAccessibleName("Help");
    settingsHeader->addWidget(helpButton);
    auto *backToKeyboardButton=new QPushButton("Back to keyboard");
    backToKeyboardButton->setAccessibleName("Back to keyboard");
    settingsHeader->addWidget(backToKeyboardButton);
    settingsLayout->addLayout(settingsHeader);
    auto *settingsControllerHint=new QLabel("D-pad: move · Left/Right: adjust · A: select · LB/RB: tabs · B: keyboard");
    settingsControllerHint->setWordWrap(true);
    settingsControllerHint->setStyleSheet("QLabel { color: #a9bfd1; padding: 3px 6px; }");
    auto *windowSettingsGroup=new QGroupBox("Window");
    windowSettingsGroup->setMinimumWidth(0);
    auto *windowSettingsLayout=new QFormLayout(windowSettingsGroup);
    windowSettingsLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *controllerSettingsGroup=new QGroupBox("Controller");
    controllerSettingsGroup->setMinimumWidth(0);
    auto *controllerSettingsLayout=new QFormLayout(controllerSettingsGroup);
    controllerSettingsLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *soundSettingsGroup=new QGroupBox("Sound effects");
    soundSettingsGroup->setMinimumWidth(0);
    auto *soundSettingsLayout=new QFormLayout(soundSettingsGroup);
    soundSettingsLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *soundEnabled=new QCheckBox("Enable sound effects");
    soundEnabled->setAccessibleName("Enable sound effects");
    soundEnabled->setChecked(settings.value("soundEnabled",true).toBool());
    soundSettingsLayout->addRow(soundEnabled);
    auto *soundVolumeLabel=new QLabel;
    auto *soundVolume=new QSlider(Qt::Horizontal);
    soundVolume->setAccessibleName("Sound volume");
    soundVolume->setSingleStep(5);
    soundVolume->setRange(0,100);
    int initialSoundVolume=settings.value("soundVolume",50).toInt();
    if(settings.value("soundLevelRevision",0).toInt()<1){
        // Preserve users' previously selected quiet level after the master
        // output is reduced by 10x. Values above 10 simply become quieter.
        if(settings.contains("soundVolume")&&initialSoundVolume>0&&initialSoundVolume<=10)
            initialSoundVolume=std::min(100,initialSoundVolume*10);
        settings.setValue("soundLevelRevision",1);
    }
    soundVolume->setValue(initialSoundVolume);
    soundSettingsLayout->addRow(soundVolumeLabel,soundVolume);
    auto *previewSound=new QPushButton("Preview button sound");
    soundSettingsLayout->addRow(previewSound);
    auto *previewPetalSound=new QPushButton("Preview petal sound");
    soundSettingsLayout->addRow(previewPetalSound);
    soundVolume->setEnabled(soundEnabled->isChecked());
    previewSound->setEnabled(soundEnabled->isChecked());
    previewPetalSound->setEnabled(soundEnabled->isChecked());
    soundEffects.setEnabled(soundEnabled->isChecked());
    soundEffects.setVolume(soundVolume->value());
    auto updateSoundVolume=[&](int value){
        soundVolumeLabel->setText(value==0?"Volume: 0% (off)":QString("Volume: %1%").arg(value));
        soundEffects.setVolume(value);
        settings.setValue("soundVolume",value);
    };
    updateSoundVolume(soundVolume->value());
    soundSettingsLayout->addRow(new QLabel("Petal movement plays a soft ringing chord; button presses play a short pluck."));
    QObject::connect(soundEnabled,&QCheckBox::toggled,[&](bool enabled){
        settings.setValue("soundEnabled",enabled);
        soundEffects.setEnabled(enabled);
        soundVolume->setEnabled(enabled);
        previewSound->setEnabled(enabled);
        previewPetalSound->setEnabled(enabled);
    });
    QObject::connect(soundVolume,&QSlider::valueChanged,updateSoundVolume);
    QObject::connect(previewSound,&QPushButton::clicked,[&]{soundEffects.buttonPressed();});
    QObject::connect(previewPetalSound,&QPushButton::clicked,[&]{soundEffects.petalChanged(0);});
#ifdef Q_OS_WIN
    auto *keyboardSettingsGroup=new QGroupBox("Keyboard");
    auto *keyboardSettingsLayout=new QVBoxLayout(keyboardSettingsGroup);
#endif
    auto *settingsTabs=new QTabWidget(settingsPanel);
    settingsTabs->addTab(windowSettingsGroup,"Window");
    settingsTabs->addTab(controllerSettingsGroup,"Controller");
    settingsTabs->addTab(soundSettingsGroup,"Sound");
#ifdef Q_OS_WIN
    if(windowsMode)settingsTabs->addTab(keyboardSettingsGroup,"Keyboard");
#endif
    auto *settingsScroll=new QScrollArea(settingsPanel);
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    settingsTabs->setMinimumWidth(0);
    settingsTabs->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);
    settingsScroll->setFrameShape(QFrame::NoFrame);
    settingsScroll->setWidget(settingsTabs);
    settingsLayout->addWidget(settingsScroll,1);
    settingsLayout->addWidget(settingsControllerHint);
    SettingsNavigation settingsNavigation(settingsTabs,settingsScroll,settingsPanel,
                                          settingsControllerHint,{helpButton,backToKeyboardButton});
    QPointer<QMessageBox> activeHelpDialog;
    QObject::connect(helpButton,&QPushButton::clicked,[&window,&activeHelpDialog]{
        QMessageBox help(QMessageBox::Information,"Daisywheel controls",QString(),QMessageBox::Close,&window);
        activeHelpDialog=&help;
        help.setTextFormat(Qt::RichText);
        help.setMinimumWidth(480);
        help.setText(
            "<h3>Choose and type</h3>"
            "<p>Move the left stick to select one of the eight petals. Press a face button "
            "to choose its matching character: A is bottom, B right, X left, and Y top. "
            "LB switches uppercase; RB switches between letters and symbols.</p>"
            "<h3>Other controls</h3>"
            "<p><b>L3</b> inserts a space. <b>R3</b> deletes; hold it to repeat. "
            "<b>Start</b> inserts Enter. Set the show/hide shortcut in Controller settings by pressing any mapped controller button.</p>"
            "<p>Hold the show/hide shortcut to open Settings. In Settings, use the D-pad to move and adjust controls, "
            "A/Start to activate, LB/RB to change tabs, and B to return to the keyboard.</p>"
            "<h3>Mouse mode</h3>"
            "<p>When enabled, mouse mode only works while the keyboard is visible. Hold <b>LT</b> "
            "and move the left stick to move the pointer. <b>R1</b> clicks and <b>R2</b> right-clicks; "
            "<b>X/Y</b> or the D-pad scroll. Release LT to return to typing.</p>"
            "<h3>Edit text</h3>"
            "<p>Hold <b>RT</b> and move the left stick to move the text cursor. Hold <b>LB</b> to select, "
            "<b>RB</b> to move by words, or both to select by words.</p>"
            "<h3>Sound</h3>"
            "<p>Petal changes use a soft pitch cue, with a brighter cue for selections and actions. "
            "Adjust the volume or disable sound effects in Sound settings.</p>"
            "<h3>Window</h3>"
            "<p>Drag the title area to move Daisywheel. Use the corner grip to resize. "
            "Settings contains position, transparency, and controller options.</p>");
        help.exec();
        activeHelpDialog=nullptr;
    });
    settingsPanel->hide();
    std::function<void()> showSettings;
    std::function<void()> showKeyboard;
    auto *headerWidget=new QWidget(content);
    auto *header=new QHBoxLayout(headerWidget);
    header->setContentsMargins(0,0,0,0);
    headerWidget->setFixedHeight(32);
    auto *resizeHandle=new ResizeHandle(kdeMode);
    header->addWidget(resizeHandle);
    auto *handle=new MoveHandle("Daisywheel · drag here to move");header->addWidget(handle,1);
    auto *settingsButton=new QPushButton("Settings");settingsButton->setFixedHeight(26);
    header->addWidget(settingsButton);QObject::connect(settingsButton,&QPushButton::clicked,[&]{if(showSettings)showSettings();});
    addControl(headerWidget);
    auto *pin=new QCheckBox("Pin position (lock dragging)");
    pin->setAccessibleName("Pin position");
    pin->setChecked(settings.value("pinned",false).toBool());handle->pinned=pin->isChecked();windowSettingsLayout->addRow(pin);
    std::optional<QPoint> pinnedPosition;
    if(windowsMode && settings.contains("windowsPinnedPosition"))
        pinnedPosition=settings.value("windowsPinnedPosition").toPoint();
    resizeHandle->geometryChanged=[&](const QRect &geometry){
        if(windowsMode && pin->isChecked()){
            pinnedPosition=geometry.topLeft();
            settings.setValue("windowsPinnedPosition",*pinnedPosition);
        }
    };
    QObject::connect(pin,&QCheckBox::toggled,[&](bool checked){
        handle->pinned=checked;settings.setValue("pinned",checked);
        if(windowsMode && checked && window.isVisible()){
            pinnedPosition=window.pos();settings.setValue("windowsPinnedPosition",*pinnedPosition);
        }
    });
    auto *opacityLabel=new QLabel;
    auto *opacity=new QSlider(Qt::Horizontal);opacity->setRange(30,100);
    opacity->setAccessibleName("Window opacity");
    opacity->setSingleStep(5);
    opacity->setValue(settings.value("opacity",100).toInt());windowSettingsLayout->addRow(opacityLabel,opacity);
    // Alpha composition works for the content on Wayland without relying on
    // compositor support for QWidget::setWindowOpacity.
    auto *alpha=new QGraphicsOpacityEffect(content);content->setGraphicsEffect(alpha);
    auto setOpacity=[&](int value){alpha->setOpacity(value/100.0);opacityLabel->setText(QString("Opacity: %1%").arg(value));settings.setValue("opacity",value);};
    setOpacity(opacity->value());QObject::connect(opacity,&QSlider::valueChanged,setOpacity);
    VisibilityPolicy visibility;
    auto *sessionGroup=new QGroupBox("Daisywheel");
    auto *sessionLayout=new QVBoxLayout(sessionGroup);
    auto *hide=new QPushButton("Hide keyboard");
    hide->setAccessibleName("Hide keyboard");
    hide->setToolTip("Hide the wheel and its overlay window. Use the controller shortcut to show it again.");
    sessionLayout->addWidget(hide);
    auto *quit=new QPushButton("Quit Daisywheel");
    quit->setAccessibleName("Quit Daisywheel");
    sessionLayout->addWidget(quit);
    windowSettingsLayout->addRow(sessionGroup);
    QObject::connect(hide,&QPushButton::clicked,[&]{
        if(showKeyboard)showKeyboard();
        visibility.manual(false);
        window.hide();
    });
    QObject::connect(quit,&QPushButton::clicked,&app,&QApplication::quit);
    auto *controllerProfile=new QComboBox;
    controllerProfile->setAccessibleName("Controller profile");
    controllerProfile->addItem("Handheld", "handheld");
    controllerProfile->addItem("Living room / desktop", "living-room");
    controllerProfile->addItem("Custom", "custom");
    QString savedProfile=settings.value("controllerProfile","living-room").toString();
    int savedProfileIndex=controllerProfile->findData(savedProfile);
    controllerProfile->setCurrentIndex(savedProfileIndex<0?controllerProfile->findData("living-room"):savedProfileIndex);
    controllerSettingsLayout->addRow("Profile",controllerProfile);
    auto *mouseModeEnabled=new QCheckBox("Enable mouse mode (only while keyboard is open)");
    mouseModeEnabled->setAccessibleName("Enable mouse mode");
    mouseModeEnabled->setChecked(settings.value("controllerMouseEnabled",savedProfile=="living-room").toBool());
    controllerSettingsLayout->addRow(mouseModeEnabled);
    auto *pointerSpeedLabel=new QLabel;
    auto *pointerSpeed=new QSlider(Qt::Horizontal);pointerSpeed->setRange(250,1800);
    pointerSpeed->setAccessibleName("Pointer speed");
    pointerSpeed->setSingleStep(50);
    pointerSpeed->setMinimumWidth(0);pointerSpeed->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    pointerSpeed->setValue(settings.value("pointerSpeed",950).toInt());controllerSettingsLayout->addRow(pointerSpeedLabel,pointerSpeed);
    auto *deadzoneLabel=new QLabel;
    auto *deadzone=new QSlider(Qt::Horizontal);deadzone->setRange(5,45);
    deadzone->setAccessibleName("Stick deadzone");
    deadzone->setSingleStep(1);
    deadzone->setMinimumWidth(0);deadzone->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    deadzone->setValue(settings.value("controllerDeadzone",20).toInt());
    controllerSettingsLayout->addRow(deadzoneLabel,deadzone);
    auto *hapticsEnabled=new QCheckBox("Enable haptic feedback");
    hapticsEnabled->setAccessibleName("Enable haptic feedback");
    hapticsEnabled->setChecked(settings.value("hapticsEnabled",false).toBool());controllerSettingsLayout->addRow(hapticsEnabled);
    auto *hapticsStrengthLabel=new QLabel;
    auto *hapticsStrength=new QSlider(Qt::Horizontal);hapticsStrength->setRange(10,100);
    hapticsStrength->setAccessibleName("Haptic strength");
    hapticsStrength->setSingleStep(5);
    hapticsStrength->setMinimumWidth(0);hapticsStrength->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    hapticsStrength->setValue(settings.value("hapticsStrength",50).toInt());controllerSettingsLayout->addRow(hapticsStrengthLabel,hapticsStrength);
    hapticsStrengthLabel->setText(QString("Haptic strength: %1%").arg(hapticsStrength->value()));
    controllerProfile->setVisible(systemMode);mouseModeEnabled->setVisible(systemMode);
    pointerSpeedLabel->setVisible(systemMode);pointerSpeed->setVisible(systemMode);
    deadzoneLabel->setVisible(systemMode);deadzone->setVisible(systemMode);
    hapticsEnabled->setVisible(systemMode);hapticsStrengthLabel->setVisible(systemMode);
    hapticsStrength->setVisible(systemMode);
    pointerSpeed->setEnabled(mouseModeEnabled->isChecked());
    hapticsStrength->setEnabled(hapticsEnabled->isChecked());
    bool applyingControllerPreset=false;
    auto setCustomProfile=[&]{
        if(applyingControllerPreset)return;
        controllerProfile->setCurrentIndex(controllerProfile->findData("custom"));
        settings.setValue("controllerProfile","custom");
    };
    auto setDeadzoneLabel=[&](int value){
        deadzoneLabel->setText(QString("Stick deadzone: %1%").arg(value));
    };
    setDeadzoneLabel(deadzone->value());
    QObject::connect(deadzone,&QSlider::valueChanged,[&](int value){
        setDeadzoneLabel(value);
        settings.setValue("controllerDeadzone",value);
        setCustomProfile();
    });
    QObject::connect(controllerProfile,&QComboBox::currentIndexChanged,[&](int index){
        const QString profile=controllerProfile->itemData(index).toString();
        settings.setValue("controllerProfile",profile);
        if(profile=="custom")return;
        applyingControllerPreset=true;
        mouseModeEnabled->setChecked(profile=="living-room");
        pointerSpeed->setValue(950);
        deadzone->setValue(20);
        applyingControllerPreset=false;
    });
    QObject::connect(mouseModeEnabled,&QCheckBox::toggled,[&](bool enabled){
        settings.setValue("controllerMouseEnabled",enabled);
        pointerSpeed->setEnabled(enabled);
        setCustomProfile();
    });
    QObject::connect(hapticsEnabled,&QCheckBox::toggled,[&](bool enabled){
        settings.setValue("hapticsEnabled",enabled);
        hapticsStrength->setEnabled(enabled);
        setCustomProfile();
    });
    QObject::connect(hapticsStrength,&QSlider::valueChanged,[&](int value){
        hapticsStrengthLabel->setText(QString("Haptic strength: %1%").arg(value));
        settings.setValue("hapticsStrength",value);
        setCustomProfile();
    });
    auto setPointerSpeed=[&](int value){
        pointerSpeedLabel->setText(QString("Pointer speed: %1").arg(value));
        settings.setValue("pointerSpeed",value);
        setCustomProfile();
    };
    setPointerSpeed(pointerSpeed->value());
    QObject::connect(pointerSpeed,&QSlider::valueChanged,setPointerSpeed);
    int shortcutButton=settings.value("shortcut",SDL_CONTROLLER_BUTTON_GUIDE).toInt();
    if(shortcutButton<0||shortcutButton>=SDL_CONTROLLER_BUTTON_MAX)shortcutButton=SDL_CONTROLLER_BUTTON_GUIDE;
    auto controllerButtonName=[](int button){
        const char *name=SDL_GameControllerGetStringForButton(static_cast<SDL_GameControllerButton>(button));
        if(!name)return QString("Button %1").arg(button+1);
        QString result=QString::fromUtf8(name);
        if(!result.isEmpty())result[0]=result[0].toUpper();
        return result;
    };
    auto *shortcutName=new QLabel(controllerButtonName(shortcutButton));
    auto *captureShortcutButton=new QPushButton("Choose on controller…");
    captureShortcutButton->setAccessibleName("Choose show or hide shortcut");
    captureShortcutButton->setToolTip("Press any mapped button on the connected controller to use it as the show/hide shortcut");
    auto *shortcutRow=new QWidget;
    auto *shortcutRowLayout=new QHBoxLayout(shortcutRow);
    shortcutRowLayout->setContentsMargins(0,0,0,0);
    shortcutRowLayout->addWidget(shortcutName,1);
    shortcutRowLayout->addWidget(captureShortcutButton);
    controllerSettingsLayout->addRow("Show / hide shortcut",shortcutRow);
    bool capturingShortcut=false;
    auto updateShortcutCapture=[&](bool active){
        capturingShortcut=active;
        captureShortcutButton->setText(active?"Cancel":"Choose on controller…");
        shortcutName->setText(active?"Press a mapped controller button…":controllerButtonName(shortcutButton));
    };
    QObject::connect(captureShortcutButton,&QPushButton::clicked,[&]{updateShortcutCapture(!capturingShortcut);});
    auto *invertScrolling=new QCheckBox("Invert scroll direction");
    invertScrolling->setAccessibleName("Invert scroll direction");
    invertScrolling->setChecked(settings.value("invertScrolling",false).toBool());
    invertScrolling->setToolTip("Reverse the direction used by the D-pad and X/Y buttons while mouse mode is active");
    invertScrolling->setVisible(systemMode);
    invertScrolling->setEnabled(mouseModeEnabled->isChecked());
    controllerSettingsLayout->addRow("Living room scrolling",invertScrolling);
    QObject::connect(invertScrolling,&QCheckBox::toggled,[&](bool enabled){settings.setValue("invertScrolling",enabled);});
    QObject::connect(mouseModeEnabled,&QCheckBox::toggled,invertScrolling,&QWidget::setEnabled);
    auto *status=new QLabel;
    status->setWordWrap(true);
    status->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Minimum);
    addControl(status);
#ifdef DAISYWHEEL_KDE
    if(portalPointer){
        portalPointer->statusChanged=[&](const QString &message){status->setText(message);};
        portalPointer->restoreTokenChanged=[&](const QString &value){
            settings.setValue("pointerRestoreToken",value);settings.sync();
        };
    }
#endif
    auto *editor=new QPlainTextEdit;editor->setPlaceholderText("Tap here, then type with your controller");editor->setMaximumHeight(110);addControl(editor);
    editor->setVisible(!systemMode);
    auto *view=new WheelView;box->addWidget(view);
    view->setShortcutHint(QString("Hold %1 for Settings").arg(controllerButtonName(shortcutButton)));
    box->setStretch(box->indexOf(headerWidget),0);
    box->setStretch(box->indexOf(status),0);
    box->setStretch(box->indexOf(view),1);
    if(windowsMode)view->setMinimumSize(360,310);
    box->addWidget(settingsPanel,1);
    showSettings=[&]{
        for(auto *control:fullModeWidgets)control->hide();
        view->hide();
        settingsPanel->show();
        settingsTabs->setCurrentWidget(systemMode?controllerSettingsGroup:windowSettingsGroup);
        QWidget *initialFocus=systemMode?static_cast<QWidget*>(controllerProfile):static_cast<QWidget*>(pin);
        settingsNavigation.focus(initialFocus);
        window.setWindowTitle("Daisywheel settings");
    };
    showKeyboard=[&]{
        settingsPanel->hide();
        window.setWindowTitle("Daisywheel — controller prototype");
        for(auto *control:fullModeWidgets)
            control->setVisible(control!=editor||!systemMode);
        view->show();
    };
    QObject::connect(backToKeyboardButton,&QPushButton::clicked,[&]{if(showKeyboard)showKeyboard();});
    TouchActivation activation;activation.activate=[=]{view->show();};
    editor->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    editor->viewport()->installEventFilter(&activation);
    auto showOverlay=[&]{
        view->show();
        if(windowsMode){
            QScreen *screen=nullptr;
#ifdef Q_OS_WIN
            MONITORINFOEXW monitor{};monitor.cbSize=sizeof(monitor);
            if(GetMonitorInfoW(MonitorFromWindow(GetForegroundWindow(),MONITOR_DEFAULTTONEAREST),&monitor)){
                for(auto *candidate:QGuiApplication::screens())
                    if(candidate->name()==QString::fromWCharArray(monitor.szDevice)){screen=candidate;break;}
            }
#endif
            if(!screen)screen=QGuiApplication::screenAt(QCursor::pos());
            if(!screen)screen=QGuiApplication::primaryScreen();
            window.adjustSize();
            QList<QRect> areas;
            for(auto *display:QGuiApplication::screens())areas.append(display->availableGeometry());
            window.move(overlayPosition(window.size(),screen->availableGeometry(),areas,
                                       pin->isChecked()?pinnedPosition:std::nullopt));
            if(pin->isChecked()){
                pinnedPosition=window.pos();settings.setValue("windowsPinnedPosition",*pinnedPosition);
            }
        }
        window.show();
    };
    auto manualToggle=[&]{
        bool show=!window.isVisible();visibility.manual(show);
        if(show){if(showKeyboard)showKeyboard();showOverlay();}
        else {if(showKeyboard)showKeyboard();window.hide();}
#ifdef DAISYWHEEL_KDE
        if(show && kde){
            QDBusInterface keyboard("org.kde.KWin","/VirtualKeyboard","org.kde.kwin.VirtualKeyboard");
            keyboard.asyncCall("forceActivate");
        }
#endif
    };
    std::function<void(Uint32)> haptic;
    auto sendText=[&](const QString &text){
#ifdef Q_OS_WIN
        if(windowsMode){windowsInput.commit(text);if(haptic)haptic(55);return;}
#endif
#ifdef DAISYWHEEL_KDE
        if(kde){
            if(!kde->commit(text))status->setText("No active text field — focus a text field in another app");
            else if(haptic)haptic(55);
            return;
        }
#endif
        editor->insertPlainText(text);
        if(haptic)haptic(55);
    };
    auto sendControl=[&](bool backspace){
#ifdef Q_OS_WIN
        if(windowsMode){if(windowsInput.key(backspace)&&haptic)haptic(backspace?40:55);return;}
#endif
#ifdef DAISYWHEEL_KDE
        if(kde){if(kde->key(backspace?0xff08:0xff0d)&&haptic)haptic(backspace?40:55);return;}
#endif
        if(backspace){auto cursor=editor->textCursor();cursor.deletePreviousChar();editor->setTextCursor(cursor);if(haptic)haptic(40);}
        else {editor->insertPlainText("\n");if(haptic)haptic(55);}
    };
    auto sendNavigation=[&](int direction,bool shift,bool control){
        const unsigned symbols[]={0xff51,0xff53,0xff52,0xff54};
#ifdef Q_OS_WIN
        if(windowsMode){const WORD keys[]={VK_LEFT,VK_RIGHT,VK_UP,VK_DOWN};windowsInput.navigate(keys[direction],shift,control);return;}
#endif
#ifdef DAISYWHEEL_KDE
        if(kde){kde->navigate(symbols[direction],shift,control);return;}
#endif
        QTextCursor cursor=editor->textCursor();
        const QTextCursor::MoveOperation operations[]={
            control?QTextCursor::WordLeft:QTextCursor::Left,
            control?QTextCursor::WordRight:QTextCursor::Right,
            control?QTextCursor::PreviousBlock:QTextCursor::Up,
            control?QTextCursor::NextBlock:QTextCursor::Down};
        cursor.movePosition(operations[direction],shift?QTextCursor::KeepAnchor:QTextCursor::MoveAnchor);
        editor->setTextCursor(cursor);
    };
    auto pointerMove=[&](int dx,int dy){
#ifdef Q_OS_WIN
        if(windowsMode)windowsInput.mouseMove(dx,dy);
#endif
#ifdef DAISYWHEEL_KDE
        if(portalPointer)portalPointer->move(dx,dy);
#endif
    };
    auto pointerButton=[&](bool left,bool pressed){
#ifdef Q_OS_WIN
        if(windowsMode)windowsInput.mouseButton(left,pressed);
#endif
#ifdef DAISYWHEEL_KDE
        if(portalPointer)portalPointer->button(left,pressed);
#endif
    };
    auto pointerScroll=[&](int steps){
#ifdef Q_OS_WIN
        if(windowsMode)windowsInput.mouseScroll(steps);
#endif
#ifdef DAISYWHEEL_KDE
        if(portalPointer)portalPointer->scroll(steps);
#else
        Q_UNUSED(steps);
#endif
    };
    auto commit=[&](int slot){
        if(view->selected<0||!view->isVisible())return;
        char ch=(view->symbol?wheel::symbols:wheel::letters)[view->selected][slot];
        QString text(QChar::fromLatin1(ch));
        if(view->upper&&!view->symbol)text=text.toUpper();
        soundEffects.buttonPressed();
        sendText(text);
    };
    SDL_GameController *controller=nullptr;
    bool controllerRumbleAvailable=true;
    haptic=[&](Uint32 duration){
        if(!systemMode||!controller||!hapticsEnabled->isChecked())return;
        const int percent=hapticsStrength->value();
        const Uint16 strength=static_cast<Uint16>((percent*65535+50)/100);
        controllerRumbleAvailable=SDL_GameControllerHasRumble(controller)==SDL_TRUE;
        if(controllerRumbleAvailable&&SDL_GameControllerRumble(controller,strength,strength,duration)<0)
            controllerRumbleAvailable=false;
    };
    QTimer timer;
    QTimer backspaceRepeatTimer;
    backspaceRepeatTimer.setSingleShot(true);
    bool backspaceHeld=false;
    bool pointerMode=false;
    bool rightTriggerClickHeld=false;
    Uint32 pointerLastTick=SDL_GetTicks();
    double pointerRemainderX=0.0,pointerRemainderY=0.0;
    QList<Uint8> heldPointerButtons;
    auto releaseMouseButtons=[&]{
        for(Uint8 button:heldPointerButtons)
            pointerButton(button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,false);
        heldPointerButtons.clear();
        if(rightTriggerClickHeld){
            pointerButton(false,false);
            rightTriggerClickHeld=false;
        }
    };
    QObject::connect(&app,&QCoreApplication::aboutToQuit,&app,[&]{
        releaseMouseButtons();
#ifdef Q_OS_WIN
        if(windowsMode)windowsInput.setPointerPassthrough(false);
#endif
#ifdef DAISYWHEEL_KDE
        if(kde&&window.windowHandle())window.windowHandle()->setFlag(Qt::WindowTransparentForInput,false);
#endif
    });
    QObject::connect(&backspaceRepeatTimer,&QTimer::timeout,[&]{
        if(!backspaceHeld||!window.isVisible()||!view->isVisible()){
            backspaceHeld=false;
            return;
        }
        sendControl(true);
        backspaceRepeatTimer.setInterval(70);
        backspaceRepeatTimer.setSingleShot(false);
        backspaceRepeatTimer.start();
    });
    bool verificationSent=false;
    bool shortcutButtonDown=false;
    bool shortcutHoldCuePlayed=false;
    int previousSoundPetal=-1;
    Uint32 shortcutPressedAt=0;
    int editDirection=-1;
    Uint32 editDirectionStarted=0,editLastRepeat=0;
    QObject::connect(&app,&QApplication::focusChanged,&window,[&](QWidget*,QWidget *focused){
        if(!settingsPanel->isVisible()||!focused)return;
        settingsNavigation.syncFocus(focused);
    });
    QObject::connect(&timer,&QTimer::timeout,[&]{
        if(sdlOk){SDL_Event event;
            while(SDL_PollEvent(&event)){
                if(event.type==SDL_CONTROLLERDEVICEADDED&&!controller){
                    controller=SDL_GameControllerOpen(event.cdevice.which);
                    controllerRumbleAvailable=controller&&SDL_GameControllerHasRumble(controller)==SDL_TRUE;
                }
                if(event.type==SDL_CONTROLLERDEVICEREMOVED&&controller&&event.cdevice.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))){
                    releaseMouseButtons();
#ifdef Q_OS_WIN
                    if(windowsMode)windowsInput.setPointerPassthrough(false);
#endif
#ifdef DAISYWHEEL_KDE
                    if(kde&&window.windowHandle())window.windowHandle()->setFlag(Qt::WindowTransparentForInput,false);
#endif
                    SDL_GameControllerClose(controller);controller=nullptr;controllerRumbleAvailable=true;view->selected=-1;shortcutButtonDown=false;backspaceHeld=false;backspaceRepeatTimer.stop();backspaceRepeatTimer.setSingleShot(true);pointerMode=false;
                    if(capturingShortcut)updateShortcutCapture(false);
                }
                if(event.type==SDL_CONTROLLERBUTTONUP&&controller&&event.cbutton.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))){
                    if(event.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSTICK){
                        backspaceHeld=false;backspaceRepeatTimer.stop();backspaceRepeatTimer.setSingleShot(true);
                    }
                    if(heldPointerButtons.contains(event.cbutton.button)){
                        pointerButton(event.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,false);
                        heldPointerButtons.removeAll(event.cbutton.button);
                    }
                    if(event.cbutton.button==shortcutButton&&shortcutButtonDown){
                        const Uint32 held=SDL_GetTicks()-shortcutPressedAt;
                        if(held>=800){
                            if(!shortcutHoldCuePlayed&&window.isVisible()&&view->isVisible())soundEffects.buttonPressed();
                            if(!window.isVisible())manualToggle();
                            if(showSettings)showSettings();
                        }
                        else if(systemMode)manualToggle();
                        else if(window.isVisible()){if(showKeyboard)showKeyboard();window.hide();}
                        else {if(showKeyboard)showKeyboard();view->show();window.show();}
                        shortcutButtonDown=false;
                        shortcutHoldCuePlayed=false;
                    }
                    continue;
                }
                if(event.type!=SDL_CONTROLLERBUTTONDOWN||!controller||event.cbutton.which!=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)))continue;
                view->flashControllerButton(event.cbutton.button);
                if(activeHelpDialog&&activeHelpDialog->isVisible()){
                    const int button=event.cbutton.button;
                    if(button==SDL_CONTROLLER_BUTTON_A||button==SDL_CONTROLLER_BUTTON_B||button==SDL_CONTROLLER_BUTTON_START){
                        activeHelpDialog->accept();soundEffects.buttonPressed();
                    }
                    continue;
                }
                if(capturingShortcut){
                    shortcutButton=event.cbutton.button;
                    settings.setValue("shortcut",shortcutButton);
                    shortcutButtonDown=false;
                    updateShortcutCapture(false);
                    soundEffects.buttonPressed();
                    continue;
                }
                if(event.cbutton.button==shortcutButton) {
                    shortcutButtonDown=true;shortcutPressedAt=SDL_GetTicks();
                    shortcutHoldCuePlayed=false;
                    soundEffects.buttonPressed();
                    continue;
                }
                if(settingsPanel->isVisible()){
                    const int button=event.cbutton.button;
                    if(button==SDL_CONTROLLER_BUTTON_B){if(showKeyboard)showKeyboard();soundEffects.buttonPressed();}
                    else if(button==SDL_CONTROLLER_BUTTON_A||button==SDL_CONTROLLER_BUTTON_START){settingsNavigation.activate();soundEffects.buttonPressed();}
                    else if(button==SDL_CONTROLLER_BUTTON_DPAD_DOWN){settingsNavigation.move(true);soundEffects.buttonPressed();}
                    else if(button==SDL_CONTROLLER_BUTTON_DPAD_UP){settingsNavigation.move(false);soundEffects.buttonPressed();}
                    else if(button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER||button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER){
                        const int delta=button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER?1:-1;
                        settingsTabs->setCurrentIndex((settingsTabs->currentIndex()+delta+settingsTabs->count())%settingsTabs->count());
                        settingsNavigation.move(true);
                        soundEffects.buttonPressed();
                    }else if(button==SDL_CONTROLLER_BUTTON_DPAD_LEFT||button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT){
                        settingsNavigation.adjust(button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT?1:-1);
                        soundEffects.buttonPressed();
                    }
                    continue;
                }
                const bool mouseModifier=systemMode&&window.isVisible()&&view->isVisible()&&mouseModeEnabled->isChecked()&&SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>16000;
                if(mouseModifier){
                    switch(event.cbutton.button){
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        pointerButton(true,true);
                        if(!heldPointerButtons.contains(event.cbutton.button))heldPointerButtons.append(event.cbutton.button);
                        soundEffects.buttonPressed();
                        if(haptic)haptic(35);
                        break;
                    case SDL_CONTROLLER_BUTTON_X:pointerScroll(invertScrolling->isChecked()?-1:1);soundEffects.buttonPressed();if(haptic)haptic(25);break;
                    case SDL_CONTROLLER_BUTTON_Y:pointerScroll(invertScrolling->isChecked()?1:-1);soundEffects.buttonPressed();if(haptic)haptic(25);break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:pointerScroll(invertScrolling->isChecked()?-3:3);soundEffects.buttonPressed();if(haptic)haptic(25);break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:pointerScroll(invertScrolling->isChecked()?3:-3);soundEffects.buttonPressed();if(haptic)haptic(25);break;
                    default:break;
                    }
                    continue;
                }
                const bool editingModifier=SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>16000;
                if(editingModifier&&(event.cbutton.button==SDL_CONTROLLER_BUTTON_LEFTSHOULDER||event.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))continue;
                if(!window.isVisible())continue;
                if(!view->isVisible())continue;
                // Sample the stick before committing, including movement in this event batch.
                view->selected=wheel::sector(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX)/32768.0,
                                             SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY)/32768.0,
                                             deadzone->value()/100.0);
                switch(event.cbutton.button){
                case SDL_CONTROLLER_BUTTON_A:view->flashFaceButton(0);commit(0);break;
                case SDL_CONTROLLER_BUTTON_B:view->flashFaceButton(1);commit(1);break;
                case SDL_CONTROLLER_BUTTON_X:view->flashFaceButton(2);commit(2);break;
                case SDL_CONTROLLER_BUTTON_Y:view->flashFaceButton(3);commit(3);break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:if(!editingModifier){view->upper=!view->upper;soundEffects.buttonPressed();if(haptic)haptic(35);}break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:if(!editingModifier){view->symbol=!view->symbol;soundEffects.buttonPressed();if(haptic)haptic(35);}break;
                case SDL_CONTROLLER_BUTTON_LEFTSTICK:soundEffects.buttonPressed();sendText(" ");break;
                case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                    soundEffects.buttonPressed();
                    sendControl(true);
                    backspaceHeld=true;
                    backspaceRepeatTimer.setInterval(400);
                    backspaceRepeatTimer.setSingleShot(true);
                    backspaceRepeatTimer.start();
                    break;
                case SDL_CONTROLLER_BUTTON_START:soundEffects.buttonPressed();sendControl(false);break;
                default:break;
                }
            }
        }
        if(!window.isVisible()||!view->isVisible()){
            backspaceHeld=false;backspaceRepeatTimer.stop();backspaceRepeatTimer.setSingleShot(true);
        }
        bool mouseModeNow=controller&&systemMode&&window.isVisible()&&view->isVisible()&&mouseModeEnabled->isChecked()&&SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>16000;
        const bool rightTriggerClickNow=mouseModeNow&&SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>16000;
        if(rightTriggerClickNow!=rightTriggerClickHeld){
            pointerButton(false,rightTriggerClickNow);
            if(rightTriggerClickNow)soundEffects.buttonPressed();
            if(rightTriggerClickNow&&haptic)haptic(35);
        }
        rightTriggerClickHeld=rightTriggerClickNow;
        if(mouseModeNow&&!pointerMode){
#ifdef DAISYWHEEL_KDE
            if(portalPointer)portalPointer->requestAccess();
#endif
        }
        if(mouseModeNow!=pointerMode){
            if(!mouseModeNow)releaseMouseButtons();
#ifdef Q_OS_WIN
            if(windowsMode)windowsInput.setPointerPassthrough(mouseModeNow);
#endif
#ifdef DAISYWHEEL_KDE
            if(kde&&window.windowHandle())window.windowHandle()->setFlag(Qt::WindowTransparentForInput,mouseModeNow);
#endif
        }
        pointerMode=mouseModeNow;
        const Uint32 now=SDL_GetTicks();
        if(shortcutButtonDown){
            const Uint32 held=now-shortcutPressedAt;
            if(held>=800){
                view->setShortcutHint("Release to open Settings");
                if(!shortcutHoldCuePlayed&&window.isVisible()&&view->isVisible()){
                    soundEffects.buttonPressed();shortcutHoldCuePlayed=true;
                }
            }else{
                const int progress=qMin(99,int(held*100/800));
                view->setShortcutHint(QString("Hold %1 for Settings · %2%").arg(controllerButtonName(shortcutButton)).arg(progress));
            }
        }else view->setShortcutHint(QString("Hold %1 for Settings").arg(controllerButtonName(shortcutButton)));
        const double dt=qMin(0.05,(now-pointerLastTick)/1000.0);
        pointerLastTick=now;
        if(pointerMode&&controller){
            const auto stick=wheel::applyDeadzone(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX)/32768.0,
                                                  SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY)/32768.0,
                                                  deadzone->value()/100.0);
            pointerRemainderX+=stick.first*pointerSpeed->value()*dt;
            pointerRemainderY+=stick.second*pointerSpeed->value()*dt;
            const int dx=static_cast<int>(pointerRemainderX),dy=static_cast<int>(pointerRemainderY);
            pointerRemainderX-=dx;pointerRemainderY-=dy;
            pointerMove(dx,dy);
        }else{
            pointerRemainderX=pointerRemainderY=0.0;
        }
        if(controller&&!pointerMode){
            view->selected=wheel::sector(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX)/32768.0,SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY)/32768.0,deadzone->value()/100.0);
        }
        const bool editingNow=controller&&window.isVisible()&&view->isVisible()&&!settingsPanel->isVisible()&&!pointerMode&&SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>16000;
        if(editingNow){
            const auto stick=wheel::applyDeadzone(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX)/32768.0,
                                                  SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY)/32768.0,
                                                  deadzone->value()/100.0);
            int direction=-1;
            if(std::abs(stick.first)>std::abs(stick.second))direction=stick.first<0?0:1;
            else if(std::abs(stick.second)>0.0)direction=stick.second<0?2:3;
            if(direction<0)editDirection=-1;
            else if(direction!=editDirection){editDirection=direction;editDirectionStarted=now;editLastRepeat=now;sendNavigation(direction,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSHOULDER),SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));}
            else if(now-editDirectionStarted>=350&&now-editLastRepeat>=85){editLastRepeat=now;sendNavigation(direction,SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSHOULDER),SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));}
        }else editDirection=-1;
        if(window.isVisible()&&view->isVisible()&&!pointerMode&&view->selected>=0){
            if(previousSoundPetal>=0&&view->selected!=previousSoundPetal)
                soundEffects.petalChanged(view->selected);
            previousSoundPetal=view->selected;
        }else{
            previousSoundPetal=-1;
        }
        view->l1Down=controller&&SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        const bool leftTriggerDown=controller&&SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>16000;
        if(leftTriggerDown&&!view->l2Down)view->flashTrigger(true);
        view->l2Down=leftTriggerDown;
        view->r1Down=controller&&SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        const bool rightTriggerDown=controller&&SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>16000;
        if(rightTriggerDown&&!view->r2Down)view->flashTrigger(false);
        view->r2Down=rightTriggerDown;
        view->l3Down=controller&&SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSTICK);
        view->r3Down=controller&&SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSTICK);
        QString statusText=controller?QString(SDL_GameControllerName(controller)):sdlOk?"Connect a controller":"Controller initialization failed";
        if(pointerMode)statusText+=" · Mouse mode";
        if(editingNow)statusText+=" · Editing (hold RT)";
        if(soundEnabled->isChecked()&&!soundAvailable)statusText+=" · Sound output unavailable";
        if(systemMode&&controller&&hapticsEnabled->isChecked()&&!controllerRumbleAvailable)
            statusText+=" · Controller rumble unavailable";
#ifdef DAISYWHEEL_KDE
        if(portalPointer&&!portalPointer->status().isEmpty()&&!portalPointer->status().startsWith("Controller mouse ready"))
            statusText+=" · "+portalPointer->status();
        if(kde)statusText+=(kde->active()?" · Text field":" · Waiting for text field");
#endif
        status->setText(statusText);view->update();
#ifdef Q_OS_WIN
        if(windowsMode && !windowsInput.lastError().isEmpty())status->setText(windowsInput.lastError());
#endif
#ifdef DAISYWHEEL_KDE
        if(kde && kde->verificationTarget() && app.arguments().contains("--verify-input") && !verificationSent){
            verificationSent=kde->commit("daisywheel-test");
            std::fprintf(stderr,"Daisywheel: verification commit sent=%d\n",verificationSent);
        }
#endif
    });
#ifdef DAISYWHEEL_KDE
    QTimer policyTimer;
    bool policyQueryPending=false;
    if(kde){
        handle->pinned=pin->isChecked();
        handle->setText("Daisywheel · drag here to move");
        handle->geometryProvider=[&]{return kde->overlayGeometry();};
        resizeHandle->geometryProvider=[&]{return kde->overlayGeometry();};
        handle->geometryPreview=[&](const QRect &geometry){
            if(geometry.isNull())kde->clearOverlayGeometryPreview();
            else kde->previewOverlayGeometry(geometry);
        };
        handle->geometryChanged=[&](const QRect &geometry){
            kde->setOverlayGeometry(geometry);
        };
        handle->geometryCommitted=[&](const QRect &geometry){
            settings.setValue("kdeOverlayGeometry",geometry);
        };
        resizeHandle->geometryChanged=[&](const QRect &geometry){
            kde->setOverlayGeometry(geometry);
        };
        resizeHandle->geometryCommitted=[&](const QRect &geometry){
            settings.setValue("kdeOverlayGeometry",geometry);
        };
        resizeHandle->geometryPreview=[&](const QRect &geometry){
            if(geometry.isNull())kde->clearOverlayGeometryPreview();
            else kde->previewOverlayGeometry(geometry);
        };
        QObject::connect(pin,&QCheckBox::toggled,[&](bool checked){handle->pinned=checked;});
        window.setAttribute(Qt::WA_ShowWithoutActivating);
        kde->activation=[&](bool active){
            if(!active){visibility.contextEnded();if(!visibility.visible(false,false))window.hide();}
        };
        if(!kde->attach(&window)){
            std::cerr<<"KDE input method unavailable. Select Daisywheel in KDE Virtual Keyboard settings; KWin must launch it.\n";
            return 3;
        }
        if(settings.contains("kdeOverlayGeometry"))
            kde->setOverlayGeometry(settings.value("kdeOverlayGeometry").toRect());
        // Asynchronous policy queries also reflect live System Settings changes
        // and touch on a field whose input context was already active.
        QObject::connect(&policyTimer,&QTimer::timeout,&window,[&]{
            if(policyQueryPending)return;
            QDBusInterface keyboard("org.kde.KWin","/VirtualKeyboard","org.kde.kwin.VirtualKeyboard");
            policyQueryPending=true;
            auto *reply=new QDBusPendingCallWatcher(keyboard.asyncCall("willShowOnActive"),&window);
            QObject::connect(reply,&QDBusPendingCallWatcher::finished,&window,[&](QDBusPendingCallWatcher *watcher){
                QDBusPendingReply<bool> answer=*watcher;
                bool show=visibility.visible(kde->active(),!answer.isError()&&answer.value());
                if(show&&!window.isVisible()){view->show();window.show();}
                else if(!show&&window.isVisible())window.hide();
                policyQueryPending=false;watcher->deleteLater();
            });
        });
        policyTimer.start(200);
    }
#endif
    timer.start(16);
#ifdef Q_OS_WIN
    QSystemTrayIcon tray;
    QMenu trayMenu;
    QTimer windowsPolicyTimer;
    std::unique_ptr<WindowsFocus> focusWatcher;
    if(windowsMode){
        focusWatcher=std::make_unique<WindowsFocus>();
        windowsInput.attach(window);
        handle->setText("Daisywheel · Windows overlay · drag here to move");
        keyboardSettingsLayout->addWidget(new QLabel("Focus a text field, then use your controller shortcut to open"));
        auto *automatic=new QCheckBox("Open automatically when a text field is selected");
        automatic->setChecked(settings.value("windowsAutomatic",false).toBool());keyboardSettingsLayout->addWidget(automatic);
        focusWatcher->setEnabled(automatic->isChecked());
        QObject::connect(automatic,&QCheckBox::toggled,[&](bool enabled){settings.setValue("windowsAutomatic",enabled);focusWatcher->setEnabled(enabled);});
        auto *startup=new QCheckBox("Start Daisywheel when I sign in to Windows");
        QSettings startupSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",QSettings::NativeFormat);
        startup->setChecked(startupSettings.contains("DaisywheelREAmade"));keyboardSettingsLayout->addWidget(startup);
        QObject::connect(startup,&QCheckBox::toggled,[&](bool enabled){
            QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",QSettings::NativeFormat);
            if(enabled)run.setValue("DaisywheelREAmade",QString("\"%1\" --windows-input").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
            else run.remove("DaisywheelREAmade");
            run.sync();
            if(run.status()!=QSettings::NoError)status->setText("Could not update Windows startup setting");
        });
        QPixmap icon(32,32);icon.fill(Qt::transparent);
        {QPainter painter(&icon);painter.setRenderHint(QPainter::Antialiasing);painter.setBrush(QColor("#66d9ef"));painter.setPen(Qt::NoPen);
         for(int i=0;i<8;++i){double a=i*wheel::pi/4;painter.drawEllipse(QPointF(16+10*std::sin(a),16-10*std::cos(a)),4,4);}}
        tray.setIcon(QIcon(icon));tray.setToolTip("Daisywheel REAmade");
        trayMenu.addAction("Show / hide keyboard",&window,manualToggle);
        auto *autoAction=trayMenu.addAction("Open automatically for text fields");
        autoAction->setCheckable(true);autoAction->setChecked(automatic->isChecked());
        QObject::connect(autoAction,&QAction::toggled,automatic,&QCheckBox::setChecked);
        QObject::connect(automatic,&QCheckBox::toggled,autoAction,&QAction::setChecked);
        trayMenu.addAction("Quit",&app,&QApplication::quit);
        tray.setContextMenu(&trayMenu);
        QObject::connect(&tray,&QSystemTrayIcon::activated,[&](QSystemTrayIcon::ActivationReason reason){if(reason==QSystemTrayIcon::Trigger)manualToggle();});
        if(QSystemTrayIcon::isSystemTrayAvailable())tray.show();
        QByteArray lastContext;
        QObject::connect(&windowsPolicyTimer,&QTimer::timeout,&window,[&,automatic,lastContext]() mutable {
            HWND foreground=GetForegroundWindow();DWORD process=0;
            if(foreground)GetWindowThreadProcessId(foreground,&process);
            // Tray/settings interaction must not clear dismissal or hide the panel.
            if(process==GetCurrentProcessId())return;
            auto focus=focusWatcher->snapshot();
            const bool fresh=focus.foreground==foreground && GetTickCount64()-focus.sampledAt<1500;
            if(fresh && focus.known && focus.context!=lastContext){visibility.contextEnded();lastContext=focus.context;}
            const bool show=visibility.visible(fresh && focus.known && focus.editable,automatic->isChecked());
            if(show && !window.isVisible())showOverlay();
            else if(!show && window.isVisible())window.hide();
        });
        windowsPolicyTimer.start(200);
    }
#endif
    if(!windowsMode)window.adjustSize();
    soundAvailable=soundEffects.open();
    if(!systemMode)window.show();
    if(app.arguments().contains("--smoke-test"))QTimer::singleShot(1000,&app,&QApplication::quit);
    int result=app.exec();
    soundEffects.close();
    if(controller)SDL_GameControllerClose(controller);
    if(sdlOk)SDL_Quit();
    return result;
}
