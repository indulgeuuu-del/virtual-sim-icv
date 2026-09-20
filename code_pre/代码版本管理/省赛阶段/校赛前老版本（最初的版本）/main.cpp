#include <iostream>
#include <fstream>
#include "SimOneServiceAPI.h"
#include "SimOneSensorAPI.h"
#include "SimOneHDMapAPI.h"
#include "SimOneEvaluationAPI.h"
#include "UtilDriver.h"
#include "UtilMath.h"
#include "function.h"
#include "SampleGetLaneST.h"
#include "define.h"
#include "json.hpp"
#include "controller.hpp"
#include "utility.h"
#include "process.h"
#include "stopline.h"
#include "obstacle.h"

double brakeSpeed = 0.0;
double brakeDist;

int main(int argc, char* argv[])
{
/************************************************** 初始化 **************************************************/
#ifndef INITIALIZATION // 初始化操作
	//globalLogger.enable(false);
	// Json 初始化
	nlohmann::json caseJs;
	globalLogger(Logger::Color::BrightGreen) << "只打印了这一行说明没改json文件路径";
	std::ifstream("D:/Sim-One/SimOneAPI/ADAS/TrajectoryControl/param.json") >> caseJs;

	JSON_LOAD_VALUE(mainVehicleName);
	JSON_LOAD_VALUE(groupingDistThres);
	JSON_LOAD_VALUE(stopLineDistThres);
	JSON_LOAD_VALUE(laneChangeSteeringFactor);
	JSON_LOAD_VALUE(laneChangeSteeringMax);
	JSON_LOAD_VALUE(speedKp);
	JSON_LOAD_VALUE(speedKi);
	JSON_LOAD_VALUE(speedKd);
	JSON_LOAD_VALUE(steerKp);
	JSON_LOAD_VALUE(steerKi);
	JSON_LOAD_VALUE(steerKd);
	JSON_LOAD_VALUE(followingKp);
	JSON_LOAD_VALUE(followingKi);
	JSON_LOAD_VALUE(followingKd);
	JSON_LOAD_VALUE(continuousCase);
	JSON_LOAD_VALUE(perceptionRange);
	JSON_LOAD_VALUE(cumulative_singel);
	JSON_LOAD_VALUE(cumulative_double);
	JSON_LOAD_VALUE(chasingLimitDist);
	int case_stop_followID;//停走场景的id
	JSON_LOAD_VALUE(case_stop_followID);
	caseNormalID = caseJs["caseNormalID"].get<std::vector<int>>(); // 哪些场景是 Normal 型的
	caseFollowingID = caseJs["caseFollowingID"].get<std::vector<int>>(); // 哪些场景是需要稳定跟车的
	parseCreatStopLine(caseJs, manualStopLineReservoir);

	//globalLogger.enable(false);

	// PID 初始化
	incPid_t speedPID(speedKp, speedKi, speedKd);
	posPid_t steerPID(steerKp, steerKi, steerKd);
	AdaptiveFollowing adaptiveFollowing(followingKp, followingKi, followingKd, 45);

	// SimOne 初始化
	SimOneAPI::InitSimOneAPI(MainVehicleId, true);
	SimOneAPI::SetDriverName(MainVehicleId, "autoDrive");
	SimOneAPI::SetDriveMode(MainVehicleId, ESimOne_Drive_Mode_API);
	SimOneAPI::InitEvaluationServiceWithLocalData("0");

	while (true) { // 加载 HDMap
		if (SimOneAPI::LoadHDMap(20)) {
			globalLogger(Logger::Color::BrightMagenta) << "HDMap Information Loaded";
			break;
		}
		globalLogger(Logger::Color::BrightMagenta) << "HDMap Information Loading...";
	}

	// 获取当前案例详细信息
	caseIdx = getCaseIdx();
	getCaseSpeed(caseJs, caseIdx, caseTargetSpeed, caseMinSpeed, caseMaxSpeed);
	globalLogger(Logger::Color::BrightMagenta) << "The current case index is [ " << caseIdx << " ]";
	globalLogger(Logger::Color::BrightMagenta) << "The current case's target speed is " << caseTargetSpeed << " km/h";
	globalLogger(Logger::Color::BrightMagenta) << "The current case's limit speed is " << caseMinSpeed << " km/h < speed < " << caseMaxSpeed << " km/h";

	// SimOneAPI 获取路径点（根据起点和终点规划出一条最短路径）
	// inputPoints -> initialPath 初始路径点的集合
	// initialPath.back() 为终点（亦即目标点）
	if (SimOneAPI::GetWayPoints(MainVehicleId, pWayPoints.get()))
	{
		for (size_t i = 0; i < pWayPoints->wayPointsSize; ++i) {
			SSD::SimPoint3D inputWayPoints(pWayPoints->wayPoints[i].posX, pWayPoints->wayPoints[i].posY, 0);
			initialPath.push_back(inputWayPoints);
		}
	}
	else {
		globalLogger(Logger::Color::BrightMagenta) << "Get mainVehicle wayPoints failed";
		return -1;
	}

	// 根据路径点数量分情况处理
	SSD::SimVector<int> indexOfValidPoints;
	if (pWayPoints->wayPointsSize >= 2) { // 大于等于 2 个路径点，调用 GenerateRoute 生成目标路径
		if (!SimOneAPI::GenerateRoute(initialPath, indexOfValidPoints, targetPath)) {
			globalLogger(Logger::Color::BrightMagenta) << "Generate mainVehicle route failed";
			return -1;
		}
	}
	else if (pWayPoints->wayPointsSize == 1) { // 只有 1 个路径点，获取该路径点所在的车道 ID 和车道信息
		// m_SampleGetNearMostLane 用于获取某个坐标所在的车道信息
		SSD::SimString laneIdInit = m_SampleGetNearMostLane(initialPath[0]);
		HDMapStandalone::MLaneInfo laneInfoInit;
		if (SimOneAPI::GetLaneSample(laneIdInit, laneInfoInit)) { // 如果获取车道信息成功
			targetPath = laneInfoInit.centerLine; // 设置中心线为目标路径
		}
		else { // 如果获取车道信息失败 
			globalLogger(Logger::Color::BrightMagenta) << "Generate mainVehicle initial route failed";
			return -1;
		}
	}
	else { // 如果没有路径点，则报错
		globalLogger(Logger::Color::BrightMagenta) << "Number of wayPoints is zero";
		return -1;
	}

	// 特殊处理：如果当前案例是连续场景
	if (caseIdx == continuousCase)
	{
		FlagType::continuousSample = 1; // 连续场景标志位置为 1
		std::cout << "Continuous Sample On" << std::endl;
	}

	// 获取当前场景得交通灯列表

	SimOneAPI::GetTrafficLightList(trafficLightList);

#endif
/************************************************** 初始化 **************************************************/

	while (true)
	{
		// 获取当前案例详细信息
		++frameCount;
		int frame = SimOneAPI::Wait();
		
		std::cout << std::endl << std::endl;
		globalLogger(Logger::Color::BrightBlue) << "Frame = " << frameCount;

		/* 清空标志位 */
		FlagType::leftLaneExist = FlagType::rightLaneExist = 0;
		FlagType::useDefaultPath = 1;

		SSD::SimPoint3D lane_change_inertial(initialPath.back());//变道的初始目标点

		// 如果案例停止，则退出循环并记录评分
		if (SimOneAPI::GetCaseRunStatus() == ESimOne_Case_Status::ESimOne_Case_Status_Stop) {
			SimOneAPI::SaveEvaluationRecord();
			globalLogger(Logger::Color::BrightMagenta) << "Case stop running";
			break;
		}

		if (!SimOneAPI::GetGps(MainVehicleId, pGps.get())) { // 获取主车 GPS 信息
			globalLogger(Logger::Color::BrightMagenta) << "Fetch GPS failed";
		}
		else
		{
			static bool StatusFlagGPS = true;
			if (StatusFlagGPS)
			{
				StatusFlagGPS = false;
				globalLogger(Logger::Color::BrightMagenta) << "GPS initialized";
			}
		}

		if (!SimOneAPI::GetGroundTruth(MainVehicleId, pObstacle.get())) { // 获取障碍物 GPS 信息
			globalLogger(Logger::Color::BrightMagenta) << "Fetch obstacle failed";
		}
		else
		{
			static bool StatusFlagObstacle = true;
			if (StatusFlagObstacle)
			{
				StatusFlagObstacle = false;
				globalLogger(Logger::Color::BrightMagenta) << "Obstacle initialized";
			}
		}

		if (SimOneAPI::GetCaseRunStatus() == ESimOne_Case_Status::ESimOne_Case_Status_Running) {
			if (!FlagType::isSimOneInitialized) {
				globalLogger(Logger::Color::BrightMagenta) << "SimOne Initialized!";
				FlagType::isSimOneInitialized = 1u;
			}
		}
		else { // 如果 SimOneAPI 有任何一个地方初始化失败，则直接开启下一次循环
			globalLogger(Logger::Color::BrightMagenta) << "SimOne Initializing...";
			SimOneAPI::NextFrame(frame);
			continue;
		}

		// 初始化控制和车灯
		pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed; // 速度 m/s
		pControl->throttle = baseSpeed; // 速度
		pControl->steering = 0.0f; // 打角
		pControl->handbrake = false; // 手刹
		pControl->isManualGear = false; // 是否手动挡
		pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive; // 挡位：前进挡
		pLight->signalLights = ESimOne_Signal_Light::ESimOne_Signal_Light_None; // 车灯

		// 计算主车的详细参数
		mainVehiclePos = SSD::SimPoint3D(pGps->posX, pGps->posY, pGps->posZ); // 主车坐标
		if (frameCount == 1) // 第一帧的时候让上次和本次的主车道路 ID 相同
		{
			lastMainVehicleLaneId = mainVehicleLaneId = m_SampleGetNearMostLane(mainVehiclePos);
		}
		else
		{
			lastMainVehicleLaneId = mainVehicleLaneId; // 上一帧主车所在的道路 ID
			mainVehicleLaneId = m_SampleGetNearMostLane(mainVehiclePos); // 主车所在的道路 ID
		}
		SSD::SimString target_lane_id = mainVehicleLaneId;//初始化变道的道路id为主车道路id

		std::string str1(mainVehicleLaneId.GetString());
		size_t pos1 = str1.find('_');
		FlagType::IsTwosideRoad= SimOneAPI::IsTwoSideRoad(std::stoi(str1.substr(0, pos1)));

		mainVehicleSpeed = UtilMath::calculateSpeed(pGps->velX, pGps->velY, pGps->velZ); // 主车速度
		mainVehicleSpeedXY = UtilMath::calculateSpeed(pGps->velX, pGps->velY); // 主车速度（仅 x、y 方向合成）

		// 获取主车的左右邻接车道情况
		SimOneAPI::GetLaneLink(mainVehicleLaneId, laneLink); // 获取主车所在的车道的其他邻接车道
		if (strlen(laneLink.leftNeighborLaneName.GetString()) != 0) // 存在左邻接车道
		{
			FlagType::leftLaneExist = 1u;
		}
		if (strlen(laneLink.rightNeighborLaneName.GetString()) != 0) // 存在右邻接车道
		{
			FlagType::rightLaneExist = 1u;
		}

	
		// 在创建停车线前，清空上一次停车线信息，避免重复的停车线，待优化：每 n 帧更新一次停止线
		stopLineList.clear();
		if (!manualStopLineReservoir.empty())
		{
			for (size_t i = 0, ie = manualStopLineReservoir.size(); i < ie; ++i)
			{
				if(caseIdx == manualStopLineReservoir[i].caseId)
				{
					stopLineList.push_back(StopLine(manualStopLineReservoir[i]));
				}
			}
		}

		if (caseIdx == 29) pLight->signalLights = ESimOne_Signal_Light_LeftBlinker; // 场景 29 一直打左转向灯

		if (caseIdx == 31)
		{

		}

/************************************************** 交通信号灯处理逻辑 **************************************************/
#ifndef TRAFFIC_LIGHT_HANDLING_LOGIC // 交通信号灯处理逻辑
		//LOG << "traffic light size = " << trafficLightList.size();
		for (size_t i = 0, ie = trafficLightList.size(); i < ie; ++i)
		{
			//LOG << "traffic light [" << i << "].pt = (" << trafficLightList[i].pt.x << ", " << trafficLightList[i].pt.y << ")";
			std::unique_ptr<SimOne_Data_TrafficLight> pTrafficLight = std::make_unique< SimOne_Data_TrafficLight>();
			SimOneAPI::GetTrafficLight(MainVehicleId, trafficLightList[i].id, pTrafficLight.get());
			if (pTrafficLight->isMainVehicleNextTrafficLight)
			{
				//LOG << "isMainVehicleNextTrafficLight index = " << i;

				potentialLight = trafficLightList[i];
				if (pTrafficLight->status == ESimOne_TrafficLight_Status::ESimOne_TrafficLight_Status_Red || pTrafficLight->status == ESimOne_TrafficLight_Status::ESimOne_TrafficLight_Status_Yellow)// 红灯、黄灯创建停止线
				{
					stopLineList.push_back(StopLine(potentialLight));
				}
				break;
			}
		}
		flag_haveCrossWalk = false;//不存在斑马线
		SimOneAPI::GetSpecifiedLaneCrosswalkList(mainVehicleLaneId, potentialcrosswalkList);
		if (potentialcrosswalkList.size() != 0)
		{
			flag_haveCrossWalk = true;
			globalLogger(Logger::Color::BrightGreen) << "存在斑马线";
		}
#endif
/************************************************** 交通信号灯处理逻辑 **************************************************/
		
		
/************************************************** 障碍物处理逻辑 **************************************************/
#ifndef OBSTACLE_HANDLING_LOGIC // 障碍物处理逻辑
		globalLogger(Logger::Color::BrightBlue) << "obs size = " << pObstacle->obstacleSize;
		for (size_t i = 0, ie = pObstacle->obstacleSize; i < ie; ++i)
		{
			globalLogger(Logger::Color::BrightBlue) << "obs[" << i << "] vx = " << pObstacle->obstacle[i].velX << ", vy = " << pObstacle->obstacle[i].velY;
			globalLogger(Logger::Color::BrightBlue) << "obs[" << i << "] velocity = " << UtilMath::calculateSpeed(pObstacle->obstacle[i].velX, pObstacle->obstacle[i].velY);
		}

		/* 筛选出垂向运动和沿车道运动的障碍物 */
		std::vector<size_t> staticObstacleIndex;
		std::vector<size_t> obstacleAmongCrosswalkIndex;
		std::vector<int> verticalObstacleIndex, horizontalObstacleID, horizontalObstacleIndex;
		std::vector<std::vector<size_t>> obstacleIDGroup; // 将距离过近的静止障碍物分组合成以后的障碍物 ID 列表
		std::vector<StaticObstacle> staticObstacleGroup; // 将距离过近的静止障碍物分组合成以后的障碍物列表

		for (size_t i = 0, ie = pObstacle->obstacleSize; i < ie; ++i)
		{
			SSD::SimPoint3D obstaclePos(pObstacle->obstacle[i].posX, pObstacle->obstacle[i].posY, pObstacle->obstacle[i].posZ);
			SSD::SimPoint2D _tl, _tr, _bl, _br;
			calculateObstacleCorners(pObstacle->obstacle[i], _tl, _tr, _bl, _br);
			
			// 判断行人是否在斑马线矩形上
			if (flag_haveCrossWalk && SomeInInRectangle(_tl, _tr, _bl, _br, potentialcrosswalkList[0].boundaryKnots[0], potentialcrosswalkList[0].boundaryKnots[2]))
			{
				obstacleAmongCrosswalkIndex.push_back(i);
			}

			if (UtilMath::calculateSpeed(pObstacle->obstacle[i].velX, pObstacle->obstacle[i].velY) < 1e-2) // 若障碍物静止
			{
				staticObstacleIndex.push_back(i);
				//LOG << "障碍物静止";
				continue;
			}
			//SSD::SimPoint3D obstaclePos(pObstacle->obstacle[i].posX, pObstacle->obstacle[i].posY, pObstacle->obstacle[i].posZ);
			SSD::SimString obstacleLaneId = m_SampleGetNearMostLane(obstaclePos);
			HDMapStandalone::MLaneInfo obstacleLaneInfo;
			SimOneAPI::GetLaneSample(obstacleLaneId, obstacleLaneInfo);
			SSD::SimPoint3DVector& leftLine = obstacleLaneInfo.leftBoundary;
			SSD::SimPoint3DVector& rightLine = obstacleLaneInfo.rightBoundary;

			double minDistTempL = std::numeric_limits<double>::max(), minDistTempR = std::numeric_limits<double>::max();
			int pointIndexTempL = leftLine.size(), pointIndexTempR = rightLine.size();
			for (size_t j = 0, je = leftLine.size(); j < je; ++j)
			{
				double distTemp = UtilMath::planarDistance(leftLine[j], obstaclePos);
				if (distTemp < minDistTempL)
				{
					minDistTempL = distTemp;
					pointIndexTempL = (int)j;
				}
			}
			for (size_t j = 0, je = rightLine.size(); j < je; ++j)
			{
				double distTemp = UtilMath::planarDistance(rightLine[j], obstaclePos);
				if (distTemp < minDistTempR)
				{
					minDistTempR = distTemp;
					pointIndexTempR = (int)j;
				}
			}

			LOG << "pointIndexTempL = " << pointIndexTempL;
			LOG << "pointIndexTempR = " << pointIndexTempR;

			float obstacleTheta = calculateResultantAzimuth(pObstacle->obstacle[i].velX, pObstacle->obstacle[i].velY);
			//float laneTheta = calculateResultantAzimuth(leftLine[pointIndexTempL].x, leftLine[pointIndexTempL].y, rightLine[pointIndexTempR].x, rightLine[pointIndexTempR].y);
			float laneTheta = getLaneAzimuth(mainVehicleLaneId);

			LOG << "obstacleTheta = " << obstacleTheta;
			LOG << "laneTheta = " << laneTheta;

			float deltaTheta = std::fabs(obstacleTheta - laneTheta);
			float angle = std::min(deltaTheta, 360.0f - deltaTheta);

			LOG << "angle = " << angle;

			float thresTemp = 10.0f;
			if (std::fabs(angle - 90.0f) <= thresTemp || std::fabs(angle - 270.0f) <= thresTemp) // 近似垂直
			{
				verticalObstacleIndex.push_back((int)i);
				//LOG << "-------------------------------------- horizental --------------------------------------";
			}
			else if (std::fabs(angle - 0.0f) <= thresTemp || std::fabs(angle - 180.0f) <= thresTemp) // 近似平行
			{
				horizontalObstacleIndex.push_back((int)i);
				//LOG << "-------------------------------------- vertical --------------------------------------";
			}
			else // 既不平行也不垂直
			{
				//horizontalObstacleIndex.push_back((int)i);//为了弯道内跟车可以执行
				verticalObstacleIndex.push_back((int)i);
				//LOG << "-------------------------------------- else horizental --------------------------------------";
			}
		}

		//--------------直接跳转到跟车----------------------
		if (std::find(caseFollowingID.begin(), caseFollowingID.end(), caseIdx) != caseFollowingID.end())
		{
			goto FOLLOW_CAR;
		}
		//----------------------------------------

		/* 筛选出静止的障碍物 */
		// 如果两个【类型相同】的【静止】的障碍物之间距离很近的话，就将他们合成一个新的障碍，使用图的深度优先搜索
		groupObstacleByDist(staticObstacleIndex, groupingDistThres, obstacleIDGroup);
		for (size_t i = 0, ie = obstacleIDGroup.size(); i < ie; ++i)
		{
			if (obstacleIDGroup[i].size() == 1) // 如果当前组内只有一个障碍物
			{
				staticObstacleGroup.push_back(StaticObstacle(pObstacle->obstacle[obstacleIDGroup[i].at(0)]));
			}
			else // 如果不止一个障碍物
			{
				std::vector<SSD::SimPoint2D> obsTL, obsBR; // 分别计算每个障碍物的 tl 和 br
				SSD::SimPoint2D minTL, maxBR;

				for (size_t j = 0, je = obstacleIDGroup[i].size(); j < je; ++j)
				{
					obsTL.push_back(calculateObstacleTL(pObstacle->obstacle[obstacleIDGroup[i].at(j)]));
					obsBR.push_back(calculateObstacleBR(pObstacle->obstacle[obstacleIDGroup[i].at(j)]));
				}

				findBoundingBox(obsTL, obsBR, minTL, maxBR); // 将不同障碍物的 bounding box 合成一个
				SimOne_Data_Obstacle_Entry& ObsTemp = pObstacle->obstacle[obstacleIDGroup[i].at(0)];
				staticObstacleGroup.push_back(StaticObstacle(ObsTemp.id, ObsTemp.posZ, ObsTemp.height, minTL, maxBR));
			}
		}

		/* 为静止的障碍物创建停止线对象 */
		for (size_t i = 0, ie = staticObstacleGroup.size(); i < ie; ++i)
		{
			// 静止的障碍物有可能是 StopLine_Type_Fixed_Obstacle 类型，也可能是 SingleStopLine_Type_Fixed_Obstacle 类型的障碍物
			if (staticObstacleGroup[i].isLaneChangeObstacle) // 如果是 SingleStopLine_Type_Fixed_Obstacle 类型的障碍物
			{
				LOG << "静物半停止线";
				stopLineList.push_back(StopLine(staticObstacleGroup[i], StopLine_Type::SingleStopLine_Type_Fixed_Obstacle));
			}
			else // 如果是 StopLine_Type_Fixed_Obstacle 类型的障碍物
			{
				stopLineList.push_back(StopLine(staticObstacleGroup[i], StopLine_Type::StopLine_Type_Fixed_Obstacle));
			}
		}
		/* 为垂向运动的障碍物创建停止线对象 */
		LOG << " verticalObstacleIndex.size:" << verticalObstacleIndex.size();
		for (size_t i = 0, ie = verticalObstacleIndex.size(); i < ie; ++i)
		{
			// 垂向运动的障碍物一定是 StopLine_Type_Mobile_Obstacle 类型的障碍物
			LOG << "创建垂向";
			//if (!hasPedestrianCompletelyLeftRoad(pObstacle->obstacle[verticalObstacleIndex.at(i)], cumulative_singel, cumulative_double)) // 判断是否通过马路
			if (1)
			{
				stopLineList.push_back(StopLine(pObstacle->obstacle[verticalObstacleIndex.at(i)], StopLine_Type::StopLine_Type_Mobile_Obstacle));
			}
		}
		/* 为斑马线上的障碍物创建停止线对象 */
		for (size_t i = 0, ie = obstacleAmongCrosswalkIndex.size(); i < ie; ++i)
		{
			stopLineList.push_back(StopLine(pObstacle->obstacle[obstacleAmongCrosswalkIndex.at(i)]));
		}
		/* 为沿车道运动的障碍物创建停止线对象 */
		//for (size_t i = 0, ie = horizontalObstacleIndex.size(); i < ie; ++i)
		//{
		//	// 沿车道运动的障碍物一定是 SingleStopLine_Type_Mobile_Obstacle 类型的障碍物
		//	stopLineList.push_back(StopLine(pObstacle->obstacle[horizontalObstacleIndex.at(i)], StopLine_Type::SingleStopLine_Type_Mobile_Obstacle));
		//}
#endif
/************************************************** 障碍物处理逻辑 **************************************************/
		
		pControl->throttle = caseTargetSpeed;  // 立即停止加速（刹车）

/************************************************** 停止线处理逻辑 **************************************************/
#ifndef STOPLINE_HANDLING_LOGIC // 停止线和半停止线的处理逻辑
		/* 计算与主车第一近和第二近的停止线，0 第一近，1 第二近 */
		double minStopLine2CarDist[2] = { std::numeric_limits<double>::max(), std::numeric_limits<double>::max() };
		int potentialStopLineIndex[2] = { pObstacle->obstacleSize, pObstacle->obstacleSize };
		if (stopLineList.empty()) // 如果没有停止线
		{
			LOG << "停止线为空";
			goto STOPLINE_HANDLING_LOGIC_END_TAG;
		}
		else if (stopLineList.size() == 1) // 如果只有一条停止线
		{
			//LOG << "停止线有一条";
			// 如果停止线和主车不在同一车道，则不关心该停止线，直接 continue
			if (!stopLineList[0].isVechicleInSameLane(mainVehiclePos, mainVehicleLaneId))
			{
				//LOG << "停止线与主车不是同车道";
				goto STOPLINE_HANDLING_LOGIC_END_TAG;
			}
			// 如果停止线在主车后方，则不关心该停止线，直接 continue
			// ！注意这样写有安全隐患，有可能后车速度过快会直接撞上来，后续想办法跟进
			if (stopLineList[0].isStopLineBehind)
			{
				//LOG << "停止线在主车后面";
				goto STOPLINE_HANDLING_LOGIC_END_TAG; 
			}
			potentialStopLineIndex[0] = 0;
			potentialStopLineIndex[1] = -1;
			
		}
		else if (stopLineList.size() == 2) // 如果只有两条停止线
		{
			//LOG << "停止线有两条";
			// 先判断第一条停止线是否符合条件
			bool isValidStopLine0 = stopLineList[0].isVechicleInSameLane(mainVehiclePos, mainVehicleLaneId) &&
				!stopLineList[0].isStopLineBehind;
			// 判断第二条停止线是否符合条件
			bool isValidStopLine1 = stopLineList[1].isVechicleInSameLane(mainVehiclePos, mainVehicleLaneId) &&
				!stopLineList[1].isStopLineBehind;

			if (!isValidStopLine0 && !isValidStopLine1)
			{
				// 如果两条停止线都不符合条件，则直接跳过
				goto STOPLINE_HANDLING_LOGIC_END_TAG;
			}
			else if (isValidStopLine0 && !isValidStopLine1)
			{
				// 只有第一条停止线符合条件
				potentialStopLineIndex[0] = 0;
				potentialStopLineIndex[1] = -1;
			}
			else if (!isValidStopLine0 && isValidStopLine1)
			{
				// 只有第二条停止线符合条件
				potentialStopLineIndex[0] = 1;
				potentialStopLineIndex[1] = -1;
			}
			else
			{
				// 两条停止线都符合条件，计算距离，找到最近的
				minStopLine2CarDist[0] = stopLineList[0].toVehicleDist(mainVehiclePos);
				minStopLine2CarDist[1] = stopLineList[1].toVehicleDist(mainVehiclePos);

				if (minStopLine2CarDist[0] <= minStopLine2CarDist[1])
				{
					potentialStopLineIndex[0] = 0;
					potentialStopLineIndex[1] = 1;
				}
				else
				{
					potentialStopLineIndex[0] = 1;
					potentialStopLineIndex[1] = 0;
				}
			}
		}
		else // 如果有三条以上的停止线
		{
			//LOG << "停止线有三条";
			for (size_t i = 0, ie = stopLineList.size(); i < ie; ++i)
			{
				// 如果停止线和主车不在同一车道，则不关心该停止线，直接 continue
				if (!stopLineList[i].isVechicleInSameLane(mainVehiclePos, mainVehicleLaneId)) continue;
				// 如果停止线在主车后方，则不关心该停止线，直接 continue
				// ！注意这样写有安全隐患，有可能后车速度过快会直接撞上来，后续想办法跟进
				if (stopLineList[i].isStopLineBehind) continue;
				double stopLine2CarDist = stopLineList[i].toVehicleDist(mainVehiclePos);

				if (stopLine2CarDist < minStopLine2CarDist[0])
				{
					// 更新第一近的停止线，第二近的停止线后移
					minStopLine2CarDist[1] = minStopLine2CarDist[0];
					potentialStopLineIndex[1] = potentialStopLineIndex[0];

					minStopLine2CarDist[0] = stopLine2CarDist;
					potentialStopLineIndex[0] = (int)i;
				}
				else if (stopLine2CarDist < minStopLine2CarDist[1])
				{
					// 更新第二近的停止线
					minStopLine2CarDist[1] = stopLine2CarDist;
					potentialStopLineIndex[1] = (int)i;
				}
			}
		}

		#define STOPLINE(n) stopLineList.at(potentialStopLineIndex[n])
		#define MIN_DIST(n) STOPLINE(0).toVehicleDist(mainVehiclePos)
		
		static bool hasLaneChanged = false; // 记录是否已经变道成功
		static int laneChangeCompleteFrame = -1; // 记录变道完成的帧索引
		static int temp = -1;

		if (STOPLINE(0).type == StopLine_Type::SingleStopLine_Type_Fixed_Obstacle ||
			STOPLINE(0).type == StopLine_Type::SingleStopLine_Type_Mobile_Obstacle) // 半停止线：需要变道
		{
			if (STOPLINE(0).isMovingAlongLane) // 如果半停止线是沿着车道方向移动的
			{
				
			}
			else // 如果半停止线是静止的：简单的变道处理
			{
				// ——————————新尝试获取全局坐标的变道尝试——————————————
				if (MIN_DIST(0) < 40)
				{
					SSD::SimPoint3D lane_change_obstaclePos(STOPLINE(0).midPos.x, STOPLINE(0).midPos.y, 0);
					globalLogger(Logger::Color::BrightGreen) << "变道停止线的坐标：" << lane_change_obstaclePos.x << "," << lane_change_obstaclePos.y;
					double lane_change_s, lane_change_t;
					if (!SimOneAPI::GetLaneST(mainVehicleLaneId, lane_change_obstaclePos, lane_change_s, lane_change_t))
					{
						globalLogger(Logger::Color::BrightGreen) << "需要变道障碍物st坐标获取失败";
					}
					if(caseIdx != 13)
					{
						lane_change_s -= 1;
					}

					if (caseIdx == 13)
					{
						lane_change_t += (FlagType::leftLaneExist) ? 1.8 : ((FlagType::rightLaneExist) ? -2.0 : 0.0);
					}
					else lane_change_t += (FlagType::leftLaneExist) ? 2.0 : ((FlagType::rightLaneExist) ? -2.0 : 0.0);

					SSD::SimPoint3D lane_change_dir;
					if (!SimOneAPI::GetInertialFromLaneST(mainVehicleLaneId, lane_change_s, lane_change_t, lane_change_inertial, lane_change_dir))
					{
						globalLogger(Logger::Color::BrightGreen) << "需要变道目标点全局坐标获取失败";
					}
					IsPosOnLeftOrRight(lane_change_inertial);//根据目标点在主车左右来给转向灯
					lanechangePath.clear(); // 更新起点和终点
					lanechangePath.push_back(mainVehiclePos);
					lanechangePath.push_back(lane_change_inertial);

					targetPath.clear(); // 清空旧路径
					indexOfValidPoints.clear();
					if (!SimOneAPI::GenerateRoute(lanechangePath, indexOfValidPoints, targetPath)) { // 重新生成新路径
						SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Error, "Generate mainVehicle route failed");
						//return -1;
					}
				}
			STOPLINE_HANDLING_LOGIC_END_TAG:
				
				LOG << "获取三条前";
				target_lane_id = m_SampleGetNearMostLane(lane_change_inertial);
				bool flag_SameRoad = false;
				//LOG << "获取三条后";
				/*if(isSameRoadId(target_lane_id,mainVehicleLaneId))
				{
					flag_SameRoad = true;
				}*/
				double mainVehicle_s = 1e6;
				double mainVehicle_t=1e6;
				std::cout <<"target_lane_id.GetString();:" << target_lane_id.GetString()<<std::endl;
				std::cout << "carPos x y:" << mainVehiclePos.x << "," << mainVehiclePos.y << std::endl;

				if (!SimOneAPI::GetLaneST(target_lane_id, mainVehiclePos, mainVehicle_s, mainVehicle_t))
				{
					globalLogger(Logger::Color::BrightGreen) << "主车st坐标获取失败";
				}
				if (abs(mainVehicle_t) < 1.0) //距离目标车道横向偏差小于0.5，视为已经变道
				{
					globalLogger(Logger::Color::BrightGreen) << "生成到终点的路径";
					SSD::SimPoint3D destinationPos(initialPath.back());
					lanechangePath.clear(); // 更新起点和终点
					lanechangePath.push_back(mainVehiclePos);
					lanechangePath.push_back(destinationPos);

					targetPath.clear(); // 清空旧路径
					indexOfValidPoints.clear();
					pLight->signalLights = ESimOne_Signal_Light_None;//变道完成后，关闭车灯
					if (!SimOneAPI::GenerateRoute(lanechangePath, indexOfValidPoints, targetPath))
					{ // 重新生成新路径
						SimOneAPI::SetLogOut(ESimOne_LogLevel_Type::ESimOne_LogLevel_Type_Error, "Generate mainVehicle route failed");
						//return -1;
					}
					FlagType::useDefaultPath = 1; // 使用默认路径
				}
				// ——————————新尝试获取全局坐标的变道尝试——————————————

			}
		}
		else // 全停止线：先停后走
		{
			globalLogger(Logger::Color::BrightGreen) << "全停止线";
			globalLogger(Logger::Color::BrightGreen) << " MIN_DIST(0) dist = " << MIN_DIST(0) - 4.38046;
			
			// 假如说要在停止线 1 米前停下来
			float d0 = 0.05272 * mainVehicleSpeedXY * mainVehicleSpeedXY + 0.06714 * mainVehicleSpeedXY - 0.02188; // d0 是刹车距离
			if (MIN_DIST(0) - 4.38046 <= d0 + stopLineDistThres) // d0 + x 表示要在停止线 x 米前停下来， - 4.38046 代表减去质心到车头的距离
			{
				pControl->throttle = 0;  // 立即停止加速（刹车）
			}
		}
#endif
/************************************************** 停止线处理逻辑 **************************************************/

/************************************************** 沿车道行驶车辆变道处理 **************************************************/
#ifndef OVERTAKE_HANDLING_LOGIC
		if (!horizontalObstacleIndex.empty()) // 如果有沿车道运动的障碍物
		{
			for (size_t i = 0, ie = horizontalObstacleIndex.size(); i < ie; ++i)
			{
				size_t idx = horizontalObstacleIndex.at(i);
				SimOne_Data_Obstacle_Entry& obstacle = pObstacle->obstacle[idx];
				SSD::SimPoint3D obstaclePosition(obstacle.posX, obstacle.posY, obstacle.posZ);
				SSD::SimString obstacleLaneId = m_SampleGetNearMostLane(obstaclePosition);
				
				globalLogger(Logger::Color::BrightGreen) <<"pLight->signalLights:" << pLight->signalLights;
				if (isSameRoadId(mainVehicleLaneId, obstacleLaneId)) // 如果主车和障碍物在同一条道路上
				{
					globalLogger(Logger::Color::BrightGreen) << "在同一条道路上";

					if (isPathRequireLaneChange(targetPath)) // 判断主车目标路径是否涉及变道操作
					{
						globalLogger(Logger::Color::BrightGreen) << "主车目标路径涉及变道操作";
						IsLaneOnLeftOrRight(mainVehiclePos, mainVehicleLaneId, obstacleLaneId);//根据目标车道在左还是在右进行打灯?????
						float azimuth = calculateResultantAzimuth(obstacle.velX, obstacle.velY);
						if (isTrajInterfere(targetPath, obstaclePosition, azimuth)) // 主车轨迹与对手车辆轨迹是否发生干涉
						{
							globalLogger(Logger::Color::BrightGreen) << "主车轨迹与对手车辆轨迹发生干涉";

							double sVehPos = 0.0, tVehPos = 0.0, zTemp = 0.0;
							double sObsPos = 0.0, tObsPos = 0.0;
							SimOneAPI::GetRoadST(mainVehicleLaneId, mainVehiclePos, sVehPos, tVehPos, zTemp);
							SimOneAPI::GetRoadST(mainVehicleLaneId, obstaclePosition, sObsPos, tObsPos, zTemp);
							float v1 = UtilMath::calculateSpeed(obstacle.velX, obstacle.velY);
							float L0 = 4.7987f;
							float L1 = obstacle.length;
							float Lx = 1.f;
							float baseLa = -((sVehPos - sObsPos) - 0.5 * (L0 + L1) - Lx);
							float limitBaseLa = MAX(baseLa, 0);
							float La = limitBaseLa + chasingLimitDist;
							float v0 = calculateTargetOvertakingVelocity(v1, sVehPos, sObsPos, L0, L1, La, Lx);
							globalLogger(Logger::Color::BrightGreen) << "s0 = " << sVehPos;
							globalLogger(Logger::Color::BrightGreen) << "s1 = " << sObsPos;
							globalLogger(Logger::Color::BrightGreen) << "La = " << La;
							globalLogger(Logger::Color::BrightGreen) << "v0 = " << v0;
							pControl->throttle = v0;
						}
					}
				}
			}
		}
#endif
/************************************************** 沿车道行驶车辆变道处理 **************************************************/

/************************************************** 稳定跟车处理逻辑 **************************************************/
#ifndef FOLLOWING_HANDLING_LOGIC // 自适应稳定跟车处理逻辑：使用串级 PID，先调好外环速度环，再调内环跟车速度环
				//---由前面goto跳转---------------------------------
	FOLLOW_CAR:
		if (std::find(caseFollowingID.begin(), caseFollowingID.end(), caseIdx) != caseFollowingID.end())
		{
			globalLogger(Logger::Color::BrightGreen) << "执行跟车操作";
			double minDistance = std::numeric_limits<double>::max(); // 主车与障碍物的最小距离
			SimOne_Data_Obstacle_Entry* potentialObstacle = nullptr;
			if (!horizontalObstacleIndex.empty()) // 如果有沿车道运动的障碍物
			{
				for (size_t i = 0, ie = horizontalObstacleIndex.size(); i < ie; ++i)
				{
					size_t idx = horizontalObstacleIndex.at(i);
					SimOne_Data_Obstacle_Entry& obstacle = pObstacle->obstacle[idx];
					SSD::SimPoint3D obstaclePosition(obstacle.posX, obstacle.posY, obstacle.posZ);
					SSD::SimString obstacleLaneId = m_SampleGetNearMostLane(obstaclePosition);
					double mainVehicle_s, mainVehicle_t,  potentialObstacle_s, potentialObstacle_t;
					SimOneAPI::GetLaneST(mainVehicleLaneId, mainVehiclePos, mainVehicle_s, mainVehicle_t);//获取主车相当于目标车道的st坐标
					SimOneAPI::GetLaneST(mainVehicleLaneId, obstaclePosition, potentialObstacle_s, potentialObstacle_t);//获取主车相当于目标车道的st坐标
					double obstacleDistance = potentialObstacle_s - mainVehicle_s;
					pLight->signalLights = ESimOne_Signal_Light::ESimOne_Signal_Light_LeftBlinker;//左转灯
					if (isSameRoadId(mainVehicleLaneId, obstacleLaneId) && (obstacleDistance < minDistance)) // 如果主车和障碍物在同一条道路上
					//if ((mainVehicleLaneId==obstacleLaneId) && (obstacleDistance < minDistance))
					{
						minDistance = obstacleDistance;
						potentialObstacle = &obstacle;
					}
				}
			}
			if (caseIdx == case_stop_followID)//默认速度0,在停走场景中可以刹住
			{
				pControl->throttle = 0;
			}
			else
			{
				pControl->throttle = 5.5;//默认速度5.5,为了让跟车上坡时，车道不同仍能上坡
			}
			if (potentialObstacle)
			{
				double potentialObstacleSpeed = UtilMath::calculateSpeed(potentialObstacle->velX, potentialObstacle->velY, potentialObstacle->velZ);
				globalLogger(Logger::Color::BrightGreen) << "potentialObstacleSpeed:" << potentialObstacleSpeed;
				if (minDistance < 1e6)//避免距离是无穷大时仍然计算速度环
				{
					globalLogger(Logger::Color::BrightGreen) << "minDistance:" << minDistance;
					adaptiveFollowing.update(potentialObstacleSpeed, minDistance);//跟车速度环
					pControl->throttle = (adaptiveFollowing.getSpeed()>3.5)? adaptiveFollowing.getSpeed():3.5;//给到速度,并且使得速度大于最小速度
				}
			}
	
		}
#endif
/************************************************** 稳定跟车处理逻辑 **************************************************/

		/* PID 控制器结合纯追踪算法处理目标路径，计算前轮打角 */
		if (FlagType::useDefaultPath) // 如果使用默认路径
		{
			double steering = UtilDriver::calculateSteering(targetPath, pGps.get()); // 路径纯追踪控制打角
			pControl->steering = steerPID.calculate(steering);
			LOG << "steering = " << steering;
			LOG << "pControl->steering = " << pControl->steering;
		}

		// 输出调试信息
		LOG << "stopLine size = " << stopLineList.size();

		for (size_t i = 0, ie = stopLineList.size(); i < ie; ++i)
		{
			LOG << "-----------------------------------------------";
			LOG << "Stopline" << i << " : ";
			LOG << "type = " << stopLineList[i].type;
			//LOG << "index = " << stopLineList[i].index;
			//LOG << "isMovingAlongLane = " << stopLineList[i].isMovingAlongLane;
			LOG << "isStopLineBehind = " << stopLineList[i].isStopLineBehind;
			LOG << "srcPos = ( " << stopLineList[i].srcPos.x << ", " << stopLineList[i].srcPos.y << " )";
			LOG << "dstPos = ( " << stopLineList[i].dstPos.x << ", " << stopLineList[i].dstPos.y << " )";
		}

		globalLogger(Logger::Color::BrightGreen) << "pControl->throttle = " << pControl->throttle;
		SimOneAPI::SetSignalLights(MainVehicleId, pLight.get());//设置车辆的转向灯
		SimOneAPI::SetDrive(MainVehicleId, pControl.get());//设置车辆的油门打角

		SimOneAPI::NextFrame(frame);
	}

	return 0;
}