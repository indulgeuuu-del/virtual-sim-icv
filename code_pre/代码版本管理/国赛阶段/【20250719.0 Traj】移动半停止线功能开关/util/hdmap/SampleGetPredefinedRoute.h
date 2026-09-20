#pragma once
#include "SimOneServiceAPI.h"
#include <iostream>

using SSD::SimString;
using SSD::SimPoint3D;

void SampleGetPredefinedRoute()
{
	SSD::SimPoint3DVector routeInfo;
	bool resule = SimOneAPI::GetPredefinedRoute(routeInfo);
	if (!resule)
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Not exists!");
	} 
	for (size_t i = 0; i < routeInfo.size(); i++)
	{
		auto &item = routeInfo[i];
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "[item.x: %f, item.y: %f, item.y: %f]", item.x, item.y, item.z);
	}
}