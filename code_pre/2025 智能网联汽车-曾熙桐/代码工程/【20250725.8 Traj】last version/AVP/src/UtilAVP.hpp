#pragma once
#include <math.h>
#include <iostream>
#include <algorithm>
#include <corecrt_math_defines.h>
#include "SSD/SimPoint2D.h"
#include "SSD/SimPoint3D.h"
#include "SimOnePNCAPI.h"

class UtilAVP {
public:
	/* 辅助驾驶 UtilDriver */
	/*!
	* @function SetControl
	* @brief Set control of a main vehicle
	* @param
	*   timestamp: Timestamp of the control signal
	* @param
	*   throttle: Throttle to be set
	* @param
	*   brake: Brake to be set
	* @param
	*   steering: Steering to be set
	* @param
	*   gear: Gear to be set, defaulted with ESimOne_Gear_Mode_Drive
	* @param
	*   mainVehicleId: Id of the controlled main vehicle
	*/
	static void SetControl(const long long timestamp, const double throttle, const double brake,
		const double steering, const ESimOne_Gear_Mode gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive,
		const char* mainVehicleId = "0")
	{
		std::unique_ptr<SimOne_Data_Control> pControl = std::make_unique <SimOne_Data_Control>();

		pControl->timestamp = timestamp;
		pControl->throttleMode = ESimOne_Throttle_Mode_Percent;
		pControl->throttle = (float)throttle;
		pControl->brake = (float)brake;
		pControl->steering = (float)steering;
		pControl->handbrake = false;
		pControl->isManualGear = false;
		pControl->gear = gear;
		SimOneAPI::SetDrive(mainVehicleId, pControl.get());
	}

	/*!
	* @function CalculateSteering
	* @brief Compute steering of the vehicle (for control purposes)
	* @param
	*   targetPath: Planned trajectory
	* @param
	*   pGps: Pointer of current gps signal
	* @return
	*   Steering of the vehicle
	*/
	static double CalculateSteering(const SSD::SimPoint3DVector& targetPath, SimOne_Data_Gps* pGps)
	{
		std::vector<double> pts;
		for (size_t i = 0; i < targetPath.size(); ++i)
		{
			pts.push_back(pow(pGps->posX - targetPath[i].x, 2.) + pow(pGps->posY - targetPath[i].y, 2.));
		}

		size_t index = std::min_element(pts.begin(), pts.end()) - pts.begin();

		size_t forwardIndex = 0;
		double minProgDist = 3.;
		double progTime = 0.8;
		double mainVehicleSpeed = (double)sqrtf(pGps->velX * pGps->velX + pGps->velY * pGps->velY + pGps->velZ * pGps->velZ);
		double progDist = mainVehicleSpeed * progTime > minProgDist ? mainVehicleSpeed * progTime : minProgDist;

		for (; index < targetPath.size(); ++index)
		{
			forwardIndex = index;
			double distance = sqrt(pow(targetPath[index].x - pGps->posX, 2.) + pow(targetPath[index].y - pGps->posY, 2.));
			if (distance >= progDist)
			{
				break;
			}
		}

		double psi = (double)pGps->oriZ;
		double Alfa = atan2(targetPath[forwardIndex].y - pGps->posY, targetPath[forwardIndex].x - pGps->posX) - psi;
		double ld = sqrt(pow(targetPath[forwardIndex].y - pGps->posY, 2.) + pow(targetPath[forwardIndex].x - pGps->posX, 2.));
		double steering = -atan2(2. * (1.3 + 1.55) * sin(Alfa), ld) * 36. / (7. * M_PI);
		return steering;
	}

	/* 数学函数 UtilMath */
	// 计算合成矢量的方位角（ENU 坐标系下，以正东为 0 度，顺时针为正方向）
	static float calculateResultantAzimuth(float xVelocity, float yVelocity)
	{
		// 使用 atan2(-yVelocity, xVelocity) 实现顺时针方向计算，atan2(-y, x) 等效于顺时针角度的计算
		float azimuth = std::atan2f(-yVelocity, xVelocity);
		return azimuth < 0 ? (azimuth + 2 * M_PI) : azimuth; // 将角度范围调整为 [0, 2pi)
	}

	static float calculateVectorAzimuth(const SSD::SimPoint3D& dirVec)
	{
		// 计算逆时针角度（数学方向），单位：弧度
		float angleRad = std::atan2(dirVec.y, dirVec.x);

		// 转换为顺时针方向角度，并转换为 [0, 360)
		float angleDeg = -angleRad * 180.0f / static_cast<float>(M_PI);

		// 保证角度在 [0, 360)
		if (angleDeg < 0.0f) {
			angleDeg += 360.0f;
		}

		return angleDeg * 0.017453f;
	}

