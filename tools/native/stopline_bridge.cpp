#include <vector>
#include <limits>
#include <cmath>
#include <new>
// @STOP_CONTROL@
struct StopLine {
    enum class Type { Whole,Single } type=Type::Whole;
    enum class Strategy { StopStart,LaneChange } strategy=Strategy::StopStart;
    bool isValid=true,isBehind=false,isVechicleInSameLane=true;
    float toVehicleDist=0,offset=0;
};
// @NEAREST@
extern "C" __declspec(dllexport) double stopline_step(double distance,double speed,double cruise,double margin) {
    if(!std::isfinite(distance) || !std::isfinite(speed) || !std::isfinite(cruise) || !std::isfinite(margin)
        || speed<0 || cruise<0 || margin<0) return 0;
    struct { float speed; } mainVehicle{static_cast<float>(speed)};
    struct Control { double throttle; } control{cruise};
    auto* pControl=&control;
    float stopLineDistThres=static_cast<float>(margin);
    StopLine line;line.toVehicleDist=static_cast<float>(distance);
    // A crossed fixed stop line remains a stop obligation in this experiment.
    StopLine potentialStopLine;float minDistance;
    if(calculateNearestStopLine({line},potentialStopLine,minDistance)) {
// @BRAKING@
    }
    return control.throttle;
}

extern "C" __declspec(dllexport) double stopline_explicit(double distance,double speed,
    double cruise,double margin,double front,int approach) {
    if(approach == 1)
        return StopLineControl::approachTarget(distance,0,front,speed,cruise,margin);
    if(approach != 0) return 0;
    return StopLineControl::legacyTarget(static_cast<float>(distance),0,front,
        static_cast<float>(speed),cruise,static_cast<float>(margin));
}

struct SequenceOutput { double target, held; int phase, ready; };
extern "C" __declspec(dllexport) void* sequence_create(double dwell) {
    if (!std::isfinite(dwell) || dwell < 0) return nullptr;
    return new(std::nothrow) StopLineControl::Sequence(dwell);
}
extern "C" __declspec(dllexport) void sequence_destroy(void* handle) {
    delete static_cast<StopLineControl::Sequence*>(handle);
}
extern "C" __declspec(dllexport) int sequence_step(void* handle,double time,double distance,
    double front,double speed,double ceiling,double margin,int permission,int observed,SequenceOutput* output) {
    if (!handle || !output) return 0;
    const auto r=static_cast<StopLineControl::Sequence*>(handle)->update(
        time,distance,front,speed,ceiling,margin,permission!=0,observed!=0);
    *output={r.targetSpeed,r.heldSeconds,static_cast<int>(r.phase),r.ready ? 1 : 0};
    return 1;
}
