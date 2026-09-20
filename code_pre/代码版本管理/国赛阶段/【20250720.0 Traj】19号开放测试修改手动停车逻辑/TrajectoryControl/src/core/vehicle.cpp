#include "UtilMath.h"
#include "UtilDriver.h"
#include "utility.h"
#include "controller.hpp"
#include "vehicle.h"

// 默认构造
MainVehicle::MainVehicle()
{
	pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
	pControl->throttle = 0.0f; // 速度 m/s
	pControl->steering = 0.0f; // 打角
	pControl->brake = 0.0f; // 刹车
	pControl->handbrake = false; // 手刹
	pControl->isManualGear = false; // 是否手动挡
	pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral; // 挡位
	pLight->signalLights = ESimOne_Signal_Light::ESimOne_Signal_Light_None; // 车灯
	s = t = 0;

	id = "0";
	reverse = false;
	leftLaneExist = rightLaneExist = false;
	useDefaultPath = true;
	isTwoSideRoad = false;
	speed = vx = vy = 0.0f;
	roll = pitch = yaw = 0.0f;
	steeringOffset = 0.0f;
	laneAzimuth = 0.0f;
	pt = SSD::SimPoint3D(0.0, 0.0, 0.0);
	laneID = "";
	lastLaneID = "";
	nextLaneID = "";
}

