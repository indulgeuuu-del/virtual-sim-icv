#pragma once
#include <memory>
#include <vector>
#include <fstream>
#include <thread>
#include <chrono>
#include <windows.h>
#include <Lmcons.h>
#include "SimOneHDMapAPI.h"
#include "SimOnePNCAPI.h"
#include "SimOneServiceAPI.h"
#include "SimOneSensorAPI.h"
#include "SimOneEvaluationAPI.h"
#include "define.h"
#include "UtilAVP.hpp"
#include "trajectory.h"
#include "process.h"
#include "logger.hpp"
#include "cv_draw.h"
#include "json.hpp"

inline int getCaseIdx(void)
{
	int index; // 当前案例的索引
	SimOne_Data_CaseInfo pCaseInfoTest = SimOne_Data_CaseInfo();
	SimOneAPI::GetCaseInfo(&pCaseInfoTest);
	sscanf_s(pCaseInfoTest.caseName, "%d", &index);
	return index;
}

inline void initSimOne(void)
{
	SimOneAPI::InitSimOneAPI(MAIN_VEHICLE_ID, true);
	SimOneAPI::SetDriverName(MAIN_VEHICLE_ID, "autoDrive");
	SimOneAPI::SetDriveMode(MAIN_VEHICLE_ID, ESimOne_Drive_Mode_API);
	SimOneAPI::InitEvaluationServiceWithLocalData(MAIN_VEHICLE_ID);
	while (true) { // 加载 HDMap
		if (SimOneAPI::LoadHDMap(20))
		{
			globalLogger(Logger::Color::BrightMagenta) << "高精度地图加载成功";
			break;
		}
		globalLogger(Logger::Color::BrightMagenta) << "高精度地图加载中";
	}

	caseIdx = getCaseIdx(); // 获取当前案例详细信息
}

