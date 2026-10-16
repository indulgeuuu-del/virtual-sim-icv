#include "UtilMath.h"
#include "UtilDriver.h"
#include "utility.h"
#include "controller.hpp"
#include "vehicle.h"
#include <stdexcept>

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
	// 上一帧索引在障碍物列表重建后失效；本帧完成列表构建后再计算邻域。
	neighborhood.clear();
	// 初始化控制和车灯
	pControl->throttle = caseTargetSpeed; // 速度
	pControl->steering = 0.0f; // 打角
	pControl->handbrake = false; // 手刹
	pControl->isManualGear = false; // 是否手动挡
	pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive; // 挡位：前进挡
	pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
	pLight->signalLights = presetLight; // 车灯

	// 获取主车 GPS 信息
	if (!SimOneAPI::GetGps(id, pGps.get()))
	{
		globalLogger(Logger::Color::BrightMagenta) << (frameCount < timeoutFrames
			? "正在获取主车 GPS 信息 ..." : "主车 GPS 更新失败，不能使用本帧数据");
		return false;
	}
	if (!FlagType::isMainVehicleInitialized)
	{
		FlagType::isMainVehicleInitialized = true;
		globalLogger(Logger::Color::BrightMagenta) << "主车 GPS 初始化成功";
	}

	// 计算主车的详细参数
	const SSD::SimPoint3D newPt(pGps->posX, pGps->posY, pGps->posZ);
	if (!std::isfinite(newPt.x) || !std::isfinite(newPt.y) || !std::isfinite(newPt.z))
	{
		globalLogger(Logger::Color::BrightMagenta) << "Invalid vehicle position: reject frame";
		return false;
	}
	// SDK failures can still modify output parameters. Commit only a complete result.
	SSD::SimString newLaneID;
	double nearS = 0, nearT = 0, centerS = 0, centerT = 0;
	if (!SimOneAPI::GetNearMostLane(newPt, newLaneID, nearS, nearT, centerS, centerT)
		|| newLaneID.Empty())
	{
		globalLogger(Logger::Color::BrightMagenta) << "Vehicle lane unavailable: reject frame";
		return false;
	}
	double newS = std::nan(""), newT = std::nan("");
	if (!SimOneAPI::GetLaneST(newLaneID, newPt, newS, newT)
		|| !std::isfinite(newS) || !std::isfinite(newT))
	{
		globalLogger(Logger::Color::BrightMagenta) << "Vehicle lane projection failed: reject frame";
		return false;
	}
	SSD::SimPoint3D laneOrigin, laneDirection(std::nan(""), std::nan(""), std::nan(""));
	if (!SimOneAPI::GetInertialFromLaneST(newLaneID, 0.0, 0.0, laneOrigin, laneDirection)
		|| !std::isfinite(laneDirection.x) || !std::isfinite(laneDirection.y)
		|| !std::isfinite(laneDirection.z) || (laneDirection.x == 0.0 && laneDirection.y == 0.0))
	{
		globalLogger(Logger::Color::BrightMagenta) << "Vehicle lane heading unavailable: reject frame";
		return false;
	}
	float newLaneAzimuth = std::atan2(laneDirection.y, laneDirection.x) * 57.29578f;
	if (newLaneAzimuth < 0) newLaneAzimuth += 360;

	// Preserve the inherited road-prefix parsing convention; malformed IDs must not exit the loop.
	const std::string strLaneID(newLaneID.GetString());
	int roadID;
	try
	{
		roadID = std::stoi(strLaneID.substr(0, strLaneID.find('_')));
	}
	catch (const std::invalid_argument&)
	{
		globalLogger(Logger::Color::BrightMagenta) << "Vehicle road ID invalid: reject frame";
		return false;
	}
	catch (const std::out_of_range&)
	{
		globalLogger(Logger::Color::BrightMagenta) << "Vehicle road ID out of range: reject frame";
		return false;
	}
	const bool newIsTwoSideRoad = SimOneAPI::IsTwoSideRoad(roadID);
	HDMapStandalone::MLaneLink newLaneLink;
	if (!SimOneAPI::GetLaneLink(newLaneID, newLaneLink))
	{
		globalLogger(Logger::Color::BrightMagenta) << "Vehicle lane links unavailable: reject frame";
		return false;
	}
	SSD::SimString newNextLaneID = "none";
	bool hasNonJunctionSuccessor = false;
	long junctionID = 0;
	for (size_t i = 0, ie = newLaneLink.successorLaneNameList.size(); i < ie; ++i)
	{
		// IsInJunction returns a property, not an API success flag.
		if (SimOneAPI::IsInJunction(newLaneLink.successorLaneNameList[i], junctionID)) continue;
		if (hasNonJunctionSuccessor)
		{
			globalLogger(Logger::Color::BrightMagenta) << "Vehicle lane successor ambiguous: reject frame";
			return false;
		}
		hasNonJunctionSuccessor = true;
		newNextLaneID = newLaneLink.successorLaneNameList[i];
	}

	// Commit a complete map snapshot only; a failed query keeps the previous valid state.
	lastLaneID = (frameCount == 1 || laneID.Empty()) ? newLaneID : laneID;
	laneID = newLaneID;
	pt = newPt;
	s = newS;
	t = newT;
	laneAzimuth = newLaneAzimuth; // 主车所在道路的方位角
	isTwoSideRoad = newIsTwoSideRoad;
	laneLink = newLaneLink;
	leftLaneExist = !laneLink.leftNeighborLaneName.Empty();
	rightLaneExist = !laneLink.rightNeighborLaneName.Empty();
	nextLaneID = newNextLaneID;

	vx = pGps->velX;
	vy = pGps->velY;
	speed = UtilMath::calculateSpeed(vx, vy);
	roll = pGps->oriX;
	pitch = pGps->oriY;
	yaw = pGps->oriZ;
	steeringOffset = 0.0f;
	useDefaultPath = true;

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

	return FlagType::isMainVehicleInitialized;
}

