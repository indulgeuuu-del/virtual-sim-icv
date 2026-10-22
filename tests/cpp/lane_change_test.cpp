// Exact lane-change, speed arbitration and drive bodies. SDK IO/vehicle state,
// pursuit geometry and manual curvature are fixtures; PID executes production code.
// Scoring PDF physical p18, IDs16/17: collision zero; correct indicators required.
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <algorithm>
#include <tuple>
namespace SSD {
using SimString=std::string;
template<class T> using SimVector=std::vector<T>;
struct SimPoint3D { double x=0,y=0,z=0; };
std::ostream& operator<<(std::ostream& out,const SimPoint3D& p) {return out<<p.x<<','<<p.y;}
using SimPoint3DVector=std::vector<SimPoint3D>;
}
namespace HDMapStandalone {
struct MLaneLink { SSD::SimString leftNeighborLaneName,rightNeighborLaneName; };
struct MLaneInfo { SSD::SimString laneName; SSD::SimPoint3DVector centerLine; };
}
enum class ESimOne_Signal_Light { ESimOne_Signal_Light_None,ESimOne_Signal_Light_LeftBlinker,ESimOne_Signal_Light_RightBlinker };
enum class ESimOne_Throttle_Mode { ESimOne_Throttle_Mode_Speed,ESimOne_Throttle_Mode_Percent };
enum class ESimOne_Gear_Mode { ESimOne_Gear_Mode_Drive,ESimOne_Gear_Mode_Reverse,ESimOne_Gear_Mode_Neutral };
struct Control { double throttle=8,steering=0; ESimOne_Throttle_Mode throttleMode=ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
    ESimOne_Gear_Mode gear=ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive; };
auto pControl=std::make_unique<Control>();auto& control=*pControl;
struct Lights { ESimOne_Signal_Light signalLights=ESimOne_Signal_Light::ESimOne_Signal_Light_None; };
auto pLight=std::make_unique<Lights>();auto& light=*pLight;
struct Gps { double angVelZ=.2; };
auto pGps=std::make_unique<Gps>();auto& gps=*pGps;
struct Vehicle { SSD::SimString laneID="ego"; SSD::SimPoint3D pt{1,0,0}; bool leftLaneExist=true,rightLaneExist=false;
    double steeringOffset=.7; const char* id="0"; float speed=3;
    bool reverse=false,useDefaultPath=true; void drive();
    struct { std::vector<int> left,right;
        float vf=0,vb=0,vl=0,vr=0,vlf=0,vrf=0,vlb=0,vrb=0;
        bool bf=false,bb=false,bl=false,br=false,blf=false,brf=false,blb=false,brb=false;
    } neighborhood; } mainVehicle;
