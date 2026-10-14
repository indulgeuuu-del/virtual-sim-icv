#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
namespace SSD {
struct SimPoint3D { double x=0,y=0,z=0; };
struct SimString {
    std::string text;
    bool Empty() const { return text.empty(); }
};
}
int reads=0, failRead=0;
bool differentRoad=false,missingLane=false,badOutput=false;
SSD::SimString m_SampleGetNearMostLane(const SSD::SimPoint3D& p) {
    return {missingLane ? "" : (differentRoad && p.x>0 ? "2_0_1" : "1_0_1")};
}
bool isSameRoadId(const SSD::SimString& a,const SSD::SimString& b) { return !a.Empty() && a.text==b.text; }
struct Logger {
    enum class Color { BrightGreen };
    Logger& operator()(Color) { return *this; }
    template<class T> Logger& operator<<(const T&) { return *this; }
} globalLogger;
namespace SimOneAPI {
bool GetLaneST(const SSD::SimString&,const SSD::SimPoint3D& p,double& s,double& t) {
    ++reads; s=p.x; t=p.y;
    if(reads==failRead) { s=12345; t=-12345; return false; }
    if(badOutput) s=std::numeric_limits<double>::quiet_NaN();
    return true;
}
}
// @LONGITUDINAL_DISTANCE@
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    try {
        require(argc==2,"test name required");
        const std::string name=argv[1];
        SSD::SimPoint3D a{0,0,0},b{10,0,0};
        if(name=="current_failure") failRead=1;
        else if(name=="target_failure") failRead=2;
        else if(name=="missing_lane") missingLane=true;
        else if(name=="nonfinite") badOutput=true;
        else if(name=="different_road") differentRoad=true;
        else if(name=="backward") b.x=-10;
        else require(name=="forward","unknown test");
        const float distance=getDistS(a,b);
        if(failRead || missingLane || badOutput) require(std::isnan(distance),"invalid map observation treated as usable distance");
        else if(differentRoad) require(distance==std::numeric_limits<float>::max(),"cross-road sentinel changed");
        else require(distance==b.x,"signed longitudinal distance changed");
        std::cout<<"PASS "<<name<<'\n'; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
