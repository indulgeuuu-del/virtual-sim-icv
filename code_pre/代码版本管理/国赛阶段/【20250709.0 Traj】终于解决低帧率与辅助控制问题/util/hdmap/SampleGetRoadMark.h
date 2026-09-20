#pragma once
#include "SimOneServiceAPI.h"
#include <iostream>

using SSD::SimString;
using SSD::SimPoint3D;

void SampleGetRoadMark(const SimString& laneId, const SimPoint3D& pos)
{
	HDMapStandalone::MRoadMark left;
	HDMapStandalone::MRoadMark right;
	bool result = SimOneAPI::GetRoadMark(pos, laneId, left, right);
	if (result)
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "left.sOffset: %f", left.sOffset);
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "left.width: %f", left.width);
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "left.type: %d", (int)left.type);
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "left.color: %d", (int)left.color);
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "right.sOffset: %f", right.sOffset);
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "right.width: %f", right.width);
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "left.type: %d", (int)left.type);
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "left.color: %d", (int)left.color);	
	}
	else
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Not exists!");
		return;
	}

}
