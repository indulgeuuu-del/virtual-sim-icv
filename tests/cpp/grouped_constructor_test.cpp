// Appended to the complete constructor fixture. SDK/map remain substitutes.
// Actual grouping, distance and complete merged/single constructors are injected.
#include <unordered_set>
namespace UtilMath {
// @DISTANCE@
}
// @GROUPING@
using Groups=std::vector<std::vector<size_t>>;
Groups normalized(Groups groups) {
    for(auto& group:groups) std::sort(group.begin(),group.end());
    std::sort(groups.begin(),groups.end());return groups;
}
void check(const std::vector<size_t>& indices,const Groups& expected) {
    Groups groups={{5}}; // A previous frame's output must be discarded.
    groupObstacleByDist(indices,3.0f,groups);
    if(normalized(groups)!=normalized(expected)) throw std::runtime_error("wrong original indices or grouping");
    for(const auto& group:groups) {
        Obstacle result(group);
        // All test objects have length 4, width 2 and share the east-facing frame.
        double lo=1e9,hi=-1e9;
        for(auto index:group) {
            lo=std::min(lo,double(pObstacle->obstacle[index].posX)-2);
            hi=std::max(hi,double(pObstacle->obstacle[index].posX)+2);
        }
        if(result.width!=2 || result.length!=hi-lo || result.pt.x!=(hi+lo)/2
            || result.type!=Obstacle::Type::Static)
            throw std::runtime_error("group did not construct expected bounds");
    }
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("missing case");
        std::string name=argv[1];scenario="valid";
        pObstacle->obstacleSize=6;
        for(size_t i=0;i<6;++i) {
            pObstacle->obstacle[i].posX=float(100*i);
            pObstacle->obstacle[i].velX=0;
        }
        pObstacle->obstacle[0].posX=0;
        pObstacle->obstacle[1].posX=3;
        if(name=="exact") check({0,1},{{0,1}});
        else if(name=="below") {
            pObstacle->obstacle[1].posX=2.5f;check({0,1},{{0,1}});
        } else if(name=="above") {
            pObstacle->obstacle[1].posX=3.5f;check({0,1},{{0},{1}});
        } else if(name=="chain") {
            pObstacle->obstacle[2].posX=6;check({0,1,2},{{0,1,2}});
        } else if(name=="sparse" || name=="reordered") {
            pObstacle->obstacle[2].posX=10;
            pObstacle->obstacle[4].posX=12;
            check(name=="sparse" ? std::vector<size_t>{2,4,5} : std::vector<size_t>{5,4,2},{{2,4},{5}});
        } else if(name=="empty") {
            check({0,1},{{0,1}});check({},{});check({1},{{1}});
        } else if(name=="recovery") {
            Groups groups;groupObstacleByDist({0,1},3.0f,groups);
            scenario="width_failed";bool rejected=false;
            try { Obstacle result(groups.at(0)); } catch(const std::domain_error&) { rejected=true; }
            if(!rejected) throw std::runtime_error("invalid map accepted after grouping");
            scenario="valid";check({0,1},{{0,1}});
        } else throw std::runtime_error("unknown case");
        std::cout<<"PASS";return 0;
    } catch(const std::exception& e) { std::cerr<<e.what();return 1; }
}
