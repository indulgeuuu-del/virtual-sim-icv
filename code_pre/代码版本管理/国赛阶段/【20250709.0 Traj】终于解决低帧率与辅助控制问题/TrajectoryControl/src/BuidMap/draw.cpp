/********************************************************************************************************
 * @file        draw.cpp
 * @category    utility
 * @brief       绘制函数实现
 * @note		仿照 MATLAB 接口进行移植
 * @copyright   Copyright (c) 2024, HIT Intelligent Vehicle Lab COMPLETE MODEL. All rights reserved.
 ********************************************************************************************************/
#include "draw.h"

// 重载 1 2dim Point2i：在图片中绘制点集
void drawPoints(cv::Mat& src, const std::vector<cv::Point2i>& points, cv::Scalar color, enum DRAW_METHOD method)
{
	if (CV_8UC1 == src.type())
	{
		cvtColor(src, src, cv::COLOR_GRAY2BGR);
	}

	if (method == DRAW_METHOD_CIRCLE)
	{
		// 循环遍历所有点，并在图像上绘制圆
		for (const auto& point : points)
		{
			cv::circle(src, point, 1, color, -1); // -1 表示填充圆
		}
	}
	else if (method == DRAW_METHOD_PIXEL)
	{
		// 获取图像的行数和列数
		int rows = src.rows;
		int cols = src.cols;

		// 循环遍历所有点，并在图像上绘制圆
		for (const auto& point : points)
		{
			// 检查点是否在图像范围内
			if (point.x >= 0 && point.x < cols && point.y >= 0 && point.y < rows)
			{
				// 获取指向该点像素的指针
				cv::Vec3b* pixel = src.ptr<cv::Vec3b>(point.y, point.x);

				// 修改像素值
				(*pixel)[0] = color[0]; // Blue
				(*pixel)[1] = color[1]; // Green
				(*pixel)[2] = color[2]; // Red
			}
		}
	}
}

// 重载 2 2dim Point2f：在图片中绘制点集
void drawPoints(cv::Mat& src, const std::vector<cv::Point2f>& points, cv::Scalar color, enum DRAW_METHOD method)
{
	if (CV_8UC1 == src.type())
	{
		cvtColor(src, src, cv::COLOR_GRAY2BGR);
	}

	if (method == DRAW_METHOD_CIRCLE)
	{
		// 循环遍历所有点，并在图像上绘制圆
		for (const auto& point : points)
		{
			cv::circle(src, point, 1, color, -1); // -1 表示填充圆
		}
	}
	else if (method == DRAW_METHOD_PIXEL)
	{
		// 获取图像的行数和列数
		int rows = src.rows;
		int cols = src.cols;

		// 循环遍历所有点，并在图像上绘制圆
		for (const auto& point : points)
		{
			// 检查点是否在图像范围内
			if (point.x >= 0 && point.x < cols && point.y >= 0 && point.y < rows)
			{
				// 获取指向该点像素的指针
				cv::Vec3b* pixel = src.ptr<cv::Vec3b>(static_cast<int>(point.y), static_cast<int>(point.x));

				// 修改像素值
				(*pixel)[0] = color[0]; // Blue
				(*pixel)[1] = color[1]; // Green
				(*pixel)[2] = color[2]; // Red
			}
		}
	}
}

// 重载 3 1dim Point2i：在图片中绘制点集
void drawPoints(cv::Mat& src, const cv::Point2i& point, cv::Scalar color)
{
    // 获取图像的行数和列数
    int rows = src.rows;
    int cols = src.cols;

    // 检查点是否在图像范围内
    if (point.x >= 0 && point.x < cols && point.y >= 0 && point.y < rows)
    {
        // 获取指向该点像素的指针
        cv::Vec3b* pixel = src.ptr<cv::Vec3b>(static_cast<int>(point.y), static_cast<int>(point.x));

        // 修改像素值
        (*pixel)[0] = color[0]; // Blue
        (*pixel)[1] = color[1]; // Green
        (*pixel)[2] = color[2]; // Red
    }
}

