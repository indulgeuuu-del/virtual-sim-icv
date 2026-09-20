#include "draw.h"
#include "drawOpendrive.h"
#include "tinyxml2.h"
#include <filesystem>
static double clamp(double value, double low, double high)
{
	return std::max(low, std::min(value, high));
}
void ViewParamsInit(std::vector<cv::Point2f> &allBoundaries, std::vector<cv::Point2f>&allJungles, std::vector<cv::Point2f> &TargetPath, ViewParams& params)
{
	// 计算地图边界
	 // 创建高分辨率画布并绘制：
	 // - 车道边界（黑色）
	 // - 路口区域（金色）
	 // - 目标路径（天蓝色）
	 // 初始化视图缩放和平移参数
	float minX = std::numeric_limits<float>::max();
	float maxX = -minX;
	float minY = minX, maxY = -minX;
	for (const auto& p : allBoundaries) {
		minX = std::min(minX, p.x);
		maxX = std::max(maxX, p.x);
		minY = std::min(minY, p.y);
		maxY = std::max(maxY, p.y);
	}

	// 扩展边界并计算高分辨率画布尺寸
	const int padding = 10;
	const int scaleFactor = 5;
	minX -= padding;
	maxX += padding;
	minY -= padding;
	maxY += padding;
	int widthHigh = (maxX - minX) * scaleFactor;
	int heightHigh = (maxY - minY) * scaleFactor;

	// 创建高分辨率画布
	cv::Mat canvasHigh(heightHigh, widthHigh, CV_8UC3, cv::Scalar(255, 255, 255));
	params.scale = 1.0;
	params.tx = params.ty = 0.0;
	params.isDragging = false;
	params.canvasHigh = canvasHigh;
	params.minX = minX;
	params.maxX = maxX;
	params.minY = minY;
	params.maxY = maxY;
	params.scaleFactor = scaleFactor;
	params.windowSize = { 800, 600 };

	// 创建高分辨率画布时使用 Y 轴翻转
	for (const auto& p : allBoundaries) {
		int x = (p.x - minX) * scaleFactor;
		int y = heightHigh - (p.y - minY) * scaleFactor;
		cv::circle(canvasHigh, { x, y }, 2, { 0, 0, 0 }, -1);
	}

	for (const auto& p : allJungles) {
		int x = (p.x - minX) * scaleFactor;
		int y = heightHigh - (p.y - minY) * scaleFactor;
		cv::circle(canvasHigh, { x, y }, 2, CV_COLOR_GOLD, -1);
	}
	for (const auto& p : TargetPath) {
		int x = (p.x - minX) * scaleFactor;
		int y = heightHigh - (p.y - minY) * scaleFactor;
		cv::circle(canvasHigh, { x, y }, 2, CV_COLOR_SKYBLUE, -1);
	}
	// 计算初始缩放和平移
	double initScale = std::min(
		static_cast<double>(params.windowSize.width) / widthHigh,
		static_cast<double>(params.windowSize.height) / heightHigh
	);
	params.scale = initScale;
	params.tx = (params.windowSize.width - widthHigh * initScale) / 2;
	params.ty = (params.windowSize.height - heightHigh * initScale) / 2;
}
// 计算两点之间的欧几里得距离
float distance(const cv::Point2f& p1, const cv::Point2f& p2) {
	float dx = p1.x - p2.x;
	float dy = p1.y - p2.y;
	return std::sqrt(dx * dx + dy * dy);
}

