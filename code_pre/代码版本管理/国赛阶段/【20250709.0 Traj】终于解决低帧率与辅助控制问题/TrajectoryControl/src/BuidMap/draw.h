/********************************************************************************************************
 * @file        draw.h
 * @category    utility
 * @brief       绘制函数接口
 * @note		仿照 MATLAB 接口进行移植
 * @copyright   Copyright (c) 2024, HIT Intelligent Vehicle Lab COMPLETE MODEL. All rights reserved.
 ********************************************************************************************************/
#pragma once
#include <opencv2/opencv.hpp>

// OpenCV BGR像素颜色
#define CV_COLOR_RED		    cv::Scalar(0, 0, 255)	        // 纯红
#define CV_COLOR_GREEN		    cv::Scalar(0, 255, 0)		    // 纯绿
#define CV_COLOR_BLUE		    cv::Scalar(255, 0, 0)	        // 纯蓝
#define CV_COLOR_CYAN		    cv::Scalar(255, 255, 0)		    // 湖蓝
#define CV_COLOR_YELLOW		    cv::Scalar(0, 255, 255)		    // 紫色
#define CV_COLOR_MAGENTA	    cv::Scalar(255, 0, 255)		    // 品红色
#define CV_COLOR_DARKGRAY	    cv::Scalar(169, 169, 169)	    // 深灰色
#define CV_COLOR_DARKRED	    cv::Scalar(0, 0, 139)		    // 深红色
#define CV_COLOR_ORANGERED	    cv::Scalar(0, 69, 255)		    // 橙红色
#define CV_COLOR_CHOCOLATE	    cv::Scalar(30, 105, 210)	    // 巧克力
#define CV_COLOR_GOLD		    cv::Scalar(10, 215, 255)	    // 金色
#define CV_COLOR_OLIVE		    cv::Scalar(0, 128, 128)		    // 橄榄色
#define CV_COLOR_LIGHTGREEN     cv::Scalar(144, 238, 144)	    // 浅绿色
#define CV_COLOR_DARKCYAN	    cv::Scalar(139, 139, 0)		    // 深青色
#define CV_COLOR_SKYBLUE	    cv::Scalar(230, 216, 173)	    // 天蓝色
#define CV_COLOR_INDIGO		    cv::Scalar(130, 0, 75)		    // 藏青色
#define CV_COLOR_PURPLE		    cv::Scalar(128, 0, 128)		    // 紫色
#define CV_COLOR_PINK		    cv::Scalar(203, 192, 255)	    // 粉色
#define CV_COLOR_DEEPPINK	    cv::Scalar(147, 20, 255)	    // 深粉色
#define CV_COLOR_VIOLET		    cv::Scalar(238, 130, 238)	    // 紫罗兰
#define CV_COLOR_BLACK		    cv::Scalar(0, 0, 0)			    // 黑色
#define CV_COLOR_WHITE		    cv::Scalar(255, 255, 255)	    // 白色

// 控制台字体ANSI转义字符
// reference 1: https://blog.csdn.net/Mculover666/article/details/105433609 
// reference 2: https://blog.csdn.net/stewie6/article/details/138463078
#define CONSOLE_COLOR_RESET         "\x1b[0m"               // 重设（白色）
#define CONSOLE_COLOR_WHITE	        "\x1b[0m"		        // 白色
#define CONSOLE_COLOR_RED           "\x1b[31m"		        // 纯红
#define CONSOLE_COLOR_MAGENTA	    "\x1b[35m"			    // 品红
#define CONSOLE_COLOR_GREEN         "\x1b[32m"			    // 纯绿
#define CONSOLE_COLOR_BLUE          "\x1b[34m"			    // 纯蓝
#define CONSOLE_COLOR_DARKGRAY      "\x1b[90m"			    // 深灰色
#define CONSOLE_COLOR_DARKRED       "\x1b[38;5;52m"		    // 深红色
#define CONSOLE_COLOR_ORANGERED	    "\x1b[38;5;208m"	    // 橙红色
#define CONSOLE_COLOR_CHOCOLATE     "\x1b[38;5;94m"		    // 巧克力
#define CONSOLE_COLOR_GOLD          "\x1b[38;5;220m"	    // 金色
#define CONSOLE_COLOR_YELLOW        "\x1b[33m"			    // 纯黄色
#define CONSOLE_COLOR_OLIVE         "\x1b[38;5;58m"		    // 橄榄色
#define CONSOLE_COLOR_LIGHTGREEN    "\x1b[38;5;119m"	    // 浅绿色
#define CONSOLE_COLOR_DARKCYAN      "\x1b[36m"			    // 深青色
#define CONSOLE_COLOR_SKYBLUE       "\x1b[38;5;223m"	    // 天蓝色
#define CONSOLE_COLOR_INDIGO        "\x1b[38;5;54m"		    // 藏青色
#define CONSOLE_COLOR_PURPLE        "\x1b[35m"			    // 紫色
#define CONSOLE_COLOR_PINK          "\x1b[38;5;225m"	    // 粉色
#define CONSOLE_COLOR_DEEPPINK      "\x1b[38;5;90m"		    // 深粉色
#define CONSOLE_COLOR_VIOLET	    "\x1b[38;5;177m"	    // 紫罗兰
#define CONSOLE_COLOR_BLACK         "\x1b[30m"			    // 黑色