struct StopLine { bool isVechicleInSameLane=true; double toVehicleDist=10; };
SSD::SimPoint3DVector targetPath{{7,0,0},{8,0,0}},initialPath{{0,0,0},{100,0,0}};
double laneChangeDistThres=30,completeLaneChangeTThres=1.5,steerKpUse=1.8;
std::ostringstream logStream;
#define LOG logStream
struct Logger { enum class Color { BrightGreen,BrightMagenta }; };
std::ostream& globalLogger(Logger::Color) { return logStream; }
// @CONTROLLER@
namespace FlagType { bool isManualTrackMode=false; }
float steerKi=0,steerKdUse=0,steeringOffsetKp=0,caseMaxSpeed=15,achieveThres=1;
int caseIdx=2;PositionalPID steerPID(1,0,0);
std::vector<std::tuple<SSD::SimPoint3D,SSD::SimPoint3D>> slideConfig;
namespace UtilMath { double planarDistance(const SSD::SimPoint3D& a,const SSD::SimPoint3D& b) {return std::hypot(a.x-b.x,a.y-b.y);} }
namespace UtilDriver { double calculateSteering(const SSD::SimPoint3DVector&,const Gps*,size_t& i) {i=0;return 0;} }
double approximateCurvature(const SSD::SimPoint3DVector&) {throw std::runtime_error("manual mode outside fixture");}
std::vector<Control> sent;std::vector<ESimOne_Signal_Light> sentLights;
struct { bool state=true; } overtakeSwitcher;
std::string fault;
int sampleCalls=0,routeCalls=0;
namespace SimOneAPI {
bool SetDrive(const char*,Control* c) {sent.push_back(*c);return true;}
void SetSignalLights(const char*,Lights* l) {sentLights.push_back(l->signalLights);}
bool GetLaneLink(const SSD::SimString&,HDMapStandalone::MLaneLink& out) {
    if(fault=="link_unwritten")return false;
    out.leftNeighborLaneName="left";out.rightNeighborLaneName="right";
    if(fault=="empty_link")out.leftNeighborLaneName="";
    return fault!="link_written";
}
bool GetLaneSample(const SSD::SimString& id,HDMapStandalone::MLaneInfo& out) {
    ++sampleCalls;
    if(fault=="sample_unwritten")return false;
    out.laneName=id;out.centerLine={{10,4,0},{50,4,0}};
    if(fault=="sample_empty")out.centerLine.clear();
    if(fault=="sample_short")out.centerLine.resize(1);
    if(fault=="sample_nan")out.centerLine[1].x=std::numeric_limits<double>::quiet_NaN();
    if(fault=="sample_inf")out.centerLine[0].z=std::numeric_limits<double>::infinity();
    if(fault=="sample_degenerate")out.centerLine[1]=out.centerLine[0];
    if(fault=="sample_wrong_lane")out.laneName="other";
    return fault!="sample_written";
}
bool GetLaneST(const SSD::SimString&,const SSD::SimPoint3D&,double& s,double& t) {
    if(fault=="projection_unwritten")return false;
    s=5;t=4;
    if(fault=="complete" || fault.rfind("route_",0)==0 || fault.rfind("destination_",0)==0)t=0;
    if(fault=="equal")t=1.5;
    if(fault=="projection_nan")s=std::numeric_limits<double>::quiet_NaN();
    if(fault=="projection_inf")t=std::numeric_limits<double>::infinity();
    return fault!="projection_written";
}
bool GenerateRoute(const SSD::SimPoint3DVector&,SSD::SimVector<int>& valid,SSD::SimPoint3DVector& out) {
    ++routeCalls;valid={0,1};out={{1,0,0},{100,0,0}};
    if(fault=="route_empty")out.clear();
    if(fault=="route_short")out.resize(1);
    if(fault=="route_nan")out[1].x=std::numeric_limits<double>::quiet_NaN();
    return fault!="route_written";
}
}
// @LANE_CHANGE@
// @DRIVE@
// @ARBITRATION@
void require(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
int main(int argc,char** argv) {
    try {
        std::string arg=argc>1?argv[1]:"static_left";
        bool moving=arg.rfind("moving_",0)==0;
        fault=arg.substr(7);
        const bool retryComplete=fault.rfind("route_",0)==0 || fault.rfind("destination_",0)==0;
        bool output=fault.rfind("output_",0)==0;
        if(output) {
            mainVehicle.reverse=fault=="output_reverse" || fault=="output_both";
            if(fault=="output_slide" || fault=="output_both")slideConfig={{{1,0,0},{50,0,0}}};
            fault="link_written";
        }
        StopLine line;
        bool inactive=fault=="far" || fault=="unrelated";
        bool success=fault=="left" || fault=="right" || fault=="complete" || fault=="equal";
        if(fault=="right") {mainVehicle.leftLaneExist=false;mainVehicle.rightLaneExist=true;}
        if(fault=="blocked") mainVehicle.neighborhood.left={1};
        if(fault=="none")mainVehicle.leftLaneExist=false;
        if(fault=="far")line.toVehicleDist=30;
        if(fault=="unrelated")line.isVechicleInSameLane=false;
        if(fault=="distance_nan")line.toVehicleDist=std::numeric_limits<double>::quiet_NaN();
        if(fault=="yaw_nan")gps.angVelZ=std::numeric_limits<double>::quiet_NaN();
        if(fault=="position_nan")mainVehicle.pt.x=std::numeric_limits<double>::quiet_NaN();
        if(fault=="destination_empty")initialPath.clear();
        if(fault=="destination_nan")initialPath.back().x=std::numeric_limits<double>::quiet_NaN();
        if(fault=="percent") {control.throttleMode=ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Percent;mainVehicle.leftLaneExist=false;}
        auto call=[&](){return moving?getLaneChangePath(line,2):getLaneChangePath(line);};
        const auto before=initialPath;
        bool result=call();require(result==success,"wrong success status");
        if(!success) {
            require(targetPath.size()==2 && targetPath[0].x==7 && targetPath[1].x==8,"failure corrupted active path");
            require(initialPath.size()==before.size() && (before.empty() || initialPath[0].x==before[0].x),"failure corrupted route inputs");
            require(mainVehicle.steeringOffset==.7 && steerKpUse==1.8,"failure mutated steering state");
            require(control.throttle==(inactive?8:0),"failure did not request zero speed");
            if(!inactive) {
                require(control.throttleMode==ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed,"failure left percent mode");
                require(!logStream.str().empty(),"failure not logged");
            }
        } else if(fault=="complete") {
            require(routeCalls==1 && targetPath[0].x==1 && initialPath[0].x==1,"completed route not installed");
            require(light.signalLights==ESimOne_Signal_Light::ESimOne_Signal_Light_None,"completed turn signal not cleared");
        } else {
            require(targetPath.size()==2 && targetPath[0].y==4,"lane path missing");
            require(light.signalLights==(fault=="right"?ESimOne_Signal_Light::ESimOne_Signal_Light_RightBlinker:
                ESimOne_Signal_Light::ESimOne_Signal_Light_LeftBlinker),"wrong turn signal");
            require(routeCalls==0,"premature completion");
        }
        if(output) {
            mainVehicle.neighborhood.bl=true;mainVehicle.neighborhood.vl=9;
            trackSpeedForTest(9,false,true);
            require(sent.size()==1 && sent.back().throttle==0,"zero speed lost before SetDrive");
            require(sent.back().gear==(mainVehicle.reverse?ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse:
                ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive),"stop remained in neutral");
        }
        // Failed queries may partially overwrite output. A later good frame
        // must recover without restarting the vehicle program.
        fault=retryComplete?"complete":"left";line={};mainVehicle.leftLaneExist=true;mainVehicle.neighborhood.left.clear();
        mainVehicle.pt={1,0,0};gps.angVelZ=.2;control.throttle=8;
        if(retryComplete)initialPath={{0,0,0},{100,0,0}};
        require(call() && (retryComplete?targetPath[0].x==1:targetPath[0].y==4),"valid retry did not recover");
        if(output) {
            mainVehicle.speed=0;trackSpeedForTest(9,false,true);
            require(sent.back().throttle>0 && sent.back().gear!=ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral,"valid retry did not reach drive");
            require(sentLights.back()==ESimOne_Signal_Light::ESimOne_Signal_Light_LeftBlinker,"turn signal not sent");
        }
        std::cout<<"PASS "<<arg<<'\n';return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
