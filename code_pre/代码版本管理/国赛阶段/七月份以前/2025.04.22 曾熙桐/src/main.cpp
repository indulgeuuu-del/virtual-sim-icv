#include "main.h"
#include "opencv2/opencv.hpp"
#include "RuleSet.h"
#include <filesystem>
#include <unordered_set>

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

#if false
int main(int argc, char* argv[])
{
	if (argc >= 2 && (strcmp(argv[1], "draw") == 0))
	{
		std::cout << "Current working directory: " << getCurrentWorkingDir() << std::endl;
		#ifndef INITIALIZATION // 初始化操作
		initSimOne(); // SimOne 初始化
		caseIdx = getCaseIdx(); // 获取当前案例详细信息
		loadJson(); // Json 初始化
		SSD::SimVector<int> indexOfValidPoints;
		initNavigation(indexOfValidPoints, targetPath); // 规划主车路径
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
			// 按下“l”导入点，格式：[x,y,pathmode,stoptime,speed,kp]
			// 按下“s”，导出点，格式：[x,y,pathmode,stoptime,speed,kp]
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
	else if (argc >= 2 && (strcmp(argv[1], "obtain") == 0))
	{
		#ifndef INITIALIZATION // 初始化操作
		initSimOne(); // SimOne 初始化
		caseIdx = getCaseIdx(); // 获取当前案例详细信息
		loadJson(); // Json 初始化
		ASSERT(SimOneAPI::GetWayPoints(mainVehicle.id, pWayPoints.get()), "获取主车预设路径的起点和终点失败");
		for (size_t i = 0; i < pWayPoints->wayPointsSize; ++i) {
			SSD::SimPoint3D inputWayPoints(pWayPoints->wayPoints[i].posX, pWayPoints->wayPoints[i].posY, 0);
			initialPath.push_back(inputWayPoints);
		}
		#endif

		namespace fs = std::filesystem;

		// 构造完整路径
		fs::path outputDir = fs::path("../../TrajectoryControl/m_strategy/");
		fs::path outputFile = outputDir / (std::to_string(caseIdx) + ".stg");

		// 创建案例专属目录
		if (!fs::exists(outputDir)) {
			fs::create_directories(outputDir);
		}

		// 打开文件流
		std::ofstream file(outputFile);
		ASSERT(file, ("无法打开文件: " + outputFile.string()));

		// 写入数据
		int defaultPointModes = 0; // 默认打点模式
		int defaultStopTime = 0; // 默认停止时间
		int defaultSpeed = 0; // 默认速度
		int defaultKp = 0;
		file << std::fixed << std::setprecision(4);
		for (size_t i = 0; i < pWayPoints->wayPointsSize; ++i)
		{
			file << "[ "
				<< pWayPoints->wayPoints[i].index << ", "
				<< static_cast<float>(pWayPoints->wayPoints[i].posX) << ", "
				<< static_cast<float>(pWayPoints->wayPoints[i].posY) << ", "
				<< defaultPointModes << ", "
				<< defaultStopTime << ", "
				<< defaultSpeed << ", "
				<< defaultKp << "]\n";
		}

		// 写入后刷新并关闭文件
		file.flush();
		file.close();

		// 等待 1000ms
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		std::cout << "成功导出案例 [" << std::to_string(caseIdx) << "] 到: " << outputFile << std::endl;

		std::string command = "notepad \"" + outputFile.string() + "\"";
		std::system(command.c_str());
	}
	else 
	{
		LOG << "执行普通模式";
	}

	return 0;
}
#endif

void groupObstacleByS(const std::vector<Obstacle>& obstacleList,
	const std::vector<size_t>& indexList,
	float threshold,
	std::vector<std::vector<size_t>>& groups)
{
	groups.clear();
	size_t n = indexList.size();
	std::vector<std::vector<size_t>> adjacencyList(n);

	// 构建邻接矩阵：根据 s 坐标是否在阈值内
	for (size_t i = 0; i < n; ++i)
	{
		for (size_t j = i + 1; j < n; ++j)
		{
			size_t idx1 = indexList[i];
			size_t idx2 = indexList[j];

			double s1 = 0.0f, s2 = 0.0f, t = 0.0f;
			SimOneAPI::GetLaneST(obstacleList[idx1].laneID, obstacleList[idx1].pt, s1, t);
			SimOneAPI::GetLaneST(obstacleList[idx2].laneID, obstacleList[idx2].pt, s2, t);

			if (isSameRoadId(obstacleList[idx1].laneID, obstacleList[idx2].laneID) && std::abs(s1 - s2) <= threshold)
			{
				adjacencyList[i].push_back(j);
				adjacencyList[j].push_back(i);
			}
		}
	}

	// 深度优先搜索 (DFS)
	std::unordered_set<size_t> visited;
	auto graphDFS = [&](size_t index, auto& graphDFSRef, std::vector<size_t>& group) -> void {
		visited.insert(index);
		group.push_back(index);

		for (size_t neighbor : adjacencyList[index]) {
			if (visited.find(neighbor) == visited.end()) {
				graphDFSRef(neighbor, graphDFSRef, group);
			}
		}
		};

	// 遍历所有节点，构建分组
	for (size_t i = 0; i < n; ++i)
	{
		if (visited.find(i) == visited.end())
		{
			std::vector<size_t> group;
			graphDFS(i, graphDFS, group);

			// 将 group 中的局部索引转为原始索引
			for (size_t& idx : group) idx = indexList[idx];
			groups.push_back(group);
		}
	}
}

void determineFeasibleRegion(const std::vector<Obstacle>& obstacleList,
	const std::vector<std::vector<size_t>>& indexList,
	const std::vector<SSD::SimPoint3D>& left, const std::vector<SSD::SimPoint3D>& right,
	std::vector<SSD::SimPoint3D>& controlPoints, std::vector<bool>& accessible)
{
	controlPoints.clear();
	accessible.clear();

	// i 表示第几组
	for (size_t i = 0, ie = indexList.size(); i < ie; ++i)
	{
		// 第 i 组有 indexList[i].size() 个障碍物
		// 第 i 组的障碍物有哪些：obstacleList.at(indexList[i][j]) ↓
		// for (size_t j = 0, je = indexList[i].size(); j < je; ++j) { }
		// 需要判断的点有哪些：obstacleList.at(indexList[i][j])、left[i]、right[i]
		std::vector<SSD::SimPoint3D> points2BJudged;
		for (size_t j = 0, je = indexList[i].size(); j < je; ++j)
		{
			points2BJudged.push_back(obstacleList.at(indexList[i][j]).pt);
		}
		points2BJudged.push_back(left[i]);
		points2BJudged.push_back(right[i]);

		auto findMaxGapMidpoint = [](const std::vector<SSD::SimPoint3D>& points2BJudged, const SSD::SimPoint3D& direction, float threshold = 2.5f) -> SSD::SimPoint3D {
			if (points2BJudged.size() < 2) return SSD::SimPoint3D{ 0.0f, 0.0f, 0.0f };

			// 构造垂直于 direction 的单位向量（只在 XY 平面考虑）
			float len = std::sqrt(direction.x * direction.x + direction.y * direction.y);
			if (len < 1e-5f) return SSD::SimPoint3D{ 0.0f, 0.0f, 0.0f };

			float vx = -direction.y / len;
			float vy = direction.x / len;

			struct ProjectedPoint {
				float projValue;
				SSD::SimPoint3D original;
			};

			std::vector<ProjectedPoint> projected;
			projected.reserve(points2BJudged.size());

			// 计算每个点在垂线方向上的投影值
			for (const auto& pt : points2BJudged) {
				float proj = pt.x * vx + pt.y * vy;
				projected.push_back({ proj, pt });
			}

			// 按照投影值排序
			std::sort(projected.begin(), projected.end(), [](const ProjectedPoint& a, const ProjectedPoint& b) {
				return a.projValue < b.projValue;
				});

			float maxGap = 0.0f;
			SSD::SimPoint3D midpoint{ 0.0f, 0.0f, 0.0f };

			// 找到最大投影间隔
			for (size_t i = 0; i < projected.size() - 1; ++i) {
				float gap = projected[i + 1].projValue - projected[i].projValue;
				if (gap > maxGap) {
					maxGap = gap;
					const auto& p1 = projected[i].original;
					const auto& p2 = projected[i + 1].original;
					midpoint.x = (p1.x + p2.x) * 0.5f;
					midpoint.y = (p1.y + p2.y) * 0.5f;
					midpoint.z = (p1.z + p2.z) * 0.5f;
				}
			}

			if (maxGap < threshold) {
				return SSD::SimPoint3D{ 0.0f, 0.0f, 0.0f };
			}

			return midpoint;
			};

		SSD::SimPoint3D pt, dir;
		SimOneAPI::GetInertialFromLaneST(m_SampleGetNearMostLane(points2BJudged[0]), 0.0, 0.0, pt, dir);
		controlPoints.push_back(findMaxGapMidpoint(points2BJudged, dir));
		if (controlPoints[i].x < 1e-4 && controlPoints[i].y < 1e-4 && controlPoints[i].z < 1e-4) accessible.push_back(false); // 不可通过
		else accessible.push_back(true); // 可通过
	}
}

#if false
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

	/* 获取当前案例规则集 */
	RuleSet ruleSet;
	if (ruleSet.loadFromYamlFile("../../TrajectoryControl/m_strategy/global.yaml")) {
		std::cout << "规则加载成功！共 " << ruleSet.rules.size() << " 条\n\n";
		ruleSet.print();
	}
	else {
		std::cerr << "规则加载失败\n";
	}

	std::vector<Rule> rules;
	for (const auto& rule : ruleSet.rules)
	{
		if (caseIdx == rule.first)
		{
			rules.push_back(rule.second);
		}
	}

	// 获取当前场景得交通灯列表
	SimOneAPI::GetTrafficLightList(trafficLightList);
	#endif

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
			// 先判断障碍物是否在主车所在的车道或者主车即将到达的下一个车道，或者障碍物距离主车特别近
			SSD::SimPoint3D position(pObstacle->obstacle->posX, pObstacle->obstacle->posY, pObstacle->obstacle->posZ);
			SSD::SimString laneID = m_SampleGetNearMostLane(position);
			if (isSameRoadId(laneID, mainVehicle.laneID) || isSameRoadId(laneID, mainVehicle.nextLaneID) ||
				UtilMath::planarDistance(position, mainVehicle.pt) < 20.0f)
			{
				if (UtilMath::calculateSpeed(pObstacle->obstacle[i].velX, pObstacle->obstacle[i].velY) < 1e-2) // 若障碍物静止
				{
					staticObstacleIndex.push_back(i);
					continue;
				}

				obstacleList.push_back(Obstacle(pObstacle->obstacle[i])); // 如果障碍物不是静止的，就直接填充进 Obstacle 类
			}
		}

		groupObstacleByDist(staticObstacleIndex, groupingDistThres, obstacleIDGroup); // 合成障碍（同类型的静止障碍）

		for (size_t i = 0, ie = obstacleIDGroup.size(); i < ie; ++i)
		{
			obstacleList.push_back(Obstacle(obstacleIDGroup[i])); // 将分好组的静止障碍物填充进 Obstacle 类
		}
		/************************************************* 障碍物处理 *************************************************/

		/* 以下是单车道变道 */
		std::vector<size_t> sameRoadObstacleIndex; // 与主车同车道的障碍物有哪些
		std::vector<std::vector<size_t>> nearSObstacleGroup; // 将 s 坐标相近的静止障碍物分组合成以后的障碍物 ID 列表

		for (size_t i = 0, ie = obstacleList.size(); i < ie; ++i)
		{
			if (isSameRoadId(obstacleList[i].laneID, mainVehicle.laneID) ||
				isSameRoadId(obstacleList[i].laneID, mainVehicle.nextLaneID) &&
				UtilMath::calculateSpeed(pObstacle->obstacle[i].velX, pObstacle->obstacle[i].velY) < 1e-2) // 若障碍物静止
			{
				sameRoadObstacleIndex.push_back(i);
			}
		}

		groupObstacleByS(obstacleList, sameRoadObstacleIndex, 1.0f, nearSObstacleGroup);

		std::vector<SSD::SimPoint3D> averagePointList;
		for (size_t i = 0; i < nearSObstacleGroup.size(); ++i) {
			SSD::SimPoint3D averagePoint(0.0, 0.0, 0.0);
			for (size_t idx : nearSObstacleGroup[i]) {
				averagePoint.x += obstacleList[idx].pt.x;
				averagePoint.y += obstacleList[idx].pt.y;
				averagePoint.z += obstacleList[idx].pt.z;
			}
			averagePoint.x /= (double)nearSObstacleGroup[i].size();
			averagePoint.y /= (double)nearSObstacleGroup[i].size();
			averagePoint.z /= (double)nearSObstacleGroup[i].size();
			averagePointList.push_back(averagePoint);
		}

		std::vector<SSD::SimPoint3D> groupLeftPointList, groupRightPointList;
		for (size_t i = 0, ie = averagePointList.size(); i < ie; ++i)
		{
			SSD::SimPoint3D groupLeftPoint, groupRightPoint;
			getObstacleLaneSidePosition(averagePointList[i], groupLeftPoint, groupRightPoint);
			groupLeftPointList.push_back(groupLeftPoint);
			groupRightPointList.push_back(groupRightPoint);
		}

		std::vector<SSD::SimPoint3D> controlPoints;
		std::vector<bool> isFeasible;
		determineFeasibleRegion(obstacleList, nearSObstacleGroup, groupLeftPointList, groupRightPointList, controlPoints, isFeasible);

		/*LOG << "控制点：";
		for (size_t i = 0; i < controlPoints.size(); ++i) {
			LOG << "controlPoints[" << i << "] = (" << controlPoints[i].x << ", " << controlPoints[i].y << ", " << controlPoints[i].z << ")";
		}*/

		/*LOG << "分组情况";
		for (size_t i = 0; i < nearSObstacleGroup.size(); ++i) {
			std::cout << "组 " << i << "：";
			for (size_t idx : nearSObstacleGroup[i]) {
				std::cout << idx << " ";
			}
			std::cout << std::endl;
		}*/

		if (!controlPoints.empty()) // 如果控制点不是空的
		{

		}

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
			/*globalLogger(Logger::Color::BrightGreen) << "——————————————————————————————————————";
			globalLogger(Logger::Color::BrightGreen) << "潜在停止线 " << "：";
			globalLogger(Logger::Color::BrightGreen) << "类型：" << ((potentialStopLine.type == StopLine::Type::Whole) ? "全停止线" : "半停止线");
			globalLogger(Logger::Color::BrightGreen) << "策略：" << ((potentialStopLine.strategy == StopLine::Strategy::StopStart) ? "停走" : "变道");
			globalLogger(Logger::Color::BrightGreen) << "起点：（" << potentialStopLine.src.x << "，" << potentialStopLine.src.y << "）";
			globalLogger(Logger::Color::BrightGreen) << "终点：（" << potentialStopLine.dst.x << "，" << potentialStopLine.dst.y << "）";*/

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

		/*globalLogger(Logger::Color::BrightGreen) << "mainVehicle.pt = " << mainVehicle.pt.x << ", " << mainVehicle.pt.y;
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
		}*/

		SimOneAPI::NextFrame(frame);
	}

	return 0;
}
#endif

