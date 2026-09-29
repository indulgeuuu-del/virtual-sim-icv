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
const Observation& obs(size_t i) {
    require(i < observations.size(), "missing observation");
    return observations[i];
}
void requireDrove(size_t i, double targetX, double speed, const char* message) {
    require(obs(i).drove && obs(i).targetX == targetX && obs(i).speed == speed, message);
}
void requireStopped(size_t i, const char* message) {
    require(!obs(i).drove && obs(i).speed == 0, message);
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
            require(samples == (name == "interpolation" ? 1 : 0), "wrong path builder used");
            require(logText.find("失败") == std::string::npos, "successful build logged as failure");
            for (size_t i = 0; i < 3; ++i) {
                requireDrove(i, 100, defaults ? 8 : 6, "incorrect speed or target");
                require(obs(i).kp == (defaults ? 1.2 : 0.7), "incorrect kp");
            }
        } else if (name == "transition") {
            strategyPoint.push_back(point(0));
            strategyPoint.push_back(point(100));
            frameLimit = 3;
            runFrames();
            require(routes == 2 && obs(0).targetX == 0 &&
                    obs(1).targetX == 100 && obs(2).targetX == 100,
                    "first point skipped or subsequent transition broken");
        } else if (name == "transition_parameters") {
            strategyPoint.push_back(point(0, 6, 0.7));
            strategyPoint.push_back(point(100, 9, 2.0));
            frameLimit = 2;
            runFrames();
            requireDrove(0, 0, 6, "first point parameters");
            requireDrove(1, 100, 9, "switch frame kept the previous point's speed");
            require(obs(1).kp == 2.0, "switch frame kept the previous point's kp");
        } else if (name == "first_route_failure" || name == "interpolation_failure") {
            strategyPoint.push_back(point(100, -1, -1, -1, name == "interpolation_failure" ? 1 : -1));
            failingRoutes = {1};
            frameLimit = 3;
            runFrames();
            requireStopped(0, "drove without a valid first path");
            requireDrove(1, 100, 8, "first path not retried");
            requireDrove(2, 100, 8, "retry did not persist");
            require(routes == 2, "path rebuilt after success");
            require(logText.find("重试") != std::string::npos, "failure not diagnosed");
        } else if (name == "transition_route_failure") {
            strategyPoint.push_back(point(0));
            strategyPoint.push_back(point(100));
            failingRoutes = {2};
            frameLimit = 4;
            runFrames();
            requireDrove(0, 0, 8, "first point");
            requireDrove(1, 0, 0, "failed switch must keep old path and stop");
            requireDrove(2, 100, 8, "switch not retried");
            requireDrove(3, 100, 8, "retried switch did not persist");
            require(routes == 3, "unexpected route calls");
        } else if (name == "fractional_stop") {
            strategyPoint.push_back(point(0, 6, -1, 0.5));
            frameLimit = 7;
            runFrames();
            for (size_t i = 0; i < 5; ++i)
                require(obs(i).speed == 0, "resumed before 0.5 seconds");
            require(obs(5).speed == 6 && obs(6).speed == 6,
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
            require(routes == 1 && obs(0).targetX == 200, "stale index on reentry");
        } else throw std::runtime_error("unknown test");
        std::cout << "PASS " << name << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << argv[1] << ": " << e.what() << '\n';
        return 1;
    }
}
