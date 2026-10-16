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
template<class T> using SimVector=std::vector<T>;
}
namespace HDMapStandalone { struct MSignal {}; struct MObject { std::vector<SSD::SimPoint3D> boundaryKnots; }; }
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
void GetSpecifiedLaneStoplineList(int,SSD::SimVector<HDMapStandalone::MObject>&) { throw std::runtime_error("crosswalk outside scope"); }
}
double StopLine::calculateVehicleDistance(const SSD::SimPoint3D&) const { return 20; }
bool StopLine::isStopLineBehindVehicle(const SSD::SimPoint3D&,const SSD::SimPoint3D&,const SSD::SimPoint3D&,const SSD::SimPoint3D&,const SSD::SimPoint3D&) { return false; }
// @CONSTRUCTOR@
// @PROJECTION@
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("missing case");
        std::string name=argv[1];Obstacle obstacle;
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
