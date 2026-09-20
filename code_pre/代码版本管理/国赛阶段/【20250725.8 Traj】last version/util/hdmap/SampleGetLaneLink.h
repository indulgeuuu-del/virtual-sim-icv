#pragma once

#include "SimOneServiceAPI.h"
#include "public/common/MLaneLink.h"
#include <iostream>

using SSD::SimString;

void SampleGetLaneLink(const SimString& laneId)
{
	MLaneLink link;
	if (!SimOneAPI::GetLaneLink(laneId, link))
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Error: lane does not eixst in the map");
		return;
	}

	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "predecessor lane Id list:");
	for (unsigned int i = 0; i < link.predecessorLaneNameList.size(); i++)
	{
		const SimString& laneId = link.predecessorLaneNameList[i];
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, " %s,", laneId.GetString());
	}
		
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "successor lane Id list:");
	for (unsigned int i = 0; i < link.successorLaneNameList.size(); i++)
	{
		const SimString& laneId = link.successorLaneNameList[i];
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, " %s,", laneId.GetString());
	}

	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "left neighbor Id: %s", link.leftNeighborLaneName.GetString());
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "right neighbor Id: %s", link.rightNeighborLaneName.GetString());
}