#pragma once

#include "SimOneServiceAPI.h"
#include "SimOneHDMapAPI.h"
#include <iostream>

using SSD::SimString;
using SSD::SimPoint3D;

void SampleContainsLane(const SimString& laneId)
{
	if (!SimOneAPI::ContainsLane(laneId))
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Error: lane does not eixst in the map.");
		return;
	}		
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Information, "lane exists in the map.");
}