#include "main.h"

extern int main_obtain(void);
extern int main_manual(void);
extern int main_follow(void);
extern int main_track(void);

int main(int argc, char* argv[]) 
{
	initSimOne();
	loadJson();
	globalLogger.enable(true);

	if (argc >= 2 && (strcmp(argv[1], "obtain") == 0)) ASSERT(main_obtain() == 0, "主函数 main_obtain 意外退出");
	else if (FlagType::isManualTrackMode) ASSERT(main_manual() == 0, "主函数 main_manual 意外退出");
	else if (FlagType::isFollowingSample) ASSERT(main_follow() == 0, "主函数 main_follow 意外退出");
	else ASSERT(main_track() == 0, "主函数 main_track 意外退出");

	return 0;
}

void initSimOne(void)
{
	SimOneAPI::InitSimOneAPI(mainVehicle.id, true);
	SimOneAPI::SetDriverName(mainVehicle.id, "AutoDrive");
	SimOneAPI::SetDriveMode(mainVehicle.id, ESimOne_Drive_Mode_API);
	SimOneAPI::InitEvaluationServiceWithLocalData(mainVehicle.id);

	while (true) { // 加载 HDMap
		if (SimOneAPI::LoadHDMap(20)) {
			globalLogger(Logger::Color::BrightMagenta) << "高精度地图加载成功";
			break;
		}
		globalLogger(Logger::Color::BrightMagenta) << "高精度地图加载中";
	}

	caseIdx = getCaseIdx(); // 获取当前案例详细信息
}

void loadJson(void)
{
	nlohmann::json caseJs;
	std::ifstream(SIMONE_ADAS_DIR + "TrajectoryControl/param.json") >> caseJs;

	/* 解析预设参数 */
	JSON_LOAD_VALUE(groupingDistThres);
	JSON_LOAD_VALUE(stopLineDistThres);
	JSON_LOAD_VALUE(laneChangeDistThres);
	JSON_LOAD_VALUE(completeLaneChangeTThres);
	JSON_LOAD_VALUE(achieveThres);
	JSON_LOAD_VALUE(chasingLimitDist);
	JSON_LOAD_VALUE(followingLimitSpeed);
	JSON_LOAD_VALUE(steerKp);
	JSON_LOAD_VALUE(steerKi);
	JSON_LOAD_VALUE(steerKd);
	JSON_LOAD_VALUE(followingKp);
	JSON_LOAD_VALUE(followingKi);
	JSON_LOAD_VALUE(followingKd);
	JSON_LOAD_VALUE(steeringOffsetKp);
	JSON_LOAD_VALUE(caseStop);

	/* 解析预设参量 */
	parseStopLines(caseJs, manualStopLineReservoir); // 预设停止线
	presetLight = parseManualLight(caseJs); // 预设转向灯
	FlagType::isManualTrackMode = loadStrategyPoints(caseIdx, strategyPoint); // 预设策略点
	caseFollowing = caseJs["caseFollowing"].get<std::vector<int>>(); // 预设稳定跟车
	if (IS_IN(caseIdx, caseFollowing))
	{
		FlagType::isFollowingSample = true; // 跟车场景标志位置为 true
		globalLogger(Logger::Color::BrightMagenta) << "启用跟车场景标志位";
	}
	slideConfig = parseSlide(caseJs["slideConfig"].get<std::string>(), caseIdx); // 预设溜车配置信息

	overtakeSwitcher.load(caseJs, "overtakeSwitcher", caseIdx); // 邻域超车功能开关
	singleLaneChangeSwitcher.load(caseJs, "singleLaneChangeSwitcher", caseIdx); // 单车道变道功能开关
	mobileSingleStoplineSwitcher.load(caseJs, "mobileSingleStoplineSwitcher", caseIdx); // 移动半停止线变道功能开关
	
	/* 解析预设速度 */
	getCaseSpeed(caseJs, caseIdx, caseTargetSpeed, caseMinSpeed, caseMaxSpeed);
	globalLogger(Logger::Color::BrightMagenta) << "当前案例编号为：" << caseIdx;
	globalLogger(Logger::Color::BrightMagenta) << "当前案例目标速度为：" << caseTargetSpeed << " m/s";
	globalLogger(Logger::Color::BrightMagenta) << "当前案例限速为：" << caseMinSpeed << " m/s < speed < " << caseMaxSpeed << " m/s";
}

