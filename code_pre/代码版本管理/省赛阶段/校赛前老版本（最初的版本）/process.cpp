#include <set>
#include <unordered_set>
#include <unordered_map>
#include <corecrt_math_defines.h>
#include "UtilMath.h"
#include "function.h"
#include "define.h"
#include "process.h"

// 获取一个有效的交通信号灯，满足其有效性信息至少覆盖两条道路
HDMapStandalone::MSignal getValidTrafficLight(const SSD::SimVector<long>& roadIdList)
{
	HDMapStandalone::MSignal validLight;
	SSD::SimVector<HDMapStandalone::MSignal> lightList;
	SimOneAPI::GetTrafficLightList(lightList);

	// 将 roadIdList 转换为 unordered_set 以加速查找
	std::unordered_set<long> roadIdSet(roadIdList.begin(), roadIdList.end());

	// 遍历 lightList 中的每一个信号灯对象 light，每个 light 的类型是 HDMapStandalone::MSignal
	for (auto& light : lightList)
	{
		int count = 0; // 对于每个信号灯 light，初始化一个计数器 count

		// 遍历 light.validities，它是一个 SSD::SimVector<MSignalValidity> 类型的容器
		// 一个 light.validities 存储了该信号灯在不同路段和车道上的有效性信息
		for (auto& ptValidities : light.validities)
		{
			// 对于每个有效性条目 ptValidities，判断 roadId 是否在 unordered_set 中
			// 如果找到匹配的道路 ID（即 ptValidities.roadId 存在于 roadIdList 中），就增加 count 计数器
			if (roadIdSet.find(ptValidities.roadId) != roadIdSet.end())
			{
				++count;
			}
		}

		// 如果 count 达到 2 或更多，表示该信号灯的有效性信息至少有两个条目匹配了 roadIdList 中的道路 ID
		if (count >= 2) // 当前交通信号灯在至少两条道路上有效（也就是说这个交通灯要管至少两条道路的车流才有效）
		{
			// 一旦找到满足条件的信号灯 light，就将其赋值给 validLight，并跳出循环（因为只需要找到第一个符合条件的信号灯）
			validLight = light;
			break;
		}
	}

	// 函数返回找到的信号灯 validLight，如果没有找到符合条件的信号灯，validLight 会保持默认值（一个空的 MSignal 对象）
	return validLight;
}

// 获取所有有效的交通信号灯列表，满足每个信号灯的有效性信息至少覆盖两条道路
//通过道路id检测不到任何信息，返回地信号灯列表为0？？？？？？？？？？？？
void getValidTrafficLightList(const SSD::SimVector<long>& roadIdList, std::vector<HDMapStandalone::MSignal>& validLightList)
{
	validLightList.clear();
	SSD::SimVector<HDMapStandalone::MSignal> lightList;
	SimOneAPI::GetTrafficLightList(lightList);
	std::unordered_set<long> roadIdSet(roadIdList.begin(), roadIdList.end());
	for (auto& light : lightList)
	{
		int count = 0;
		for (auto& ptValidities : light.validities)
		{
			if (roadIdSet.find(ptValidities.roadId) != roadIdSet.end())
			{
				++count;
			}
		}

		if (count >= 2) // 当前交通信号灯在至少两条道路上有效（也就是说这个交通灯要管至少两条车道的车流才有效）
		{
			validLightList.push_back(light);
		}
	}
}

// 计算合速度矢方位角（ENU 坐标系下，即以正东为 0 度，逆时针为正方向）
float calculateResultantAzimuth(float xVelocity, float yVelocity)
{
	float azimuth = std::atan2(yVelocity, xVelocity) * 180.0f / M_PI;
	return azimuth < 0 ? (azimuth + 360.0f) : azimuth; // 将角度范围调整为[0, 360)
}

