#pragma once
#include <QString>
#include <QWidget>
#include <qt_windows.h>
#include <vector>

// Keep the real application foreground. Never redirect input to a remembered
// window: the user may have switched applications since opening the wheel.
class WindowsInput {
    HWND overlay=nullptr;
    QString error;
    bool send(std::vector<INPUT> &events) {
        HWND target=GetForegroundWindow();
        DWORD process=0;
        if(target)GetWindowThreadProcessId(target,&process);
        if(!target || process==GetCurrentProcessId()) {
            error="Focus a text field in another application";return false;
        }
        for(int key : {VK_CONTROL,VK_MENU,VK_LWIN,VK_RWIN,VK_SHIFT}) {
            if(GetAsyncKeyState(key)&0x8000) {
                error="Release keyboard modifiers before typing";return false;
            }
        }
        const UINT count=static_cast<UINT>(events.size());
        const UINT sent=SendInput(count,events.data(),sizeof(INPUT));
        if(sent!=count) {
            // Release any key left down by a partial insertion.
            if(sent%2 && sent<count)SendInput(1,&events[sent],sizeof(INPUT));
            error="Windows blocked input; elevated applications may require matching permissions";
            return false;
        }
        error.clear();return true;
    }
public:
    void attach(QWidget &window) {
        window.setAttribute(Qt::WA_ShowWithoutActivating);
        window.setWindowFlag(Qt::WindowDoesNotAcceptFocus);
        overlay=reinterpret_cast<HWND>(window.winId());
        SetWindowLongPtrW(overlay,GWL_EXSTYLE,
            GetWindowLongPtrW(overlay,GWL_EXSTYLE)|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW);
    }
    void setPointerPassthrough(bool enabled) {
        if(!overlay)return;
        LONG_PTR style=GetWindowLongPtrW(overlay,GWL_EXSTYLE);
        const LONG_PTR updated=enabled?(style|WS_EX_TRANSPARENT):(style&~WS_EX_TRANSPARENT);
        if(updated==style)return;
        SetWindowLongPtrW(overlay,GWL_EXSTYLE,updated);
        SetWindowPos(overlay,nullptr,0,0,0,0,
                     SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    }
    bool commit(const QString &text) {
        if(text.isEmpty())return true;
        std::vector<INPUT> events;
        events.reserve(static_cast<size_t>(text.size())*2);
        for(QChar character:text) {
            INPUT down{};down.type=INPUT_KEYBOARD;
            down.ki.wScan=character.unicode();down.ki.dwFlags=KEYEVENTF_UNICODE;
            INPUT up=down;up.ki.dwFlags|=KEYEVENTF_KEYUP;
            events.push_back(down);events.push_back(up);
        }
        return send(events);
    }
    bool key(bool backspace) {
        INPUT down{};down.type=INPUT_KEYBOARD;down.ki.wVk=backspace?VK_BACK:VK_RETURN;
        INPUT up=down;up.ki.dwFlags=KEYEVENTF_KEYUP;
        std::vector<INPUT> events{down,up};return send(events);
    }
    bool mouseMove(int dx,int dy) {
        if(dx==0&&dy==0)return true;
        INPUT event{};event.type=INPUT_MOUSE;event.mi.dx=dx;event.mi.dy=dy;
        event.mi.dwFlags=MOUSEEVENTF_MOVE;
        if(SendInput(1,&event,sizeof(INPUT))!=1){
            error="Windows blocked controller pointer movement";return false;
        }
        error.clear();return true;
    }
    bool mouseButton(bool left,bool pressed) {
        INPUT event{};event.type=INPUT_MOUSE;
        if(left)event.mi.dwFlags=pressed?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP;
        else event.mi.dwFlags=pressed?MOUSEEVENTF_RIGHTDOWN:MOUSEEVENTF_RIGHTUP;
        if(SendInput(1,&event,sizeof(INPUT))!=1){
            error="Windows blocked controller mouse click";return false;
        }
        error.clear();return true;
    }
    bool mouseScroll(int steps) {
        if(steps==0)return true;
        INPUT event{};event.type=INPUT_MOUSE;event.mi.mouseData=static_cast<DWORD>(steps*WHEEL_DELTA);
        event.mi.dwFlags=MOUSEEVENTF_WHEEL;
        if(SendInput(1,&event,sizeof(INPUT))!=1){
            error="Windows blocked controller scrolling";return false;
        }
        error.clear();return true;
    }
    const QString &lastError() const {return error;}
};

class WindowsOverlay : public QWidget {
public:
    bool preserveFocus=false;
protected:
    bool nativeEvent(const QByteArray &type,void *message,qintptr *result) override {
        if(preserveFocus && static_cast<MSG*>(message)->message==WM_MOUSEACTIVATE) {
            *result=MA_NOACTIVATE;return true;
        }
        return QWidget::nativeEvent(type,message,result);
    }
};
