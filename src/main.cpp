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
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWindow>
#include <QSettings>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QGraphicsOpacityEffect>
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
            const int width=qBound(minimumWidth,initialGeometry.width()-delta.x(),bounds.width());
            const int height=qBound(minimumHeight,initialGeometry.height()-delta.y(),bounds.height());
            const QRect geometry(initialGeometry.topLeft(),QSize(width,height));
            if(geometryChanged){
                pendingGeometry=geometry;
                if(geometryPreview)geometryPreview(geometry);
                event->accept();return;
            }
            window()->resize(width,height);
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
    WheelView() { setMinimumSize(420,360); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#111827"));
        QPointF center(width()/2.0,height()/2.0);
        // Eight circles need diameter < 2 * radius * sin(pi / 8).
        // Scale petals and labels together so the handheld layout stays legible.
        double radius=std::min(width(),height())*.35;
        double petalRadius=std::min(53.0,radius*.34);
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
                p.setPen(colors[j]);p.drawText(QRectF(c+offsets[j]-QPointF(14*scale,14*scale),QSizeF(28*scale,28*scale)),Qt::AlignCenter,text);
            }
        }
        p.setPen(QColor("#dce6f5"));
        p.drawText(QRectF(center-QPointF(67,40),QSizeF(134,80)),Qt::AlignCenter,
                   symbol?"Symbols":upper?"ABC":"abc");
    }
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
    if(kdeMode)kde=std::make_unique<KdeInput>();
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
    auto *headerWidget=new QWidget(content);
    auto *header=new QHBoxLayout(headerWidget);
    auto *resizeHandle=new ResizeHandle(kdeMode);
    header->addWidget(resizeHandle);
    auto *handle=new MoveHandle("Daisywheel · drag here to move");header->addWidget(handle,1);
    addControl(headerWidget);
    auto *pin=new QCheckBox("Pin position (lock dragging)");
    pin->setChecked(settings.value("pinned",false).toBool());handle->pinned=pin->isChecked();addControl(pin);
    auto *minimalMode=new QCheckBox("Minimal mode (wheel only)");
    minimalMode->setChecked(settings.value("minimalMode",false).toBool());addControl(minimalMode);
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
    auto *opacityLabel=new QLabel;addControl(opacityLabel);
    auto *opacity=new QSlider(Qt::Horizontal);opacity->setRange(30,100);
    opacity->setValue(settings.value("opacity",100).toInt());addControl(opacity);
    // Alpha composition works for the content on Wayland without relying on
    // compositor support for QWidget::setWindowOpacity.
    auto *alpha=new QGraphicsOpacityEffect(content);content->setGraphicsEffect(alpha);
    auto setOpacity=[&](int value){alpha->setOpacity(value/100.0);opacityLabel->setText(QString("Opacity: %1%").arg(value));settings.setValue("opacity",value);};
    setOpacity(opacity->value());QObject::connect(opacity,&QSlider::valueChanged,setOpacity);
    addControl(new QLabel("Controller shortcut (works while running in background)"));
    auto *shortcut=new QComboBox;
    shortcut->addItem("Home / Guide",SDL_CONTROLLER_BUTTON_GUIDE);
    shortcut->addItem("Back / View",SDL_CONTROLLER_BUTTON_BACK);
    shortcut->addItem("Start / Menu",SDL_CONTROLLER_BUTTON_START);
    int shortcutIndex=shortcut->findData(settings.value("shortcut",SDL_CONTROLLER_BUTTON_GUIDE));
    shortcut->setCurrentIndex(shortcutIndex<0?0:shortcutIndex);addControl(shortcut);
    QObject::connect(shortcut,&QComboBox::currentIndexChanged,[&](int){settings.setValue("shortcut",shortcut->currentData());});
    auto *status=new QLabel;addControl(status);
    auto *editor=new QPlainTextEdit;editor->setPlaceholderText("Tap here, then type with your controller");editor->setMaximumHeight(110);addControl(editor);
    editor->setVisible(!systemMode);
    auto *view=new WheelView;box->addWidget(view);
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
    addControl(new QLabel("Left stick: petal · Face buttons: character · LB: shift · RB: symbols\n"
                             "Left stick click: space · Right stick click: backspace · Start: enter · Back: hide\n"
                             "Tap ≡ or hold the selected controller shortcut to restore controls from Minimal mode"));
    auto *toggle=new QPushButton("Show / hide wheel");addControl(toggle);
    QObject::connect(toggle,&QPushButton::clicked,[=]{view->setVisible(!view->isVisible());});
    auto *hide=new QPushButton("Hide window · controller shortcut to reopen");addControl(hide);
    VisibilityPolicy visibility;
    QObject::connect(hide,&QPushButton::clicked,[&]{visibility.manual(false);window.hide();});
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
    auto *quit=new QPushButton("Quit");addControl(quit);
    QObject::connect(quit,&QPushButton::clicked,&app,&QApplication::quit);
    auto sendText=[&](const QString &text){
#ifdef Q_OS_WIN
        if(windowsMode){windowsInput.commit(text);return;}
#endif
#ifdef DAISYWHEEL_KDE
        if(kde){if(!kde->commit(text))status->setText("No active text field — focus a text field in another app");return;}
#endif
        editor->insertPlainText(text);
    };
    auto sendControl=[&](bool backspace){
#ifdef Q_OS_WIN
        if(windowsMode){windowsInput.key(backspace);return;}
#endif
#ifdef DAISYWHEEL_KDE
        if(kde){kde->key(backspace?0xff08:0xff0d);return;}
#endif
        if(backspace){auto cursor=editor->textCursor();cursor.deletePreviousChar();editor->setTextCursor(cursor);}
        else editor->insertPlainText("\n");
    };
    auto commit=[&](int slot){if(view->selected<0||!view->isVisible())return;
        char ch=(view->symbol?wheel::symbols:wheel::letters)[view->selected][slot];
        QString text(QChar::fromLatin1(ch));if(view->upper&&!view->symbol)text=text.toUpper();sendText(text);
    };
    SDL_GameController *controller=nullptr;
    QTimer timer;
    bool verificationSent=false;
    bool shortcutButtonDown=false;
    Uint32 shortcutPressedAt=0;
    QObject::connect(&timer,&QTimer::timeout,[&]{
        if(sdlOk){SDL_Event event;
            while(SDL_PollEvent(&event)){
                if(event.type==SDL_CONTROLLERDEVICEADDED&&!controller)controller=SDL_GameControllerOpen(event.cdevice.which);
                if(event.type==SDL_CONTROLLERDEVICEREMOVED&&controller&&event.cdevice.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))){SDL_GameControllerClose(controller);controller=nullptr;view->selected=-1;shortcutButtonDown=false;}
                if(event.type==SDL_CONTROLLERBUTTONUP&&controller&&event.cbutton.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))&&event.cbutton.button==shortcut->currentData().toInt()){
                    if(shortcutButtonDown){
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
                if(event.cbutton.button==shortcut->currentData().toInt()) {
                    shortcutButtonDown=true;shortcutPressedAt=SDL_GetTicks();
                    continue;
                }
                if(windowsMode && event.cbutton.button==SDL_CONTROLLER_BUTTON_BACK){manualToggle();continue;}
#ifdef DAISYWHEEL_KDE
                if(kde && event.cbutton.button==SDL_CONTROLLER_BUTTON_BACK){manualToggle();continue;}
#endif
                if(!window.isVisible())continue;
                if(!view->isVisible()){if(event.cbutton.button==SDL_CONTROLLER_BUTTON_BACK)view->show();continue;}
                // Sample the stick before committing, including movement in this event batch.
                view->selected=wheel::sector(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX)/32768.0,SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY)/32768.0);
                switch(event.cbutton.button){
                case SDL_CONTROLLER_BUTTON_A:commit(0);break;
                case SDL_CONTROLLER_BUTTON_B:commit(1);break;
                case SDL_CONTROLLER_BUTTON_X:commit(2);break;
                case SDL_CONTROLLER_BUTTON_Y:commit(3);break;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:view->upper=!view->upper;break;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:view->symbol=!view->symbol;break;
                case SDL_CONTROLLER_BUTTON_LEFTSTICK:sendText(" ");break;
                case SDL_CONTROLLER_BUTTON_RIGHTSTICK:sendControl(true);break;
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
        if(controller)view->selected=wheel::sector(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX)/32768.0,SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY)/32768.0);
        status->setText(controller?QString("Connected: %1").arg(SDL_GameControllerName(controller)):sdlOk?"Connect a controller":"Controller initialization failed");view->update();
#ifdef Q_OS_WIN
        if(windowsMode && !windowsInput.lastError().isEmpty())status->setText(windowsInput.lastError());
#endif
#ifdef DAISYWHEEL_KDE
        if(kde)status->setText(status->text()+(kde->active()?" · text field active":" · waiting for text field"));
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
        addControl(new QLabel("Automatic opening follows KDE → Virtual Keyboard\nHome / Back opens manually regardless of that setting"));
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
        addControl(new QLabel("Focus a text field, then use your controller shortcut to open"));
        auto *automatic=new QCheckBox("Open automatically when a text field is selected");
        automatic->setChecked(settings.value("windowsAutomatic",false).toBool());addControl(automatic);
        focusWatcher->setEnabled(automatic->isChecked());
        QObject::connect(automatic,&QCheckBox::toggled,[&](bool enabled){settings.setValue("windowsAutomatic",enabled);focusWatcher->setEnabled(enabled);});
        auto *startup=new QCheckBox("Start Daisywheel when I sign in to Windows");
        QSettings startupSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",QSettings::NativeFormat);
        startup->setChecked(startupSettings.contains("DaisywheelREAmade"));addControl(startup);
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
