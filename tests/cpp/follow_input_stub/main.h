#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "controller.hpp"

// SDK boundary fixture: failed reads may leave OLD output or CLEAR it.
// Full following loop, obstacle fetch and GPS gate come from production.
namespace SSD { template<class T> using SimVector = std::vector<T>; }
namespace SSD {
struct SimString {
    std::string value;
    SimString(const char* s=""):value(s) {}
    bool Empty() const { return value.empty(); }
    const char* GetString() const { return value.c_str(); }
};
struct SimPoint3D {
    double x,y,z;
    SimPoint3D(double a=0,double b=0,double c=0):x(a),y(b),z(c) {}
};
}
namespace HDMapStandalone {
struct MLaneLink {
    SSD::SimString leftNeighborLaneName,rightNeighborLaneName;
    SSD::SimVector<SSD::SimString> predecessorLaneNameList,successorLaneNameList;
};
}
class UtilMath { public: static double calculateSpeed(const double, const double); };
// Old revisions abort on ambiguous successors. Turn that fatal assertion into
// a test failure without opening a Windows crash dialog; it never means success.
#define ASSERT(ok, message) do { if(!(ok)) throw std::runtime_error(message); } while(0)
struct Logger {
    enum class Color { BrightBlue, BrightMagenta, BrightGreen };
    std::string text;
    Logger& operator()(Color) { return *this; }
    template<class T> Logger& operator<<(const T& value) {
        std::ostringstream out; out << value; text += out.str(); return *this;
    }
};
inline Logger globalLogger;
#define LOG globalLogger
enum class ESimOne_Gear_Mode { ESimOne_Gear_Mode_Drive, ESimOne_Gear_Mode_Reverse };
enum class ESimOne_Throttle_Mode { ESimOne_Throttle_Mode_Speed, ESimOne_Throttle_Mode_Percent };
struct Control {
    float throttle=8, steering=0, brake=0;
    bool handbrake=false, isManualGear=false;
    ESimOne_Gear_Mode gear=ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
    ESimOne_Throttle_Mode throttleMode=ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
};
struct Light { int signalLights=0; };
struct Gps { double posX=10,posY=20,posZ=0; float velX=3,velY=0,oriX=0,oriY=0,oriZ=0; };
struct RawObstacle {
    double s=100;
    float posX=100,posY=0,posZ=0,velX=1,velY=0,velZ=0,oriZ=0,width=2,length=4;
};
struct GroundTruth { int obstacleSize=1; RawObstacle obstacle[2]; };
inline auto pControl=std::make_unique<Control>();
inline auto pLight=std::make_unique<Light>();
inline auto pGps=std::make_unique<Gps>();
inline auto pObstacle=std::make_unique<GroundTruth>();
namespace FlagType {
    inline bool isObstacleInitialized=false, isMainVehicleInitialized=false, isSimOneInitialized=false;
}
inline int frameCount=0, presetLight=0, waits=0, advances=0, drives=0, gpsReads=0, obstacleReads=0;
inline float caseTargetSpeed=8, steerKp=1, steerKd=0, steerKpUse=0, steerKdUse=0;
inline int caseIdx=25, caseStop=25;
inline float followingKp=-0.2f, followingKi=0, followingKd=0, followingLimitSpeed=3.5f;
inline AdaptiveFollowing adaptiveFollowing(-0.2f,0,0,45);
inline std::vector<int> validWayPoints, targetPath;
inline std::vector<int> trafficLightList, stopLineList;
struct Timer { void tic() {} double get() const { return 0.1; } };
inline Timer timer;
inline float calculateFps(int,double) { return 10; }
struct Input { bool gps=true, obstacle=true, send=true; int count=1; bool route=true;
    int obstacleHeading=0; // 1 API failure, 2 zero, 3 NaN, 4 infinity, 5 empty lane
    int rawField=0, rawIndex=0; float rawValue=100;
    bool near=true, projection=true, emptyLane=false, finiteProjection=true, finitePosition=true;
    bool heading=true,finiteHeading=true,zeroHeading=false,link=true,clearLink=false;
    bool badRoad=false,largeRoad=false,ambiguous=false,noNeighbors=false,noSuccessors=false,junctionOnly=false;
};
inline bool mapValid(const Input& a) {
    return a.near && a.projection && !a.emptyLane && a.finiteProjection && a.finitePosition &&
           a.heading && a.finiteHeading && !a.zeroHeading && a.link && !a.badRoad && !a.largeRoad && !a.ambiguous;
}
inline std::vector<Input> inputs;
inline bool clearFailedOutput=false;
inline std::vector<Control> sends;
struct MapState { double x=0,s=0,t=0; std::string lane,lastLane,next;
    float heading=0; bool left=false,right=false; std::string leftName,rightName;
    size_t successors=0;
};
inline std::vector<MapState> mapStates;
inline void recordMapState();
inline const Input& current() { return inputs.at(waits-1); }
struct EndFrames {};
namespace SimOneAPI {
// Official API is void: empty list is not a failure signal.
inline void GetTrafficLightList(std::vector<int>&) {}
inline int Wait() { if(waits==inputs.size()) throw EndFrames{}; return ++waits; }
inline void NextFrame(int frame) {
    if(frame!=waits || advances+1!=waits) throw std::runtime_error("frame pairing");
    ++advances;
    recordMapState();
}
inline bool GetGps(const char*, Gps* out) {
    ++gpsReads; out->posX=current().finitePosition ? 10+waits : std::nan(""); return current().gps;
}
inline bool GetNearMostLane(const SSD::SimPoint3D&,SSD::SimString& lane,double& s,double& t,double& sc,double& tc) {
    lane=current().emptyLane ? "" : "1_0_1"; s=10;t=sc=tc=0;
    if(current().badRoad) lane="invalid_0_1";
    if(current().largeRoad) lane="999999999999999999999_0_1";
    return current().near; // Failure may still write an apparently valid lane.
}
inline bool GetLaneST(const SSD::SimString&,const SSD::SimPoint3D&,double& s,double& t) {
    s=current().projection ? 10 : 999;
    t=current().finiteProjection ? 0 : std::nan("");
    return current().projection;
}
inline bool GetInertialFromLaneST(const SSD::SimString& lane,double,double,SSD::SimPoint3D& pt,SSD::SimPoint3D& dir) {
    if(std::strcmp(lane.GetString(),"obstacle")==0 || lane.Empty()) {
        pt={0,0,0}; dir={1,0,0};
        if(current().obstacleHeading==2) dir={0,0,0};
        if(current().obstacleHeading==3) dir.y=std::nan("");
        if(current().obstacleHeading==4) dir.z=std::numeric_limits<double>::infinity();
        return current().obstacleHeading!=1;
    }
    pt={0,0,0}; dir={current().zeroHeading ? 0.0 : 1.0,0,0};
    if(!current().finiteHeading) dir.y=std::nan("");
    return current().heading;
}
inline bool GetLaneLink(const SSD::SimString&,HDMapStandalone::MLaneLink& link) {
    if(!current().link && !current().clearLink) {
        link.leftNeighborLaneName="bad_partial_lane"; return false;
    }
    link=HDMapStandalone::MLaneLink{};
    if(!current().link) return false;
    if(!current().noNeighbors) link.leftNeighborLaneName="1_0_2";
    if(!current().noSuccessors) link.successorLaneNameList={"2_0_1"};
    if(current().ambiguous) link.successorLaneNameList.push_back("3_0_1");
    return true;
}
inline bool IsTwoSideRoad(const long&) { return false; } // false is a property, not failure.
// Inherited code uses the older long output; current web docs use SimString.
// Match the inherited signature here; actual competition SDK build is pending.
inline bool IsInJunction(const SSD::SimString&,long& id) { id=1; return current().junctionOnly; }
inline bool GetGroundTruth(const char*, GroundTruth* out) {
    ++obstacleReads;
    if(!current().obstacle) {
        if(clearFailedOutput) *out=GroundTruth{0};
        return false;
    }
    *out=GroundTruth{};
    out->obstacleSize=current().count;
    auto& raw=out->obstacle[current().rawIndex];
    float* fields[]={&raw.posX,&raw.posY,&raw.posZ,&raw.velX,&raw.velY,&raw.velZ,&raw.oriZ,&raw.width,&raw.length};
    *fields[current().rawField]=current().rawValue;
    return true;
}
inline bool SetDrive(const char*, Control* control) {
    sends.push_back(*control); return current().send;
}
}
struct MainVehicle {
    const char* id="0";
    double s=0,t=0;
    SSD::SimPoint3D pt;
    SSD::SimString laneID,lastLaneID,nextLaneID;
    HDMapStandalone::MLaneLink laneLink;
    float laneAzimuth=0,vx=0,vy=0,roll=0,pitch=0,yaw=0,steeringOffset=0;
    bool leftLaneExist=false,rightLaneExist=false,isTwoSideRoad=false,useDefaultPath=true;
    float speed=3;
    bool reverse=false;
    struct { void clear() {} } neighborhood;
    bool update(int timeoutFrames=6);
    // Lateral/drive logic is NOT tested here; invalid-input stop must bypass it.
    void drive() { ++drives; SimOneAPI::SetDrive(id,pControl.get()); }
};
inline MainVehicle mainVehicle;
inline void recordMapState() {
    mapStates.push_back({mainVehicle.pt.x,mainVehicle.s,mainVehicle.t,
                        mainVehicle.laneID.GetString(),mainVehicle.lastLaneID.GetString(),
                        mainVehicle.nextLaneID.GetString(),mainVehicle.laneAzimuth,
                        mainVehicle.leftLaneExist,mainVehicle.rightLaneExist,
                        mainVehicle.laneLink.leftNeighborLaneName.GetString(),
                        mainVehicle.laneLink.rightNeighborLaneName.GetString(),
                        mainVehicle.laneLink.successorLaneNameList.size()});
}
float getLaneAzimuth(const SSD::SimString&);
inline SSD::SimString m_SampleGetNearMostLane(const SSD::SimPoint3D& pt) {
    SSD::SimString lane; double s,t,sc,tc;
    SimOneAPI::GetNearMostLane(pt,lane,s,t,sc,tc); return lane;
}
struct Obstacle {
    enum class Type { Static, Vertical, Horizental, AmongCrosswalk };
    Type type=Type::Horizental;
    double sRelativeToVehicle=100;
    float velocity=3;
    bool isSameLaneWithMainVehicle=true, isSameRoadWithMainVehicle=true;
    Obstacle() = default;
    explicit Obstacle(const RawObstacle& raw): sRelativeToVehicle(raw.s) {
        // Exercise the real helper; full map-dependent construction remains outside this fixture.
        getLaneAzimuth(inputs.at(waits-1).obstacleHeading==5 ? "" : "obstacle");
    }
    explicit Obstacle(const std::vector<size_t>& indices): Obstacle(pObstacle->obstacle[indices.at(0)]) {}
};
inline float groupingDistThres=3;
inline void groupObstacleByDist(const std::vector<size_t>& indices,float,std::vector<std::vector<size_t>>& groups) {
    for(auto i:indices) groups.push_back({i});
}
inline std::vector<Obstacle> obstacleList;
inline int navigationCalls=0;
inline bool initNavigation(std::vector<int>&,std::vector<int>& path) {
    ++navigationCalls;
    if(!inputs.at(waits>0 ? waits-1 : 0).route) { path.clear(); return false; }
    path={0,100}; return true;
}
inline void recordEvaluation() {}
inline void waitInitial() {}
bool updateObstacleData(int timeoutFrames=6);
float caculateFollowingSpeed(const std::vector<Obstacle>&,int);
int main_follow();
int main_track();
