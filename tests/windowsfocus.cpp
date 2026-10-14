#include "windowsfocus.h"
#include "windowsinput.h"
#include "visibility.h"
#include "windowsdesktop.h"
#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QProcess>
#include <QTimer>
#include <iostream>

constexpr UINT focusCommand=WM_APP+41;
class FocusTarget : public QWidget {
    QLineEdit first,second,readOnly,password;
    QPlainTextEdit document,readOnlyDocument;
    QPushButton button{"Test button"};
    HWND nativeEdit=nullptr;
public:
    FocusTarget(){
        setWindowTitle("Daisywheel focus detection fixture");resize(460,540);
        auto *layout=new QVBoxLayout(this);
        for(auto *field:{&first,&second,&readOnly,&password})layout->addWidget(field);
        readOnly.setReadOnly(true);password.setEchoMode(QLineEdit::Password);
        readOnlyDocument.setReadOnly(true);
        layout->addWidget(&document);layout->addWidget(&readOnlyDocument);layout->addWidget(&button);
        layout->addSpacing(40);
        show();
        nativeEdit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE,10,495,420,25,
            reinterpret_cast<HWND>(winId()),nullptr,GetModuleHandleW(nullptr),nullptr);
        activateWindow();SetForegroundWindow(reinterpret_cast<HWND>(winId()));
    }
protected:
    bool nativeEvent(const QByteArray &type,void *message,qintptr *result) override {
        auto *event=static_cast<MSG*>(message);
        if(event->message==focusCommand){
            activateWindow();SetForegroundWindow(reinterpret_cast<HWND>(winId()));
            QWidget *fields[]={nullptr,&first,&second,&readOnly,&button,&document,&readOnlyDocument,&password};
            const size_t index=static_cast<size_t>(event->wParam);
            if(index>=std::size(fields)){*result=0;return true;}
            if(index==0)SetFocus(nativeEdit);else fields[index]->setFocus(Qt::OtherFocusReason);
            *result=1;return true;
        }
        return QWidget::nativeEvent(type,message,result);
    }
};

struct WindowSearch {DWORD process;HWND window=nullptr;};
BOOL CALLBACK findWindow(HWND window,LPARAM parameter){
    auto &search=*reinterpret_cast<WindowSearch*>(parameter);
    DWORD process=0;GetWindowThreadProcessId(window,&process);
    if(process==search.process && IsWindowVisible(window) && !GetWindow(window,GW_OWNER)){
        search.window=window;return FALSE;
    }
    return TRUE;
}

int main(int argc,char **argv){
    QApplication app(argc,argv);
    if(app.arguments().contains("--target")){
        FocusTarget target;QTimer::singleShot(30000,&app,&QApplication::quit);return app.exec();
    }
    WindowsFocus watcher;watcher.setEnabled(true);
    QProcess target;target.setProcessChannelMode(QProcess::ForwardedChannels);
    target.start(QCoreApplication::applicationFilePath(),{"--target"});
    const char *names[]={"native EDIT","Qt text box","second field in same window","read-only Qt text box",
        "button","Qt document editor","read-only Qt document","password field"};
    const bool expected[]={true,true,true,false,false,true,false,true};
    int stage=-1,ticks=0;
    HWND targetWindow=nullptr;
    ULONGLONG requestedAt=0;
    QByteArray firstKey;
    VisibilityPolicy policy;
    QTimer timer;
    QObject::connect(&timer,&QTimer::timeout,[&]{
        if(!targetWindow){
            WindowSearch search{static_cast<DWORD>(target.processId())};
            EnumWindows(findWindow,reinterpret_cast<LPARAM>(&search));targetWindow=search.window;
            if(!targetWindow){if(++ticks>50)app.exit(1);return;}
            if(!focusFixture(targetWindow,search.process)){app.exit(10);return;}
            ticks=0;
        }
        if(stage<0){stage=0;requestedAt=GetTickCount64();}
        auto state=watcher.snapshot();
        if(state.known && state.foreground==targetWindow && state.sampledAt>requestedAt+400 && state.editable==expected[stage]){
            if(state.editable && (!state.automationAvailable || state.context.isEmpty())){app.exit(2);return;}
            if(stage==1){
                firstKey=state.context;
                if(!policy.visible(true,true)){app.exit(3);return;}
                policy.manual(false);
                if(policy.visible(true,true)){app.exit(4);return;}
            }
            if(stage==2){
                if(firstKey==state.context){std::cerr<<"Same-window fields did not receive distinct identities\n";app.exit(5);return;}
                policy.contextEnded();
                if(!policy.visible(true,true)){app.exit(6);return;}
            }
            std::cout<<"PASS: "<<names[stage]<<" editable="<<state.editable<<'\n';
            ++stage;ticks=0;
            if(stage==8){app.exit(0);return;}
            requestedAt=GetTickCount64();
        }else if(++ticks>45){
            std::cerr<<"FAIL: "<<names[stage]<<" known="<<state.known<<" editable="<<state.editable
                <<" automation="<<state.automationAvailable<<"\n";app.exit(7);return;
        }
        // Only message this test's own target process.
        DWORD process=0;GetWindowThreadProcessId(targetWindow,&process);
        if(process!=static_cast<DWORD>(target.processId())){app.exit(8);return;}
        DWORD_PTR answer=0;
        if(!SendMessageTimeoutW(targetWindow,focusCommand,stage,ticks==1?1:0,SMTO_ABORTIFHUNG,500,&answer)||answer!=1){app.exit(9);}
    });
    timer.start(100);
    int result=app.exec();
    target.kill();target.waitForFinished(1000);return result;
}
