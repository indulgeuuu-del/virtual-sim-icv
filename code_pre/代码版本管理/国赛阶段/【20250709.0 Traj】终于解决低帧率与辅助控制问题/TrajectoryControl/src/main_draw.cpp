#include "main.h"

int main_draw(void)
{
	initNavigation(validWayPoints, targetPath); // 规划主车路径

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

	while (true)
	{
		++frameCount;
		int frame = SimOneAPI::Wait();

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