#include <unordered_set>
#include "UtilMath.h"
#include "function.h"
#include "define.h"
#include "process.h"
#include "obstacle.h"

StaticObstacle::StaticObstacle() {}
StaticObstacle::StaticObstacle(const StaticObstacle& other) : id(other.id), posX(other.posX), posY(other.posY), posZ(other.posZ), length(other.length), width(other.width), height(other.height), tlPos(other.tlPos), trPos(other.trPos), blPos(other.blPos), brPos(other.brPos), isLaneChangeObstacle(other.isLaneChangeObstacle) {}

StaticObstacle::StaticObstacle(int _id, float _posZ, float _height, const SSD::SimPoint2D& _tlPos, const SSD::SimPoint2D& _brPos) : id(_id), posZ(_posZ), height(_height), tlPos(_tlPos), brPos(_brPos)
{
	//float azimuth = calculateResultantAzimuth(pGps->velX, pGps->accelY);
	float azimuth = getLaneAzimuth(mainVehicleLaneId);

	if ((0 <= azimuth && azimuth <= 45 || 315 < azimuth <= 360)) // 主车向东运动，道路是东西方向的
	{
		trPos = SSD::SimPoint2D(tlPos.x, brPos.y); // 右上
		blPos = SSD::SimPoint2D(brPos.x, tlPos.y); // 左下
		width = tlPos.y - brPos.y;
		length = tlPos.x - brPos.x;
	}
	else if (135 < azimuth && azimuth <= 225) // 主车向西运动，道路是东西方向的
	{
		trPos = SSD::SimPoint2D(tlPos.x, brPos.y); // 右上
		blPos = SSD::SimPoint2D(brPos.x, tlPos.y); // 左下
		width = brPos.y - tlPos.y;
		length = brPos.x - tlPos.x;
	}
	else if (45 < azimuth && azimuth <= 135) // 主车向北运动，道路是南北方向的
	{
		trPos = SSD::SimPoint2D(brPos.x, tlPos.y); // 右上
		blPos = SSD::SimPoint2D(tlPos.x, brPos.y); // 左下
		width = brPos.x - tlPos.x;
		length = tlPos.y - brPos.y;
	}
	else // 主车向南运动，道路是南北方向的
	{
		trPos = SSD::SimPoint2D(brPos.x, tlPos.y); // 右上
		blPos = SSD::SimPoint2D(tlPos.x, brPos.y); // 左下
		width = tlPos.x - brPos.x;
		length = brPos.y - tlPos.y;
	}

	posX = 0.5f * (tlPos.x + brPos.x);
	posY = 0.5f * (tlPos.y + brPos.y);

	//LOG << "tl = " << _tlPos.x << ", " << _tlPos.y;
	//LOG << "br = " << _brPos.x << ", " << _brPos.y;

	isLaneChangeObstacle = isLaneChangeObs(tlPos, brPos, posX, posY, posZ); // 判断当前障碍物是否是 lane change 障碍物
}

StaticObstacle::StaticObstacle(const SimOne_Data_Obstacle_Entry& obstacle) : id(obstacle.id), posX(obstacle.posX), posY(obstacle.posY), posZ(obstacle.posZ), length(obstacle.length), width(obstacle.width), height(obstacle.height)
{
	calculateObstacleCorners(obstacle, tlPos, trPos, blPos, brPos); // 计算俯视图的四个角点
	isLaneChangeObstacle = isLaneChangeObs(obstacle); // 判断当前障碍物是否是 lane change 障碍物
}

// 图的深度优先搜索
static void graphDFS(size_t index, const std::vector<std::vector<size_t>>& adjacencyList, std::unordered_set<size_t>& visited, std::vector<size_t>& group)
{
	visited.insert(index);
	group.push_back(index);

	for (size_t neighbor : adjacencyList[index]) {
		if (visited.find(neighbor) == visited.end()) {
			graphDFS(neighbor, adjacencyList, visited, group);
		}
	}
}

