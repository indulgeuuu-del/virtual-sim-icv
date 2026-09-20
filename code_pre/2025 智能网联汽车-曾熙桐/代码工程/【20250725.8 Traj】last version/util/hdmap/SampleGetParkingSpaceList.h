#pragma once

#include "SimOneServiceAPI.h"
#include <iostream>

using SSD::SimString;
using SSD::SimStringVector;

std::string typeToStr_(const HDMapStandalone::ERoadMarkType& type)
{
	std::string strType = "none";
	switch (type)
	{
	case ERoadMarkType::none:
	{
		strType = "none";
	}
	break;
	case ERoadMarkType::solid:
	{
		strType = "solid";
	}
	break;
	case ERoadMarkType::broken:
	{
		strType = "broken";
	}
	break;
	case ERoadMarkType::solid_solid:
	{
		strType = "solid_solid";
	}
	break;
	case ERoadMarkType::solid_broken:
	{
		strType = "solid_broken";
	}
	break;
	case ERoadMarkType::broken_solid:
	{
		strType = "broken_solid";
	}
	break;
	case ERoadMarkType::broken_broken:
	{
		strType = "broken_broken";
	}
	break;
	case ERoadMarkType::botts_dots:
	{
		strType = "botts_dots";
	}
	break;
	case ERoadMarkType::grass:
	{
		strType = "grass";
	}
	break;
	case ERoadMarkType::curb:
	{
		strType = "curb";
	}
	break;
	default:
		break;
	}
	return std::move(strType);
}

SSD::SimVector<HDMapStandalone::MParkingSpace> SampleGetParkingSpaceList()
{
	SSD::SimVector<HDMapStandalone::MParkingSpace> parkingSpaceList;
	SimOneAPI::GetParkingSpaceList(parkingSpaceList);
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "GetParkingSpaceIds returns count: %d", parkingSpaceList.size());
		if ((int)parkingSpaceList.size() > 0)
		{
			SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "parking space info:");
			for (unsigned int i = 0; i < parkingSpaceList.size(); i++)
			{
				SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "id: %ld", parkingSpaceList[i].id);
				SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "front type: %s", typeToStr_(parkingSpaceList[i].front.type).c_str());
			}
		}
	}
	return std::move(parkingSpaceList);
}