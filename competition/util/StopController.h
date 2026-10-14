#pragma once
#include <cmath>
#include <limits>

namespace competition {

// Time: simulation seconds. Speed: m/s. Tolerances are engineering settings,
// not official scoring thresholds. Caller supplies the zone and departure permission.
enum class StopPhase { Approaching = 0, Stopping = 1, Holding = 2, Ready = 3, Invalid = 4 };
struct StopOutput {
    double targetSpeed = 0;
    double heldSeconds = 0;
    StopPhase phase = StopPhase::Invalid;
    bool ready = false;
};

class StopController {
public:
    explicit StopController(double dwellSeconds = 0.5, bool exclusive = false,
                            double stationarySpeed = 0.1, double maxSampleGap = 0.25)
        : dwell_(dwellSeconds), stationarySpeed_(stationarySpeed), maxGap_(maxSampleGap),
          exclusive_(exclusive) {}

    void reset(double dwellSeconds, bool exclusive = false) {
        dwell_ = dwellSeconds; exclusive_ = exclusive;
        active_ = false;
        invalidate();
    }

    // Missing observations cannot count as standstill. Keep an active stop latched.
    void invalidate() {
        last_ = start_ = std::numeric_limits<double>::quiet_NaN();
        held_ = 0; ready_ = false;
    }

    StopOutput update(double seconds, double speed, bool inZone,
                      double approachSpeed, bool releaseAllowed, bool valid = true) {
        if (!valid || !std::isfinite(seconds) || seconds < 0 || !std::isfinite(speed) ||
            !std::isfinite(approachSpeed) || approachSpeed < 0 || !std::isfinite(dwell_) || dwell_ < 0 ||
            !std::isfinite(stationarySpeed_) || stationarySpeed_ < 0 ||
            !std::isfinite(maxGap_) || maxGap_ <= 0) {
            invalidate(); return {};
        }
        if (std::isfinite(last_) && (seconds < last_ || seconds - last_ > maxGap_)) {
            invalidate(); // next fresh sample starts a new observed interval
            return {};
        }
        last_ = seconds;
        if (ready_) return {releaseAllowed ? approachSpeed : 0, held_, StopPhase::Ready, true};
        active_ = active_ || inZone;
        if (!active_) return {approachSpeed, 0, StopPhase::Approaching, false};
        if (!inZone || std::abs(speed) > stationarySpeed_) {
            start_ = std::numeric_limits<double>::quiet_NaN(); held_ = 0;
            return {0, 0, StopPhase::Stopping, false};
        }
        if (!std::isfinite(start_)) start_ = seconds;
        held_ = seconds - start_;
        // Exact >= or > semantics; no epsilon that could authorize an early departure.
        ready_ = exclusive_ ? held_ > dwell_ : held_ >= dwell_;
        return {ready_ && releaseAllowed ? approachSpeed : 0, held_,
                ready_ ? StopPhase::Ready : StopPhase::Holding, ready_};
    }

private:
    double dwell_, stationarySpeed_, maxGap_;
    bool exclusive_, active_ = false, ready_ = false;
    double last_ = std::numeric_limits<double>::quiet_NaN();
    double start_ = std::numeric_limits<double>::quiet_NaN();
    double held_ = 0;
};
}
