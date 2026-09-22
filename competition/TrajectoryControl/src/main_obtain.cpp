#include "main.h"

int main_obtain(void)
{
	/* 获取地图预设主车路径点 */
	ASSERT(SimOneAPI::GetWayPoints(mainVehicle.id, pWayPoints.get()), "获取主车预设路径点失败");
	for (size_t i = 0; i < pWayPoints->wayPointsSize; ++i) {
		SSD::SimPoint3D inputWayPoints(pWayPoints->wayPoints[i].posX, pWayPoints->wayPoints[i].posY, 0);
		initialPath.push_back(inputWayPoints);
	}

	namespace fs = std::filesystem;

	// 构造完整路径
	fs::path outputDir = fs::path("../../TrajectoryControl/m_strategy/");
	fs::path outputFile = outputDir / (std::to_string(caseIdx) + ".stg");

	// 创建案例专属目录
	if (!fs::exists(outputDir)) {
		try
		{
			fs::create_directories(outputDir);
		}
		catch (const std::exception& e)
		{
			globalLogger(Logger::Color::BrightMagenta) << "创建目录失败：" << e.what();
			return -1;
		}
	}

	// 打开文件流
	std::ofstream file(outputFile);
	ASSERT(file, ("无法打开文件: " + outputFile.string()));

	// 写入数据
	constexpr int digit = 4; // 浮点型输出保留几位小数
	constexpr int defaultPointModes = -1; // 默认打点模式
	constexpr int defaultStopTime = -1; // 默认停止时间
	constexpr int defaultKp = -1; // 默认 PID 参数
	constexpr float defaultSpeed = 10.0f; // 默认速度

	/* std::fixed 设置输出浮点数为定点格式，以小数的形式输出，而非科学计数法
	 * std::setprecision(n) 设置小数精度为 n 位，也就是小数点后保留 n 位数字
	 * std::fixed 和 std::setprecision(n) 只影响浮点数输出格式，对整数无影响 */
	file << std::fixed << std::setprecision(digit);

	file << "{\n  \"strategy\": [\n";
	for (size_t i = 0; i < pWayPoints->wayPointsSize; ++i)
	{
		file << "    ["
			<< pWayPoints->wayPoints[i].index << ", "
			<< static_cast<float>(pWayPoints->wayPoints[i].posX) << ", "
			<< static_cast<float>(pWayPoints->wayPoints[i].posY) << ", "
			<< defaultPointModes << ", "
			<< defaultStopTime << ", "
			<< defaultSpeed << ", "
			<< defaultKp << "]";

		if (i + 1 < pWayPoints->wayPointsSize)
			file << ",\n";
		else
			file << "\n";
	}
	file << "  ]\n}";

	// 写入后刷新并关闭文件
	file.flush();
	file.close();

	// 日志输出提示
	std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 等待 1000ms
	globalLogger(Logger::Color::BrightMagenta) << "★ 成功导出案例 [" << std::to_string(caseIdx) << "] 策略点到: " << outputFile;
	globalLogger(Logger::Color::BrightBlue) << "★ 导出路径点数量：" << pWayPoints->wayPointsSize;

	// 打开策略点文件
	std::string command = "notepad \"" + outputFile.string() + "\"";
	std::system(command.c_str());

	return 0;
}