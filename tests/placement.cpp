#include "placement.h"
#include <iostream>
int main(){
    const QRect left(-1920,0,1920,1080),right(0,0,1920,1080);
    const QList<QRect> screens{left,right};
    auto check=[](bool value,const char *name){if(!value)std::cerr<<"FAIL: "<<name<<'\n';return value;};
    if(!check(overlayPosition({400,600},right,screens,QPoint(-1500,100))==QPoint(-1500,100),"pin stays on negative-coordinate monitor"))return 1;
    if(!check(overlayPosition({400,600},right,screens,QPoint(1800,900))==QPoint(1520,480),"pin clamped after size/work-area change"))return 2;
    if(!check(overlayPosition({400,600},right,{right},QPoint(-1500,100))==QPoint(1508,468),"removed monitor falls back"))return 3;
    if(!check(overlayPosition({400,600},right,screens,std::nullopt)==QPoint(1508,468),"unpinned foreground placement"))return 4;
    if(!check(overlayPosition({2400,1200},right,screens,QPoint(100,100))==QPoint(0,0),"oversized overlay retains accessible top left"))return 5;
    std::cout<<"PASS: pin preservation, bounds and monitor fallback\n";
}
