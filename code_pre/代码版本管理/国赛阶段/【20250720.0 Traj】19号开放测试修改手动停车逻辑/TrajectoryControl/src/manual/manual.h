#pragma once
#include "SSD/SimPoint3D.h"
#include "Service/SimOneIOStruct.h"
#include "json.hpp"

/* 功能开关 */
class FeatureSwitcher {
public:
	bool state; // 本案例的开关状态
	std::vector<int> caseList; // 配置案例列表
	FeatureSwitcher() : state(true), caseList() {}
	void load(const nlohmann::json& jsonData, const std::string& feature, const int caseIdx);
};

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

/**
 * 解析形如 "41:(10.25,20.35)->(20.65,25.55)" 的滑移段描述串
 * @param inputStr 输入字符串，可包含多个片段
 * @return 由 (id, beginPoint, endPoint) 组成的结果向量
 */
extern std::vector<std::tuple<SSD::SimPoint3D, SSD::SimPoint3D>> parseSlide(const std::string& inputStr, int caseId);