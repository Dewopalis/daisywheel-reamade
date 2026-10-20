#include "wheel.h"
#include "visibility.h"
#include "placement.h"
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
#include <QSystemTrayIcon>
#include <QMenu>
#include <QScreen>
#include <QCursor>
#include <QList>
#include <QDir>
#include <QLockFile>
#include <QStandardPaths>
#include <functional>
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

class MoveHandle : public QLabel {
public:
    bool pinned=false;
    std::function<void(const QRect &)> geometryChanged;
    std::function<void(const QRect &)> geometryCommitted;
    std::function<QRect()> geometryProvider;
    std::function<void(const QRect &)> geometryPreview;
    using QLabel::QLabel;
    void mousePressEvent(QMouseEvent *event) override {
        if(pinned || event->button()!=Qt::LeftButton)return;
        origin=event->globalPosition().toPoint();initialGeometry=geometryProvider?geometryProvider():window()->geometry();dragging=true;
        if(!geometryChanged && window()->windowHandle())window()->windowHandle()->startSystemMove();
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if(!dragging || pinned){QLabel::mouseMoveEvent(event);return;}
        pendingGeometry=initialGeometry.translated(event->globalPosition().toPoint()-origin);
        if(geometryPreview)geometryPreview(pendingGeometry);
        event->accept();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if(event->button()==Qt::LeftButton){
            if(dragging && !pinned && geometryChanged){
                const QRect finalGeometry=initialGeometry.translated(event->globalPosition().toPoint()-origin);
                geometryChanged(finalGeometry);
                if(geometryCommitted)geometryCommitted(finalGeometry);
            }
            if(geometryPreview)geometryPreview(QRect());
            dragging=false;event->accept();return;
        }
        QLabel::mouseReleaseEvent(event);
    }
private:
    bool dragging=false;
    QPoint origin;
    QRect initialGeometry;
    QRect pendingGeometry;
};

class ResizeHandle : public QWidget {
public:
    explicit ResizeHandle(bool resizeInPlace, QWidget *parent=nullptr)
        : QWidget(parent), resizeInPlace(resizeInPlace) {
        setFixedSize(28,28);
        setCursor(Qt::SizeFDiagCursor);
        setToolTip("Drag this corner to resize Daisywheel");
        setAccessibleName("Resize Daisywheel");
    }
    std::function<QRect()> geometryProvider;
    std::function<void(const QRect &)> geometryCommitted;
    std::function<void(const QRect &)> geometryPreview;
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button()==Qt::LeftButton){
            origin=event->globalPosition().toPoint();
            initialGeometry=geometryProvider?geometryProvider():window()->geometry();
            resizing=true;
            pendingGeometry=initialGeometry;
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if(!resizing){QWidget::mouseMoveEvent(event);return;}
        const QPoint delta=event->globalPosition().toPoint()-origin;
        const QSize requestedMinimum=window()->minimumSizeHint().expandedTo(window()->minimumSize());
        QScreen *screen=window()->screen();
        if(!screen)screen=QGuiApplication::primaryScreen();
        const QRect bounds=screen?screen->availableGeometry():QRect(QPoint(0,0),initialGeometry.size());
        if(resizeInPlace){
            const int minimumWidth=qMin(requestedMinimum.width(),bounds.width());
            const int minimumHeight=qMin(requestedMinimum.height(),bounds.height());
            const int right=qBound(bounds.left()+minimumWidth,initialGeometry.x()+initialGeometry.width(),bounds.right()+1);
            const int bottom=qBound(bounds.top()+minimumHeight,initialGeometry.y()+initialGeometry.height(),bounds.bottom()+1);
            const int x=qBound(bounds.left(),initialGeometry.x()+delta.x(),right-minimumWidth);
            const int y=qBound(bounds.top(),initialGeometry.y()+delta.y(),bottom-minimumHeight);
            const QRect geometry(x,y,right-x,bottom-y);
            if(geometryChanged){
                pendingGeometry=geometry;
                if(geometryPreview)geometryPreview(geometry);
                event->accept();return;
            }
            window()->setGeometry(geometry);
        }else{
            const int minimumWidth=qMin(requestedMinimum.width(),bounds.width());
            const int minimumHeight=qMin(requestedMinimum.height(),bounds.height());
            const int right=qBound(bounds.left()+minimumWidth,initialGeometry.x()+initialGeometry.width(),bounds.right()+1);
            const int bottom=qBound(bounds.top()+minimumHeight,initialGeometry.y()+initialGeometry.height(),bounds.bottom()+1);
            const int x=qBound(bounds.left(),initialGeometry.x()+delta.x(),right-minimumWidth);
            const int y=qBound(bounds.top(),initialGeometry.y()+delta.y(),bottom-minimumHeight);
            window()->setGeometry(x,y,right-x,bottom-y);
        }
        if(geometryChanged)geometryChanged(window()->geometry());
        event->accept();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if(event->button()==Qt::LeftButton){
            if(resizing && resizeInPlace && geometryChanged && !pendingGeometry.isNull()){
                geometryChanged(pendingGeometry);
                if(geometryCommitted)geometryCommitted(pendingGeometry);
            }
            if(geometryPreview)geometryPreview(QRect());
            resizing=false;pendingGeometry=QRect();event->accept();return;
        }
        QWidget::mouseReleaseEvent(event);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#9aa8bd"),1.5));
        painter.drawLine(5,23,23,5);painter.drawLine(13,23,23,13);painter.drawLine(20,23,23,20);
    }
    std::function<void(const QRect &)> geometryChanged;
