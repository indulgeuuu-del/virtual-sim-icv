#include "main.h"

int main_manual(void)
{
	timer.tic(); // 开始计时

	int stopTime;
	bool isStopping = false;
	bool stopTimerStarted = false;
	std::set<int> stoppedPointIds; // 记录已经处理过的点编号
	std::unordered_map<int, float> brakingStartDists; // 为了使得在每个寻迹点前只会打下一个标签

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

		steerKpUse = steerKp;
		steerKdUse = steerKd;
		obstacleList.clear();
		stopLineList.clear();

		static bool isBraking = false;
		static bool notUpdateNextPt = false;
		static int currentIdx = -1; // 当前循迹点的索引，初始为 -1
		float distNextPoint = 0.0f; // 到下一个循迹点之间的 ST 距离

		/* 如果当前还未设置循迹点，距离设为无限大；否则计算当前位置与当前循迹点的距离 */
		if (currentIdx == -1) distNextPoint = std::numeric_limits<float>::max();
		else distNextPoint = getDistS(mainVehicle.pt, strategyPoint[currentIdx].Pos); // 到下一个循迹点之间的 ST 距离

		/* 如果 [dat1] == -1，[dat2] = defaut，否则 [dat2] = [dat1] */
#define JudgeJsonData(DATA1, DATA2, DATA3) if (strategyPoint[currentIdx].DATA1 == -1) DATA2 = DATA3; else DATA2 = strategyPoint[currentIdx].DATA1
		JudgeJsonData(kp, steerKpUse, steerKp);
		JudgeJsonData(stopTime, stopTime, -1);

		if (stopTime <= 0 || stoppedPointIds.count(currentIdx) > 0) // 无需停止 || 已停止过该点
		{
			isStopping = false;
			stopTimerStarted = false;
			JudgeJsonData(speed, pControl->throttle, caseTargetSpeed);
		}
		else // 否则，计算制动距离 d0 并判断是否进入停止区间
		{
			if (brakingStartDists.count(currentIdx) == 0)
			{
				float d0 = 0.05272f * mainVehicle.speed * mainVehicle.speed + 0.06714f * mainVehicle.speed - 0.02188f;
				brakingStartDists[currentIdx] = d0;
			}

			float d0 = brakingStartDists[currentIdx];
			if (distNextPoint - 4.38046 <= d0 + 7) // 当车距小于期望停止点距离时
			{
				if (!isStopping)
				{
					// 第一次触发“停止”逻辑
					timer.mark("stop_timer" + std::to_string(currentIdx)); // 标记定时器，开始“停止”状态
					isStopping = true;
					stopTimerStarted = true;

					LOG << "○ 停止的追踪点 [" << currentIdx << "] 开始停止，stopTime = " << stopTime << " 秒";
					LOG << "○ 期望距离：" << d0 << "，距离：" << distNextPoint << "，点编号：" << currentIdx;
				}

				// 正在“停止中”，判断是否到了停止时长
				if (stopTimerStarted && timer.isTriggered("stop_timer" + std::to_string(currentIdx), static_cast<float>(stopTime), Timer::Unit::Seconds))
				{
					timer.removeMark("stop_timer" + std::to_string(currentIdx));
					isStopping = false;
					stopTimerStarted = false;
					stoppedPointIds.insert(currentIdx);
					JudgeJsonData(speed, pControl->throttle, caseTargetSpeed);

					LOG << "○ 停止时间到，重新启动";
				}
				else
				{
					pControl->throttle = 0; // 停止状态，油门归零
				}
			}
			else // 不在停止区域范围内，重置状态（防止提前触发）
			{
				isStopping = false;
				stopTimerStarted = false;
				JudgeJsonData(speed, pControl->throttle, caseTargetSpeed);
			}
		}

		/* 第一次进入循迹逻辑，初始化起点、终点并生成路径 */
		if (currentIdx == -1 && currentIdx + 1 < strategyPoint.size() && !notUpdateNextPt) // 刚开始循迹的时候
		{
			currentIdx = 0;
			initialPath.clear(); // 更新起点和终点
			targetPath.clear(); // 清空旧路径
			validWayPoints.clear();

			initialPath.push_back(mainVehicle.pt);
			initialPath.push_back(strategyPoint[0].Pos);

			/* 根据策略点的 pathMode 决定用 A* 还是插值 */
			if (strategyPoint[0].pathMode == -1 && !SimOneAPI::GenerateRoute(initialPath, validWayPoints, targetPath)) // 生成到第一个循迹点的路径
			{
				globalLogger(Logger::Color::BrightMagenta) << "◆ 使用 A* 算法生成路径失败";
			}
			else if (strategyPoint[0].pathMode == 1 && equidistantSampling(initialPath, targetPath, 0.2))
			{
				globalLogger(Logger::Color::BrightMagenta) << "◆ 使用插值生成轨迹失败";
			}
			else
			{
				globalLogger(Logger::Color::BrightGreen) << "※ 切换到下一个循迹点，下一个循迹点的编号：" << currentIdx;
			}
		}
		/* 若当前点已接近目标点（≤5m），且后续还有点，切换到下一个循迹点，重新生成路径 */
		else if (distNextPoint <= 5 && currentIdx + 1 < strategyPoint.size() && !notUpdateNextPt)
		{
			++currentIdx; // 切换到下一个循迹点
			initialPath.clear(); // 更新起点和终点
			targetPath.clear(); // 清空旧路径
			validWayPoints.clear();

			initialPath.push_back(mainVehicle.pt);
			initialPath.push_back(strategyPoint[currentIdx].Pos);

			/* 根据策略点的 pathMode 决定用 A* 还是插值 */
			if (strategyPoint[currentIdx].pathMode == -1 && !SimOneAPI::GenerateRoute(initialPath, validWayPoints, targetPath)) // 生成到第一个循迹点的路径
			{
				globalLogger(Logger::Color::BrightMagenta) << "◆ 使用 A* 算法生成路径失败";
			}
			else if (strategyPoint[currentIdx].pathMode == 1 && equidistantSampling(initialPath, targetPath, 0.2))
			{
				globalLogger(Logger::Color::BrightMagenta) << "◆ 使用插值生成轨迹失败";
			}
			else
			{
				globalLogger(Logger::Color::BrightGreen) << "※ 切换到下一个循迹点，下一个循迹点的编号：" << currentIdx;
			}
		}

		/* 信息总览 */
		globalLogger(Logger::Color::BrightGreen) << "▲ 信息总览：";
		LOG << "\t△ 正在导航至第 " << currentIdx << " 个循迹点：（" << strategyPoint[currentIdx].Pos.x << "，" << strategyPoint[currentIdx].Pos.y << "）";
		LOG << "\t△ 循迹模式为：" << ((strategyPoint[currentIdx].pathMode == -1) ? "A* 算法" : "插值算法");
		LOG << "\t△ 停车时间为：" << ((strategyPoint[currentIdx].stopTime == -1) ? "不停车" : std::to_string(strategyPoint[currentIdx].stopTime));
		LOG << "\t△ 循迹速度为：" << strategyPoint[currentIdx].speed;
		LOG << "\t△ 使用的 PID 参数：P = " << ((strategyPoint[currentIdx].kp == -1) ? "默认" : std::to_string(strategyPoint[currentIdx].kp));
		LOG << "\t△ 距离下一个循迹点的距离为：" << distNextPoint;
		LOG << "\t△ 主车坐标为：（" << mainVehicle.pt.x << "，" << mainVehicle.pt.y << "）";
		mainVehicle.drive();
		SimOneAPI::NextFrame(frame);
	}

	return 0;
}