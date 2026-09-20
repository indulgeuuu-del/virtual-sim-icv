#pragma once
#include <vector>
#include <string>
#include <memory>
#include "SSD/SimPoint3D.h"
#include "stopline.h"
#include "logger.hpp"
#include "json.hpp"
#include "obstacle.h"
#include "utility.h"

// 常量和宏定义
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN_MAX(input, low, upper) MIN(MAX(input, low), upper)
#define JSON_LOAD_VALUE(val) val = caseJs[#val]
#define LOG globalLogger(Logger::Color::BrightYellow)



// 全局变量声明
extern Logger globalLogger; // 全局日志
extern float caseTargetSpeed;
extern float caseMinSpeed;
extern float caseMaxSpeed;
extern std::vector<int> caseNormalID;
extern std::vector<int> caseFollowingID;
extern float groupingDistThres;
extern float stopLineDistThres;
extern float laneChangeSteeringFactor;
extern float laneChangeSteeringMax;
extern float perceptionRange;
extern float speedKp, speedKi, speedKd;
extern float steerKp, steerKi, steerKd;
extern float followingKp, followingKi, followingKd;

extern uint64_t frameCount;
extern int caseIdx;
extern int continuousCase; // 连续场景

extern SSD::SimPoint3DVector initialPath;
extern SSD::SimPoint3DVector targetPath;
extern SSD::SimVector<long> allRoadIdList;
extern SSD::SimPoint3DVector lanechangePath;
extern const char* MainVehicleId;
extern std::string mainVehicleName;
extern SSD::SimPoint3D mainVehiclePos;
extern SSD::SimString lastMainVehicleLaneId;
extern SSD::SimString mainVehicleLaneId;
extern double sMainVehicle;
extern double tMainVehicle;
extern double mainVehicleSpeed;
extern double mainVehicleSpeedXY;
extern float mainVehicleTargetSpeed;

extern float baseSpeed;
extern float chasingLimitDist;
extern bool flag_haveCrossWalk ;
extern SSD::SimVector<HDMapStandalone::MObject> potentialcrosswalkList;
extern HDMapStandalone::MLaneLink laneLink;
extern HDMapStandalone::MSignal potentialLight;
extern HDMapStandalone::MObject potentialCrosswalk;
extern SSD::SimVector<HDMapStandalone::MSignal> trafficLightList;
extern SSD::SimPoint3D potentialStopLinePos;
extern std::unique_ptr<SimOne_Data_WayPoints> pWayPoints;
extern std::unique_ptr<SimOne_Data_Gps> pGps;
extern std::unique_ptr<SimOne_Data_Obstacle> pObstacle;
extern std::unique_ptr<SimOne_Data_Control> pControl;
extern std::unique_ptr<SimOne_Data_Signal_Lights> pLight;
extern std::vector<StopLine> stopLineList;
extern std::vector<ManualStopLineReservoir> manualStopLineReservoir;

namespace FlagType {
    extern uint8_t isSimOneInitialized;
    extern uint8_t continuousSample;
    extern uint8_t leftLaneExist;
    extern uint8_t rightLaneExist;
    extern uint8_t useDefaultPath;
    extern uint8_t IsTwosideRoad;
}
extern  double cumulative_singel;
extern  double cumulative_double;