#pragma once
#include <vector>
#include <string>
#include <memory>
#include "SSD/SimPoint3D.h"
#include "timer.h"
#include "stopline.h"
#include "vehicle.h"
#include "logger.hpp"
#include "json.hpp"
#include "obstacle.h"
#include "manual.h"
#include "utility.h"

/* SIMONE ADAS 工作目录，更换设备请务必更改 */
#define SIMONE_ADAS_DIR std::string("D:/Sim-One/SimOneAPI/ADAS/")

class MainVehicle;
class PositionalPID;
class AdaptiveFollowing;

// 常量和宏定义
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN_MAX(input, low, upper) MIN(MAX(input, low), upper)
#define JSON_LOAD_VALUE(val) val = caseJs[#val]
#define LOG globalLogger(Logger::Color::BrightYellow)

extern Timer timer;
extern PositionalPID steerPID; // 转向 PID
extern AdaptiveFollowing adaptiveFollowing;
extern Logger globalLogger; // 全局日志

extern float lessThanLaneFactor; // 如果障碍物的宽度小于该因子乘以车道宽度，则主车遭遇该障碍物需要变道
extern float moreThanRoadFactor; // 如果障碍物的宽度大于该因子乘以道路宽度，则主车遭遇该障碍物需要停走

extern float caseTargetSpeed;
extern float caseMinSpeed;
extern float caseMaxSpeed;

extern float groupingDistThres;
extern float stopLineDistThres;
extern float laneChangeDistThres;
extern float completeLaneChangeTThres;
extern float achieveThres;
extern float steerKp, steerKi, steerKd, steerKpUse, steerKdUse; // steerKpUse 是实际使用的 Kp，steerKp 只用来储存用户给的初值
extern float followingKp, followingKi, followingKd;
extern float steeringOffsetKp; // 变道的时候用来抑制转向过于剧烈
extern std::vector<int> caseFollowing;

extern uint64_t frameCount;
extern int caseIdx;

extern SSD::SimVector<int> validWayPoints;
extern SSD::SimPoint3DVector initialPath;
extern SSD::SimPoint3DVector targetPath;

extern int caseStop;
extern float chasingLimitDist;

extern HDMapStandalone::MSignal potentialLight;
extern HDMapStandalone::MObject potentialCrosswalk;
extern SSD::SimVector<HDMapStandalone::MSignal> trafficLightList;
extern SSD::SimPoint3D potentialStopLinePos;
extern SSD::SimVector<HDMapStandalone::MObject> crosswalkList;

extern std::unique_ptr<SimOne_Data_WayPoints> pWayPoints;
extern std::unique_ptr<SimOne_Data_Gps> pGps;
extern std::unique_ptr<SimOne_Data_Obstacle> pObstacle;
extern std::unique_ptr<SimOne_Data_Control> pControl;
extern std::unique_ptr<SimOne_Data_Signal_Lights> pLight;

namespace FlagType {
    extern bool isObstacleInitialized;
    extern bool isMainVehicleInitialized;
    extern bool isSimOneInitialized;
    extern bool continuousSample;
    extern bool isCrosswalkExist;
    extern bool isFollowingSample;
    extern bool isManualTrackMode;
}

extern MainVehicle mainVehicle; // 主车对象
extern std::vector<Obstacle> obstacleList;
extern std::vector<StopLine> stopLineList;

extern FeatureSwitcher overtakeSwitcher; // 邻域超车功能开关
extern FeatureSwitcher singleLaneChangeSwitcher; // 单车道变道功能开关
extern FeatureSwitcher mobileSingleStoplineSwitcher; // 移动半停止线变道功能开关
extern std::vector<ManualStopLineReservoir> manualStopLineReservoir;
extern std::vector<StrategyPoint> strategyPoint; // 策略点
extern std::vector<std::tuple<int, int, int>> slideConfig; // 溜车配置
extern ESimOne_Signal_Light presetLight;

extern size_t ppTargetIndex; // 当前纯追踪循到第几个循迹点