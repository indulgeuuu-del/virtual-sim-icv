#include "SimOneSensorAPI.h"
//#include "m_SampleGetNearMostLane.h"
#include "define.h"
#include "process.h"
#include "stopline.h"

static float crossProduct2D(const SSD::SimPoint2D& a, const SSD::SimPoint2D& b, const SSD::SimPoint2D& c) { // 计算二维叉积，忽略z轴
	float ux = b.x - a.x, uy = b.y - a.y;
	float vx = c.x - a.x, vy = c.y - a.y;
	return ux * vy - uy * vx;
}

// 交通信号灯产生的停止线：构造函数
StopLine::StopLine(const HDMapStandalone::MSignal& light)
{
	SSD::SimVector<HDMapStandalone::MObject> trafficLightStopLineList;
	SimOneAPI::GetStoplineList(light, mainVehicleLaneId, trafficLightStopLineList);

	srcPos = SSD::SimPoint2D(trafficLightStopLineList[0].boundaryKnots[0].x, trafficLightStopLineList[0].boundaryKnots[0].y);
	dstPos = SSD::SimPoint2D(trafficLightStopLineList[0].boundaryKnots[1].x, trafficLightStopLineList[0].boundaryKnots[1].y);
	midPos = SSD::SimPoint2D(0.5 * (srcPos.x + dstPos.x), 0.5 * (srcPos.y + dstPos.y));

	type = StopLine_Type::StopLine_Type_Traffic_Light;
	isMovingAlongLane = false; // 交通信号灯所产生的停止线一定是静止的
	isStopLineBehind = false; // 交通信号灯所产生的停止线一定在主车前方

	vx = vy = 0;
}

// 静态障碍物产生的停止线：构造函数（静态障碍物有可能构成全停止线，也可能构成半停止线）
StopLine::StopLine(const StaticObstacle& obstacle, StopLine_Type _type)
{
	if (_type == StopLine_Type::StopLine_Type_Fixed_Obstacle) // statical 的 obstacle 构成的 stopLine
	{
		type = StopLine_Type::StopLine_Type_Fixed_Obstacle;
		isMovingAlongLane = false;

		SSD::SimPoint3D srcPos3D, dstPos3D;
		getObstaclePerpendicularLinePointsOutermost<StaticObstacle>(obstacle, srcPos3D, dstPos3D);
		srcPos = SSD::SimPoint2D(srcPos3D.x, srcPos3D.y);
		dstPos = SSD::SimPoint2D(dstPos3D.x, dstPos3D.y);
		midPos = SSD::SimPoint2D(0.5 * (srcPos.x + dstPos.x), 0.5 * (srcPos.y + dstPos.y));
	}
	else if (_type == StopLine_Type::SingleStopLine_Type_Fixed_Obstacle) // statical 的 obstacle 构成的 singleStopLine
	{
		type = StopLine_Type::SingleStopLine_Type_Fixed_Obstacle;
		isMovingAlongLane = false;

		SSD::SimPoint3D leftPoint, rightPoint;
		getObstacleLaneSidePosition<StaticObstacle>(obstacle, leftPoint, rightPoint);
		srcPos = SSD::SimPoint2D(leftPoint.x, leftPoint.y);
		dstPos = SSD::SimPoint2D(rightPoint.x, rightPoint.y);
		midPos = SSD::SimPoint2D(0.5 * (srcPos.x + dstPos.x), 0.5 * (srcPos.y + dstPos.y));
	}

	vx = vy = 0;

	float crossPos = crossProduct2D(srcPos, dstPos, SSD::SimPoint2D(mainVehiclePos.x, mainVehiclePos.y));
	float crossTL = crossProduct2D(srcPos, dstPos, obstacle.tlPos);
	float crossBR = crossProduct2D(srcPos, dstPos, obstacle.brPos);

	if ((crossPos >= 0 && crossTL >= 0) || (crossPos <= 0 && crossTL <= 0)) {
		isStopLineBehind = true;
	}
	if ((crossPos >= 0 && crossBR >= 0) || (crossPos <= 0 && crossBR <= 0)) {
		isStopLineBehind = false;
	}
}

// 判断一个数是否在两数之间，不包含边界
template <typename T>
static bool IsBetween(const T val, const T p1, const T p2)
{
	return (p1 < val && val < p2) || (p2 < val && val < p1);
}

