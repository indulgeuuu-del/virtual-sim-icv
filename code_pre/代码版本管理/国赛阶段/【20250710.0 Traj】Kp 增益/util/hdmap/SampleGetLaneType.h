#pragma once

#include "SimOneServiceAPI.h"
#include <iostream>

using SSD::SimString;

void SampleGetLaneType(const SimString& laneId)
{
	HDMapStandalone::MLaneType type;
	if (!SimOneAPI::GetLaneType(laneId, type))
	{
		SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Warning, "Error: lane does not eixst in the map");
		return;
	}
	std::string typeStr;
	switch (type)
	{
		case MLaneType::none:
		{
			typeStr = "MLaneType::none";
		}
		break;
		case MLaneType::driving:
		{
			typeStr = "MLaneType::driving";
		}
		break;
		case MLaneType::stop:
		{
			typeStr = "MLaneType::stop";
		}
		break;
		case MLaneType::shoulder:
		{
			typeStr = "MLaneType::shoulder";
		}
		break;
		case MLaneType::biking:
		{
			typeStr = "MLaneType::biking";
		}
		break;
		case MLaneType::sidewalk:
		{
			typeStr = "MLaneType::sidewalk";
		}
		break;
		case MLaneType::border:
		{
			typeStr = "MLaneType::border";
		}
		break;
		case MLaneType::restricted:
		{
			typeStr = "MLaneType::restricted";
		}
		break;
		case MLaneType::parking:
		{
			typeStr = "MLaneType::parking";
		}
		break;
		case MLaneType::bidirectional:
		{
			typeStr = "MLaneType::bidirectional";
		}
		break;
		case MLaneType::median:
		{
			typeStr = "MLaneType::median";
		}
		break;
		case MLaneType::special1:
		{
			typeStr = "MLaneType::special1";
		}
		break;
		case MLaneType::special2:
		{
			typeStr = "MLaneType::special2";
		}
		break;
		case MLaneType::special3:
		{
			typeStr = "MLaneType::special3";
		}
		break;
		case MLaneType::roadWorks:
		{
			typeStr = "MLaneType::roadWorks";
		}
		break;
		case MLaneType::tram:
		{
			typeStr = "MLaneType::tram";
		}
		break;
		case MLaneType::rail:
		{
			typeStr = "MLaneType::rail";
		}
		break;
		case MLaneType::entry:
		{
			typeStr = "MLaneType::entry";
		}
		break;
		case MLaneType::exit:
		{
			typeStr = "MLaneType::exit";
		}
		break;
		case MLaneType::offRamp:
		{
			typeStr = "MLaneType::offRamp";
		}
		break;
		case MLaneType::onRamp:
		{
			typeStr = "MLaneType::onRamp";
		}
		break;
		case MLaneType::mwyEntry:
		{
			typeStr = "MLaneType::mwyEntry";
		}
		break;
		case MLaneType::mwyExit:
		{
			typeStr = "MLaneType::mwyExit";
		}
		break;
		default:
			typeStr = "MLaneType::none";
	}
	SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Debug, "lane type: %s", typeStr.c_str());
}