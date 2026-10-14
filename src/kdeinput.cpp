#include "kdeinput.h"
#include "qwayland-input-method-unstable-v1.h"
#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <QWidget>
#include <QGuiApplication>
#include <QWindow>
#include <QDateTime>
#include <QDebug>
#include <cstdio>
#include <LayerShellQt/Window>

namespace {
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
KdeInput::~KdeInput()=default;
bool KdeInput::attach(QWidget *widget){
    widget->winId();
    if(!QGuiApplication::platformName().startsWith("wayland"))return false;
    impl->initialize();
    if(!impl->isActive())return false;
    auto *layer=LayerShellQt::Window::get(widget->windowHandle());
    layer->setScope("daisywheel");
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layer->setActivateOnShow(false);
    layer->setExclusiveZone(0);
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom)|LayerShellQt::Window::AnchorRight);
    layer->setMargins(QMargins(0,0,24,24));
    layer->setWantsToBeOnActiveScreen(true);
    std::fprintf(stderr,"Daisywheel: non-focusing overlay configured\n");
    return true;
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
