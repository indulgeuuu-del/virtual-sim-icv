#pragma once
#include "SimOneHDMapAPI.h"
#include "define.h"

class Trajectory {
public:
	typedef AVPStatus Type;

	SSD::SimPoint3D startPoint;
	SSD::SimPoint3D endPoint;
	SSD::SimPoint3DVector trajectory;

	Trajectory(void);
	Trajectory(const SSD::SimPoint3D& startPt, const SSD::SimPoint3D& endPt, const ParkingSlot& slot, Type type);
};