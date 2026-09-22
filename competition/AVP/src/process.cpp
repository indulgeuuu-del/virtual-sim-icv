#include "process.h"
#include "SampleGetNearMostLane.h"
#include "UtilMath.h"
#include "UtilAVP.hpp"
#include "define.h"

// 查找目标停车位
void findTargetParkingSpace(const SSD::SimVector<HDMapStandalone::MParkingSpace>& parkingSpaces, const SimOne_Data_Obstacle* obstacles, HDMapStandalone::MParkingSpace& targetParkingSpace)
{
	std::vector<size_t> availableParkingSpaceIndices;
	for (size_t i = 0; i < parkingSpaces.size(); ++i)
	{
		auto& parkingSpace = parkingSpaces[i];
		auto& vertices = parkingSpace.boundaryKnots;
		bool occupied = false;

		for (size_t j = 0; j < obstacles->obstacleSize; ++j)
		{
			auto& obstacle = obstacles->obstacle[j];
			SSD::SimPoint3D pos(obstacle.posX, obstacle.posY, obstacle.posZ);
			if (UtilAVP::InRectangleMargin(pos, vertices[0], vertices[2]))
			{
				occupied = true;
				break;
			}
		}

		if (!occupied)
		{
			availableParkingSpaceIndices.push_back(i);
		}
	}

	targetParkingSpace = parkingSpaces[availableParkingSpaceIndices[0]];
}

// 重新排列停车位的边界顶点，使其从左上角顶点开始
void rearrangeKnots(HDMapStandalone::MParkingSpace& space)
{
	size_t knotSize = space.boundaryKnots.size();
	SSD::SimPoint2D heading{ space.heading.x, space.heading.y };
	SSD::SimPoint3DVector arrangedBoundaryKnots(knotSize);
	double headingErrorThresholdDegree = 10.;

	for (size_t i = 0; i < knotSize; ++i)
	{
		SSD::SimPoint2D dir = { space.boundaryKnots[i + 1].x - space.boundaryKnots[i].x,
								space.boundaryKnots[i + 1].y - space.boundaryKnots[i].y };
		double angle = UtilAVP::Angle(heading, dir);
		if (angle > 0. && angle < UtilAVP::DegreeToRad(headingErrorThresholdDegree))
		{
			for (size_t j = 0; j < knotSize; ++j)
			{
				size_t index = i + j + 2 < knotSize ? i + j + 2 : i + j + 2 - knotSize;
				arrangedBoundaryKnots[j] = space.boundaryKnots[index];
			}
			break;
		}
	}

	for (size_t i = 0; i < knotSize; ++i)
	{
		space.boundaryKnots[i] = arrangedBoundaryKnots[i];
	}
}

// 重新排列停车位的边界顶点，使其从左上角顶点开始
SSD::SimPoint3DVector rearrangeKnots(const HDMapStandalone::MParkingSpace& space, const SSD::SimPoint3D& m_heading)
{
	size_t knotSize = space.boundaryKnots.size();
	SSD::SimPoint2D heading{ m_heading.x, m_heading.y };
	SSD::SimPoint3DVector arrangedBoundaryKnots(knotSize);
	double headingErrorThresholdDegree = 10.;

	for (size_t i = 0; i < knotSize; ++i)
	{
		const SSD::SimPoint3D& p1 = space.boundaryKnots[i];
		const SSD::SimPoint3D& p2 = space.boundaryKnots[(i + 1) % knotSize];

		SSD::SimPoint2D dir = { p2.x - p1.x, p2.y - p1.y };
		double angle = UtilAVP::Angle(heading, dir);

		if (angle > -0.01 && angle < UtilAVP::DegreeToRad(headingErrorThresholdDegree))
		{
			for (size_t j = 0; j < knotSize; ++j)
			{
				size_t index = (i + j + 2) % knotSize;
				arrangedBoundaryKnots[j] = space.boundaryKnots[index];
			}
			return arrangedBoundaryKnots;
		}
	}

	return space.boundaryKnots; // 如果没有找到匹配的方向，则返回原始顺序
}

// 判断停车位是否为垂直泊车
bool isVerticalParking(const HDMapStandalone::MParkingSpace& parkingSpace)
{
	ASSERT(parkingSpace.boundaryKnots.size() >= 4, "无效停车位：少于 4 个角点");
	double slotL = UtilAVP::PlanarDistance(parkingSpace.boundaryKnots[3], parkingSpace.boundaryKnots[0]);
	double slotW = UtilAVP::PlanarDistance(parkingSpace.boundaryKnots[3], parkingSpace.boundaryKnots[2]);
	return slotL >= slotW;
}

