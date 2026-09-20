#include "define.h"
#include "utility.h"

void getCaseSpeed(const nlohmann::json& js, int id, float& targetSpeed, float& minSpeed, float& maxSpeed)
{
    nlohmann::json casetargetSpeedJs = js["casetargetSpeed"];
    nlohmann::json caseMinSpeedJs = js["caseMinSpeed"];
    nlohmann::json caseMaxSpeedJs = js["caseMaxSpeed"];

    std::map<int, float> casetargetSpeedMap;
    std::map<int, float> caseMinSpeedMap;
    std::map<int, float> caseMaxSpeedMap;

    auto floatValJson2Map = [](const nlohmann::json& js, std::map<int, float>& map) {
        for (auto it = js.begin(); it != js.end(); ++it) {
            if (it.key() != "default") {
                map[std::stoi(it.key())] = it.value().get<float>();
            }
        }
        };

    floatValJson2Map(casetargetSpeedJs, casetargetSpeedMap);
    floatValJson2Map(caseMinSpeedJs, caseMinSpeedMap);
    floatValJson2Map(caseMaxSpeedJs, caseMaxSpeedMap);

    auto itTargetSpeed = casetargetSpeedMap.find(id);
    auto itMinSpeed = caseMinSpeedMap.find(id);
    auto itMaxSpeed = caseMaxSpeedMap.find(id);

    if (itTargetSpeed != casetargetSpeedMap.end()) targetSpeed = itTargetSpeed->second;
    else targetSpeed = casetargetSpeedJs["default"];

    if (itMinSpeed != caseMinSpeedMap.end()) minSpeed = itMinSpeed->second;
    else minSpeed = caseMinSpeedJs["default"];

    if (itMaxSpeed != caseMaxSpeedMap.end()) maxSpeed = itMaxSpeed->second;
    else maxSpeed = caseMaxSpeedJs["default"];
}

int getCaseIdx(void)
{
	int index; // 当前案例的索引
	SimOne_Data_CaseInfo pCaseInfoTest = SimOne_Data_CaseInfo();
	SimOneAPI::GetCaseInfo(&pCaseInfoTest);
	sscanf_s(pCaseInfoTest.caseName, "%d", &index);

	return index;
}

SSD::SimString m_SampleGetNearMostLane(const SSD::SimPoint3D& pos)
{
    SSD::SimString laneId;
    double s, t, s_toCenterLine, t_toCenterLine;
    if (!SimOneAPI::GetNearMostLane(pos, laneId, s, t, s_toCenterLine, t_toCenterLine))
    {
        SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Error: lane is not found.");
        return laneId;
    }
    return laneId;
}

void saveSimPoint3DVectorToTxt(const SSD::SimPoint3DVector& points, const std::string& filename) {
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    outFile << std::fixed << std::setprecision(6); // 保留6位小数，防止精度丢失

    for (const auto& point : points) {
        outFile << point.x << " " << point.y << " " << point.z << "\n";
    }

    outFile.close();
}

float calculateFps(uint64_t frameCount, float currentTimeMs)
{
    static uint64_t lastFrameCount = 0;
    static float lastTimeMs = 0.0f;

    // 至少经过一帧，且时间前进了，才能计算 FPS
    if (frameCount > lastFrameCount && currentTimeMs > lastTimeMs)
    {
        uint64_t framesPassed = frameCount - lastFrameCount;
        float timePassedSec = (currentTimeMs - lastTimeMs) / 1000.0f; // 毫秒转秒
        lastFrameCount = frameCount;
        lastTimeMs = currentTimeMs;

        return framesPassed / timePassedSec; // 每秒帧数
    }

    return 0.0f; // 无法计算 FPS
}


