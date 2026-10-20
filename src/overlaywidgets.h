#pragma once
#include "wheel.h"
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QWindow>
#include <QScreen>
#include <functional>
#include <algorithm>
#include <SDL.h>

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
    void setShortcutHint(const QString &hint) {
        if(shortcutHint==hint)return;
        shortcutHint=hint;
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
        if(!shortcutHint.isEmpty()){
            QFont hintFont=p.font();hintFont.setPixelSize(qMax(8,qRound(10*scale)));p.setFont(hintFont);
            p.setPen(QColor("#9aa8bd"));
            p.drawText(QRectF(center-QPointF(88,-28),QSizeF(176,20)),Qt::AlignCenter,shortcutHint);
        }
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
    QString shortcutHint;
};

