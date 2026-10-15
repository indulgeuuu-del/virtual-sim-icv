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
        else if(name=="heading_failed") inputs[1].heading=false;
        else if(name=="heading_nonfinite") inputs[1].finiteHeading=false;
        else if(name=="heading_zero") inputs[1].zeroHeading=true;
        else if(name=="link_partial") inputs[1].link=false;
        else if(name=="link_cleared") { inputs[1].link=false; inputs[1].clearLink=true; }
        else if(name=="link_initial") inputs[0].link=false;
        else if(name=="link_repeated") { inputs[0].link=false; inputs[1].link=false; }
        else if(name=="road_invalid") inputs[1].badRoad=true;
        else if(name=="road_overflow") inputs[1].largeRoad=true;
        else if(name=="successor_ambiguous") inputs[1].ambiguous=true;
        else if(name=="link_empty_valid") { for(auto& a:inputs) { a.noNeighbors=true; a.noSuccessors=true; } }
        else if(name=="junction_valid") { for(auto& a:inputs) a.junctionOnly=true; }
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
            if(input.gps && input.obstacle && input.route && mapValid(input)) {
                ++expectedDrives;
                require(sends[i].throttle>0,"fresh input failed to resume");
            } else {
                require(sends[i].throttle==0,"failed input still requests movement");
                require(sends[i].steering==0 && sends[i].throttleMode==ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed,
                        "invalid stop mode or stale steering");
                require(sends[i].gear==(mainVehicle.reverse ? ESimOne_Gear_Mode::ESimOne_Gear_Mode_Reverse
                        : ESimOne_Gear_Mode::ESimOne_Gear_Mode_Drive),"stop used wrong gear");
            }
            if(!mapValid(input)) {
                const MapState previous=i==0 ? MapState{} : mapStates[i-1];
                const auto& now=mapStates[i];
                require(now.x==previous.x && now.s==previous.s && now.t==previous.t &&
                        now.lane==previous.lane && now.lastLane==previous.lastLane && now.next==previous.next &&
                        now.heading==previous.heading && now.left==previous.left && now.right==previous.right &&
                        now.leftName==previous.leftName && now.rightName==previous.rightName && now.successors==previous.successors,
                        "failed map read partially overwrote valid vehicle state");
            }
            if(mapValid(input) && input.gps) {
                require(mapStates[i].left==!input.noNeighbors && !mapStates[i].right,"incorrect neighbor flags");
                require(mapStates[i].next==((input.noSuccessors || input.junctionOnly) ? "none" : "2_0_1"),
                        "valid successor semantics changed");
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