// 测试，可随时删除
#if true
// 计算 Roll（绕 X 轴旋转）
float getRoll(float x, float y, float z, float w)
{
	float sinr_cosp = 2.0f * (w * x + y * z);
	float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
	return std::atan2(sinr_cosp, cosr_cosp);
}

// 计算 Pitch（绕 Y 轴旋转）
float getPitch(float x, float y, float z, float w)
{
	float sinp = 2.0f * (w * y - z * x);
	if (std::fabs(sinp) >= 1.0f)
		return std::copysign(M_PI / 2.0f, sinp); // 使用 90 度限制
	return std::asin(sinp);
}

// 计算 Yaw（绕 Z 轴旋转，常用于车头方向）
float getYaw(float x, float y, float z, float w)
{
	float siny_cosp = 2.0f * (w * z + x * y);
	float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
	return std::atan2(siny_cosp, cosy_cosp);
}

int main(int argc, char* argv[]) {
	#ifndef INITIALIZATION // 初始化操作
	initSimOne(); // SimOne 初始化
	caseIdx = getCaseIdx(); // 获取当前案例详细信息
	loadJson(); // Json 初始化
	ASSERT(SimOneAPI::GetWayPoints(mainVehicle.id, pWayPoints.get()), "获取主车预设路径的起点和终点失败");
	for (size_t i = 0; i < pWayPoints->wayPointsSize; ++i) {
		SSD::SimPoint3D inputWayPoints(pWayPoints->wayPoints[i].posX, pWayPoints->wayPoints[i].posY, 0);
		initialPath.push_back(inputWayPoints);
	}
	#endif

	LOG << "pWayPoints->wayPointsSize = " << pWayPoints->wayPointsSize;
	LOG << "pos = " << pWayPoints->wayPoints[3].posX << ", " << pWayPoints->wayPoints[3].posY;
	LOG << "roll = " << getRoll(pWayPoints->wayPoints[0].heading_x, pWayPoints->wayPoints[0].heading_y, pWayPoints->wayPoints[0].heading_z, pWayPoints->wayPoints[0].heading_w);
	LOG << "pitch = " << getPitch(pWayPoints->wayPoints[0].heading_x, pWayPoints->wayPoints[0].heading_y, pWayPoints->wayPoints[0].heading_z, pWayPoints->wayPoints[0].heading_w);
	LOG << "yaw = " << getYaw(pWayPoints->wayPoints[3].heading_x, pWayPoints->wayPoints[0].heading_y, pWayPoints->wayPoints[0].heading_z, pWayPoints->wayPoints[0].heading_w);
	LOG << "x = " << pWayPoints->wayPoints[3].heading_x << ", y = " << pWayPoints->wayPoints[3].heading_y << ", z = " << pWayPoints->wayPoints[3].heading_z << ", w = " << pWayPoints->wayPoints[3].heading_w;
	size_t idx = 0;
	while (idx < pWayPoints->wayPointsSize)
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

		SSD::SimPoint3D target(pWayPoints->wayPoints[idx].posX, pWayPoints->wayPoints[idx].posY, 0.0);

		SimOne_Data_Pose_Control pose;
		pose.posX = pWayPoints->wayPoints[idx].posX;
		pose.posY = pWayPoints->wayPoints[idx].posY;
		pose.posZ = 0.f;
		pose.autoZ = true;
		pose.oriX = getRoll(pWayPoints->wayPoints[idx].heading_x, pWayPoints->wayPoints[idx].heading_y, pWayPoints->wayPoints[idx].heading_z, pWayPoints->wayPoints[idx].heading_w);
		pose.oriY = getPitch(pWayPoints->wayPoints[idx].heading_x, pWayPoints->wayPoints[idx].heading_y, pWayPoints->wayPoints[idx].heading_z, pWayPoints->wayPoints[idx].heading_w);
		pose.oriZ = getYaw(pWayPoints->wayPoints[idx].heading_x, pWayPoints->wayPoints[idx].heading_y, pWayPoints->wayPoints[idx].heading_z, pWayPoints->wayPoints[idx].heading_w);
		
		ASSERT(SimOneAPI::SetPose(mainVehicle.id, &pose), "Set Pose failed!");
		SimOneAPI::SetDrive(mainVehicle.id, pControl.get());

		if (UtilMath::planarDistance(mainVehicle.pt, target) < 5)
		{
			++idx;
		}

		SimOneAPI::NextFrame(frame);
	}

	recordEvaluation(); // 更新 NEVC 系统评分
	return 0;
}
#endif

// 测试模板
#if false
int main(int argc, char* argv[]) {
	#ifndef INITIALIZATION // 初始化操作
	initSimOne(); // SimOne 初始化
	caseIdx = getCaseIdx(); // 获取当前案例详细信息
	loadJson(); // Json 初始化
	ASSERT(SimOneAPI::GetWayPoints(mainVehicle.id, pWayPoints.get()), "获取主车预设路径的起点和终点失败");
	for (size_t i = 0; i < pWayPoints->wayPointsSize; ++i) {
		SSD::SimPoint3D inputWayPoints(pWayPoints->wayPoints[i].posX, pWayPoints->wayPoints[i].posY, 0);
		initialPath.push_back(inputWayPoints);
	}
	#endif

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



		mainVehicle.drive();

		SimOneAPI::NextFrame(frame);
	}

	return 0;
}
#endif