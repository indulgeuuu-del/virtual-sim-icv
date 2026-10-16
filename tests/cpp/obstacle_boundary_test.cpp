#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>
#include <set>
namespace SSD {
struct SimPoint3D { double x=0,y=0,z=0; SimPoint3D(double a=0,double b=0,double c=0):x(a),y(b),z(c) {} };
struct SimString { std::string s; SimString(const char* p="lane"):s(p) {} bool Empty() const { return s.empty(); } bool operator<(const SimString& other) const { return s<other.s; } };
// Bounds-checked SDK fixture makes old-code out-of-bounds a test failure, not a crash.
struct SimPoint3DVector:std::vector<SimPoint3D> {
    using std::vector<SimPoint3D>::vector;
    const SimPoint3D& operator[](size_t i) const { return at(i); }
};
}
namespace HDMapStandalone {
struct MLaneInfo { SSD::SimPoint3DVector leftBoundary,rightBoundary,centerLine; };
struct MLaneLink { SSD::SimString leftNeighborLaneName{""},rightNeighborLaneName{""}; };
}
std::string scenario;
bool road=false;
namespace SimOneAPI {
bool GetNearMostLane(const SSD::SimPoint3D&,SSD::SimString& lane,double& s,double& t,double& sc,double& tc) {
    lane=scenario=="empty_lane" || scenario=="near_failed_empty" ? "" : "lane";
    s=t=sc=tc=0;
    return scenario!="near_failed_written" && scenario!="near_failed_empty";
}
bool GetLaneSample(const SSD::SimString& lane,HDMapStandalone::MLaneInfo& info) {
    if(road) {
        info.centerLine={{10,0,0},{0,0,0}};
        info.leftBoundary={{0,lane.s=="left" ? 6.0 : 2.0,0}};
        info.rightBoundary={{30,lane.s=="right" ? -6.0 : -2.0,0},{0,lane.s=="right" ? -6.0 : -2.0,0}};
        if(scenario=="aligned") info.leftBoundary={{10,lane.s=="left" ? 6.0 : 2.0,0},{0,lane.s=="left" ? 6.0 : 2.0,0}};
        if(scenario=="left_empty" && lane.s=="left") info.leftBoundary.clear();
        if(scenario=="right_empty" && lane.s=="right") info.rightBoundary.clear();
        if(scenario=="nan" && lane.s=="left") info.leftBoundary.at(0).x=std::nan("");
        return !(scenario=="failed_partial" || (scenario=="right_failed" && lane.s=="right"));
    }
    info.leftBoundary={{10,2,0},{0,2,0}};
    info.rightBoundary={{0,-2,0},{20,-2,0},{30,-2,0}};
    if(scenario=="left_empty") info.leftBoundary.clear();
    if(scenario=="right_empty") info.rightBoundary.clear();
    if(scenario=="nan") info.leftBoundary.at(0).x=std::nan("");
    if(scenario=="inf") info.rightBoundary.at(0).z=std::numeric_limits<double>::infinity();
    if(scenario=="all_nan") for(auto& p:info.rightBoundary) p.x=std::nan("");
    return scenario!="failed_partial";
}
bool GetLaneLink(const SSD::SimString& lane,HDMapStandalone::MLaneLink& link) {
    link={};
    if(lane.s=="lane" && scenario!="single") { link.leftNeighborLaneName="left"; link.rightNeighborLaneName="right"; }
    if(scenario=="cycle" && lane.s=="left") link.leftNeighborLaneName="lane";
    return scenario!="link_failed";
}
}
SSD::SimString m_SampleGetNearMostLane(const SSD::SimPoint3D& point) {
    SSD::SimString lane; double s,t,sc,tc;
    SimOneAPI::GetNearMostLane(point,lane,s,t,sc,tc); return lane;
}
namespace UtilMath { double planarDistance(const SSD::SimPoint3D& a,const SSD::SimPoint3D& b) { return std::hypot(a.x-b.x,a.y-b.y); } }
// @CHECKED_LANE@
// @BOUNDARY@
// @ROAD_BOUNDARY@
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("missing scenario");
        scenario=argv[1];
        if(scenario.rfind("road_",0)==0) { road=true; scenario=scenario.substr(5); }
        SSD::SimPoint3D left(77,77,77),right(88,88,88),point;
        if(scenario=="point_nan") point.x=std::nan("");
        bool rejected=false;
        const auto query=[&](const SSD::SimPoint3D& p) {
            if(road) getObstacleRoadSidePosition(p,left,right);
            else getObstacleLaneSidePosition(p,left,right);
        };
        try { query(point); }
        catch(const std::domain_error&) { rejected=true; }
        if(scenario=="valid" || scenario=="single" || scenario=="aligned") {
            const double edge=road && scenario!="single" ? 6 : 2;
            if(rejected || left.x!=0 || left.y!=edge || right.x!=0 || right.y!=-edge) throw std::runtime_error("wrong nearest boundary");
        } else {
            if(!rejected) throw std::runtime_error("invalid boundary accepted");
            if(left.x!=77 || right.x!=88) throw std::runtime_error("partial output on failure");
            scenario="valid";
            query({});
            if(left.y!=(road ? 6 : 2) || right.y!=(road ? -6 : -2)) throw std::runtime_error("recovery failed");
        }
        std::cout<<"PASS\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what(); return 1; }
}