#define CONSOLE_BRIGHTER            "\x1b[1m"       // 变亮
#define CONSOLE_DIMMER              "\x1b[2m"       // 变暗
#define CONSOLE_ITALIC              "\x1b[3m"       // 斜体
#define CONSOLE_UNDERLINE           "\x1b[4m"       // 下划线
#define CONSOLE_DOUBLE_UNDERLINE    "\x1b[21m"      // 双下划线
#define CONSOLE_OVERLINE            "\x1b[53m"      // 上划线
#define CONSOLE_SLOW_FLICKER        "\x1b[5m"       // 慢速闪烁
#define CONSOLE_FAST_FLICKER        "\x1b[6m"       // 快速闪烁
#define CONSOLE_COLOR_INVERSE       "\x1b[7m"       // 背景前景反色
#define CONSOLE_HIDE                "\x1b[8m"       // 隐藏（最好别用
#define CONSOLE_DELETELINE          "\x1b[9m"       // 删除线

enum DRAW_METHOD
{
	DRAW_METHOD_CIRCLE,
	DRAW_METHOD_PIXEL
};

/**
 * @brief 更改控制台下一次输出的字体颜色
 * @param color	颜色
 */
#define set_console_color(color) printf(color)

/**
 * @brief 更改控制台下一次输出的字体rgb颜色
 * @param r	红色0~255
 * @param g	绿色0~255
 * @param b	蓝色0~255
 */
inline void set_console_rgb_color(const int& r, const int& g, const int& b)
{
    std::stringstream ss; 
    ss << "\x1b[38;2;" << r << ";" << g << ";" << b << "m";
    std::cout << ss.str() << std::endl;
}

 /**
  * @brief 在图片中绘制点集
  * @param src		绘制点集的图片
  * @param points	要绘制的点集
  * @param color	绘制颜色
  * @param method	使用何种方法绘制
  */
void drawPoints(cv::Mat& src, const std::vector<cv::Point2i>& points, cv::Scalar color, enum DRAW_METHOD method = DRAW_METHOD_CIRCLE);
void drawPoints(cv::Mat& src, const std::vector<cv::Point2f>& points, cv::Scalar color, enum DRAW_METHOD method = DRAW_METHOD_CIRCLE);
void drawPoints(cv::Mat& src, const cv::Point2i& point, cv::Scalar color);
void draw3x3Points(cv::Mat& src, const cv::Point2i& point, cv::Scalar color);

/**
 * @brief 创建绘制窗口
 * @param window_name	窗口名称
 * @return 				窗口句柄
 */
std::string figure(const std::string window_name);

/**
 * @brief 绘制曲线
 * @param handle    要绘制曲线的窗口句柄
 * @param image		要绘制曲线的图像
 * @param data      曲线数据
 * @param color     曲线颜色
 * @param thickness 曲线粗细
 */
void plot(const std::string handle, const std::vector<cv::Point2i>& data, const cv::Scalar color, int thickness = 1);
void plot(const std::string handle, const std::vector<cv::Point2f>& data, const cv::Scalar color, int thickness = 1);
void plot(cv::Mat& image, const std::vector<cv::Point2i>& data, const cv::Scalar color, int thickness = 1);
void plot(cv::Mat& image, const std::vector<cv::Point2f>& data, const cv::Scalar color, int thickness = 1);

/**
 * @brief 绘制散点图
 * @param handle    要绘制曲线的窗口句柄
 * @param image		要绘制曲线的图像
 * @param data      散点数据
 * @param color     散点颜色
 */
void scatter(const std::string handle, const std::vector<cv::Point2i>& data, const cv::Scalar color);
void scatter(const std::string handle, const std::vector<cv::Point2f>& data, const cv::Scalar color);
void scatter(cv::Mat& image, const std::vector<cv::Point2i>& data, const cv::Scalar color);
void scatter(cv::Mat& image, const std::vector<cv::Point2f>& data, const cv::Scalar color);

/**
 * @brief 绘制多个图像
 * @param handle    要绘制 subplot 的窗口句柄
 * @param image		要绘制 subplot 的图像
 * @param img1      左上角的图片
 * @param img2		右上角的图片
 * @param img3      左下角的图片
 * @param img4		右下角的图片
 */
void subplot(cv::Mat& result, cv::Mat img1, cv::Mat img2 = cv::Mat(), cv::Mat img3 = cv::Mat(), cv::Mat img4 = cv::Mat());
void subplot(const std::string handle, cv::Mat img1, cv::Mat img2 = cv::Mat(), cv::Mat img3 = cv::Mat(), cv::Mat img4 = cv::Mat());