void draw3x3Points(cv::Mat& src, const cv::Point2i& point, cv::Scalar color)
{
    // 获取图像的行数和列数
    int rows = src.rows;
    int cols = src.cols;

    for(int yy = point.y - 1; yy < point.y + 2; ++yy)
    {
        for(int xx = point.x - 1; xx < point.x + 2; ++xx)
        {
            // 检查点是否在图像范围内
            if (xx >= 0 && xx < cols && yy >= 0 && yy < rows)
            {
                // 获取指向该点像素的指针
                cv::Vec3b* pixel = src.ptr<cv::Vec3b>(static_cast<int>(yy), static_cast<int>(xx));

                // 修改像素值
                (*pixel)[0] = color[0]; // Blue
                (*pixel)[1] = color[1]; // Green
                (*pixel)[2] = color[2]; // Red
            }
        }
    }
}

// 创建绘制窗口
std::string figure(const std::string window_name)
{
	cv::namedWindow(window_name, cv::WINDOW_FREERATIO);
	return window_name;
}

// 重载 1 Point2i：在含有句柄的窗口中绘制曲线
void plot(const std::string handle, const std::vector<cv::Point2i>& data, const cv::Scalar color, int thickness)
{
    // 创建点集的副本
    int size = data.size();
    std::vector<cv::Point2i> points = std::vector<cv::Point2i>(data);

    // 初始化最大横坐标和纵坐标为第一个点的坐标
    int max_x = data[0].x;
    int max_y = data[0].y;

    // 遍历所有点，更新最大坐标值
    for (const auto& point : data)
    {
        if (point.x > max_x)
        {
            max_x = point.x;
        }
        if (point.y > max_y)
        {
            max_y = point.y;
        }
    }

    // 倒转整个 y 轴
    for (int i = 0; i < size; ++i)
    {
        points[i].y = max_y - points[i].y;
    }

    // 根据输入的数据调整窗口和图像的大小
    cv::Mat image = cv::Mat(max_y, max_x, CV_8UC3, cv::Scalar(200, 200, 200));

    // 绘制曲线
    for (int i = 0; i < size - 1; ++i)
    {
        cv::line(image, points[i], points[i + 1], color, thickness);
    }

    // 显示曲线
    cv::imshow(handle, image);
}

// 重载 2 Point2f：在含有句柄的窗口中绘制曲线
void plot(const std::string handle, const std::vector<cv::Point2f>& data, const cv::Scalar color, int thickness)
{
    // 创建点集的副本
    int size = data.size();
    std::vector<cv::Point2f> points = std::vector<cv::Point2f>(data);

    // 初始化最大横坐标和纵坐标为第一个点的坐标
    int max_x = data[0].x;
    int max_y = data[0].y;

    // 遍历所有点，更新最大坐标值
    for (const auto& point : data)
    {
        if (point.x > max_x)
        {
            max_x = (int)point.x;
        }
        if (point.y > max_y)
        {
            max_y = (int)point.y;
        }
    }

    // 倒转整个 y 轴
    for (int i = 0; i < size; ++i)
    {
        points[i].y = max_y - points[i].y;
    }

    // 根据输入的数据调整窗口和图像的大小
    cv::Mat image = cv::Mat(max_y, max_x, CV_8UC3, cv::Scalar(200, 200, 200));

    // 绘制曲线
	for (int i = 0; i < size - 1; ++i)
	{
		cv::line(image, points[i], points[i + 1], color, thickness);
	}

    // 显示曲线
    cv::imshow(handle, image);
}

