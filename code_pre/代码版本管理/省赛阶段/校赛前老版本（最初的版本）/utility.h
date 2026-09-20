#pragma once
#include "SimOneServiceAPI.h"
#include "SimOneSensorAPI.h"
#include "SimOneHDMapAPI.h"
#include "json.hpp"

class ManualStopLineReservoir {
public:
    int caseId;
    int cmd;
    SSD::SimPoint3D src;
    SSD::SimPoint3D dst;

    ManualStopLineReservoir(int _caseId, int _cmd, SSD::SimPoint3D _src, SSD::SimPoint3D _dst)
    {
        caseId = _caseId;
        cmd = _cmd;
        src = _src;
        dst = _dst;
    }
};

void intValJson2Map(const nlohmann::json& js, std::map<int, int>& map);
void floatValJson2Map(const nlohmann::json& js, std::map<int, float>& map);
void getCaseSpeed(const nlohmann::json& js, int id, float& targetSpeed, float& minSpeed, float& maxSpeed);
int getCaseIdx(void);
void parseCreatStopLine(const nlohmann::json& jsonData, std::vector<ManualStopLineReservoir>& reservoir);