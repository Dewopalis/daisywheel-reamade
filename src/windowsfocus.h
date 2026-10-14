#pragma once
#include <QByteArray>
#include <qt_windows.h>
#include <condition_variable>
#include <mutex>
#include <thread>

struct WindowsFocusState {
    HWND foreground=nullptr;
    QByteArray context;
    bool known=false;
    bool editable=false;
    bool automationAvailable=false;
    ULONGLONG sampledAt=0;
};

// UI Automation queries may call another application's accessibility provider.
// Keep COM objects and those calls off the Qt/controller thread.
class WindowsFocus {
    mutable std::mutex mutex;
    std::condition_variable wake;
    bool stopping=false,enabled=false;
    WindowsFocusState state;
    std::thread worker;
    void run();
public:
    WindowsFocus();
    ~WindowsFocus();
    WindowsFocus(const WindowsFocus &)=delete;
    WindowsFocus &operator=(const WindowsFocus &)=delete;
    void setEnabled(bool value);
    WindowsFocusState snapshot() const;
};
