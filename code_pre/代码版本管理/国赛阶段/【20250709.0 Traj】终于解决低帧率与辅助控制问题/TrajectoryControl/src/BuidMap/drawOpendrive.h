#include "define.h"
#include <fstream>
#include <stack>
struct ObstacleXOSC {
	float x, y, z;
	float length, width, height;
	cv::Point2f pt;
	cv::Point2f tl, tr, bl, br;
	std::string name;
	std::string category;
};

struct RouteSegment {
	int startIndex;  // 在clickedPoints中的起点索引
	int endIndex;    // 在clickedPoints中的终点索引
	std::vector<cv::Point2f> routePoints; // 生成的轨迹点
};

struct ViewParams {
	// 视图变换参数
	double scale;
	double tx, ty;

	// 鼠标状态
	bool isDragging;
	int dragStartX, dragStartY;
	double dragStartTx, dragStartTy;

	// 用户点击点
	std::vector<cv::Point2f> clickedPoints;

	// 高分辨率参数
	cv::Mat canvasHigh;
	float minX, maxX, minY, maxY;
	int scaleFactor;
	cv::Size windowSize;

	// 橡皮擦相关参数
	bool eraserActive;
	cv::Point lastEraserPos;

	// 插入点相关的参数
	bool waitingForInput = false;
	std::string inputBuffer;
	int insertIndex = -1;
	std::vector<int> Idx;
	std::vector<int> pointModes;
	std::vector<float> speed;
	std::vector<float>steerKp;
	//停止时间相关
	int stopTimeIndex;
	std::vector<int>stopTime;
	enum InputType { INPUT_NONE,
		INPUT_MODIFY_MODE,
		INPUT_INSERT_INDEX, 
		INPUT_STOPTIME_INDEX,  
		INPUT_STOPTIME_VALUE  
	};
	int inputType;
	// 信息框相关参数
	int scrollOffset = 0;				// 滚动偏移量
	const int maxVisiblePoints = 25;	// 可见点数
	const int panelWidth = 300;			// 信息面板宽度

	// 轨迹相关参数
	std::vector<RouteSegment> routeSegments;
	cv::Scalar routeColor = cv::Scalar(255, 0, 255); // 轨迹颜色（洋红色

	//撤销相关
	struct HistoryState {
		std::vector<int> Idx;
		std::vector<cv::Point2f> clickedPoints;
		std::vector<RouteSegment> routeSegments;
		std::vector<int> pointModes; // 新增
		std::vector<int>stopTime;
		std::vector<float> speed;
		std::vector<float>steerKp;
		int scrollOffset;
	};
	std::stack<HistoryState> undoStack;
	const int MAX_HISTORY = 100; // 最大历史记录数
};

void ViewParamsInit(std::vector<cv::Point2f>& allBoundaries, std::vector<cv::Point2f>& allJungles, std::vector<cv::Point2f>& TargetPath, ViewParams& params);
cv::Point2f enuToImage(const cv::Point2f& enuPoint, const ViewParams& params);
cv::Point2f imageToEnu(const cv::Point2f& imagePoint, const ViewParams& params);
void ImageWarpAffine(ViewParams& params, cv::Mat& view);
void generateRouteSegment(ViewParams* params, int startIdx, int endIdx);
void extractLaneBoundaries(const SSD::SimVector<HDMapStandalone::MLaneInfo>& lanes, std::vector<cv::Point2f>& boundaries, std::vector<cv::Point2f>& jungles);
void extractTargetPath(SSD::SimPoint3DVector& targetpath, std::vector<cv::Point2f>& TargetPath);
void onMouse(int event, int x, int y, int flags, void* userdata);
void drawObstacle(std::vector<ObstacleXOSC>& obstaclelist, ViewParams& params, cv::Mat& view);
void drawInfoBoard(ViewParams& params, cv::Mat& infoPanel);
void drawPoints(ViewParams& params, cv::Mat& view);
void drawInsertBoard(ViewParams& params, cv::Mat& infoPanel);
void drawPath(ViewParams& params, cv::Mat& view);
std::vector<ObstacleXOSC> parseXoscFile(const std::string& filename);
void KeyBoardOrder(ViewParams& params, int& caseIdx);