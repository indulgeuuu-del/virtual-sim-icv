#pragma once
#include "SSD/SimPoint3D.h"
#include "obstacle.h"
#include "utility.h"

enum StopLine_Type
{
	StopLine_Type_Traffic_Light, // 交通信号灯产生的全停止线
	StopLine_Type_Fixed_Obstacle, // 固定障碍物产生的全停止线
	StopLine_Type_Mobile_Obstacle, // 垂向移动障碍物产生的全停止线
	SingleStopLine_Type_Fixed_Obstacle, // 固定障碍物产生的半停止线
	SingleStopLine_Type_Mobile_Obstacle, // 沿车道运动的障碍物产生的半停止线
	StopLine_Type_Crosswalk // 斑马线上的障碍物产生的全停止线
};

class StopLine {
public:
	int index = -1;
	StopLine_Type type;
	bool isMovingAlongLane; // 是否沿车道方向移动，只有 SingleStopLine 有可能沿车道方向移动
	bool isStopLineBehind; // 该停止线是否在主车后方
	float vx, vy;
	SSD::SimPoint2D srcPos;
	SSD::SimPoint2D dstPos;
	SSD::SimPoint2D midPos;

public:
	// 交通灯产生的停止线的衍生变量
	HDMapStandalone::MSignal trafficLight; // （如果有）停止线所对应的交通信号灯对象
	SimOne_Data_TrafficLight* pTrafficLight; // （如果有）停止线所对应的交通信号灯对象

	// 障碍物产生的停止线的衍生变量
	StaticObstacle staticObs; // （如果有）停止线所对应的移动障碍物对象
	SimOne_Data_Obstacle_Entry mobileObs; // （如果有）停止线所对应的移动障碍物对象

public:
	// 交通信号灯产生的停止线：构造函数
	StopLine(const HDMapStandalone::MSignal& light);

	// 静态障碍物产生的停止线：构造函数（静态障碍物有可能构成全停止线，也可能构成半停止线）
	StopLine(const StaticObstacle& obstacle, StopLine_Type _type);

	// 动态障碍物产生的停止线：构造函数（动态障碍物有可能构成全停止线，也可能构成半停止线）
	StopLine(const SimOne_Data_Obstacle_Entry& obstacle, StopLine_Type _type);

	StopLine(const SimOne_Data_Obstacle_Entry& obstacle);
	StopLine(const ManualStopLineReservoir& reservoir);

	// 计算停止线到主车的距离
	double toVehicleDist(const SSD::SimPoint3D& vehiclePosition);

	// 查询停止线是否与主车同车道
	bool isVechicleInSameLane(const SSD::SimPoint3D& vehiclePosition, const SSD::SimString& vehicleLane);
};

bool SomeInInRectangle(SSD::SimPoint2D& tl, SSD::SimPoint2D& tr,
	SSD::SimPoint2D& bl, SSD::SimPoint2D& br,
	const SSD::SimPoint3D& vertex1, const SSD::SimPoint3D& vertex2);