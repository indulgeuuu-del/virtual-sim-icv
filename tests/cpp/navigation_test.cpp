// Exact initNavigation body; only SDK map/waypoint IO is replaced.
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace SSD {
template<class T> using SimVector=std::vector<T>;
struct SimPoint3D { double x=0,y=0,z=0; SimPoint3D()=default; SimPoint3D(double a,double b,double c):x(a),y(b),z(c){} };
using SimPoint3DVector=std::vector<SimPoint3D>;
using SimString=std::string;
}
namespace HDMapStandalone { struct MLaneInfo { SSD::SimPoint3DVector centerLine; }; }
struct Waypoint { double posX=0,posY=0; };
struct Waypoints { int wayPointsSize=2; Waypoint wayPoints[2]={{0,0},{100,0}}; };
auto pWayPoints=std::make_unique<Waypoints>();
struct { const char* id="0"; } mainVehicle;
SSD::SimPoint3DVector initialPath;
bool waypointsOk=true, routeOk=true, laneOk=true;
int routeSize=2,laneSize=2,routeCalls=0;
SSD::SimString m_SampleGetNearMostLane(const SSD::SimPoint3D&) { return "1_0_1"; }
struct Logger {
    enum class Color { BrightMagenta };
    Logger& operator()(Color) { return *this; }
    template<class T> Logger& operator<<(const T&) { return *this; }
} globalLogger;
namespace SimOneAPI {
bool GetWayPoints(const char*,Waypoints*) { return waypointsOk; }
bool GenerateRoute(const SSD::SimPoint3DVector& input,SSD::SimVector<int>& valid,SSD::SimPoint3DVector& output) {
    ++routeCalls;
    if(input.size()!=2) throw std::runtime_error("accumulated old input points");
    valid={0,1}; output=SSD::SimPoint3DVector(routeSize,{100,0,0});
    return routeOk; // false may still modify output arguments
}
bool GetLaneSample(const SSD::SimString&,HDMapStandalone::MLaneInfo& lane) {
    lane.centerLine=SSD::SimPoint3DVector(laneSize,{100,0,0}); return laneOk;
}
}
// @NAVIGATION@
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    try {
        require(argc==2,"test name required");
        const std::string name=argv[1];
        SSD::SimVector<int> valid={7};
        SSD::SimPoint3DVector path={{7,0,0},{8,0,0}};
        bool expectSuccess=false;
        if(name=="waypoint_failure") waypointsOk=false;
        else if(name=="zero_waypoints") pWayPoints->wayPointsSize=0;
        else if(name=="route_failure") routeOk=false;
        else if(name=="empty_route") routeSize=0;
        else if(name=="short_route") routeSize=1;
        else if(name=="lane_failure") { pWayPoints->wayPointsSize=1; laneOk=false; }
        else if(name=="empty_lane") { pWayPoints->wayPointsSize=1; laneSize=0; }
        else if(name=="short_lane") { pWayPoints->wayPointsSize=1; laneSize=1; }
        else if(name=="route_success" || name=="repeat") expectSuccess=true;
        else if(name=="lane_success") { pWayPoints->wayPointsSize=1; expectSuccess=true; }
        else if(name=="recovery") routeOk=false;
        else throw std::runtime_error("unknown test");
        const bool result=initNavigation(valid,path);
        require(result==expectSuccess,"incorrect build status");
        if(!expectSuccess) {
            require(path.size()==2 && path[0].x==7 && valid==std::vector<int>{7},"failed build corrupted active route");
            require(initialPath.empty(),"failed build corrupted input route");
        } else {
            require(path.size()==2 && path[0].x==100,"successful route not installed");
            require(initialPath.size()==pWayPoints->wayPointsSize,"input points accumulated");
            if(name=="lane_success") require(valid.empty(),"single lane retained old waypoint indexes");
        }
        if(name=="repeat" || name=="recovery") {
            routeOk=true;
            require(initNavigation(valid,path),"retry failed");
            require(initialPath.size()==2 && path.size()==2 && path[0].x==100,"retry state invalid");
        }
        std::cout<<"PASS "<<name<<'\n'; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
