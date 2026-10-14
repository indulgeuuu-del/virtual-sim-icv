// Prefix; runner inserts exact production corner functions and merge geometry.
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace SSD {
struct SimPoint3D {
    double x = 0, y = 0, z = 0;
    SimPoint3D() = default;
    SimPoint3D(double a, double b, double c) : x(a), y(b), z(c) {}
};
}
struct SimOne_Data_Obstacle_Entry { double posX, posY, posZ, length, width; };
struct ObstacleData { std::vector<SimOne_Data_Obstacle_Entry> obstacle; };
auto pObstacle = std::make_unique<ObstacleData>();
struct Vehicle { float laneAzimuth = 0; } mainVehicle;
// Map lookup fixture: all objects and ego are on the nominated straight lane.
int m_SampleGetNearMostLane(const SSD::SimPoint3D&) { return 1; }
float getLaneAzimuth(int) { return mainVehicle.laneAzimuth; }
struct Box {
    SSD::SimPoint3D tl, tr, bl, br, pt;
    double width = 0, length = 0, velocity = 0, velocityPlanar = 0;
    double vx = 0, vy = 0, vz = 0, yaw = 0;
};
// @PRODUCTION_GEOMETRY@
void require(bool value, const char* text) {
    if (!value) throw std::runtime_error(text);
}
bool close(double a, double b) { return std::abs(a-b) < 1e-6; }
void same(const SSD::SimPoint3D& a, const SSD::SimPoint3D& b) {
    require(close(a.x,b.x) && close(a.y,b.y) && close(a.z,b.z), "wrong corner/center");
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "angle index required");
        // Expected forward directions are explicit fixtures at every inherited
        // quadrant boundary, not a second copy of production classification.
        struct Fixture { float angle; int fx, fy; };
        const Fixture cases[] = {
            {0,1,0}, {45,1,0}, {45.001f,0,1}, {90,0,1}, {135,0,1},
            {135.001f,-1,0}, {180,-1,0}, {225,-1,0}, {225.001f,0,-1},
            {270,0,-1}, {315,0,-1}, {315.001f,1,0}, {360,1,0}
        };
        const int n = std::stoi(argv[1]);
        require(n >= 0 && n < 13, "invalid fixture");
        const auto f = cases[n]; mainVehicle.laneAzimuth = f.angle;
        for (double origin : {100.0, -200.0}) {
            // Two boxes: union is 11m forward by 5m lateral; deliberately
            // asymmetric so swapped length/width and sign errors cannot pass.
            auto point = [&](double forward, double left) {
                return SSD::SimPoint3D(origin + forward*f.fx - left*f.fy,
                                      200 + forward*f.fy + left*f.fx, 3);
            };
            const auto a=point(0,0), b=point(6,2);
            pObstacle->obstacle = {{a.x,a.y,a.z,4,2}, {b.x,b.y,b.z,6,4}};
            for (const auto& indices : {std::vector<size_t>{0,1}, std::vector<size_t>{1,0}}) {
                const auto box = mergeGeometry(indices);
                require(close(box.width,5) && close(box.length,11), "wrong merged width/length");
                same(box.tl, point(9,4)); same(box.tr, point(9,-1));
                same(box.bl, point(-2,4)); same(box.br, point(-2,-1));
                same(box.pt, point(3.5,1.5));
            }
        }
        std::cout << "PASS angle=" << f.angle << " (translated and reordered)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << argv[1] << ": " << e.what() << '\n'; return 1;
    }
}
