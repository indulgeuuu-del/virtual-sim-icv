#include "json.hpp"
#include "SimOneServiceAPI.h"
#include "utility.h"
#include "stopline.h"
#include "logger.hpp"
#include "define.h"

void intValJson2Map(const nlohmann::json& js, std::map<int, int>& map)
{
	for (auto it = js.begin(); it != js.end(); ++it) {
		if (it.key() == "default") continue; // 跳过 default 所对应的键值对
		map[std::stoi(it.key())] = it.value();
	}
}

void floatValJson2Map(const nlohmann::json& js, std::map<int, float>& map)
{
    for (auto it = js.begin(); it != js.end(); ++it) {
        if (it.key() == "default") continue; // 跳过 default 所对应的键值对
        map[std::stoi(it.key())] = it.value().get<float>();
    }
}

void getCaseSpeed(const nlohmann::json& js, int id, float& targetSpeed, float& minSpeed, float& maxSpeed)
{
    nlohmann::json casetargetSpeedJs = js["casetargetSpeed"];
    nlohmann::json caseMinSpeedJs = js["caseMinSpeed"];
    nlohmann::json caseMaxSpeedJs = js["caseMaxSpeed"];

    std::map<int, float> casetargetSpeedMap;
    std::map<int, float> caseMinSpeedMap;
    std::map<int, float> caseMaxSpeedMap;

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
	/* std::cout << "case name " << pCaseInfoTest.caseName << "\n"; */
	sscanf_s(pCaseInfoTest.caseName, "%d", &index);
	return index;
}

void parseCreatStopLine(const nlohmann::json& jsonData, std::vector<ManualStopLineReservoir>& reservoir)
{
    reservoir.clear();
    const auto& stopLines = jsonData["creatStopLine"];

    for (nlohmann::json::const_iterator it = stopLines.begin(); it != stopLines.end(); ++it)
    {
        int caseId = std::stoi(it.key()); // 车道 ID
        const auto& value = it.value();

        if (!value.is_array()) continue;

        for (size_t i = 0; i < value.size(); ++i)
        {
            const auto& entry = value[i];

            if (!entry.is_array() || entry.size() != 5) continue;

            SSD::SimPoint3D start = { entry[0].get<float>(), entry[1].get<float>(), 0.0f };
            SSD::SimPoint3D end = { entry[2].get<float>(), entry[3].get<float>(), 0.0f };
            int command = entry[4].get<int>();

            globalLogger(Logger::Color::BrightBlue) << "case: " << caseId << "Start(" << start.x << ", " << start.y << "), "
                << "End(" << end.x << ", " << end.y << "), "
                << "Command: " << command;

            reservoir.push_back(ManualStopLineReservoir(caseId, command, start, end));
        }
    }
}