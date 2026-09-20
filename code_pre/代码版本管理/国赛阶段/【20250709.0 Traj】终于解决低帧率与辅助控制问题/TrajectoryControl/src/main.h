#pragma once
#include <iostream>
#include <fstream>
#include <windows.h>
#include <Lmcons.h>
#include <chrono>
#include <cmath>
#include <unordered_set>
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
#include "RuleSet.h"
#include "LaneChange.h"
#include "prediction.h"

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
		SimOneAPI::NextFrame(frame); \
		continue; \
	} \
}

extern void initSimOne(void);

// 加载 JSON 文件
extern void loadJson(void);

extern bool initNavigation(SSD::SimVector<int>& validWayPoints, SSD::SimPoint3DVector& targetPath);

// 获取障碍物 GPS 信息
extern bool updateObstacleData(int timeoutFrames = 6);