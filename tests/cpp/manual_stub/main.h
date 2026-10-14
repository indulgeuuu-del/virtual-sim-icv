#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

// Deterministic test doubles, not an SDK compatibility layer.
namespace SSD {
struct SimPoint3D {
    double x = 0, y = 0, z = 0;
    SimPoint3D() = default;
    SimPoint3D(double a, double b, double c) : x(a), y(b), z(c) {}
};
inline std::ostream& operator<<(std::ostream& os, const SimPoint3D& p) {
    return os << p.x << ',' << p.y << ',' << p.z;
}
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
enum class ESimOne_Gear_Mode {
    ESimOne_Gear_Mode_Drive, ESimOne_Gear_Mode_Other,
    ESimOne_Gear_Mode_Reverse, ESimOne_Gear_Mode_Neutral
};
enum class ESimOne_Throttle_Mode { ESimOne_Throttle_Mode_Speed, ESimOne_Throttle_Mode_Percent };
struct Control {
    double throttle = -999, steering = -999;
    ESimOne_Gear_Mode gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Other;
    ESimOne_Throttle_Mode throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
    long long timestamp = 0;
    double brake = 0;
    bool handbrake = false, isManualGear = false;
};
inline std::unique_ptr<Control> pControl = std::make_unique<Control>();
// drove: pure pursuit ran on targetPath; otherwise a direct stop command was sent.
struct Observation {
    double speed, kp, targetX;
    bool drove;
    ESimOne_Gear_Mode gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Other;
    double steering = 0;
};
inline std::vector<Observation> observations;
inline std::vector<SSD::SimPoint3D> initialPath, targetPath;
inline std::vector<int> validWayPoints, obstacleList, stopLineList;
struct SimOne_Data_Gps {
    double posX = 0, posY = 0, posZ = 0, velX = 0, velY = 0, velZ = 0, oriZ = 0;
    long long timestamp = 0;
};
inline auto pGps = std::make_unique<SimOne_Data_Gps>();
struct FrameInput { double x, speed; long long timestamp; bool valid = true; bool obstacleValid = true; };
inline std::vector<FrameInput> frameInputs;
// The real-drive build retains the production drive, pursuit, PID and curvature
// bodies. update() supplies deterministic frame input; it is not an SDK test.
#ifdef TEST_REAL_DRIVE
using SimOne_Data_Control = Control;
struct SignalLights { int signalLights = 0; };
inline auto pLight = std::make_unique<SignalLights>();
inline int lightSends = 0, lastLights = -1, caseIdx = 41;
inline size_t ppTargetIndex = 0;
inline float steerKi = 0, steeringOffsetKp = 0;
inline std::vector<std::tuple<SSD::SimPoint3D, SSD::SimPoint3D>> slideConfig;
namespace FlagType { inline bool isManualTrackMode = true; }
#endif
struct MainVehicle {
    const char* id = "0";
    SSD::SimPoint3D pt;
    bool useDefaultPath = true, reverse = false;
    float speed = 0, steeringOffset = 0;
    double s = 0;
    bool update() {
        const FrameInput input = frameInputs.empty()
            ? FrameInput{pt.x, speed, static_cast<long long>(std::llround(now * 1000))}
            : frameInputs.at(waits - 1);
        if (!input.valid) return false;
        pt.x = input.x; speed = static_cast<float>(input.speed);
        pGps->timestamp = input.timestamp;
#ifdef TEST_REAL_DRIVE
        pControl->throttle = caseTargetSpeed;
        pControl->steering = 0;
        pControl->gear = ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive;
        pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
        pGps->posX = pt.x; pGps->posY = pt.y; pGps->posZ = pt.z;
#endif
        return true;
    }
#ifdef TEST_REAL_DRIVE
    void drive();
#else
    void drive() {
        if (targetPath.size() < 2) throw std::runtime_error("drive without path");
        observations.push_back({pControl->throttle, steerKpUse, targetPath.back().x, true});
    }
#endif
};
inline MainVehicle mainVehicle;
inline float calculateFps(int, double) { return 10; }
inline void recordEvaluation() {}
inline bool updateObstacleData() { return frameInputs.empty() || frameInputs.at(waits-1).obstacleValid; }
inline void waitInitial() {}
namespace UtilMath {
inline double planarDistance(const SSD::SimPoint3D& a, const SSD::SimPoint3D& b) {
    return std::hypot(a.x-b.x, a.y-b.y);
}
}
inline float getDistS(const SSD::SimPoint3D& a, const SSD::SimPoint3D& b) {
    return static_cast<float>(b.x-a.x); // signed lane-s distance, as in process.cpp
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
inline bool SetDrive(const char*, Control* c) {
#ifdef TEST_REAL_DRIVE
    // Capture the SDK boundary AFTER the complete production drive() body.
    const bool hasPath = targetPath.size() >= 2;
    observations.push_back({c->throttle, steerKpUse,
                           hasPath ? targetPath.back().x : std::nan(""), hasPath,
                           c->gear, c->steering});
#else
    if (c->gear != ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive || c->steering != 0)
        throw std::runtime_error("direct command is not a stop");
    observations.push_back({c->throttle, steerKpUse, std::nan(""), false});
#endif
    return true;
}
#ifdef TEST_REAL_DRIVE
inline bool SetSignalLights(const char*, SignalLights* lights) {
    ++lightSends; lastLights = lights->signalLights; return true;
}
#endif
}
