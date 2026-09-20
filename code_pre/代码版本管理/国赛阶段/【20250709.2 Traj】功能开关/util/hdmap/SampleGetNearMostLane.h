#pragma once

#include "SimOneServiceAPI.h"
#include "SimOneHDMapAPI.h"

#include <iostream>

using SSD::SimPoint3D;
using SSD::SimString;

SimString SampleGetNearMostLane(const SimPoint3D& pos)
{
	SimString laneId;
	double s, t, s_toCenterLine, t_toCenterLine;
	if (!SimOneAPI::GetNearMostLane(pos, laneId, s, t, s_toCenterLine, t_toCenterLine))
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Error: lane is not found.");
		return laneId;
	}

	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "lane id: %s", laneId.GetString());
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "[s: %f, t: %f]", s,t);
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "[s_toCenterLine: %f, t_toCenterLine: %f]", s_toCenterLine, t_toCenterLine);
	return laneId;
}