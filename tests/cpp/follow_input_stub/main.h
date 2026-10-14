#pragma once
#include <algorithm>
#include <cmath>
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
struct Gps { double posX=10,posY=20,posZ=0; };
struct RawObstacle { double s=100; };
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
    bool near=true, projection=true, emptyLane=false, finiteProjection=true, finitePosition=true; };
inline std::vector<Input> inputs;
inline bool clearFailedOutput=false;
inline std::vector<Control> sends;
struct MapState { double x,s,t; std::string lane,lastLane; };
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
    return current().near; // Failure may still write an apparently valid lane.
}
inline bool GetLaneST(const SSD::SimString&,const SSD::SimPoint3D&,double& s,double& t) {
    s=current().projection ? 10 : 999;
    t=current().finiteProjection ? 0 : std::nan("");
    return current().projection;
}
inline bool GetGroundTruth(const char*, GroundTruth* out) {
    ++obstacleReads;
    if(!current().obstacle) {
        if(clearFailedOutput) *out=GroundTruth{0};
        return false;
    }
    out->obstacleSize=current().count; return true;
}
inline bool SetDrive(const char*, Control* control) {
    sends.push_back(*control); return current().send;
}
}
struct MainVehicle {
    const char* id="0";
    double s=0,t=0;
    SSD::SimPoint3D pt;
    SSD::SimString laneID,lastLaneID;
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
                        mainVehicle.laneID.GetString(),mainVehicle.lastLaneID.GetString()});
}
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
    explicit Obstacle(const RawObstacle& raw): sRelativeToVehicle(raw.s) {}
};
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
