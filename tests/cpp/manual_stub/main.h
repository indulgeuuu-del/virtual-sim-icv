#pragma once
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Deterministic test doubles, not an SDK compatibility layer.
namespace SSD {
struct SimPoint3D { double x = 0, y = 0, z = 0; };
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
struct Logger {
    enum class Color { BrightBlue, BrightMagenta, BrightGreen };
    Logger& operator()(Color) { return *this; }
    template<class T> Logger& operator<<(const T&) { return *this; }
};
inline Logger globalLogger;
#define LOG globalLogger
inline int frameCount = 0, waits = 0, advances = 0, frameLimit = 1, routes = 0;
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
struct Control { double throttle = -999; };
inline Control control;
inline Control* pControl = &control;
struct Observation { double speed, kp, targetX; };
inline std::vector<Observation> observations;
inline std::vector<SSD::SimPoint3D> initialPath, targetPath;
inline std::vector<int> validWayPoints, obstacleList, stopLineList;
struct Vehicle {
    SSD::SimPoint3D pt;
    void update() {}
    void drive() {
        if (targetPath.empty()) throw std::runtime_error("drive without path");
        observations.push_back({control.throttle, steerKpUse, targetPath.back().x});
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
inline bool equidistantSampling(const std::vector<SSD::SimPoint3D>& in,
                               std::vector<SSD::SimPoint3D>& out, double) {
    ++routes; out = in; return false;
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
    ++routes; out = in; return true;
}
}
