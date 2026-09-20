#pragma once
#include <set>
#include <memory>
#include "SSD/SimPoint3D.h"
#include "SSD/SimPoint2D.h"
#include "UtilMath.h"
#include "function.h"
/**
 * @brief 获取一个有效的交通信号灯，满足其有效性信息至少覆盖两条道路
 * @param roadIdList 一个包含多个道路 ID 的列表，用于判断交通信号灯的有效性
 * @return 返回第一个符合条件的有效交通信号灯，如果没有找到符合条件的信号灯，则返回默认构造的空信号灯
 */
HDMapStandalone::MSignal getValidTrafficLight(const SSD::SimVector<long>& roadIdList);

/**
 * @brief 获取所有有效的交通信号灯列表，满足每个信号灯的有效性信息至少覆盖两条道路
 * @param roadIdList 一个包含多个道路 ID 的列表，用于判断交通信号灯的有效性
 * @param validLightList 输出参数，返回符合条件的有效交通信号灯的列表
 */
void getValidTrafficLightList(const SSD::SimVector<long>& roadIdList, std::vector<HDMapStandalone::MSignal>& validLightList);

/**
 * @brief 计算合速度矢量的方位角（ENU 坐标系下，即以正东为 0 度，逆时针为正方向）
 * @param xVelocity 水平方向上的速度分量
 * @param yVelocity 垂直方向上的速度分量
 * @return float 返回合速度矢量的方位角，单位为度
 */
float calculateResultantAzimuth(float xVelocity, float yVelocity);

/**
 * @brief 计算两个点之间矢量的方位角（ENU 坐标系下，即以正东为 0 度，逆时针为正方向）
 * @param x1 起点的x坐标
 * @param y1 起点的y坐标
 * @param x2 终点的x坐标
 * @param y2 终点的y坐标
 * @return float 返回从起点到终点矢量的方位角，单位为度
 */
float calculateResultantAzimuth(float x1, float y1, float x2, float y2);
float calculateResultantAzimuth(double x1, double y1, double x2, double y2);

/**
 * @brief 判断两条道路是否具有相同的道路 Road ID
 * @param roadStr1 第一个道路的标识符
 * @param roadStr2 第二个道路的标识符
 * @return bool 如果两条道路的主道路ID相同，返回 `true`，否则返回 `false`
 */
bool isSameRoadId(const SSD::SimString& roadStr1, const SSD::SimString& roadStr2);

/**
 * @brief 计算车辆的线性减速制动速度
 * @param speed 当前车辆的速度
 * @param dist 车辆距离停止线的距离
 * @param initStopThres 初始停止阈值（即开始减速的距离）
 * @param fullStopThres 完全停止阈值（即车辆完全停止前的距离）
 * @return float 返回根据距离调整后的目标速度
 */
float linearBrake(float speed, float dist, float initStopThres, float fullStopThres);

/**
 * @brief 判断主车是否变道完成
 * @return bool 是否变道完成
 */
bool isLaneChanged(void);

/**
 * @brief 计算一条车道所属道路的总宽度
 * @param laneId 当前车道的唯一标识符
 * @param pos 车辆在车道上的位置，用于计算相邻车道的宽度
 * @return double 返回道路的总宽度。该宽度包括当前车道及所有邻接车道的宽度。
 */
double getRoadWidth(const SSD::SimString& laneId, const SSD::SimPoint3D& pos);

/**
 * @brief 计算指定车道的方位角，该函数通过获取指定车道的中线信息，计算车道中线首尾点之间的方位角
 * @param laneId 车道的唯一标识符
 * @return float 返回车道中线与正东方向的夹角（度）,如果无法计算，返回 0.0f
 */
float getLaneAzimuth(const SSD::SimString& laneId);

/**
 * @brief 判断车道是否近似垂直于南北方向（方位角接近 0° 或 180°）
 * @param azimuth 车道的方位角
 * @param tolerance 判断阈值
 * @return bool 是否垂直于南北方向
 */
inline bool isRoadPerp2North(float azimuth, float tolerance = 5.0f) { return (std::fabs(azimuth - 0.0f) < tolerance || std::fabs(azimuth - 180.0f) < tolerance); }

