#pragma once

#include "SimOneServiceAPI.h"
#include <iostream>

using SSD::SimString;
using SSD::SimPoint3D;

void SampleGetLaneWidth(const SimString& laneId, const SimPoint3D& pos)
{
	double width;
	if (!SimOneAPI::GetLaneWidth(laneId, pos, width))
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Error: lane does not eixst in the map.");
		return;
	}
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "lane width at this location: (%f, %f, %f)", pos.x, pos.y, pos.z);
}