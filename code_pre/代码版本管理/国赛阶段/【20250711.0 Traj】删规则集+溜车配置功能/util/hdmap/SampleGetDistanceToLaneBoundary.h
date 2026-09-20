#pragma once

#include "SimOneServiceAPI.h"
#include <iostream>

using SSD::SimPoint3D;
using SSD::SimString;

void SampleGetDistanceToLaneBoundary(const SimPoint3D& pos)
{
	SimString laneId;
	double distToLeft, distToRight, distToLeft2D, distToRight2D;
	if (!SimOneAPI::GetDistanceToLaneBoundary(pos, laneId, distToLeft, distToRight, distToLeft2D, distToRight2D))
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Error: lane is not found.");
		return;
	}

	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "lane id: %s", laneId.GetString());
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "[distToLeft, distToRight]: %f, %f", distToLeft, distToRight);
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "[distToLeft2D, distToRight2D]: %f, %f", distToLeft2D, distToRight2D);
}