// 计算两个点所构成矢量的方位角（ENU 坐标系下，即以正东为 0 度，逆时针为正方向）
float calculateResultantAzimuth(float x1, float y1, float x2, float y2)
{
	float deltaX = x2 - x1;
	float deltaY = y2 - y1;
	float azimuth = std::atan2(deltaY, deltaX) * 180.0f / M_PI;
	return azimuth < 0 ? (azimuth + 360.0f) : azimuth; // 将角度范围调整为[0, 360)
}

// 计算两个点所构成矢量的方位角（ENU 坐标系下，即以正东为 0 度，逆时针为正方向）
float calculateResultantAzimuth(double x1, double y1, double x2, double y2)
{
	return calculateResultantAzimuth((float)x1, (float)y1, (float)x2, (float)y2);
}

// 判断两条道路是否具有相同的道路 Road ID
bool isSameRoadId(const SSD::SimString& roadStr1, const SSD::SimString& roadStr2)
{
	std::string str1(roadStr1.GetString());
	std::string str2(roadStr2.GetString());

	size_t pos1 = str1.find('_');
	size_t pos2 = str2.find('_');

	if (pos1 == std::string::npos || pos2 == std::string::npos)
	{
		return false; // 格式不正确，无法比较
	}
	globalLogger(Logger::Color::BrightGreen) << "str1.substr(0, pos1): " << str1.substr(0, pos1);
	globalLogger(Logger::Color::BrightGreen) << "str2.substr(0, pos2): " << str2.substr(0, pos2);

	return str1.substr(0, pos1) == str2.substr(0, pos2);
}

// 计算车辆的线性减速制动速度
float linearBrake(float speed, float dist, float initStopThres, float fullStopThres)
{
	if (dist > initStopThres) // 远离停止线，继续行驶，直接返回目标速度
	{
		return speed;
	}
	else if (dist > fullStopThres) // 进入刹车区间，逐渐刹车，线性插值计算新的速度
	{
		return speed * (dist - fullStopThres) / (initStopThres - fullStopThres);
	}
	else // 车辆完全停止
	{
		return 0.f;
	}
}

// 判断变道是否完成
bool isLaneChanged(void)
{
	if (strcmp(mainVehicleLaneId.GetString(), lastMainVehicleLaneId.GetString()) != 0) return true;
	return false;
}

// 辅助函数：递归获取邻接车道的宽度
static void getNeighborLaneWidth(const SSD::SimString& laneId, const SSD::SimPoint3D& pos, std::set<SSD::SimString>& visitedLanes, double& totalWidth)
{
	// 防止重复访问车道
	if (visitedLanes.find(laneId) != visitedLanes.end()) return;
	visitedLanes.insert(laneId);

	// 获取当前车道的宽度
	double laneWidth = 0.0;
	if (SimOneAPI::GetLaneWidth(laneId, pos, laneWidth))
	{
		totalWidth += laneWidth;
	}

	// 获取车道连接信息
	HDMapStandalone::MLaneLink linkLane;
	if (SimOneAPI::GetLaneLink(laneId, linkLane))
	{
		// 递归获取左邻接车道的宽度
		if (!linkLane.leftNeighborLaneName.Empty())
		{
			getNeighborLaneWidth(linkLane.leftNeighborLaneName, pos, visitedLanes, totalWidth);
		}

		// 递归获取右邻接车道的宽度
		if (!linkLane.rightNeighborLaneName.Empty())
		{
			getNeighborLaneWidth(linkLane.rightNeighborLaneName, pos, visitedLanes, totalWidth);
		}
	}
}

// 计算一条车道所属道路的总宽度
double getRoadWidth(const SSD::SimString& laneId, const SSD::SimPoint3D& pos)
{
	double totalWidth = 0.0;
	std::set<SSD::SimString> visitedLanes;  // 记录已经访问过的车道，避免重复计算
	getNeighborLaneWidth(laneId, pos, visitedLanes, totalWidth); // 获取当前车道及其邻接车道的宽度
	return totalWidth;  // 返回道路总宽度
}

