#include <iostream>
#include <chrono>
#include <iomanip>
#include <thread>
#include <unordered_map>
#include <optional>
#include <string>

class Timer {
#define GENERAL
#define MARK
public:
    enum class Unit {
        Seconds,
        Milliseconds,
        Microseconds
    };

    Timer() {
        tic();
    }

    GENERAL void tic() {
        startTime = std::chrono::steady_clock::now();
        lastLapTime = startTime;
        markMap.clear();
    }

    GENERAL float get(Unit unit = Unit::Milliseconds) const {
        auto now = std::chrono::steady_clock::now();
        return convertDuration(now - startTime, unit);
    }

    GENERAL float lap(Unit unit = Unit::Milliseconds) {
        auto now = std::chrono::steady_clock::now();
        auto duration = now - lastLapTime;
        lastLapTime = now;
        return convertDuration(duration, unit);
    }

    MARK void mark(const std::string& tag) {
        markMap[tag] = std::chrono::steady_clock::now();
    }

    MARK bool isTriggered(const std::string& tag, float delay, Unit unit = Unit::Milliseconds) const {
        auto it = markMap.find(tag);
        if (it == markMap.end()) return false;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - it->second;
        return convertDuration(elapsed, unit) >= delay;
    }

    MARK float sinceMark(const std::string& tag, Unit unit = Unit::Milliseconds) const {
        auto it = markMap.find(tag);
        if (it == markMap.end()) return -1.0f;

        auto now = std::chrono::steady_clock::now();
        return convertDuration(now - it->second, unit);
    }

    MARK bool removeMark(const std::string& tag) {
        return markMap.erase(tag) > 0;
    }

private:
    GENERAL std::chrono::steady_clock::time_point startTime;
    GENERAL std::chrono::steady_clock::time_point lastLapTime;
    MARK std::unordered_map<std::string, std::chrono::steady_clock::time_point> markMap;

    GENERAL float convertDuration(std::chrono::steady_clock::duration duration, Unit unit) const {
        switch (unit) {
        case Unit::Seconds:
            return std::chrono::duration<float>(duration).count();
        case Unit::Milliseconds:
            return std::chrono::duration<float, std::milli>(duration).count();
        case Unit::Microseconds:
            return std::chrono::duration<float, std::micro>(duration).count();
        default:
            return 0.0f;
        }
    }
#undef GENERAL
#undef MARK
};

#if false
Timer timer;
int main() {
    timer.tic(); // tic 用于开始计时，初始化的时候调用一次

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 等待 100 ms 后
    timer.mark("task1"); // 标记一个时间标签 1
    std::cout << "Marked task1 at " << timer.get() << " ms\n"; // 默认返回毫秒单位
    std::cout << "Marked task1 at " << timer.get(Timer::Unit::Seconds) << " s\n"; // 显示秒单位
    std::cout << "Marked task1 at " << timer.get(Timer::Unit::Microseconds) << " μs\n"; // 显示微秒单位

    std::this_thread::sleep_for(std::chrono::milliseconds(120)); // 等待 120 ms 后
    timer.mark("task2"); // 标记一个时间标签 2

    if (timer.isTriggered("task1", 120.0f)) { // 判断距离记录时间标签 1 是否已经过去 120 ms（称作时间标签 1 是否触发）
        std::cout << "task1 triggered (120 ms)\n";
    }
    else {
        std::cout << "task1 not triggered yet\n";
    }

    if (timer.isTriggered("task2", 120.0f)) { // 判断距离记录时间标签 2 是否已经过去 120 ms
        std::cout << "task2 triggered (120 ms)\n";
    }
    else {
        std::cout << "task2 not triggered yet\n";
    }

    std::cout << "Time since task1: " << timer.sinceMark("task1") << " ms\n"; // 默认返回的单位是毫秒
    std::cout << "Time since task2: " << timer.sinceMark("task2", Timer::Unit::Seconds) << " ms\n"; // 返回以秒为单位的时间

    if (timer.removeMark("task1")) {
        std::cout << "task1 mark removed.\n";
    }

    if (!timer.isTriggered("task1", 100.0f)) {
        std::cout << "After removal, task1 trigger check: false\n";
    }

    return 0;
}
#endif