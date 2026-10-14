// Compile the exact production update() prefix through GetGps/initialization.
// Reaching geometry/data processing is represented by ++processed; no SDK claim.
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
enum class ESimOne_Gear_Mode { ESimOne_Gear_Mode_Drive };
enum class ESimOne_Throttle_Mode { ESimOne_Throttle_Mode_Speed };
struct Control {
    double throttle=0, steering=0;
    bool handbrake=false, isManualGear=false;
    ESimOne_Gear_Mode gear{};
    ESimOne_Throttle_Mode throttleMode{};
};
struct Lights { int signalLights=0; };
struct Gps {};
auto pControl=std::make_unique<Control>();
auto pLight=std::make_unique<Lights>();
auto pGps=std::make_unique<Gps>();
int frameCount=1, presetLight=0, processed=0;
double caseTargetSpeed=8;
bool gpsOk=false;
namespace FlagType { bool isMainVehicleInitialized=false; }
struct Logger {
    enum class Color { BrightMagenta };
    Logger& operator()(Color) { return *this; }
    template<class T> Logger& operator<<(const T&) { return *this; }
} globalLogger;
namespace SimOneAPI { bool GetGps(const char*, Gps*) { return gpsOk; } }
struct MainVehicle {
    const char* id="0";
    struct { void clear() {} } neighborhood;
    bool update(int timeoutFrames=10);
};
// @GPS_PREFIX@
void require(bool ok, const char* text) { if(!ok) throw std::runtime_error(text); }
int main(int argc,char** argv) {
    try {
        require(argc==2,"test name required");
        const std::string name=argv[1];
        MainVehicle vehicle;
        if(name=="late_failure" || name=="transient_failure") frameCount=100;
        if(name=="transient_failure") FlagType::isMainVehicleInitialized=true;
        if(name=="success") gpsOk=true;
        const bool result=vehicle.update();
        if(gpsOk) require(result && processed==1 && FlagType::isMainVehicleInitialized,"successful GPS rejected");
        else {
            require(!result && processed==0,"failed GPS reached stale data processing or reported success");
            if(name!="transient_failure") require(!FlagType::isMainVehicleInitialized,"failed GPS marked initialized");
        }
        if(name=="recovery") {
            gpsOk=true; ++frameCount;
            require(vehicle.update() && processed==1,"fresh GPS did not recover");
        }
        std::cout<<"PASS "<<name<<'\n'; return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL "<<argv[1]<<": "<<e.what()<<'\n'; return 1;
    }
}
