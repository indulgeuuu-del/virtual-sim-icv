#include "define.h"
#include "process.h"
#include "trajectory.h"

Timer timer; // 定时器
Logger globalLogger("Global", Logger::Color::BrightCyan); // 全局日志

std::unique_ptr<SimOne_Data_Gps> gpsPtr = std::make_unique<SimOne_Data_Gps>();
std::unique_ptr<SimOne_Data_Obstacle> obstaclesPtr = std::make_unique<SimOne_Data_Obstacle>();
std::unique_ptr<SimOne_Data_WayPoints> wayPointsPtr = std::make_unique<SimOne_Data_WayPoints>();
std::unique_ptr<SimOne_Data_Control> pControl = std::make_unique<SimOne_Data_Control>();

SSD::SimPoint3D startPoint, endPoint, vehiclePoint;
SSD::SimVector<HDMapStandalone::MParkingSpace> parkingSpaces;
HDMapStandalone::MParkingSpace targetParkingSpace;
ParkingSlot targetParkingSlot;

Trajectory trajectoryForward;
Trajectory trajectoryReverse;
Trajectory trajectoryLeaving;

int caseIdx;
int frameCount = 0;

namespace FlagType {
    bool isObstacleInitialized = false;
    bool isMainVehicleInitialized = false;
    bool isSimOneInitialized = false;
}

/* 关键参数 */
AVPStatus status = AVPStatus::eForwarding;

bool brakingPrepStarted = false;
bool waitingStarted = false;

float brakingPrepParkingTime = 1000.0f;
float waitingParkingTime = 10000.0f;

double headingErrorThresholdDegree = 0.5;

/* JSON 变量 */
float steerKp, steerKi, steerKd;
float caseTargetSpeed = 0;
float caseMinSpeed = 0;
float caseMaxSpeed = 0;