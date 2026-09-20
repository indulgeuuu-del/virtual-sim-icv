#include "cv_draw.h"

// 将一个点按照指定方向和平移距离进行平移
SSD::SimPoint3D translatePoint(const SSD::SimPoint3D& point, float theta, float distance)
{
	SSD::SimPoint3D result = point;
	result.x += distance * std::cos(theta);
	result.y += distance * std::sin(theta);
	return result;
}

// 将 ENU 实际坐标转换成图像坐标
static cv::Point2i convertToImageCoord(const SSD::SimPoint3D& pt, float scale, const cv::Point2i& origin)
{
	return cv::Point2i(
		static_cast<int>(pt.x * scale) + origin.x,
		origin.y - static_cast<int>(pt.y * scale)  // y 轴反转（图像 y 向下，实际 y 向上）
	);
}

void drawPolygon(cv::Mat& image, const SSD::SimPoint3DVector& points, const cv::Scalar& color, float scale, const cv::Point2i& origin, const std::string& label)
{
	std::vector<cv::Point> poly;
	for (const auto& pt : points) {
		poly.push_back(convertToImageCoord(pt, scale, origin));
	}
	const cv::Point* pts = poly.data();
	int npts = static_cast<int>(poly.size());
	cv::polylines(image, &pts, &npts, 1, true, color, 2);

	// 在第一个点附近显示 label
	if (!poly.empty()) {
		cv::putText(image, label, poly[0] + cv::Point(5, -5), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
	}
}

void drawSpline(cv::Mat& image, const SSD::SimPoint3DVector& points, const cv::Scalar& color, float scale, const cv::Point2i& origin, const std::string& label)
{
	std::vector<cv::Point> poly;
	for (const auto& pt : points) {
		poly.push_back(convertToImageCoord(pt, scale, origin));
	}
	const cv::Point* pts = poly.data();
	int npts = static_cast<int>(poly.size());
	cv::polylines(image, &pts, &npts, 1, false, color, 2);

	// 在第一个点附近显示 label
	if (!poly.empty()) {
		cv::putText(image, label, poly[0] + cv::Point(5, -5), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
	}
}

void drawScatter(cv::Mat& image, const SSD::SimPoint3DVector& points, const cv::Scalar& color, float scale, const cv::Point2i& origin, const std::string& label)
{
	std::vector<cv::Point> scatter;
	for (const auto& pt : points) {
		scatter.push_back(convertToImageCoord(pt, scale, origin));
	}

	// 绘制每个点（散点）
	for (const auto& pt : scatter) {
		cv::circle(image, pt, 2, color, cv::FILLED);  // 实心圆点
	}

	// 在第一个点附近显示 label
	if (!scatter.empty()) {
		cv::putText(image, label, scatter[0] + cv::Point(5, -5), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
	}
}
void drawScatter(cv::Mat& image, const std::vector<SSD::SimPoint3DVector>& pointsList, const cv::Scalar& color, float scale, const cv::Point2i& origin, const std::string& label)
{
	bool labelDrawn = false;

	for (const auto& points : pointsList) {
		std::vector<cv::Point> scatter;

		for (const auto& pt : points) {
			scatter.push_back(convertToImageCoord(pt, scale, origin));
		}

		// 绘制每个点
		for (const auto& pt : scatter) {
			cv::circle(image, pt, 2, color, cv::FILLED);
		}

		// 只在第一组非空点集中显示 label
		if (!labelDrawn && !scatter.empty()) {
			cv::putText(image, label, scatter[0] + cv::Point(5, -5),
				cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
			labelDrawn = true;
		}
	}
}

// 计算所有点的包围盒，基于 scale 计算画布大小和原点坐标（原点在图像坐标系中的位置）
void computeCanvasAndOrigin(const SSD::SimPoint3DVector& points, float scale, cv::Size& outCanvasSize, cv::Point2i& outOrigin)
{
	if (points.empty()) {
		outCanvasSize = cv::Size(100, 100);
		outOrigin = cv::Point2i(50, 50);
		return;
	}

	// 找到点集的包围盒（最小最大 x,y）
	float minX = points[0].x;
	float maxX = points[0].x;
	float minY = points[0].y;
	float maxY = points[0].y;

	for (const auto& pt : points) {
		if (pt.x < minX) minX = pt.x;
		if (pt.x > maxX) maxX = pt.x;
		if (pt.y < minY) minY = pt.y;
		if (pt.y > maxY) maxY = pt.y;
	}

	// 根据 scale 计算画布宽高（留点边距，比如20像素）
	const int margin = 100;
	int width = static_cast<int>((maxX - minX) * scale) + margin * 2;
	int height = static_cast<int>((maxY - minY) * scale) + margin * 2;

	outCanvasSize = cv::Size(width, height);

	// 原点在图像坐标系中应该对应实际坐标的 (minX, maxY)，因为 y 轴反转
	// 所以 origin.x = margin - minX * scale
	// origin.y = margin + maxY * scale
	outOrigin.x = margin - static_cast<int>(minX * scale);
	outOrigin.y = margin + static_cast<int>(maxY * scale);
}

void computeCanvasAndOrigin(const std::vector<SSD::SimPoint3DVector>& pointSets, float scale, cv::Size& outCanvasSize, cv::Point2i& outOrigin)
{
	SSD::SimPoint3DVector allPoints;

	// 用通用 lambda 遍历每个点集并逐点复制
	auto collect = [&](const SSD::SimPoint3DVector& pts) {
		for (const auto& pt : pts) {
			allPoints.push_back(pt);
		}
		};

	for (const auto& pts : pointSets) {
		collect(pts);
	}

	computeCanvasAndOrigin(allPoints, scale, outCanvasSize, outOrigin);
}

template<typename... PointSets> // 支持任意数量点集的 computeCanvasAndOrigin 扩展版本
void computeCanvasAndOrigin(float scale, cv::Size& outCanvasSize, cv::Point2i& outOrigin, const PointSets&... pointSets)
{
	SSD::SimPoint3DVector allPoints;

	// 用通用 lambda 遍历每个点集并逐点复制
	auto collect = [&](const SSD::SimPoint3DVector& pts) {
		for (const auto& pt : pts) {
			allPoints.push_back(pt);
		}
		};

	(collect(pointSets), ...); // fold expression

	computeCanvasAndOrigin(allPoints, scale, outCanvasSize, outOrigin);
}