#pragma once
#include <memory>
#include "SSD/SimPoint2D.h"
#include "SimOneHDMapAPI.h"

// 静止障碍物类
class StaticObstacle {
public:
	StaticObstacle();
	StaticObstacle(const StaticObstacle& other);
	StaticObstacle(int _id, float _posZ, float _height, const SSD::SimPoint2D& _tlPos, const SSD::SimPoint2D& _brPos);
	StaticObstacle(const SimOne_Data_Obstacle_Entry& obstacle);

	bool isLaneChangeObstacle = false; // 是否将当前障碍物标记为需要使主车变道的障碍物
	int id; // Obstacle global unique ID
	float posX, posY, posZ; // the centroid of the obstacle
	float length, width, height;
	SSD::SimPoint2D tlPos, trPos, blPos, brPos;
};

void groupObstacleByDist(const std::unique_ptr<SimOne_Data_Obstacle>& pObstacleList, float threshold, std::vector<std::vector<size_t>>& groups);
void groupObstacleByDist(const std::vector<size_t>& indexList, float threshold, std::vector<std::vector<size_t>>& groups);

SSD::SimPoint2D calculateObstacleTL(const SimOne_Data_Obstacle_Entry& obstacle);
SSD::SimPoint2D calculateObstacleTR(const SimOne_Data_Obstacle_Entry& obstacle);
SSD::SimPoint2D calculateObstacleBL(const SimOne_Data_Obstacle_Entry& obstacle);
SSD::SimPoint2D calculateObstacleBR(const SimOne_Data_Obstacle_Entry& obstacle);

inline void calculateObstacleCorners(const SimOne_Data_Obstacle_Entry& obstacle, SSD::SimPoint2D& tl, SSD::SimPoint2D& tr, SSD::SimPoint2D& bl, SSD::SimPoint2D& br)
{
	tl = calculateObstacleTL(obstacle);
	tr = calculateObstacleTR(obstacle);
	bl = calculateObstacleBL(obstacle);
	br = calculateObstacleBR(obstacle);
}

void findBoundingBox(const std::vector<SSD::SimPoint2D>& obsTL, const std::vector<SSD::SimPoint2D>& obsBR, SSD::SimPoint2D& minTL, SSD::SimPoint2D& maxBR);

// 判断一个障碍物是否是 lane change 障碍物
bool isLaneChangeObs(const SimOne_Data_Obstacle_Entry& obstacle);
bool isLaneChangeObs(const SSD::SimPoint2D& tlPos, const SSD::SimPoint2D& brPos, float posX, float posY, float posZ);