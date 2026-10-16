#pragma once
#include <algorithm>
#include <cmath>

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
} // namespace StopLineControl
