#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
namespace SSD {
struct SimPoint3D { double x=0,y=0,z=0; SimPoint3D()=default; SimPoint3D(double a,double b,double c):x(a),y(b),z(c){} };
struct SimString {
    std::string text;
    bool Empty() const { return text.empty(); }
};
}
struct SimOne_Data_Vec3f { float x=0,y=0,z=0; };
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
    if(badOutput) s=t=std::numeric_limits<double>::quiet_NaN();
    return true;
}
}
// @LONGITUDINAL_DISTANCE@
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    try {
        require(argc==2,"test name required");
        std::string name=argv[1];
        const bool lateral=name.rfind("lateral_",0)==0;
        if(lateral) name=name.substr(8);
        SSD::SimPoint3D a{0,0,0},b{10,2,0};
        if(name=="current_failure") failRead=1;
        else if(name=="target_failure") failRead=2;
        else if(name=="missing_lane") missingLane=true;
        else if(name=="nonfinite") badOutput=true;
        else if(name=="different_road") differentRoad=true;
        else if(name=="backward") { b.x=-10; b.y=-2; }
        else require(name=="forward","unknown test");
        auto check=[&](float distance) {
            if(failRead || missingLane || badOutput) require(std::isnan(distance),"invalid map observation treated as usable distance");
            else if(differentRoad) require(distance==std::numeric_limits<float>::max(),"cross-road sentinel changed");
            else require(distance==(lateral ? b.y : b.x),"signed distance changed");
        };
        check(lateral ? getDistT(a,b) : getDistS(a,b));
        if(lateral) { reads=0; check(getDistT(a,SimOne_Data_Vec3f{float(b.x),float(b.y),float(b.z)})); }
        std::cout<<"PASS "<<name<<'\n'; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
