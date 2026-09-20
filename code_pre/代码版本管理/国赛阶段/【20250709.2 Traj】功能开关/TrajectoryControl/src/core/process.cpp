#include <set>
#include <memory>
#include "SimOneServiceAPI.h"
#include "SimOneSensorAPI.h"
#include "vehicle.h"
#include "obstacle.h"
#include "process.h"

double getRoadWidth(const SSD::SimString& laneId, const SSD::SimPoint3D& pos)
{
	double totalWidth = 0.0;
	std::set<SSD::SimString> visitedLanes; // 记录已访问的车道，避免重复计算

	// 使用 Lambda 递归计算邻接车道的总宽度，注：模板递归Lambda（隐式Y组合子模式），与普通递归函数相当（零额外开销）
	auto getNeighborLaneWidth = [&](const SSD::SimString& laneId, const SSD::SimPoint3D& pos, auto& getNeighborLaneWidthRef) -> void {
		// 防止重复访问
		if (visitedLanes.find(laneId) != visitedLanes.end()) return;
		visitedLanes.insert(laneId);

		// 获取当前车道的宽度
		double laneWidth = 0.0;
		if (SimOneAPI::GetLaneWidth(laneId, pos, laneWidth))
		{
			totalWidth += laneWidth;
		}

		// 获取车道连接信息
		HDMapStandalone::MLaneLink linkLane;
		if (SimOneAPI::GetLaneLink(laneId, linkLane))
		{
			// 递归获取左邻接车道的宽度
			if (!linkLane.leftNeighborLaneName.Empty())
			{
				getNeighborLaneWidthRef(linkLane.leftNeighborLaneName, pos, getNeighborLaneWidthRef);
			}

			// 递归获取右邻接车道的宽度
			if (!linkLane.rightNeighborLaneName.Empty())
			{
				getNeighborLaneWidthRef(linkLane.rightNeighborLaneName, pos, getNeighborLaneWidthRef);
			}
		}
		};

	// 计算当前车道及其邻接车道的总宽度
	getNeighborLaneWidth(laneId, pos, getNeighborLaneWidth);

	return totalWidth;
}

float getLaneAzimuth(const SSD::SimString& laneId)
{
	SSD::SimPoint3D pt, dir;
	SimOneAPI::GetInertialFromLaneST(laneId, 0.0, 0.0, pt, dir);
	float azimuth = std::atan2(dir.y, dir.x) * 57.29578f;
	return azimuth < 0.0f ? azimuth + 360.0f : azimuth;
}

bool getValidTrafficLight(const SSD::SimVector<HDMapStandalone::MSignal>& list, HDMapStandalone::MSignal& light)
{
	std::unique_ptr<SimOne_Data_TrafficLight> pTrafficLight = std::make_unique< SimOne_Data_TrafficLight>();
	for (size_t i = 0, ie = list.size(); i < ie; ++i)
	{
		SimOneAPI::GetTrafficLight(mainVehicle.id, list[i].id, pTrafficLight.get());
		if (pTrafficLight->isMainVehicleNextTrafficLight)
		{
			if (pTrafficLight->status == ESimOne_TrafficLight_Status::ESimOne_TrafficLight_Status_Red || pTrafficLight->status == ESimOne_TrafficLight_Status::ESimOne_TrafficLight_Status_Yellow) // 红灯、黄灯
			{
				light = list[i];
				return true;
			}
		}
	}
	return false;
}

// 判断主车轨迹是否涉及变道过程
bool isPathRequireLaneChange(const SSD::SimVector<SSD::SimPoint3D>& path)
{
	if (targetPath.size() < 2) {
		return false; // 路径过短，无法判断
	}
	SSD::SimString srcLaneId = m_SampleGetNearMostLane(path[0]);
	SSD::SimString dstLaneId = m_SampleGetNearMostLane(path.back());
	if (strcmp(srcLaneId.GetString(), dstLaneId.GetString()) != 0) return true;
	return false;
}

// 判断主车轨迹与对手车辆轨迹是否会产生干涉
bool isTrajInterfere(const SSD::SimVector<SSD::SimPoint3D>& path, const Obstacle& obstacle)
{
	auto arePointsOnSameSide = [](const SSD::SimPoint3D& pt1, const SSD::SimPoint3D& pt2, const SSD::SimPoint3D& linePoint, float azimuthDeg) {
		// 计算直线的方向向量
		float azimuth = azimuthDeg * 3.14159265358979323846f / 180.0f;
		SSD::SimPoint3D lineDirection = { cos(azimuth), sin(azimuth), 0.0f };
		// 计算起点和终点相对于直线的位置
		float cross1 = crossProduct(linePoint, lineDirection, pt1);
		float cross2 = crossProduct(linePoint, lineDirection, pt2);
		// 判断它们是否在直线的同一侧
		return (cross1 * cross2 >= 0);  // 叉积的符号相同，表示在同一侧
		};

	// 判断轨迹起点和终点是否在速度矢量的同一侧
	//return arePointsOnSameSide(path[0], path.back(), obstacle.pt, mainVehicle.laneAzimuth);
	return arePointsOnSameSide(mainVehicle.pt, path.back(), obstacle.pt, mainVehicle.laneAzimuth);
}