// 计算指定车道的方位角（0~360°）
float getLaneAzimuth(const SSD::SimString& laneId)
{
	HDMapStandalone::MLaneInfo laneInfo;
	SimOneAPI::GetLaneSample(laneId, laneInfo);

	if (laneInfo.centerLine.size() < 2) {
		// 如果道路中线坐标点少于2个点，无法计算
		return 0.0f;
	}

	// 获取道路中线的首尾点
	SSD::SimPoint3D startPoint = laneInfo.centerLine.front();
	SSD::SimPoint3D endPoint = laneInfo.centerLine.back();

	// 计算首尾向量
	SSD::SimPoint3D directionVec(endPoint.x - startPoint.x, endPoint.y - startPoint.y, endPoint.z - startPoint.z);

	// 计算首尾向量的模长
	double directionVecLength = std::sqrt(directionVec.x * directionVec.x + directionVec.y * directionVec.y + directionVec.z * directionVec.z);

	// 计算正东方向的单位向量
	SSD::SimPoint3D eastVector(1.0, 0.0, 0.0);

	// 计算两个向量的点积
	double dotProduct = directionVec.x * eastVector.x + directionVec.y * eastVector.y + directionVec.z * eastVector.z;

	// 计算夹角的余弦值
	double cosTheta = dotProduct / directionVecLength;

	// 计算夹角（返回值为角度），acos 返回值是弧度，转换为度数
	float azimuth = std::acos(cosTheta) * 180.0f / static_cast<float>(M_PI);

	// 如果方向向量的y分量为负，表示夹角大于180度，需要做调整
	if (directionVec.y < 0) {
		azimuth = 360.0f - azimuth;
	}

	return azimuth;
}


