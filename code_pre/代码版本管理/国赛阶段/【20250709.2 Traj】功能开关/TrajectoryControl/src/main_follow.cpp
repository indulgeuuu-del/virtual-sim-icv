#include "main.h"

int main_follow(void)
{
	adaptiveFollowing = AdaptiveFollowing(followingKp, followingKi, followingKd, 45); // 跟车 PID

	while (true) // 跟车场景
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

	return 0;
}