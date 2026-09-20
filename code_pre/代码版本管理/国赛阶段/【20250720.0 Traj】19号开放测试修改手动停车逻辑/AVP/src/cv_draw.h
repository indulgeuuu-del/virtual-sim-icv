#include "opencv2/opencv.hpp"
#include "SSD/SimPoint2D.h"
#include "SSD/SimPoint3D.h"
#include "SSD/SimVector.h"

/**
 * @brief 将一个点按照指定方向和平移距离进行平移
 *
 * @param point 原始点，类型为 SSD::SimPoint3D
 * @param theta 朝向角（相对于正东方向，顺时针为正，单位为弧度，范围 [0, 2π]）
 * @param distance 平移距离，单位为米
 * @return SSD::SimPoint3D 平移后的点
 */
extern SSD::SimPoint3D translatePoint(const SSD::SimPoint3D& point, float theta, float distance);

extern void drawPolygon(cv::Mat& image, const SSD::SimPoint3DVector& points, const cv::Scalar& color, float scale, const cv::Point2i& origin, const std::string& label);
extern void drawSpline(cv::Mat& image, const SSD::SimPoint3DVector& points, const cv::Scalar& color, float scale, const cv::Point2i& origin, const std::string& label);
extern void drawScatter(cv::Mat& image, const SSD::SimPoint3DVector& points, const cv::Scalar& color, float scale, const cv::Point2i& origin, const std::string& label);
extern void drawScatter(cv::Mat& image, const std::vector<SSD::SimPoint3DVector>& pointsList, const cv::Scalar& color, float scale, const cv::Point2i& origin, const std::string& label);

// 计算所有点的包围盒，基于 scale 计算画布大小和原点坐标（原点在图像坐标系中的位置）
void computeCanvasAndOrigin(const SSD::SimPoint3DVector& points, float scale, cv::Size& outCanvasSize, cv::Point2i& outOrigin);
void computeCanvasAndOrigin(const std::vector<SSD::SimPoint3DVector>& pointSets, float scale, cv::Size& outCanvasSize, cv::Point2i& outOrigin);
template<typename... PointSets>
void computeCanvasAndOrigin(float scale, cv::Size& outCanvasSize, cv::Point2i& outOrigin, const PointSets&... pointSets);