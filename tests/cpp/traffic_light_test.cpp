#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace SSD { template<class T> using SimVector=std::vector<T>; }
namespace HDMapStandalone { struct MSignal { int id=0; }; }
enum class ESimOne_TrafficLight_Status { ESimOne_TrafficLight_Status_Red, ESimOne_TrafficLight_Status_Yellow, ESimOne_TrafficLight_Status_Green };
using Status=ESimOne_TrafficLight_Status;
struct SimOne_Data_TrafficLight { bool isMainVehicleNextTrafficLight=false; Status status=Status::ESimOne_TrafficLight_Status_Green; };
struct { const char* id="0"; } mainVehicle;
struct Logger {
    enum class Color { BrightGreen };
    Logger& operator()(Color) { return *this; }
    template<class T> Logger& operator<<(const T&) { return *this; }
} globalLogger;
struct Input { bool ok=true,next=true; Status status=Status::ESimOne_TrafficLight_Status_Green; };
std::vector<Input> inputs;
namespace SimOneAPI {
bool GetTrafficLight(const char*,int id,SimOne_Data_TrafficLight* out) {
    const auto& input=inputs.at(id);
    if(!input.ok) return false; // failure leaves prior output untouched
    *out={input.next,input.status}; return true;
}
}
// @TRAFFIC_LIGHT@
void require(bool ok,const char* text) { if(!ok) throw std::runtime_error(text); }
int main(int argc,char** argv) {
    try {
        require(argc==2,"name required"); std::string name=argv[1];
        inputs={{true,true,Status::ESimOne_TrafficLight_Status_Green}};
        bool expectedStop=false,expectedKnown=true;
        if(name=="red") { inputs[0].status=Status::ESimOne_TrafficLight_Status_Red; expectedStop=true; }
        else if(name=="yellow") { inputs[0].status=Status::ESimOne_TrafficLight_Status_Yellow; expectedStop=true; }
        else if(name=="failure") { inputs[0].ok=false; expectedKnown=false; }
        else if(name=="green_then_failure") { inputs.push_back({false,true,Status::ESimOne_TrafficLight_Status_Red}); expectedKnown=false; }
        else if(name=="failure_then_red") { inputs[0].ok=false; inputs.push_back({true,true,Status::ESimOne_TrafficLight_Status_Red}); expectedKnown=false; expectedStop=true; }
        else if(name=="unrelated_red") { inputs[0].next=false; inputs[0].status=Status::ESimOne_TrafficLight_Status_Red; }
        else if(name=="empty") inputs.clear();
        else require(name=="green","unknown name");
        SSD::SimVector<HDMapStandalone::MSignal> list;
        for(size_t i=0;i<inputs.size();++i) list.push_back({int(i)});
        HDMapStandalone::MSignal light{-1}; bool known=true;
        const bool stop=READ_LIGHT(list,light,known);
        require(stop==expectedStop,"wrong stopping light");
        require(known==expectedKnown,"failed traffic-light read treated as known clear");
        if(stop) require(inputs.at(light.id).status!=Status::ESimOne_TrafficLight_Status_Green,"wrong light selected");
        if(name=="failure") {
            inputs[0].ok=true; known=false;
            require(!READ_LIGHT(list,light,known) && known,"fresh green did not recover");
        }
        std::cout<<"PASS "<<name<<'\n'; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
