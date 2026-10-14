#include "main.h"

#include "StopController.h"

int main_manual(void)
{
	if (strategyPoint.empty())
	{
		globalLogger(Logger::Color::BrightMagenta) << "Empty strategy: manual mode cannot start";
		return 1;
	}
	int currentIdx = 0;
	int pendingIdx = 0; // 尚未建成路径的目标点编号，-1 表示无；首帧先建立到第 0 个点的路径
	targetPath.clear(); // 本模式不沿用其他模式或上次运行的旧路径
	timer.tic(); // 开始计时

	/* 生成从主车到第 idx 个点的路径；先写入局部路径，成功后才替换 targetPath */
	auto buildPathTo = [](size_t idx) {
		initialPath.clear(); // 更新起点和终点
		validWayPoints.clear();
		initialPath.push_back(mainVehicle.pt);
		initialPath.push_back(strategyPoint[idx].Pos);

		SSD::SimPoint3DVector newPath;
		bool ok = false;
		/* 根据策略点的 pathMode 决定用 A* 还是插值 */
		if (strategyPoint[idx].pathMode == -1)
		{
			ok = SimOneAPI::GenerateRoute(initialPath, validWayPoints, newPath);
			if (!ok) globalLogger(Logger::Color::BrightMagenta) << "◆ 使用 A* 算法生成路径失败";
		}
		else if (strategyPoint[idx].pathMode == 1)
		{
			ok = equidistantSampling(initialPath, newPath, 0.2); // 成功返回 true
			if (!ok) globalLogger(Logger::Color::BrightMagenta) << "◆ 使用插值生成轨迹失败";
		}
		else globalLogger(Logger::Color::BrightMagenta) << "◆ 未知的 pathMode：" << strategyPoint[idx].pathMode;

		if (ok && newPath.size() < 2) // 少于两个点无法循迹
		{
			ok = false;
			globalLogger(Logger::Color::BrightMagenta) << "◆ 生成的路径点少于 2 个";
		}
		if (ok) targetPath = newPath;
		return ok;
	};

	double stopTime;
	competition::StopController stopController;
	int stopControllerIdx = -1;
	std::set<int> stoppedPointIds; // 记录已经处理过的点编号

	while (true)
	{
		++frameCount;
		int frame = SimOneAPI::Wait();
		float Fps = calculateFps(frameCount, timer.get());
		globalLogger(Logger::Color::BrightBlue) << "\n\nFrame = " << frameCount << "，Fps = " << Fps;

		recordEvaluation(); // 更新 NEVC 系统评分

		const bool obstaclesReady = updateObstacleData();
		const bool vehicleReady = mainVehicle.update();
		if (!obstaclesReady || !vehicleReady) // 数据缺失时不得沿用旧观测累计停车时间
		{
			stopController.invalidate();
			obstacleList.clear();
			stopLineList.clear();
			pControl->throttle = 0;
			pControl->steering = 0;
			pControl->gear = mainVehicle.reverse ? ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse
				: ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
			pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
			if (!SimOneAPI::SetDrive(mainVehicle.id, pControl.get()))
				LOG << "SetDrive failed while requesting safe stop";
			LOG << "GPS或障碍物更新失败：停车并重置连续驻停计时";
			SimOneAPI::NextFrame(frame);
			continue;
		}

		waitInitial(); // 等待仿真平台初始化完成

		steerKpUse = steerKp;
		steerKdUse = steerKd;
		obstacleList.clear();
		stopLineList.clear();

		float distNextPoint = 0.0f; // 到下一个循迹点之间的 ST 距离

		distNextPoint = getDistS(mainVehicle.pt, strategyPoint[currentIdx].Pos); // 到下一个循迹点之间的 ST 距离

		/* 如果 [dat1] == -1，[dat2] = defaut，否则 [dat2] = [dat1] */
#define JudgeJsonData(DATA1, DATA2, DATA3) if (strategyPoint[currentIdx].DATA1 == -1) DATA2 = DATA3; else DATA2 = strategyPoint[currentIdx].DATA1
		auto applyPointControl = [&]() {
			JudgeJsonData(kp, steerKpUse, steerKp);
			JudgeJsonData(stopTime, stopTime, -1);
			JudgeJsonData(speed, pControl->throttle, caseTargetSpeed);
			if (stopTime <= 0 || stoppedPointIds.count(currentIdx) > 0) return false;
			if (stopControllerIdx != currentIdx) {
				stopController.reset(stopTime);
				stopControllerIdx = currentIdx;
			}
			// SimOne_Data::timestamp is milliseconds (vendor API reference).
			// https://simone-docs.51sim.com/SimOneCpp/struct_sim_one___data/index.html
			// .stg duration is a minimum; mapping to each 2027 rule is separate.
			const auto stop = stopController.update(pGps->timestamp * 0.001, mainVehicle.speed,
				UtilMath::planarDistance(mainVehicle.pt, strategyPoint[currentIdx].Pos) <= achieveThres,
				pControl->throttle, true);
			pControl->throttle = stop.targetSpeed;
			if (stop.ready) stoppedPointIds.insert(currentIdx);
			LOG << "驻停状态=" << static_cast<int>(stop.phase) << " 连续停稳=" << stop.heldSeconds << " 秒";
			return !stop.ready;
		};
		const bool stopPending = applyPointControl();

		/* 若当前点已接近目标点（≤5m），且后续还有点，准备切换到下一个循迹点 */
		/* 还要求停止的过程中不允许切换到下一个点；首帧的 pendingIdx 已是 0，不会跳过第一个点 */
		if (pendingIdx < 0 && distNextPoint <= achieveThres && currentIdx + 1 < strategyPoint.size() && !stopPending)
		{
			pendingIdx = currentIdx + 1;
		}

		/* 建立到待切换点的路径：成功才切换编号；失败保持原编号并停车，下一帧重试 */
		if (pendingIdx >= 0)
		{
			if (buildPathTo(pendingIdx))
			{
				bool switched = pendingIdx != currentIdx;
				currentIdx = pendingIdx;
				pendingIdx = -1;
				globalLogger(Logger::Color::BrightGreen) << "※ 切换到下一个循迹点，下一个循迹点的编号：" << currentIdx;
				if (switched) // 切点当帧就使用新点的速度和转向参数
				{
					distNextPoint = getDistS(mainVehicle.pt, strategyPoint[currentIdx].Pos);
					applyPointControl(); // 同一帧处理新点的停车义务
				}
			}
			else
			{
				pControl->throttle = 0; // 没有到目标点的路径，停车等待重试
				globalLogger(Logger::Color::BrightMagenta) << "◆ 到第 " << pendingIdx << " 个循迹点的路径未建立，停车并在下一帧重试";
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
		if (targetPath.size() < 2) // 第一个点的路径尚未建成：纯追踪无路径可跟，直接下发停车指令
		{
			pControl->throttle = 0;
			pControl->steering = 0;
			pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
			SimOneAPI::SetDrive(mainVehicle.id, pControl.get());
		}
		else mainVehicle.drive();
		SimOneAPI::NextFrame(frame);
	}

	return 0;
}
