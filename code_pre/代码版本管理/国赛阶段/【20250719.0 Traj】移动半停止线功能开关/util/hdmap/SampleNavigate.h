#pragma once
#include "SimOneServiceAPI.h"
#include <iostream>

using SSD::SimString;

void SampleNavigate(const SSD::SimPoint3DVector& inputPoints)
{
	SSD::SimVector<int> validWayPoints;
	SSD::SimVector<long> roadIdList;
	bool result = SimOneAPI::Navigate(inputPoints, validWayPoints, roadIdList);
	if (!result)
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Not exists!");
	}
	for (auto&item: validWayPoints)
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "indexOfValidPoint: %d", item);
	}
	for (auto&item : roadIdList)
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "roadId: %ld", item);
	}
}
