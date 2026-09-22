#pragma once

#include "SimOneServiceAPI.h"
#include "public/common/MLaneInfo.h"
#include <iostream>

using namespace HDMapStandalone;
using SSD::SimPoint3D;
using SSD::SimString;

void SampleGetLaneSample(const SimString& laneId)
{
	MLaneInfo info;
	if (!SimOneAPI::GetLaneSample(laneId, info))
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Error: lane does not eixst in the map.");
		return;
	}
		
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "left boundary knot size: %d", info.leftBoundary.size());
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "left boundary knot list:");

	for (unsigned int i = 0; i < info.leftBoundary.size(); i++)
	{
		const SimPoint3D& knot = info.leftBoundary[i];
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "( %f, %f, %f),", knot.x, knot.y, knot.z);
	}

	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "right boundary knot size: %d", info.rightBoundary.size());
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "right boundary knot list:");

	for (unsigned int i = 0; i < info.rightBoundary.size(); i++)
	{
		const SimPoint3D& knot = info.rightBoundary[i];
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "( %f, %f, %f),", knot.x, knot.y, knot.z);
	}

	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "center line knot size: %d", info.centerLine.size());
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "center line knot list:");

	for (unsigned int i = 0; i < info.centerLine.size(); i++)
	{
		const SimPoint3D& knot = info.centerLine[i];
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "( %f, %f, %f),", knot.x, knot.y, knot.z);
	}
}