#pragma once
#include <iostream>
#include <fstream>
#include <windows.h>
#include <Lmcons.h>
#include <chrono>
#include <cmath>
#include "SimOneServiceAPI.h"
#include "SimOneSensorAPI.h"
#include "SimOneHDMapAPI.h"
#include "SimOneEvaluationAPI.h"
#include "UtilDriver.h"
#include "UtilMath.h"
#include "draw.h"
#include "drawOpendrive.h"
#include "controller.hpp"
#include "json.hpp"
#include "logger.hpp"
#include "utility.h"
#include "manual.h"
#include "define.h"
#include "navigation.h"

// 获取障碍物 GPS 信息
inline bool updateObstacleData(void)
{
	if (!SimOneAPI::GetGroundTruth(mainVehicle.id, pObstacle.get())) {
		//globalLogger(Logger::Color::BrightMagenta) << "获取障碍物信息失败";
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
	if (SimOneAPI::GetCaseRunStatus() == ESimOne_Case_Status::ESimOne_Case_Status_Running && ((FlagType::isObstacleInitialized && FlagType::isMainVehicleInitialized) || frameCount > 5)) { \
		if (!FlagType::isSimOneInitialized) { \
			globalLogger(Logger::Color::BrightMagenta) << "仿真平台初始化成功"; \
			FlagType::isSimOneInitialized = true; \
		} \
	} \
	else { \
		globalLogger(Logger::Color::BrightMagenta) << "仿真平台正在初始化"; \
		SimOneAPI::NextFrame(frame); \
		continue; \
	} \
}

// 加载 JSON 文件
inline void loadJson(void)
{
	nlohmann::json caseJs;

	auto username = getCurrentUsername();
	if (username == "crb") std::ifstream("E:/Sim-One/SimOneAPI/ADAS/TrajectoryControl/param.json") >> caseJs;
	else std::ifstream("D:/Sim-One/SimOneAPI/ADAS/TrajectoryControl/param.json") >> caseJs;
	LOG << "欢迎您，当前用户：" << username;

	JSON_LOAD_VALUE(groupingDistThres);
	JSON_LOAD_VALUE(stopLineDistThres);
	JSON_LOAD_VALUE(laneChangeDistThres);
	JSON_LOAD_VALUE(completeLaneChangeTThres);
	JSON_LOAD_VALUE(chasingLimitDist);
	JSON_LOAD_VALUE(steerKp);
	JSON_LOAD_VALUE(steerKi);
	JSON_LOAD_VALUE(steerKd);
	JSON_LOAD_VALUE(followingKp);
	JSON_LOAD_VALUE(followingKi);
	JSON_LOAD_VALUE(followingKd);
	JSON_LOAD_VALUE(steeringOffsetKp);
	JSON_LOAD_VALUE(continuousCase);
	JSON_LOAD_VALUE(case_stop_followID);

	caseFollowingID = caseJs["caseFollowingID"].get<std::vector<int>>(); // 哪些场景是需要稳定跟车的

	parseStopLines(caseJs, manualStopLineReservoir);
	parseManualTrack(caseJs, manualTrackReservoirMap);
	presetLight = parseManualLight(caseJs);
	parseManualPreciseTrack(caseJs, manualPreciseTrackReservoir);

	getCaseSpeed(caseJs, caseIdx, caseTargetSpeed, caseMinSpeed, caseMaxSpeed);
	globalLogger(Logger::Color::BrightMagenta) << "当前案例编号为：" << caseIdx;
	globalLogger(Logger::Color::BrightMagenta) << "当前案例目标速度为：" << caseTargetSpeed << " m/s";
	globalLogger(Logger::Color::BrightMagenta) << "当前案例限速为：" << caseMinSpeed << " m/s < speed < " << caseMaxSpeed << " m/s";
}

inline void initSimOne(void)
{
	SimOneAPI::InitSimOneAPI(mainVehicle.id, true);
	SimOneAPI::SetDriverName(mainVehicle.id, "autoDrive");
	SimOneAPI::SetDriveMode(mainVehicle.id, ESimOne_Drive_Mode_API);
	SimOneAPI::InitEvaluationServiceWithLocalData("0");

	while (true) { // 加载 HDMap
		if (SimOneAPI::LoadHDMap(20)) {
			globalLogger(Logger::Color::BrightMagenta) << "高精度地图加载成功";
			break;
		}
		globalLogger(Logger::Color::BrightMagenta) << "高精度地图加载中";
	}
}

inline bool initNavigation(SSD::SimVector<int>& indexOfValidPoints, SSD::SimPoint3DVector& targetPath)
{
	// SimOneAPI 获取路径点（根据起点和终点规划出一条最短路径）
	if (SimOneAPI::GetWayPoints(mainVehicle.id, pWayPoints.get()))
	{
		for (size_t i = 0; i < pWayPoints->wayPointsSize; ++i) {
			SSD::SimPoint3D inputWayPoints(pWayPoints->wayPoints[i].posX, pWayPoints->wayPoints[i].posY, 0);
			initialPath.push_back(inputWayPoints);
		}
	}
	else {
		globalLogger(Logger::Color::BrightMagenta) << "获取主车预设路径的起点和终点失败";
		return -1;
	}

	// 根据路径点数量分情况处理
	if (pWayPoints->wayPointsSize >= 2) { // 大于等于 2 个路径点，调用 GenerateRoute 生成目标路径
		if (!SimOneAPI::GenerateRoute(initialPath, indexOfValidPoints, targetPath)) {
			globalLogger(Logger::Color::BrightMagenta) << "使用 A* 生成主车路径规划失败";
			return -1;
		}
	}
	else if (pWayPoints->wayPointsSize == 1) { // 只有 1 个路径点，获取该路径点所在的车道 ID 和车道信息
		// m_SampleGetNearMostLane 用于获取某个坐标所在的车道信息
		SSD::SimString laneIdInit = m_SampleGetNearMostLane(initialPath[0]);
		HDMapStandalone::MLaneInfo laneInfoInit;
		if (SimOneAPI::GetLaneSample(laneIdInit, laneInfoInit)) { // 如果获取车道信息成功
			targetPath = laneInfoInit.centerLine; // 设置中心线为目标路径
		}
		else { // 如果获取车道信息失败 
			globalLogger(Logger::Color::BrightMagenta) << "使用 A* 生成主车路径规划失败";
			return -1;
		}
	}
	else { // 如果没有路径点，则报错
		globalLogger(Logger::Color::BrightMagenta) << "主车的路径点数量为 0";
		return -1;
	}
}