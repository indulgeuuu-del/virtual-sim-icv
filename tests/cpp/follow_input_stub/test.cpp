#include "main.h"
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    try {
        require(argc==2,"test name required");
        const std::string name=argv[1];
        inputs={{},{},{}};
        if(name.rfind("obstacle_heading_",0)==0) {
            if(name=="obstacle_heading_failed") inputs[1].obstacleHeading=1;
            else if(name=="obstacle_heading_zero") inputs[1].obstacleHeading=2;
            else if(name=="obstacle_heading_nan") inputs[1].obstacleHeading=3;
            else if(name=="obstacle_heading_inf") inputs[1].obstacleHeading=4;
            else if(name=="obstacle_heading_empty") inputs[1].obstacleHeading=5;
            else if(name=="obstacle_heading_initial") inputs[0].obstacleHeading=1;
            else if(name=="obstacle_heading_repeated") { inputs[0].obstacleHeading=1; inputs[1].obstacleHeading=1; }
            else if(name=="obstacle_heading_static") { inputs[1].obstacleHeading=1; inputs[1].rawField=3; inputs[1].rawValue=0; }
            else if(name=="obstacle_heading_reverse") { inputs[1].obstacleHeading=1; mainVehicle.reverse=true; }
            else if(name=="obstacle_heading_send_failure") { inputs[1].obstacleHeading=1; inputs[1].send=false; }
            else if(name=="obstacle_heading_pid") { inputs[1].obstacleHeading=1; followingKi=0.1f; }
            else throw std::runtime_error("unknown heading test");
        }
        else if(name.rfind("raw_",0)==0) {
            const float nan=std::numeric_limits<float>::quiet_NaN();
            if(name=="raw_initial") { inputs[0].rawValue=nan; }
            else if(name=="raw_repeated") { inputs[0].rawValue=nan; inputs[1].rawValue=nan; }
            else if(name=="raw_second") { inputs[1].count=2; inputs[1].rawIndex=1; inputs[1].rawValue=nan; }
            else if(name=="raw_unused") { inputs[1].rawIndex=1; inputs[1].rawValue=nan; }
            else if(name=="raw_signed") { for(auto& a:inputs) { a.rawField=3; a.rawValue=-2; } }
            else {
                const std::string fields[]={"posX","posY","posZ","velX","velY","velZ","oriZ","width","length"};
                bool found=false;
                for(int i=0;i<9;++i) {
                    for(const auto& kind: {std::string("nan"),std::string("inf"),std::string("neginf")}) {
                        if(name=="raw_"+fields[i]+"_"+kind) {
                            inputs[1].rawField=i;
                            inputs[1].rawValue=kind=="nan" ? nan : (kind=="inf" ? 1 : -1)*std::numeric_limits<float>::infinity();
                            found=true;
                        }
                    }
                }
                require(found,"unknown raw test");
            }
        }
        else if(name=="count_negative") inputs[1].count=-1;
        else if(name=="count_overflow") inputs[1].count=3;
        else if(name=="count_extreme") inputs[1].count=2147483647;
        else if(name=="count_initial") inputs[0].count=-1;
        else if(name=="count_repeated") { inputs[0].count=3; inputs[1].count=3; }
        else if(name=="count_capacity") { for(auto& a:inputs) a.count=2; }
        else if(name=="gps_initial") inputs[0].gps=false;
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
        // Prove rejection before executing a loop with malformed counts. Old
        // revisions fail here, rather than performing an out-of-bounds read.
        if(name.rfind("count_",0)==0 && name!="count_capacity") {
            waits = name=="count_initial" || name=="count_repeated" ? 1 : 2;
            require(!updateObstacleData(),"successful SDK read accepted malformed obstacle count");
            waits=0; obstacleReads=0;
            FlagType::isObstacleInitialized=false;
        }
        if(name.rfind("raw_",0)==0 && name!="raw_unused" && name!="raw_signed") {
            waits=name=="raw_initial" || name=="raw_repeated" ? 1 : 2;
            require(!updateObstacleData(),"successful SDK read accepted nonfinite obstacle field");
            waits=0; obstacleReads=0;
            FlagType::isObstacleInitialized=false;
        }
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
            const bool rawValid=input.rawIndex>=input.count || std::isfinite(input.rawValue);
            if(input.gps && input.obstacle && input.count>=0 && input.count<=2 && rawValid && input.route && mapValid(input) && input.obstacleHeading==0) {
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
        if(name=="obstacle_heading_send_failure") require(globalLogger.text.find("SetDrive")!=std::string::npos,"map failure stop send failure not diagnosed");
        if(name=="obstacle_heading_pid") require(sends[0].throttle==sends[2].throttle,"PID history retained across obstacle map failure");
        if(name=="pid_recovery") require(sends[0].throttle==sends[2].throttle,"PID history retained across invalid input");
        if(name=="route_initial") require(navigationCalls==2,"route not retried or rebuilt after success");
        if(name=="route_repeated") require(navigationCalls==3,"route retries did not continue");
        std::cout<<"PASS "<<name<<'\n'; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
