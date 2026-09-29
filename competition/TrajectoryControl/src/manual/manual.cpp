#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <regex>
#include <vector>
#include <tuple>
#include "json.hpp"
#include "define.h"
#include "utility.h"
#include "manual.h"

/* 策略点每行固定 7 列，列名用于报错定位 */
static const char* const STRATEGY_COLUMNS[] = { "id", "x", "y", "pathMode", "stopTime", "speed", "kp" };

/* 取整型列：必须是 JSON 整数（1.5、"1"、null 都拒绝），且在 int 范围内 */
static bool readStrategyInt(const nlohmann::json& value, int& out)
{
    if (!value.is_number_integer()) return false;
    if (value.is_number_unsigned())
    {
        if (value.get<unsigned long long>() > static_cast<unsigned long long>(std::numeric_limits<int>::max())) return false;
    }
    else
    {
        long long v = value.get<long long>();
        if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) return false;
    }
    out = value.get<int>();
    return true;
}

/* 取数值列：必须是有限数字（字符串、布尔、null 都拒绝） */
static bool readStrategyNumber(const nlohmann::json& value, double& out)
{
    if (!value.is_number()) return false;
    out = value.get<double>();
    return std::isfinite(out);
}

/* 校验整个 strategy 数组；任一行不合法则返回 false，error 说明第几行、第几列 */
static bool parseStrategyRows(const nlohmann::json& jsonData, std::vector<StrategyPoint>& points, std::string& error)
{
    if (!jsonData.is_object() || !jsonData.contains("strategy") || !jsonData.at("strategy").is_array())
    {
        error = "缺少 strategy 数组";
        return false;
    }
    const nlohmann::json& rows = jsonData.at("strategy");
    if (rows.empty())
    {
        error = "strategy 数组为空";
        return false;
    }

    for (size_t i = 0; i < rows.size(); ++i)
    {
        const nlohmann::json& arr = rows.at(i);
        std::string where = "strategy[" + std::to_string(i) + "]";
        if (!arr.is_array() || arr.size() != 7)
        {
            error = where + " 应为恰好 7 列的数组 [id, x, y, pathMode, stopTime, speed, kp]";
            return false;
        }
        auto columnError = [&](size_t col, const std::string& expect) {
            error = where + " 第 " + std::to_string(col + 1) + " 列 " + STRATEGY_COLUMNS[col] + " " + expect;
            return false;
        };

        int id = 0;
        if (!readStrategyInt(arr.at(0), id) || id != static_cast<int>(i)) return columnError(0, "应为整数且等于行号 " + std::to_string(i));

        StrategyPoint pt;
        double x = 0.0, y = 0.0;
        if (!readStrategyNumber(arr.at(1), x)) return columnError(1, "应为有限数字");
        if (!readStrategyNumber(arr.at(2), y)) return columnError(2, "应为有限数字");
        pt.Pos = SSD::SimPoint3D(x, y, 0.0);

        /* -1：SDK 路径规划（GenerateRoute）；1：两点等距插值（equidistantSampling） */
        if (!readStrategyInt(arr.at(3), pt.pathMode) || (pt.pathMode != -1 && pt.pathMode != 1)) return columnError(3, "应为整数 -1 或 1");

        /* stopTime / speed / kp：-1 表示使用默认值，否则必须大于 0 */
        double* targets[] = { &pt.stopTime, &pt.speed, &pt.kp };
        for (size_t col = 4; col < 7; ++col)
        {
            double& v = *targets[col - 4];
            if (!readStrategyNumber(arr.at(col), v) || (v != -1.0 && v <= 0.0)) return columnError(col, "应为 -1（默认）或大于 0 的数");
        }
        points.push_back(pt);
    }
    return true;
}

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

    /* 文件存在但内容无效时整份拒绝：不启用策略点模式，并说明第几行、第几列出错 */
    auto reject = [&filePath](const std::string& reason) {
        globalLogger(Logger::Color::BrightMagenta) << "◆ 策略点文件“" << filePath << "”无效，未启用策略点模式：" << reason;
        return false;
    };

    nlohmann::json jsonData;
    try
    {
        jsonData = nlohmann::json::parse(file);
    }
    catch (const nlohmann::json::exception& e)
    {
        return reject(std::string("JSON 格式错误，") + e.what());
    }
    file.close();

    std::vector<StrategyPoint> parsed;
    std::string error;
    if (!parseStrategyRows(jsonData, parsed, error)) return reject(error);

    strategyPoint = std::move(parsed); // 全部校验通过后才替换，失败时不留下半份数据
    globalLogger(Logger::Color::BrightCyan) << "※ 启用策略点模式，共 " << strategyPoint.size() << " 个策略点";
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

void FeatureSwitcher::load(const nlohmann::json& jsonData, const std::string& feature, const int caseIdx)
{
    caseList = jsonData[feature].get<std::vector<int>>();
    bool stateConfig = caseList[0] == 1 ? true : false;

    std::vector<int> caseItems;
    for (size_t i = 1, ie = caseList.size(); i < ie; ++i) caseItems.push_back(caseList[i]);

    if (IS_IN(caseIdx, caseItems)) state = stateConfig;
    else state = !stateConfig;
}

std::vector<std::tuple<SSD::SimPoint3D, SSD::SimPoint3D>> parseSlide(const std::string& inputStr, int caseId)
{
    // 1:id  2:x1  3:y1  4:x2  5:y2
    std::regex pattern(R"((\d+):\(([+-]?\d*\.?\d+),([+-]?\d*\.?\d+)\)->\(([+-]?\d*\.?\d+),([+-]?\d*\.?\d+)\))");
    std::vector<std::tuple<SSD::SimPoint3D, SSD::SimPoint3D>> results;
    for (auto it = std::sregex_iterator(inputStr.begin(), inputStr.end(), pattern); it != std::sregex_iterator(); ++it)
    {
        const std::smatch& match = *it;
        if (caseId != std::stoi(match[1])) continue; // 只筛选出跟当前案例相关的溜车规则
        float x1 = std::stof(match[2]);
        float y1 = std::stof(match[3]);
        float x2 = std::stof(match[4]);
        float y2 = std::stof(match[5]);
        results.emplace_back(SSD::SimPoint3D{ x1, y1, 0.0 }, SSD::SimPoint3D{ x2, y2, 0.0 });
    }
    return results;
}