// 更新主车详细参数
bool MainVehicle::update(int timeoutFrames)
{
	// 初始化控制和车灯
	pControl->throttle = caseTargetSpeed; // 速度
	pControl->steering = 0.0f; // 打角
	pControl->handbrake = false; // 手刹
	pControl->isManualGear = false; // 是否手动挡
	pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive; // 挡位：前进挡
	pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
	pLight->signalLights = presetLight; // 车灯

	// 获取主车 GPS 信息
	if (!SimOneAPI::GetGps(id, pGps.get()) && frameCount < timeoutFrames) {
		globalLogger(Logger::Color::BrightMagenta) << "正在获取主车 GPS 信息 ...";
		//std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	else
	{
		if (!FlagType::isMainVehicleInitialized)
		{
			FlagType::isMainVehicleInitialized = true;
			globalLogger(Logger::Color::BrightMagenta) << "主车 GPS 初始化成功";
		}
	}

	// 计算主车的详细参数
	pt = SSD::SimPoint3D(pGps->posX, pGps->posY, pGps->posZ); // 主车坐标
	if (frameCount == 1) // 第一帧的时候让上次和本次的主车道路 ID 相同
	{
		lastLaneID = laneID = m_SampleGetNearMostLane(pt);
	}
	else
	{
		lastLaneID = laneID; // 上一帧主车所在的道路 ID
		laneID = m_SampleGetNearMostLane(pt); // 主车所在的道路 ID
	}
	SimOneAPI::GetLaneST(laneID, pt, s, t);
	laneAzimuth = getLaneAzimuth(laneID); // 主车所在道路的方位角

	vx = pGps->velX;
	vy = pGps->velY;
	speed = UtilMath::calculateSpeed(vx, vy);
	roll = pGps->oriX;
	pitch = pGps->oriY;
	yaw = pGps->oriZ;
	steeringOffset = 0.0f;
	useDefaultPath = true;

	std::string strLaneID(laneID.GetString());
	isTwoSideRoad = SimOneAPI::IsTwoSideRoad(std::stoi(strLaneID.substr(0, strLaneID.find('_'))));

	// 获取主车的左右邻接车道情况
	leftLaneExist = rightLaneExist = false;
	SimOneAPI::GetLaneLink(laneID, laneLink); // 获取主车所在的车道的其他邻接车道
	if (strlen(laneLink.leftNeighborLaneName.GetString()) != 0) // 存在左邻接车道
	{
		leftLaneExist = true;
	}
	if (strlen(laneLink.rightNeighborLaneName.GetString()) != 0) // 存在右邻接车道
	{
		rightLaneExist = true;
	}

	/*LOG << "主车当前位置为：" << pt.x << ", " << pt.y << ", " << "所在车道为：" << laneID.GetString();
	LOG << "predecessorLaneNameList.size = " << laneLink.predecessorLaneNameList.size();
	LOG << "successorLaneNameList.size = " << laneLink.successorLaneNameList.size();

	for (size_t i = 0, ie = laneLink.predecessorLaneNameList.size(); i < ie; ++i) LOG << "predecessorLaneNameList[" << i << "] = " << laneLink.predecessorLaneNameList[i].GetString();
	for (size_t i = 0, ie = laneLink.successorLaneNameList.size(); i < ie; ++i)
	{
		LOG << "successorLaneNameList[" << i << "] = " << laneLink.successorLaneNameList[i].GetString();

		long temp;
		LOG << "successorLaneNameList[" << i << "] 是否在路口内：" << (SimOneAPI::IsInJunction(laneLink.successorLaneNameList[i], temp) == true ? "true" : "false");

	}*/

	if (laneLink.successorLaneNameList.empty()) nextLaneID = "none";
	else
	{
		long tempJungleID = 0;
		std::vector<SSD::SimString> notJungleLane;
		for (size_t i = 0, ie = laneLink.successorLaneNameList.size(); i < ie; ++i)
		{
			if (!SimOneAPI::IsInJunction(laneLink.successorLaneNameList[i], tempJungleID))
			{
				notJungleLane.push_back(laneLink.successorLaneNameList[i]);
			}
		}

		ASSERT(notJungleLane.size() < 2, "后继道路中不是路口的道路大于一条");

		if (notJungleLane.empty()) nextLaneID = "none";
		else
		{
			nextLaneID = notJungleLane[0];
		}
	}

	// 获取各障碍物相对于主车的邻域情况，第一帧的时候障碍物对象还没有被创建，邻域对象亦为空
	static constexpr float OFFSET = 1.0f;
	neighborhood.clear();
	for (size_t i = 0, ie = obstacleList.size(); i < ie; ++i)
	{
		if (!isSameRoadId(obstacleList[i].laneID, mainVehicle.laneID)) continue;

		// 障碍物在前邻域
		if (obstacleList[i].sRelativeToVehicle - mainVehicle.s > 0.5f * (MAIN_VEHICLE_LENGTH + OFFSET) &&
			std::abs(obstacleList[i].tRelativeToVehicle - mainVehicle.t) <= 0.5f * (MAIN_VEHICLE_WIDTH + OFFSET))
		{
			neighborhood.front.push_back(i);
		}

		// 障碍物在后邻域
		else if (obstacleList[i].sRelativeToVehicle - mainVehicle.s < -0.5f * (MAIN_VEHICLE_LENGTH + OFFSET) &&
			std::abs(obstacleList[i].tRelativeToVehicle - mainVehicle.t) <= 0.5f * (MAIN_VEHICLE_WIDTH + OFFSET))
		{
			neighborhood.back.push_back(i);
		}

		// 障碍物在左邻域
		else if (obstacleList[i].tRelativeToVehicle - mainVehicle.t > 0.5f * (MAIN_VEHICLE_WIDTH + OFFSET) &&
			std::abs(obstacleList[i].sRelativeToVehicle - mainVehicle.s) <= 0.5f * (MAIN_VEHICLE_LENGTH + OFFSET))
		{
			neighborhood.left.push_back(i);
		}

		// 障碍物在右邻域
		else if (obstacleList[i].tRelativeToVehicle - mainVehicle.t < -0.5f * (MAIN_VEHICLE_WIDTH + OFFSET) &&
			std::abs(obstacleList[i].sRelativeToVehicle - mainVehicle.s) <= 0.5f * (MAIN_VEHICLE_LENGTH + OFFSET))
		{
			neighborhood.right.push_back(i);
		}

		// 障碍物在左前邻域
		else if (obstacleList[i].sRelativeToVehicle - mainVehicle.s > 0.5f * (MAIN_VEHICLE_LENGTH + OFFSET) &&
			obstacleList[i].tRelativeToVehicle - mainVehicle.t > 0.5f * (MAIN_VEHICLE_WIDTH + OFFSET))
		{
			neighborhood.leftFront.push_back(i);
		}

		// 障碍物在右前邻域
		else if (obstacleList[i].sRelativeToVehicle - mainVehicle.s > 0.5f * (MAIN_VEHICLE_LENGTH + OFFSET) &&
			obstacleList[i].tRelativeToVehicle - mainVehicle.t < -0.5f * (MAIN_VEHICLE_WIDTH + OFFSET))
		{
			neighborhood.rightFront.push_back(i);
		}

		// 障碍物在左后邻域
		else if (obstacleList[i].sRelativeToVehicle - mainVehicle.s < -0.5f * (MAIN_VEHICLE_LENGTH + OFFSET) &&
			obstacleList[i].tRelativeToVehicle - mainVehicle.t > 0.5f * (MAIN_VEHICLE_WIDTH + OFFSET))
		{
			neighborhood.leftBack.push_back(i);
		}

		// 障碍物在右后邻域
		else if (obstacleList[i].sRelativeToVehicle - mainVehicle.s < -0.5f * (MAIN_VEHICLE_LENGTH + OFFSET) &&
			obstacleList[i].tRelativeToVehicle - mainVehicle.t < -0.5f * (MAIN_VEHICLE_WIDTH + OFFSET))
		{
			neighborhood.rightBack.push_back(i);
		}
	}

	// 对每个邻域内的障碍物按速度从大到小排序
	neighborhood.sort(neighborhood.front);
	neighborhood.sort(neighborhood.back);
	neighborhood.sort(neighborhood.left);
	neighborhood.sort(neighborhood.right);
	neighborhood.sort(neighborhood.leftFront);
	neighborhood.sort(neighborhood.rightFront);
	neighborhood.sort(neighborhood.leftBack);
	neighborhood.sort(neighborhood.rightBack);

	return FlagType::isMainVehicleInitialized;
}

// 更新主车控制参数
void MainVehicle::drive(void)
{
	if (useDefaultPath) // 如果使用默认路径，PID 控制器结合纯追踪算法处理目标路径，计算前轮打角
	{
		size_t carIndex;
		double curvature;
		double steering = UtilDriver::calculateSteering(targetPath, pGps.get(), carIndex); // 路径纯追踪控制打角

		if(FlagType::isManualTrackMode) // 只有在手动打点模式下，进行增益
		{
			int forwardIndex = 30;	// 取前方多少个点
			int backIndex = 20;		// 取后方多少个点

			// 根据索引，取出一段距离的轨迹，计算曲率
			size_t endIndex = std::min(carIndex + forwardIndex, targetPath.size());
			size_t startIndex = std::max(size_t(0), carIndex - backIndex);
			if (carIndex >= targetPath.size()) curvature = 0.0;
			else {
				std::vector<SSD::SimPoint3D> path(targetPath.begin() + carIndex, targetPath.begin() + endIndex);
				curvature = approximateCurvature(path);
				SSD::SimPoint3D &point = targetPath[endIndex-1];
				LOG << "曲线终点位置：" << point;
			}
			LOG << "后方 " << (carIndex - startIndex) << " 个点到前方 " << (endIndex - carIndex) << " 个点的轨迹的曲率：" << curvature;

			// 根据速度和曲率得到增益
			float speedGain = 0.08;
			float curveGain = 30;
			LOG << "增益前的 Kp：" << steerKpUse;
			steerKpUse = steerKpUse + speedGain * mainVehicle.speed + curveGain * curvature;
			LOG << "速度增益：" << speedGain * mainVehicle.speed << "，曲率增益：" << curveGain * curvature << "，增益后 Kp：" << steerKpUse;
		}

		steerPID.set(steerKpUse, steerKi, steerKdUse);
		pControl->steering = steerPID.calculate(steering) - steeringOffsetKp * steeringOffset;
	}

	if (reverse) /* 倒车模式 */
	{
		pControl->throttle = 3.0f;
		pControl->steering = 0.0f;
		pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse;
	}

	for (const auto& [start, end] : slideConfig) /* 检查溜车配置 */
	{
		static bool isSliding = false; // 当前是否处于滑行状态

		/* 如果当前不处于溜车状态，且已到达溜车起点 */
		if (!isSliding && UtilMath::planarDistance(mainVehicle.pt, start) < achieveThres) isSliding = true;
		/* 如果当前正处于溜车状态，且已到达溜车终点 */
		else if (isSliding && UtilMath::planarDistance(mainVehicle.pt, end) < achieveThres) isSliding = false;

		/* 根据是否处于溜车状态来决定挡位 */
		if (isSliding) pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral;
		else pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
	}

	SimOneAPI::SetSignalLights(id, pLight.get()); // 设置车辆的转向灯
	SimOneAPI::SetDrive(id, pControl.get()); // 设置车辆的油门打角
}

// 清除所有邻域信息
void Neighborhood::clear(void)
{
	vf = vb = vl = vr = vlf = vrf = vlb = vrb = 0.0f;
	bf = bb = bl = br = blf = brf = blb = brb = false;

	front.clear();
	back.clear();
	left.clear();
	right.clear();
	leftFront.clear();
	rightFront.clear();
	leftBack.clear();
	rightBack.clear();
}

// 按速度从大到小对 arr 邻域内的元素进行排列
void Neighborhood::sort(std::vector<size_t>& arr)
{
	std::sort(arr.begin(), arr.end(), [](auto& a, auto& b) { return obstacleList[a].velocityPlanar > obstacleList[b].velocityPlanar; });
}

// 从后向前查找第一个速度不为 0 的障碍物
const Obstacle* Neighborhood::findSlowestMovingObstacle(const std::vector<size_t>& obstacleIndices, float speedThreshold)
{
	for (auto it = obstacleIndices.rbegin(); it != obstacleIndices.rend(); ++it)
	{
		const Obstacle& obstacle = obstacleList[*it];
		if (obstacle.velocityPlanar > speedThreshold)
		{
			return &obstacle;
		}
	}

	return nullptr; // 注意！如果该邻域中全是静止的障碍物，就会返回一个空指针
}

const Obstacle* Neighborhood::findFastestMovingObstacle(const std::vector<size_t>& obstacleIndices, float speedThreshold)
{
	for (size_t i = 0; i < obstacleIndices.size(); ++i)
	{
		const Obstacle& obstacle = obstacleList[obstacleIndices[i]];
		if (obstacle.velocityPlanar > speedThreshold)
		{
			return &obstacle;
		}
	}

	return nullptr;
}