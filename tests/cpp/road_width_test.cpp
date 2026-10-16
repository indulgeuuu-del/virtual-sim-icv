#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <stdexcept>
#include <iostream>
namespace SSD {
struct SimString { std::string s; SimString(const char* p=""):s(p) {} bool Empty() const { return s.empty(); } bool operator<(const SimString& b) const { return s<b.s; } };
struct SimPoint3D { double x=0,y=0,z=0; };
}
namespace HDMapStandalone { struct MLaneLink { SSD::SimString leftNeighborLaneName,rightNeighborLaneName; }; }
std::string mode;
int reads=0;
namespace SimOneAPI {
bool GetLaneWidth(const SSD::SimString& lane,const SSD::SimPoint3D&,double& w) {
    ++reads;
    if(reads==1 && mode=="first_unwritten") return true;
    if(mode=="unwritten") return true;
    w=lane.s=="left" ? 2 : lane.s=="right" ? 4 : 3;
    if(reads==1 && mode=="first_failed") return false;
    if(reads==1 && mode=="first_nan") { w=std::nan(""); return true; }
    if(mode=="nan") w=std::nan("");
    if(mode=="inf") w=std::numeric_limits<double>::infinity();
    if(mode=="negative") w=-1;
    if(mode=="zero") w=0;
    if(mode=="overflow") w=std::numeric_limits<double>::max();
    return mode!="failed" && !(mode=="partial" && lane.s=="right");
}
bool GetLaneLink(const SSD::SimString& lane,HDMapStandalone::MLaneLink& link) {
    link={};
    if(mode!="single" && lane.s=="lane") { link.leftNeighborLaneName="left"; link.rightNeighborLaneName="right"; }
    // Normal reciprocal links must not double-count or be rejected as cycles.
    if(lane.s=="left") link.rightNeighborLaneName="lane";
    if(lane.s=="right") link.leftNeighborLaneName="lane";
    return mode!="link_failed";
}
}
// @ROAD_WIDTH@
double rawWidth() { SSD::SimString laneID("lane"); SSD::SimPoint3D pt; double laneWidth=77,roadWidth=88;
// @RAW_WIDTH@
return laneWidth;
}
double groupWidth() { SSD::SimString laneID("lane"); SSD::SimPoint3D pt; double laneWidth=77,roadWidth=88;
// @GROUP_WIDTH@
return laneWidth;
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("missing case");
        std::string name=argv[1]; auto split=name.find('_');
        std::string kind=name.substr(0,split); mode=name.substr(split+1);
        auto run=[&]() {
            if(kind=="raw") return rawWidth();
            if(kind=="group") return groupWidth();
            SSD::SimPoint3D p; if(mode=="point_nan") p.x=std::nan("");
            return getRoadWidth(mode=="empty" ? "" : "lane",p);
        };
        bool rejected=false; double result=0;
        try { result=run(); } catch(const std::domain_error&) { rejected=true; }
        if(mode=="valid" || mode=="single") {
            if(rejected || result!=(kind=="road" && mode=="valid" ? 9 : 3)) throw std::runtime_error("wrong valid width");
            if(kind=="road" && mode=="valid" && reads!=3) throw std::runtime_error("duplicate lane count");
        } else {
            if(!rejected) throw std::runtime_error("incomplete or invalid width accepted");
            mode="valid"; if(run()!=(kind=="road" ? 9 : 3)) throw std::runtime_error("recovery failed");
        }
        std::cout<<"PASS"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what(); return 1; }
}
