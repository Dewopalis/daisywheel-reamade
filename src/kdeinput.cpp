#include "kdeinput.h"
#include "qwayland-input-method-unstable-v1.h"
#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <QWidget>
#include <QGuiApplication>
#include <QWindow>
#include <QScreen>
#include <QDateTime>
#include <QDebug>
#include <QPainter>
#include <QWidget>
#include <cmath>
#include <cstdio>
#include <LayerShellQt/Window>

namespace {
class GeometryPreviewWidget : public QWidget {
public:
    GeometryPreviewWidget():QWidget(nullptr,Qt::Tool|Qt::FramelessWindowHint|
                                    Qt::WindowStaysOnTopHint|Qt::WindowTransparentForInput){
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const QRectF bounds=rect().adjusted(3,3,-4,-4);
        p.setPen(QPen(QColor(102,217,239,220),2,Qt::DashLine));
        p.setBrush(QColor(17,24,39,100));p.drawRoundedRect(bounds,12,12);
        const qreal radius=qMin(width(),height())*.35;
        const qreal petal=qMin(53.0,radius*.34);
        p.setPen(QPen(QColor(102,217,239,175),1.5));p.setBrush(QColor(102,217,239,45));
        const QPointF center(width()/2.0,height()/2.0);
        for(int i=0;i<8;++i){
            const qreal angle=i*3.14159265358979323846/4;
            const QPointF point=center+QPointF(std::sin(angle)*radius,-std::cos(angle)*radius);
            p.drawEllipse(point,petal,petal);
        }
        p.setPen(QPen(QColor(210,240,250,220),2));p.drawEllipse(center,4,4);
    }
};
class Context : public QtWayland::zwp_input_method_context_v1 {
public:
    uint32_t serial=0;
    bool ready=false;
    bool probe=false;
    explicit Context(::zwp_input_method_context_v1 *context) : QtWayland::zwp_input_method_context_v1(context) {}
    ~Context() override { destroy(); }
    void zwp_input_method_context_v1_commit_state(uint32_t value) override {serial=value;ready=true;qInfo()<<"Daisywheel: input state ready";}
    void zwp_input_method_context_v1_reset() override {ready=false;}
    void zwp_input_method_context_v1_surrounding_text(const QString &text,uint32_t,uint32_t) override {
        probe=(text=="DAISYWHEEL_ISOLATED_PROBE");
    }
};

}
class KdeInput::Impl : public QWaylandClientExtensionTemplate<Impl>, public QtWayland::zwp_input_method_v1 {
public:
    using QWaylandClientExtension::initialize;
    KdeInput *owner;
    std::unique_ptr<Context> context;
    QWidget *widget=nullptr;
    LayerShellQt::Window *layer=nullptr;
    GeometryPreviewWidget *previewWidget=nullptr;
    LayerShellQt::Window *previewLayer=nullptr;
    QRect geometry;
    explicit Impl(KdeInput *owner):QWaylandClientExtensionTemplate(1),owner(owner){}
    void zwp_input_method_v1_activate(::zwp_input_method_context_v1 *id) override {
        qInfo()<<"Daisywheel: input context activated";
        context=std::make_unique<Context>(id);
        if(owner->activation)owner->activation(true);
    }
    void zwp_input_method_v1_deactivate(::zwp_input_method_context_v1 *id) override {
        qInfo()<<"Daisywheel: input context deactivated";
        if(context && context->object()==id){context.reset();if(owner->activation)owner->activation(false);}
    }
};
KdeInput::KdeInput():impl(std::make_unique<Impl>(this)){}
KdeInput::~KdeInput(){delete impl->previewWidget;}
bool KdeInput::attach(QWidget *widget){
    widget->winId();
    if(!QGuiApplication::platformName().startsWith("wayland"))return false;
    impl->initialize();
    if(!impl->isActive())return false;
    auto *layer=LayerShellQt::Window::get(widget->windowHandle());
    impl->widget=widget;
    impl->layer=layer;
    impl->previewWidget=new GeometryPreviewWidget;
    impl->previewWidget->winId();
    impl->previewLayer=LayerShellQt::Window::get(impl->previewWidget->windowHandle());
    impl->previewLayer->setScope("daisywheel-position-preview");
    impl->previewLayer->setLayer(LayerShellQt::Window::LayerOverlay);
    impl->previewLayer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    impl->previewLayer->setExclusiveZone(0);
    impl->previewLayer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom)|LayerShellQt::Window::AnchorRight);
    impl->previewLayer->setWantsToBeOnActiveScreen(true);
    layer->setScope("daisywheel");
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layer->setExclusiveZone(0);
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom)|LayerShellQt::Window::AnchorRight);
    widget->adjustSize();
    QScreen *screen=widget->screen();if(!screen)screen=QGuiApplication::primaryScreen();
    const QRect area=screen?screen->availableGeometry():QRect(0,0,1280,720);
    impl->geometry=QRect(area.right()+1-24-widget->width(),area.bottom()+1-24-widget->height(),widget->width(),widget->height());
    layer->setDesiredSize(widget->size());
    layer->setMargins(QMargins(0,0,24,24));
