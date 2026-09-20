#pragma once
#include "SSD/SimPoint3D.h"
#include "Service/SimOneIOStruct.h"
#include "json.hpp"

class StrategyPoint {
public:
    SSD::SimPoint3D Pos;
    int pathMode;
    double stopTime;
    double speed;
    double kp;
};

class ManualStopLineReservoir {
public:
	int caseIndex;
	SSD::SimPoint3D srcPos;
	SSD::SimPoint3D dstPos;

	int command;
	int srcFrame;
	int dstFrame;
};

bool loadStrategyPoints(int &caseIdx, std::vector<StrategyPoint>& strategyPoint);

extern void parseStopLines(const nlohmann::json& jsonData, std::vector<ManualStopLineReservoir>& stopLines);

// 解析 JSON 并返回转向灯状态
extern ESimOne_Signal_Light parseManualLight(const nlohmann::json& jsonData);