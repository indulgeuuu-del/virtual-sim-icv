#pragma once
#include <algorithm>
#include <cmath>
#include "StopController.h"

// SI units. Geometry and actuator response must be supplied/validated per platform.
// This controls a continuing stop obligation; release/dwell are caller decisions.
namespace StopLineControl {
struct Approach {
    double deceleration = 1.5; // engineering speed envelope, NOT measured braking capability
    double gain = 0.8;         // 1/s, slow approach near the desired gap
    double capture = 0.05;     // m, finite stop band instead of asymptotic creeping
    double captureSpeed = 0.25;// m/s, only capture at low speed
};

inline bool valid(double distance, double lineOffset, double frontOffset,
                  double speed, double ceiling, double margin) {
    return std::isfinite(distance) && std::isfinite(lineOffset) && lineOffset >= 0
        && std::isfinite(frontOffset) && frontOffset >= 0
        && std::isfinite(speed) && speed >= 0
        && std::isfinite(ceiling) && ceiling >= 0
        && std::isfinite(margin) && margin >= 0;
}

// Preserve historical float arithmetic and equality at the braking boundary.
inline double legacyTarget(float distance, float lineOffset, double frontOffset,
                           float speed, double ceiling, float margin) {
    if (!valid(distance,lineOffset,frontOffset,speed,ceiling,margin)) return 0;
    float d0 = 0.05272 * speed * speed + 0.06714 * speed - 0.02188;
    return distance - lineOffset - frontOffset <= d0 + margin ? 0 : ceiling;
}

// reference distance - obstacle/line offset - ego front offset = signed body gap.
// The limit falls continuously with remaining distance; it never raises a prior
// speed constraint. A passed line remains a stop obligation (negative gap).
inline double approachTarget(double distance, double lineOffset, double frontOffset,
                             double speed, double ceiling, double margin,
                             const Approach& p = Approach{}) {
    if (!valid(distance,lineOffset,frontOffset,speed,ceiling,margin)
        || !std::isfinite(p.deceleration) || p.deceleration <= 0
        || !std::isfinite(p.gain) || p.gain <= 0
        || !std::isfinite(p.capture) || p.capture <= 0
        || !std::isfinite(p.captureSpeed) || p.captureSpeed <= 0) return 0;
    const double remaining = distance - lineOffset - frontOffset - margin;
    if (!std::isfinite(remaining) || remaining <= 0
        || (remaining <= p.capture && speed <= p.captureSpeed)) return 0;
    const double envelope = std::sqrt(2 * p.deceleration * remaining);
    const double approach = p.gain * remaining;
    if (!std::isfinite(envelope) || !std::isfinite(approach)) return 0;
    return std::min(ceiling, std::min(envelope, approach));
}

// One assigned mandatory stop, with a separately supplied departure permission.
// The adapter must reset for a new stop and mark unknown observations invalid.
// No signal recognition or scenario-ID assumptions are made here.
class Sequence {
public:
    explicit Sequence(double dwell, bool exclusive = true) : stop_(dwell,exclusive) {}
    void reset(double dwell, bool exclusive = true) {
        stop_.reset(dwell,exclusive); ready_ = departed_ = false;
    }
    competition::StopOutput update(double seconds, double distance, double front,
        double speed, double ceiling, double margin, bool permission, bool observed = true) {
        const Approach p;
        if (!observed || !valid(distance,0,front,speed,ceiling,margin)) {
            stop_.invalidate(); ready_ = false; return {};
        }
        const double gap = distance - front;
        const bool captured = gap >= 0 && gap <= margin + p.capture && speed <= p.captureSpeed;
        // Before departure, rolling or leaving the stop zone invalidates a
        // completed hold. StopController's generic ready latch is otherwise sticky.
        if (!departed_ && ready_ && (speed > 0.1 || !captured)) stop_.invalidate();
        const auto state = stop_.update(seconds,speed,captured,ceiling,permission);
        ready_ = state.ready;
        if (state.ready && permission && state.targetSpeed > 0) departed_ = true;
        if (state.phase != competition::StopPhase::Approaching) return state;
        // A crossed line cannot become a successful stop via a negative distance.
        if (gap < 0) { stop_.invalidate(); return {}; }
        auto result = state;
        result.targetSpeed = approachTarget(distance,0,front,speed,ceiling,margin);
        return result;
    }
private:
    competition::StopController stop_;
    bool ready_ = false, departed_ = false;
};
} // namespace StopLineControl