// 重载 3 Point2i：在图片中绘制曲线
void plot(cv::Mat& image, const std::vector<cv::Point2i>& data, const cv::Scalar color, int thickness)
{
    // 创建点集的副本
    int size = data.size();
    std::vector<cv::Point2i> points = std::vector<cv::Point2i>(data);

    // 初始化最大横坐标和纵坐标为第一个点的坐标
    int max_x = data[0].x;
    int max_y = data[0].y;

    // 遍历所有点，更新最大坐标值
    for (const auto& point : data)
    {
        if (point.x > max_x)
        {
            max_x = point.x;
        }
        if (point.y > max_y)
        {
            max_y = point.y;
        }
    }

    // 倒转整个 y 轴
    for (int i = 0; i < size; ++i)
    {
        points[i].y = max_y - points[i].y;
    }

    // 根据输入的数据调整窗口和图像的大小
    image = cv::Mat(max_y, max_x, CV_8UC3, cv::Scalar(200, 200, 200));

    // 绘制曲线
    for (int i = 0; i < size - 1; ++i)
    {
        cv::line(image, points[i], points[i + 1], color, thickness);
    }
}

// 重载 4 Point2f：在图片中绘制曲线
void plot(cv::Mat& image, const std::vector<cv::Point2f>& data, const cv::Scalar color, int thickness)
{
    // 创建点集的副本
    int size = data.size();
    std::vector<cv::Point2f> points = std::vector<cv::Point2f>(data);

    // 初始化最大横坐标和纵坐标为第一个点的坐标
    int max_x = data[0].x;
    int max_y = data[0].y;

    // 遍历所有点，更新最大坐标值
    for (const auto& point : data)
    {
        if (point.x > max_x)
        {
            max_x = (int)point.x;
        }
        if (point.y > max_y)
        {
            max_y = (int)point.y;
        }
    }

    // 倒转整个 y 轴
    for (int i = 0; i < size; ++i)
    {
        points[i].y = max_y - points[i].y;
    }

    // 根据输入的数据调整窗口和图像的大小
    image = cv::Mat(max_y, max_x, CV_8UC3, cv::Scalar(200, 200, 200));

    // 绘制曲线
    for (int i = 0; i < size - 1; ++i)
    {
        cv::line(image, points[i], points[i + 1], color, thickness);
    }
}

// 重载 1 Point2i：在含有句柄的窗口中绘制散点图
void scatter(const std::string handle, const std::vector<cv::Point2i>& data, const cv::Scalar color)
{
    // 创建点集的副本
    int size = data.size();
    std::vector<cv::Point2i> points = std::vector<cv::Point2i>(data);

    // 初始化最大横坐标和纵坐标为第一个点的坐标
    int max_x = data[0].x;
    int max_y = data[0].y;

    // 遍历所有点，更新最大坐标值
    for (const auto& point : data)
    {
        if (point.x > max_x)
        {
            max_x = point.x;
        }
        if (point.y > max_y)
        {
            max_y = point.y;
        }
    }

    // 倒转整个 y 轴
    for (int i = 0; i < size; ++i)
    {
        points[i].y = max_y - points[i].y;
    }

    // 根据输入的数据调整窗口和图像的大小
    cv::Mat image = cv::Mat(max_y, max_x, CV_8UC3, cv::Scalar(200, 200, 200));

    // 循环遍历所有点，并在图像上绘制圆
    for (const auto& point : points)
    {
        cv::circle(image, point, 1, color, -1); // -1 表示填充圆
    }

    // 显示曲线
    cv::imshow(handle, image);
}

// 重载 2 Point2f：在含有句柄的窗口中绘制散点图
void scatter(const std::string handle, const std::vector<cv::Point2f>& data, const cv::Scalar color)
{
    // 创建点集的副本
    int size = data.size();
    std::vector<cv::Point2f> points = std::vector<cv::Point2f>(data);

    // 初始化最大横坐标和纵坐标为第一个点的坐标
    int max_x = data[0].x;
    int max_y = data[0].y;

    // 遍历所有点，更新最大坐标值
    for (const auto& point : data)
    {
        if (point.x > max_x)
        {
            max_x = (int)point.x;
        }
        if (point.y > max_y)
        {
            max_y = (int)point.y;
        }
    }

    // 倒转整个 y 轴
    for (int i = 0; i < size; ++i)
    {
        points[i].y = max_y - points[i].y;
    }

    // 根据输入的数据调整窗口和图像的大小
    cv::Mat image = cv::Mat(max_y, max_x, CV_8UC3, cv::Scalar(200, 200, 200));

    // 循环遍历所有点，并在图像上绘制圆
    for (const auto& point : points)
    {
        cv::circle(image, point, 1, color, -1); // -1 表示填充圆
    }

    // 显示曲线
    cv::imshow(handle, image);
}

