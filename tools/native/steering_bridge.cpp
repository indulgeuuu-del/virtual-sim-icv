#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <mutex>
#include <new>
#include <vector>
#include "controller.hpp"
namespace SSD {
struct SimPoint3D { double x,y,z; };
using SimPoint3DVector=std::vector<SimPoint3D>;
}
struct SimOne_Data_Gps { double posX,posY,oriZ,velX,velY,velZ; };
static size_t ppTargetIndex=0;
static std::mutex stepMutex;
class UtilDriver {
public:
// @PRODUCTION_PURSUIT@
};
struct SteeringOutput { double raw,command; int nearest,target; };
extern "C" __declspec(dllexport) int steering_abi_version() { return 1; }
extern "C" __declspec(dllexport) void* steering_create(float kp,float ki,float kd) {
    if(!std::isfinite(kp)||!std::isfinite(ki)||!std::isfinite(kd)) return nullptr;
    return new(std::nothrow) PositionalPID(kp,ki,kd);
}
extern "C" __declspec(dllexport) void steering_destroy(void* handle) { delete static_cast<PositionalPID*>(handle); }
extern "C" __declspec(dllexport) int steering_step(void* handle,const double* points,int count,
    double x,double y,double yaw,double speed,SteeringOutput* out) {
    if(!handle||!points||!out||count<2||count>10000||speed<0||
        !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(yaw)||!std::isfinite(speed)) return 0;
    try {
        SSD::SimPoint3DVector path;
        for(int i=0;i<count;++i) {
            const double* p=points+3*i;
            if(!std::isfinite(p[0])||!std::isfinite(p[1])||!std::isfinite(p[2])) return 0;
            path.push_back({p[0],p[1],p[2]});
        }
        SimOne_Data_Gps gps{x,y,yaw,speed*std::cos(yaw),speed*std::sin(yaw),0};
        size_t nearest=0;
        std::lock_guard<std::mutex> lock(stepMutex);
        const double raw=UtilDriver::calculateSteering(path,&gps,nearest);
        if(!std::isfinite(raw)) return 0;
        auto* pid=static_cast<PositionalPID*>(handle);
        const double command=pid->calculate(static_cast<float>(raw));
        if(!std::isfinite(command)) { pid->reset(); return 0; }
        *out={raw,command,static_cast<int>(nearest),static_cast<int>(ppTargetIndex)};
        return 1;
    } catch(...) { return 0; }
}
