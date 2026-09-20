#include "main.h"
#include "opencv2/opencv.hpp"

std::string getCurrentWorkingDir() {
#ifdef _WIN32
	char buffer[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, buffer);
	return std::string(buffer);
#else
	char buffer[PATH_MAX];
	getcwd(buffer, PATH_MAX);
	return std::string(buffer);
#endif
}

#if true
int main(int argc, char* argv[]) 
{
	initSimOne(); // SimOne 初始化
	caseIdx = getCaseIdx(); // 获取当前案例详细信息
	loadJson(caseIdx); // Json 初始化
	SSD::SimVector<int> indexOfValidPoints;
	initNavigation(indexOfValidPoints, targetPath); // 规划主车路径
	if(argc >= 2&& (strcmp(argv[1], "draw") == 0))
	{
		std::cout << "Current working directory: " << getCurrentWorkingDir() << std::endl;
		#ifndef INITIALIZATION // 初始化操作
		std::vector<cv::Point2f> TargetPath;
		extractTargetPath(targetPath, TargetPath);
		// 获取所有障碍物
		std::vector<ObstacleXOSC> obsList = parseXoscFile("../../TrajectoryControl/m_resource/.resource");
		// 获取所有道路信息
		SSD::SimVector<HDMapStandalone::MLaneInfo> allLanes;
		SimOneAPI::GetLaneData(allLanes);
		std::vector<cv::Point2f> allBoundaries, allJungles;
		extractLaneBoundaries(allLanes, allBoundaries, allJungles);
		// 初始化视图参数
		ViewParams params;
		ViewParamsInit(allBoundaries, allJungles, TargetPath, params);
		// 创建窗口并设置回调
		cv::namedWindow("Canvas", cv::WINDOW_NORMAL);
		cv::resizeWindow("Canvas", params.windowSize.width, params.windowSize.height);
		cv::setMouseCallback("Canvas", onMouse, &params);
		#endif
		while (true)
		{
			++frameCount;
			int frame = SimOneAPI::Wait();

			//globalLogger(Logger::Color::BrightBlue) << "\n\nFrame = " << frameCount;
			recordEvaluation(); // 更新 NEVC 系统评分
			updateObstacleData(); // 更新障碍物参数
			mainVehicle.update(); // 更新主车参数
			waitInitial(); // 等待仿真平台初始化完成

			steerKpUse = steerKp;
			steerKdUse = steerKd;

			// 功能：绘制 opendrive 类型地图和预设轨迹，绘制点，绘制障碍物角点的框，绘制轨迹，
			// 按下 alt 时，左键相应的点可以擦除
			// 按住 ctrl,滚动滚轮，上下移动点的信息列表
			// 按住 ctrl+z,可以撤销
			// 按下“l”导入点，格式：[编号，x,y,pathmode,stoptime,speed,kp]
			// 按下“s”，导出点，格式：[编号，x,y,pathmode,stoptime,speed,kp]
			// 按下“i键”，输入编号，按下回车，右键打点，会在输入编号的点的前面插入点
			// 按下“m”,输入编号，按下回车，会使得这个点与上个点的轨迹变成插值的
			// 按下“t"，输入编号，按下回车，再输入停止时间，按下回车，可以更改停止时间
			// 左键移动，滚轮缩放，右键打点
			cv::Mat view;
			cv::Mat infoPanel(params.windowSize.height, 200, CV_8UC3, cv::Scalar(200, 200, 200));
			ImageWarpAffine(params, view);// 应用仿射变换
			drawObstacle(obsList, params, view);//绘制障碍物
			drawPoints(params, view);// 绘制用户点击点
			drawInfoBoard(params, infoPanel);// 绘制信息面板
			drawPath(params, view);	// 绘制轨迹（动态计算坐标）
			drawInsertBoard(params, infoPanel);// 绘制输入框背景
			// 合并视图
			cv::Mat combined;
			cv::hconcat(view, infoPanel, combined);
			cv::imshow("Canvas", combined);

			KeyBoardOrder(params, caseIdx);//键盘指令输入（没有退出指令）

			mainVehicle.useDefaultPath = false;
			pControl->throttle = pControl->steering = 0.0f;
			mainVehicle.drive();
			SimOneAPI::NextFrame(frame);
		}

		return 0;
	}
	else 
	{
		SSD::SimVector<int> indexOfValidPoints;
		Timer timer;//初始化定时器类
		timer.tic(); // tic 用于开始计时，初始化的时候调用一次
		int stopTime;
		bool isStopping = false;
		bool stopTimerStarted = false;
		std::set<int> stoppedPointIds; // 记录已经处理过的点编号
		std::unordered_map<int, float> brakingStartDists;//为了使得在每个寻迹点前只会打下一个标签
		/* 如果当前案例被设置为打点循迹模式 */
		while (FlagType::isManualTrackMode)
		{
			++frameCount;
			int frame = SimOneAPI::Wait();

			//globalLogger(Logger::Color::BrightBlue) << "\n\nFrame = " << frameCount;

			recordEvaluation(); // 更新 NEVC 系统评分

			updateObstacleData(); // 更新障碍物参数
			mainVehicle.update(); // 更新主车参数

			waitInitial(); // 等待仿真平台初始化完成

			steerKpUse = steerKp;
			steerKdUse = steerKd;
			obstacleList.clear();
			stopLineList.clear();
			static bool isBraking = false;
			static bool notUpdateNextPt = false;
			static int currentIdx = -1; // 当前循迹点的索引，初始为 -1
			float distNextPoint = 0.0f; // 到下一个循迹点之间的 ST 距离
			if (currentIdx == -1) distNextPoint = std::numeric_limits<float>::max();
			else distNextPoint = getDistST(mainVehicle.pt, strategyPoint[currentIdx].Pos); // 到下一个循迹点之间的 ST 距离

		#define JudgeJsonData(DATA1,DATA2,DATA3) if (strategyPoint[currentIdx].DATA1 == -1) DATA2 = DATA3; else DATA2 = strategyPoint[currentIdx].DATA1
			JudgeJsonData(kp, steerKpUse, steerKp);
			JudgeJsonData(stopTime, stopTime, -1);

			if (stopTime <= 0 || stoppedPointIds.count(currentIdx) > 0)
			{
				isStopping = false;
				stopTimerStarted = false;
				JudgeJsonData(speed, pControl->throttle, caseTargetSpeed);
			}
			else
			{

				if (brakingStartDists.count(currentIdx) == 0)
				{
					float d0 = 0.05272f * mainVehicle.speed * mainVehicle.speed
						+ 0.06714f * mainVehicle.speed - 0.02188f;
					brakingStartDists[currentIdx] = d0;
				}
				float d0 = brakingStartDists[currentIdx];
				if (distNextPoint - 4.38046 <= d0 + 7)
				{
					if (!isStopping)
					{
						// 第一次触发“停止”逻辑
						timer.mark("stop_timer" + std::to_string(currentIdx));
						isStopping = true;
						stopTimerStarted = true;
						std::cout << "停止的追踪点" << currentIdx << "开始停止，stopTime = " << stopTime << " 秒" << std::endl;
						LOG << "期望距离" << d0 << ",距离：" << distNextPoint << ",点编号" << currentIdx;
					}
					// 正在“停止中”，判断是否到了停止时长
					if (stopTimerStarted && timer.isTriggered("stop_timer" + std::to_string(currentIdx), static_cast<float>(stopTime), Timer::Unit::Seconds))
					{
						std::cout << "停止时间到，重新启动。" << std::endl;
						timer.removeMark("stop_timer" + std::to_string(currentIdx));
						isStopping = false;
						stopTimerStarted = false;
						stoppedPointIds.insert(currentIdx);
						JudgeJsonData(speed, pControl->throttle, caseTargetSpeed);
					}
					else
					{
						// 停止状态，油门归零
						pControl->throttle = 0;
					}
				}
				else
				{
					// 不在停止区域范围内，重置状态（防止提前触发）
					isStopping = false;
					stopTimerStarted = false;
					JudgeJsonData(speed, pControl->throttle, caseTargetSpeed);
				}
			}


			if (currentIdx == -1 && currentIdx + 1 < strategyPoint.size() && !notUpdateNextPt) // 刚开始循迹的时候
			{
				currentIdx = 0;
				initialPath.clear(); // 更新起点和终点
				targetPath.clear(); // 清空旧路径
				indexOfValidPoints.clear();

				initialPath.push_back(mainVehicle.pt);
				initialPath.push_back(strategyPoint[0].Pos);
				if (strategyPoint[0].pathMode == -1 && !SimOneAPI::GenerateRoute(initialPath, indexOfValidPoints, targetPath)) // 生成到第一个循迹点的路径
				{
					SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Error, "使用 A* 算法生成路径失败");
				}
				else if (strategyPoint[0].pathMode == 1 && equidistantSampling(initialPath, targetPath, 0.2))
				{
					LOG << "使用插值生成轨迹失败";
				}
				else LOG << "切换到下一个循迹点，下一个循迹点的编号：" << currentIdx;
			}
			else if (distNextPoint <= 5 && currentIdx + 1 < strategyPoint.size() && !notUpdateNextPt)
			{
				++currentIdx;
				initialPath.clear(); // 更新起点和终点
				targetPath.clear(); // 清空旧路径
				indexOfValidPoints.clear();

				initialPath.push_back(mainVehicle.pt);
				initialPath.push_back(strategyPoint[currentIdx].Pos);
				if (strategyPoint[currentIdx].pathMode == -1 && !SimOneAPI::GenerateRoute(initialPath, indexOfValidPoints, targetPath)) // 生成到第一个循迹点的路径
				{
					SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Error, "使用 A* 算法生成路径失败");
				}
				else if (strategyPoint[currentIdx].pathMode == 1 && equidistantSampling(initialPath, targetPath, 0.2))
				{
					LOG << "使用插值生成轨迹失败";
				}
				else LOG << "切换到下一个循迹点，下一个循迹点的编号：" << currentIdx;
			}
			//LOG << "正在导航至第 " << currentIdx << " 个循迹点：（" << strategyPoint[currentIdx].Pos.x << "，" << strategyPoint[currentIdx].Pos.y << "）\n" <<
			//	"循迹速度为 " << strategyPoint[currentIdx].speed << "，使用的 PID 参数：[ P = " <<
			//	((strategyPoint[currentIdx].kp == -1) ? "默认" : std::to_string(strategyPoint[currentIdx].kp)) << " ]\n" <<
			//	"距离下一个循迹点的距离为：" << distNextPoint << "，主车坐标为：（" << mainVehicle.pt.x << "，" << mainVehicle.pt.y << "）";
			mainVehicle.drive();
			SimOneAPI::NextFrame(frame);
		}
		return 0;
	}
}


