#pragma once
#include "SimOneServiceAPI.h"
#include "stopline.h"

class ManualCreateStopLineReservoir {
public:
	StopLine_Type type;
	int srcFrame;
	int dstFrame;
	SSD::SimPoint3D srcPos;
	SSD::SimPoint3D dstPos;
};