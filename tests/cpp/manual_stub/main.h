#pragma once
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Deterministic test doubles, not an SDK compatibility layer.
namespace SSD {
struct SimPoint3D { double x = 0, y = 0, z = 0; };
using SimPoint3DVector = std::vector<SimPoint3D>;
}
struct StrategyPoint {
    SSD::SimPoint3D Pos;
    int pathMode = -1;
    double stopTime = -1, speed = -1, kp = -1;
};
struct CheckedPoints : std::vector<StrategyPoint> {
    StrategyPoint& operator[](size_t i) { return at(i); }
};
inline CheckedPoints strategyPoint;
inline std::string logText; // everything the loop logged, for asserting diagnostics
struct Logger {
    enum class Color { BrightBlue, BrightMagenta, BrightGreen };
    Logger& operator()(Color) { logText += '\n'; return *this; }
    template<class T> Logger& operator<<(const T& v) {
        std::ostringstream os; os << v; logText += os.str(); return *this;
    }
};
inline Logger globalLogger;
#define LOG globalLogger
inline int frameCount = 0, waits = 0, advances = 0, frameLimit = 1, routes = 0, samples = 0;
inline std::set<int> failingRoutes; // 1-based route/sampling call numbers that fail
inline double now = 0, step = 0.1;
struct Timer {
    enum class Unit { Seconds };
    std::unordered_map<std::string, double> marks;
    void tic() {}
    double get() { return now; }
    void mark(const std::string& s) { marks[s] = now; }
    bool isTriggered(const std::string& s, float duration, Unit) {
        return now - marks.at(s) >= duration;
    }
    void removeMark(const std::string& s) { marks.erase(s); }
    double sinceMark(const std::string& s) { return now - marks.at(s); }
};
inline Timer timer;
inline double steerKp = 1.2, steerKd = 0.1, steerKpUse = 0, steerKdUse = 0;
inline double caseTargetSpeed = 8, achieveThres = 1;
enum class ESimOne_Gear_Mode { ESimOne_Gear_Mode_Drive, ESimOne_Gear_Mode_Other };
struct Control {
    double throttle = -999, steering = -999;
    ESimOne_Gear_Mode gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Other;
};
inline std::unique_ptr<Control> pControl = std::make_unique<Control>();
// drove: pure pursuit ran on targetPath; otherwise a direct stop command was sent.
struct Observation { double speed, kp, targetX; bool drove; };
inline std::vector<Observation> observations;
inline std::vector<SSD::SimPoint3D> initialPath, targetPath;
inline std::vector<int> validWayPoints, obstacleList, stopLineList;
struct Vehicle {
    const char* id = "0";
    SSD::SimPoint3D pt;
    void update() {}
    void drive() {
        if (targetPath.size() < 2) throw std::runtime_error("drive without path");
        observations.push_back({pControl->throttle, steerKpUse, targetPath.back().x, true});
    }
};
inline Vehicle mainVehicle;
inline float calculateFps(int, double) { return 10; }
inline void recordEvaluation() {}
inline void updateObstacleData() {}
inline void waitInitial() {}
namespace UtilMath {
inline double planarDistance(const SSD::SimPoint3D& a, const SSD::SimPoint3D& b) {
    return std::hypot(a.x-b.x, a.y-b.y);
}
}
inline float getDistS(const SSD::SimPoint3D& a, const SSD::SimPoint3D& b) {
    return static_cast<float>(UtilMath::planarDistance(a,b));
}
// A failed call leaves partial garbage in out, so callers must not trust it.
inline bool routeCall(const std::vector<SSD::SimPoint3D>& in, std::vector<SSD::SimPoint3D>& out) {
    if (failingRoutes.count(++routes)) { out = {{-12345, 0, 0}, {-12345, 0, 0}}; return false; }
    out = in; return true;
}
// Mirrors process.cpp: true means success, fewer than two input points fail.
inline bool equidistantSampling(const std::vector<SSD::SimPoint3D>& in,
                               std::vector<SSD::SimPoint3D>& out, double) {
    ++samples;
    out.clear();
    if (in.size() < 2) return false;
    return routeCall(in, out);
}
struct EndFrames {};
namespace SimOneAPI {
inline int Wait() {
    if (waits == frameLimit) throw EndFrames{};
    now = waits * step;
    return ++waits;
}
inline void NextFrame(int frame) {
    if (frame != waits || advances + 1 != waits)
        throw std::runtime_error("unpaired frame");
    ++advances;
}
inline bool GenerateRoute(const std::vector<SSD::SimPoint3D>& in,
                          std::vector<int>&, std::vector<SSD::SimPoint3D>& out) {
    return routeCall(in, out);
}
inline void SetDrive(const char*, Control* c) {
    if (c->gear != ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive || c->steering != 0)
        throw std::runtime_error("direct command is not a stop");
    observations.push_back({c->throttle, steerKpUse, std::nan(""), false});
}
}
