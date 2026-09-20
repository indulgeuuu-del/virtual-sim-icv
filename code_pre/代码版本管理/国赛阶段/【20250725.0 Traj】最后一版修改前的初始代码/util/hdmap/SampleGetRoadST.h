#pragma once

#include "SimOneServiceAPI.h"
#include <iostream>


using SSD::SimString;
using SSD::SimPoint3D;

void SampleGetRoadST(const SimString& laneId, const SimPoint3D& pos)
{
	double s, t, z;
	if (!SimOneAPI::GetRoadST(laneId, pos, s, t, z))
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Error: lane does not eixst in the map");
		return;
	}
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "relative to this lane's owner road, this location: (%f, %f, %f), s-t coordidate position: [s: %f,t: %f]", pos.x, pos.y, pos.z, s, t);
}