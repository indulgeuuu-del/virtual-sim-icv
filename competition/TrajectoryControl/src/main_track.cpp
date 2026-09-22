#include "main.h"

int main_track(void)
{
	timer.tic(); // 开始计时
	initNavigation(validWayPoints, targetPath); // 规划主车路径
	SimOneAPI::GetTrafficLightList(trafficLightList); // 获取当前场景得交通灯列表

	while (true)
	{
		++frameCount;
		int frame = SimOneAPI::Wait();
		float Fps = calculateFps(frameCount, timer.get());
		globalLogger(Logger::Color::BrightBlue) << "\n\nFrame = " << frameCount << "，Fps = " << Fps;

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
		std::vector<std::vector<size_t>> obstacleSGroup; // 将 S 坐标相近的所有障碍物分组合成以后的障碍物 ID 列表

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
		LOG << "dist分组情况";
		for (size_t i = 0; i < obstacleIDGroup.size(); ++i) {
			std::cout << "组 " << i << "：";
			for (size_t idx : obstacleIDGroup[i]) {
				std::cout << idx << " ";
			}
			std::cout << std::endl;
		}

		for (size_t i = 0, ie = obstacleIDGroup.size(); i < ie; ++i)
		{
			obstacleList.push_back(Obstacle(obstacleIDGroup[i])); // 将分好组的静止障碍物填充进 Obstacle 类
		}

		float groupingSThres = 1.0f;
		groupObstacleByS(obstacleList, groupingSThres, obstacleSGroup); // 将 S 坐标相近的障碍物分组合成
		mainVehicle.rebuildNeighborhood(); // 使用本帧完成的列表，避免旧索引关联到新对象
		/************************************************* 障碍物处理 *************************************************/

		if (!mainVehicle.leftLaneExist && !mainVehicle.rightLaneExist) // 如果主车没有邻接车道，则要判断是否处于单车道变道工况
		{
			/* 为障碍物创建停止线或吸引线 */
			for (size_t i = 0, ie = obstacleSGroup.size(); i < ie; ++i)
			{
				SSD::SimPoint3D feasiblePoint; // 控制点

				if (singleLaneChangeSwitcher.state && /* 首先判断单车道变道的开关是否打开，若未打开则直接建立停止线 */
					ModeRecognizer::process(obstacleSGroup[i], feasiblePoint) == ModeRecognizer::Mode::SINGE_LANE_CHANGE)
				{
					stopLineList.push_back(StopLine::AttractLine(obstacleList[obstacleSGroup.at(i).front()], feasiblePoint));
				}
				else for (size_t j = 0, je = obstacleSGroup[i].size(); j < je; ++j)
				{
					stopLineList.push_back(StopLine(obstacleList[obstacleSGroup.at(i).at(j)])); // 构造停止线
				}
			}
		}
		else // 如果有多条车道，则可以放心创建停止线
		{
			/* 只为障碍物创建停止线 */
			for (size_t i = 0, ie = obstacleList.size(); i < ie; ++i)
			{
				stopLineList.push_back(StopLine(obstacleList[i])); // 构造停止线
			}
		}

		/* 检测并创建红绿灯停止线 */
		if (getValidTrafficLight(trafficLightList, potentialLight)) stopLineList.push_back(StopLine(potentialLight));

		float minDistance;
		StopLine potentialStopLine;
		if (calculateNearestStopLine(stopLineList, potentialStopLine, minDistance)) // 如果存在距离主车最近的停止线
		{
			globalLogger(Logger::Color::BrightGreen) << "——————————————————————————————————————";
			globalLogger(Logger::Color::BrightGreen) << "潜在停止线 " << "：";
			globalLogger(Logger::Color::BrightGreen) << "类型：" << (potentialStopLine.type == StopLine::Type::Whole ? "全停止线" : (potentialStopLine.type == StopLine::Type::Single ? "半停止线" : "吸引线"));
			globalLogger(Logger::Color::BrightGreen) << "策略：" << ((potentialStopLine.strategy == StopLine::Strategy::StopStart) ? "停走" : "变道");
			globalLogger(Logger::Color::BrightGreen) << "合法：" << ((potentialStopLine.isValid == true) ? "合法" : "不合法");
			globalLogger(Logger::Color::BrightGreen) << "后方：" << ((potentialStopLine.isBehind == true) ? "位于主车后面" : "位于主车前面");
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
					LOG << "静止的";

					getLaneChangePath(potentialStopLine);
				}
				else // 移动的半停止线
				{
					LOG << "移动的";

					//getLaneChangePath(potentialStopLine, potentialStopLine.velocityPlanar);
					if (mobileSingleStoplineSwitcher.state) getLaneChangePath(potentialStopLine);
				}
			}
			/* 吸引线，主车的策略是变道 */
			else if (potentialStopLine.type == StopLine::Type::Attract && potentialStopLine.strategy == StopLine::Strategy::LaneChange)
			{
				if (potentialStopLine.toVehicleDist < laneChangeDistThres) // 距离小于阈值，开始变道
				{
					SSD::SimPoint3DVector wayPoints;
					wayPoints.push_back(mainVehicle.pt);
					wayPoints.push_back(potentialStopLine.mid);
					ASSERT(equidistantSampling(wayPoints, targetPath, 0.2), "吸引线等距采样生成路径失败");

					// 已经到达目标点，规划到终点的轨迹
					if (UtilMath::planarDistance(mainVehicle.pt, potentialStopLine.mid) < achieveThres)
					{
						SSD::SimPoint3D destinationPos(initialPath.back());
						initialPath.clear(); // 更新起点和终点
						initialPath.push_back(mainVehicle.pt);
						initialPath.push_back(destinationPos);
						targetPath.clear();
						ASSERT(SimOneAPI::GenerateRoute(initialPath, validWayPoints, targetPath), "使用 A* 生成主车路径规划失败");
					}
				}
			}
		}

		// 超车速度规划（这个只能处理主车即将变道，而主车后方又有一辆快速行驶的对手车辆的情况，不能处理移动的半停止线）
		// 还需要处理那些在主车前方行驶的移动半停止线
		// calculateOvertakingSpeed(obstacleList);

		if (overtakeSwitcher.state) /* 邻域超车 */
		{
			// 如果左前邻域有车，且主车轨迹的终点落在左前邻域内
			if (!mainVehicle.neighborhood.leftFront.empty() &&
				isSameRoadId(m_SampleGetNearMostLane(targetPath.back()), mainVehicle.laneLink.leftNeighborLaneName))
			{
				// 找到左前邻域内速度最慢、但是不静止的车
				const Obstacle* vehicle = mainVehicle.neighborhood.findSlowestMovingObstacle(mainVehicle.neighborhood.leftFront);
				LOG << "左前有车";
				if (vehicle != nullptr) // 如果能够找到一辆非静止的对手车辆，才继续往下讨论
				{
					// 如果该对手车辆的轨迹与主车将来的轨迹相交
					if (isTrajInterfere(targetPath, *vehicle) ||
						UtilGeometry::curvesIntersect(vehicle->predictionGM, targetPath)
						|| getDistT(targetPath.back(), vehicle->predictionGM.back()) < 1.1
						)
						//UtilGeometry::curvesIntersect(vehicle->predictionPtr->trajectory, vehicle->predictionPtr->trajectorySize, targetPath))
					{
						LOG << "与左前车轨迹相交";
						static AdaptiveFollowing LFfollower(followingKp, followingKi, followingKd, 20.0f); // 左前邻域跟车器
						LFfollower.update(vehicle->velocityPlanar, UtilMath::planarDistance(mainVehicle.pt, vehicle->pt));
						pControl->throttle = mainVehicle.neighborhood.vlf = LFfollower.getSpeed();
						mainVehicle.neighborhood.blf = true;
					}
				}
			}

			// 如果右前邻域有车，且主车轨迹的终点落在右前邻域内
			if (!mainVehicle.neighborhood.rightFront.empty() &&
				isSameRoadId(m_SampleGetNearMostLane(targetPath.back()), mainVehicle.laneLink.rightNeighborLaneName))
			{
				// 找到右前邻域内速度最慢、但是不静止的车
				const Obstacle* vehicle = mainVehicle.neighborhood.findSlowestMovingObstacle(mainVehicle.neighborhood.rightFront);
				LOG << "如果右前邻域有车，且主车轨迹的终点落在右前邻域内";

				if (vehicle != nullptr) // 如果能够找到一辆非静止的对手车辆，才继续往下讨论
				{
					///*	for(auto&point: vehicle->predictionGM)
					//	{
					//		LOG << point.x << "," << point.y;
					//	}*/
						// 如果该对手车辆的轨迹与主车将来的轨迹相交
					if (isTrajInterfere(targetPath, *vehicle) ||
						UtilGeometry::curvesIntersect(vehicle->predictionGM, targetPath)
						|| getDistT(targetPath.back(), vehicle->predictionGM.back()) < 1.1
						)

						//UtilGeometry::curvesIntersect(vehicle->predictionPtr->trajectory, vehicle->predictionPtr->trajectorySize, targetPath))
					{
						LOG << "在右前测区域";
						static AdaptiveFollowing RFfollower(followingKp, followingKi, followingKd, 20.0f); // 右前邻域跟车器
						RFfollower.update(vehicle->velocityPlanar, UtilMath::planarDistance(mainVehicle.pt, vehicle->pt));
						pControl->throttle = mainVehicle.neighborhood.vrf = RFfollower.getSpeed();
						mainVehicle.neighborhood.brf = true;
						LOG << "如果右前邻域有车，且主车轨迹的终点落在右前邻域内，该对手车辆的轨迹与主车将来的轨迹相交";
					}
				}
			}
			// 如果前邻域有车，且主车轨迹的终点落在前邻域内
			if (!mainVehicle.neighborhood.front.empty()
				&& isSameRoadId(m_SampleGetNearMostLane(targetPath.back()), mainVehicle.laneID))
				// 优化建议：如果跨过路口会出问题！！！！

			{
				LOG << "前面有东西";
				// 找到前邻域内速度最慢、但是不静止的车
				const Obstacle* vehicle = mainVehicle.neighborhood.findSlowestMovingObstacle(mainVehicle.neighborhood.front);
				if (vehicle != nullptr) // 如果能够找到一辆非静止的对手车辆，才继续往下讨论
				{
					// 如果该对手车辆的轨迹与主车将来的轨迹相交
					/*if (isTrajInterfere(targetPath, *vehicle) ||
						UtilGeometry::curvesIntersect(vehicle->predictionPtr->trajectory, vehicle->predictionPtr->trajectorySize, targetPath))*/
					{
						LOG << "前有车";
						static AdaptiveFollowing Ffollower(followingKp, followingKi, followingKd, 20.0f); // 前邻域跟车器
						Ffollower.update(vehicle->velocityPlanar, UtilMath::planarDistance(mainVehicle.pt, vehicle->pt));
						pControl->throttle = mainVehicle.neighborhood.vf = Ffollower.getSpeed();
						mainVehicle.neighborhood.bf = true;
					}
				}
			}

			// 如果后邻域有车
			if (!mainVehicle.neighborhood.back.empty())
			{
				// 找到后邻域内速度最快、但是不静止的车
				const Obstacle* vehicle = mainVehicle.neighborhood.findFastestMovingObstacle(mainVehicle.neighborhood.back);
				if (vehicle != nullptr) // 如果能够找到一辆非静止的对手车辆，才继续往下讨论
				{
					// 如果该对手车辆的速度大于主车
					if (vehicle->velocityPlanar > caseTargetSpeed)
					{
						LOG << "后有快车";
						LOG << "后车是否与主车干涉：" << isTrajInterfere(targetPath, *vehicle);
						LOG << "后车是否与主车轨迹相交：" << UtilGeometry::curvesIntersect(vehicle->predictionGM, targetPath);
						// 如果该对手车辆的轨迹与主车将来的轨迹相交
						if (isTrajInterfere(targetPath, *vehicle) ||
							UtilGeometry::curvesIntersect(vehicle->predictionGM, targetPath)
							|| getDistT(targetPath.back(), vehicle->predictionGM.back()) < 1.1
							)
							//UtilGeometry::curvesIntersect(vehicle->predictionPtr->trajectory, vehicle->predictionPtr->trajectorySize, targetPath))
						{
							LOG << "后有快车与主车相交";
							static AdaptiveLeading BFollowing(followingKp, followingKi, followingKd, 20.0f); // 后邻域反向跟车器
							BFollowing.update(vehicle->velocityPlanar, UtilMath::planarDistance(mainVehicle.pt, vehicle->pt));
							pControl->throttle = mainVehicle.neighborhood.vb = BFollowing.getSpeed();
							mainVehicle.neighborhood.bb = true;
						}
					}
				}
			}

			// 如果左后邻域有车
			if (!mainVehicle.neighborhood.leftBack.empty())
			{
				// 找到左后邻域内速度最快、但是不静止的车
				const Obstacle* vehicle = mainVehicle.neighborhood.findFastestMovingObstacle(mainVehicle.neighborhood.leftBack);

				if (vehicle != nullptr) // 如果能够找到一辆非静止的对手车辆，才继续往下讨论
				{
					LOG << "左后有车";
					// 如果该对手车辆的速度大于主车
					if (vehicle->velocityPlanar > caseTargetSpeed)
					{
						LOG << "左后后车是否与主车干涉：" << isTrajInterfere(targetPath, *vehicle);
						LOG << "左后后车是否与主车轨迹相交：" << UtilGeometry::curvesIntersect(vehicle->predictionGM, targetPath);
						// 如果该对手车辆的轨迹与主车将来的轨迹相交
						if (isTrajInterfere(targetPath, *vehicle) ||
							UtilGeometry::curvesIntersect(vehicle->predictionGM, targetPath)
							|| getDistT(targetPath.back(), vehicle->predictionGM.back()) < 1.1
							)
							//UtilGeometry::curvesIntersect(vehicle->predictionPtr->trajectory, vehicle->predictionPtr->trajectorySize, targetPath))
						{
							LOG << "左后有车相交";

							static AdaptiveLeading LBFollowing(followingKp, followingKi, followingKd, 20.0f); // 左后邻域反向跟车器
							LBFollowing.update(vehicle->velocityPlanar, UtilMath::planarDistance(mainVehicle.pt, vehicle->pt));
							pControl->throttle = mainVehicle.neighborhood.vlb = LBFollowing.getSpeed();
							mainVehicle.neighborhood.blb = true;
						}
					}
				}
			}

			// 如果右后邻域有车
			if (!mainVehicle.neighborhood.rightBack.empty())
			{
				// 找到右后邻域内速度最快、但是不静止的车
				const Obstacle* vehicle = mainVehicle.neighborhood.findFastestMovingObstacle(mainVehicle.neighborhood.rightBack);

				if (vehicle != nullptr) // 如果能够找到一辆非静止的对手车辆，才继续往下讨论
				{
					// 如果该对手车辆的速度大于主车

					if (vehicle->velocityPlanar > caseTargetSpeed)
					{
						LOG << "右后有车";
						LOG << "右后后车是否与主车干涉：" << isTrajInterfere(targetPath, *vehicle);
						LOG << "右后后车是否与主车轨迹相交：" << UtilGeometry::curvesIntersect(vehicle->predictionGM, targetPath);
						// 如果该对手车辆的轨迹与主车将来的轨迹相交
						if (isTrajInterfere(targetPath, *vehicle) ||
							UtilGeometry::curvesIntersect(vehicle->predictionGM, targetPath)
							|| getDistT(targetPath.back(), vehicle->predictionGM.back()) < 1.1
							)
						{
							LOG << "右后有车与主车相交";
							static AdaptiveLeading RBFollowing(followingKp, followingKi, followingKd, 20.0f); // 右后邻域反向跟车器
							RBFollowing.update(vehicle->velocityPlanar, UtilMath::planarDistance(mainVehicle.pt, vehicle->pt));
							pControl->throttle = mainVehicle.neighborhood.vrb = RBFollowing.getSpeed();
							mainVehicle.neighborhood.brb = true;
						}
					}
				}
			}

			// 如果左邻域有车
			if (!mainVehicle.neighborhood.left.empty())
			{
				// 找到左邻域内速度最慢、但是不静止的车
				const Obstacle* vehicle = mainVehicle.neighborhood.findSlowestMovingObstacle(mainVehicle.neighborhood.left);

				if (vehicle != nullptr) // 如果能够找到一辆非静止的对手车辆，才继续往下讨论
				{
					LOG << "左有车";

					// 如果该对手车辆的轨迹与主车将来的轨迹相交
					if (isTrajInterfere(targetPath, *vehicle) ||
						UtilGeometry::curvesIntersect(vehicle->predictionGM, targetPath)
						|| getDistT(targetPath.back(), vehicle->predictionGM.back()) < 1.1
						)
						//UtilGeometry::curvesIntersect(vehicle->predictionPtr->trajectory, vehicle->predictionPtr->trajectorySize, targetPath))
					{
						LOG << "左有车与主车相交";
						static AdaptiveLeading LFollowing(followingKp, followingKi, followingKd, 20.0f); // 左邻域反向跟车器
						LFollowing.update(vehicle->velocityPlanar, UtilMath::planarDistance(mainVehicle.pt, vehicle->pt));
						pControl->throttle = mainVehicle.neighborhood.vl = LFollowing.getSpeed();
						mainVehicle.neighborhood.bl = true;
					}
				}
			}

			// 如果右邻域有车
			if (!mainVehicle.neighborhood.right.empty())
			{
				// 找到右邻域内速度最慢、但是不静止的车
				const Obstacle* vehicle = mainVehicle.neighborhood.findSlowestMovingObstacle(mainVehicle.neighborhood.right);

				if (vehicle != nullptr) // 如果能够找到一辆非静止的对手车辆，才继续往下讨论
				{
					LOG << "右有车";

					// 如果该对手车辆的轨迹与主车将来的轨迹相交
					if (isTrajInterfere(targetPath, *vehicle) ||
						UtilGeometry::curvesIntersect(vehicle->predictionGM, targetPath)
						|| getDistT(targetPath.back(), vehicle->predictionGM.back()) < 1.1
						)
						//UtilGeometry::curvesIntersect(vehicle->predictionPtr->trajectory, vehicle->predictionPtr->trajectorySize, targetPath))
					{
						LOG << "右有车与主车相交";

						static AdaptiveLeading RFollowing(followingKp, followingKi, followingKd, 20.0f); // 右邻域反向跟车器
						RFollowing.update(vehicle->velocityPlanar, UtilMath::planarDistance(mainVehicle.pt, vehicle->pt));
						pControl->throttle = mainVehicle.neighborhood.vr = RFollowing.getSpeed();
						mainVehicle.neighborhood.br = true;
					}
				}
			}
			// 1. LF、LB / RF、RB
			if (mainVehicle.neighborhood.blf && mainVehicle.neighborhood.blb)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vlf, mainVehicle.neighborhood.vlb);
			}
			if (mainVehicle.neighborhood.brf && mainVehicle.neighborhood.brb)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vrf, mainVehicle.neighborhood.vrb);
			}

			// 2. L、LB / R、RB
			if (mainVehicle.neighborhood.bl && mainVehicle.neighborhood.blb)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vl, mainVehicle.neighborhood.vlb);
			}
			if (mainVehicle.neighborhood.br && mainVehicle.neighborhood.brb)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vr, mainVehicle.neighborhood.vrb);
			}

			// 3. B、LB / B、RB
			if (mainVehicle.neighborhood.bb && mainVehicle.neighborhood.blb)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vb, mainVehicle.neighborhood.vlb);
			}
			if (mainVehicle.neighborhood.bb && mainVehicle.neighborhood.brb)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vb, mainVehicle.neighborhood.vrb);
			}

			// 4. F、LB / F、RB
			if (mainVehicle.neighborhood.bf && mainVehicle.neighborhood.blb)
			{
				pControl->throttle = std::min(mainVehicle.neighborhood.vf, mainVehicle.neighborhood.vlb);
			}
			if (mainVehicle.neighborhood.bf && mainVehicle.neighborhood.brb)
			{
				pControl->throttle = std::min(mainVehicle.neighborhood.vf, mainVehicle.neighborhood.vrb);
			}

			// 5. LB、RB
			if (mainVehicle.neighborhood.blb && mainVehicle.neighborhood.brb)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vlb, mainVehicle.neighborhood.vrb);
			}

			// 6. R、LB / L、RB
			if (mainVehicle.neighborhood.br && mainVehicle.neighborhood.blb)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vr, mainVehicle.neighborhood.vlb);
			}
			if (mainVehicle.neighborhood.bl && mainVehicle.neighborhood.brb)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vl, mainVehicle.neighborhood.vrb);
			}

			// 7. LB、RF / RB、LF
			if (mainVehicle.neighborhood.blb && mainVehicle.neighborhood.brf)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vlb, mainVehicle.neighborhood.vrf);
			}
			if (mainVehicle.neighborhood.brb && mainVehicle.neighborhood.blf)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vrb, mainVehicle.neighborhood.vlf);
			}

			// 8. L、LF / R、RF
			if (mainVehicle.neighborhood.bl && mainVehicle.neighborhood.blf)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vl, mainVehicle.neighborhood.vlf);
			}
			if (mainVehicle.neighborhood.br && mainVehicle.neighborhood.brf)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vr, mainVehicle.neighborhood.vrf);
			}

			// 9. B、L / B、R
			if (mainVehicle.neighborhood.bb && mainVehicle.neighborhood.bl)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vb, mainVehicle.neighborhood.vl);
			}
			if (mainVehicle.neighborhood.bb && mainVehicle.neighborhood.br)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vb, mainVehicle.neighborhood.vr);
			}

			// 10. F、L / F、R
			if (mainVehicle.neighborhood.bf && mainVehicle.neighborhood.bl)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vf, mainVehicle.neighborhood.vl);
			}
			if (mainVehicle.neighborhood.bf && mainVehicle.neighborhood.br)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vf, mainVehicle.neighborhood.vr);
			}

			// 11. L、R
			if (mainVehicle.neighborhood.bl && mainVehicle.neighborhood.br)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vl, mainVehicle.neighborhood.vr);
			}

			// 12. L、RF / R、LF
			if (mainVehicle.neighborhood.bl && mainVehicle.neighborhood.brf)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vl, mainVehicle.neighborhood.vrf);
			}
			if (mainVehicle.neighborhood.br && mainVehicle.neighborhood.blf)
			{
				pControl->throttle = std::max(mainVehicle.neighborhood.vr, mainVehicle.neighborhood.vlf);
			}

			// 13. B、LF / B、RF
			if (mainVehicle.neighborhood.bb && mainVehicle.neighborhood.blf)
			{
				pControl->throttle = std::min(mainVehicle.neighborhood.vb, mainVehicle.neighborhood.vlf);
			}
			if (mainVehicle.neighborhood.bb && mainVehicle.neighborhood.brf)
			{
				pControl->throttle = std::min(mainVehicle.neighborhood.vb, mainVehicle.neighborhood.vrf);
			}

			// 14. F、LF / F、RF
			if (mainVehicle.neighborhood.bf && mainVehicle.neighborhood.blf)
			{
				pControl->throttle = std::min(mainVehicle.neighborhood.vf, mainVehicle.neighborhood.vlf);
			}
			if (mainVehicle.neighborhood.bf && mainVehicle.neighborhood.brf)
			{
				pControl->throttle = std::min(mainVehicle.neighborhood.vf, mainVehicle.neighborhood.vrf);
			}

			// 15. LF、RF
			if (mainVehicle.neighborhood.blf && mainVehicle.neighborhood.brf)
			{
				pControl->throttle = std::min(mainVehicle.neighborhood.vlf, mainVehicle.neighborhood.vrf);
			}

			// 16. B、F
			if (mainVehicle.neighborhood.bb && mainVehicle.neighborhood.bf)
			{
				pControl->throttle = std::min(mainVehicle.neighborhood.vb, mainVehicle.neighborhood.vf);
			}
		}

		mainVehicle.drive();

		for (size_t i = 0, ie = stopLineList.size(); i < ie; ++i)
		{
			/*if (stopLineList[i].isValid)*/
			{
				globalLogger(Logger::Color::BrightGreen) << "——————————————————————————————————————";
				globalLogger(Logger::Color::BrightGreen) << "停止线 " << i << "：";
				globalLogger(Logger::Color::BrightGreen) << "类型：" << (stopLineList[i].type == StopLine::Type::Whole ? "全停止线" : (stopLineList[i].type == StopLine::Type::Single ? "半停止线" : "吸引线"));
				globalLogger(Logger::Color::BrightGreen) << "策略：" << ((stopLineList[i].strategy == StopLine::Strategy::StopStart) ? "停走" : "变道");
				globalLogger(Logger::Color::BrightGreen) << "合法：" << ((stopLineList[i].isValid == true) ? "合法" : "不合法");
				globalLogger(Logger::Color::BrightGreen) << "后方：" << ((stopLineList[i].isBehind == true) ? "位于主车后面" : "位于主车前面");
				globalLogger(Logger::Color::BrightGreen) << "起点：（" << stopLineList[i].src.x << "，" << stopLineList[i].src.y << "）";
				globalLogger(Logger::Color::BrightGreen) << "终点：（" << stopLineList[i].dst.x << "，" << stopLineList[i].dst.y << "）";
			}
		}

		SimOneAPI::NextFrame(frame);
	}

	return 0;
}
