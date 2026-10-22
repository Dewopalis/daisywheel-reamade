#include "pointerbuttonstate.h"

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition,const char *message) {
    if(!condition){std::cerr<<message<<'\n';std::exit(1);}
}
}

int main() {
    PointerButtonState buttons;
    require(buttons.press(true),"first left press must be emitted");
    require(!buttons.press(true),"duplicate left press must be ignored");
    require(buttons.release(true),"left release must be emitted");
    require(!buttons.release(true),"duplicate left release must be ignored");

    buttons.press(true);
    buttons.press(false);
    const auto releases=buttons.releaseAll();
    require(releases.size()==2,"interruption must release both held buttons");
    require(releases[0],"left button must be released first");
    require(!releases[1],"right button must also be released");
    require(buttons.releaseAll().empty(),"cleanup must be idempotent");
    require(!buttons.isHeld(true)&&!buttons.isHeld(false),"cleanup must clear held state");
}
