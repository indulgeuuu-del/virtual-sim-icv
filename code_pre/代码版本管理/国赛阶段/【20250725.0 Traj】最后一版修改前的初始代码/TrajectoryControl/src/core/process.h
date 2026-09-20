#pragma once
#include <cmath>
#include <corecrt_math_defines.h>
#include "SSD/SimPoint3D.h"
#include "SSD/SimString.h"
#include "SimOneHDMapAPI.h"

class Obstacle;

extern double getRoadWidth(const SSD::SimString& laneId, const SSD::SimPoint3D& pos);
extern float getLaneAzimuth(const SSD::SimString& laneId);
extern bool getValidTrafficLight(const SSD::SimVector<HDMapStandalone::MSignal>& list, HDMapStandalone::MSignal& light);

// 计算合成矢量的方位角（ENU 坐标系下，即以正东为 0 度，逆时针为正方向）
inline float calculateResultantAzimuth(float xVelocity, float yVelocity)
{
	float azimuth = std::atan2f(yVelocity, xVelocity) * 180.0f / M_PI;
	return azimuth < 0 ? (azimuth + 360.0f) : azimuth; // 将角度范围调整为 [0, 360)
}

/**
 * @brief 计算二维向量的叉积，用于判断三点构成的转向关系。
 *
 * 给定三个点 a、b、c，分别计算向量 AB 和向量 AC，
 * 返回它们的二维叉积（ux * vy - uy * vx）。
 *
 * 该叉积结果可用于判断点 c 相对于向量 AB 的方位关系：
 * - 返回值 > 0：点 c 在从 a 到 b 的方向的左侧（左转）
 * - 返回值 < 0：点 c 在从 a 到 b 的方向的右侧（右转）
 * - 返回值 = 0：三点共线（即点 c 在直线 AB 上）
 *
 * @param a 点 a，表示起点
 * @param b 点 b，表示方向向量的终点（从 a 指向 b）
 * @param c 点 c，表示需要判断其相对位置的点
 * @return float 叉积结果（正负表示左右，0 表示共线）
 */
inline float crossProduct(const SSD::SimPoint3D& a, const SSD::SimPoint3D& b, const SSD::SimPoint3D& c)
{
	float ux = b.x - a.x, uy = b.y - a.y;
	float vx = c.x - a.x, vy = c.y - a.y;
	return ux * vy - uy * vx;
}

extern bool isPathRequireLaneChange(const SSD::SimVector<SSD::SimPoint3D>& path);
extern bool isTrajInterfere(const SSD::SimVector<SSD::SimPoint3D>& path, const Obstacle& obstacle);

// 超车速度规划
inline float calculateTargetOvertakingVelocity(float v1, float s0, float s1, float L0, float L1, float La, float Lx)
{
	return v1 * La / ((s0 - s1) - 0.5 * (L0 + L1) + La - Lx);
}

// 判断两条道路是否具有相同的道路 Road ID
extern bool isSameRoadId(const SSD::SimString& roadStr1, const SSD::SimString& roadStr2);
extern bool isSameRoadId(const SSD::SimPoint3D& p1, const SSD::SimPoint3D& p2);

// 获取两个点之间的 st 距离
extern float getDistS(const SSD::SimPoint3D& currentPos, const SSD::SimPoint3D& target);
extern float getDistT(const SSD::SimPoint3D& currentPos, const SSD::SimPoint3D& target);
extern float getDistT(const SSD::SimPoint3D& currentPos, const SimOne_Data_Vec3f& target);

// brief 查看有无斑马线
extern bool getCrossWalk(void);
//brief 插值法获得两点间的轨迹
bool equidistantSampling(const SSD::SimPoint3DVector& initPath, SSD::SimPoint3DVector& targetPath, float dist);
//brief 计算一段轨迹的平均曲率
double approximateCurvature(const std::vector<SSD::SimPoint3D>& pts,
	double sampleArcDist = 0.5 /*米*/,
	double minValidCurv = 1e-6);