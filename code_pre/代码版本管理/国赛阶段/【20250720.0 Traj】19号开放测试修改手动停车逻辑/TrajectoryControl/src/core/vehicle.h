#pragma once
#include "SimOneServiceAPI.h"
#include "define.h"

#define MAIN_VEHICLE_WIDTH (2.5f)
#define MAIN_VEHICLE_LENGTH (4.57f)

class Neighborhood {
public:
	// 储存的是在 obstacleList 中的索引
	std::vector<size_t> front;
	std::vector<size_t> back;
	std::vector<size_t> left;
	std::vector<size_t> right;
	std::vector<size_t> leftFront;
	std::vector<size_t> rightFront;
	std::vector<size_t> leftBack;
	std::vector<size_t> rightBack;

	float vf, vb, vl, vr, vlf, vrf, vlb, vrb;
	bool bf, bb, bl, br, blf, brf, blb, brb;

	Neighborhood(void) { clear(); }
	~Neighborhood(void) { clear(); }

	// 清除所有邻域信息
	void clear(void);

	// 按速度从大到小对 arr 邻域内的元素进行排列
	void sort(std::vector<size_t>& arr);

	// 从后向前查找第一个速度不为 0 的障碍物
	const Obstacle* findSlowestMovingObstacle(const std::vector<size_t>& obstacleIndices, float speedThreshold = 1e-2f);
	// 从前向后查找第一个速度不为 0 的障碍物
	const Obstacle* findFastestMovingObstacle(const std::vector<size_t>& obstacleIndices, float speedThreshold = 1e-2f);
};

class MainVehicle {
public:
	const char* id;
	bool leftLaneExist, rightLaneExist;
	bool useDefaultPath;
	bool reverse;
	bool isTwoSideRoad; // 主车所在的道路是否双向车道
	float speed;
	float vx, vy;
	float roll, pitch, yaw;
	float laneAzimuth; // 主车所处道路的方位角
	float steeringOffset; // 主车打角抑制量

	SSD::SimPoint3D pt;
	double s, t;
	SSD::SimString laneID;
	SSD::SimString lastLaneID;
	SSD::SimString nextLaneID; // 预测的主车即将到达的下一个道路

	HDMapStandalone::MLaneLink laneLink;
	Neighborhood neighborhood;

public:
	// 默认构造
	MainVehicle();

	// 更新主车详细参数
	bool update(int timeoutFrames = 10);

	// 更新主车控制参数
	void drive(void);
};