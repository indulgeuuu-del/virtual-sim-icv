#include "main.h"
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    try {
        require(argc==2,"test name required");
        const std::string name=argv[1];
        inputs={{},{},{}};
        if(name=="gps_initial") inputs[0].gps=false;
        else if(name=="obstacle_initial") { inputs[0].obstacle=false; frameCount=100; }
        else if(name=="gps_transient") inputs[1].gps=false;
        else if(name=="obstacle_stale") inputs[1].obstacle=false;
        else if(name=="obstacle_cleared") { inputs[1].obstacle=false; clearFailedOutput=true; }
        else if(name=="both_failed") { inputs[1].gps=false; inputs[1].obstacle=false; }
        else if(name=="send_failure") { inputs[1].gps=false; inputs[1].send=false; }
        else if(name=="gps_reverse") { inputs[1].gps=false; mainVehicle.reverse=true; }
        else if(name=="prolonged_failure") { inputs[0].obstacle=false; inputs[1].obstacle=false; }
        else if(name=="pid_recovery") { inputs[1].gps=false; followingKi=0.1f; }
        else if(name=="route_initial") inputs[0].route=false;
        else if(name=="route_repeated") { inputs[0].route=false; inputs[1].route=false; }
        else if(name=="empty_valid") { for(auto& input:inputs) input.count=0; }
        else if(name=="map_nearest") inputs[1].near=false;
        else if(name=="map_projection") inputs[1].projection=false;
        else if(name=="map_empty_lane") inputs[1].emptyLane=true;
        else if(name=="map_nonfinite") inputs[1].finiteProjection=false;
        else if(name=="map_bad_position") inputs[1].finitePosition=false;
        else if(name=="map_initial") inputs[0].projection=false;
        else if(name=="map_repeated") { inputs[0].projection=false; inputs[1].projection=false; }
        else throw std::runtime_error("unknown test");
        try {
#ifdef TEST_TRACK_PREFIX
            main_track();
#else
            main_follow();
#endif
            throw std::runtime_error("unexpected return");
        } catch(const EndFrames&) {}
        require(waits==advances && sends.size()==inputs.size(),"missing stop command or unpaired frame");
        require(mapStates.size()==inputs.size(),"missing map observations");
        int expectedDrives=0;
        for(size_t i=0;i<inputs.size();++i) {
            const auto& input=inputs[i];
            if(input.gps && input.obstacle && input.route && input.near && input.projection &&
                !input.emptyLane && input.finiteProjection && input.finitePosition) {
                ++expectedDrives;
                require(sends[i].throttle>0,"fresh input failed to resume");
            } else {
                require(sends[i].throttle==0,"failed input still requests movement");
                require(sends[i].steering==0 && sends[i].throttleMode==ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed,
                        "invalid stop mode or stale steering");
                require(sends[i].gear==(mainVehicle.reverse ? ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse
                        : ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive),"stop used wrong gear");
            }
            if(!input.near || !input.projection || input.emptyLane || !input.finiteProjection || !input.finitePosition) {
                const MapState previous=i==0 ? MapState{0,0,0,"",""} : mapStates[i-1];
                const auto& now=mapStates[i];
                require(now.x==previous.x && now.s==previous.s && now.t==previous.t &&
                        now.lane==previous.lane && now.lastLane==previous.lastLane,
                        "failed map read partially overwrote valid vehicle state");
            }
        }
        require(drives==expectedDrives,"invalid input reached drive or valid input not recovered");
        require(obstacleReads==3 && gpsReads==3,"one stream skipped after other failure");
        if(name!="empty_valid") require(!globalLogger.text.empty(),"failure has no diagnostic");
        if(name=="send_failure") require(globalLogger.text.find("SetDrive")!=std::string::npos,"stop send failure not diagnosed");
        if(name=="pid_recovery") require(sends[0].throttle==sends[2].throttle,"PID history retained across invalid input");
        if(name=="route_initial") require(navigationCalls==2,"route not retried or rebuilt after success");
        if(name=="route_repeated") require(navigationCalls==3,"route retries did not continue");
        std::cout<<"PASS "<<name<<'\n'; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