/**
 * @brief 判断车道是否近似垂直于东西方向（方位角接近 90° 或 270°）
 * @param azimuth 车道的方位角
 * @param tolerance 判断阈值
 * @return bool 是否垂直于东西方向
 */
inline bool isRoadPerp2East(float azimuth, float tolerance = 5.0f) { return (std::fabs(azimuth - 90.0f) < tolerance || std::fabs(azimuth - 270.0f) < tolerance); }

// 获取障碍物所在的道路两侧的坐标
//template<typename ObstacleType>
//void getObstacleLaneSidePosition(const ObstacleType& obstacle, SSD::SimPoint3D& leftPoint, SSD::SimPoint3D& rightPoint);

// 获取经过障碍物的并且与所在车道线垂直的一条直线上的特定两个点（分别在障碍物两侧并且与障碍物之间的距离为 2 倍车长）
//template<typename ObstacleType>
//void getObstaclePerpendicularLinePoints(const ObstacleType& obstacle, SSD::SimPoint3D& leftPoint, SSD::SimPoint3D& rightPoint);



// 获取障碍物所在的车道两侧的坐标
template<typename ObstacleType> // 既可以传入 StaticObstacle 类，也可以传入 SimOne_Data_Obstacle_Entry 类
void getObstacleLaneSidePosition(const ObstacleType& obstacle, SSD::SimPoint3D& leftPoint, SSD::SimPoint3D& rightPoint)
{
	SSD::SimPoint3D obstaclePos(obstacle.posX, obstacle.posY, obstacle.posZ);
	SSD::SimString obstacleLaneId = m_SampleGetNearMostLane(obstaclePos);
	HDMapStandalone::MLaneInfo obstacleLaneInfo;
	SimOneAPI::GetLaneSample(obstacleLaneId, obstacleLaneInfo);
	const SSD::SimPoint3DVector& midLine = obstacleLaneInfo.centerLine;
	const SSD::SimPoint3DVector& leftLine = obstacleLaneInfo.leftBoundary;
	const SSD::SimPoint3DVector& rightLine = obstacleLaneInfo.rightBoundary;

	double minDistTemp = std::numeric_limits<double>::max();
	int pointIndexTemp = midLine.size();
	for (size_t j = 0, je = midLine.size(); j < je; ++j)
	{
		double distTemp = UtilMath::planarDistance(midLine[j], obstaclePos);
		if (distTemp < minDistTemp)
		{
			minDistTemp = distTemp;
			pointIndexTemp = (int)j;
		}
	}

	leftPoint = leftLine[pointIndexTemp];
	rightPoint = rightLine[pointIndexTemp];
}

// 辅助函数：递归查找右侧最边缘车道的 ID
static SSD::SimString findRightMostLane(const SSD::SimString& laneId, std::set<SSD::SimString>& visitedLanes)
{
	if (visitedLanes.count(laneId) > 0) return laneId;
	visitedLanes.insert(laneId);

	HDMapStandalone::MLaneLink link;
	if (SimOneAPI::GetLaneLink(laneId, link) && !link.rightNeighborLaneName.Empty()) {
		return findRightMostLane(link.rightNeighborLaneName, visitedLanes);
	}
	return laneId;
}

// 辅助函数：递归查找左侧最边缘车道的 ID
static SSD::SimString findLeftMostLane(const SSD::SimString& laneId, std::set<SSD::SimString>& visitedLanes)
{
	if (visitedLanes.count(laneId) > 0) return laneId;
	visitedLanes.insert(laneId);

	HDMapStandalone::MLaneLink link;
	if (SimOneAPI::GetLaneLink(laneId, link) && !link.leftNeighborLaneName.Empty()) {
		return findLeftMostLane(link.leftNeighborLaneName, visitedLanes);
	}
	return laneId;
}