// 重载 3 Point2i：在图片中绘制散点图
void scatter(cv::Mat& image, const std::vector<cv::Point2i>& data, const cv::Scalar color)
{
    // 创建点集的副本
    int size = data.size();
    std::vector<cv::Point2i> points = std::vector<cv::Point2i>(data);

    // 初始化最大横坐标和纵坐标为第一个点的坐标
    int max_x = data[0].x;
    int max_y = data[0].y;

    // 遍历所有点，更新最大坐标值
    for (const auto& point : data)
    {
        if (point.x > max_x)
        {
            max_x = point.x;
        }
        if (point.y > max_y)
        {
            max_y = point.y;
        }
    }

    // 倒转整个 y 轴
    for (int i = 0; i < size; ++i)
    {
        points[i].y = max_y - points[i].y;
    }

    // 根据输入的数据调整窗口和图像的大小
    image = cv::Mat(max_y, max_x, CV_8UC3, cv::Scalar(200, 200, 200));

    // 循环遍历所有点，并在图像上绘制圆
    for (const auto& point : points)
    {
        cv::circle(image, point, 1, color, -1); // -1 表示填充圆
    }
}

// 重载 4 Point2f：在图片中绘制散点图
void scatter(cv::Mat& image, const std::vector<cv::Point2f>& data, const cv::Scalar color)
{
    // 创建点集的副本
    int size = data.size();
    std::vector<cv::Point2f> points = std::vector<cv::Point2f>(data);

    // 初始化最大横坐标和纵坐标为第一个点的坐标
    int max_x = data[0].x;
    int max_y = data[0].y;

    // 遍历所有点，更新最大坐标值
    for (const auto& point : data)
    {
        if (point.x > max_x)
        {
            max_x = (int)point.x;
        }
        if (point.y > max_y)
        {
            max_y = (int)point.y;
        }
    }

    // 倒转整个 y 轴
    for (int i = 0; i < size; ++i)
    {
        points[i].y = max_y - points[i].y;
    }

    // 根据输入的数据调整窗口和图像的大小
    image = cv::Mat(max_y, max_x, CV_8UC3, cv::Scalar(200, 200, 200));

    // 循环遍历所有点，并在图像上绘制圆
    for (const auto& point : points)
    {
        cv::circle(image, point, 1, color, -1); // -1 表示填充圆
    }
}

