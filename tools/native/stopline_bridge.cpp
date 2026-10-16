#include <vector>
#include <limits>
#include <cmath>
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
