#pragma once

#include <vector>

// Tracks injected pointer buttons independently from controller button state.
// Call releaseAll() whenever the controller or pointer session is interrupted.
class PointerButtonState {
public:
    bool press(bool left) {
        bool &held=left?leftHeld:rightHeld;
        if(held)return false;
        held=true;
        return true;
    }

    bool release(bool left) {
        bool &held=left?leftHeld:rightHeld;
        if(!held)return false;
        held=false;
        return true;
    }

    bool isHeld(bool left) const { return left?leftHeld:rightHeld; }

    std::vector<bool> releaseAll() {
        std::vector<bool> released;
        if(leftHeld)released.push_back(true);
        if(rightHeld)released.push_back(false);
        leftHeld=false;
        rightHeld=false;
        return released;
    }

private:
    bool leftHeld=false;
    bool rightHeld=false;
};