// 重载 1 输出 subplot 图像：绘制多个图像
void subplot(cv::Mat& result, cv::Mat img1, cv::Mat img2, cv::Mat img3, cv::Mat img4)
{
    // 创建足够大的图片
    result = cv::Mat::zeros(1080, 1920, CV_8UC3);

    // 缩放用户输入的图片到输出图片的四分之一大小
    cv::Mat resized_img1, resized_img2, resized_img3, resized_img4;
    if (!img1.empty())
    {
        if (img1.type() == CV_8UC1)
        {
            cv::cvtColor(img1, img1, cv::COLOR_GRAY2BGR);
        }
        cv::resize(img1, resized_img1, cv::Size(result.cols / 2, result.rows / 2));
    }
    if (!img2.empty())
    {
        if (img2.type() == CV_8UC1)
        {
            cv::cvtColor(img2, img2, cv::COLOR_GRAY2BGR);
        }
        cv::resize(img2, resized_img2, cv::Size(result.cols / 2, result.rows / 2));
    }
    if (!img3.empty())
    {
        if (img3.type() == CV_8UC1)
        {
            cv::cvtColor(img3, img3, cv::COLOR_GRAY2BGR);
        }
        cv::resize(img3, resized_img3, cv::Size(result.cols / 2, result.rows / 2));
    }
    if (!img4.empty())
    {
        if (img4.type() == CV_8UC1)
        {
            cv::cvtColor(img4, img4, cv::COLOR_GRAY2BGR);
        }
        cv::resize(img4, resized_img4, cv::Size(result.cols / 2, result.rows / 2));
    }

    // 将用户图片放置到合适的位置上去
    if (!resized_img1.empty())
    {
        cv::Rect roi1(0, 0, resized_img1.cols, resized_img1.rows);
        resized_img1.copyTo(result(roi1));
    }
    if (!resized_img2.empty())
    {
        cv::Rect roi2(resized_img1.cols, 0, resized_img2.cols, resized_img2.rows);
        resized_img2.copyTo(result(roi2));
    }
    if (!resized_img3.empty())
    {
        cv::Rect roi3(0, resized_img1.rows, resized_img3.cols, resized_img3.rows);
        resized_img3.copyTo(result(roi3));
    }
    if (!resized_img4.empty())
    {
        cv::Rect roi4(resized_img3.cols, resized_img1.rows, resized_img4.cols, resized_img4.rows);
        resized_img4.copyTo(result(roi4));
    }
}

// 重载 2 在含有句柄的窗口中直接 subplot：绘制多个图像
void subplot(const std::string handle, cv::Mat img1, cv::Mat img2, cv::Mat img3, cv::Mat img4)
{
    // 创建足够大的图片
    cv::Mat result = cv::Mat::zeros(1080, 1920, CV_8UC3);

    // 缩放用户输入的图片到输出图片的四分之一大小
    cv::Mat resized_img1, resized_img2, resized_img3, resized_img4;
    if (!img1.empty())
    {
        if (img1.type() == CV_8UC1)
        {
            cv::cvtColor(img1, img1, cv::COLOR_GRAY2BGR);
        }
        cv::resize(img1, resized_img1, cv::Size(result.cols / 2, result.rows / 2));
    }
    if (!img2.empty())
    {
        if (img2.type() == CV_8UC1)
        {
            cv::cvtColor(img2, img2, cv::COLOR_GRAY2BGR);
        }
        cv::resize(img2, resized_img2, cv::Size(result.cols / 2, result.rows / 2));
    }
    if (!img3.empty())
    {
        if (img3.type() == CV_8UC1)
        {
            cv::cvtColor(img3, img3, cv::COLOR_GRAY2BGR);
        }
        cv::resize(img3, resized_img3, cv::Size(result.cols / 2, result.rows / 2));
    }
    if (!img4.empty())
    {
        if (img4.type() == CV_8UC1)
        {
            cv::cvtColor(img4, img4, cv::COLOR_GRAY2BGR);
        }
        cv::resize(img4, resized_img4, cv::Size(result.cols / 2, result.rows / 2));
    }

    // 将用户图片放置到合适的位置上去
    if (!resized_img1.empty())
    {
        cv::Rect roi1(0, 0, resized_img1.cols, resized_img1.rows);
        resized_img1.copyTo(result(roi1));
    }
    if (!resized_img2.empty())
    {
        cv::Rect roi2(resized_img1.cols, 0, resized_img2.cols, resized_img2.rows);
        resized_img2.copyTo(result(roi2));
    }
    if (!resized_img3.empty())
    {
        cv::Rect roi3(0, resized_img1.rows, resized_img3.cols, resized_img3.rows);
        resized_img3.copyTo(result(roi3));
    }
    if (!resized_img4.empty())
    {
        cv::Rect roi4(resized_img3.cols, resized_img1.rows, resized_img4.cols, resized_img4.rows);
        resized_img4.copyTo(result(roi4));
    }

    // 显示图像
    cv::imshow(handle, result);
}
