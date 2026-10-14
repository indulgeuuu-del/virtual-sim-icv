// Included by the real-drive build of manual_stub/test_main.cpp.
// Engineering invariant supporting 2027 scoring PDF physical p11, table 3:
// a zero target speed must reach SetDrive even during reverse/sliding logic.
// This does not assert physical standstill, stopping distance, or official score.
bool driveOutputTest(const std::string& name) {
    using Gear = ESimOne_Gear_Mode;
    using Mode = ESimOne_Throttle_Mode;
    if (name.rfind("output_", 0) != 0) return false;
    targetPath = {{0, 0, 0}, {10, 1, 0}, {20, 2, 0}};
    mainVehicle.update();
    steerKpUse = steerKp;
    steerKdUse = steerKd;
    pLight->signalLights = 4;
    if (name == "output_route_failure_reverse" || name == "output_route_failure_slide") {
        // Same loop -> real drive -> captured SDK boundary; old path remains valid.
        strategyPoint = CheckedPoints{};
        strategyPoint.push_back(point(0));
        strategyPoint.push_back(point(100));
        mainVehicle.reverse = name == "output_route_failure_reverse";
        if (!mainVehicle.reverse) slideConfig = {{{0, 0, 0}, {50, 0, 0}}};
        failingRoutes = {2}; frameLimit = 4;
        runFrames();
        require(obs(1).speed == 0, "route failure stop overwritten inside real drive");
        require(obs(1).gear == (mainVehicle.reverse ? Gear::ESimOne_Gear_Mode_Reverse
                                                   : Gear::ESimOne_Gear_Mode_Drive),
                "route failure stop sent in neutral");
        require(obs(1).targetX == 0 && obs(2).targetX == 100 && routes == 3,
                "failure/retry changed the retained path");
        require(obs(2).speed == (mainVehicle.reverse ? 3 : 8), "retry did not resume");
        require(observations.size() == 4 && lightSends == 4, "missing final commands");
        return true;
    }
    const bool slide = name == "output_stop_slide" || name == "output_stop_both" ||
                       name == "output_move_slide" || name == "output_percent_zero" ||
                       name == "output_resume_slide";
    mainVehicle.reverse = name == "output_stop_reverse" || name == "output_stop_both" ||
                          name == "output_move_reverse";
    if (slide) slideConfig = {{{0, 0, 0}, {50, 0, 0}}};
    const bool move = name == "output_move_reverse" || name == "output_move_slide";
    pControl->throttle = move ? 8 : 0;
    if (name == "output_percent_zero")
        pControl->throttleMode = Mode::ESimOne_Throttle_Mode_Percent;
    mainVehicle.drive();
    const bool stop = !move && name != "output_percent_zero";
    const auto expectedGear = stop
        ? (mainVehicle.reverse ? Gear::ESimOne_Gear_Mode_Reverse : Gear::ESimOne_Gear_Mode_Drive)
        : (slide ? Gear::ESimOne_Gear_Mode_Neutral : Gear::ESimOne_Gear_Mode_Reverse);
    require(obs(0).speed == (move ? (mainVehicle.reverse ? 3 : 8) : 0),
            "final target speed disagrees with stop/move request");
    require(obs(0).gear == expectedGear, "final gear disagrees with stop/move request");
    require(std::isfinite(obs(0).steering), "nonfinite final steering");
    require(lightSends == 1 && lastLights == 4, "light request not forwarded");
    if (name == "output_stop_forward")
        require(std::abs(obs(0).steering) > 1e-5, "stop discarded lane steering");
    if (name == "output_resume_slide") {
        mainVehicle.update(); // release stop, still inside sliding region
        mainVehicle.drive();
        require(obs(1).speed == 8 && obs(1).gear == Gear::ESimOne_Gear_Mode_Neutral,
                "stop incorrectly latched or disabled later sliding");
        mainVehicle.pt = {50, 0, 0};
        mainVehicle.update(); mainVehicle.drive();
        require(obs(2).gear == Gear::ESimOne_Gear_Mode_Drive, "sliding end state lost");
    }
    return true;
}
