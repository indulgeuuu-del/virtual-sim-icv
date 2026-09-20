#pragma once
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include "SimOnePNCAPI.h"
#include "SimOneSensorAPI.h"
#include "SimOneHDMapAPI.h"
#include "Service/SimOneIOStruct.h"
#include "define.h"

class UtilDriver
{
public:
	static void setDriver(long long& timestamp, const float& throttle, const float& brake, const float& steering)
	{
		std::unique_ptr<SimOne_Data_Control> pControl = std::make_unique<SimOne_Data_Control>();
		pControl->timestamp = timestamp;
		pControl->throttle = throttle;
		pControl->brake = brake;
		pControl->steering = steering;
		pControl->handbrake = false;
		pControl->isManualGear = false;
		pControl->gear = static_cast<ESimOne_Gear_Mode>(1);
		SimOneAPI::SetDrive(0, pControl.get());
	}
	
	static double calculateSteering(const SSD::SimPoint3DVector& targetPath, SimOne_Data_Gps *pGps)
	{
		std::vector<float> pts;
		for (size_t i = 0; i < targetPath.size(); ++i)
		{
			pts.push_back(pow((pGps->posX - (float)targetPath[i].x), 2) + pow((pGps->posY - (float)targetPath[i].y), 2));
		}

		size_t index = std::min_element(pts.begin(), pts.end()) - pts.begin();
		size_t forwardIndex = 0;
		float minProgDist = 3.f;
		float progTime = 0.8f;
		float mainVehicleSpeed = sqrtf(pGps->velX * pGps->velX + pGps->velY * pGps->velY + pGps->velZ * pGps->velZ);
		float progDist = mainVehicleSpeed * progTime > minProgDist ? mainVehicleSpeed * progTime : minProgDist;

		for (; index < targetPath.size(); ++index)
		{
			forwardIndex = index;
			float distance = sqrtf(((float)pow(targetPath[index].x - pGps->posX, 2) + pow((float)targetPath[index].y - pGps->posY, 2)));
			if (distance >= progDist)
			{
				break;
			}
		}
		double psi = (double)pGps->oriZ;
		double alfa = atan2(targetPath[forwardIndex].y - pGps->posY, targetPath[forwardIndex].x - pGps->posX) - psi;
		double ld = sqrt(pow(targetPath[forwardIndex].y - pGps->posY, 2) + pow(targetPath[forwardIndex].x - pGps->posX, 2));
		double steering = -atan2(2. * (1.3 + 1.55) * sin(alfa), ld) * 36. / (7. * M_PI);

		ppTargetIndex = forwardIndex;

		return steering;
	}
	static double calculateSteering(const SSD::SimPoint3DVector& targetPath, SimOne_Data_Gps* pGps, size_t &carIndex)
	{
		std::vector<float> pts;
		for (size_t i = 0; i < targetPath.size(); ++i)
		{
			pts.push_back(pow((pGps->posX - (float)targetPath[i].x), 2) + pow((pGps->posY - (float)targetPath[i].y), 2));
		}

		size_t index = std::min_element(pts.begin(), pts.end()) - pts.begin();
		carIndex = index;
		size_t forwardIndex = 0;
		float minProgDist = 3.f;
		float progTime = 0.8f;
		float mainVehicleSpeed = sqrtf(pGps->velX * pGps->velX + pGps->velY * pGps->velY + pGps->velZ * pGps->velZ);
		float progDist = mainVehicleSpeed * progTime > minProgDist ? mainVehicleSpeed * progTime : minProgDist;

		for (; index < targetPath.size(); ++index)
		{
			forwardIndex = index;
			float distance = sqrtf(((float)pow(targetPath[index].x - pGps->posX, 2) + pow((float)targetPath[index].y - pGps->posY, 2)));
			if (distance >= progDist)
			{
				break;
			}
		}
		double psi = (double)pGps->oriZ;
		double alfa = atan2(targetPath[forwardIndex].y - pGps->posY, targetPath[forwardIndex].x - pGps->posX) - psi;
		double ld = sqrt(pow(targetPath[forwardIndex].y - pGps->posY, 2) + pow(targetPath[forwardIndex].x - pGps->posX, 2));
		double steering = -atan2(2. * (1.3 + 1.55) * sin(alfa), ld) * 36. / (7. * M_PI);

		ppTargetIndex = forwardIndex;

		return steering;
	}
};