// 判断是否在某个矩形内
// param[in] pt 被判断的点
// param[in] vertex1、vertex2 矩形对角线上的两个角点
// return   bool 
static bool InRectangle(const SSD::SimPoint3D& pt, const SSD::SimPoint3D& vertex1, const SSD::SimPoint3D& vertex2)
{
	return IsBetween(pt.x, vertex1.x, vertex2.x) && IsBetween(pt.y, vertex1.y, vertex2.y);
}
static bool InRectangle(const SSD::SimPoint2D& pt, const SSD::SimPoint3D& vertex1, const SSD::SimPoint3D& vertex2)
{
	return IsBetween(pt.x, vertex1.x, vertex2.x) && IsBetween(pt.y, vertex1.y, vertex2.y);
}
bool SomeInInRectangle(SSD::SimPoint2D& tl, SSD::SimPoint2D& tr,
	SSD::SimPoint2D& bl, SSD::SimPoint2D& br,
	const SSD::SimPoint3D& vertex1, const SSD::SimPoint3D& vertex2)
{
	// 计算矩形四条边的中点
	SSD::SimPoint2D topMid((tl.x + tr.x) / 2, (tl.y + tr.y) / 2);  // 顶边中点
	SSD::SimPoint2D bottomMid((bl.x + br.x) / 2, (bl.y + br.y) / 2); // 底边中点
	SSD::SimPoint2D leftMid((tl.x + bl.x) / 2, (tl.y + bl.y) / 2);  // 左边中点
	SSD::SimPoint2D rightMid((tr.x + br.x) / 2, (tr.y + br.y) / 2); // 右边中点

	// 判断四个角点是否在矩形内部
	bool flag1 = InRectangle(tl, vertex1, vertex2);
	bool flag2 = InRectangle(tr, vertex1, vertex2);
	bool flag3 = InRectangle(bl, vertex1, vertex2);
	bool flag4 = InRectangle(br, vertex1, vertex2);

	// 判断四条边的中点是否在矩形内部
	bool flag5 = InRectangle(topMid, vertex1, vertex2);
	bool flag6 = InRectangle(bottomMid, vertex1, vertex2);
	bool flag7 = InRectangle(leftMid, vertex1, vertex2);
	bool flag8 = InRectangle(rightMid, vertex1, vertex2);

	// 只要有一个点在矩形内部，就返回 true
	return flag1 || flag2 || flag3 || flag4 || flag5 || flag6 || flag7 || flag8;
}

// 动态障碍物产生的停止线：构造函数（动态障碍物有可能构成全停止线，也可能构成半停止线）
StopLine::StopLine(const SimOne_Data_Obstacle_Entry& obstacle, StopLine_Type _type)
{
	index = obstacle.id;
	mobileObs = obstacle;

	if (_type == StopLine_Type::StopLine_Type_Mobile_Obstacle) // vertical 移动的 obstacle 构成的 stopLine
	{
		type = StopLine_Type::StopLine_Type_Mobile_Obstacle;
		isMovingAlongLane = false;

		SSD::SimPoint3D srcPos3D, dstPos3D;
		SSD::SimPoint3D obstaclePos(obstacle.posX, obstacle.posY, obstacle.posZ);
		SSD::SimPoint2D tl, tr, bl, br;
		calculateObstacleCorners(obstacle, tl, tr, bl, br);
		// 判断行人是否在斑马线矩形上
		if (flag_haveCrossWalk && SomeInInRectangle(tl, tr, bl, br, potentialcrosswalkList[0].boundaryKnots[0], potentialcrosswalkList[0].boundaryKnots[2]))
		{
			globalLogger(Logger::Color::BrightGreen) << "在斑马线内";
			SSD::SimVector<HDMapStandalone::MObject> trafficLightStopLineList;
			SimOneAPI::GetSpecifiedLaneStoplineList(mainVehicleLaneId, trafficLightStopLineList);
			srcPos = SSD::SimPoint2D(trafficLightStopLineList[0].boundaryKnots[0].x, trafficLightStopLineList[0].boundaryKnots[0].y);
			dstPos = SSD::SimPoint2D(trafficLightStopLineList[0].boundaryKnots[1].x, trafficLightStopLineList[0].boundaryKnots[1].y);
		}
		else
		{
			globalLogger(Logger::Color::BrightGreen) << "在斑马线外";
			globalLogger(Logger::Color::BrightGreen) << "障碍物：" << obstaclePos.x << "," << obstaclePos.y;
			SSD::SimString laneid = m_SampleGetNearMostLane(obstaclePos);
			if(SimOneAPI::IsDriving(laneid))
				getObstaclePerpendicularLinePointsOutermost<SimOne_Data_Obstacle_Entry>(obstacle, srcPos3D, dstPos3D); // 利用递归，获取最两侧车道上的点
			else 
				getObstaclePerpendicularLinePoints<SimOne_Data_Obstacle_Entry>(obstacle, srcPos3D, dstPos3D);
			srcPos = SSD::SimPoint2D(srcPos3D.x, srcPos3D.y);
			dstPos = SSD::SimPoint2D(dstPos3D.x, dstPos3D.y);
			midPos = SSD::SimPoint2D(0.5 * (srcPos.x + dstPos.x), 0.5 * (srcPos.y + dstPos.y));
		}
		
		vx = obstacle.velX;
		vy = obstacle.velY;
	}
	else if (_type == StopLine_Type::SingleStopLine_Type_Mobile_Obstacle) // horizental 移动的 obstacle 构成的 singleStopLine
	{
		type = StopLine_Type::SingleStopLine_Type_Mobile_Obstacle;
		isMovingAlongLane = true;
		SSD::SimPoint3D leftPoint, rightPoint;
		getObstaclePerpendicularLinePointsOutermost<StaticObstacle>(obstacle, leftPoint, rightPoint);
		srcPos = SSD::SimPoint2D(leftPoint.x, leftPoint.y);
		dstPos = SSD::SimPoint2D(rightPoint.x, rightPoint.y);
		midPos = SSD::SimPoint2D(0.5 * (srcPos.x + dstPos.x), 0.5 * (srcPos.y + dstPos.y));

		vx = obstacle.velX;
		vy = obstacle.velY;
	}
}

