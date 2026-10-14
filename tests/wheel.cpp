#include "wheel.h"
#include "visibility.h"
#include <iostream>
int main() {
    const double points[][2] = {{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
    for (int i=0;i<8;++i) if(wheel::sector(points[i][0],points[i][1])!=i) return 1;
    if(wheel::sector(0,0)!=-1 || wheel::sector(.2,.2)!=-1) return 2;
    for(auto &s:wheel::letters) if(s.size()!=4) return 3;
    for(auto &s:wheel::symbols) if(s.size()!=4) return 4;
    VisibilityPolicy policy;
    if(policy.visible(false,true)||policy.visible(true,false))return 5;
    if(!policy.visible(true,true))return 6;
    policy.manual(false);
    if(policy.visible(true,true))return 7;
    policy.contextEnded();
    if(!policy.visible(true,true))return 8;
    policy.manual(true);
    if(!policy.visible(false,false)||!policy.visible(true,false))return 9;
    std::cout << "Eight directions, neutral deadzone and character slots passed\n";
}