void groupObstacleByDist(const std::unique_ptr<SimOne_Data_Obstacle>& pObstacleList, float threshold, std::vector<std::vector<size_t>>& groups)
{
	groups.clear();
	size_t n = pObstacleList->obstacleSize;
	std::vector<std::vector<size_t>> adjacencyList(n);
	Logger obstacleLog("obstacle.cpp/groupObstacleByDist", Logger::Color::BrightYellow);

	// 基于距离阈值构建邻接矩阵
	for (size_t i = 0; i < n; ++i)
	{
		for (size_t j = i + 1; j < n; ++j)
		{
			if (pObstacleList->obstacle[i].type == pObstacleList->obstacle[j].type &&
				UtilMath::calculateSpeed(pObstacleList->obstacle[i].velX, pObstacleList->obstacle[i].velY) < 1e-2 &&
				UtilMath::calculateSpeed(pObstacleList->obstacle[j].velX, pObstacleList->obstacle[j].velY) < 1e-2 &&
				UtilMath::distance(pObstacleList->obstacle[i].posX, pObstacleList->obstacle[i].posY, pObstacleList->obstacle[j].posX, pObstacleList->obstacle[j].posY) <= threshold)
			{
				adjacencyList[i].push_back(j);
				adjacencyList[j].push_back(i);
			}
		}
	}

	// 使用图的深度优先搜索
	std::unordered_set<size_t> visited;
	for (size_t i = 0; i < n; ++i)
	{
		if (visited.find(i) == visited.end())
		{
			std::vector<size_t> group;
			graphDFS(i, adjacencyList, visited, group);
			groups.push_back(group);
		}
	}

	// 输出调试信息
	for (size_t i = 0; i < groups.size(); ++i) {
		int j = 0;
		for (size_t idx : groups[i]) {
			obstacleLog(Logger::Color::BrightMagenta) << "Group " << i << " - " << j++ << " : " << idx << " ";
		}
	}
}

void groupObstacleByDist(const std::vector<size_t>& indexList, float threshold, std::vector<std::vector<size_t>>& groups)
{
	groups.clear();
	size_t n = indexList.size();
	std::vector<std::vector<size_t>> adjacencyList(n);
	Logger obstacleLog("obstacle.cpp/groupObstacleByDist", Logger::Color::BrightYellow);

	// 基于距离阈值构建邻接矩阵
	for (size_t i = 0; i < n; ++i)
	{
		for (size_t j = i + 1; j < n; ++j)
		{
			size_t idx1 = indexList[i];
			size_t idx2 = indexList[j];

			if (pObstacle->obstacle[idx1].type == pObstacle->obstacle[idx2].type &&
				UtilMath::calculateSpeed(pObstacle->obstacle[idx1].velX, pObstacle->obstacle[idx1].velY) < 1e-2 &&
				UtilMath::calculateSpeed(pObstacle->obstacle[idx2].velX, pObstacle->obstacle[idx2].velY) < 1e-2 &&
				UtilMath::distance(pObstacle->obstacle[idx1].posX, pObstacle->obstacle[idx1].posY, pObstacle->obstacle[idx2].posX, pObstacle->obstacle[idx2].posY) <= threshold)
			{
				adjacencyList[i].push_back(j);
				adjacencyList[j].push_back(i);
			}
		}
	}

	// 使用图的深度优先搜索
	std::unordered_set<size_t> visited;
	for (size_t i = 0; i < n; ++i)
	{
		if (visited.find(indexList.at(i)) == visited.end())
		{
			std::vector<size_t> group;
			graphDFS(i, adjacencyList, visited, group);
			
			// group 里存储的是 `indexList` 的索引，需要转换成原始索引
			for (size_t& idx : group) idx = indexList[idx];
			groups.push_back(group);
		}
	}

	// 输出调试信息
	for (size_t i = 0; i < groups.size(); ++i) {
		int j = 0;
		for (size_t idx : groups[i]) {
			obstacleLog(Logger::Color::BrightMagenta) << "Group " << i << " - " << j++ << " : " << idx << " ";
		}
	}
}

