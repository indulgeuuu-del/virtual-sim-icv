#pragma once

#include "SimOneServiceAPI.h"
#include <iostream>

using SSD::SimPoint3D;
using SSD::SimString;
using SSD::SimStringVector;

void SampleGetNearLanes(const SimPoint3D& pos, const double& distance)
{
	SimStringVector nearLanes;
	if (!SimOneAPI::GetNearLanes(pos, distance, nearLanes))
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "No lanes(lane) are(is) found");
		return;
	}
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "lane id list:");
	for (unsigned int i = 0; i < nearLanes.size(); i++)
	{
		const SimString& laneId = nearLanes[i];
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "%s,", laneId.GetString());
	}
}