void generateInterpolatedRouteSegment(ViewParams* params, int startIdx, int endIdx, float interval = 0.2f) {
	if (startIdx < 0 || endIdx < 0 ||
		startIdx >= params->clickedPoints.size() ||
		endIdx >= params->clickedPoints.size()) {
		return;
	}

	const auto& p1 = params->clickedPoints[startIdx];
	const auto& p2 = params->clickedPoints[endIdx];
	float totalDist = distance(p1, p2);

	if (totalDist < 1e-3f) return; // 避免除以0或点重合

	int numSamples = std::max(1, static_cast<int>(totalDist / interval));

	RouteSegment segment;
	segment.startIndex = startIdx;
	segment.endIndex = endIdx;

	for (int i = 0; i <= numSamples; ++i) {
		float t = static_cast<float>(i) / numSamples;
		float x = (1 - t) * p1.x + t * p2.x;
		float y = (1 - t) * p1.y + t * p2.y;
		segment.routePoints.emplace_back(x, y);
	}

	params->routeSegments.push_back(segment);
}
void saveHistoryState(ViewParams* params)
{
	// 删除旧历史如果超过限制
	if (params->undoStack.size() > params->MAX_HISTORY)
	{
		std::stack<ViewParams::HistoryState> temp;
		while (params->undoStack.size() > params->MAX_HISTORY - 1) {
			temp.push(params->undoStack.top());
			params->undoStack.pop();
		}
		params->undoStack.swap(temp);
	}

	// 保存当前状态
	params->undoStack.push({
		params->Idx,
		params->clickedPoints,
		params->routeSegments,
		params->pointModes,
		params->stopTime,
		params->speed,
		params->steerKp,
		params->scrollOffset
		});
};
//绘制相关
#ifndef Draw
void ImageWarpAffine(ViewParams&params, cv::Mat& view)
{
	cv::Mat M = (cv::Mat_<double>(2, 3) <<
		params.scale, 0, params.tx,
		0, params.scale, params.ty
		);
	cv::warpAffine(
		params.canvasHigh, view, M, params.windowSize,
		cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar::all(255)
	);
}
void drawInsertBoard(ViewParams& params, cv::Mat& infoPanel)
{
	if (params.waitingForInput) {
		cv::rectangle(infoPanel,
			cv::Rect(0, infoPanel.rows - 100, infoPanel.cols, 100),
			cv::Scalar(150, 150, 150), cv::FILLED);

		// 绘制输入提示
		if(params.inputType!=ViewParams::InputType::INPUT_STOPTIME_VALUE)
		{
			cv::putText(infoPanel, "Input index (1~" + std::to_string(params.clickedPoints.size() + 1) + "):",
				cv::Point(10, infoPanel.rows - 70),
				cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
		}
		else
		{
			cv::putText(infoPanel, "Input StopTime",
				cv::Point(10, infoPanel.rows - 70),
				cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
		}

		// 绘制输入文本和光标
		std::string displayText = params.inputBuffer + "_"; // 添加光标效果
		cv::putText(infoPanel, displayText,
			cv::Point(10, infoPanel.rows - 30),
			cv::FONT_HERSHEY_SIMPLEX, 0.7,
			cv::Scalar(0, 0, 0), 2);
	}
}
void drawPath(ViewParams&params, cv::Mat& view)
{
	for (const auto& segment : params.routeSegments) {
		// 跳过无效索引段
		if (segment.startIndex >= params.clickedPoints.size() ||
			segment.endIndex >= params.clickedPoints.size()) continue;

		// 绘制轨迹线段
		cv::Point2f prevViewPoint;
		bool firstPoint = true;

		for (const auto& enuPt : segment.routePoints) {
			cv::Point2f imgPt = enuToImage(enuPt, params);
			cv::Point2f viewPt(
				imgPt.x * params.scale + params.tx,
				imgPt.y * params.scale + params.ty
			);

			if (!firstPoint) {
				cv::line(view, prevViewPoint, viewPt, params.routeColor, 2);
			}
			else {
				firstPoint = false;
			}
			prevViewPoint = viewPt;
		}
	}
}
void drawInfoBoard(ViewParams &params, cv::Mat &infoPanel)
{
	
	// 绘制点列表
	const int lineHeight = 25;
	int startY = 30; // 起始Y坐标
	// 计算可见范围
	int startIdx = params.scrollOffset;
	int endIdx = std::min(startIdx + params.maxVisiblePoints,
		(int)params.clickedPoints.size());
	// 仅绘制可见点
	for (int i = startIdx; i < endIdx; ++i) {
		int displayOrder = i + 1; // 显示序号从1开始
		int posY = startY + (i - startIdx) * lineHeight; // 相对位置计算

		std::stringstream ss;
		ss << std::fixed << std::setprecision(4)
			<< displayOrder << ": ["
			<< (int)params.clickedPoints[i].x << ", "
			<< (int)params.clickedPoints[i].y << ", " 
			<< params.pointModes[i]<<"," 
			<<params.stopTime[i] << ","
			<< (int)params.speed[i] << ","
			<< (int)params.steerKp[i] << ","
			<<"]";
		cv::putText(
			infoPanel, ss.str(),
			cv::Point(10, posY),
			cv::FONT_HERSHEY_SIMPLEX, 0.35,
			cv::Scalar(0, 0, 0), 1);
	}
}
void drawPoints(ViewParams& params, cv::Mat& view)
{
	for (size_t i = 0; i < params.clickedPoints.size(); ++i)
	{
		auto& p = params.clickedPoints[i];
		cv::Point2f imgPoint = enuToImage(p, params);
		cv::Point2f viewPoint(
			imgPoint.x * params.scale + params.tx,
			imgPoint.y * params.scale + params.ty
		);
		//绘制点的数字编号
		cv::putText(view, std::to_string(i + 1),
			viewPoint + cv::Point2f(5, -5),
			cv::FONT_HERSHEY_SIMPLEX, 0.5,
			cv::Scalar(255, 0, 0), 1);
		cv::circle(view, viewPoint, 3, { 0, 0, 255 }, -1);
	}
}

void extractLaneBoundaries(const SSD::SimVector<HDMapStandalone::MLaneInfo>& lanes, std::vector<cv::Point2f>& boundaries, std::vector<cv::Point2f>& jungles)
{
	boundaries.clear();
	jungles.clear();

	for (const auto& lane : lanes)
	{
		// 添加左边界点
		for (const auto& pt : lane.leftBoundary)
		{
			long jungleID;
			if (!SimOneAPI::IsInJunction(m_SampleGetNearMostLane(pt), jungleID))
			{
				boundaries.emplace_back(cv::Point2f(pt.x, pt.y));
			}
			else
			{
				jungles.emplace_back(cv::Point2f(pt.x, pt.y));
			}
		}

		// 添加右边界点
		for (const auto& pt : lane.rightBoundary)
		{
			long jungleID;
			if (!SimOneAPI::IsInJunction(m_SampleGetNearMostLane(pt), jungleID))
			{
				boundaries.emplace_back(cv::Point2f(pt.x, pt.y));
			}
			else
			{
				jungles.emplace_back(cv::Point2f(pt.x, pt.y));
			}
		}
	}
}
void extractTargetPath(SSD::SimPoint3DVector &targetpath, std::vector<cv::Point2f>& TargetPath)
{
	TargetPath.clear();
	// 添加右边界点
	for (const auto& pt : targetpath)
	{
		TargetPath.emplace_back(cv::Point2f(pt.x, pt.y));
	}

}
// 坐标转换函数（ENU <-> OpenCV）
cv::Point2f enuToImage(const cv::Point2f& enuPoint, const ViewParams& params) {
	float x = (enuPoint.x - params.minX) * params.scaleFactor;
	float y = (params.maxY - enuPoint.y) * params.scaleFactor; // Y轴翻转
	return { x, y };
}

// 坐标转换函数（ENU <-> OpenCV）
cv::Point2f imageToEnu(const cv::Point2f& imagePoint, const ViewParams& params) {
	float x = imagePoint.x / params.scaleFactor + params.minX;
	float y = params.maxY - imagePoint.y / params.scaleFactor; // Y轴翻转
	return { x, y };
}

// 新增轨迹生成辅助函数
void generateRouteSegment(ViewParams* params, int startIdx, int endIdx) {
	
	// 边界检查
	if (startIdx < 0 || endIdx < 0 ||
		startIdx >= params->clickedPoints.size() ||
		endIdx >= params->clickedPoints.size()) {
		return;
	}

	const auto& p1 = params->clickedPoints[startIdx];
	const auto& p2 = params->clickedPoints[endIdx];
	SSD::SimPoint3DVector inputPoints;
	inputPoints.push_back(SSD::SimPoint3D(p1.x, p1.y, 0));
	inputPoints.push_back(SSD::SimPoint3D(p2.x, p2.y, 0));

	// 生成新轨迹前清理旧数据
	auto& segments = params->routeSegments;
	segments.erase(
		std::remove_if(segments.begin(), segments.end(),
			[startIdx, endIdx](const RouteSegment& s) {
		return (s.startIndex == startIdx && s.endIndex == endIdx) ||
			(s.startIndex == endIdx && s.endIndex == startIdx);
	}),
		segments.end());
	SSD::SimVector<int> validIndices;
	SSD::SimPoint3DVector newRoute;
	if (SimOneAPI::GenerateRoute(inputPoints, validIndices, newRoute)) {
		RouteSegment segment;
		segment.startIndex = startIdx;
		segment.endIndex = endIdx;
		for (const auto& pt : newRoute) {
			segment.routePoints.emplace_back(pt.x, pt.y);
		}
		params->routeSegments.push_back(segment);
	}
}

// 绘制障碍物
void drawObstacle(std::vector<ObstacleXOSC>& obstacleList, ViewParams& params, cv::Mat& view) {
	for (const auto& obstacle : obstacleList) {
		// 转换四个角点到图像坐标系（ENU转OpenCV）
		auto convertPoint = [&](const cv::Point2f& pt) {
			cv::Point2f enuPt(pt.x, pt.y);
			cv::Point2f imgPt = enuToImage(enuPt, params);
			// 应用视图变换
			return cv::Point2f(
				imgPt.x * params.scale + params.tx,
				imgPt.y * params.scale + params.ty
			);
			};

		// 获取转换后的四个角点
		cv::Point2f tl = convertPoint(obstacle.tl);
		cv::Point2f tr = convertPoint(obstacle.tr);
		cv::Point2f br = convertPoint(obstacle.br);
		cv::Point2f bl = convertPoint(obstacle.bl);

		// 根据障碍物类型设置颜色
		cv::Scalar color;
		color = CV_COLOR_BLACK;

		// 绘制障碍物边界
		const int thickness = 2;
		cv::line(view, tl, tr, color, thickness); // 上边
		cv::line(view, tr, br, color, thickness); // 右边
		cv::line(view, br, bl, color, thickness); // 下边
		cv::line(view, bl, tl, color, thickness); // 左边

		// 绘制对角线增强可视性
		cv::line(view, tl, br, color, thickness / 2);
		cv::line(view, tr, bl, color, thickness / 2);
	}
}

std::vector<ObstacleXOSC> parseXoscFile(const std::string& filename) {
    using namespace tinyxml2;

    std::vector<ObstacleXOSC> obstacles;
    std::unordered_map<std::string, ObstacleXOSC> obstacleMap;  // 用于临时存储实体信息

	tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != XML_SUCCESS) {
        std::cerr << "Failed to load file: " << filename << std::endl;
        return obstacles;
    }

    XMLElement* root = doc.RootElement();

    // ----------------- 1. 解析 <Entities> -------------------
    XMLElement* entities = root->FirstChildElement("Entities");
    if (entities) {
        for (XMLElement* scenarioObj = entities->FirstChildElement("ScenarioObject"); scenarioObj; scenarioObj = scenarioObj->NextSiblingElement("ScenarioObject")) {
            const char* name = scenarioObj->Attribute("name");
            if (!name) continue;

            XMLElement* misc = scenarioObj->FirstChildElement("MiscObject");
            if (!misc) continue;

            XMLElement* box = misc->FirstChildElement("BoundingBox");
            XMLElement* dims = box ? box->FirstChildElement("Dimensions") : nullptr;

            ObstacleXOSC obs{};
            obs.name = name;

            if (dims) {
                obs.height = dims->FloatAttribute("height");
                obs.length = dims->FloatAttribute("length");
                obs.width = dims->FloatAttribute("width");
            }

            XMLElement* properties = misc->FirstChildElement("Properties");
            if (properties) {
                for (XMLElement* prop = properties->FirstChildElement("Property"); prop; prop = prop->NextSiblingElement("Property")) {
                    const char* propName = prop->Attribute("name");
                    const char* propVal = prop->Attribute("value");
                    if (propName && propVal) {
                        if (std::string(propName) == "category") {
                            obs.category = propVal;
                        }
                    }
                }
            }

            obstacleMap[obs.name] = obs;  // 暂存
        }
    }

    // ----------------- 2. 解析 <Storyboard> 中的 <WorldPosition> -------------------
    XMLElement* storyboard = root->FirstChildElement("Storyboard");
    if (storyboard) {
        XMLElement* init = storyboard->FirstChildElement("Init");
        if (init) {
            XMLElement* actions = init->FirstChildElement("Actions");
            if (actions) {
                for (XMLElement* priv = actions->FirstChildElement("Private"); priv; priv = priv->NextSiblingElement("Private")) {
                    const char* refName = priv->Attribute("entityRef");
                    if (!refName) continue;

                    XMLElement* teleport = priv->FirstChildElement("PrivateAction")
                        ? priv->FirstChildElement("PrivateAction")->FirstChildElement("TeleportAction")
                        : nullptr;

                    XMLElement* position = teleport ? teleport->FirstChildElement("Position") : nullptr;
                    XMLElement* worldPos = position ? position->FirstChildElement("WorldPosition") : nullptr;

                    if (worldPos) {
                        float x = worldPos->FloatAttribute("x");
                        float y = worldPos->FloatAttribute("y");
                        float z = worldPos->FloatAttribute("z");

                        auto it = obstacleMap.find(refName);
                        if (it != obstacleMap.end()) {
                            it->second.x = x;
                            it->second.y = y;
                            it->second.z = z;
                            it->second.pt = cv::Point2f(x, y);

                            // 使用固定方向计算四个角点
                            float dx = it->second.width / 2.0f;
                            float dy = it->second.length / 2.0f;
                            it->second.tl = cv::Point2f(x - dx, y + dy); // 左前
                            it->second.tr = cv::Point2f(x + dx, y + dy); // 右前
                            it->second.bl = cv::Point2f(x - dx, y - dy); // 左后
                            it->second.br = cv::Point2f(x + dx, y - dy); // 右后

                            obstacles.push_back(it->second);
                        }
                    }
                }
            }
        }
    }

    return obstacles;
}
#endif
//鼠标相关事件
#ifndef Mouse
// 鼠标回调函数
void onMouse(int event, int x, int y, int flags, void* userdata) {
	ViewParams* params = static_cast<ViewParams*>(userdata);

	switch (event) {
		// 左键按下：开始拖拽
	case cv::EVENT_LBUTTONDOWN:

		if (flags & cv::EVENT_FLAG_ALTKEY) {
			// 计算距离最近的点
			float minDist = 20; // 像素容差
			int eraseIndex = -1;
			for (size_t i = 0; i < params->clickedPoints.size(); ++i) {
				auto viewPos = enuToImage(params->clickedPoints[i], *params);
				viewPos = viewPos * params->scale + cv::Point2f(params->tx, params->ty);
				if (cv::norm(viewPos - cv::Point2f(x, y)) < minDist) {
					eraseIndex = i;
					minDist = cv::norm(viewPos - cv::Point2f(x, y));
				}
			}
			if (eraseIndex != -1) {
				saveHistoryState(params); // 先保存历史
				params->Idx.erase(params->Idx.begin() + eraseIndex);
				params->pointModes.erase(params->pointModes.begin() + eraseIndex);
				params->stopTime.erase(params->stopTime.begin() + eraseIndex);
				params->clickedPoints.erase(params->clickedPoints.begin() + eraseIndex);
				params->routeSegments.erase(
					std::remove_if(params->routeSegments.begin(),
						params->routeSegments.end(),
						[eraseIndex](const RouteSegment& s) {
					return s.startIndex == eraseIndex ||
						s.endIndex == eraseIndex; 
				}),
					params->routeSegments.end());
				for (auto& segment : params->routeSegments) {
					if (segment.startIndex > eraseIndex) segment.startIndex--;
					if (segment.endIndex > eraseIndex) segment.endIndex--;
				}
				// 重建相邻轨迹段
				const int prevIdx = eraseIndex - 1;
				const int nextIdx = eraseIndex;
				// 重建前段（prev <-> next）
				if (prevIdx >= 0 && nextIdx < params->clickedPoints.size()) {
					generateRouteSegment(params, prevIdx, nextIdx);
				}
				// 重建后段（next <-> next+1）
				if (nextIdx < params->clickedPoints.size() - 1) {
					generateRouteSegment(params, nextIdx, nextIdx + 1);
				}
			}
		}
		else
		{
			params->isDragging = true;
			params->dragStartX = x;
			params->dragStartY = y;
			params->dragStartTx = params->tx;
			params->dragStartTy = params->ty;
		}
		break;

		// 左键释放：结束拖拽
	case cv::EVENT_LBUTTONUP:
		params->isDragging = false;
		break;

		// 鼠标移动：处理拖拽
	case cv::EVENT_MOUSEMOVE:
		if (params->isDragging) {
			int dx = x - params->dragStartX;
			int dy = y - params->dragStartY;
			params->tx = params->dragStartTx + dx;
			params->ty = params->dragStartTy + dy;
		}
		break;

		// 右键点击：添加点
	case cv::EVENT_RBUTTONDOWN: {
		// 转换到高分辨率图像坐标
		saveHistoryState(params);
		cv::Point2f viewPoint(x, y);
		cv::Point2f highResPoint(
			(viewPoint.x - params->tx) / params->scale,
			(viewPoint.y - params->ty) / params->scale
		);

		// 转换到ENU坐标系
		cv::Point2f enuPoint = imageToEnu(highResPoint, *params);
		int newIndex = -1;  // 用于存储新添加点的索引
		if (params->insertIndex >= 0 &&
			params->insertIndex <= params->clickedPoints.size())
		{
			newIndex = params->insertIndex; 
#define PARAMSINSERT(key,value) params->key.insert(params->key.begin() + params->insertIndex,	value)
			PARAMSINSERT(clickedPoints, enuPoint); 
			PARAMSINSERT(Idx, params->insertIndex);
			PARAMSINSERT(pointModes,-1);
			PARAMSINSERT(stopTime,-1);
			PARAMSINSERT(speed,-1);
			PARAMSINSERT(steerKp, -1);
			params->insertIndex = -1;
		}
		else {
			params->clickedPoints.push_back(enuPoint);
			params->pointModes.push_back(-1);
			params->stopTime.push_back(-1);
			params->speed.push_back(-1);
			params->steerKp.push_back(-1);
			newIndex = params->clickedPoints.size() - 1;
			params->Idx.push_back(newIndex);
		}
		if (params->clickedPoints.size() > params->maxVisiblePoints) {
			params->scrollOffset = params->clickedPoints.size() - params->maxVisiblePoints;
		}
		if (newIndex >= 0) {
			for (auto& segment : params->routeSegments) {
				if (segment.startIndex >= newIndex) {
					segment.startIndex++;
				}
				if (segment.endIndex >= newIndex) {
					segment.endIndex++;
				}
			}
		}
		// 与前一个点生成轨迹
		if (newIndex > 0) {
			params->routeSegments.erase(
				std::remove_if(params->routeSegments.begin(),
					params->routeSegments.end(),
					[newIndex](const RouteSegment& s) {
				return (s.startIndex == newIndex - 1 &&
					s.endIndex == newIndex+1) ||
					(s.startIndex == newIndex+1 &&
						s.endIndex == newIndex -1);
			}),
				params->routeSegments.end());
			if ((params->pointModes[newIndex]) == 0) generateInterpolatedRouteSegment(params, newIndex - 1, newIndex);
			else  generateRouteSegment(params, newIndex - 1, newIndex);
		}

		// 与后一个点生成轨迹
		if (newIndex < params->clickedPoints.size() - 1) {
		
			if ((params->pointModes[newIndex+1]) == 0) generateInterpolatedRouteSegment(params, newIndex, newIndex + 1);
			else  generateRouteSegment(params, newIndex, newIndex + 1);
		}
		break;
	}
							  // 鼠标滚轮：缩放
	case cv::EVENT_MOUSEWHEEL: {

		// 在鼠标回调的滚轮事件中添加：
		if (flags & cv::EVENT_FLAG_CTRLKEY)
		{ // 按住Ctrl滚动
			int delta = cv::getMouseWheelDelta(flags);
			params->scrollOffset = clamp(
				params->scrollOffset - delta,
				0,
				std::max(0, (int)params->clickedPoints.size() - params->maxVisiblePoints)); // 最大偏移);
		}
		else {
			int delta = cv::getMouseWheelDelta(flags);
			double scaleFactor = 1.1;

			// 保存旧参数并计算新比例
			double oldScale = params->scale;
			if (delta > 0) params->scale *= scaleFactor;
			else params->scale /= scaleFactor;
			params->scale = clamp(params->scale, 0.1, 50.0);

			// 计算缩放中心点
			double xHigh = (x - params->tx) / oldScale;
			double yHigh = (y - params->ty) / oldScale;

			// 调整平移量保持缩放中心
			params->tx = x - xHigh * params->scale;
			params->ty = y - yHigh * params->scale;
		}
		break;
	}
		break;
	}
}
#endif
//键盘指令相关
#ifndef KeyBoard
// 导入点的函数
void importPoints(ViewParams& params, const std::string& caseName, const std::string& basePath) {
	saveHistoryState(&params);
	std::string filePath = basePath + "/" + caseName + ".stg";
	std::ifstream file(filePath);

	try {
		nlohmann::json jsonData;
		file >> jsonData; // 解析整个JSON文件

		auto& strategyArray = jsonData["strategy"];
		if (!strategyArray.is_array()) {
			throw std::runtime_error("Invalid strategy file format");
		}

		std::vector<cv::Point2f> importedPoints;
		std::vector<int> importedModes, importedStopTime, importedIds;
		std::vector<float> importedSpeed, importedSteerKp;

		for (const auto& point : strategyArray) {
			int id = point.size() > 0 ? point[0].get<int>() : -1; 
			float x = point.size() > 1 ? point[1].get<float>() : -1.0f;  
			float y = point.size() > 2 ? point[2].get<float>() : -1.0f;
			int mode = point.size() > 3 ? point[3].get<int>() : -1;      
			int stopTime = point.size() > 4 ? point[4].get<int>() : -1; 
			float speed = point.size() > 5 ? point[5].get<float>() : -1.0f; 
			float steerKp = point.size() > 6 ? point[6].get<float>() : -1.0f; 

			importedIds.push_back(id);  
			importedPoints.emplace_back(x, y);
			importedModes.push_back(mode);
			importedStopTime.push_back(stopTime);
			importedSpeed.push_back(speed);
			importedSteerKp.push_back(steerKp);
		}

		// 插入数据
		const size_t baseIndex = params.clickedPoints.size();
		params.Idx.insert(params.Idx.end(),
			importedIds.begin(), importedIds.end());
		params.clickedPoints.insert(params.clickedPoints.end(),
			importedPoints.begin(), importedPoints.end());
		params.pointModes.insert(params.pointModes.end(),
			importedModes.begin(), importedModes.end());
		params.stopTime.insert(params.stopTime.end(),
			importedStopTime.begin(), importedStopTime.end());
		params.speed.insert(params.speed.end(),
			importedSpeed.begin(), importedSpeed.end());
		params.steerKp.insert(params.steerKp.end(),
			importedSteerKp.begin(), importedSteerKp.end());

		// 生成轨迹
		for (size_t i = baseIndex + 1; i < params.clickedPoints.size(); ++i) {
			if (params.pointModes[i] == 0) {
				generateInterpolatedRouteSegment(&params, i - 1, i);
			}
			else {
				generateRouteSegment(&params, i - 1, i);
			}
		}

		saveHistoryState(&params);
	}
	catch (const std::exception& e) {
		std::cerr << "导入失败: " << e.what() << std::endl;
		// 回滚历史
		if (!params.undoStack.empty()) params.undoStack.pop();
	}
}
void exportPoints(const ViewParams& params, const std::string& caseName, const std::string& basePath)
{
	namespace fs = std::filesystem;

	try {
		fs::path outputDir = fs::path(basePath);
		fs::path outputFile = outputDir / (caseName + ".stg");
		if (!fs::exists(outputDir)) {
			fs::create_directories(outputDir);
		}
		std::ofstream file(outputFile);
		if (!file) {
			std::cerr << "无法打开文件: " << outputFile << std::endl;
			return;
		}
		// 写入 JSON 头部
		file << "{\n  \"strategy\": [\n";
		// 遍历点并写入，每个点一行
		for (size_t i = 0; i < params.clickedPoints.size(); ++i) {
			file << "    ["
				<<params.Idx[i]<<", "
				<< params.clickedPoints[i].x << ", "
				<< params.clickedPoints[i].y << ", "
				<< params.pointModes[i] << ", "
				<< params.stopTime[i] << ", "
				<< params.speed[i] << ", "
				<< params.steerKp[i] << "]";
			if (i + 1 != params.clickedPoints.size()) {
				file << ",";
			}
			file << "\n";
		}
		// 写入 JSON 尾部
		file << "  ]\n}\n";

		std::cout << "成功导出案例 [" << caseName << "] 到: " << outputFile << std::endl;
		// 写入后刷新并关闭文件
		file.flush();
		file.close();
		// 等待 1000ms
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		std::string command = "notepad \"" + outputFile.string() + "\"";
		std::system(command.c_str());

	}
	catch (const std::exception& e) {
		std::cerr << "导出异常: " << e.what() << std::endl;
	}
}



void performUndo(ViewParams* params) {
	if (!params->undoStack.empty()) {
		// 恢复历史状态
		const auto& prev = params->undoStack.top();
		params->clickedPoints = prev.clickedPoints;
		params->routeSegments = prev.routeSegments;
		params->pointModes = prev.pointModes;
		params->stopTime = prev.stopTime;
		params->scrollOffset = prev.scrollOffset;
		params->speed = prev.speed,
		params->steerKp = prev.steerKp,
		params->Idx = prev.Idx,
		params->undoStack.pop();
	}
}
void KeyBoardOrder(ViewParams& params,int&caseIdx)
{
	int key = cv::waitKey(30);
	static std::string str = std::to_string(caseIdx);
	if (!params.waitingForInput) {
		switch (key) {
		case 'i':
		case 'I':
			if (!params.waitingForInput)
			{
				params.waitingForInput = true;
				params.inputBuffer.clear();
				params.inputType =ViewParams::InputType::INPUT_INSERT_INDEX;
			} break;
		case 'm': // 切换轨迹生成策略为插值
		case 'M':
			if (!params.waitingForInput)
			{
				params.waitingForInput = true;
				params.inputBuffer.clear();
				params.inputType = ViewParams::InputType::INPUT_MODIFY_MODE;
			}
			break;
		case 't': // 切换轨迹生成策略为插值
		case 'T':
			if (!params.waitingForInput)
			{
				params.waitingForInput = true;
				params.stopTimeIndex = -1;
				params.inputBuffer.clear();
				params.inputType = ViewParams::InputType::INPUT_STOPTIME_INDEX;
			}
			break;
		case 'l': 
		case 'L':
			importPoints(params, str, "../../TrajectoryControl/m_strategy/");
			 break;
		case 's': 
		case 'S': 
			exportPoints(params, str, "../../TrajectoryControl/m_strategy/");
			break;
		//case 27: return 0; // ESC退出
		default:
			if ((key & 0xFF) == 26) { // ASCII 26对应Ctrl+Z
				performUndo(&params);
			}
			break;
		}
	}
	else {
		// 处理文本输入
		if (key >= '0' && key <= '9' && params.inputBuffer.length() < 3) {
			params.inputBuffer += static_cast<char>(key);
		}
		else if (key == 8 && !params.inputBuffer.empty()) { // Backspace
			params.inputBuffer.pop_back();
		}
		else if (key == 13) { // Enter
			try {
				// 针对不同模式做处理
				if (params.inputType == ViewParams::InputType::INPUT_STOPTIME_INDEX) {
					int idx = std::stoi(params.inputBuffer) - 1;				// 将输入转为点的索引（注意序号从1开始）
					if (idx < 0 || idx >= params.clickedPoints.size()) {
						std::cerr << "无效的点序号" << std::endl;
					}
					else {
						params.stopTimeIndex = idx;
						// 清空输入并切换到下一个阶段，等待停止时间值输入
						params.inputBuffer.clear();
						params.inputType = ViewParams::InputType::INPUT_STOPTIME_VALUE;
					}
				}
				else if (params.inputType == ViewParams::InputType::INPUT_STOPTIME_VALUE) {
					saveHistoryState(&params);
					int newStopTime = std::stoi(params.inputBuffer);		// 解析新的停止时间
					if (params.stopTimeIndex >= 0 && params.stopTimeIndex < params.clickedPoints.size()) {
						params.stopTime[params.stopTimeIndex] = newStopTime;
					}
					// 重置所有输入状态
					params.stopTimeIndex = -1;
					params.insertIndex = -1;
					params.inputBuffer.clear();
					params.inputType = ViewParams::InputType::INPUT_NONE;
					params.waitingForInput = false;
				}
				else if ((params.inputType == ViewParams::InputType::INPUT_MODIFY_MODE)
					|| (params.inputType == ViewParams::InputType::INPUT_INSERT_INDEX)) 
				{
						params.insertIndex = std::stoi(params.inputBuffer) - 1;
						if (params.insertIndex < 0 || params.insertIndex > params.clickedPoints.size())
						{
							params.insertIndex = -1;
						}
						if (params.inputType == ViewParams::InputType::INPUT_MODIFY_MODE &&
							params.insertIndex != -1) {
							int idx = params.insertIndex;
							params.pointModes[idx] = 0;
							int prevIdx = idx - 1;
							auto& segments = params.routeSegments;
							segments.erase(
								std::remove_if(segments.begin(), segments.end(),
									[prevIdx, idx](const RouteSegment& s) {
								return (s.startIndex == prevIdx && s.endIndex == idx) ||
									(s.startIndex == idx && s.endIndex == prevIdx);
							}),
								segments.end()
							);
							generateInterpolatedRouteSegment(&params, prevIdx, idx);
							params.insertIndex = -1;
						}
						// 处理完后重置状态
						params.inputBuffer.clear();
						params.inputType = ViewParams::InputType::INPUT_NONE;
						params.waitingForInput = false;
				}
			}
			catch (...) {
				params.insertIndex = -1;
				params.inputBuffer.clear();
				params.inputType = ViewParams::InputType::INPUT_NONE;
				params.waitingForInput = false;
			}
		}
		else if (key == 27) { // ESC取消
			params.waitingForInput = false;
			params.inputBuffer.clear();
		}
	}
}
#endif