// 辅助函数：在给定车道上，找到“经过障碍物位置并且与车道线垂直的一条直线”上的点。左侧返回车道的左边界点，右侧返回车道的右边界点。
static void getRoadEdgeExtremePoints(const SSD::SimString& laneId, const SSD::SimPoint3D& obstaclePos,
	SSD::SimPoint3D& leftPoint, SSD::SimPoint3D& rightPoint, const int& idx)
{
	std::set<SSD::SimString> visitedLanes;  // 记录已经访问过的车道，避免重复计算
	// 对最左和最右两侧分别查找
	SSD::SimString leftExtremeLane = findLeftMostLane(laneId, visitedLanes);
	SSD::SimString rightExtremeLane = findRightMostLane(laneId, visitedLanes);
	visitedLanes.clear();
	// 对左侧极值车道，获取车道采样信息
	HDMapStandalone::MLaneInfo leftLaneInfo;
	if (SimOneAPI::GetLaneSample(leftExtremeLane, leftLaneInfo))
	{
		const SSD::SimPoint3DVector& leftBoundary = leftLaneInfo.leftBoundary;
		// 取该点对应的左边界点
		leftPoint = leftBoundary[idx];
	}

	// 对右侧极值车道，获取车道采样信息
	HDMapStandalone::MLaneInfo rightLaneInfo;
	if (SimOneAPI::GetLaneSample(rightExtremeLane, rightLaneInfo))
	{
		const SSD::SimPoint3DVector& rightBoundary = rightLaneInfo.rightBoundary;
		rightPoint = rightBoundary[idx];
	}
}

/**
 * @brief 获取障碍物所在的 road，最两侧 lane 上的坐标
 * @param[in] obstacle 障碍物信息
 * @param[out] leftPoint 最左侧坐标
 * @param[out] rightPoint 最右侧坐标
 */
 // 获取经过障碍物且与所在车道线垂直的一条直线上的点，该直线在道路最左和最右两侧车道上各取一点（即道路最边缘处的点）
template<typename ObstacleType>
void getObstaclePerpendicularLinePointsOutermost(const ObstacleType& obstacle,
	SSD::SimPoint3D& leftPoint,
	SSD::SimPoint3D& rightPoint)
{
	SSD::SimPoint3D obstaclePos(obstacle.posX, obstacle.posY, obstacle.posZ);
	SSD::SimString obstacleLaneId = m_SampleGetNearMostLane(obstaclePos);
	HDMapStandalone::MLaneInfo obstacleLaneInfo;
	SimOneAPI::GetLaneSample(obstacleLaneId, obstacleLaneInfo);
	const SSD::SimPoint3DVector& midLine = obstacleLaneInfo.centerLine;

	double minDistTemp = std::numeric_limits<double>::max();
	int pointIndexTemp = midLine.size();

	// 找到距离障碍物最近的车道中点的标号
	for (size_t j = 0, je = midLine.size(); j < je; ++j)
	{
		double distTemp = UtilMath::planarDistance(midLine[j], obstaclePos);
		if (distTemp < minDistTemp)
		{
			minDistTemp = distTemp;
			pointIndexTemp = (int)j;
		}
	}

	getRoadEdgeExtremePoints(obstacleLaneId, obstaclePos, leftPoint, rightPoint, pointIndexTemp);//按照标号，找到最两侧上点
}

inline void getObstaclePerpendicularLinePointsOutermost(const SSD::SimPoint3D& pt,
	SSD::SimPoint3D& leftPoint,
	SSD::SimPoint3D& rightPoint)
{
	SSD::SimPoint3D obstaclePos(pt.x, pt.y, pt.z);
	SSD::SimString obstacleLaneId = m_SampleGetNearMostLane(obstaclePos);
	HDMapStandalone::MLaneInfo obstacleLaneInfo;
	SimOneAPI::GetLaneSample(obstacleLaneId, obstacleLaneInfo);
	const SSD::SimPoint3DVector& midLine = obstacleLaneInfo.centerLine;

	double minDistTemp = std::numeric_limits<double>::max();
	int pointIndexTemp = midLine.size();

	// 找到距离障碍物最近的车道中点的标号
	for (size_t j = 0, je = midLine.size(); j < je; ++j)
	{
		double distTemp = UtilMath::planarDistance(midLine[j], obstaclePos);
		if (distTemp < minDistTemp)
		{
			minDistTemp = distTemp;
			pointIndexTemp = (int)j;
		}
	}

	getRoadEdgeExtremePoints(obstacleLaneId, obstaclePos, leftPoint, rightPoint, pointIndexTemp);//按照标号，找到最两侧上点
}

