#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>
namespace SSD {
struct SimPoint3D { double x=0,y=0,z=0; SimPoint3D(double a=0,double b=0,double c=0):x(a),y(b),z(c) {} };
struct SimString { std::string s; SimString(const char* p="lane"):s(p) {} bool Empty() const { return s.empty(); } };
// Bounds-checked SDK fixture makes old-code out-of-bounds a test failure, not a crash.
struct SimPoint3DVector:std::vector<SimPoint3D> {
    using std::vector<SimPoint3D>::vector;
    const SimPoint3D& operator[](size_t i) const { return at(i); }
};
}
namespace HDMapStandalone { struct MLaneInfo { SSD::SimPoint3DVector leftBoundary,rightBoundary; }; }
std::string scenario;
SSD::SimString m_SampleGetNearMostLane(const SSD::SimPoint3D&) { return scenario=="empty_lane" ? "" : "lane"; }
namespace SimOneAPI {
bool GetLaneSample(const SSD::SimString&,HDMapStandalone::MLaneInfo& info) {
    info.leftBoundary={{10,2,0},{0,2,0}};
    info.rightBoundary={{0,-2,0},{20,-2,0},{30,-2,0}};
    if(scenario=="left_empty") info.leftBoundary.clear();
    if(scenario=="right_empty") info.rightBoundary.clear();
    if(scenario=="nan") info.leftBoundary.at(0).x=std::nan("");
    if(scenario=="inf") info.rightBoundary.at(0).z=std::numeric_limits<double>::infinity();
    if(scenario=="all_nan") for(auto& p:info.rightBoundary) p.x=std::nan("");
    return scenario!="failed_partial";
}
}
namespace UtilMath { double planarDistance(const SSD::SimPoint3D& a,const SSD::SimPoint3D& b) { return std::hypot(a.x-b.x,a.y-b.y); } }
// @BOUNDARY@
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("missing scenario");
        scenario=argv[1];
        SSD::SimPoint3D left(77,77,77),right(88,88,88),point;
        if(scenario=="point_nan") point.x=std::nan("");
        bool rejected=false;
        try { getObstacleLaneSidePosition(point,left,right); }
        catch(const std::domain_error&) { rejected=true; }
        if(scenario=="valid") {
            if(rejected || left.x!=0 || left.y!=2 || right.x!=0 || right.y!=-2) throw std::runtime_error("wrong nearest boundary");
        } else {
            if(!rejected) throw std::runtime_error("invalid boundary accepted");
            if(left.x!=77 || right.x!=88) throw std::runtime_error("partial output on failure");
            scenario="valid";
            getObstacleLaneSidePosition({},left,right);
            if(left.y!=2 || right.y!=-2) throw std::runtime_error("recovery failed");
        }
        std::cout<<"PASS\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what(); return 1; }
}
