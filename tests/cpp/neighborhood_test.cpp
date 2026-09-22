// Included after the production neighborhood declarations and method bodies.
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
Obstacle object(int id, double s, double t, float speed = 5, int road = 1) {
    return {id, road, s, t, speed};
}
void frame(std::initializer_list<Obstacle> objects) {
    // Only the list/classification scheduling is modeled, not the entire driving loop.
    if (legacyOrder) mainVehicle.rebuildNeighborhood();
    else mainVehicle.neighborhood.clear();
    obstacleList.assign(objects);
    if (!legacyOrder) mainVehicle.rebuildNeighborhood();
}
int id(const Obstacle* p) { return p ? p->id : -1; }
int main(int argc, char** argv) {
    try {
        require(argc == 2, "test name required");
        std::string name = argv[1];
        auto& n = mainVehicle.neighborhood;
        if (name == "first_frame") {
            frame({object(11, 20, 0)});
            require(id(n.findSlowestMovingObstacle(n.front)) == 11, "new front object missing");
        } else if (name == "disappear") {
            frame({object(11, 20, 0)});
            frame({});
            require(id(n.findSlowestMovingObstacle(n.front)) == -1, "removed object retained");
        } else if (name == "shrink") {
            frame({object(11, -20, 0), object(22, 20, 0)});
            frame({object(22, 20, 0)});
            require(id(n.findSlowestMovingObstacle(n.front)) == 22, "index not remapped");
            require(n.back.empty(), "removed rear object retained");
        } else if (name == "reorder") {
            frame({object(11, 20, 0), object(22, -20, 4)});
            frame({object(22, -20, 4), object(11, 20, 0)});
            require(id(n.findSlowestMovingObstacle(n.front)) == 11, "front identity changed");
            require(id(n.findFastestMovingObstacle(n.leftBack)) == 22, "rear identity changed");
        } else if (name == "speed_change") {
            frame({object(11, 20, 0, 10), object(22, 30, 0, 2)});
            frame({object(11, 20, 0, 1), object(22, 30, 0, 12)});
            require(id(n.findSlowestMovingObstacle(n.front)) == 11, "stale speed ordering");
            require(id(n.findFastestMovingObstacle(n.front)) == 22, "stale fastest object");
        } else if (name == "region_change") {
            frame({object(11, -20, 4)});
            frame({object(11, 20, 4)});
            require(n.leftBack.empty() && id(n.findSlowestMovingObstacle(n.leftFront)) == 11,
                    "object did not move from rear to front");
        } else if (name == "all_regions") {
            frame({object(1,20,0),object(2,-20,0),object(3,0,4),object(4,0,-4),
                   object(5,20,4),object(6,20,-4),object(7,-20,4),object(8,-20,-4),
                   object(9,20,0,5,2)});
            std::vector<std::vector<size_t>*> groups = {&n.front,&n.back,&n.left,&n.right,
                &n.leftFront,&n.rightFront,&n.leftBack,&n.rightBack};
            for (size_t i=0; i<groups.size(); ++i) {
                require(groups[i]->size()==1, "region or road filter changed");
                require(obstacleList[groups[i]->at(0)].id==static_cast<int>(i+1), "wrong region");
            }
        } else if (name == "stationary_and_reset") {
            frame({object(1,20,0,0),object(2,30,0,6)});
            n.vf = 99; n.bf = true;
            frame({object(1,20,0,0)});
            require(id(n.findSlowestMovingObstacle(n.front)) == -1, "stationary object selected as moving");
            require(!n.bf && n.vf==0, "decision state carried across frames");
        } else if (name == "boundaries") {
            const double s = 0.5f*(MAIN_VEHICLE_LENGTH+1.0f);
            const double t = 0.5f*(MAIN_VEHICLE_WIDTH+1.0f);
            frame({object(1,s,t),object(2,s+0.01,t),object(3,s,t+0.01)});
            require(n.front.size()==1 && obstacleList[n.front[0]].id==2, "longitudinal boundary changed");
            require(n.left.size()==1 && obstacleList[n.left[0]].id==3, "lateral boundary changed");
        } else throw std::runtime_error("unknown test");
        std::cout << "PASS " << name << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << argv[1] << ": " << e.what() << '\n';
        return 1;
    }
}