// 判断两条道路是否具有相同的道路 Road ID
bool isSameRoadId(const SSD::SimString& roadStr1, const SSD::SimString& roadStr2)
{
	std::string str1(roadStr1.GetString());
	std::string str2(roadStr2.GetString());

	size_t pos1 = str1.find('_');
	size_t pos2 = str2.find('_');

	if (pos1 == std::string::npos || pos2 == std::string::npos)
	{
		return false; // 格式不正确，无法比较
	}

	return str1.substr(0, pos1) == str2.substr(0, pos2);
}
bool isSameRoadId(const SSD::SimPoint3D& p1, const SSD::SimPoint3D& p2)
{
	return isSameRoadId(m_SampleGetNearMostLane(p1), m_SampleGetNearMostLane(p2));
}

// 获取两个点之间的 st 距离
float getDistS(const SSD::SimPoint3D& currentPos, const SSD::SimPoint3D& target)
{
	SSD::SimString currentPosLaneId = m_SampleGetNearMostLane(currentPos);
	SSD::SimString targetPosLaneId = m_SampleGetNearMostLane(target);

	if (isSameRoadId(currentPosLaneId, targetPosLaneId))
	{
		double currentPos_s, currentPos_t, targetPos_s, targetPos_t;
		if (!SimOneAPI::GetLaneST(currentPosLaneId, currentPos, currentPos_s, currentPos_t))
		{
			globalLogger(Logger::Color::BrightGreen) << "获取两个点之间的 ST 距离时，主车 ST 坐标获取失败";
		}
		if (!SimOneAPI::GetLaneST(currentPosLaneId, target, targetPos_s, targetPos_t))
		{
			globalLogger(Logger::Color::BrightGreen) << "获取两个点之间的 ST 距离时，目标点 ST 坐标获取失败";
		}

		return static_cast<float>(targetPos_s - currentPos_s);
	}

	return std::numeric_limits<float>::max();
}
float getDistT(const SSD::SimPoint3D& currentPos, const SSD::SimPoint3D& target)
{
	SSD::SimString currentPosLaneId = m_SampleGetNearMostLane(currentPos);
	SSD::SimString targetPosLaneId = m_SampleGetNearMostLane(target);
	if (isSameRoadId(currentPosLaneId, targetPosLaneId))
	{
		double currentPos_s, currentPos_t, targetPos_s, targetPos_t;
		if (!SimOneAPI::GetLaneST(currentPosLaneId, currentPos, currentPos_s, currentPos_t))
		{
			globalLogger(Logger::Color::BrightGreen) << "获取两个点之间的 ST 距离时，主车 ST 坐标获取失败";
		}
		if (!SimOneAPI::GetLaneST(currentPosLaneId, target, targetPos_s, targetPos_t))
		{
			globalLogger(Logger::Color::BrightGreen) << "获取两个点之间的 ST 距离时，目标点 ST 坐标获取失败";
		}

		return static_cast<float>(targetPos_t - currentPos_t);
	}
}
float getDistT(const SSD::SimPoint3D& currentPos, const SimOne_Data_Vec3f& target)
{
	SSD::SimPoint3D  point = SSD::SimPoint3D(target.x, target.y, target.z);
	SSD::SimString currentPosLaneId = m_SampleGetNearMostLane(currentPos);
	SSD::SimString targetPosLaneId = m_SampleGetNearMostLane(point);
	if (isSameRoadId(currentPosLaneId, targetPosLaneId))
	{
		double currentPos_s, currentPos_t, targetPos_s, targetPos_t;
		if (!SimOneAPI::GetLaneST(currentPosLaneId, currentPos, currentPos_s, currentPos_t))
		{
			globalLogger(Logger::Color::BrightGreen) << "获取两个点之间的 ST 距离时，主车 ST 坐标获取失败";
		}
		if (!SimOneAPI::GetLaneST(currentPosLaneId, point, targetPos_s, targetPos_t))
		{
			globalLogger(Logger::Color::BrightGreen) << "获取两个点之间的 ST 距离时，目标点 ST 坐标获取失败";
		}

		return static_cast<float>(targetPos_t - currentPos_t);
	}
}

// 查看有无斑马线
bool getCrossWalk(void)
{
	SimOneAPI::GetSpecifiedLaneCrosswalkList(mainVehicle.laneID, crosswalkList);

	if (crosswalkList.size() != 0) { return true; }
	return false;
}



bool equidistantSampling(const SSD::SimPoint3DVector& initPath, SSD::SimPoint3DVector& targetPath, float dist)
{
	targetPath.clear();
	if (initPath.size() < 2) return false;

	float acc = 0.f;
	targetPath.push_back(initPath[0]);

	for (size_t i = 1; i < initPath.size(); ++i) {
		float dx = initPath[i].x - initPath[i - 1].x;
		float dy = initPath[i].y - initPath[i - 1].y;
		float segLen = std::hypot(dx, dy);

		while (acc + dist <= segLen) {
			float r = (acc + dist) / segLen;
			SSD::SimPoint3D p
			{
				initPath[i - 1].x + r * dx,
				initPath[i - 1].y + r * dy,
				initPath[i - 1].z + r * (initPath[i].z - initPath[i - 1].z)
			};
			targetPath.push_back(p);
			acc += dist;
		}
		acc -= segLen;
	}
	targetPath.push_back(initPath.back());
	if (targetPath.size() <= 1) return false; // 如果只有起点，就当作失败
	return true;
}