double vectorLength(const SSD::SimPoint3D& vector);
SSD::SimPoint3D normalizeVector(const SSD::SimPoint3D& vector);
SSD::SimPoint3D vectorAdd(const SSD::SimPoint3D& vec1, const SSD::SimPoint3D& vec2);
SSD::SimPoint3D vectorSubtract(const SSD::SimPoint3D& vec1, const SSD::SimPoint3D& vec2);
SSD::SimPoint3D scaleVector(const SSD::SimPoint3D& vec, double scalar);

// 获取经过障碍物的并且与所在车道线垂直的一条直线上的特定两个点（分别在障碍物两侧并且与障碍物之间的距离为 2 倍车长）
#define CAR_LENGTH (20)
template<typename ObstacleType>
void getObstaclePerpendicularLinePoints(const ObstacleType& obstacle, SSD::SimPoint3D& leftPoint, SSD::SimPoint3D& rightPoint)
{
	SSD::SimPoint3D obstaclePos(obstacle.posX, obstacle.posY, obstacle.posZ);
	SSD::SimString obstacleLaneId = m_SampleGetNearMostLane(obstaclePos);
	HDMapStandalone::MLaneInfo obstacleLaneInfo;
	SimOneAPI::GetLaneSample(obstacleLaneId, obstacleLaneInfo);
	const SSD::SimPoint3DVector& midLine = obstacleLaneInfo.centerLine;
	const SSD::SimPoint3DVector& leftLine = obstacleLaneInfo.leftBoundary;
	const SSD::SimPoint3DVector& rightLine = obstacleLaneInfo.rightBoundary;

	double minDistTemp = std::numeric_limits<double>::max();
	int pointIndexTemp = midLine.size();

	// 找到距离障碍物最近的车道中点
	for (size_t j = 0, je = midLine.size(); j < je; ++j)
	{
		double distTemp = UtilMath::planarDistance(midLine[j], obstaclePos);
		if (distTemp < minDistTemp)
		{
			minDistTemp = distTemp;
			pointIndexTemp = (int)j;
		}
	}

	// 获取车道线的法向量（假设我们需要与车道垂直的方向）
	SSD::SimPoint3D vectorToLeft = vectorSubtract(leftLine[pointIndexTemp], midLine[pointIndexTemp]);
	SSD::SimPoint3D vectorToRight = vectorSubtract(rightLine[pointIndexTemp], midLine[pointIndexTemp]);

	// 计算单位法向量
	SSD::SimPoint3D normalVectorLeft = normalizeVector(vectorToLeft);
	SSD::SimPoint3D normalVectorRight = normalizeVector(vectorToRight);

	// 获取垂直于车道线的两个点，分别在障碍物两侧，距离为2倍车长
	leftPoint = vectorAdd(obstaclePos, scaleVector(normalVectorLeft, 2 * CAR_LENGTH));
	rightPoint = vectorSubtract(obstaclePos, scaleVector(normalVectorRight, 2 * CAR_LENGTH));
}
// 判断行人是否已经穿过马路
bool hasPedestrianCompletelyLeftRoad(const SimOne_Data_Obstacle_Entry& obstacle, double cumulative_singel, double cumulative_double);

bool isPathRequireLaneChange(const SSD::SimVector<SSD::SimPoint3D>& path);
bool isTrajInterfere(const SSD::SimVector<SSD::SimPoint3D>& path, const SSD::SimPoint3D& pt, float azimuth);
inline float calculateTargetOvertakingVelocity(float v1, float s0, float s1, float L0, float L1, float La, float Lx)
{
	return v1 * La / ((s0 - s1) - 0.5 * (L0 + L1) + La - Lx);
}
//根据目标点在主车左右来给转向灯
void IsPosOnLeftOrRight(SSD::SimPoint3D targetPos);
//根据目标车道在主车的哪一侧来设置相应的信号灯
void IsLaneOnLeftOrRight(const SSD::SimPoint3D& mainVehiclePos, const SSD::SimString& mainLaneId, const SSD::SimString& targetLaneId);

float getRelativeLateralVelocity(const SimOne_Data_Obstacle_Entry& obstacle);