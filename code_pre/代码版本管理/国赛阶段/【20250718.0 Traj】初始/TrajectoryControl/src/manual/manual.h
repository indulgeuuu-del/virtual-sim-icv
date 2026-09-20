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

// 函数说明：解析形如"41:100->200, 42:200->300"的字符串，提取编号、起始时间、终止时间
// 参数说明：inputStr 是要解析的输入字符串
// 返回值：返回一个 vector，包含每个消息的编号、起始时间、终止时间组成的 tuple
std::vector<std::tuple<int, int, int>> parseSlide(const std::string& inputStr);