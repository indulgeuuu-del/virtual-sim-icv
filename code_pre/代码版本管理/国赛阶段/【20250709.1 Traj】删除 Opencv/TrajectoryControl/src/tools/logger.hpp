#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <windows.h>
#include <cstdarg>
#include <vector>

#define SYMBOL_CRITICAL       "※"   // 特别重要，最高等级
#define SYMBOL_STAR           "★"   // 普通重要（可以组合表示多等级）
#define SYMBOL_CIRCLE_SOLID   "●"   // 中等重要
#define SYMBOL_CIRCLE_HOLLOW  "○"   // 次要
#define SYMBOL_TRIANGLE       "▲"   // 注意事项
#define SYMBOL_TRIANGLE_EMPTY "△"   // 普通、候选项
#define SYMBOL_TRIANGLE_DOWN  "▽"   // 不活跃、次级状态
#define SYMBOL_SQUARE         "■"   // 固定内容块
#define SYMBOL_DIAMOND        "◆"   // 特别标注

inline std::ostream& operator<<(std::ostream& os, const SimOne_Data_Vec3f& p)
{
    os << "(" << p.x << ", " << p.y << ")";
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const SSD::SimPoint3D& p)
{
    os << "(" << p.x << ", " << p.y << ")";
    return os;
}

class Logger {
public:
    // 颜色枚举（包含16种常见控制台颜色）
    enum class Color : WORD {
        Black = 0,
        Blue = FOREGROUND_BLUE,
        Green = FOREGROUND_GREEN,
        Cyan = FOREGROUND_BLUE | FOREGROUND_GREEN,
        Red = FOREGROUND_RED,
        Magenta = FOREGROUND_BLUE | FOREGROUND_RED,
        Yellow = FOREGROUND_GREEN | FOREGROUND_RED,
        White = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
        Gray = FOREGROUND_INTENSITY,
        BrightBlue = FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        BrightGreen = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        BrightCyan = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        BrightRed = FOREGROUND_RED | FOREGROUND_INTENSITY,
        BrightMagenta = FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY,
        BrightYellow = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY,
        BrightWhite = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        Default = White
    };

    // 日志输出模式
    enum class Mode {
        ConsoleOnly,
        FileOnly,
        Both
    };

    // 构造函数
    Logger(const std::string& name,
        Color nameColor = Color::BrightWhite,
        Mode mode = Mode::ConsoleOnly,
        const std::string& filename = "")
        : name_(name), nameColor_(nameColor), mode_(mode), enabled_(true) {
        if (!filename.empty()) {
            openLogFile(filename);
        }
    }

    // 设置日志启用状态
    void enable(bool enabled) { enabled_ = enabled; }

    // 设置输出模式
    void setMode(Mode mode) { mode_ = mode; }

    // 打开日志文件
    void openLogFile(const std::string& filename) {
        file_.open(filename, std::ios::out | std::ios::app);
    }

    // printf风格日志输出
    template<typename... Args>
    void log(Color color, const char* format, Args... args) {
        if (!enabled_) return;

        std::string message = formatString(format, args...);
        output(message, color);
    }

    // 流式日志输出
    class LogStream {
    public:
        LogStream(Logger& logger, Color color)
            : logger_(logger), color_(color) {
        }

        // 禁用拷贝构造函数
        LogStream(const LogStream&) = delete;

        // 提供移动构造函数
        LogStream(LogStream&& other) noexcept
            : logger_(other.logger_), color_(other.color_), ss_(std::move(other.ss_)) {
        }

        ~LogStream() {
            if (logger_.enabled_) {
                logger_.output(ss_.str(), color_);
            }
        }

        template<typename T>
        LogStream& operator<<(const T& value) {
            ss_ << value;
            return *this;
        }

        // 处理流操作符（如std::endl）
        LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
            ss_ << manip;
            return *this;
        }
 
    private:
        Logger& logger_;
        Color color_;
        std::ostringstream ss_;
    };

    // 获取流式输出对象
    LogStream operator()(Color color = Color::Default) {
        return LogStream(*this, color);
    }

private:
    std::string name_;
    Color nameColor_;
    Mode mode_;
    std::ofstream file_;
    bool enabled_;
    std::mutex mutex_;

    std::string formatString(const char* format, ...) {
        va_list args;
        va_start(args, format);
        int size = vsnprintf(nullptr, 0, format, args);
        va_end(args);

        if (size < 0) return "[Format Error]";

        std::vector<char> buf(size + 1);
        va_start(args, format);
        vsnprintf(buf.data(), buf.size(), format, args);
        va_end(args);

        return std::string(buf.data());
    }

    // 获取时间戳
    std::string getTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &in_time_t);
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // 设置控制台颜色（RAII保护）
    class ColorGuard {
    public:
        ColorGuard(HANDLE hConsole, Color color)
            : hConsole_(hConsole) {
            GetConsoleScreenBufferInfo(hConsole_, &origInfo_);
            SetConsoleTextAttribute(hConsole_, static_cast<WORD>(color));
        }

        ~ColorGuard() {
            SetConsoleTextAttribute(hConsole_, origInfo_.wAttributes);
        }

    private:
        HANDLE hConsole_;
        CONSOLE_SCREEN_BUFFER_INFO origInfo_;
    };

    // 核心输出方法
    void output(const std::string& message, Color color) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string timestamp = getTimestamp();
        const std::string fullMessage = timestamp + " [" + name_ + "] " + message;

        // 文件输出
        if ((mode_ == Mode::FileOnly || mode_ == Mode::Both) && file_.is_open()) {
            file_ << fullMessage << std::endl;
        }

        // 控制台输出
        if (mode_ == Mode::ConsoleOnly || mode_ == Mode::Both) {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

            // 输出时间戳（白色）
            {
                ColorGuard guard(hConsole, Color::BrightWhite);
                std::cout << timestamp << " ";
            }

            // 输出logger名称（指定颜色）
            {
                ColorGuard guard(hConsole, nameColor_);
                std::cout << "[" << name_ << "] ";
            }

            // 输出消息内容（指定颜色）
            {
                ColorGuard guard(hConsole, color);
                std::cout << message;
            }

            // 换行并刷新
            std::cout << std::endl;
        }
    }
};

#if false
// 全局日志实例
Logger globalLogger("Global", Logger::Color::BrightCyan);

// 使用示例
int main() {
    // printf风格输出
    globalLogger.log(Logger::Color::BrightYellow, "System initialized, version: %d.%d", 1, 0);

    // 流式输出
    globalLogger(Logger::Color::BrightGreen) << "Sensor data received: "
        << 42.5f << " units";

    // 禁用日志
    globalLogger.enable(false);
    globalLogger(Logger::Color::Red) << "This won't be displayed";

    // 重新启用并设置文件输出
    globalLogger.enable(true);
    globalLogger.openLogFile("app.log");
    globalLogger.setMode(Logger::Mode::Both);
    globalLogger(Logger::Color::Default) << "Application shutdown";

    return 0;
}
#endif