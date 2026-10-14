#include "windowsfocus.h"
#include <initguid.h>
#include <uiautomationclient.h>
#include <memory>
#include <chrono>

namespace {
template<class T> struct ComRelease {void operator()(T *value) const {if(value)value->Release();}};
template<class T> using ComPtr=std::unique_ptr<T,ComRelease<T>>;
struct Property {
    VARIANT value;
    Property(){VariantInit(&value);}
    ~Property(){VariantClear(&value);}
    bool get(IUIAutomationElement *element,PROPERTYID id){
        return SUCCEEDED(element->GetCachedPropertyValue(id,&value));
    }
    bool isBool(bool expected) const {return value.vt==VT_BOOL && (value.boolVal!=VARIANT_FALSE)==expected;}
};

QByteArray runtimeKey(IUIAutomationElement *element,HWND foreground) {
    SAFEARRAY *runtime=nullptr;
    if(FAILED(element->GetRuntimeId(&runtime)) || !runtime)return {};
    QByteArray key=QByteArray::number(reinterpret_cast<quintptr>(foreground));
    LONG first=0,last=-1;
    if(SafeArrayGetDim(runtime)==1 && SUCCEEDED(SafeArrayGetLBound(runtime,1,&first)) &&
       SUCCEEDED(SafeArrayGetUBound(runtime,1,&last))){
        for(LONG i=first;i<=last;++i){
            int part=0;
            if(FAILED(SafeArrayGetElement(runtime,&i,&part))){key.clear();break;}
            key+=':';key+=QByteArray::number(part);
        }
    }else key.clear();
    SafeArrayDestroy(runtime);return key;
}

WindowsFocusState inspect(IUIAutomation *automation,IUIAutomationCacheRequest *cache) {
    WindowsFocusState result;result.foreground=GetForegroundWindow();
    result.automationAvailable=automation!=nullptr;
    DWORD process=0;
    const DWORD thread=result.foreground?GetWindowThreadProcessId(result.foreground,&process):0;
    if(!thread || process==GetCurrentProcessId()){result.known=true;return result;}
    GUITHREADINFO native{};native.cbSize=sizeof(native);
    if(GetGUIThreadInfo(thread,&native) && native.hwndFocus){
        wchar_t className[64]{};GetClassNameW(native.hwndFocus,className,64);
        // A native child inside a custom-framework window can be absent from
        // that framework's UIA focus tree. Handle standard EDIT directly.
        if(_wcsicmp(className,L"Edit")==0){
            result.known=true;
            result.editable=IsWindowEnabled(native.hwndFocus) && !(GetWindowLongPtrW(native.hwndFocus,GWL_STYLE)&ES_READONLY);
            if(result.editable)result.context="native:"+QByteArray::number(reinterpret_cast<quintptr>(native.hwndFocus));
            return result;
        }
    }
    if(automation && cache){
        IUIAutomationElement *raw=nullptr;
        if(SUCCEEDED(automation->GetFocusedElementBuildCache(cache,&raw)) && raw){
            ComPtr<IUIAutomationElement> element(raw);
            Property pid,focus,enabled,type,valueAvailable,readOnly;
            if(pid.get(raw,UIA_ProcessIdPropertyId) && pid.value.vt==VT_I4 &&
               pid.value.lVal==static_cast<LONG>(process) &&
               focus.get(raw,UIA_HasKeyboardFocusPropertyId) && focus.isBool(true) &&
               enabled.get(raw,UIA_IsEnabledPropertyId) && type.get(raw,UIA_ControlTypePropertyId)){
                result.known=true;
                if(enabled.isBool(true) && type.value.vt==VT_I4 &&
                   (type.value.lVal==UIA_EditControlTypeId || type.value.lVal==UIA_DocumentControlTypeId ||
                    type.value.lVal==UIA_ComboBoxControlTypeId || type.value.lVal==UIA_CustomControlTypeId)){
                    valueAvailable.get(raw,UIA_IsValuePatternAvailablePropertyId);
                    readOnly.get(raw,UIA_ValueIsReadOnlyPropertyId);
                    if(valueAvailable.isBool(true))result.editable=readOnly.isBool(false);
                    else if(type.value.lVal==UIA_EditControlTypeId || type.value.lVal==UIA_DocumentControlTypeId){
                        IUIAutomationTextPattern *textRaw=nullptr;
                        if(SUCCEEDED(raw->GetCurrentPatternAs(UIA_TextPatternId,IID_IUIAutomationTextPattern,reinterpret_cast<void**>(&textRaw))) && textRaw){
                            ComPtr<IUIAutomationTextPattern> text(textRaw);
                            IUIAutomationTextRange *rangeRaw=nullptr;
                            if(SUCCEEDED(text->get_DocumentRange(&rangeRaw)) && rangeRaw){
                                ComPtr<IUIAutomationTextRange> range(rangeRaw);
                                Property attribute;
                                // UIA_IsReadOnlyAttributeId, absent from older MinGW headers.
                                if(SUCCEEDED(range->GetAttributeValue(40015,&attribute.value)))result.editable=attribute.isBool(false);
                            }
                        }
                    }
                }
                if(result.editable){result.context=runtimeKey(raw,result.foreground);result.editable=!result.context.isEmpty();}
                return result;
            }
        }
    }
    return result;
}
}

WindowsFocus::WindowsFocus():worker([this]{run();}){}
WindowsFocus::~WindowsFocus(){
    {std::lock_guard<std::mutex> lock(mutex);stopping=true;}
    wake.notify_one();worker.join();
}
void WindowsFocus::setEnabled(bool value){
    {std::lock_guard<std::mutex> lock(mutex);enabled=value;if(!enabled)state={};}
    wake.notify_one();
}
WindowsFocusState WindowsFocus::snapshot() const {std::lock_guard<std::mutex> lock(mutex);return state;}
void WindowsFocus::run(){
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(initialized))return;
    {
        IUIAutomation *raw=nullptr;
        CoCreateInstance(CLSID_CUIAutomation8,nullptr,CLSCTX_INPROC_SERVER,IID_IUIAutomation,reinterpret_cast<void**>(&raw));
        ComPtr<IUIAutomation> automation(raw);
        if(raw){
            IUIAutomation2 *optionsRaw=nullptr;
            if(SUCCEEDED(raw->QueryInterface(IID_IUIAutomation2,reinterpret_cast<void**>(&optionsRaw)))){
                ComPtr<IUIAutomation2> options(optionsRaw);
                options->put_ConnectionTimeout(300);options->put_TransactionTimeout(300);
            }
        }
        IUIAutomationCacheRequest *cacheRaw=nullptr;
        if(raw)raw->CreateCacheRequest(&cacheRaw);
        ComPtr<IUIAutomationCacheRequest> cache(cacheRaw);
        if(cache){
            for(PROPERTYID id:{UIA_ProcessIdPropertyId,UIA_HasKeyboardFocusPropertyId,UIA_IsEnabledPropertyId,
                UIA_ControlTypePropertyId,UIA_IsValuePatternAvailablePropertyId,UIA_ValueIsReadOnlyPropertyId})cache->AddProperty(id);
        }
        for(;;){
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock,[&]{return stopping||enabled;});
                if(stopping)break;
            }
            auto next=inspect(raw,cacheRaw);
            next.sampledAt=GetTickCount64();
            {
                std::unique_lock<std::mutex> lock(mutex);
                if(stopping)break;
                if(enabled)state=next;
                wake.wait_for(lock,std::chrono::milliseconds(200),[&]{return stopping||!enabled;});
            }
        }
    }
    CoUninitialize();
}