	static float calculateVectorAzimuthCounterClockwise(const SSD::SimPoint3D& direction)
	{
		// 使用 x, y 分量计算角度，正东为 0
		double angle = std::atan2(direction.y, direction.x);

		// 将 [-π, π) 映射到 [0, 2π)
		if (angle < 0)
			angle += 2 * M_PI;

		return angle;
	}

	template <typename T>
	static int Sign(const T val)
	{
		return (val > (T)0) - (val < (T)0);
	}

	template <typename T>
	static bool IsBetween(const T val, const T p1, const T p2)
	{
		return (p1 < val && val < p2) || (p2 < val && val < p1);
	}

	template <typename T>
	static bool IsBetweenMargin(const T val, const T p1, const T p2)
	{
		return (p1 <= val && val <= p2) || (p2 <= val && val <= p1);
	}

	static bool InRectangle(const SSD::SimPoint3D& pt, const SSD::SimPoint3D& vertex1, const SSD::SimPoint3D& vertex2)
	{
		return IsBetween(pt.x, vertex1.x, vertex2.x) && IsBetween(pt.y, vertex1.y, vertex2.y);
	}

	static bool InRectangleMargin(const SSD::SimPoint3D& pt, const SSD::SimPoint3D& vertex1, const SSD::SimPoint3D& vertex2)
	{
		return IsBetweenMargin(pt.x, vertex1.x, vertex2.x) && IsBetweenMargin(pt.y, vertex1.y, vertex2.y);
	}

	template <typename T>
	static bool CloseEnough(const T p1, const T p2, const T threshold)
	{
		return std::abs(p1 - p2) < threshold;
	}

	static double Length(const SSD::SimPoint2D& vec)
	{
		return sqrt(vec.x * vec.x + vec.y * vec.y);
	}

	static double Length(const SSD::SimPoint3D& vec)
	{
		return sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
	}

	static double Dot(const SSD::SimPoint2D& v1, const SSD::SimPoint2D v2)
	{
		return v1.x * v2.x + v1.y * v2.y;
	}

	static double Dot(const SSD::SimPoint3D& v1, const SSD::SimPoint3D v2)
	{
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	// Compute the angle between two 2D vectors
	static double Angle(const SSD::SimPoint2D& v1, const SSD::SimPoint2D& v2)
	{
		double l1 = Length(v1);
		double l2 = Length(v2);
		double dot = Dot(v1, v2);
		return acos(Dot(v1, v2) / (Length(v1) * Length(v2)));
	}

	static double Distance(const SSD::SimPoint2D& pt1, const SSD::SimPoint2D& pt2)
	{
		return std::sqrt(std::pow(pt1.x - pt2.x, 2) + std::pow(pt1.y - pt2.y, 2));
	}

	static double Distance(const SSD::SimPoint3D& pt1, const SSD::SimPoint3D& pt2)
	{
		return std::sqrt(std::pow(pt1.x - pt2.x, 2) + std::pow(pt1.y - pt2.y, 2) + std::pow(pt1.z - pt2.z, 2));
	}

	static double PlanarDistance(const SSD::SimPoint3D& pt1, const SSD::SimPoint3D& pt2)
	{
		return std::sqrt(std::pow(pt1.x - pt2.x, 2) + std::pow(pt1.y - pt2.y, 2));
	}

	// Rotate a point for a certain angle
	static SSD::SimPoint2D Rotate(const SSD::SimPoint2D& ori, const double theta)
	{
		SSD::SimPoint2D res;
		res.x = ori.x * cos(theta) + ori.y * sin(theta);
		res.y = -ori.x * sin(theta) + ori.y * cos(theta);
		return std::move(res);
	}

	// Translate a point with a certain vector
	static SSD::SimPoint2D Translate(const SSD::SimPoint2D& ori, const SSD::SimPoint2D& dir)
	{
		SSD::SimPoint2D res;
		res.x = ori.x + dir.x;
		res.y = ori.y + dir.y;
		return std::move(res);
	}

	// Transform a point in local coordinate to global coordinate
	static SSD::SimPoint2D LocalToGlobal(const SSD::SimPoint2D& originCoord, const double theta, const SSD::SimPoint2D& ptLocal)
	{
		SSD::SimPoint2D rotatedPt = Rotate(ptLocal, -theta);
		SSD::SimPoint2D res = Translate(rotatedPt, { originCoord.x, originCoord.y });
		return std::move(res);
	}

	/* 单位转换 UtilUnit */
	static double RadToDegree(const double rad)
	{
		return rad * 180. / M_PI;
	}

	static double DegreeToRad(const double degree)
	{
		return degree * M_PI / 180.;
	}

	static double MsToKmH(const double ms)
	{
		return ms * 3.6;
	}

	static double KmHToMs(const double kmh)
	{
		return kmh / 3.6;
	}
};