private:
    bool resizeInPlace=false;
    bool resizing=false;
    QPoint origin;
    QRect initialGeometry;
    QRect pendingGeometry;
};

class TouchActivation : public QObject {
public:
    std::function<void()> activate;
    bool eventFilter(QObject *, QEvent *event) override {
        if(event->type()==QEvent::TouchBegin ||
           (event->type()==QEvent::MouseButtonPress &&
            static_cast<QMouseEvent*>(event)->source()!=Qt::MouseEventNotSynthesized)) activate();
        return false;
    }
};

class WheelView : public QWidget {
public:
    int selected=-1;
    bool upper=false, symbol=false;
    bool l1Down=false,l2Down=false,r1Down=false,r2Down=false,l3Down=false,r3Down=false;
    WheelView() {
        setMinimumSize(420,360);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    }
    void flashFaceButton(int slot) {
        flashedFaceSlot=slot;
        faceFlashStarted=SDL_GetTicks();
        faceFlashActive=true;
        update();
    }
    void flashControllerButton(Uint8 button) {
        const Uint32 until=SDL_GetTicks()+150;
        switch(button){
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:l1FlashUntil=until;break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:r1FlashUntil=until;break;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:l3FlashUntil=until;break;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:r3FlashUntil=until;break;
        default:break;
        }
        update();
    }
    void flashTrigger(bool left) {
        (left?l2FlashUntil:r2FlashUntil)=SDL_GetTicks()+150;
        update();
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#111827"));
        QPointF center(width()/2.0,height()/2.0);
        // Eight circles need diameter < 2 * radius * sin(pi / 8).
        // Scale petals and labels together so the handheld layout stays legible.
        double radius=std::min(width(),height())*.35;
        double petalRadius=radius*.34;
        double scale=petalRadius/53.0;
        const auto &layout=symbol?wheel::symbols:wheel::letters;
        QFont font=p.font();font.setPixelSize(qRound(19*scale));p.setFont(font);
        const QColor colors[]={QColor("#5ee69a"),QColor("#ff7777"),QColor("#80baff"),QColor("#ffe07a")};
        const QPointF offsets[]={{0,23*scale},{29*scale,0},{-29*scale,0},{0,-23*scale}};
        for(int i=0;i<8;++i){
            double a=i*wheel::pi/4;
            QPointF c=center+QPointF(std::sin(a)*radius,-std::cos(a)*radius);
            p.setPen(QPen(i==selected?QColor("#66d9ef"):QColor("#394459"),i==selected?3:1));
            p.setBrush(QColor(i==selected?"#253c55":"#1c2636"));p.drawEllipse(c,petalRadius,petalRadius);
            for(int j=0;j<4;++j){
                QString text=QString(QChar(layout[i][j]));if(upper&&!symbol)text=text.toUpper();
                if(i==selected&&faceFlashActive&&j==flashedFaceSlot&&SDL_GetTicks()-faceFlashStarted<180){
                    QColor feedback=colors[j];feedback.setAlpha(55);
                    p.setPen(QPen(colors[j],2.5));p.setBrush(feedback);
                    p.drawEllipse(c+offsets[j],17*scale,17*scale);
                }
                p.setPen(colors[j]);p.drawText(QRectF(c+offsets[j]-QPointF(14*scale,14*scale),QSizeF(28*scale,28*scale)),Qt::AlignCenter,text);
            }
        }
        p.setPen(QColor("#dce6f5"));
        p.drawText(QRectF(center-QPointF(67,40),QSizeF(134,80)),Qt::AlignCenter,
                   symbol?"Symbols":upper?"ABC":"abc");
        auto drawShoulder=[&](const QRectF &r,const QString &label,bool down){
            p.setPen(QPen(down?QColor("#9cecff"):QColor("#536075"),down?2:1));
            p.setBrush(down?QColor("#66d9ef"):QColor("#202b3b"));p.drawRoundedRect(r,5,5);
            p.setPen(down?QColor("#101827"):QColor("#aab7ca"));
            p.drawText(r,Qt::AlignCenter,label);
        };
        const Uint32 now=SDL_GetTicks();
        auto lit=[now](bool down,Uint32 until){return down||(until&&Sint32(until-now)>0);};
        const qreal controlScale=qBound(0.65,qreal(std::min(width(),height()))/420.0,1.35);
        const qreal side=std::min(width(),height())<320?4:12*controlScale;
        const qreal shoulderWidth=48*controlScale,shoulderHeight=31*controlScale,gap=11*controlScale;
        drawShoulder(QRectF(side,side,shoulderWidth,shoulderHeight),"L2",lit(l2Down,l2FlashUntil));
        drawShoulder(QRectF(side,side+shoulderHeight+gap,shoulderWidth,shoulderHeight),"L1",lit(l1Down,l1FlashUntil));
        drawShoulder(QRectF(width()-side-shoulderWidth,side,shoulderWidth,shoulderHeight),"R1",lit(r1Down,r1FlashUntil));
        drawShoulder(QRectF(width()-side-shoulderWidth,side+shoulderHeight+gap,shoulderWidth,shoulderHeight),"R2",lit(r2Down,r2FlashUntil));
        auto drawStick=[&](const QPointF &c,const QString &label,bool down){
            const qreal radius=22*controlScale;
            p.setPen(QPen(down?QColor("#9cecff"):QColor("#536075"),down?2:1));
            p.setBrush(down?QColor("#66d9ef"):QColor("#202b3b"));p.drawEllipse(c,radius,radius);
            p.setPen(down?QColor("#101827"):QColor("#aab7ca"));
            p.drawText(QRectF(c-QPointF(radius,radius),QSizeF(radius*2,radius*2)),Qt::AlignCenter,label);
        };
        const qreal stickRadius=22*controlScale;
        drawStick(QPointF(side+stickRadius,height()-side-stickRadius),"L3",lit(l3Down,l3FlashUntil));
        drawStick(QPointF(width()-side-stickRadius,height()-side-stickRadius),"R3",lit(r3Down,r3FlashUntil));
    }
private:
    int flashedFaceSlot=-1;
    Uint32 faceFlashStarted=0;
    Uint32 l1FlashUntil=0,l2FlashUntil=0,r1FlashUntil=0,r2FlashUntil=0,l3FlashUntil=0,r3FlashUntil=0;
    bool faceFlashActive=false;
};

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
    window.setWindowTitle("Daisywheel — controller prototype");
    app.setQuitOnLastWindowClosed(false);
    window.setWindowFlags(Qt::Window|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    window.setAttribute(Qt::WA_TranslucentBackground);
    auto *box=new QVBoxLayout(&window);
    auto *windowLayout=box;
    const QMargins fullWindowMargins=windowLayout->contentsMargins();
    const int fullWindowSpacing=windowLayout->spacing();
    auto *content=new QWidget;content->setObjectName("panel");
    content->setStyleSheet("QWidget#panel { background: #1d2229; border: 1px solid #536075; border-radius: 10px; }");
    box->addWidget(content);
    box=new QVBoxLayout(content);
    const QMargins fullContentMargins=box->contentsMargins();
    const int fullContentSpacing=box->spacing();
    QList<QWidget*> fullModeWidgets;
    auto addControl=[&](QWidget *control){box->addWidget(control);fullModeWidgets.append(control);return control;};
    auto *settingsDialog=new QDialog(&window);
    settingsDialog->setWindowTitle("Daisywheel settings");
    settingsDialog->setWindowFlag(Qt::WindowStaysOnTopHint,true);
    settingsDialog->setMinimumWidth(380);
    auto *settingsLayout=new QVBoxLayout(settingsDialog);
    auto *windowSettingsGroup=new QGroupBox("Window");
    auto *windowSettingsLayout=new QFormLayout(windowSettingsGroup);
    auto *controllerSettingsGroup=new QGroupBox("Controller");
    auto *controllerSettingsLayout=new QFormLayout(controllerSettingsGroup);
    auto *keyboardSettingsGroup=new QGroupBox("Keyboard");
    auto *keyboardSettingsLayout=new QVBoxLayout(keyboardSettingsGroup);
    auto *settingsTabs=new QTabWidget(settingsDialog);
    settingsTabs->addTab(windowSettingsGroup,"Window");
    settingsTabs->addTab(controllerSettingsGroup,"Controller");
    settingsTabs->addTab(keyboardSettingsGroup,"Keyboard");
    settingsLayout->addWidget(settingsTabs);
    auto *settingsButtons=new QDialogButtonBox(QDialogButtonBox::Help|QDialogButtonBox::Close);
    QObject::connect(settingsButtons,&QDialogButtonBox::rejected,settingsDialog,&QDialog::hide);
    settingsLayout->addWidget(settingsButtons);
    QObject::connect(settingsButtons,&QDialogButtonBox::helpRequested,[settingsDialog]{
        QMessageBox help(QMessageBox::Information,"Daisywheel controls",QString(),QMessageBox::Close,settingsDialog);
        help.setTextFormat(Qt::RichText);
        help.setMinimumWidth(480);
        help.setText(
            "<h3>Choose and type</h3>"
            "<p>Move the left stick to select one of the eight petals. Press a face button "
            "to choose its matching character: A is bottom, B right, X left, and Y top. "
            "LB switches uppercase; RB switches between letters and symbols.</p>"
            "<h3>Other controls</h3>"
            "<p><b>L3</b> inserts a space. <b>R3</b> deletes; hold it to repeat. "
            "<b>Start</b> inserts Enter. Use your selected controller shortcut to show or hide the keyboard.</p>"
            "<h3>Mouse mode</h3>"
            "<p>When enabled, mouse mode only works while the keyboard is visible. Hold <b>LT</b> "
            "and move the left stick to move the pointer. <b>R1</b> clicks and <b>R2</b> right-clicks; "
            "<b>X/Y</b> or the D-pad scroll. Release LT to return to typing.</p>"
            "<h3>Window</h3>"
            "<p>Drag the title area to move Daisywheel. Use the corner grip to resize. "
            "Settings contains position, transparency, controller, and keyboard options.</p>");
        help.exec();
    });
    auto showSettings=[settingsDialog]{settingsDialog->show();settingsDialog->raise();settingsDialog->activateWindow();};
    auto *headerWidget=new QWidget(content);
    auto *header=new QHBoxLayout(headerWidget);
    header->setContentsMargins(0,0,0,0);
    headerWidget->setFixedHeight(32);
    auto *resizeHandle=new ResizeHandle(kdeMode);
    header->addWidget(resizeHandle);
    auto *handle=new MoveHandle("Daisywheel · drag here to move");header->addWidget(handle,1);
    auto *settingsButton=new QPushButton("Settings");settingsButton->setFixedHeight(26);
    header->addWidget(settingsButton);QObject::connect(settingsButton,&QPushButton::clicked,showSettings);
    addControl(headerWidget);
    auto *pin=new QCheckBox("Pin position (lock dragging)");
    pin->setChecked(settings.value("pinned",false).toBool());handle->pinned=pin->isChecked();windowSettingsLayout->addRow(pin);
    auto *minimalMode=new QCheckBox("Minimal mode (wheel only)");
    minimalMode->setChecked(settings.value("minimalMode",false).toBool());windowSettingsLayout->addRow(minimalMode);
    bool minimalActive=minimalMode->isChecked();
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
    opacity->setValue(settings.value("opacity",100).toInt());windowSettingsLayout->addRow(opacityLabel,opacity);
    // Alpha composition works for the content on Wayland without relying on
    // compositor support for QWidget::setWindowOpacity.
    auto *alpha=new QGraphicsOpacityEffect(content);content->setGraphicsEffect(alpha);
    auto setOpacity=[&](int value){alpha->setOpacity(value/100.0);opacityLabel->setText(QString("Opacity: %1%").arg(value));settings.setValue("opacity",value);};
    setOpacity(opacity->value());QObject::connect(opacity,&QSlider::valueChanged,setOpacity);
    auto *controllerProfile=new QComboBox;
    controllerProfile->addItem("Handheld", "handheld");
    controllerProfile->addItem("Living room / desktop", "living-room");
    controllerProfile->addItem("Custom", "custom");
    QString savedProfile=settings.value("controllerProfile","handheld").toString();
    int savedProfileIndex=controllerProfile->findData(savedProfile);
    controllerProfile->setCurrentIndex(savedProfileIndex<0?0:savedProfileIndex);
    controllerSettingsLayout->addRow("Profile",controllerProfile);
    auto *mouseModeEnabled=new QCheckBox("Enable mouse mode (only while keyboard is open)");
    mouseModeEnabled->setChecked(settings.value("controllerMouseEnabled",savedProfile=="living-room").toBool());
    controllerSettingsLayout->addRow(mouseModeEnabled);
    auto *pointerSpeedLabel=new QLabel;
    auto *pointerSpeed=new QSlider(Qt::Horizontal);pointerSpeed->setRange(250,1800);
    pointerSpeed->setValue(settings.value("pointerSpeed",950).toInt());controllerSettingsLayout->addRow(pointerSpeedLabel,pointerSpeed);
    auto *hapticsEnabled=new QCheckBox("Enable haptic feedback");
    hapticsEnabled->setChecked(settings.value("hapticsEnabled",false).toBool());controllerSettingsLayout->addRow(hapticsEnabled);
    auto *hapticsStrengthLabel=new QLabel;
    auto *hapticsStrength=new QSlider(Qt::Horizontal);hapticsStrength->setRange(10,100);
    hapticsStrength->setValue(settings.value("hapticsStrength",50).toInt());controllerSettingsLayout->addRow(hapticsStrengthLabel,hapticsStrength);
    hapticsStrengthLabel->setText(QString("Haptic strength: %1%").arg(hapticsStrength->value()));
    controllerProfile->setVisible(systemMode);mouseModeEnabled->setVisible(systemMode);
    pointerSpeedLabel->setVisible(systemMode);pointerSpeed->setVisible(systemMode);
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
    QObject::connect(controllerProfile,&QComboBox::currentIndexChanged,[&](int index){
        const QString profile=controllerProfile->itemData(index).toString();
        settings.setValue("controllerProfile",profile);
        if(profile=="custom")return;
        applyingControllerPreset=true;
        mouseModeEnabled->setChecked(profile=="living-room");
        pointerSpeed->setValue(950);
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
    auto *shortcut=new QComboBox;
    shortcut->addItem("Home / Guide",SDL_CONTROLLER_BUTTON_GUIDE);
    shortcut->addItem("Back / View",SDL_CONTROLLER_BUTTON_BACK);
    shortcut->addItem("Start / Menu",SDL_CONTROLLER_BUTTON_START);
    int shortcutIndex=shortcut->findData(settings.value("shortcut",SDL_CONTROLLER_BUTTON_GUIDE));
    shortcut->setCurrentIndex(shortcutIndex<0?0:shortcutIndex);controllerSettingsLayout->addRow("Controller shortcut",shortcut);
    QObject::connect(shortcut,&QComboBox::currentIndexChanged,[&](int){settings.setValue("shortcut",shortcut->currentData());});
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
    box->setStretch(box->indexOf(headerWidget),0);
    box->setStretch(box->indexOf(status),0);
    box->setStretch(box->indexOf(view),1);
    if(windowsMode)view->setMinimumSize(360,310);
    auto *minimalBar=new QWidget(content);
    auto *minimalBarLayout=new QHBoxLayout(minimalBar);
    minimalBarLayout->setContentsMargins(2,1,2,1);
    minimalBarLayout->setSpacing(2);
    minimalBar->setFixedHeight(30);
    auto *minimalMoveHandle=new MoveHandle("⠿",minimalBar);
    minimalMoveHandle->setAlignment(Qt::AlignCenter);
    minimalMoveHandle->setFixedSize(28,26);
    minimalMoveHandle->setToolTip("Drag here to move the wheel");
    minimalMoveHandle->setAccessibleName("Drag to move Daisywheel");
    minimalMoveHandle->setCursor(Qt::SizeAllCursor);
    minimalBarLayout->addWidget(minimalMoveHandle);
    minimalBarLayout->addStretch();
    auto *minimalSettingsButton=new QPushButton("Settings",minimalBar);
    minimalSettingsButton->setToolTip("Open Daisywheel settings");
    minimalSettingsButton->setAccessibleName("Open Daisywheel settings");
    minimalSettingsButton->setFixedSize(68,26);
    minimalBarLayout->addWidget(minimalSettingsButton);
    QObject::connect(minimalSettingsButton,&QPushButton::clicked,showSettings);
    auto *restoreControls=new QPushButton("Show controls",minimalBar);
    restoreControls->setToolTip("Show full controls");
    restoreControls->setAccessibleName("Show full controls");
    restoreControls->setFixedSize(96,26);
    minimalBarLayout->addWidget(restoreControls);
    box->insertWidget(box->indexOf(view),minimalBar);
    minimalBar->hide();
    QObject::connect(restoreControls,&QPushButton::clicked,[&]{minimalMode->setChecked(false);});
    minimalMoveHandle->pinned=pin->isChecked();
    QObject::connect(pin,&QCheckBox::toggled,[minimalMoveHandle](bool checked){minimalMoveHandle->pinned=checked;});
    TouchActivation activation;activation.activate=[=]{view->show();};
    editor->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    editor->viewport()->installEventFilter(&activation);
    auto *toggle=new QPushButton("Show / hide wheel");keyboardSettingsLayout->addWidget(toggle);
    QObject::connect(toggle,&QPushButton::clicked,[=]{view->setVisible(!view->isVisible());});
    auto *hide=new QPushButton("Hide keyboard");keyboardSettingsLayout->addWidget(hide);
    VisibilityPolicy visibility;
    QObject::connect(hide,&QPushButton::clicked,[&]{settingsDialog->hide();visibility.manual(false);window.hide();});
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
        if(show)showOverlay();else window.hide();
#ifdef DAISYWHEEL_KDE
        if(show && kde){
            QDBusInterface keyboard("org.kde.KWin","/VirtualKeyboard","org.kde.kwin.VirtualKeyboard");
            keyboard.asyncCall("forceActivate");
        }
#endif
    };
    auto *quit=new QPushButton("Quit Daisywheel");keyboardSettingsLayout->addWidget(quit);
    QObject::connect(quit,&QPushButton::clicked,&app,&QApplication::quit);
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
    auto commit=[&](int slot){if(view->selected<0||!view->isVisible())return;
        char ch=(view->symbol?wheel::symbols:wheel::letters)[view->selected][slot];
        QString text(QChar::fromLatin1(ch));if(view->upper&&!view->symbol)text=text.toUpper();sendText(text);
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
    Uint32 shortcutPressedAt=0;
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
                }
                if(event.type==SDL_CONTROLLERBUTTONUP&&controller&&event.cbutton.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))){
                    if(event.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSTICK){
                        backspaceHeld=false;backspaceRepeatTimer.stop();backspaceRepeatTimer.setSingleShot(true);
                    }
                    if(heldPointerButtons.contains(event.cbutton.button)){
                        pointerButton(event.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,false);
                        heldPointerButtons.removeAll(event.cbutton.button);
                    }
                    if(event.cbutton.button==shortcut->currentData().toInt()&&shortcutButtonDown){
                        const Uint32 held=SDL_GetTicks()-shortcutPressedAt;
                        if(minimalActive&&window.isVisible()&&held>=800)minimalMode->setChecked(false);
                        else if(systemMode)manualToggle();
                        else if(window.isVisible()&&view->isVisible())window.hide();
                        else {view->show();window.show();}
                        shortcutButtonDown=false;
                    }
                    continue;
                }
                if(event.type!=SDL_CONTROLLERBUTTONDOWN||!controller||event.cbutton.which!=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)))continue;
                view->flashControllerButton(event.cbutton.button);
                if(event.cbutton.button==shortcut->currentData().toInt()) {
                    shortcutButtonDown=true;shortcutPressedAt=SDL_GetTicks();
                    continue;
                }
                if(windowsMode && event.cbutton.button==SDL_CONTROLLER_BUTTON_BACK){manualToggle();continue;}
