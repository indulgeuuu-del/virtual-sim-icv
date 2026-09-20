#pragma once

#include "SimOneServiceAPI.h"
#include <iostream>

void SampleGenerateRoute()
{
	SSD::SimPoint3DVector route;
	SSD::SimPoint3DVector inputPoints;
	inputPoints.push_back(SSD::SimPoint3D(-135.477, 52.329, 0));
	inputPoints.push_back(SSD::SimPoint3D(-48.290, -18.109, 0));
	SSD::SimVector<int> validWayPoints;
	if (!SimOneAPI::GenerateRoute(inputPoints, validWayPoints, route))
	{
		
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Error: route is not generated in the map.");
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "validWayPoints:");
	
		for (auto& index : validWayPoints)
		{
			SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, " %d,", index);
		}
		return;
	}

	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "route size: %d", route.size());

	for (unsigned int i = 0; i < route.size(); i++)
	{
		auto& knot = route[i];
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "( %f, %f, %f),", knot.x, knot.y, knot.z);
	}
}