SSD::SimPoint2D calculateObstacleTL(const SimOne_Data_Obstacle_Entry& obstacle) // 左上
{
	float halfWidth = 0.5f * obstacle.width;
	float halfLength = 0.5f * obstacle.length;
	//float azimuth = calculateResultantAzimuth(pGps->velX, pGps->accelY);
	float azimuth = getLaneAzimuth(mainVehicleLaneId);

	if (0 <= azimuth && azimuth <= 45 || 315 < azimuth <= 360) // 主车向东运动，道路是东西方向的
	{
		return SSD::SimPoint2D(obstacle.posX + halfLength, obstacle.posY + halfWidth);
	}
	else if (135 < azimuth && azimuth <= 225) // 主车向西运动，道路是东西方向的
	{
		return SSD::SimPoint2D(obstacle.posX - halfLength, obstacle.posY - halfWidth);
	}
	else if (45 < azimuth && azimuth <= 135) // 主车向北运动，道路是南北方向的
	{
		return SSD::SimPoint2D(obstacle.posX - halfWidth, obstacle.posY + halfLength);
	}
	else if (225 < azimuth && azimuth <= 315) // 主车向南运动，道路是南北方向的
	{
		return SSD::SimPoint2D(obstacle.posX + halfWidth, obstacle.posY - halfLength);
	}
}

SSD::SimPoint2D calculateObstacleTR(const SimOne_Data_Obstacle_Entry& obstacle) // 右上
{
	float halfWidth = 0.5f * obstacle.width;
	float halfLength = 0.5f * obstacle.length;
	//float azimuth = calculateResultantAzimuth(pGps->velX, pGps->accelY);
	float azimuth = getLaneAzimuth(mainVehicleLaneId);

	if (0 <= azimuth && azimuth <= 45 || 315 < azimuth <= 360) // 主车向东运动，道路是东西方向的
	{
		return SSD::SimPoint2D(obstacle.posX + halfLength, obstacle.posY - halfWidth);
	}
	else if (135 < azimuth && azimuth <= 225) // 主车向西运动，道路是东西方向的
	{
		return SSD::SimPoint2D(obstacle.posX - halfLength, obstacle.posY + halfWidth);
	}
	else if (45 < azimuth && azimuth <= 135) // 主车向北运动，道路是南北方向的
	{
		return SSD::SimPoint2D(obstacle.posX + halfWidth, obstacle.posY + halfLength);
	}
	else if (225 < azimuth && azimuth <= 315) // 主车向南运动，道路是南北方向的
	{
		return SSD::SimPoint2D(obstacle.posX - halfWidth, obstacle.posY - halfLength);
	}
}

SSD::SimPoint2D calculateObstacleBL(const SimOne_Data_Obstacle_Entry& obstacle) // 左下
{
	float halfWidth = 0.5f * obstacle.width;
	float halfLength = 0.5f * obstacle.length;
	//float azimuth = calculateResultantAzimuth(pGps->velX, pGps->accelY);
	float azimuth = getLaneAzimuth(mainVehicleLaneId);

	if (0 <= azimuth && azimuth <= 45 || 315 < azimuth <= 360) // 主车向东运动，道路是东西方向的
	{
		return SSD::SimPoint2D(obstacle.posX - halfLength, obstacle.posY + halfWidth);
	}
	else if (135 < azimuth && azimuth <= 225) // 主车向西运动，道路是东西方向的
	{
		return SSD::SimPoint2D(obstacle.posX + halfLength, obstacle.posY - halfWidth);
	}
	else if (45 < azimuth && azimuth <= 135) // 主车向北运动，道路是南北方向的
	{
		return SSD::SimPoint2D(obstacle.posX - halfWidth, obstacle.posY - halfLength);
	}
	else if (225 < azimuth && azimuth <= 315) // 主车向南运动，道路是南北方向的
	{
		return SSD::SimPoint2D(obstacle.posX + halfWidth, obstacle.posY + halfLength);
	}
}

