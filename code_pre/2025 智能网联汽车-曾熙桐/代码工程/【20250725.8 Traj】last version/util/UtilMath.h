#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include "Service/SimOneIOStruct.h"
#include "SSD/SimPoint3D.h"
#include "SSD/SimPoint2D.h"

class UtilMath {
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

	// 计算向量的单位向量（三维）
	static SSD::SimPoint3D normalize(const SSD::SimPoint3D& vec) {
		float length = std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
		if (length > 1e-6f) { // 避免除零
			return { vec.x / length, vec.y / length, vec.z / length };
		}
		return { 0.0f, 0.0f, 0.0f }; // 零向量
	}

	// 计算向量的点积（三维）
	static float dotProduct(const SSD::SimPoint3D& v1, const SSD::SimPoint3D& v2) {
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	// 计算向量的叉积（二维）
	static float crossProduct(float x1, float y1, float x2, float y2) {
		return x1 * y2 - y1 * x2;
	}
};

class UtilGeometry {
public:
	// 判断两线段 (p1, p2) 与 (q1, q2) 是否在二维平面相交
	static bool isSegmentsIntersect(const SSD::SimPoint3D& p1, const SSD::SimPoint3D& p2, const SSD::SimPoint3D& q1, const SSD::SimPoint3D& q2)
	{
		float dx1 = p2.x - p1.x;
		float dy1 = p2.y - p1.y;
		float dx2 = q1.x - p1.x;
		float dy2 = q1.y - p1.y;
		float dx3 = q2.x - p1.x;
		float dy3 = q2.y - p1.y;

		float c1 = UtilMath::crossProduct(dx1, dy1, dx2, dy2);
		float c2 = UtilMath::crossProduct(dx1, dy1, dx3, dy3);
		if (c1 * c2 > 0) return false;

		dx1 = q2.x - q1.x;
		dy1 = q2.y - q1.y;
		dx2 = p1.x - q1.x;
		dy2 = p1.y - q1.y;
		dx3 = p2.x - q1.x;
		dy3 = p2.y - q1.y;

		c1 = UtilMath::crossProduct(dx1, dy1, dx2, dy2);
		c2 = UtilMath::crossProduct(dx1, dy1, dx3, dy3);
		if (c1 * c2 > 0) return false;

		return true;
	}
	static bool isSegmentsIntersect(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
	{
		float dx1 = x2 - x1;
		float dy1 = y2 - y1;
		float dx2 = x3 - x1;
		float dy2 = y3 - y1;
		float dx3 = x4 - x1;
		float dy3 = y4 - y1;

		float c1 = UtilMath::crossProduct(dx1, dy1, dx2, dy2);
		float c2 = UtilMath::crossProduct(dx1, dy1, dx3, dy3);
		if (c1 * c2 >= 0) return false;

		dx1 = x4 - x3;
		dy1 = y4 - y3;
		dx2 = x1 - x3;
		dy2 = y1 - y3;
		dx3 = x2 - x3;
		dy3 = y2 - y3;

		c1 = UtilMath::crossProduct(dx1, dy1, dx2, dy2);
		c2 = UtilMath::crossProduct(dx1, dy1, dx3, dy3);
		if (c1 * c2 >= 0) return false;

		return true;
	}

	// 判断两段曲线是否相交
	static bool curvesIntersect(const std::vector<SSD::SimPoint3D>& curve1, const std::vector<SSD::SimPoint3D>& curve2)
	{
		if (curve1.size() < 2 || curve2.size() < 2) return false;

		for (size_t i = 0; i + 1 < curve1.size(); ++i)
		{
			const SSD::SimPoint3D& p1 = curve1[i];
			const SSD::SimPoint3D& p2 = curve1[i + 1];

			for (size_t j = 0; j + 1 < curve2.size(); ++j)
			{
				const SSD::SimPoint3D& q1 = curve2[j];
				const SSD::SimPoint3D& q2 = curve2[j + 1];

				if (isSegmentsIntersect(p1, p2, q1, q2))
				{
					return true;
				}
			}
		}

		return false;
	}
	static bool curvesIntersect(const SimOne_Data_Vec3f* curve1, size_t curve1_size, const SSD::SimPoint3DVector& curve2)
	{
		std::cout << "进来啦" << std::endl;
		std::cout << "sizeof = " << (sizeof(curve1) / sizeof(curve1[0])) << std::endl;
		std::cout << "sizein = " << curve1_size << std::endl;

		if (curve1_size < 2 || curve2.size() < 2) return false;

		for (size_t i = 0; i + 1 < curve1_size; ++i)
		{
			float x1 = curve1[i].x;
			float y1 = curve1[i].y;
			float x2 = curve1[i + 1].x;
			float y2 = curve1[i + 1].y;

			for (size_t j = 0; j + 1 < curve2.size(); ++j)
			{
				float x3 = curve2[j].x;
				float y3 = curve2[j].y;
				float x4 = curve2[j + 1].x;
				float y4 = curve2[j + 1].y;

				if (isSegmentsIntersect(x1, y1, x2, y2, x3, y3, x4, y4))
				{
					return true;
				}
			}
		}

		return false;
	}
	//根据灰色预测轨迹判断相交
	static bool curvesIntersect(const std::vector<SimOne_Data_Vec3f>& curve1, const SSD::SimPoint3DVector& curve2)
	{
		std::cout << "进来啦" << std::endl;
		std::cout << "sizein = " << curve1.size() << std::endl;

		if (curve1.size() < 2 || curve2.size() < 2) {
			std::cout << "点的数目不够"<<std::endl;
			return false; }

		for (size_t i = 0; i + 1 < curve1.size(); ++i)
		{
			float x1 = curve1[i].x;
			float y1 = curve1[i].y;
			float x2 = curve1[i + 1].x;
			float y2 = curve1[i + 1].y;

			for (size_t j = 0; j + 1 < curve2.size(); ++j)
			{
				float x3 = curve2[j].x;
				float y3 = curve2[j].y;
				float x4 = curve2[j + 1].x;
				float y4 = curve2[j + 1].y;

				if (isSegmentsIntersect(x1, y1, x2, y2, x3, y3, x4, y4))
				{
					return true;
				}
			}
		}
		return false;
	}
};