#ifdef DAISYWHEEL_KDE
                if(kde && event.cbutton.button==SDL_CONTROLLER_BUTTON_BACK){manualToggle();continue;}
#endif
                const bool mouseModifier=systemMode&&window.isVisible()&&view->isVisible()&&mouseModeEnabled->isChecked()&&SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>16000;
                if(mouseModifier){
                    switch(event.cbutton.button){
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        pointerButton(true,true);
                        if(!heldPointerButtons.contains(event.cbutton.button))heldPointerButtons.append(event.cbutton.button);
                        if(haptic)haptic(35);
                        break;
                    case SDL_CONTROLLER_BUTTON_X:pointerScroll(1);if(haptic)haptic(25);break;
                    case SDL_CONTROLLER_BUTTON_Y:pointerScroll(-1);if(haptic)haptic(25);break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:pointerScroll(3);if(haptic)haptic(25);break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:pointerScroll(-3);if(haptic)haptic(25);break;
                    default:break;
                    }
                    continue;
                }
                if(!window.isVisible())continue;
                if(!view->isVisible()){if(event.cbutton.button==SDL_CONTROLLER_BUTTON_BACK)view->show();continue;}
                // Sample the stick before committing, including movement in this event batch.
                view->selected=wheel::sector(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX)/32768.0,SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY)/32768.0);
                switch(event.cbutton.button){
                case SDL_CONTROLLER_BUTTON_A:view->flashFaceButton(0);commit(0);break;
                case SDL_CONTROLLER_BUTTON_B:view->flashFaceButton(1);commit(1);break;
                case SDL_CONTROLLER_BUTTON_X:view->flashFaceButton(2);commit(2);break;
                case SDL_CONTROLLER_BUTTON_Y:view->flashFaceButton(3);commit(3);break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:view->upper=!view->upper;if(haptic)haptic(35);break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:view->symbol=!view->symbol;if(haptic)haptic(35);break;
                case SDL_CONTROLLER_BUTTON_LEFTSTICK:sendText(" ");break;
                case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                    sendControl(true);
                    backspaceHeld=true;
                    backspaceRepeatTimer.setInterval(400);
                    backspaceRepeatTimer.setSingleShot(true);
                    backspaceRepeatTimer.start();
                    break;
                case SDL_CONTROLLER_BUTTON_START:sendControl(false);break;
                case SDL_CONTROLLER_BUTTON_BACK:
#ifdef DAISYWHEEL_KDE
                    if(kde){
                        manualToggle();
                        break;
                    }
#endif
                    view->hide();break;
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
        const double dt=qMin(0.05,(now-pointerLastTick)/1000.0);
        pointerLastTick=now;
        if(pointerMode&&controller){
            auto curve=[](Sint16 raw){
                const double value=raw/32768.0;
                const double magnitude=std::abs(value);
                if(magnitude<=0.18)return 0.0;
                const double normalized=(magnitude-0.18)/0.82;
                return std::copysign(std::pow(normalized,1.6),value);
            };
            pointerRemainderX+=curve(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX))*pointerSpeed->value()*dt;
            pointerRemainderY+=curve(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY))*pointerSpeed->value()*dt;
            const int dx=static_cast<int>(pointerRemainderX),dy=static_cast<int>(pointerRemainderY);
            pointerRemainderX-=dx;pointerRemainderY-=dy;
            pointerMove(dx,dy);
        }else{
            pointerRemainderX=pointerRemainderY=0.0;
        }
        if(controller&&!pointerMode){
            view->selected=wheel::sector(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX)/32768.0,SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY)/32768.0);
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
        minimalMoveHandle->geometryProvider=[&]{return kde->overlayGeometry();};
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
        minimalMoveHandle->geometryChanged=handle->geometryChanged;
        minimalMoveHandle->geometryCommitted=handle->geometryCommitted;
        minimalMoveHandle->geometryPreview=[&](const QRect &geometry){
            if(geometry.isNull())kde->clearOverlayGeometryPreview();
            else kde->previewOverlayGeometry(geometry);
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
        QObject::connect(pin,&QCheckBox::toggled,[minimalMoveHandle](bool checked){minimalMoveHandle->pinned=checked;});
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
    auto applyMinimalMode=[&](bool enabled){
        minimalActive=enabled;
        for(auto *control:fullModeWidgets)
            control->setVisible(!enabled && (control!=editor || !systemMode));
        view->setMinimumSize(enabled?QSize(280,280):windowsMode?QSize(360,310):QSize(420,360));
        minimalBar->setVisible(enabled);
        if(enabled){
            windowLayout->setContentsMargins(0,0,0,0);windowLayout->setSpacing(0);
            box->setContentsMargins(1,1,1,1);box->setSpacing(0);
            window.resize(view->minimumSize().width()+2,view->minimumSize().height()+32);
        }else{
            windowLayout->setContentsMargins(fullWindowMargins);windowLayout->setSpacing(fullWindowSpacing);
            box->setContentsMargins(fullContentMargins);box->setSpacing(fullContentSpacing);
            window.adjustSize();
        }
#ifdef DAISYWHEEL_KDE
        if(kde){
            QRect geometry=kde->overlayGeometry();geometry.setSize(window.size());
            kde->setOverlayGeometry(geometry);
            settings.setValue("kdeOverlayGeometry",kde->overlayGeometry());
        }
#endif
    };
    QObject::connect(minimalMode,&QCheckBox::toggled,[&](bool enabled){
        settings.setValue("minimalMode",enabled);applyMinimalMode(enabled);
    });
    applyMinimalMode(minimalActive);
    if(!windowsMode)window.adjustSize();
    if(!systemMode)window.show();
    if(app.arguments().contains("--smoke-test"))QTimer::singleShot(1000,&app,&QApplication::quit);
    int result=app.exec();
    if(controller)SDL_GameControllerClose(controller);if(sdlOk)SDL_Quit();return result;
}