// 获得道路的路径点、起点和终点
inline void initWayPoint(const char* mainVehicleId, SimOne_Data_WayPoints* pWayPoints, SSD::SimPoint3D& src, SSD::SimPoint3D& dst)
{
	while (!SimOneAPI::GetWayPoints(mainVehicleId, pWayPoints))
	{
		globalLogger(Logger::Color::BrightMagenta) << "第 " << frameCount << " 帧正在获取路径点 ...";
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	src = SSD::SimPoint3D(pWayPoints->wayPoints[0].posX, pWayPoints->wayPoints[0].posY, 0.0);
	dst = SSD::SimPoint3D(pWayPoints->wayPoints[pWayPoints->wayPointsSize - 1].posX, pWayPoints->wayPoints[pWayPoints->wayPointsSize - 1].posY, 0.0);
}

// 获取障碍物 GPS 信息
inline bool updateObstacleData(void)
{
	if (!SimOneAPI::GetGroundTruth(MAIN_VEHICLE_ID, obstaclesPtr.get())) {
		globalLogger(Logger::Color::BrightMagenta) << "正在获取障碍物信息 ...";
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	else
	{
		if (!FlagType::isObstacleInitialized)
		{
			FlagType::isObstacleInitialized = true;
			globalLogger(Logger::Color::BrightMagenta) << "障碍物信息初始化成功";
		}
	}

	return FlagType::isObstacleInitialized;
}

// 获取主车 GPS 信息
inline bool updateVehicleData(void)
{
	// 初始化控制和车灯
	pControl->throttle = (double)5 / 3.6; // 速度
	pControl->steering = 0.0f; // 打角
	pControl->handbrake = false; // 手刹
	pControl->isManualGear = false; // 是否手动挡
	pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive; // 挡位：前进挡
	pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;

	// 获取主车 GPS 信息
	if (!SimOneAPI::GetGps(MAIN_VEHICLE_ID, gpsPtr.get())) {
		globalLogger(Logger::Color::BrightMagenta) << "正在获取主车 GPS 信息 ...";
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	else
	{
		vehiclePoint = SSD::SimPoint3D(gpsPtr->posX, gpsPtr->posY, 0.0);
		if (!FlagType::isMainVehicleInitialized)
		{
			FlagType::isMainVehicleInitialized = true;
			globalLogger(Logger::Color::BrightMagenta) << "主车 GPS 信息初始化成功";
		}
	}

	return FlagType::isMainVehicleInitialized;
}

// 如果案例停止，则退出循环并记录评分
#define recordEvaluation() \
{ \
	if (SimOneAPI::GetCaseRunStatus() == ESimOne_Case_Status::ESimOne_Case_Status_Stop) { \
		SimOneAPI::SaveEvaluationRecord(); \
		globalLogger(Logger::Color::BrightMagenta) << "案例 " << caseIdx << " 已完成，正在记录评分"; \
		return 0; \
	} \
}

// 等待 SimOne 初始化完成
#define waitInitial() \
{ \
	if (SimOneAPI::GetCaseRunStatus() == ESimOne_Case_Status::ESimOne_Case_Status_Running && FlagType::isObstacleInitialized && FlagType::isMainVehicleInitialized) { \
		if (!FlagType::isSimOneInitialized) { \
			globalLogger(Logger::Color::BrightMagenta) << "仿真平台初始化成功"; \
			FlagType::isSimOneInitialized = true; \
		} \
	} \
	else { \
		globalLogger(Logger::Color::BrightMagenta) << "仿真平台正在初始化"; \
		SimOneAPI::NextFrame(frameCount); \
		continue; \
	} \
}

inline void loadJson(void)
{
	auto getCaseSpeed = [](const nlohmann::json& js, int id, float& targetSpeed, float& minSpeed, float& maxSpeed) {
		nlohmann::json caseTargetSpeedJs = js["caseTargetSpeed"];
		nlohmann::json caseMinSpeedJs = js["caseMinSpeed"];
		nlohmann::json caseMaxSpeedJs = js["caseMaxSpeed"];

		std::map<int, float> caseTargetSpeedMap;
		std::map<int, float> caseMinSpeedMap;
		std::map<int, float> caseMaxSpeedMap;

		auto floatValJson2Map = [](const nlohmann::json& js, std::map<int, float>& map) {
			for (auto it = js.begin(); it != js.end(); ++it) {
				if (it.key() != "default") {
					map[std::stoi(it.key())] = it.value().get<float>();
				}
			}
			};

		floatValJson2Map(caseTargetSpeedJs, caseTargetSpeedMap);
		floatValJson2Map(caseMinSpeedJs, caseMinSpeedMap);
		floatValJson2Map(caseMaxSpeedJs, caseMaxSpeedMap);

		auto itTargetSpeed = caseTargetSpeedMap.find(id);
		auto itMinSpeed = caseMinSpeedMap.find(id);
		auto itMaxSpeed = caseMaxSpeedMap.find(id);

		if (itTargetSpeed != caseTargetSpeedMap.end()) targetSpeed = itTargetSpeed->second;
		else targetSpeed = caseTargetSpeedJs["default"];

		if (itMinSpeed != caseMinSpeedMap.end()) minSpeed = itMinSpeed->second;
		else minSpeed = caseMinSpeedJs["default"];

		if (itMaxSpeed != caseMaxSpeedMap.end()) maxSpeed = itMaxSpeed->second;
		else maxSpeed = caseMaxSpeedJs["default"];
		};

	nlohmann::json caseJs;
	#define JSON_LOAD_VALUE(val) val = caseJs[#val]

	std::ifstream("D:/Sim-One/SimOneAPI/ADAS/TrajectoryControl/param.json") >> caseJs;

	JSON_LOAD_VALUE(steerKp);
	JSON_LOAD_VALUE(steerKi);
	JSON_LOAD_VALUE(steerKd);

	getCaseSpeed(caseJs, caseIdx, caseTargetSpeed, caseMinSpeed, caseMaxSpeed);
	globalLogger(Logger::Color::BrightMagenta) << "当前案例编号为：" << caseIdx;
	globalLogger(Logger::Color::BrightMagenta) << "当前案例目标速度为：" << caseTargetSpeed << " m/s";
	globalLogger(Logger::Color::BrightMagenta) << "当前案例限速为：" << caseMinSpeed << " m/s < speed < " << caseMaxSpeed << " m/s";
}