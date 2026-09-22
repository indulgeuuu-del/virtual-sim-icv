#pragma once
#include "SimOneHDMapAPI.h"

/*
 * @function findTargetParkingSpace
 * @brief 查找目标停车位
 * @param parkingSpaces 所有可用的停车位列表（每个停车位包含边界顶点信息）
 * @param obstacles 所有障碍物的信息指针（包括位置、朝向、尺寸等）
 * @param[out] targetParkingSpace 输出找到的目标停车位（第一个未被障碍物占用的停车位）
 *
 * @details
 * 遍历所有停车位，并判断每个停车位是否被障碍物占用。
 * 判断方法是检查障碍物中心点是否落在停车位矩形范围内。
 * 若停车位未被占用，则加入可用停车位索引列表中。
 * 最后选取第一个可用的停车位作为目标停车位并返回。
 */
extern void findTargetParkingSpace(const SSD::SimVector<HDMapStandalone::MParkingSpace>& parkingSpaces,
	const SimOne_Data_Obstacle* obstacles, HDMapStandalone::MParkingSpace& targetParkingSpace);

/*
 * @function rearrangeKnots
 * @brief 重新排列停车位的边界顶点，使其从左上角顶点开始
 * @param space 输入的停车位对象，其 boundaryKnots 成员将被重新排列
 *
 * @details
 * 根据停车位的朝向向量，从其四个边界顶点中找到与朝向方向一致的边（允许一定角度误差），
 * 并据此重新排列顶点顺序，使得第一个顶点为停车位的左上角。
 * 重新排列后的顶点顺序将覆盖原有的 `space.boundaryKnots`。
 */
extern void rearrangeKnots(HDMapStandalone::MParkingSpace& space);
extern SSD::SimPoint3DVector rearrangeKnots(const HDMapStandalone::MParkingSpace& space, const SSD::SimPoint3D& m_heading);

/*
 * @function isVerticalParking
 * @brief 判断停车位是否为侧方位停车（横向停车）
 * @param parkingSpace 输入的停车位对象（包含至少 4 个角点）
 * @return 如果停车位为侧方位停车，返回 true；否则返回 false
 *
 * @details
 * 通过计算停车位第 4 个角点到第 1 个角点之间的距离（视为停车位长度），
 * 以及第 4 个角点到第 3 个角点之间的距离（视为停车位宽度），
 * 判断长度是否大于或等于宽度，从而确定是否为横向停车位。
 */
extern bool isVerticalParking(const HDMapStandalone::MParkingSpace& parkingSpace);

class LocalCoordinate {
public:
	enum class XAxisDirection {
		Clockwise,
		CounterClockwise
	};

	// 默认构造，初始化为与 ENU 坐标系完全一致
	LocalCoordinate() {
		yAxis_[0] = 0.0f;
		yAxis_[1] = 1.0f;
		xAxis_[0] = 1.0f;
		xAxis_[1] = 0.0f;
	}

	// 通过夹角方式设置（angle 是 Y 轴与正东的夹角，顺时针为正，范围为 [0, 2π)）
	LocalCoordinate(float yAxisAngleRad, XAxisDirection xDir) {
		setFromAngle(yAxisAngleRad, xDir);
	}

	// 通过方向向量方式设置
	LocalCoordinate(const SSD::SimPoint3D& yDir, XAxisDirection xDir) {
		setFromVector(static_cast<float>(yDir.x), static_cast<float>(yDir.y), xDir);
	}

	// ENU → 新坐标系
	SSD::SimPoint3D toLocal(const SSD::SimPoint3D& enuPoint) const {
		float x = enuPoint.x * xAxis_[0] + enuPoint.y * xAxis_[1];
		float y = enuPoint.x * yAxis_[0] + enuPoint.y * yAxis_[1];
		return { x, y, 0.0f };
	}

	// 新坐标系 → ENU
	SSD::SimPoint3D toENU(const SSD::SimPoint3D& localPoint) const {
		float x = localPoint.x * xAxis_[0] + localPoint.y * yAxis_[0];
		float y = localPoint.x * xAxis_[1] + localPoint.y * yAxis_[1];
		return { x, y, 0.0f };
	}

	// 在新坐标系中平移一个 ENU 点 (dx, dy 为局部坐标下的位移)
	SSD::SimPoint3D translateInLocal(const SSD::SimPoint3D& enuPoint, float dx, float dy) const {
		SSD::SimPoint3D local = toLocal(enuPoint);
		local.x += dx;
		local.y += dy;
		return toENU(local);
	}

private:
	float xAxis_[2]; // X 轴单位方向向量（ENU 坐标系）
	float yAxis_[2]; // Y 轴单位方向向量（ENU 坐标系）

	void setFromAngle(float angle, XAxisDirection xDir) {
		yAxis_[0] = std::cos(angle);
		yAxis_[1] = std::sin(angle);
		setXAxis(xDir);
	}

	void setFromVector(float yx, float yy, XAxisDirection xDir) {
		float length = std::sqrt(yx * yx + yy * yy);
		yAxis_[0] = yx / length;
		yAxis_[1] = yy / length;
		setXAxis(xDir);
	}

	void setXAxis(XAxisDirection xDir) {
		if (xDir == XAxisDirection::Clockwise) {
			xAxis_[0] = yAxis_[1];
			xAxis_[1] = -yAxis_[0];
		}
		else {
			xAxis_[0] = -yAxis_[1];
			xAxis_[1] = yAxis_[0];
		}
	}
};

class ParkingSlot {
public:
	enum class Type {
		Vertical,	// 垂直泊车
		Lateral		// 水平泊车
	};

	enum class Side {
		Upside,		// 车位位于上侧
		Downside	// 车位位于下侧
	};

	enum class Direction {
		West2East,	// 主车从西侧驶向东侧
		East2West	// 主车从东侧驶向西侧
	};

	Type type; // 垂直泊车还是水平泊车
	Side side; // 车位在上侧还是在下侧
	Direction direction; // 主车哪一侧驶向哪一侧

	SSD::SimPoint3D xDirection; // 车位坐标系 X 轴方向向量
	SSD::SimPoint3D yDirection; // 车位坐标系 Y 轴方向向量
	LocalCoordinate localCoordinate; // 与车位绑定的车位坐标系

	bool clockwise; // 角点按顺时针排列还是逆时针排列，true：顺时针，false：逆时针
	SSD::SimPoint3DVector originalKnots; // 原始的角点（一次重排列后的角点）
	SSD::SimPoint3DVector boundaryKnots; // 处理后的角点（二次重排列后的角点）

public:
	/* 判别当前场景是水平泊车还是垂直泊车 */
	void judgeType(const HDMapStandalone::MParkingSpace& space);

	/* 判别车位在地图上侧还是下侧 */
	void judgeSide(const HDMapStandalone::MParkingSpace& space);

	/* 判断主车是自西向东还是自东向西 */
	void judgeDirection(float vx, float vy);

	/* 建立车位坐标系 */
	void buildCoordinate(const HDMapStandalone::MParkingSpace& space);

	/* 处理车位角点 */
	void processBoundaryKnots(const HDMapStandalone::MParkingSpace& space);
};