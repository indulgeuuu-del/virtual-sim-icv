#include <vector>
#include <limits>
#include <cmath>
#include <new>
// @STOP_CONTROL@
// @TRAFFIC_QUERY@
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

extern "C" __declspec(dllexport) void* signal_create(double dwell) {
    if (!std::isfinite(dwell) || dwell < 0) return nullptr;
    return new(std::nothrow) StopLineControl::SignalTask(dwell);
}
extern "C" __declspec(dllexport) void signal_destroy(void* handle) {
    delete static_cast<StopLineControl::SignalTask*>(handle);
}
extern "C" __declspec(dllexport) void signal_clear(void* handle) {
    if(handle) static_cast<StopLineControl::SignalTask*>(handle)->clear();
}
extern "C" __declspec(dllexport) int signal_passed(void* handle) {
    return handle && static_cast<StopLineControl::SignalTask*>(handle)->passed() ? 1 : 0;
}
extern "C" __declspec(dllexport) int signal_step(void* handle,int token,double time,double distance,
    double front,double speed,double ceiling,double margin,int status,int ok,int relevant,
    SequenceOutput* output,int* signalOut) {
    if(!handle || !output || !signalOut) return 0;
    // Substitute only SDK fields/return value; the query body is production code.
    inputs.clear();SSD::SimVector<HDMapStandalone::MSignal> list;
    Status mapped=static_cast<Status>(99);
    if(status==1) mapped=Status::ESimOne_TrafficLight_Status_Red;
    if(status==2) mapped=Status::ESimOne_TrafficLight_Status_Green;
    if(status==3) mapped=Status::ESimOne_TrafficLight_Status_Yellow;
    if(status!=-1) { inputs.push_back({ok!=0,relevant!=0,mapped});list.push_back({0}); }
    HDMapStandalone::MSignal selected;bool known=false,found=false;
    const bool stop=getTrafficLightObservation(list,selected,known,found);
    using Signal=StopLineControl::Signal;
    const Signal signal=!known ? Signal::Unknown : !found ? Signal::Absent : stop ? Signal::Stop : Signal::Go;
    const auto r=static_cast<StopLineControl::SignalTask*>(handle)->update(
        token,signal,time,distance,front,speed,ceiling,margin);
    *signalOut=static_cast<int>(signal);
    *output={r.targetSpeed,r.heldSeconds,static_cast<int>(r.phase),r.ready ? 1 : 0};return 1;
}
