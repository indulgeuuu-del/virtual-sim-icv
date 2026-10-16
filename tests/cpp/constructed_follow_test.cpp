// Appended to the shared complete-constructor fixture by the runner.
// SDK scheduling, ego update/navigation and steering geometry are substitutes;
// complete follow loop, constructors, following controller and drive are production.
#include <tuple>
std::ostream& operator<<(std::ostream& os,const SSD::SimPoint3D& p) { return os<<p.x<<','<<p.y<<','<<p.z; }
// @CONTROLLER@
enum class ESimOne_Gear_Mode { ESimOne_Gear_Mode_Drive, ESimOne_Gear_Mode_Reverse, ESimOne_Gear_Mode_Neutral };
enum class ESimOne_Throttle_Mode { ESimOne_Throttle_Mode_Speed };
struct Control { float throttle=0,steering=0; ESimOne_Gear_Mode gear=ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
    ESimOne_Throttle_Mode throttleMode=ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed; };
auto pControl=std::make_unique<Control>();
struct Logger { enum class Color { BrightBlue,BrightMagenta }; SilentLog& operator()(Color) { return silentLog; } } globalLogger;
namespace FlagType { bool isObstacleInitialized=false,isManualTrackMode=false; }
int frameCount=0,waits=0,advances=0,drives=0,caseIdx=25,caseStop=25;
float followingKp=-0.2f,followingKi=0.1f,followingKd=0,steerKp=1,steerKd=0,steerKpUse=0,steerKdUse=0;
AdaptiveFollowing adaptiveFollowing(followingKp,followingKi,followingKd,45);
std::vector<int> validWayPoints;
SSD::SimPoint3DVector targetPath;
float steerKi=0,steeringOffsetKp=0,caseMaxSpeed=20,achieveThres=1;
PositionalPID steerPID(1,0,0);
std::vector<std::tuple<SSD::SimPoint3D,SSD::SimPoint3D>> slideConfig;
struct Gps {};
auto pGps=std::make_unique<Gps>();
struct Lights {};
auto pLight=std::make_unique<Lights>();
namespace UtilDriver {
double calculateSteering(const SSD::SimPoint3DVector&,const Gps*,size_t& index) {
    index=0; return scenario=="steering_nan" ? std::nan("") : 0.25;
}
}
double approximateCurvature(const std::vector<SSD::SimPoint3D>&) { throw std::runtime_error("manual curvature outside fixture scope"); }
std::vector<Obstacle> obstacleList;
std::vector<Control> sent;
std::vector<size_t> listSizes;
std::vector<std::string> frames;
bool failSend=false;
struct EndFrames {};
namespace SimOneAPI {
int Wait() { if(waits==frames.size()) throw EndFrames{}; scenario=frames.at(waits++);return waits; }
void NextFrame(int frame) {
    if(frame!=waits || ++advances!=waits) throw std::runtime_error("unpaired frame");
    listSizes.push_back(obstacleList.size());
}
bool GetGroundTruth(const char*,GroundTruth* out) {
    *out=GroundTruth{};out->obstacleSize=2;
    for(auto& o:out->obstacle) {
        o.posX=100;
        for(int i=0;i<10;++i) o.prediction.trajectory[i]={float(i),0,0};
    }
    if(scenario=="prediction_nan") out->obstacle[1].prediction.trajectory[3].x=std::nanf("");
    if(scenario=="prediction_inf") out->obstacle[1].prediction.trajectory[3].y=std::numeric_limits<float>::infinity();
    if(scenario=="prediction_growth") for(int i=0;i<10;++i) out->obstacle[1].prediction.trajectory[i].x=std::pow(100.0f,i);
    return true;
}
bool SetDrive(const char*,Control* c) { sent.push_back(*c);return !(failSend && scenario!="valid"); }
void SetSignalLights(const char*,Lights*) { ++drives; }
}
bool MainVehicleFixture::update() { *pControl=Control{};return true; }
// @DRIVE@
bool initNavigation(std::vector<int>&,SSD::SimPoint3DVector& path) { path={{0,0,0},{100,0,0}};return true; }
void recordEvaluation() {}
void waitInitial() {}
bool updateObstacleData(int timeoutFrames=6);
// @FETCH@
// @NEAREST@
// @FOLLOWING@
// @LOOP@
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("missing case");
        std::string name=argv[1];
        frames={"valid",name,"valid"};
        if(name=="valid") frames={"valid","valid","valid"};
        if(name=="initial") frames={"prediction_nan","valid","valid"};
        if(name=="repeated") frames={"prediction_nan","prediction_nan","valid"};
        if(name=="reverse") { frames={"valid","prediction_nan","valid"}; mainVehicle.reverse=true; }
        if(name=="send_failure") { frames={"valid","prediction_nan","valid"};failSend=true; }
        if(name=="slide" || name=="reverse_slide" || name=="speed_cap") {
            frames={"valid","prediction_nan","valid"};
            if(name=="speed_cap") caseMaxSpeed=2;
            else slideConfig.push_back({{0,0,0},{50,0,0}});
            if(name=="reverse_slide") mainVehicle.reverse=true;
        }
        try { main_follow(); } catch(const EndFrames&) {}
        if(sent.size()!=3 || advances!=3 || waits!=3) throw std::runtime_error("missing output or frame");
        int expectedDrives=0;
        for(size_t i=0;i<3;++i) {
            if(frames[i]=="valid" || frames[i]=="steering_nan") {
                ++expectedDrives;
                if(listSizes[i]!=2) throw std::runtime_error("valid construction missing targets");
                if(frames[i]=="steering_nan") {
                    if(sent[i].throttle!=0 || sent[i].steering!=0 || sent[i].gear==ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral)
                        throw std::runtime_error("invalid steering not stopped");
                } else {
                    if(sent[i].throttle<=0 || sent[i].throttle>caseMaxSpeed) throw std::runtime_error("incorrect final speed");
                    if(mainVehicle.reverse && sent[i].throttle>3) throw std::runtime_error("reverse cap lost");
                    if(!mainVehicle.reverse && sent[i].steering!=0.25f) throw std::runtime_error("steering PID output missing");
                    if(mainVehicle.reverse && sent[i].steering!=0) throw std::runtime_error("reverse steering not cleared");
                    if(!slideConfig.empty() && sent[i].gear!=ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral)
                        throw std::runtime_error("slide recovery missing");
                }
            } else {
                if(sent[i].throttle!=0 || sent[i].steering!=0 || listSizes[i]!=0) throw std::runtime_error("failed construction retained target or motion");
                auto gear=mainVehicle.reverse ? ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse : ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
                if(sent[i].gear!=gear) throw std::runtime_error("incorrect stop gear");
            }
        }
        if(drives!=expectedDrives) throw std::runtime_error("invalid input reached drive");
        if(frames[0]=="valid" && frames[1]!="valid" && frames[1]!="steering_nan" && sent[0].throttle!=sent[2].throttle)
            throw std::runtime_error("PID history survived failure");
        if(failSend && silentLog.text.str().find("SetDrive failed")==std::string::npos)
            throw std::runtime_error("missing send failure log");
        std::cout<<"PASS";return 0;
    } catch(const std::exception& e) { std::cerr<<e.what();return 1; }
}