SSD::SimPoint2D calculateObstacleBR(const SimOne_Data_Obstacle_Entry& obstacle) // 右下
{
	float halfWidth = 0.5f * obstacle.width;
	float halfLength = 0.5f * obstacle.length;
	//float azimuth = calculateResultantAzimuth(pGps->velX, pGps->accelY);
	float azimuth = getLaneAzimuth(mainVehicleLaneId);

	if (0 <= azimuth && azimuth <= 45 || 315 < azimuth <= 360) // 主车向东运动，道路是东西方向的
	{
		return SSD::SimPoint2D(obstacle.posX - halfLength, obstacle.posY - halfWidth);
	}
	else if (135 < azimuth && azimuth <= 225) // 主车向西运动，道路是东西方向的
	{
		return SSD::SimPoint2D(obstacle.posX + halfLength, obstacle.posY + halfWidth);
	}
	else if (45 < azimuth && azimuth <= 135) // 主车向北运动，道路是南北方向的
	{
		return SSD::SimPoint2D(obstacle.posX + halfWidth, obstacle.posY - halfLength);
	}
	else if (225 < azimuth && azimuth <= 315) // 主车向南运动，道路是南北方向的
	{
		return SSD::SimPoint2D(obstacle.posX - halfWidth, obstacle.posY + halfLength);
	}
}

