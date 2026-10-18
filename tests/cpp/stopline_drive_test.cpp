// Appended to stopline_projection fixture before main(). Production constructors,
// distance, nearest selection, Whole braking, speed arbitration and drive execute.
// SDK/map, vehicle state, behind test and steering geometry remain substitutes.
#include <memory>
#include <tuple>
#include <algorithm>
std::ostream& operator<<(std::ostream& out,const SSD::SimPoint3D& p) { return out<<p.x<<','<<p.y; }
// @CONTROLLER@
enum class ESimOne_Gear_Mode { ESimOne_Gear_Mode_Drive,ESimOne_Gear_Mode_Reverse,ESimOne_Gear_Mode_Neutral };
enum class ESimOne_Throttle_Mode { ESimOne_Throttle_Mode_Speed };
struct Control { double throttle=8,steering=0; ESimOne_Gear_Mode gear=ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
    ESimOne_Throttle_Mode throttleMode=ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed; };
auto pControl=std::make_unique<Control>();
struct Gps {}; auto pGps=std::make_unique<Gps>();
struct Lights {}; auto pLight=std::make_unique<Lights>();
namespace FlagType { bool isManualTrackMode=false; }
float steerKpUse=1,steerKi=0,steerKdUse=0,steeringOffsetKp=0,caseMaxSpeed=15,achieveThres=1;
float stopLineDistThres=0.2f;
int caseIdx=2;
PositionalPID steerPID(1,0,0);
std::vector<SSD::SimPoint3D> targetPath={{0,0,0},{100,0,0}};
std::vector<std::tuple<SSD::SimPoint3D,SSD::SimPoint3D>> slideConfig;
namespace UtilMath { double planarDistance(const SSD::SimPoint3D& a,const SSD::SimPoint3D& b) { return std::hypot(a.x-b.x,a.y-b.y); } }
namespace UtilDriver { double calculateSteering(const std::vector<SSD::SimPoint3D>&,const Gps*,size_t& i) { i=0;return 0; } }
double approximateCurvature(const std::vector<SSD::SimPoint3D>&) { throw std::runtime_error("manual curvature outside scope"); }
std::vector<Control> sent;
namespace SimOneAPI {
void SetSignalLights(const char*,Lights*) {}
bool SetDrive(const char*,Control* control) { sent.push_back(*control);return true; }
}
struct { bool state=true; } overtakeSwitcher;
// @DEFAULT_AND_NEAREST@
// @DRIVE@
// @ARBITRATION@
void apply(const std::vector<StopLine>& lines) {
    *pControl=Control{};
    StopLine potentialStopLine;float minDistance;
    if(calculateNearestStopLine(lines,potentialStopLine,minDistance)) {
// @BRAKING@
    }
    // Deliberately request acceleration from a lateral neighbor after braking.
    mainVehicle.neighborhood.bl=true;mainVehicle.neighborhood.vl=6;
    trackSpeedForTest(9,false,true);
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("missing case");
        std::string name=argv[1];
        mainVehicle.reverse=name.find("reverse")!=std::string::npos || name.find("both")!=std::string::npos;
        if(name.find("slide")!=std::string::npos || name.find("both")!=std::string::npos) slideConfig={{{0,0,0},{50,0,0}}};
        if(name.rfind("sequence_",0)==0) {
            StopLineControl::Sequence sequence(5);
            auto send=[&](double t,double distance,double speed,bool permission) {
                mainVehicle.speed=static_cast<float>(speed);
                auto r=sequence.update(t,distance,2.43,speed,8,.2,permission);
                *pControl=Control{};pControl->throttle=r.targetSpeed;
                mainVehicle.neighborhood.bl=true;mainVehicle.neighborhood.vl=9;
                trackSpeedForTest(9,false,true);
                if(sent.back().throttle>r.targetSpeed) throw std::runtime_error("sequence speed raised downstream");
                if(speed>r.targetSpeed && sent.back().gear==ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral)
                    throw std::runtime_error("sequence slowdown remained in neutral");
                return r;
            };
            auto slow=send(0,3.43,3,false);
            if(!(slow.targetSpeed>0 && slow.targetSpeed<3)) throw std::runtime_error("no gradual slowdown");
            for(int i=1;i<=23;++i) {
                send(i*.25,2.65,0,false);
                if(sent.back().throttle!=0 || sent.back().gear==ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral)
                    throw std::runtime_error("hold lost at final output");
            }
            auto released=send(6,2.65,0,true);
            if(!released.ready || sent.back().throttle<=0) throw std::runtime_error("departure blocked");
            if(sent.back().gear==ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral)
                throw std::runtime_error("departure stayed in neutral");
            std::cout<<"PASS";return 0;
        }
        fault="valid";StopLine farLine(HDMapStandalone::MSignal{});
        StopLine ignored=farLine;ignored.toVehicleDist=0;ignored.isBehind=true;
        StopLine adjacent=ignored;adjacent.isBehind=false;adjacent.isVechicleInSameLane=false;
        StopLine invalid=ignored;invalid.isBehind=false;invalid.isValid=false;
        bool shouldStop=name!="far" && name!="ignored";
        if(name=="near") stopLineX=5;
        if(name.rfind("light_",0)==0 || name.rfind("crosswalk_",0)==0) fault="empty";
        Obstacle obstacle;
        if(name.rfind("crosswalk_",0)==0) obstacle.type=Obstacle::Type::AmongCrosswalk;
        if(name.rfind("projection_",0)==0) { fault="unwritten";badCall=0; }
        StopLine constraint=(name.rfind("crosswalk_",0)==0 || name.rfind("projection_",0)==0)
            ? StopLine(obstacle) : StopLine(HDMapStandalone::MSignal{});
        if(name=="ignored") apply({ignored,adjacent,invalid});
        else apply({farLine,ignored,adjacent,invalid,constraint});
        if(sent.size()!=1 || !std::isfinite(sent[0].throttle) || (shouldStop ? sent[0].throttle!=0 : sent[0].throttle<=0))
            throw std::runtime_error("stop constraint lost before SetDrive or valid path blocked");
        if(shouldStop && sent[0].gear!=(mainVehicle.reverse ? ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse : ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive))
            throw std::runtime_error("stop remained in neutral or wrong gear");
        fault="valid";badCall=-1;calls=0;stopLineX=20;
        StopLine recovered(HDMapStandalone::MSignal{});apply({recovered});
        if(sent.size()!=2 || !std::isfinite(sent[1].throttle) || sent[1].throttle<=0)
            throw std::runtime_error("valid constraint recovery failed");
        std::cout<<"PASS";return 0;
    } catch(const std::exception& e) { std::cerr<<e.what();return 1; }
}
