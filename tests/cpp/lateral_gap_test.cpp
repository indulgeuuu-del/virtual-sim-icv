#include "lane_distance_support.hpp"
#include <vector>
template<class T> struct CheckedVector: std::vector<T> {
    using std::vector<T>::vector;
    T& back() { return this->at(this->size()-1); }
    const T& back() const { return this->at(this->size()-1); }
};
// Checked back() turns legacy empty-vector UB into a deterministic failure.
struct Obstacle { CheckedVector<SimOne_Data_Vec3f> predictionGM; };
int main(int argc,char** argv) {
    try {
        require(argc==2,"test name required");
        const std::string name=argv[1];
        CheckedVector<SSD::SimPoint3D> targetPath{{0,0,0},{0,0,0}};
        Obstacle obstacle; obstacle.predictionGM.push_back({10,2,0});
        bool lateralProjectionFailed=false;
        if(name=="negative_far") obstacle.predictionGM.back().y=-2;
        else if(name=="positive_near") obstacle.predictionGM.back().y=1;
        else if(name=="negative_near") obstacle.predictionGM.back().y=-1;
        else if(name=="different_road") differentRoad=true;
        else if(name=="missing_lane") missingLane=true;
        else if(name=="current_failure") failRead=1;
        else if(name=="target_failure") failRead=2;
        else if(name=="empty_prediction") obstacle.predictionGM.clear();
        else if(name=="empty_path") targetPath.clear();
        else require(name=="positive_far","unknown test");
// @PREDICTED_GAP@
        const float gap=predictedLateralGap(obstacle);
        const bool invalid=missingLane || failRead || obstacle.predictionGM.empty() || targetPath.empty();
        require(lateralProjectionFailed==invalid,"missing/invalid prediction not propagated");
        if(!invalid) {
            const bool near=name=="positive_near" || name=="negative_near";
            require((gap<1.1)==near,"signed lateral offset treated as closeness");
        }
        std::cout<<"PASS "<<name<<'\n'; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