// 必须在本帧 obstacleList 构建完成后调用，直到决策结束不能再重排该列表。
void MainVehicle::rebuildNeighborhood(void)
{
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

}

// 更新主车控制参数
void MainVehicle::drive(void)
{
	// 在倒车等分支覆盖请求前检查，避免NaN/负速度被伪装成正常加速。
	if (!std::isfinite(pControl->throttle) ||
		(pControl->throttleMode == ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed && pControl->throttle < 0))
	{
		LOG << "Invalid throttle request: requesting stop";
		pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
		pControl->throttle = 0.0f;
	}
	const bool invalidDefaultPath = useDefaultPath && targetPath.size() < 2;
	if (invalidDefaultPath)
	{
		LOG << "循迹路径不足两个点：跳过转向计算并请求停车";
		pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
		pControl->throttle = 0.0f;
		pControl->steering = 0.0f;
	}
	// Speed 模式的 0 表示停车请求，不能被后续倒车速度或溜车空挡覆盖。
	// Percent 模式的 0 仅表示松油门，不能据此要求停车。
	const bool stopRequested = pControl->throttleMode == ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed
		&& pControl->throttle == 0.0f;

	if (useDefaultPath && !invalidDefaultPath) // 有效路径才允许进入纯追踪转向计算
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
		// 3m/s仅保留为旧倒车上限，不抬高上游减速请求；Percent保持百分比语义。
		if (pControl->throttleMode == ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed)
			pControl->throttle = std::min<double>(pControl->throttle, 3.0);
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

		LOG << "start：" << start << "end：" << end;
		LOG << "主车和 start 的距离：" << UtilMath::planarDistance(mainVehicle.pt, start);
		LOG << "主车和 end 的距离：" << UtilMath::planarDistance(mainVehicle.pt, end);

		/* 根据是否处于溜车状态来决定挡位 */
		if (isSliding) pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral;
		else pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
	}
	LOG << "主车位置：" << mainVehicle.pt;

	if (caseIdx == 33)
	{
		pControl->steering = 0;
	}

	if (stopRequested)
	{
		if (pControl->throttle != 0.0f || pControl->gear == ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral)
			LOG << "停车请求优先：覆盖倒车速度或溜车空挡";
		pControl->throttle = 0.0f;
		pControl->gear = reverse ? ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse
			: ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
	}

	// 配置上限单位为m/s；必须在倒车/溜车分支之后执行，Percent模式不作速度解释。
	if (pControl->throttleMode == ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed)
	{
		const double speedCeiling = std::isfinite(caseMaxSpeed) && caseMaxSpeed >= 0 ? caseMaxSpeed : 0;
		if (!std::isfinite(caseMaxSpeed) || caseMaxSpeed < 0)
			LOG << "Invalid caseMaxSpeed: requesting stop";
		if (pControl->throttle > speedCeiling || mainVehicle.speed > speedCeiling || speedCeiling == 0)
		{
			pControl->throttle = std::min<double>(pControl->throttle, speedCeiling);
			pControl->gear = reverse ? ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse
				: ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
			LOG << "Configured speed ceiling active: " << speedCeiling << " m/s";
		}
	}
	if (!std::isfinite(pControl->steering) || !std::isfinite(pControl->throttle))
	{
		LOG << "Invalid final control output: requesting stop";
		// 非有限转向会污染PID历史；清除后才可在有效输入恢复时继续行驶。
		steerPID.reset();
		pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
		pControl->throttle = 0.0f;
		pControl->steering = 0.0f;
		pControl->gear = reverse ? ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse
			: ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
	}
	SimOneAPI::SetSignalLights(id, pLight.get()); // 设置车辆的转向灯
	if (!SimOneAPI::SetDrive(id, pControl.get()))
		LOG << "SetDrive failed: control command was not accepted";
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
