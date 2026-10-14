#include "main.h"

int main_follow(void)
{
	initNavigation(validWayPoints, targetPath); // 规划主车路径
	adaptiveFollowing = AdaptiveFollowing(followingKp, followingKi, followingKd, 45); // 跟车 PID

	while (true) // 跟车场景
	{
		++frameCount;
		int frame = SimOneAPI::Wait();

		globalLogger(Logger::Color::BrightBlue) << "\n\nFrame = " << frameCount;

		recordEvaluation(); // 更新 NEVC 系统评分

		const bool obstaclesReady = updateObstacleData(); // 本帧读取结果，不是历史初始化标记
		const bool vehicleReady = mainVehicle.update();
		if (!obstaclesReady || !vehicleReady)
		{
			obstacleList.clear(); // 不保留失效的帧间目标
			adaptiveFollowing = AdaptiveFollowing(followingKp, followingKi, followingKd, 45);
			pControl->throttle = 0;
			pControl->steering = 0;
			pControl->gear = mainVehicle.reverse ? ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse
				: ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
			pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
			if (!SimOneAPI::SetDrive(mainVehicle.id, pControl.get()))
				LOG << "SetDrive failed while requesting safe stop";
			LOG << "Following input unavailable: stop until both streams recover";
			SimOneAPI::NextFrame(frame);
			continue;
		}

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

	return 0;
}
