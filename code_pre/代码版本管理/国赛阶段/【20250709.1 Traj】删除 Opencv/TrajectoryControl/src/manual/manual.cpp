#include <iostream>
#include <vector>
#include "json.hpp"
#include "define.h"
#include "manual.h"

bool loadStrategyPoints(int &caseIdx, std::vector<StrategyPoint>& strategyPoint)
{
    /***********************************************************
        {
            "strategy": [
            [id, x, y, pathMode, stopTime, speed, kp],
            ...
            ]
        }

        | 索引 | 含义              | 类型   |
        |  --  | ----------------- | ------ |
        |  0   | 点 ID             | 整型   |
        |  1   | X 坐标            | 浮点型 |
        |  2   | Y 坐标            | 浮点型 |
        |  3   | pathMode 路径模式 | 整型   |
        |  4   | stopTime 停车时间 | 浮点型 |
        |  5   | speed 速度        | 浮点型 |
        |  6   | kp 控制参数       | 浮点型 |
    ***********************************************************/
    std::string filePath = SIMONE_ADAS_DIR + "TrajectoryControl/m_strategy/"+ std::to_string(caseIdx) + ".stg";
    std::ifstream file = std::ifstream(filePath);
    if (!file) return false; // 代表当前案例无对应策略点文件

    nlohmann::json jsonData;
    file >> jsonData;
    file.close();
    if (!jsonData.contains("strategy") || !jsonData["strategy"].is_array()) 
    {
        globalLogger(Logger::Color::BrightMagenta) << "◆ 策略点文件“" << filePath << "”中含有无效的策略数据格式，请检查策略点文件";
        return false;
    }

    for (const auto& arr : jsonData["strategy"])
    {
        if (arr.is_array() && arr.size() >= 6)
        {
            StrategyPoint pt;
            pt.Pos = SSD::SimPoint3D(arr[1].get<double>(), arr[2].get<double>(), 0.0);
            pt.pathMode = arr[3].get<int>();
            pt.stopTime = arr[4].get<double>();
            pt.speed = arr[5].get<double>();
            pt.kp = arr[6].get<double>();
            strategyPoint.push_back(pt);
        }
    }

    globalLogger(Logger::Color::BrightCyan) << "※ 启用策略点模式";
    return true;
}

void parseStopLines(const nlohmann::json& jsonData, std::vector<ManualStopLineReservoir>& stopLines) 
{
    if (!jsonData.count("creatStopLine") || !jsonData["creatStopLine"].is_object()) {
        std::cerr << "Invalid JSON format: Missing or incorrect 'creatStopLine' object." << std::endl;
        return;
    }
    for (nlohmann::json::const_iterator it = jsonData["creatStopLine"].begin(); it != jsonData["creatStopLine"].end(); ++it) {
        int caseIndex = std::stoi(it.key());
        if (!it.value().is_array()) continue;

        for (size_t i = 0; i < it.value().size(); ++i) {
            const nlohmann::json& line = it.value()[i];
            if (line.size() != 7) continue;

            ManualStopLineReservoir stopLine = {
                caseIndex,
                SSD::SimPoint3D(line[0], line[1], 0.0f), // srcPos (z 默认为 0)
                SSD::SimPoint3D(line[2], line[3], 0.0f), // dstPos (z 默认为 0)
                line[4],                  // command
                line[5],                  // srcFrame
                line[6]                   // dstFrame
            };
            stopLines.push_back(stopLine);
        }
    }
}

// 解析 JSON 并返回转向灯状态
ESimOne_Signal_Light parseManualLight(const nlohmann::json& jsonData) {
    // 检查 JSON 是否包含 manualLight 字段
    if (!jsonData.contains("manualLight") || !jsonData["manualLight"].is_object()) {
        return ESimOne_Signal_Light_None;
    }

    // 获取 manualLight 对象
    const auto& manualLight = jsonData["manualLight"];

    // 将 caseIdx 转换为字符串格式（JSON 键是字符串）
    std::string caseIdxStr = std::to_string(caseIdx);

    // 查找是否存在当前案例索引对应的键
    if (manualLight.contains(caseIdxStr)) {
        int lightValue = manualLight[caseIdxStr];

        // 根据值 1 / 2 返回对应的转向灯枚举
        if (lightValue == 1) {
            return ESimOne_Signal_Light_LeftBlinker;
        }
        else if (lightValue == 2) {
            return ESimOne_Signal_Light_RightBlinker;
        }
    }

    // 默认返回无转向灯
    return ESimOne_Signal_Light_None;
}