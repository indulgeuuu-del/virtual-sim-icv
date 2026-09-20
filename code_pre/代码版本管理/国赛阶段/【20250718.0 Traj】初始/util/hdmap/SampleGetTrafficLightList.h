#pragma once
#include "SimOneServiceAPI.h"
#include <iostream>

using SSD::SimString;

SSD::SimVector<HDMapStandalone::MSignal> SampleGetTrafficLightList()
{
	SSD::SimVector<HDMapStandalone::MSignal> TrafficLightList;
	SimOneAPI::GetTrafficLightList(TrafficLightList);
	if (TrafficLightList.size() < 1)
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "No traffic light exists!");
		return TrafficLightList;
	}
	
	for (auto& item : TrafficLightList)
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "sign id: %ld", item.id);
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "type: %s", item.type.GetString());
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "subType: %s", item.subType.GetString());
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "item.pt.x: %f, item.pt.y: %f, item.pt.z: %f", item.pt.x, item.pt.y, item.pt.z);
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "heading.pt.x: %f, heading.pt.y: %f, heading.pt.z: %f,", item.heading.x, item.heading.y, item.heading.z);
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "value: %s", item.value.GetString());
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "unit: %s", item.unit.GetString());
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "isDynamic: %d", item.isDynamic);
	}
	return TrafficLightList;
}