// 计算向量的长度
double vectorLength(const SSD::SimPoint3D& vector)
{
	return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

// 归一化向量
SSD::SimPoint3D normalizeVector(const SSD::SimPoint3D& vector)
{
	double length = vectorLength(vector);
	if (length > 0)
	{
		return SSD::SimPoint3D(vector.x / length, vector.y / length, vector.z / length);
	}
	return SSD::SimPoint3D(0, 0, 0);  // 防止除以零
}

// 向量加法
SSD::SimPoint3D vectorAdd(const SSD::SimPoint3D& vec1, const SSD::SimPoint3D& vec2)
{
	return SSD::SimPoint3D(vec1.x + vec2.x, vec1.y + vec2.y, vec1.z + vec2.z);
}

// 向量减法
SSD::SimPoint3D vectorSubtract(const SSD::SimPoint3D& vec1, const SSD::SimPoint3D& vec2)
{
	return SSD::SimPoint3D(vec1.x - vec2.x, vec1.y - vec2.y, vec1.z - vec2.z);
}
SSD::SimPoint3D scaleVector(const SSD::SimPoint3D& vec, double scalar)
{
	return SSD::SimPoint3D(vec.x * scalar, vec.y * scalar, vec.z * scalar);
}

// 行人状态结构体
struct PedestrianState
{
	double initialT;			// 第一次出现时的 t 值
	double lastT;				// 上一次采样的 t 值
	double cumulativeDistance;  // 累计横向行走距离
	double roadWidth;			// 当前马路宽度
	bool crossed;				// 是否完成横穿
};

// 判断行人是否已经穿过马路
bool hasPedestrianCompletelyLeftRoad(const SimOne_Data_Obstacle_Entry& obstacle,double cumulative_singel, double cumulative_double)
{
	double cumulativeThreshold= cumulative_singel;
	if (FlagType::IsTwosideRoad)
		cumulativeThreshold = cumulative_double;
	LOG << "cumulativeThreshold:" << cumulativeThreshold;
	static std::unordered_map<int, PedestrianState> pedestrianStates; // 行人状态管理哈希表，键为行人唯一 ID
	double s = 0.0, t = 0.0;
	int pedID = obstacle.id;
	SSD::SimPoint3D currentPos(obstacle.posX, obstacle.posY, obstacle.posZ);
	if (!SimOneAPI::GetLaneST(mainVehicleLaneId, currentPos, s, t))
	{
		std::cout << "get st out" << std::endl;
		return false;
	} // 获取 s-t 坐标

	// 初始化行人状态（仅在第一次出现时记录初始状态）优化：if 行人由静止变成行动的时候
	if (pedestrianStates.find(pedID) == pedestrianStates.end())
	{
		PedestrianState state;
		state.initialT = t;                        // 记录初始横向位置
		state.lastT = t;                           // 初始化上一采样值
		state.cumulativeDistance = 0.0;            // 累计距离初始为 0
		state.roadWidth = getRoadWidth(mainVehicleLaneId, currentPos); // 获取当前马路宽度
		state.crossed = false;
		pedestrianStates[pedID] = state;
	}

	PedestrianState& state = pedestrianStates[pedID];// 获取当前行人的状态
	std::cout << "roadwidth" << state.roadWidth << std::endl;
	double delta = std::fabs(t - state.lastT); // 计算本次采样与上一次采样的横向位移差值
	state.cumulativeDistance += delta; // 累计横向行走距离
	//double netDisplacement = std::fabs(t - state.initialT); // 当前净偏移量

	state.lastT = t; // 更新 lastT 为当前 t 值

	// 累计横向距离达到或超过 (roadWidth * cumulativeThreshold)，则认为行人完全离开马路
	if (state.cumulativeDistance >= state.roadWidth * cumulativeThreshold) {
		state.crossed = true;
	}

	return state.crossed;
}

//将斑马线列表合成为一条斑马线，为判断行人是否在斑马线上做准备
void mergeCrosswalks(const SSD::SimVector<HDMapStandalone::MObject>& list, HDMapStandalone::MObject crosswalk)
{

}

constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

// 计算车辆相对于车道的横向速度
float getRelativeLateralVelocity(void)
{
	// 计算指定车道的方位角
	float azimuth = getLaneAzimuth(mainVehicleLaneId) * DEG_TO_RAD;

	// 车辆在 ENU 坐标系下的速度分量
	float velE = pGps->velX; // 东向速度
	float velN = pGps->velY; // 北向速度

	// 车道方向的单位向量（基于 azimuth 角）
	float laneDirX = std::cosf(azimuth);
	float laneDirY = std::sinf(azimuth);

	// 计算速度在车道方向上的投影
	return velE * laneDirX + velN * laneDirY;
}
float getRelativeLateralVelocity(const SimOne_Data_Obstacle_Entry& obstacle)
{
	// 计算指定车道的方位角
	SSD::SimPoint3D obstaclePosition(obstacle.posX, obstacle.posY, obstacle.posZ);
	float azimuth = getLaneAzimuth(m_SampleGetNearMostLane(obstaclePosition)) * DEG_TO_RAD;

	// 车辆在 ENU 坐标系下的速度分量
	float velE = obstacle.velX; // 东向速度
	float velN = obstacle.velY; // 北向速度

	// 车道方向的单位向量（基于 azimuth 角）
	float laneDirX = std::cosf(azimuth);
	float laneDirY = std::sinf(azimuth);

	// 计算速度在车道方向上的投影
	return velE * laneDirX + velN * laneDirY;
}

constexpr float LANE_CHANGE_ANGLE_THRESHOLD = 10.0f * DEG_TO_RAD; // 10° 角度阈值

// 变道检测函数
bool isPathRequireLaneChange(const SSD::SimVector<SSD::SimPoint3D>& path)
{
	if (targetPath.size() < 2) {
		return false; // 路径过短，无法判断
	}

	SSD::SimString srcLaneId = m_SampleGetNearMostLane(path[0]);
	SSD::SimString dstLaneId = m_SampleGetNearMostLane(path.back());

	if (strcmp(srcLaneId.GetString(), dstLaneId.GetString()) != 0) return true;
	return false;
}

// 计算点到直线的叉积，用于判断点的位置
static float crossProduct(const SSD::SimPoint3D& linePoint, const SSD::SimPoint3D& lineDirection, const SSD::SimPoint3D& pt) {
	// 直线的方向向量
	float dx = lineDirection.x;
	float dy = lineDirection.y;
	// 点到直线上的点的向量
	float dx1 = pt.x - linePoint.x;
	float dy1 = pt.y - linePoint.y;
	// 叉积
	return dx1 * dy - dy1 * dx;
}

// 判断轨迹起点和终点是否在同一侧
static bool arePointsOnSameSide(const SSD::SimPoint3D& pt1, const SSD::SimPoint3D& pt2, const SSD::SimPoint3D& linePoint, float azimuthDeg) {
	// 计算直线的方向向量
	float azimuth = azimuthDeg * DEG_TO_RAD;
	SSD::SimPoint3D lineDirection = { cos(azimuth), sin(azimuth), 0.0f };

	// 计算起点和终点相对于直线的位置
	float cross1 = crossProduct(linePoint, lineDirection, pt1);
	float cross2 = crossProduct(linePoint, lineDirection, pt2);

	// 判断它们是否在直线的同一侧
	return (cross1 * cross2 >= 0);  // 叉积的符号相同，表示在同一侧
}

// 主车轨迹与对手车辆轨迹是否发生干涉
bool isTrajInterfere(const SSD::SimVector<SSD::SimPoint3D>& path, const SSD::SimPoint3D& pt, float azimuth)
{
	/* 首先检查对手车辆与主车是否在同一个路段，道路 ID 形式为 roadId_sectionIndex_laneId */
	SSD::SimString ptLane = m_SampleGetNearMostLane(pt);
	if (isSameRoadId(mainVehicleLaneId, ptLane)) return true; // 如果在同一个路段，直接返回 true

	// 判断轨迹起点和终点是否在速度矢量的同一侧
	return arePointsOnSameSide(path[0], path.back(), pt, azimuth);
}

//根据目标点在主车左右来给转向灯
void IsPosOnLeftOrRight(SSD::SimPoint3D targetPos)
{
	HDMapStandalone::MSideState sideState;
	if (!SimOneAPI::IsInsideLane(targetPos, mainVehicleLaneId, sideState)) {
		pLight->signalLights = ESimOne_Signal_Light::ESimOne_Signal_Light_None;//不打灯
	}

	if (sideState == HDMapStandalone::MSideState::eLeftSide) {
		LOG << "目标车道在主车车道的左侧";
		pLight->signalLights = ESimOne_Signal_Light::ESimOne_Signal_Light_LeftBlinker;//左转灯
	}
	else if (sideState == HDMapStandalone::MSideState::eRightSide) {
		LOG << "目标车道在主车车道的右侧";
		pLight->signalLights = ESimOne_Signal_Light::ESimOne_Signal_Light_RightBlinker;//右转灯
	}
	else {
		LOG << "目标车道与主车车道重叠或无法判断";
		pLight->signalLights = ESimOne_Signal_Light::ESimOne_Signal_Light_None;//不打灯
	}
}
//根据目标车道在主车的哪一侧来设置相应的信号灯
void IsLaneOnLeftOrRight(const SSD::SimPoint3D& mainVehiclePos, const SSD::SimString& mainLaneId, const SSD::SimString& targetLaneId) {
	// 获取主车车道的 ST 坐标
	double mainS, mainT;
	if (!SimOneAPI::GetLaneST(mainLaneId, mainVehiclePos, mainS, mainT)) {
		pLight->signalLights = ESimOne_Signal_Light::ESimOne_Signal_Light_None;//不打灯
	}
	// 获取目标车道的中心点
	SSD::SimPoint3D targetLaneCenter;
	SSD::SimPoint3D targetLaneDir;
	if (!SimOneAPI::GetInertialFromLaneST(targetLaneId, mainS, 0.0, targetLaneCenter, targetLaneDir)) {
		pLight->signalLights = ESimOne_Signal_Light::ESimOne_Signal_Light_None;//不打灯
	}
	IsPosOnLeftOrRight(targetLaneCenter);//根据目标点在主车左右来给转向灯
}