#endif

#if false
SSD::SimPoint3D stopPt1(19.6, 297.39, 0.0);
SSD::SimPoint3D stopPt2(66.78, 227.546, 0.0);
double stopT1 = 10.f, stopT2 = 10.f;

int main(int argc, char* argv[]) {
	#ifndef INITIALIZATION // 初始化操作
	initSimOne(); // SimOne 初始化
	caseIdx = getCaseIdx(); // 获取当前案例详细信息
	loadJson(); // Json 初始化

	SSD::SimVector<int> indexOfValidPoints;
	initNavigation(indexOfValidPoints, targetPath); // 规划主车路径

	adaptiveFollowing = AdaptiveFollowing(followingKp, followingKi, followingKd, 45); // 跟车 PID

	// 判断 Json 中是否将当前案例编号设置为打点循迹模式
	for (auto& reservoirPair : manualTrackReservoirMap)
	{
		if (caseIdx == reservoirPair.first)
		{
			FlagType::isManualTrackMode = true;
			manualTrackReservoir = reservoirPair.second;
			globalLogger(Logger::Color::BrightMagenta) << "启用打点循迹模式";
			break;
		}
	}

	for (size_t i = 0, ie = manualPreciseTrackReservoir.size(); i < ie; ++i)
	{
		if (caseIdx == manualPreciseTrackReservoir[i].caseIndex)
		{
			FlagType::isManualPreciseTrackMode = true;
			potentialManualPreciseTrackIndex = i;
			manualPreciseTrackReservoir[potentialManualPreciseTrackIndex].equidistantSampling(0.2);
			globalLogger(Logger::Color::BrightMagenta) << "启用精密打点循迹模式";
			break;
		}
	}

	if (std::find(caseFollowingID.begin(), caseFollowingID.end(), caseIdx) != caseFollowingID.end())
	{
		FlagType::isFollowingSample = 1; // 跟车场景标志位置为 1
		globalLogger(Logger::Color::BrightMagenta) << "启用跟车场景标志位";
	}

	// 获取当前场景得交通灯列表
	SimOneAPI::GetTrafficLightList(trafficLightList);
	#endif

	/* 如果当前案例被设置为精密打点循迹模式 */
	while (FlagType::isManualPreciseTrackMode)
	{
		++frameCount;
		int frame = SimOneAPI::Wait();

		globalLogger(Logger::Color::BrightBlue) << "\n\nFrame = " << frameCount;
		globalLogger(Logger::Color::BrightGreen) << "当前处于精密打点循迹模式";

		recordEvaluation(); // 更新 NEVC 系统评分

		updateObstacleData(); // 更新障碍物参数
		mainVehicle.update(); // 更新主车参数

		waitInitial(); // 等待仿真平台初始化完成

		steerKpUse = steerKp;
		steerKdUse = steerKd;
		obstacleList.clear();
		stopLineList.clear();

		auto& track = manualPreciseTrackReservoir[potentialManualPreciseTrackIndex];
		targetPath = track.path;
		mainVehicle.useDefaultPath = true;

		if (caseIdx == 33)
		{
			if (frameCount < 250)
			{
				pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
				pControl->throttle = 8;
			}
			else pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral;
		}
		LOG << "主车当前位置：" << mainVehicle.pt.x << "，" << mainVehicle.pt.y;

		mainVehicle.drive();

		SimOneAPI::NextFrame(frame);
	}

	/* 如果当前案例被设置为打点循迹模式 */
	while (FlagType::isManualTrackMode)
	{
		++frameCount;
		int frame = SimOneAPI::Wait();

		globalLogger(Logger::Color::BrightBlue) << "\n\nFrame = " << frameCount;

		recordEvaluation(); // 更新 NEVC 系统评分

		updateObstacleData(); // 更新障碍物参数
		mainVehicle.update(); // 更新主车参数

		waitInitial(); // 等待仿真平台初始化完成

		steerKpUse = steerKp;
		steerKdUse = steerKd;
		obstacleList.clear();
		stopLineList.clear();

		static auto startTime = std::chrono::system_clock::now();
		static bool isBraking = false;
		static bool notUpdateNextPt = false;

		static int currentIdx = -1; // 当前循迹点的索引，初始为 -1
		float distNextPoint = 0.0f; // 到下一个循迹点之间的 ST 距离
		if (currentIdx == -1) distNextPoint = std::numeric_limits<float>::max();
		else distNextPoint = getDistST(mainVehicle.pt, manualTrackReservoir[currentIdx].dst); // 到下一个循迹点之间的 ST 距离

		if (currentIdx == -1 && currentIdx + 1 < manualTrackReservoir.size() && !notUpdateNextPt) // 刚开始循迹的时候
		{
			currentIdx = 0;
			initialPath.clear(); // 更新起点和终点
			targetPath.clear(); // 清空旧路径
			indexOfValidPoints.clear();

			initialPath.push_back(mainVehicle.pt);
			initialPath.push_back(manualTrackReservoir[0].dst);
			if (!SimOneAPI::GenerateRoute(initialPath, indexOfValidPoints, targetPath)) // 生成到第一个循迹点的路径
			{
				SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Error, "使用 A* 算法生成路径失败");
			}
			else LOG << "切换到下一个循迹点，下一个循迹点的编号：" << currentIdx;
		}
		else if (distNextPoint <= 5 && currentIdx + 1 < manualTrackReservoir.size() && !notUpdateNextPt)
		{
			++currentIdx;

			initialPath.clear(); // 更新起点和终点
			targetPath.clear(); // 清空旧路径
			indexOfValidPoints.clear();

			initialPath.push_back(mainVehicle.pt);
			initialPath.push_back(manualTrackReservoir[currentIdx].dst);
			if (!SimOneAPI::GenerateRoute(initialPath, indexOfValidPoints, targetPath)) // 生成到第一个循迹点的路径
			{
				SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Error, "使用 A* 算法生成路径失败");
			}
			else LOG << "切换到下一个循迹点，下一个循迹点的编号：" << currentIdx;
		}

		if (manualTrackReservoir[currentIdx].kp == -1) // 使用默认的 Kp
		{
			steerKpUse = steerKp;
		}
		else // 否则使用 Json 中设定的 Kp
		{
			steerKpUse = manualTrackReservoir[currentIdx].kp;
		}

		if (manualTrackReservoir[currentIdx].kd == -1) // 使用默认的 Kd
		{
			steerKdUse = steerKd;
		}
		else // 否则使用 Json 中设定的 Kd
		{
			steerKdUse = manualTrackReservoir[currentIdx].kd;
		}

		if (manualTrackReservoir[currentIdx].targetSpeed == -1) // 使用默认的速度
		{
			pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
			pControl->throttle = caseTargetSpeed;
		}
		else // 否则使用 Json 中设定的速度
		{
			pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
			pControl->throttle = manualTrackReservoir[currentIdx].targetSpeed;
		}

		// 如果在 Json 中传入了 brakeDist 数据
		if (!(std::fabs(manualTrackReservoir[currentIdx].brakeDist - (-1)) < 1e-5))
		{
			if (distNextPoint <= manualTrackReservoir[currentIdx].brakeDist)
			{
				// 如果用户传入的 prepareSpeed 不是 -1，则在接近下一个循迹点半径为 brakeDist 的一个圆内的时候，减速为 prepareSpeed
				if (!(std::fabs(manualTrackReservoir[currentIdx].prepareSpeed - (-1)) < 1e-5))
				{
					pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
					pControl->throttle = manualTrackReservoir[currentIdx].prepareSpeed;
				}
			}

			// 如果用户把 prepareSpeed 设置为 -1，但是 brakeDist 不是 -1，则溜车
			if (std::fabs(manualTrackReservoir[currentIdx].prepareSpeed - (-1)) < 1e-5)
			{
				pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Neutral;
				LOG << "当前处于溜车状态";
			}
		}

		// 应该停车
		bool shouldStop1 = UtilMath::planarDistance(mainVehicle.pt, stopPt1) <= 8;
		bool shouldStop2 = UtilMath::planarDistance(mainVehicle.pt, stopPt2) <= 8;
		if (!isBraking && (shouldStop1 || shouldStop2))
		{
			pControl->throttle = 0;
			isBraking = true;
			notUpdateNextPt = true;
			startTime = std::chrono::system_clock::now();
		}

		if (isBraking)
		{
			auto deltaT = std::chrono::system_clock::now() - startTime;

			if (deltaT.count() > stopT1) // 应该完成刹车
			{
				isBraking = false;
				notUpdateNextPt = false;
			}
		}

		LOG << "正在导航至第 " << currentIdx << " 个循迹点：（" << manualTrackReservoir[currentIdx].dst.x << "，" << manualTrackReservoir[currentIdx].dst.y << "）\n" <<
			"循迹速度为 " << manualTrackReservoir[currentIdx].targetSpeed << "，使用的 PID 参数：[ P = " <<
			((manualTrackReservoir[currentIdx].kp == -1) ? "默认" : std::to_string(manualTrackReservoir[currentIdx].kp)) << "；D = " <<
			((manualTrackReservoir[currentIdx].kd == -1) ? "默认" : std::to_string(manualTrackReservoir[currentIdx].kd)) << " ]\n" <<
			"距离下一个循迹点的距离为：" << distNextPoint << "，主车坐标为：（" << mainVehicle.pt.x << "，" << mainVehicle.pt.y << "）";

		mainVehicle.drive();
		SimOneAPI::NextFrame(frame);
	}	   

	/* 如果当前案例被设置为跟车模式 */
	while (FlagType::isFollowingSample) // 跟车场景
	{
		++frameCount;
		int frame = SimOneAPI::Wait();

		globalLogger(Logger::Color::BrightBlue) << "\n\nFrame = " << frameCount;

		recordEvaluation(); // 更新 NEVC 系统评分

		updateObstacleData(); // 更新障碍物参数
		mainVehicle.update(); // 更新主车参数

		waitInitial(); // 等待仿真平台初始化完成

		steerKpUse = steerKp;
		steerKdUse = steerKd;
		obstacleList.clear();

		for (size_t i = 0, ie = pObstacle->obstacleSize; i < ie; ++i)
		{
			obstacleList.push_back(Obstacle(pObstacle->obstacle[i]));
		}

		pControl->throttle = caculateFollowingSpeed(obstacleList, caseIdx);

		globalLogger(Logger::Color::BrightBlue) << "pControl->throttle = " << pControl->throttle;

		mainVehicle.drive();

		SimOneAPI::NextFrame(frame);
	}

	while (true)
	{
		++frameCount;
		int frame = SimOneAPI::Wait();

		globalLogger(Logger::Color::BrightBlue) << "\n\nFrame = " << frameCount;

		recordEvaluation(); // 更新 NEVC 系统评分

		updateObstacleData(); // 更新障碍物参数
		mainVehicle.update(); // 更新主车参数

		waitInitial(); // 等待仿真平台初始化完成

		FlagType::isCrosswalkExist = getCrossWalk();
		steerKpUse = steerKp;
		steerKdUse = steerKd;
		obstacleList.clear();
		stopLineList.clear();

		// 手动创建停止线
		for (size_t i = 0, ie = manualStopLineReservoir.size(); i < ie; ++i)
		{
			if (caseIdx == manualStopLineReservoir[i].caseIndex && manualStopLineReservoir[i].srcFrame <= frameCount && frameCount <= manualStopLineReservoir[i].dstFrame)
			{
				stopLineList.push_back(StopLine(manualStopLineReservoir[i]));
			}
		}

		/************************************************* 障碍物处理 *************************************************/
		std::vector<size_t> staticObstacleIndex; // 静止的障碍物有哪些
		std::vector<std::vector<size_t>> obstacleIDGroup; // 将距离过近的静止障碍物分组合成以后的障碍物 ID 列表

		for (size_t i = 0, ie = pObstacle->obstacleSize; i < ie; ++i)
		{
			if (UtilMath::calculateSpeed(pObstacle->obstacle[i].velX, pObstacle->obstacle[i].velY) < 1e-2) // 若障碍物静止
			{
				staticObstacleIndex.push_back(i);
				continue;
			}

			obstacleList.push_back(Obstacle(pObstacle->obstacle[i])); // 如果障碍物不是静止的，就直接填充进 Obstacle 类
		}

		groupObstacleByDist(staticObstacleIndex, groupingDistThres, obstacleIDGroup); // 合成障碍（同类型的静止障碍）

		for (size_t i = 0, ie = obstacleIDGroup.size(); i < ie; ++i)
		{
			obstacleList.push_back(Obstacle(obstacleIDGroup[i])); // 将分好组的静止障碍物填充进 Obstacle 类
		}
		/************************************************* 障碍物处理 *************************************************/

		/* 为障碍物创建停止线 */
		for (size_t i = 0, ie = obstacleList.size(); i < ie; ++i)
		{
			stopLineList.push_back(StopLine(obstacleList[i]));
		}

		/* 检测并创建红绿灯停止线 */
		if (getValidTrafficLight(trafficLightList, potentialLight)) stopLineList.push_back(StopLine(potentialLight));

		StopLine potentialStopLine;
		float minDistance;
		if (calculateNearestStopLine(stopLineList, potentialStopLine, minDistance)) // 如果存在距离主车最近的停止线
		{
			globalLogger(Logger::Color::BrightGreen) << "——————————————————————————————————————";
			globalLogger(Logger::Color::BrightGreen) << "潜在停止线 " << "：";
			globalLogger(Logger::Color::BrightGreen) << "类型：" << ((potentialStopLine.type == StopLine::Type::Whole) ? "全停止线" : "半停止线");
			globalLogger(Logger::Color::BrightGreen) << "策略：" << ((potentialStopLine.strategy == StopLine::Strategy::StopStart) ? "停走" : "变道");
			globalLogger(Logger::Color::BrightGreen) << "起点：（" << potentialStopLine.src.x << "，" << potentialStopLine.src.y << "）";
			globalLogger(Logger::Color::BrightGreen) << "终点：（" << potentialStopLine.dst.x << "，" << potentialStopLine.dst.y << "）";

			/* 全停止线，主车的策略是停走 */
			if (potentialStopLine.type == StopLine::Type::Whole && potentialStopLine.strategy == StopLine::Strategy::StopStart)
			{
				float d0 = 0.05272 * mainVehicle.speed * mainVehicle.speed + 0.06714 * mainVehicle.speed - 0.02188; // d0 是刹车距离
				if (potentialStopLine.toVehicleDist - potentialStopLine.offset - 4.38046 <= d0 + stopLineDistThres) // d0 + x 表示要在停止线 x 米前停下来， - 4.38046 代表减去质心到车头的距离
				{
					pControl->throttle = 0;  // 立即停止加速（刹车）
				}
			}
			/* 半停止线，主车的策略是变道 */
			else if (potentialStopLine.type == StopLine::Type::Single && potentialStopLine.strategy == StopLine::Strategy::LaneChange)
			{
				if (potentialStopLine.velocityPlanar < 1e-4 && frameCount >= 10) // 静止的半停止线
				{
					getLaneChangePath(potentialStopLine);
				}
				else // 移动的半停止线
				{
					getLaneChangePath(potentialStopLine, potentialStopLine.velocityPlanar);
					if(caseIdx == 17 || caseIdx == 39) getLaneChangePath(potentialStopLine);
				}
			}
		}

		// 超车速度规划（这个只能处理主车即将变道，而主车后方又有一辆快速行驶的对手车辆的情况，不能处理移动的半停止线）
		// 还需要处理那些在主车前方行驶的移动半停止线
		calculateOvertakingSpeed(obstacleList);

		mainVehicle.drive();

		globalLogger(Logger::Color::BrightGreen) << "mainVehicle.pt = " << mainVehicle.pt.x << ", " << mainVehicle.pt.y;
		globalLogger(Logger::Color::BrightGreen) << "caseTargetSpeed = " << caseTargetSpeed;
		globalLogger(Logger::Color::BrightGreen) << "pObstacle->obstacleSize = " << pObstacle->obstacleSize;
		globalLogger(Logger::Color::BrightGreen) << "obstacleList.size = " << obstacleList.size();
		globalLogger(Logger::Color::BrightGreen) << "stopLineList.size = " << stopLineList.size();

		for (size_t i = 0, ie = stopLineList.size(); i < ie; ++i)
		{
			globalLogger(Logger::Color::BrightGreen) << "——————————————————————————————————————";
			globalLogger(Logger::Color::BrightGreen) << "停止线 " << i << "：";
			globalLogger(Logger::Color::BrightGreen) << "类型：" << ((stopLineList[i].type == StopLine::Type::Whole) ? "全停止线" : "半停止线");
			globalLogger(Logger::Color::BrightGreen) << "策略：" << ((stopLineList[i].strategy == StopLine::Strategy::StopStart) ? "停走" : "变道");
			globalLogger(Logger::Color::BrightGreen) << "起点：（" << stopLineList[i].src.x << "，" << stopLineList[i].src.y << "）";
			globalLogger(Logger::Color::BrightGreen) << "终点：（" << stopLineList[i].dst.x << "，" << stopLineList[i].dst.y << "）";
		}

		SimOneAPI::NextFrame(frame);
	}

	return 0;
}
#endif