#pragma once

#include "SimOneServiceAPI.h"
#include <iostream>

using SSD::SimString;
using SSD::SimPoint3D;

void SampleGetLaneST(const SimString& laneId, const SimPoint3D& pos, double &s, double &t)
{
	//double s, t;
	if (!SimOneAPI::GetLaneST(laneId, pos, s, t))
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Error: lane does not eixst in the map.");
		return;
	}
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "relative to this lane, this location: (%f, %f, %f),s s-t coordidate position: [s: %f, t: %f]", pos.x, pos.y, pos.z, s, t);
}