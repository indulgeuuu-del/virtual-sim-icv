#pragma once
#include <memory>
#include "SSD/SimPoint2D.h"
#include "SimOneHDMapAPI.h"

class Obstacle {
public:
	enum class Type {
		Static,
		Vertical,
		Horizental,
		AmongCrosswalk
	};

	Type type;

	float width, length;

	float velocity, velocityPlanar;
	float vx, vy, vz;
	float yaw;

	double sRelativeToVehicle, tRelativeToVehicle; // 相对于主车所在车道的 s、t 坐标
	double sRelativeToLane, tRelativeToLane; // 相对于自身所在车道的 s、t 坐标
	double sRelativeToRoad, tRelativeToRoad; // 相对于自身所在道路的 s、t 坐标
	SSD::SimPoint3D pt;
	SSD::SimPoint3D tl, tr, bl, br;

	SSD::SimString laneID;
	float laneAzimuth; // 障碍物所在车道的方位角
	bool isSameLaneWithMainVehicle;
	bool isSameRoadWithMainVehicle;
	double laneWidth;
	double roadWidth;

	SSD::SimPoint3D leftLanePt, rightLanePt; // 障碍物所属的车道的左右边界点
	SSD::SimPoint3D leftRoadPt, rightRoadPt; // 障碍物所属的道路的左右边界点

	//const Prediction* predictionPtr;
	std::shared_ptr<Prediction> predictionPtr;
	std::vector<SimOne_Data_Vec3f>predictionGM;

public:
	Obstacle() : predictionPtr(std::make_shared<Prediction>()) {}

	// 通过 SimOne_Data_Obstacle_Entry 来构造
	Obstacle(const SimOne_Data_Obstacle_Entry& obstacle);

	// 通过索引来构造
	Obstacle(size_t index);

	// 通过索引列表来构造
	Obstacle(const std::vector<size_t>& index);
};

extern SSD::SimPoint3D calculateObstacleTL(const SimOne_Data_Obstacle_Entry& obstacle);
extern SSD::SimPoint3D calculateObstacleTR(const SimOne_Data_Obstacle_Entry& obstacle);
extern SSD::SimPoint3D calculateObstacleBL(const SimOne_Data_Obstacle_Entry& obstacle);
extern SSD::SimPoint3D calculateObstacleBR(const SimOne_Data_Obstacle_Entry& obstacle);

inline void calculateObstacleCorners(const SimOne_Data_Obstacle_Entry& obstacle, SSD::SimPoint3D& tl, SSD::SimPoint3D& tr, SSD::SimPoint3D& bl, SSD::SimPoint3D& br)
{
	tl = calculateObstacleTL(obstacle);
	tr = calculateObstacleTR(obstacle);
	bl = calculateObstacleBL(obstacle);
	br = calculateObstacleBR(obstacle);
}

extern void groupObstacleByDist(const std::vector<size_t>& indexList, float threshold, std::vector<std::vector<size_t>>& groups);
extern void groupObstacleByS(const std::vector<Obstacle>& obstacleList, const std::vector<size_t>& indexList, float threshold, std::vector<std::vector<size_t>>& groups);
inline void groupObstacleByS(const std::vector<Obstacle>& obstacleList, float threshold, std::vector<std::vector<size_t>>& groups)
{
	std::vector<size_t> consideredIdx; // 按 S 分组时关心的索引有哪些，默认所有索引都关心
	for (size_t i = 0, ie = obstacleList.size(); i < ie; ++i) consideredIdx.push_back(i);
	groupObstacleByS(obstacleList, consideredIdx, threshold, groups);
}

extern void getObstacleLaneSidePosition(const SSD::SimPoint3D& point, SSD::SimPoint3D& leftPoint, SSD::SimPoint3D& rightPoint);
extern void getObstacleRoadSidePosition(const SSD::SimPoint3D& point, SSD::SimPoint3D& leftPoint, SSD::SimPoint3D& rightPoint);

inline void getObstacleLaneSidePosition(const SimOne_Data_Obstacle_Entry& obstacle, SSD::SimPoint3D& leftPoint, SSD::SimPoint3D& rightPoint)
{
	SSD::SimPoint3D obstaclePos(obstacle.posX, obstacle.posY, obstacle.posZ);
	getObstacleLaneSidePosition(obstaclePos, leftPoint, rightPoint);
}

inline void getObstacleRoadSidePosition(const SimOne_Data_Obstacle_Entry& obstacle, SSD::SimPoint3D& leftPoint, SSD::SimPoint3D& rightPoint)
{
	SSD::SimPoint3D obstaclePos(obstacle.posX, obstacle.posY, obstacle.posZ);
	getObstacleRoadSidePosition(obstaclePos, leftPoint, rightPoint);
}

extern SSD::SimVector<HDMapStandalone::MObject> crosswalkList;
extern bool isObstacleAmongCrosswalk(const SSD::SimPoint3D& tl, const SSD::SimPoint3D& tr, const SSD::SimPoint3D& bl, const SSD::SimPoint3D& br, const SSD::SimPoint3D& vertex1 = crosswalkList[0].boundaryKnots[0], const SSD::SimPoint3D& vertex2 = crosswalkList[0].boundaryKnots[2]);

extern float lessThanLaneFactor; // 如果障碍物的宽度小于该因子乘以车道宽度，则主车遭遇该障碍物需要变道
extern float moreThanRoadFactor; // 如果障碍物的宽度大于该因子乘以道路宽度，则主车遭遇该障碍物需要停走

// 主车遭遇该障碍物时是否需要变道，是否需要创建半停止线
#define NEED_TO_CHANGE_LANES(OBSTACLE) ([](const Obstacle& obstacle) { if (obstacle.width < lessThanLaneFactor * obstacle.laneWidth) return true; if (obstacle.width > moreThanRoadFactor * obstacle.roadWidth) return false; return true; }(OBSTACLE))
#define IS_SINGLE_STOPLINE NEED_TO_CHANGE_LANES

// 计算到主车最近的障碍物
extern bool calculateNearestObstacle(const std::vector<Obstacle>& list, Obstacle& obstacle, double& distance);
extern void findObstacleBehind(const std::vector<Obstacle>& list, std::vector<size_t>& behindIndexList);
extern bool findNearestObstacleBehind(const std::vector<Obstacle>& list, const std::vector<size_t>& indexList, size_t& index, float& distance);

// brief:计算跟车的速度
// param[in] obstacleList障碍物列表
// param[in] index 场景编号
// param[in] adaptiveFollowing 跟车类的实例化对象
extern float caculateFollowingSpeed(const std::vector<Obstacle>& obstacleList, int index);

// brief 计算超车速度
extern void calculateOvertakingSpeed(std::vector<Obstacle>& obstacleList);
