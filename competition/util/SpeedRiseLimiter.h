#pragma once
#include <algorithm>
#include <cmath>

// SI units, simulation time. This limits requested speed, not measured acceleration.
// Decreases (especially a zero-speed request) must never be delayed.
class SpeedRiseLimiter {
public:
    void reset() { initialized = false; lastTime = lastSpeed = 0.0; }

    bool update(double requested, double measured, double time, double rate, double& output) {
        output = 0.0;
        if (!std::isfinite(requested) || requested < 0 ||
            !std::isfinite(measured) || measured < 0 ||
            !std::isfinite(time) || time < 0 || !std::isfinite(rate) || rate <= 0 ||
            (initialized && (time < lastTime || time - lastTime > 0.25 + 1e-9))) {
            reset();
            return false;
        }
        // First observation cannot grant acceleration before any time elapsed.
        output = initialized ? std::min(requested, lastSpeed + rate * (time - lastTime))
                             : std::min(requested, measured);
        lastTime = time;
        lastSpeed = output;
        initialized = true;
        return true;
    }

private:
    bool initialized = false;
    double lastTime = 0.0, lastSpeed = 0.0;
};
