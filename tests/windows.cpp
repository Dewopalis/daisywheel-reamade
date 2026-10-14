#include "windowsinput.h"
#include "windowsdesktop.h"
#include <QApplication>
#include <QPlainTextEdit>
#include <QProcess>
#include <QTimer>
#include <iostream>

int main(int argc,char **argv) {
    QApplication app(argc,argv);
    const QString expected=QString::fromUtf8("A\xc3\xa9\xf0\x9f\x98\x80\r\nZ");
    if(app.arguments().contains("--target")) {
        QWidget targetWindow;targetWindow.resize(400,200);
        targetWindow.setWindowTitle("Daisywheel Windows integration test");targetWindow.show();
        HWND owner=reinterpret_cast<HWND>(targetWindow.winId());
        HWND editor=CreateWindowExW(0,L"EDIT",L"replace me",WS_CHILD|WS_VISIBLE|ES_MULTILINE,
            0,0,400,200,owner,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!editor)return 11;
        SetForegroundWindow(owner);SetFocus(editor);SendMessageW(editor,EM_SETSEL,0,-1);
        QTimer check;
        QObject::connect(&check,&QTimer::timeout,[&]{
            wchar_t text[256]{};GetWindowTextW(editor,text,256);
            if(QString::fromWCharArray(text)==expected)app.exit(0);
        });
        check.start(50);
        QTimer::singleShot(10000,&app,[&]{
            wchar_t text[256]{};GetWindowTextW(editor,text,256);
            std::cerr<<"Synthetic target received UTF-8 hex: "<<QString::fromWCharArray(text).toUtf8().toHex().toStdString()<<'\n';app.exit(3);
        });
        return app.exec();
    }
    QProcess target;
    target.setProcessChannelMode(QProcess::ForwardedChannels);
    WindowsOverlay overlay;overlay.preserveFocus=true;
    WindowsInput input;input.attach(overlay);
    // Sending to our own foreground window must be refused.
    QWidget ownWindow;ownWindow.show();ownWindow.activateWindow();
    SetForegroundWindow(reinterpret_cast<HWND>(ownWindow.winId()));
    if(!focusFixture(reinterpret_cast<HWND>(ownWindow.winId()),GetCurrentProcessId()))return 12;
    QTimer attempt;
    bool sent=false;
    int ticks=0;
    QObject::connect(&attempt,&QTimer::timeout,[&]{
        DWORD process=0;
        GetWindowThreadProcessId(GetForegroundWindow(),&process);
        if(process==static_cast<DWORD>(target.processId()) && !sent) {
            HWND foreground=GetForegroundWindow();
            overlay.hide();overlay.preserveFocus=true;overlay.show();
            if(GetForegroundWindow()!=foreground){app.exit(4);return;}
            auto result=SendMessageW(reinterpret_cast<HWND>(overlay.winId()),WM_MOUSEACTIVATE,0,0);
            if(result!=MA_NOACTIVATE || GetForegroundWindow()!=foreground){app.exit(5);return;}
            sent=true;
            if(!input.commit(QString::fromUtf8("A\xc3\xa9\xf0\x9f\x98\x80x")) ||
               !input.key(true) || !input.key(false) || !input.commit("Z")) {
                std::cerr<<input.lastError().toStdString()<<'\n';app.exit(6);
            }
        }
        if(++ticks>100){std::cerr<<"Timed out waiting for test focus/input\n";app.exit(7);}
    });
    QObject::connect(&target,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),
        [&](int code,QProcess::ExitStatus status){
            if(sent && code==0 && status==QProcess::NormalExit){
                std::cout<<"PASS: Unicode, surrogate pairs, Backspace, Enter, foreground preservation\n";
                app.exit(0);
            }else {std::cerr<<"Target exit "<<code<<", sent="<<sent<<'\n';app.exit(8);}
        });
    QTimer::singleShot(200,&app,[&]{
        DWORD foregroundProcess=0;
        GetWindowThreadProcessId(GetForegroundWindow(),&foregroundProcess);
        if(foregroundProcess!=GetCurrentProcessId()){
            std::cerr<<"Could not acquire focus for the self-input rejection test\n";app.exit(10);return;
        }
        if(input.commit("must not type into ourselves")){app.exit(9);return;}
        // Let the child inherit foreground rights when it launches.
        SetWindowPos(reinterpret_cast<HWND>(ownWindow.winId()),HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        target.start(QCoreApplication::applicationFilePath(),{"--target"});
        attempt.start(100);
    });
    int result=app.exec();
    if(target.state()!=QProcess::NotRunning){target.kill();target.waitForFinished(1000);}
    return result;
}
