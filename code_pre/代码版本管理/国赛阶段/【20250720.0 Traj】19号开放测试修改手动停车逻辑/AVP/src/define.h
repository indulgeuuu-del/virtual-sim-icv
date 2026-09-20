#pragma once
#include <memory>
#include <cstdlib>
#include "SimOneHDMapAPI.h"
#include "timer.h"
#include "logger.hpp"

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            globalLogger(Logger::Color::BrightMagenta) << "Assertion failed: (" #condition ")\n" << "Message: " << message << "\n" << "Location: " << __FILE__ << ":" << __LINE__ << " (" << __FUNCTION__ << ")"; \
            std::abort(); \
        } \
    } while (false)

#define MAIN_VEHICLE_WIDTH (2.21f)
#define MAIN_VEHICLE_LENGTH (4.57f)

constexpr double WHEELBASE = 2.9187;           // 轴距 291.87cm -> 2.9187m
constexpr double CG_TO_FRONT_AXLE = 1.3433;    // 质心到前轴距离 134.33cm -> 1.3433m
constexpr double FRONT_AXLE_TO_HEAD = 1.0;     // 前轴到车头距离 100cm -> 1.0m
constexpr double REAR_AXLE_TO_TAIL = 0.88;     // 后轴到车尾距离 88cm -> 0.88m

constexpr double Lf = CG_TO_FRONT_AXLE + FRONT_AXLE_TO_HEAD;  // 质心到车头（前悬）
constexpr double Lr = REAR_AXLE_TO_TAIL + (WHEELBASE - CG_TO_FRONT_AXLE); // 质心到车尾（后悬）

#define MAIN_VEHICLE_ID ("0")
#define LOG globalLogger(Logger::Color::BrightYellow)

class Trajectory;
class Logger;
class ParkingSlot;

enum class AVPStatus
{
	eForwarding,	// 前进接近目标车位
	eBrakingPrep,	// 刹车准备，停下等待
	eReversing,		// 倒车进入车位
	eWaiting,		// 驻车等待
	eLeaving		// 驶离车位
};

extern Timer timer; // 定时器
extern Logger globalLogger;

extern std::unique_ptr<SimOne_Data_Gps> gpsPtr;
extern std::unique_ptr<SimOne_Data_Obstacle> obstaclesPtr;
extern std::unique_ptr<SimOne_Data_WayPoints> wayPointsPtr;
extern std::unique_ptr<SimOne_Data_Control> pControl;

extern SSD::SimPoint3D startPoint, endPoint, vehiclePoint;
extern SSD::SimVector<HDMapStandalone::MParkingSpace> parkingSpaces;
extern HDMapStandalone::MParkingSpace targetParkingSpace;
extern ParkingSlot targetParkingSlot;

extern Trajectory trajectoryForward;
extern Trajectory trajectoryReverse;
extern Trajectory trajectoryLeaving;

extern int caseIdx;
extern int frameCount;

namespace FlagType {
    extern bool isObstacleInitialized;
    extern bool isMainVehicleInitialized;
	extern bool isSimOneInitialized;
}

/* 关键参数 */
extern AVPStatus status;

extern bool brakingPrepStarted;
extern bool waitingStarted;

extern float brakingPrepParkingTime;
extern float waitingParkingTime;

extern double headingErrorThresholdDegree;

/* JSON 变量 */
extern float steerKp, steerKi, steerKd;
extern float caseTargetSpeed;
extern float caseMinSpeed;
extern float caseMaxSpeed;