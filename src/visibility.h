#pragma once
// Manual intent overrides automatic opening until explicitly changed.
class VisibilityPolicy {
    enum class Intent { Automatic, Open, Dismissed };
    Intent intent=Intent::Automatic;
public:
    void manual(bool show) {intent=show?Intent::Open:Intent::Dismissed;}
    void contextEnded() {if(intent==Intent::Dismissed)intent=Intent::Automatic;}
    bool visible(bool context, bool desktopAllows) const {
        if(intent==Intent::Open)return true;
        if(intent==Intent::Dismissed)return false;
        return context && desktopAllows;
    }
};
