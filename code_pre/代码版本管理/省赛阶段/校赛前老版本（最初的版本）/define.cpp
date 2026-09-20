#include "define.h"

// 全局变量定义
Logger globalLogger("Global", Logger::Color::BrightCyan); // 全局日志
float caseTargetSpeed = 0;
float caseMinSpeed = 0;
float caseMaxSpeed = 0;
std::vector<int> caseNormalID;
std::vector<int> caseFollowingID;

float groupingDistThres = 2.f;
float stopLineDistThres = 0.f;
float laneChangeSteeringFactor = 1.f;
float laneChangeSteeringMax = 0.1f;
float perceptionRange = 50.0f;
float speedKp, speedKi, speedKd;
float steerKp, steerKi, steerKd;
float followingKp, followingKi, followingKd;

uint64_t frameCount = 0;
int caseIdx = 0;
int continuousCase = 0;

SSD::SimPoint3DVector initialPath;
SSD::SimPoint3DVector lanechangePath;
SSD::SimPoint3DVector targetPath;
SSD::SimVector<long> allRoadIdList;

std::string mainVehicleName;
double sMainVehicle = 0.0;
double tMainVehicle = 0.0;

float chasingLimitDist = 10.f;

HDMapStandalone::MLaneLink laneLink;
HDMapStandalone::MSignal potentialLight;
HDMapStandalone::MObject potentialCrosswalk;
SSD::SimVector<HDMapStandalone::MSignal> trafficLightList;
SSD::SimPoint3D potentialStopLinePos;
bool flag_haveCrossWalk=false;//是否存在斑马线
SSD::SimVector<HDMapStandalone::MObject> potentialcrosswalkList;
std::unique_ptr<SimOne_Data_WayPoints> pWayPoints = std::make_unique<SimOne_Data_WayPoints>();
std::unique_ptr<SimOne_Data_Gps> pGps = std::make_unique<SimOne_Data_Gps>();
std::unique_ptr<SimOne_Data_Obstacle> pObstacle = std::make_unique<SimOne_Data_Obstacle>();
std::unique_ptr<SimOne_Data_Control> pControl = std::make_unique<SimOne_Data_Control>();
std::unique_ptr<SimOne_Data_Signal_Lights> pLight = std::make_unique<SimOne_Data_Signal_Lights>();
std::vector<ManualStopLineReservoir> manualStopLineReservoir;

std::vector<StopLine> stopLineList;
double cumulative_singel=0.0;
double cumulative_double=0.0;
namespace FlagType {
    uint8_t isSimOneInitialized = 0;
    uint8_t continuousSample = 0;
}