void findBoundingBox(const std::vector<SSD::SimPoint2D>& obsTL, const std::vector<SSD::SimPoint2D>& obsBR, SSD::SimPoint2D& minTL, SSD::SimPoint2D& maxBR)
{
	if (obsTL.empty() || obsBR.empty()) {
		return; // 直接返回，避免访问空向量
	}

	for (int i = 0, ie = obsTL.size(); i < ie; ++i)
	{
		//LOG << "obsTL[" << i << "] = ( " << obsTL[i].x << ", " << obsTL[i].y << " )";
	}

	for (int i = 0, ie = obsBR.size(); i < ie; ++i)
	{
		//LOG << "obsBR[" << i << "] = ( " << obsBR[i].x << ", " << obsBR[i].y << " )";
	}

	// 获取主车的方位角
	//float azimuth = calculateResultantAzimuth(pGps->velX, pGps->accelY);
	float azimuth = getLaneAzimuth(mainVehicleLaneId);
	//LOG << "azimuth = " << azimuth;
	//LOG << "std::numeric_limits<float>::max() = " << std::numeric_limits<float>::max();
	//LOG << "std::numeric_limits<float>::lowest()" << std::numeric_limits<float>::lowest();

	// 初始化 minTL 和 maxBR
	if ((0 <= azimuth && azimuth <= 45) || (315 < azimuth && azimuth <= 360)) { // 主车向东运动，道路是东西方向的
		// minTL 取 x 最大, y 最大；maxBR 取 x 最小, y 最小
		minTL = SSD::SimPoint2D(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
		maxBR = SSD::SimPoint2D(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	}
	else if (135 < azimuth && azimuth <= 225) { // 主车向西运动，道路是东西方向的
		// minTL 取 x 最小, y 最小；maxBR 取 x 最大, y 最大
		minTL = SSD::SimPoint2D(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		maxBR = SSD::SimPoint2D(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
	}
	else if (45 < azimuth && azimuth <= 135) { // 主车向北运动，道路是南北方向的
		// minTL 取 x 最小, y 最大；maxBR 取 x 最大, y 最小
		minTL = SSD::SimPoint2D(std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest());
		maxBR = SSD::SimPoint2D(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max());
	}
	else if (225 < azimuth && azimuth <= 315) { // 主车向南运动，道路是南北方向的
		// minTL 取 x 最大, y 最小；maxBR 取 x 最小, y 最大
		minTL = SSD::SimPoint2D(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max());
		maxBR = SSD::SimPoint2D(std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest());
	}

	// 根据主车的方位角进行判断
	for (size_t i = 0; i < obsTL.size(); ++i) {
		const auto& tl = obsTL[i];
		const auto& br = obsBR[i];

		if ((0 <= azimuth && azimuth <= 45) || (315 < azimuth && azimuth <= 360)) { // 主车向东运动，道路是东西方向的
			// minTL 取 x 最大, y 最大
			if (tl.x > minTL.x) minTL.x = tl.x;
			if (tl.y > minTL.y) minTL.y = tl.y;

			// maxBR 取 x 最小, y 最小
			if (br.x < maxBR.x) maxBR.x = br.x;
			if (br.y < maxBR.y) maxBR.y = br.y;
		}
		else if (135 < azimuth && azimuth <= 225) { // 主车向西运动，道路是东西方向的
			// minTL 取 x 最小, y 最小
			if (tl.x < minTL.x) minTL.x = tl.x;
			if (tl.y < minTL.y) minTL.y = tl.y;

			// maxBR 取 x 最大, y 最大
			if (br.x > maxBR.x) maxBR.x = br.x;
			if (br.y > maxBR.y) maxBR.y = br.y;
		}
		else if (45 < azimuth && azimuth <= 135) { // 主车向北运动，道路是南北方向的
			// minTL 取 x 最小, y 最大
			if (tl.x < minTL.x) minTL.x = tl.x;
			if (tl.y > minTL.y) minTL.y = tl.y;

			// maxBR 取 x 最大, y 最小
			if (br.x > maxBR.x) maxBR.x = br.x;
			if (br.y < maxBR.y) maxBR.y = br.y;
		}
		else if (225 < azimuth && azimuth <= 315) { // 主车向南运动，道路是南北方向的
			// minTL 取 x 最大, y 最小
			if (tl.x > minTL.x) minTL.x = tl.x;
			if (tl.y < minTL.y) minTL.y = tl.y;

			// maxBR 取 x 最小, y 最大
			if (br.x < maxBR.x) maxBR.x = br.x;
			if (br.y > maxBR.y) maxBR.y = br.y;
		}
	}
}

// 判断一个障碍物是否是 lane change 障碍物
bool isLaneChangeObs(const SimOne_Data_Obstacle_Entry& obstacle)
{
	SSD::SimPoint3D obstaclePos(obstacle.posX, obstacle.posY, obstacle.posZ);
	SSD::SimString obstacleLaneId = m_SampleGetNearMostLane(obstaclePos);
	double laneWidth;
	SimOneAPI::GetLaneWidth(obstacleLaneId, obstaclePos, laneWidth);
	LOG << "width = " << obstacle.width;
	LOG << "laneWidth = " << laneWidth;
	if (obstacle.width < 0.7 * laneWidth) return true; // 如果障碍物的宽度
	if (obstacle.width > 0.7 * getRoadWidth(obstacleLaneId, obstaclePos)) return false;
	return true;
}

// 判断一个障碍物是否是 lane change 障碍物
bool isLaneChangeObs(const SSD::SimPoint2D& tlPos, const SSD::SimPoint2D& brPos, float posX, float posY, float posZ)
{
	float width = 0.0f;
	//float azimuth = calculateResultantAzimuth(pGps->velX, pGps->accelY);
	float azimuth = getLaneAzimuth(mainVehicleLaneId);
	if ((0 <= azimuth && azimuth <= 45) || (315 < azimuth && azimuth <= 360)) // 主车向东运动，道路是东西方向的
	{
		width = tlPos.y - brPos.y;
		//LOG << "dong-xi";
	}
	else if (135 < azimuth && azimuth <= 225) // 主车向西运动，道路是东西方向的
	{
		width = brPos.y - tlPos.y;
		//LOG << "dong-xi";
	}
	else if (45 < azimuth && azimuth <= 135) // 主车向北运动，道路是南北方向的
	{
		width = brPos.x - tlPos.x;
		//LOG << "nan-bei";
	}
	else if (225 < azimuth && azimuth <= 315) // 主车向南运动，道路是南北方向的
	{
		width = tlPos.x - brPos.x;
		//LOG << "nan-bei";
	}
	SSD::SimPoint3D obstaclePos(posX, posY, posZ);
	SSD::SimString obstacleLaneId = m_SampleGetNearMostLane(obstaclePos);
	double laneWidth;
	SimOneAPI::GetLaneWidth(obstacleLaneId, obstaclePos, laneWidth);
	LOG << "width = " << width;
	LOG << "laneWidth = " << laneWidth;
	//LOG << "getRoadWidth(obstacleLaneId, obstaclePos) = " << getRoadWidth(obstacleLaneId, obstaclePos);
	if (width < 0.7 * laneWidth) return true; // 如果障碍物的宽度
	else if (width > 0.7 * getRoadWidth(obstacleLaneId, obstaclePos)) return false;
	else if (width < 1  *laneWidth)
		return true;
	else return false;
}