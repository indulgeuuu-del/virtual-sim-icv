#include "main.h"
int main_manual();

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void runFrames() {
    try { main_manual(); throw std::runtime_error("unexpected return"); }
    catch (const EndFrames&) {}
    require(waits == advances, "Wait/NextFrame mismatch");
}
StrategyPoint point(double x, double speed = -1, double kp = -1,
                    double stop = -1, int mode = -1) {
    return {{x, 0, 0}, mode, stop, speed, kp};
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "test name required");
        std::string name = argv[1];
        if (name == "empty") {
            require(main_manual() != 0, "empty strategy accepted");
            require(waits == 0 && observations.empty(), "empty strategy entered loop");
        } else if (name == "defaults" || name == "explicit" || name == "interpolation") {
            bool defaults = name == "defaults";
            strategyPoint.push_back(point(100, defaults ? -1 : 6, defaults ? -1 : 0.7,
                                          -1, name == "interpolation" ? 1 : -1));
            frameLimit = 3;
            runFrames();
            require(routes == 1 && observations.size() == 3, "first path not built exactly once");
            for (const auto& row : observations) {
                require(row.speed == (defaults ? 8 : 6), "incorrect speed");
                require(row.kp == (defaults ? 1.2 : 0.7), "incorrect kp");
                require(row.targetX == 100, "incorrect target");
            }
        } else if (name == "transition") {
            strategyPoint.push_back(point(0));
            strategyPoint.push_back(point(100));
            frameLimit = 3;
            runFrames();
            require(routes == 2 && observations[0].targetX == 0 &&
                    observations[1].targetX == 100 && observations[2].targetX == 100,
                    "first point skipped or subsequent transition broken");
        } else if (name == "fractional_stop") {
            strategyPoint.push_back(point(0, 6, -1, 0.5));
            frameLimit = 7;
            runFrames();
            for (int i = 0; i < 5; ++i)
                require(observations.at(i).speed == 0, "resumed before 0.5 seconds");
            require(observations[5].speed == 6 && observations[6].speed == 6,
                    "did not resume or stopped twice");
        } else if (name == "reentry") {
            strategyPoint.push_back(point(0));
            strategyPoint.push_back(point(100));
            frameLimit = 2;
            runFrames();
            strategyPoint.clear();
            strategyPoint.push_back(point(200, 5));
            waits = advances = 0; routes = 0; observations.clear();
            frameLimit = 1;
            runFrames();
            require(routes == 1 && observations[0].targetX == 200, "stale index on reentry");
        } else throw std::runtime_error("unknown test");
        std::cout << "PASS " << name << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << argv[1] << ": " << e.what() << '\n';
        return 1;
    }
}