bool initNavigation(SSD::SimVector<int>& validWayPoints, SSD::SimPoint3DVector& targetPath)
{
	// SDK失败可能已经修改输出；完整建路成功后才替换当前路径。
	if (!SimOneAPI::GetWayPoints(mainVehicle.id, pWayPoints.get())) {
		globalLogger(Logger::Color::BrightMagenta) << "获取主车预设路径的起点和终点失败";
		return false;
	}
	SSD::SimPoint3DVector newInitialPath, newTargetPath;
	SSD::SimVector<int> newValidWayPoints;
	for (size_t i = 0; i < pWayPoints->wayPointsSize; ++i) {
		newInitialPath.push_back(SSD::SimPoint3D(pWayPoints->wayPoints[i].posX, pWayPoints->wayPoints[i].posY, 0));
	}

	// 根据路径点数量分情况处理
	if (pWayPoints->wayPointsSize >= 2) { // 大于等于 2 个路径点，调用 GenerateRoute 生成目标路径
		if (!SimOneAPI::GenerateRoute(newInitialPath, newValidWayPoints, newTargetPath)) {
			globalLogger(Logger::Color::BrightMagenta) << "使用 A* 生成主车路径规划失败";
			return false;
		}
	}
	else if (pWayPoints->wayPointsSize == 1) { // 只有 1 个路径点，获取该路径点所在的车道 ID 和车道信息
		SSD::SimString laneIdInit = m_SampleGetNearMostLane(newInitialPath[0]);
		HDMapStandalone::MLaneInfo laneInfoInit;
		if (SimOneAPI::GetLaneSample(laneIdInit, laneInfoInit)) { // 如果获取车道信息成功
			newTargetPath = laneInfoInit.centerLine; // 设置中心线为目标路径
		}
		else { // 如果获取车道信息失败 
			globalLogger(Logger::Color::BrightMagenta) << "使用 A* 生成主车路径规划失败";
			return false;
		}
	}
	else { // 如果没有路径点，则报错
		globalLogger(Logger::Color::BrightMagenta) << "主车的路径点数量为 0";
		return false;
	}
	if (newTargetPath.size() < 2) {
		globalLogger(Logger::Color::BrightMagenta) << "主车路径少于两个点，等待有效路径";
		return false;
	}
	initialPath = newInitialPath;
	validWayPoints = newValidWayPoints;
	targetPath = newTargetPath;
	return true;
}

bool updateObstacleData(int timeoutFrames)
{
	if (!SimOneAPI::GetGroundTruth(mainVehicle.id, pObstacle.get())) {
		globalLogger(Logger::Color::BrightMagenta) << (frameCount < timeoutFrames
			? "正在获取障碍物信息 ..." : "障碍物更新失败，不能使用本帧数据");
		return false;
	}
	else
	{
		// SDK读取成功不代表数量有效；拒绝异常帧，不能当作空场景继续行驶。
		const size_t obstacleCapacity = sizeof(pObstacle->obstacle) / sizeof(pObstacle->obstacle[0]);
		if (pObstacle->obstacleSize < 0 || static_cast<size_t>(pObstacle->obstacleSize) > obstacleCapacity) {
			globalLogger(Logger::Color::BrightMagenta) << "障碍物数量超出数组范围，不能使用本帧数据";
			return false;
		}
		for (size_t i = 0; i < static_cast<size_t>(pObstacle->obstacleSize); ++i) {
			const auto& obstacle = pObstacle->obstacle[i];
			// 无效对象不能被丢弃后当作道路畅通；让调用方执行整帧停车保护。
			if (!std::isfinite(obstacle.posX) || !std::isfinite(obstacle.posY) || !std::isfinite(obstacle.posZ)
				|| !std::isfinite(obstacle.velX) || !std::isfinite(obstacle.velY) || !std::isfinite(obstacle.velZ)
				|| !std::isfinite(obstacle.oriZ) || !std::isfinite(obstacle.width) || !std::isfinite(obstacle.length)) {
				globalLogger(Logger::Color::BrightMagenta) << "障碍物位置、速度、朝向或尺寸含非有限值，不能使用本帧数据；索引=" << i;
				return false;
			}
		}
		if (!FlagType::isObstacleInitialized)
		{
			FlagType::isObstacleInitialized = true;
			globalLogger(Logger::Color::BrightMagenta) << "障碍物信息初始化成功";
		}
	}

	return FlagType::isObstacleInitialized;
}
