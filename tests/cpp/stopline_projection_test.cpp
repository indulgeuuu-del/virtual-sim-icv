// Real obstacle StopLine constructor and road-projection classification.
// Obstacle geometry, distance/behind helpers, SDK types and responses are fixtures.
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <iostream>
namespace SSD {
struct SimPoint3D { double x=0,y=0,z=0; SimPoint3D(double a=0,double b=0,double c=0):x(a),y(b),z(c) {} };
template<class T> struct SimVector:std::vector<T> {
    using std::vector<T>::vector;
    T& operator[](size_t i) { return this->at(i); }
    const T& operator[](size_t i) const { return this->at(i); }
};
}
namespace HDMapStandalone { struct MSignal {}; struct MObject { SSD::SimVector<SSD::SimPoint3D> boundaryKnots; }; }
struct ManualStopLineReservoir {};
struct Obstacle {
    enum class Type { Static,Horizental,Vertical,AmongCrosswalk } type=Type::Static;
    float vx=0,vy=0,velocity=0,velocityPlanar=0,width=2,length=4;
    float laneWidth=4,roadWidth=8,tRelativeToVehicle=0,tRelativeToLane=0;
    int laneID=1;
    SSD::SimPoint3D leftLanePt{20,2,0},rightLanePt{20,-2,0},leftRoadPt{20,4,0},rightRoadPt{20,-4,0},tl,br;
};
struct Vehicle { int laneID=1; bool leftLaneExist=true,rightLaneExist=false; SSD::SimPoint3D pt; } mainVehicle;
std::ostringstream logText;
#define LOG logText
#define IS_SINGLE_STOPLINE(o) true
// @CLASS@
int calls=0,badCall=-1;
std::string fault;
double leftT=2,rightT=-2;
namespace SimOneAPI {
bool GetRoadST(int,const SSD::SimPoint3D&,double& s,double& t,double& z) {
    int n=calls++;
    if(n==badCall && fault=="unwritten") return false;
    s=20;t=n==0 ? leftT : n==1 ? rightT : 0;z=0;
    if(n==badCall) {
        if(fault=="written") return false;
        if(fault=="nan_s") s=std::nan("");
        if(fault=="nan_t") t=std::nan("");
        if(fault=="inf_z") z=std::numeric_limits<double>::infinity();
    }
    return true;
}
void fillLines(SSD::SimVector<HDMapStandalone::MObject>& lines) {
    if(fault=="empty") return;
    HDMapStandalone::MObject line;line.boundaryKnots={{20,2,0},{20,-2,0}};
    if(fault=="knots0") line.boundaryKnots.clear();
    if(fault=="knots1") line.boundaryKnots.resize(1);
    if(fault=="nan") line.boundaryKnots[0].x=std::nan("");
    if(fault=="inf") line.boundaryKnots[1].z=std::numeric_limits<double>::infinity();
    if(fault=="degenerate") line.boundaryKnots[1]=line.boundaryKnots[0];
    lines.push_back(line);
}
void GetSpecifiedLaneStoplineList(int,SSD::SimVector<HDMapStandalone::MObject>& lines) { fillLines(lines); }
void GetStoplineList(const HDMapStandalone::MSignal&,int,SSD::SimVector<HDMapStandalone::MObject>& lines) { fillLines(lines); }
}
double StopLine::calculateVehicleDistance(const SSD::SimPoint3D&) const { return 20; }
bool StopLine::isStopLineBehindVehicle(const SSD::SimPoint3D&,const SSD::SimPoint3D&,const SSD::SimPoint3D&,const SSD::SimPoint3D&,const SSD::SimPoint3D&) { return false; }
// @CONSTRUCTOR@
// @PROJECTION@
// @LIGHT_CONSTRUCTOR@
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("missing case");
        std::string name=argv[1];Obstacle obstacle;
        if(name.rfind("light_",0)==0 || name.rfind("crosswalk_",0)==0) {
            bool light=name.rfind("light_",0)==0;
            fault=name.substr(name.find('_')+1);
            obstacle.type=Obstacle::Type::AmongCrosswalk;
            StopLine result=light ? StopLine(HDMapStandalone::MSignal{}) : StopLine(obstacle);
            if(!result.isValid || result.isBehind || !result.isVechicleInSameLane
                || result.type!=StopLine::Type::Whole || result.strategy!=StopLine::Strategy::StopStart
                || result.offset!=0 || result.toVehicleDist!=(fault=="valid" ? 20 : 0))
                throw std::runtime_error("bad map list did not retain stop constraint");
            if(fault!="valid" && logText.str().empty()) throw std::runtime_error("missing map failure log");
            fault="valid";calls=0;
            StopLine recovered=light ? StopLine(HDMapStandalone::MSignal{}) : StopLine(obstacle);
            if(recovered.toVehicleDist!=20 || !recovered.isValid) throw std::runtime_error("map list recovery failed");
            std::cout<<"PASS";return 0;
        }
        bool invalid=name!="valid" && name!="outside" && name!="reversed";
        if(invalid) {
            auto split=name.find_last_of('_');fault=name.substr(0,split);badCall=std::stoi(name.substr(split+1));
        }
        if(name=="outside") rightT=3;
        if(name=="reversed") { leftT=-2;rightT=2; }
        StopLine result(obstacle);
        if(invalid) {
            if(!result.isValid || result.isBehind || !result.isVechicleInSameLane
                || result.type!=StopLine::Type::Whole || result.strategy!=StopLine::Strategy::StopStart
                || result.toVehicleDist!=0 || result.offset!=0 || logText.str().empty())
                throw std::runtime_error("unknown projection did not force stop constraint");
        } else if(result.isVechicleInSameLane!=(name!="outside") || result.type!=StopLine::Type::Single || result.toVehicleDist!=20)
            throw std::runtime_error("valid projection behavior changed");
        calls=0;badCall=-1;leftT=2;rightT=-2;
        StopLine recovered(obstacle);
        if(!recovered.isVechicleInSameLane || recovered.type!=StopLine::Type::Single || recovered.toVehicleDist!=20)
            throw std::runtime_error("projection recovery failed");
        std::cout<<"PASS";return 0;
    } catch(const std::exception& e) { std::cerr<<e.what();return 1; }
}