/**************************************** ParkingSlot ****************************************/
// 判别当前场景是水平泊车还是垂直泊车
void ParkingSlot::judgeType(const HDMapStandalone::MParkingSpace& space)
{
	type = isVerticalParking(targetParkingSpace) ? Type::Vertical : Type::Lateral;
}

// 判别车位在地图上侧还是下侧
void ParkingSlot::judgeSide(const HDMapStandalone::MParkingSpace& space)
{
	SSD::SimString lane = SampleGetNearMostLane(space.pt);
	HDMapStandalone::MLaneInfo laneInfo;
	SimOneAPI::GetLaneSample(lane, laneInfo);
	float referenceY = laneInfo.centerLine[0].y;

	if (space.pt.y >= referenceY) side = Side::Upside;
	else side = Side::Downside;
}

// 判断主车是自西向东还是自东向西
void ParkingSlot::judgeDirection(float vx, float vy)
{
	static float vehicleAzimuth;
	static bool gotten = false;

	if (!gotten && UtilMath::calculateSpeed(vx, vy) > 1e-2) // 当主车有速度时，测算其朝向
	{
		vehicleAzimuth = UtilAVP::calculateResultantAzimuth(vx, vy);
		if ((1.75f * M_PI <= vehicleAzimuth && vehicleAzimuth < 2.0f * M_PI) ||
			(0.0f <= vehicleAzimuth && vehicleAzimuth < 0.25f * M_PI)) direction = Direction::West2East;
		else direction = Direction::East2West;

		gotten = true;
	}
}

// 建立车位坐标系
void ParkingSlot::buildCoordinate(const HDMapStandalone::MParkingSpace& space)
{
	/* 说明：如果想要创建一个从点 0 指向 1 的向量，则调用 DIR2PT(0, 1) */
	#define DIR(perior, latter, symbol) (space.boundaryKnots[latter].symbol - space.boundaryKnots[perior].symbol)
	#define DIR2PT(perior, latter) (SSD::SimPoint3D(DIR(perior, latter, x), DIR(perior, latter, y), DIR(perior, latter, z)))

	// 主车的方向向量
	SSD::SimPoint3D vehicleDirection = (direction == Direction::West2East) ? SSD::SimPoint3D(1.0, 0.0, 0.0) : SSD::SimPoint3D(-1.0, 0.0, 0.0);
	// Y 轴正方向
	SSD::SimPoint3D yAxisDirection = (side == Side::Downside) ? DIR2PT(0, 3) : DIR2PT(3, 0);
	// X 轴正方向：
	// Y 轴正方向 × 主车的方向向量，叉乘结果为正：则说明 [主车的方向向量] 在 [Y 轴正方向] 的逆时针方向，反之逆时针方向
	LocalCoordinate::XAxisDirection xAxisDirection = UtilMath::crossProduct(yAxisDirection.x, yAxisDirection.y, vehicleDirection.x, vehicleDirection.y) > 0.0f ? LocalCoordinate::XAxisDirection::CounterClockwise : LocalCoordinate::XAxisDirection::Clockwise;

	// 建立局部车位坐标系
	localCoordinate = LocalCoordinate(yAxisDirection, xAxisDirection);

	// 储存 X、Y 轴方向向量
	xDirection = vehicleDirection;
	yDirection = yAxisDirection;
}

// 处理车位角点
void ParkingSlot::processBoundaryKnots(const HDMapStandalone::MParkingSpace& space)
{
	originalKnots = space.boundaryKnots;
	boundaryKnots = rearrangeKnots(space, (side == Side::Downside) ? DIR2PT(0, 3) : DIR2PT(3, 0));

	// Y 轴在 X 轴的顺时针方向 / 逆时针方向
	clockwise = !(UtilMath::crossProduct(xDirection.x, xDirection.y, yDirection.x, yDirection.y) > 0.0f);
	if (clockwise) // 如果 Y 轴在 X 轴的顺时针方向，就要把角点进行三次重排列，更改为顺时针排序，右上角为 0
	{
		std::swap<SSD::SimPoint3D>(boundaryKnots[0], boundaryKnots[3]);
		std::swap<SSD::SimPoint3D>(boundaryKnots[1], boundaryKnots[2]);
	}
}