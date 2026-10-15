// SDK boundary for compiling the exact production following functions.
// No obstacle construction, map projection, SDK scheduling or drive() emulation.
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <new>
#include <vector>
#include "controller.hpp"

struct Obstacle {
    enum class Type { Static, Vertical, Horizental, AmongCrosswalk };
    Type type = Type::Static;
    double sRelativeToVehicle = 0;
    float velocity = 0;
    bool isSameLaneWithMainVehicle = true, isSameRoadWithMainVehicle = true;
};
struct VehicleInput { double s = 0; float speed = 0; } mainVehicle;
struct NullLog { template<class T> NullLog& operator<<(const T&) { return *this; } } nullLog;
#define LOG nullLog
float followingLimitSpeed = 0;
int caseStop = 0;
AdaptiveFollowing adaptiveFollowing(0, 0, 0, 45);

// @PRODUCTION_NEAREST@
// @PRODUCTION_FOLLOWING@

struct Context {
    AdaptiveFollowing following;
    float minimum;
    int stopCase;
    Context(float kp, float ki, float kd, float limit, int stop)
        : following(kp, ki, kd, 45), minimum(limit), stopCase(stop) {}
};
struct Input { double s, speed; int type, sameLane, sameRoad; };
struct Output { double target, selectedS, selectedDistance; int found; };
static std::mutex boundaryMutex;
#define API extern "C" __declspec(dllexport)
API int following_abi_version() { return 3; }
API void* following_create(float kp, float ki, float kd, float minimum, int stopCase) {
    if (!std::isfinite(kp) || !std::isfinite(ki) || !std::isfinite(kd) ||
        !std::isfinite(minimum) || minimum < 0) return nullptr;
    return new(std::nothrow) Context(kp, ki, kd, minimum, stopCase);
}
API void following_destroy(void* p) { delete static_cast<Context*>(p); }
API int following_reset(void* p) {
    if (!p) return 0;
    const std::lock_guard<std::mutex> lock(boundaryMutex);
    static_cast<Context*>(p)->following.reset();
    return 1;
}
API int following_step(void* p, double egoS, double egoSpeed, const Input* input,
                       int count, int caseIndex, int useGeometry, double staticOffset,
                       int usePolicy, double movingGap, double stoppedGap, double maxSpeed, Output* out) {
    if (!p || !out || count < 0 || count > 10000 || (count && !input) ||
        !std::isfinite(egoS) || !std::isfinite(egoSpeed) || egoSpeed < 0 || egoSpeed > 1000) return 0;
    try {
        std::vector<Obstacle> obstacles;
        for (int i=0; i<count; ++i) {
            if (!std::isfinite(input[i].s) || !std::isfinite(input[i].speed) ||
                input[i].speed < 0 || input[i].speed > 1000 || input[i].type < 0 || input[i].type > 3) return 0;
            Obstacle o;
            o.sRelativeToVehicle=input[i].s; o.velocity=static_cast<float>(input[i].speed);
            o.type=static_cast<Obstacle::Type>(input[i].type);
            o.isSameLaneWithMainVehicle=input[i].sameLane!=0;
            o.isSameRoadWithMainVehicle=input[i].sameRoad!=0;
            obstacles.push_back(o);
        }
        const std::lock_guard<std::mutex> lock(boundaryMutex);
        auto& context=*static_cast<Context*>(p);
        mainVehicle={egoS, static_cast<float>(egoSpeed)};
        followingLimitSpeed=context.minimum; caseStop=context.stopCase;
        adaptiveFollowing=context.following;
        const float target=usePolicy ? calculateFollowingSpeedWithPolicy(obstacles, staticOffset, movingGap, stoppedGap, maxSpeed)
            : (useGeometry ? calculateFollowingSpeedWithGeometry(obstacles, caseIndex, staticOffset)
                           : caculateFollowingSpeed(obstacles, caseIndex));
        context.following=adaptiveFollowing;
        Obstacle selected; double distance=0;
        const bool found=calculateNearestObstacle(obstacles, selected, distance);
        *out={target, found ? selected.sRelativeToVehicle : 0, distance, found ? 1 : 0};
        return std::isfinite(target) && target >= 0 ? 1 : 0;
    } catch (...) { return 0; }
}