#ifdef DAISYWHEEL_LEGACY_LAYER_SCREEN
    layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromCompositor);
#else
    layer->setWantsToBeOnActiveScreen(true);
#endif
    std::fprintf(stderr,"Daisywheel: non-focusing overlay configured\n");
    return true;
}
QRect KdeInput::overlayGeometry()const{return impl->geometry;}
void KdeInput::setOverlayGeometry(const QRect &requested){
    if(!impl->widget||!impl->layer)return;
    QScreen *screen=impl->widget->screen();if(!screen)screen=QGuiApplication::primaryScreen();
    if(!screen)return;
    const QRect area=screen->availableGeometry();
    QSize size=requested.size().expandedTo(impl->widget->minimumSizeHint().expandedTo(impl->widget->minimumSize()));
    size.setWidth(qMin(size.width(),area.width()));size.setHeight(qMin(size.height(),area.height()));
    impl->widget->resize(size);size=impl->widget->size();
    const int x=qBound(area.left(),requested.x(),area.right()+1-size.width());
    const int y=qBound(area.top(),requested.y(),area.bottom()+1-size.height());
    impl->geometry=QRect(x,y,size.width(),size.height());
    impl->layer->setDesiredSize(size);
    impl->layer->setMargins(QMargins(0,0,area.right()+1-impl->geometry.right()-1,
                                           area.bottom()+1-impl->geometry.bottom()-1));
}
void KdeInput::previewOverlayGeometry(const QRect &requested){
    if(!impl->widget||!impl->previewWidget||!impl->previewLayer)return;
    QScreen *screen=impl->widget->screen();if(!screen)screen=QGuiApplication::primaryScreen();
    if(!screen)return;
    const QRect area=screen->availableGeometry();
    QSize size=requested.size();
    size.setWidth(qBound(1,size.width(),area.width()));
    size.setHeight(qBound(1,size.height(),area.height()));
    const int x=qBound(area.left(),requested.x(),area.right()+1-size.width());
    const int y=qBound(area.top(),requested.y(),area.bottom()+1-size.height());
    impl->previewWidget->resize(size);
    impl->previewLayer->setDesiredSize(size);
    impl->previewLayer->setMargins(QMargins(0,0,area.right()+1-x-size.width(),
                                                  area.bottom()+1-y-size.height()));
    if(!impl->previewWidget->isVisible())impl->previewWidget->show();
    impl->previewWidget->raise();
}
void KdeInput::clearOverlayGeometryPreview(){
    if(impl->previewWidget)impl->previewWidget->hide();
}
bool KdeInput::active()const{return impl->context && impl->context->ready;}
bool KdeInput::verificationTarget()const{return active() && impl->context->probe;}
bool KdeInput::commit(const QString &text){
    if(!active())return false;
    impl->context->commit_string(impl->context->serial,text);return true;
}
bool KdeInput::key(unsigned symbol){
    if(!active())return false;
    auto &c=*impl->context;
    uint32_t time=uint32_t(QDateTime::currentMSecsSinceEpoch());
    c.keysym(c.serial,time,symbol,1,0);c.keysym(c.serial,time,symbol,0,0);return true;
}
