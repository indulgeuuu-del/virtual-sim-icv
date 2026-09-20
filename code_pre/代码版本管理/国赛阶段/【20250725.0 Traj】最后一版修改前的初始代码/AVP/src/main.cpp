#include "main.h"
#include "UtilMath.h"

float forwardingSpeed = 5.0f; // 单位：m/s
float forwardingSpeedMax = 10.0f; // 单位：m/s

float verticalLeavingSpeedMax = 25.0f; // 单位：m/s
float verticalLeavingSpeedMin = 4.0f; // 单位：m/s

float lateralLeavingSpeedMax = 20.0f; // 单位：m/s
float lateralLeavingSpeedMin = 12.0f; // 单位：m/s

/* 说明：如果想要创建一个从点 0 指向 1 的向量，则调用 DIR2PT(0, 1) */
#define DIR(perior, latter, symbol) (targetParkingSlot.boundaryKnots[latter].symbol - targetParkingSlot.boundaryKnots[perior].symbol)
#define DIR2PT(perior, latter) (SSD::SimPoint3D(DIR(perior, latter, x), DIR(perior, latter, y), DIR(perior, latter, z)))

int main(int argc, char* argv[])
{
	initSimOne(); // SimOne 初始化
	loadJson();
	initWayPoint(MAIN_VEHICLE_ID, wayPointsPtr.get(), startPoint, endPoint); // 获得道路的路径点、起点和终点
	timer.tic();

	while (true)
	{
		globalLogger(Logger::Color::BrightBlue) << "\n\nFrame = " << (frameCount = SimOneAPI::Wait());

		recordEvaluation(); // 更新 NEVC 系统评分

		updateObstacleData(); // 更新障碍物参数
		updateVehicleData(); // 更新主车 GPS 参数

		/*endPoint.x = -15.289393;
		endPoint.y = -11.933903;*/

		waitInitial(); // 等待仿真平台初始化完成

		SimOneAPI::GetParkingSpaceList(parkingSpaces); // 获取所有车位与障碍物信息，用于判断可泊车区域
		findTargetParkingSpace(parkingSpaces, obstaclesPtr.get(), targetParkingSpace); // 查找目标停车位
		rearrangeKnots(targetParkingSpace); // 重新排列停车位的边界顶点，使其从左上角顶点开始

		// 计算 ParkingSlot
		if (frameCount < 15)
		{
			targetParkingSlot.judgeType(targetParkingSpace); // 判别当前场景是水平泊车还是垂直泊车
			targetParkingSlot.judgeSide(targetParkingSpace); // 判断目标车位在地图上侧还是下侧
			targetParkingSlot.judgeDirection(gpsPtr->velX, gpsPtr->velY); // 判断主车是自西向东还是自东向西
			targetParkingSlot.buildCoordinate(targetParkingSpace); // 建立局部车位坐标系
			targetParkingSlot.processBoundaryKnots(targetParkingSpace); // 处理车位角点

			/* 路径规划 */
			trajectoryForward = Trajectory(startPoint, endPoint, targetParkingSlot, Trajectory::Type::eForwarding);
			trajectoryReverse = Trajectory(startPoint, endPoint, targetParkingSlot, Trajectory::Type::eReversing);
			trajectoryLeaving = Trajectory(startPoint, endPoint, targetParkingSlot, Trajectory::Type::eLeaving);
		}

		#ifndef CORE_STATE_MACHINE // 核心状态机
		if (status == AVPStatus::eForwarding) // 车辆向前接近目标车位
		{
			// 如果主车接近 forwardPath 的最后一点，则切换到下一个状态
			if (UtilAVP::PlanarDistance(vehiclePoint, trajectoryForward.trajectory.back()) < 1)
			{
				status = AVPStatus::eBrakingPrep;
				pControl->throttle = pControl->steering = 0.0f;
				pControl->handbrake = pControl->isManualGear = false;
				pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
			}
			else // 否则，主车依旧处于接近状态
			{
				LOG << "仍处于接近状态";

				static bool achieveSlowPoint = false; // 是否到达减速点
				size_t slowPoint = MAX(0, trajectoryForward.trajectory.size() - 3); // 减速点

				/* 判定是否到达减速点 */
				if (UtilAVP::PlanarDistance(vehiclePoint, trajectoryForward.trajectory[slowPoint]) < 6.0f) achieveSlowPoint = true;

				/* 如果主车目前行驶在(startPt) -> (slowPt)之间，使用高速；否则使用低速 */
				if (!achieveSlowPoint) pControl->throttle = forwardingSpeedMax;
				else pControl->throttle = forwardingSpeed;

				pControl->steering = UtilAVP::CalculateSteering(trajectoryForward.trajectory, gpsPtr.get());
			}

			SimOneAPI::SetDrive(MAIN_VEHICLE_ID, pControl.get());
		}
		else if (status == AVPStatus::eBrakingPrep) // 短暂停车准备
		{
			/* 停车等待 brakingPrepParkingTime 毫秒 */
			timer.markOnce("BrakingPrep", brakingPrepStarted);
			if (timer.isTriggered("BrakingPrep", brakingPrepParkingTime))
			{
				status = AVPStatus::eReversing;
				timer.removeMark("BrakingPrep");
			}

			pControl->throttle = pControl->steering = 0.0f;
			pControl->handbrake = pControl->isManualGear = false;
			pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse;

			SimOneAPI::SetDrive(MAIN_VEHICLE_ID, pControl.get());
		}
		else if (status == AVPStatus::eReversing) // 倒车进入车位
		{
			if (isVerticalParking(targetParkingSpace)) // 如果是水平泊车
			{
				// 车辆当前航向角 oriZ 与车位朝向 heading 是否接近。加减 π 和 π/2 是为了坐标系对齐。
				// 如果角度误差在 0.5° 以内，则认为车辆已经对准。
				if (UtilAVP::CloseEnough(static_cast<double>(UtilAVP::calculateVectorAzimuthCounterClockwise(DIR2PT(1, 0))),
					static_cast<double>(gpsPtr->oriZ), UtilAVP::DegreeToRad(headingErrorThresholdDegree)))
				{
					// 如果当前 Y 方向位置足够接近车位中心。
					// 2.5 是估计车身中心到车头的偏移（亦即车长的一半），用于防止车尾突出。
					// 如果距离小于等于 0.5 米，认为入位成功 → 跳转状态。
					if (vehiclePoint.y - targetParkingSpace.pt.y + 2.5 <= 0.5)
					{
						status = AVPStatus::eWaiting;
						pControl->throttle = pControl->steering = 0.0f;
						pControl->handbrake = pControl->isManualGear = false;
						pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Parking;
					}
					else // 如果当前 Y 方向位置不够接近车位中心，继续倒车
					{
						pControl->throttle = UtilAVP::KmHToMs(2.0);
						pControl->steering = 0.0f;
						pControl->handbrake = pControl->isManualGear = false;
						pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse;
					}
				}
				else // 如果车辆仍未对准，则自动调整转向，继续倒车
				{
					pControl->throttle = UtilAVP::KmHToMs(3.0);
					pControl->steering = UtilAVP::CalculateSteering(trajectoryReverse.trajectory, gpsPtr.get());
					pControl->handbrake = pControl->isManualGear = false;
					pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse;
				}
			}
			else // 如果是垂直泊车
			{
				LOG << "calculateVectorAzimuthCounterClockwise:" << UtilAVP::calculateVectorAzimuthCounterClockwise(DIR2PT(0, 3));
				LOG << "gpsPtr->oriZ:" << gpsPtr->oriZ;

				// 如果车辆已经对准
				if (UtilAVP::CloseEnough(static_cast<double>(UtilAVP::calculateVectorAzimuthCounterClockwise(DIR2PT(0, 3))),
					static_cast<double>(gpsPtr->oriZ), UtilAVP::DegreeToRad(headingErrorThresholdDegree)))
				{
					// 如果当前 Y 方向位置足够接近车位中心，距离小于等于 0.5 米，认为入位成功 → 跳转状态
					if (vehiclePoint.y - targetParkingSpace.pt.x + 2.5 <= 0.5)
					{
						status = AVPStatus::eWaiting;
						pControl->throttle = pControl->steering = 0.0f;
						pControl->handbrake = pControl->isManualGear = false;
						pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Parking;
					}
					else // 如果当前 Y 方向位置不够接近车位中心，继续倒车
					{
						pControl->throttle = UtilAVP::KmHToMs(2.0);
						pControl->steering = 0.0f;
						pControl->handbrake = pControl->isManualGear = false;
						pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse;
					}
				}
				else // 如果车辆仍未对准，则自动调整转向，继续倒车
				{
					pControl->throttle = UtilAVP::KmHToMs(3.0);
					pControl->steering = UtilAVP::CalculateSteering(trajectoryReverse.trajectory, gpsPtr.get());
					pControl->handbrake = pControl->isManualGear = false;
					pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse;
				}
			}

			SimOneAPI::SetDrive(MAIN_VEHICLE_ID, pControl.get());
		}
		else if (status == AVPStatus::eWaiting)
		{
			/* 停车等待 waitingParkingTime 毫秒 */
			timer.markOnce("Waiting", waitingStarted);
			if (timer.isTriggered("Waiting", waitingParkingTime))
			{
				status = AVPStatus::eLeaving;
				timer.removeMark("Waiting");
			}
			LOG << "等待时间：" << timer.sinceMark("Waiting") << " ms";

			pControl->throttle = pControl->steering = 0.0f;
			pControl->handbrake = pControl->isManualGear = false;
			pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Parking;

			SimOneAPI::SetDrive(MAIN_VEHICLE_ID, pControl.get());
		}
		else if (status == AVPStatus::eLeaving)
		{
			if (isVerticalParking(targetParkingSpace)) // 如果是垂直泊车
			{
				if (UtilAVP::CloseEnough(static_cast<double>(UtilAVP::calculateVectorAzimuthCounterClockwise(DIR2PT(1, 2))),
					static_cast<double>(gpsPtr->oriZ), UtilAVP::DegreeToRad(5.0)))
				{
					pControl->throttle = verticalLeavingSpeedMax;
				}
				else
				{
					pControl->throttle = verticalLeavingSpeedMin;
				}
			}
			else // 如果是水平泊车
			{
				if (UtilAVP::CloseEnough(static_cast<double>(UtilAVP::calculateVectorAzimuthCounterClockwise(DIR2PT(0, 3))),
					static_cast<double>(gpsPtr->oriZ), UtilAVP::DegreeToRad(5.0)))
				{
					pControl->throttle = lateralLeavingSpeedMax;
				}
				else
				{
					pControl->throttle = lateralLeavingSpeedMin;
				}
			}

			pControl->steering = UtilAVP::CalculateSteering(trajectoryLeaving.trajectory, gpsPtr.get());
			pControl->handbrake = pControl->isManualGear = false;
			pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;

			SimOneAPI::SetDrive(MAIN_VEHICLE_ID, pControl.get());
		}
		#endif

		#ifndef DEBUG_OUTPUT // 调试输出
		globalLogger(Logger::Color::BrightGreen) << "▲ 垂直泊车还是水平泊车： " << (targetParkingSlot.type == ParkingSlot::Type::Vertical ? "垂直泊车" : "水平泊车");
		globalLogger(Logger::Color::BrightGreen) << "▲ 目标车位在上侧还是下侧： " << (targetParkingSlot.side == ParkingSlot::Side::Upside ? "上侧" : "下侧");
		globalLogger(Logger::Color::BrightGreen) << "▲ 主车是自西向东还是自东向西： " << (targetParkingSlot.direction == ParkingSlot::Direction::West2East ? "自西向东" : "自东向西");

		/* 输出目标车位重排列后的四个顶点坐标 */
		globalLogger(Logger::Color::BrightGreen) << "▲ 重新排列后的停车位角点：";
		for (size_t i = 0, ie = targetParkingSpace.boundaryKnots.size(); i < ie; ++i)
		{
			SSD::SimPoint3DVector& cornerPoints = targetParkingSpace.boundaryKnots;
			globalLogger(Logger::Color::Yellow) << "\t- 第 " << i + 1 << " 个角点 = (" << cornerPoints[i].x << ", " << cornerPoints[i].y << ")";
		}
		#endif

		SimOneAPI::NextFrame(frameCount);
	}

	return 0;
}