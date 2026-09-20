#pragma once

#include "SSD/SimPoint3D.h"
#include "SSD/SimPoint2D.h"
#include <cmath>

class UtilMath
{
public:
	static double distance(const SSD::SimPoint3D& pt1, const SSD::SimPoint3D& pt2)
	{
		return std::sqrt(std::pow(pt1.x - pt2.x, 2) + std::pow(pt1.y - pt2.y, 2) + std::pow(pt1.z - pt2.z, 2));
	}

	static double distance(const SSD::SimPoint2D& pt1, const SSD::SimPoint2D& pt2)
	{
		return std::sqrt(std::pow(pt1.x - pt2.x, 2) + std::pow(pt1.y - pt2.y, 2));
	}

	static double distance(double pt1_x, double pt1_y, double pt2_x, double pt2_y)
	{
		return std::sqrt(std::pow(pt1_x - pt2_x, 2) + std::pow(pt1_y - pt2_y, 2));
	}

	static double planarDistance(const SSD::SimPoint3D& pt1, const SSD::SimPoint3D& pt2)
	{
		return std::sqrt(std::pow(pt1.x - pt2.x, 2) + std::pow(pt1.y - pt2.y, 2));
	}

	static double calculateSpeed(const double velX, const double velY, const double velZ)
	{
		return std::sqrt(std::pow(velX, 2) + std::pow(velY, 2) + std::pow(velZ, 2));
	}

	static double calculateSpeed(const double velX, const double velY)
	{
		return std::sqrt(std::pow(velX, 2) + std::pow(velY, 2));
	}

	// 计算 3D 向量的单位向量
	static SSD::SimPoint3D normalize(const SSD::SimPoint3D& vec) {
		float length = std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
		if (length > 1e-6f) { // 避免除零
			return { vec.x / length, vec.y / length, vec.z / length };
		}
		return { 0.0f, 0.0f, 0.0f }; // 零向量
	}

	// 计算 3D 向量的点积
	static float dotProduct(const SSD::SimPoint3D& v1, const SSD::SimPoint3D& v2) {
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}
};