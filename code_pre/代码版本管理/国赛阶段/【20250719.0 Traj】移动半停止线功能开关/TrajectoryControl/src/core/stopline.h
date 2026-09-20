#pragma once
#include "SimOneHDMapAPI.h"
#include "SSD/SimPoint3D.h"

extern class Obstacle;
extern class ManualStopLineReservoir;

class StopLine {
public:
	enum class Type {
		Whole,
		Single,
		Attract,
	};

	enum class Strategy {
		StopStart,
		LaneChange,
	};

	Type type;
	Strategy strategy;

	bool isVechicleInSameLane; // 与主车是否同车道，或是否横跨主车所在的车道
	bool isBehind; // 是否在主车后面
	bool isValid; // 停止线是否有效
	float toVehicleDist; // 到主车的距离
	float vx, vy;
	float velocity, velocityPlanar;
	float offset; // 距离障碍物 offset 时停车

	SSD::SimPoint3D src;
	SSD::SimPoint3D dst;
	SSD::SimPoint3D mid;

	Obstacle* obstaclePtr;

public:
	// 默认构造函数
	StopLine();

	// 通过交通信号灯创建停止线
	StopLine(const HDMapStandalone::MSignal& light);

	// 通过障碍物创建停止线
	StopLine(Obstacle& obstacle);

	// 通过手动创建停止线
	StopLine(const ManualStopLineReservoir& reservoir);

	// （工厂函数）创建特殊停止线：吸引线
	static StopLine AttractLine(Obstacle& obstacle, const SSD::SimPoint3D& pt);

private:
	// 计算停止线到主车的距离
	double calculateVehicleDistance(const SSD::SimPoint3D& vehiclePosition) const;

	// 判断停止线是否在主车后方
	bool isStopLineBehindVehicle(const SSD::SimPoint3D& srcPos, const SSD::SimPoint3D& dstPos, const SSD::SimPoint3D& mainVehiclePos, const SSD::SimPoint3D& tlPos, const SSD::SimPoint3D& brPos);

	// 查询停止线是否与主车同车道
	bool checkVechicleInSameLane(const Obstacle& obstacle) const;
};

// 计算到主车最近的停止线
extern bool calculateNearestStopLine(const std::vector<StopLine>& list, StopLine& stopLine, float& distance);

// 对于静止的停止线的变道操作（使用 Liy 变道法）
extern bool getLaneChangePath(StopLine& stopLine);

// 对于运动的停止线的变道操作
bool getLaneChangePath(StopLine& stopLine,float velocityPlanar);