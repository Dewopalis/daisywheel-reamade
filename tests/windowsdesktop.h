#pragma once
#include <qt_windows.h>

// Desktop tests need real foreground focus. Windows may refuse a background
// process's SetForegroundWindow call. Click only a verified fixture window.
inline bool focusFixture(HWND window,DWORD process){
    DWORD actualProcess=0;GetWindowThreadProcessId(window,&actualProcess);
    if(actualProcess!=process)return false;
    ShowWindow(window,SW_SHOWNOACTIVATE);
    SetWindowPos(window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    POINT point{20,20};if(!ClientToScreen(window,&point))return false;
    HWND hit=WindowFromPoint(point);DWORD hitProcess=0;
    if(hit)GetWindowThreadProcessId(hit,&hitProcess);
    if(hitProcess!=process || GetAncestor(hit,GA_ROOT)!=window)return false;
    const int left=GetSystemMetrics(SM_XVIRTUALSCREEN),top=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(width<2||height<2)return false;
    POINT previous{};GetCursorPos(&previous);
    INPUT events[3]{};
    for(auto &event:events)event.type=INPUT_MOUSE;
    events[0].mi.dx=static_cast<LONG>((point.x-left)*65535.0/(width-1));
    events[0].mi.dy=static_cast<LONG>((point.y-top)*65535.0/(height-1));
    events[0].mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
    events[1].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;events[2].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    const UINT sent=SendInput(3,events,sizeof(INPUT));
    if(sent==2)SendInput(1,&events[2],sizeof(INPUT));
    // Restoring the pointer does not change keyboard focus.
    SetCursorPos(previous.x,previous.y);
    return sent==3;
}