StopLine::StopLine(const SimOne_Data_Obstacle_Entry& obstacle)
{
	type = StopLine_Type::StopLine_Type_Crosswalk;
	isMovingAlongLane = false;

	SSD::SimVector<HDMapStandalone::MObject> trafficLightStopLineList;
	SimOneAPI::GetSpecifiedLaneStoplineList(mainVehicleLaneId, trafficLightStopLineList);
	srcPos = SSD::SimPoint2D(trafficLightStopLineList[0].boundaryKnots[0].x, trafficLightStopLineList[0].boundaryKnots[0].y);
	dstPos = SSD::SimPoint2D(trafficLightStopLineList[0].boundaryKnots[1].x, trafficLightStopLineList[0].boundaryKnots[1].y);

	vx = obstacle.velX;
	vy = obstacle.velY;
}

StopLine::StopLine(const ManualStopLineReservoir& reservoir)
{
	type = static_cast<StopLine_Type>(reservoir.cmd);
	isMovingAlongLane = false;

	srcPos = SSD::SimPoint2D(reservoir.src.x, reservoir.src.y);
	dstPos = SSD::SimPoint2D(reservoir.dst.x, reservoir.dst.y);

	vx = vy = 0;
}

// 计算停止线到主车的距离
double StopLine::toVehicleDist(const SSD::SimPoint3D& vehiclePosition)
{
	float dx = dstPos.x - srcPos.x;
	float dy = dstPos.y - srcPos.y;

	float numerator = std::abs(dy * vehiclePosition.x - dx * vehiclePosition.y + dstPos.x * srcPos.y - dstPos.y * srcPos.x);
	float denominator = std::sqrt(dx * dx + dy * dy);

	return (denominator > 1e-6f) ? (numerator / denominator) : 0.0;
}

// 查询停止线是否与主车同车道
bool StopLine::isVechicleInSameLane(const SSD::SimPoint3D& vehiclePosition, const SSD::SimString& vehicleLane)
{
	//isSameRoadId
	/* 首先检查停止线与主车是否在同一个路段，道路 ID 形式为 roadId_sectionIndex_laneId */
	//SSD::SimString midPosLane = m_SampleGetNearMostLane(SSD::SimPoint3D(midPos.x, midPos.y, vehiclePosition.z));
	/*if (isSameRoadId(vehicleLane, midPosLane))
	{*/
		//globalLogger(Logger::Color::BrightGreen)<< "相同道路";
		// 转入 Frenet 坐标系下处理，判断停止线的起点和终点是否均在主车的同一侧，如果分别在主车的两侧，则停止线与主车同车道
		// s：沿车道中心线的距离 t：垂直于车道中心线的距离 z：输入点在局部 ENU 坐标系中的高度值
		double sSrcPos = 0.0, tSrcPos = 0.0;
		double sDstPos = 0.0, tDstPos = 0.0;
		double sVehPos = 0.0, tVehPos = 0.0;
		double zTemp = 0.0;
		SSD::SimPoint3D srcPos3D = SSD::SimPoint3D(srcPos.x, srcPos.y, vehiclePosition.z);
		SSD::SimPoint3D dstPos3D = SSD::SimPoint3D(dstPos.x, dstPos.y, vehiclePosition.z);
		// 计算停止线起点和终点以及主车相对于道路参考线的 ST 坐标
		SimOneAPI::GetRoadST(vehicleLane, srcPos3D, sSrcPos, tSrcPos, zTemp);
		SimOneAPI::GetRoadST(vehicleLane, dstPos3D, sDstPos, tDstPos, zTemp);
		SimOneAPI::GetRoadST(vehicleLane, vehiclePosition, sVehPos, tVehPos, zTemp);
		// 如果停止线的起点和终点均在主车的同一侧
		float dt1 = tSrcPos - tVehPos;
		float dt2 = tDstPos - tVehPos;
		if (dt1 * dt2 < 0) 
		{
			globalLogger(Logger::Color::BrightGreen) << "相同道路";
			return true; }
	//}
	return false;
}