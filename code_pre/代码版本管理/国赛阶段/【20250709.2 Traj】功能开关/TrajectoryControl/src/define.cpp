#include "controller.hpp"
#include "define.h"

Timer timer;
PositionalPID steerPID(steerKp, steerKi, steerKd); // 转向 PID
AdaptiveFollowing adaptiveFollowing(followingKp, followingKi, followingKd, 45); // 跟车 PID
Logger globalLogger("Global", Logger::Color::BrightCyan); // 全局日志

float lessThanLaneFactor = 0.63; // 如果障碍物的宽度小于该因子乘以车道宽度，则主车遭遇该障碍物需要变道
float moreThanRoadFactor = 0.6; // 如果障碍物的宽度大于该因子乘以道路宽度，则主车遭遇该障碍物需要停走

float caseTargetSpeed = 0;
float caseMinSpeed = 0;
float caseMaxSpeed = 0;

float groupingDistThres = 2.f;
float stopLineDistThres = 0.f;
float laneChangeDistThres = 30.f;
float completeLaneChangeTThres = 1.5f;
float steerKp, steerKi, steerKd, steerKpUse, steerKdUse; // steerKpUse 是实际使用的 Kp，steerKp 只用来储存用户给的初值
float followingKp, followingKi, followingKd;
float steeringOffsetKp = 0.0f; // 变道的时候用来抑制转向过于剧烈
std::vector<int> caseFollowing;

uint64_t frameCount = 0;
int caseIdx = 0;

SSD::SimVector<int> validWayPoints;
SSD::SimPoint3DVector initialPath;
SSD::SimPoint3DVector targetPath;

int caseStop; // 停走场景的id
float chasingLimitDist = 10.f;

HDMapStandalone::MSignal potentialLight;
HDMapStandalone::MObject potentialCrosswalk;
SSD::SimVector<HDMapStandalone::MSignal> trafficLightList;
SSD::SimPoint3D potentialStopLinePos;
SSD::SimVector<HDMapStandalone::MObject> crosswalkList;

std::unique_ptr<SimOne_Data_WayPoints> pWayPoints = std::make_unique<SimOne_Data_WayPoints>();
std::unique_ptr<SimOne_Data_Gps> pGps = std::make_unique<SimOne_Data_Gps>();
std::unique_ptr<SimOne_Data_Obstacle> pObstacle = std::make_unique<SimOne_Data_Obstacle>();
std::unique_ptr<SimOne_Data_Control> pControl = std::make_unique<SimOne_Data_Control>();
std::unique_ptr<SimOne_Data_Signal_Lights> pLight = std::make_unique<SimOne_Data_Signal_Lights>();

namespace FlagType {
    bool isObstacleInitialized = false;
    bool isMainVehicleInitialized = false;
    bool isSimOneInitialized = false;
    bool continuousSample = false;
    bool isCrosswalkExist = false;
    bool isFollowingSample = false;
    bool isManualTrackMode = false;
}

MainVehicle mainVehicle; // 主车对象
std::vector<Obstacle> obstacleList;
std::vector<StopLine> stopLineList;

FeatureSwitcher overtakeSwitcher; // 邻域超车功能开关
FeatureSwitcher singleLaneChangeSwitcher; // 单车道变道功能开关
std::vector<ManualStopLineReservoir> manualStopLineReservoir;
std::vector<StrategyPoint> strategyPoint;
size_t ppTargetIndex = 1; // 当前纯追踪循到第几个循迹点

ESimOne_Signal_Light presetLight = ESimOne_Signal_Light::ESimOne_Signal_Light_None;