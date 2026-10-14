#include "StopController.h"
#include <new>

// Narrow C ABI: Python/CARLA calls the same C++ controller as main_manual.cpp.
// This bridge contains no timing or decision algorithm.
#ifdef _WIN32
#define STOP_EXPORT extern "C" __declspec(dllexport)
#else
#define STOP_EXPORT extern "C"
#endif
struct StopAbiOutput { double targetSpeed, heldSeconds; int phase, ready; };
STOP_EXPORT int stop_abi_version() { return 1; }
STOP_EXPORT void* stop_create(double seconds, int exclusive, double speed, double gap) {
    if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(speed) || speed < 0 ||
        !std::isfinite(gap) || gap <= 0) return nullptr;
    return new (std::nothrow) competition::StopController(seconds, exclusive != 0, speed, gap);
}
STOP_EXPORT void stop_destroy(void* handle) {
    delete static_cast<competition::StopController*>(handle);
}
STOP_EXPORT int stop_reset(void* handle, double seconds, int exclusive) {
    if (!handle || !std::isfinite(seconds) || seconds < 0) return 0;
    static_cast<competition::StopController*>(handle)->reset(seconds, exclusive != 0);
    return 1;
}
STOP_EXPORT int stop_step(void* handle, double time, double speed, int inZone,
                         double approach, int release, int valid, StopAbiOutput* out) {
    if (!handle || !out) return 0;
    const auto value = static_cast<competition::StopController*>(handle)->update(
        time, speed, inZone != 0, approach, release != 0, valid != 0);
    *out = {value.targetSpeed, value.heldSeconds, static_cast<int>(value.phase), value.ready ? 1 : 0};
    return 1;
}
