#include "wheel.h"
#include "visibility.h"
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
#include <functional>
#include <iostream>
#include <SDL.h>
#ifdef DAISYWHEEL_KDE
#include "kdeinput.h"
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

class MoveHandle : public QLabel {
public:
    bool pinned=false;
    using QLabel::QLabel;
    void mousePressEvent(QMouseEvent *event) override {
        if(!pinned && event->button()==Qt::LeftButton && window()->windowHandle())
            window()->windowHandle()->startSystemMove();
    }
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
    WheelView() { setMinimumSize(580,500); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#111827"));
        QPointF center(width()/2.0,height()/2.0);
        double radius=std::min(width(),height())*.33;
        const auto &layout=symbol?wheel::symbols:wheel::letters;
        QFont font=p.font();font.setPixelSize(19);p.setFont(font);
        const QColor colors[]={QColor("#5ee69a"),QColor("#ff7777"),QColor("#80baff"),QColor("#ffe07a")};
        const QPoint offsets[]={{0,23},{29,0},{-29,0},{0,-23}};
        for(int i=0;i<8;++i){
            double a=i*wheel::pi/4;
            QPointF c=center+QPointF(std::sin(a)*radius,-std::cos(a)*radius);
            p.setPen(QPen(i==selected?QColor("#66d9ef"):QColor("#394459"),i==selected?3:1));
            p.setBrush(QColor(i==selected?"#253c55":"#1c2636"));p.drawEllipse(c,53,53);
            for(int j=0;j<4;++j){
                QString text=QString(QChar(layout[i][j]));if(upper&&!symbol)text=text.toUpper();
                p.setPen(colors[j]);p.drawText(QRectF(c+offsets[j]-QPointF(14,14),QSizeF(28,28)),Qt::AlignCenter,text);
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
    const bool systemMode=app.arguments().contains("--kde-input-method");
#ifdef DAISYWHEEL_KDE
    std::unique_ptr<KdeInput> kde;
    if(systemMode)kde=std::make_unique<KdeInput>();
#else
    if(systemMode){std::cerr<<"KDE backend unavailable in this build\n";return 1;}
#endif
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
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
    QWidget window;window.setWindowTitle("Daisywheel — controller prototype");
    app.setQuitOnLastWindowClosed(false);
    window.setWindowFlags(Qt::Window|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    window.setAttribute(Qt::WA_TranslucentBackground);
    auto *box=new QVBoxLayout(&window);
    auto *content=new QWidget;content->setObjectName("panel");
    content->setStyleSheet("QWidget#panel { background: #1d2229; border: 1px solid #536075; border-radius: 10px; }");
    box->addWidget(content);
    box=new QVBoxLayout(content);
    auto *handle=new MoveHandle("Daisywheel · drag here to move");box->addWidget(handle);
    auto *pin=new QCheckBox("Pin position (lock dragging)");
    pin->setChecked(settings.value("pinned",false).toBool());handle->pinned=pin->isChecked();box->addWidget(pin);
    QObject::connect(pin,&QCheckBox::toggled,[&](bool checked){handle->pinned=checked;settings.setValue("pinned",checked);});
    auto *opacityLabel=new QLabel;box->addWidget(opacityLabel);
    auto *opacity=new QSlider(Qt::Horizontal);opacity->setRange(30,100);
    opacity->setValue(settings.value("opacity",100).toInt());box->addWidget(opacity);
    // Alpha composition works for the content on Wayland without relying on
    // compositor support for QWidget::setWindowOpacity.
    auto *alpha=new QGraphicsOpacityEffect(content);content->setGraphicsEffect(alpha);
    auto setOpacity=[&](int value){alpha->setOpacity(value/100.0);opacityLabel->setText(QString("Opacity: %1%").arg(value));settings.setValue("opacity",value);};
    setOpacity(opacity->value());QObject::connect(opacity,&QSlider::valueChanged,setOpacity);
    box->addWidget(new QLabel("Controller shortcut (works while running in background)"));
    auto *shortcut=new QComboBox;
    shortcut->addItem("Home / Guide",SDL_CONTROLLER_BUTTON_GUIDE);
    shortcut->addItem("Back / View",SDL_CONTROLLER_BUTTON_BACK);
    shortcut->addItem("Start / Menu",SDL_CONTROLLER_BUTTON_START);
    int shortcutIndex=shortcut->findData(settings.value("shortcut",SDL_CONTROLLER_BUTTON_GUIDE));
    shortcut->setCurrentIndex(shortcutIndex<0?0:shortcutIndex);box->addWidget(shortcut);
    QObject::connect(shortcut,&QComboBox::currentIndexChanged,[&](int){settings.setValue("shortcut",shortcut->currentData());});
    auto *status=new QLabel;box->addWidget(status);
    auto *editor=new QPlainTextEdit;editor->setPlaceholderText("Tap here, then type with your controller");editor->setMaximumHeight(110);box->addWidget(editor);
    editor->setVisible(!systemMode);
    auto *view=new WheelView;box->addWidget(view);
    TouchActivation activation;activation.activate=[=]{view->show();};
    editor->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    editor->viewport()->installEventFilter(&activation);
    box->addWidget(new QLabel("Left stick: petal · Face buttons: character · LB: shift · RB: symbols\n"
                             "Left stick click: space · Right stick click: backspace · Start: enter · Back: hide"));
    auto *toggle=new QPushButton("Show / hide wheel");box->addWidget(toggle);
    QObject::connect(toggle,&QPushButton::clicked,[=]{view->setVisible(!view->isVisible());});
    auto *hide=new QPushButton("Hide window · controller shortcut to reopen");box->addWidget(hide);
    VisibilityPolicy visibility;
    QObject::connect(hide,&QPushButton::clicked,[&]{visibility.manual(false);window.hide();});
    auto manualToggle=[&]{
        bool show=!window.isVisible();visibility.manual(show);
        if(show){view->show();window.show();}else window.hide();
#ifdef DAISYWHEEL_KDE
        if(show && kde){
            QDBusInterface keyboard("org.kde.KWin","/VirtualKeyboard","org.kde.kwin.VirtualKeyboard");
            keyboard.asyncCall("forceActivate");
        }
#endif
    };
    auto *quit=new QPushButton("Quit");box->addWidget(quit);
    QObject::connect(quit,&QPushButton::clicked,&app,&QApplication::quit);
    auto sendText=[&](const QString &text){
#ifdef DAISYWHEEL_KDE
        if(kde){if(!kde->commit(text))status->setText("No active text field — focus a text field in another app");return;}
#endif
        editor->insertPlainText(text);
    };
    auto sendControl=[&](bool backspace){
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
    QObject::connect(&timer,&QTimer::timeout,[&]{
        if(sdlOk){SDL_Event event;
            while(SDL_PollEvent(&event)){
                if(event.type==SDL_CONTROLLERDEVICEADDED&&!controller)controller=SDL_GameControllerOpen(event.cdevice.which);
                if(event.type==SDL_CONTROLLERDEVICEREMOVED&&controller&&event.cdevice.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))){SDL_GameControllerClose(controller);controller=nullptr;view->selected=-1;}
                if(event.type!=SDL_CONTROLLERBUTTONDOWN||!controller||event.cbutton.which!=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)))continue;
                if(event.cbutton.button==shortcut->currentData().toInt()) {
#ifdef DAISYWHEEL_KDE
                    if(kde) {
                        manualToggle();
                        continue;
                    }
#endif
                    if(window.isVisible()&&view->isVisible())window.hide();
                    else {view->show();window.show();}
                    continue;
                }
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
        status->setText(controller?QString("Connected: %1").arg(SDL_GameControllerName(controller)):sdlOk?"Connect a controller — this is a local typing prototype":"Controller initialization failed");view->update();
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
        pin->setEnabled(false);handle->pinned=true;
        handle->setText("Daisywheel · KDE overlay (bottom right)");
        window.setAttribute(Qt::WA_ShowWithoutActivating);
        box->addWidget(new QLabel("Automatic opening follows KDE → Virtual Keyboard\nHome / Back opens manually regardless of that setting"));
        kde->activation=[&](bool active){
            if(!active){visibility.contextEnded();if(!visibility.visible(false,false))window.hide();}
        };
        if(!kde->attach(&window)){
            std::cerr<<"KDE input method unavailable. Select Daisywheel in KDE Virtual Keyboard settings; KWin must launch it.\n";
            return 3;
        }
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
    if(!systemMode)window.show();
    if(app.arguments().contains("--smoke-test"))QTimer::singleShot(1000,&app,&QApplication::quit);
    int result=app.exec();
    if(controller)SDL_GameControllerClose(controller);if(sdlOk)SDL_Quit();return result;
}
