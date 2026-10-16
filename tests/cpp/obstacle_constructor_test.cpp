// The runner prepends the boundary SDK fixture and real boundary functions.
#include <memory>
#include <algorithm>
#include <sstream>
bool operator==(const SSD::SimString& a,const SSD::SimString& b) { return a.s==b.s; }
struct SimOne_Data_Vec3f { float x=0,y=0,z=0; };
struct Prediction { SimOne_Data_Vec3f trajectory[10]; };
struct SimOne_Data_Obstacle_Entry {
    float width=2,length=4,velX=1,velY=0,velZ=0,oriZ=0,posX=0,posY=0,posZ=0;
    Prediction prediction;
};
struct GroundTruth { int obstacleSize=2; SimOne_Data_Obstacle_Entry obstacle[2]; };
auto pObstacle=std::make_unique<GroundTruth>();
struct MainVehicleFixture {
    float laneAzimuth=0,speed=3; double s=0; SSD::SimPoint3D pt; SSD::SimString laneID{"lane"};
    const char* id="0"; bool reverse=false,useDefaultPath=true;
    float steeringOffset=0;
    bool update(); // Main-vehicle update is covered by the separate input suite.
    void drive();
} mainVehicle;
namespace FlagType { bool isCrosswalkExist=false; }
struct SilentLog { std::ostringstream text; template<class T> SilentLog& operator<<(const T& v) { text<<v; return *this; } } silentLog;
#define LOG silentLog
namespace SimOneAPI {
bool GetInertialFromLaneST(const SSD::SimString&,double,double,SSD::SimPoint3D& p,SSD::SimPoint3D& dir) {
    p={}; dir={1,0,0};
    if(scenario.rfind("axes_",0)==0) {
        int heading=std::stoi(scenario.substr(scenario.find_last_of('_')+1));
        if(heading==90) dir={0,1,0};
        if(heading==180) dir={-1,0,0};
        if(heading==270) dir={0,-1,0};
    }
    return scenario!="heading_failed";
}
bool GetLaneST(const SSD::SimString&,const SSD::SimPoint3D& p,double& s,double& t) {
    s=p.x;t=0;return scenario!="projection_failed";
}
bool GetRoadST(const SSD::SimString&,const SSD::SimPoint3D&,double& s,double& t,double& z) {
    s=t=z=0;return scenario!="road_projection_failed";
}
bool GetLaneWidth(const SSD::SimString&,const SSD::SimPoint3D&,double& w) {
    w=4;return scenario!="width_failed";
}
}
namespace UtilMath {
// @SPEED@
}
// Lane comparison and crosswalk objects are fixtures; crosswalk classification is not covered.
bool isSameRoadId(const SSD::SimString&,const SSD::SimString&) { return true; }
bool isObstacleAmongCrosswalk(const SSD::SimPoint3D&,const SSD::SimPoint3D&,const SSD::SimPoint3D&,const SSD::SimPoint3D&) { return false; }
// @CLASS@
// @CORNERS@
void calculateObstacleCorners(const SimOne_Data_Obstacle_Entry& o,SSD::SimPoint3D& tl,SSD::SimPoint3D& tr,SSD::SimPoint3D& bl,SSD::SimPoint3D& br) {
    tl=calculateObstacleTL(o); tr=calculateObstacleTR(o); bl=calculateObstacleBL(o); br=calculateObstacleBR(o);
}
void getObstacleLaneSidePosition(const SimOne_Data_Obstacle_Entry& o,SSD::SimPoint3D& l,SSD::SimPoint3D& r) { getObstacleLaneSidePosition(SSD::SimPoint3D(o.posX,o.posY,o.posZ),l,r); }
void getObstacleRoadSidePosition(const SimOne_Data_Obstacle_Entry& o,SSD::SimPoint3D& l,SSD::SimPoint3D& r) { getObstacleRoadSidePosition(SSD::SimPoint3D(o.posX,o.posY,o.posZ),l,r); }
// @PROCESS@
// @AZIMUTH@
// @PREDICTION@
// @CONSTRUCTOR@
// @MERGED_CONSTRUCTOR@
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("missing case");
        scenario=argv[1];
        if(scenario.rfind("axes_",0)==0) {
            mainVehicle.laneAzimuth=std::stof(scenario.substr(5));
            pObstacle->obstacle[0].velX=pObstacle->obstacle[1].velX=0;
            pObstacle->obstacle[1].posX=2;
            Obstacle result(std::vector<size_t>{0,1});
            bool horizontal=mainVehicle.laneAzimuth==0 || mainVehicle.laneAzimuth==180;
            if(result.width!=(horizontal ? 2 : 4) || result.length!=(horizontal ? 6 : 4)
                || result.pt.x!=1 || result.pt.y!=0)
                throw std::runtime_error("merged bounds changed with obstacle lane heading");
            // All four corners must bound both source boxes in the ego-lane frame.
            for(size_t i=0;i<2;++i) {
                auto a=calculateObstacleTL(pObstacle->obstacle[i]);
                auto b=calculateObstacleBR(pObstacle->obstacle[i]);
                for(auto p:{a,b})
                    if(p.x<std::min(result.tl.x,result.br.x) || p.x>std::max(result.tl.x,result.br.x)
                        || p.y<std::min(result.tl.y,result.br.y) || p.y>std::max(result.tl.y,result.br.y))
                        throw std::runtime_error("merged bounds exclude source corner");
            }
            std::cout<<"PASS";return 0;
        }
        if(scenario.rfind("merged_",0)==0) {
            scenario=scenario.substr(7);
            std::vector<size_t> indices={0,1};
            pObstacle->obstacle[0].velX=pObstacle->obstacle[1].velX=0;
            pObstacle->obstacle[1].posX=2;
            if(scenario=="single") indices={0};
            if(scenario=="empty") indices.clear();
            bool rejected=false;
            try {
                Obstacle result(indices);
                if(scenario=="valid" || scenario=="single") {
                    const bool single=scenario=="single";
                    if(result.type!=Obstacle::Type::Static || result.width!=2 || result.length!=(single ? 4 : 6)
                        || result.pt.x!=(single ? 0 : 1) || result.velocity!=0 || result.laneWidth!=4 || result.roadWidth!=(single ? 4 : 12)
                        || result.predictionGM.size()!=(single ? 50 : 0))
                        throw std::runtime_error("wrong merged constructor fields");
                }
            } catch(const std::domain_error&) { rejected=true; }
            bool valid=scenario=="valid" || scenario=="single";
            if(rejected==valid) throw std::runtime_error("merged construction accepted failure or rejected valid data");
            scenario="valid";
            Obstacle recovered(std::vector<size_t>{0,1});
            if(recovered.length!=6) throw std::runtime_error("merged recovery failed");
            std::cout<<"PASS"; return 0;
        }
        SimOne_Data_Obstacle_Entry input;
        if(scenario=="static") input.velX=0;
        if(scenario=="vertical") { input.velX=0;input.velY=1; }
        for(int i=0;i<10;++i) input.prediction.trajectory[i]={float(i),0,0};
        if(scenario=="prediction_nan") input.prediction.trajectory[3].x=std::nanf("");
        if(scenario=="prediction_inf") input.prediction.trajectory[3].y=std::numeric_limits<float>::infinity();
        if(scenario=="prediction_growth") for(int i=0;i<10;++i) input.prediction.trajectory[i].x=std::pow(100.0f,i);
        bool rejected=false;
        try {
            Obstacle result(input);
            if(scenario=="valid" || scenario=="static" || scenario=="vertical") {
                const auto expected=scenario=="static" ? Obstacle::Type::Static : scenario=="vertical" ? Obstacle::Type::Vertical : Obstacle::Type::Horizental;
                if(result.type!=expected || result.laneWidth!=4 || result.roadWidth!=12 || result.predictionGM.size()!=50)
                    throw std::runtime_error("wrong complete constructor result");
            }
        } catch(const std::domain_error&) { rejected=true; }
        bool valid=scenario=="valid" || scenario=="static" || scenario=="vertical";
        if(rejected==valid) throw std::runtime_error("constructor accepted bad data or rejected valid data");
        scenario="valid"; for(int i=0;i<10;++i) input.prediction.trajectory[i]={float(i),0,0};
        Obstacle recovered(input);
        if(recovered.laneWidth!=4) throw std::runtime_error("recovery failed");
        std::cout<<"PASS";return 0;
    } catch(const std::exception& e) { std::cerr<<e.what();return 1; }
}
