// Included by the real-drive build of manual_stub/test_main.cpp.
// Engineering invariant supporting 2027 scoring PDF physical p11, table 3:
// a zero target speed must reach SetDrive even during reverse/sliding logic.
// This does not assert physical standstill, stopping distance, or official score.
extern float followingTargetForTest(double distance, float leadSpeed);
extern void trackSpeedForTest(float proposed, bool projectionFailed=false);
bool driveOutputTest(const std::string& name) {
    using Gear = ESimOne_Gear_Mode;
    using Mode = ESimOne_Throttle_Mode;
    if (name.rfind("output_", 0) != 0) return false;
    targetPath = {{0, 0, 0}, {10, 1, 0}, {20, 2, 0}};
    mainVehicle.update();
    steerKpUse = steerKp;
    steerKdUse = steerKd;
    pLight->signalLights = 4;
    if(name.rfind("output_invalid_",0)==0 || name=="output_send_failure") {
        if(name=="output_send_failure") {
            controlSendOk=false;
            mainVehicle.drive();
            require(logText.find("SetDrive")!=std::string::npos,"failed drive send has no diagnostic");
            return true;
        }
        pControl->throttle=std::nan("");
        if(name=="output_invalid_infinite") pControl->throttle=std::numeric_limits<double>::infinity();
        if(name=="output_invalid_negative") pControl->throttle=-1;
        if(name=="output_invalid_reverse") mainVehicle.reverse=true;
        if(name=="output_invalid_slide") slideConfig={{{0,0,0},{50,0,0}}};
        if(name=="output_invalid_percent") pControl->throttleMode=Mode::ESimOne_Throttle_Mode_Percent;
        if(name=="output_invalid_steering") {
            pControl->throttle=8; pControl->steering=std::nan(""); mainVehicle.useDefaultPath=false;
        }
        mainVehicle.drive();
        require(obs(0).speed==0 && std::isfinite(obs(0).steering),"invalid control reached SetDrive");
        require(pControl->throttleMode==Mode::ESimOne_Throttle_Mode_Speed,"invalid control stop not in speed mode");
        require(obs(0).gear==(mainVehicle.reverse ? Gear::ESimOne_Gear_Mode_Reverse : Gear::ESimOne_Gear_Mode_Drive),
                "invalid control stop stayed in neutral");
        require(logText.find("Invalid")!=std::string::npos,"invalid control lacks diagnostic");
        return true;
    }
    if(name.rfind("output_limit_",0)==0) {
        caseMaxSpeed=2;
        mainVehicle.reverse=name=="output_limit_reverse";
        if(name=="output_limit_slide" || name=="output_limit_overspeed_slide")
            slideConfig={{{0,0,0},{50,0,0}}};
        double expected=2;
        if(name=="output_limit_equal") pControl->throttle=2;
        if(name=="output_limit_below") { pControl->throttle=1; expected=1; }
        if(name=="output_limit_overspeed_slide") { pControl->throttle=1; expected=1; mainVehicle.speed=3; }
        if(name=="output_limit_zero") { caseMaxSpeed=0; expected=0; }
        if(name=="output_limit_nan") { caseMaxSpeed=std::nanf(""); expected=0; }
        if(name=="output_limit_negative") { caseMaxSpeed=-1; expected=0; }
        if(name=="output_limit_percent") {
            caseMaxSpeed=0; pControl->throttleMode=Mode::ESimOne_Throttle_Mode_Percent;
            pControl->throttle=0.5; expected=0.5;
        }
        mainVehicle.drive();
        require(obs(0).speed==expected,"configured ceiling not enforced or ordinary request changed");
        require(obs(0).gear==(mainVehicle.reverse ? Gear::ESimOne_Gear_Mode_Reverse : Gear::ESimOne_Gear_Mode_Drive),
                "limited command remained in neutral");
        return true;
    }
    if(name.rfind("output_arbitration_",0)==0) {
        auto& n=mainVehicle.neighborhood;
        const bool front=name=="output_arbitration_front";
        const bool leftFront=name=="output_arbitration_left_front";
        const bool rightFront=name=="output_arbitration_right_front";
        const bool release=name=="output_arbitration_release";
        const bool disabled=name=="output_arbitration_disabled";
        const bool projectionFailed=name=="output_arbitration_bad_projection";
        pControl->throttle=(front || leftFront || rightFront || projectionFailed) ? 8 : 0;
        mainVehicle.reverse=name=="output_arbitration_reverse";
        if(name=="output_arbitration_slide") slideConfig={{{0,0,0},{50,0,0}}};
        overtakeSwitcher.state=!disabled;
        n.bl=true; n.vl=6;
        if(front) { n.bf=true; n.vf=2; }
        if(leftFront) { n.blf=true; n.vlf=2; }
        if(rightFront) { n.brf=true; n.vrf=2; }
        trackSpeedForTest(6,projectionFailed);
        require(obs(0).speed==((front || leftFront || rightFront) ? 2 : 0),"overtaking overrode stop/front speed ceiling");
        require(obs(0).gear==(mainVehicle.reverse ? Gear::ESimOne_Gear_Mode_Reverse : Gear::ESimOne_Gear_Mode_Drive),
                "stop arbitration lost engaged gear");
        if(release) {
            mainVehicle.update(); trackSpeedForTest(6);
            require(obs(1).speed==6,"released stop remained latched");
        }
        return true;
    }
    if (name == "output_empty_path" || name == "output_short_path" ||
        name == "output_empty_reverse" || name == "output_empty_slide" ||
        name == "output_empty_recovery" || name == "output_explicit_no_path") {
        targetPath.clear();
        if(name == "output_short_path") targetPath.push_back({0,0,0});
        mainVehicle.reverse = name == "output_empty_reverse";
        if(name == "output_empty_slide") slideConfig={{{0,0,0},{50,0,0}}};
        const bool explicitSteering = name == "output_explicit_no_path";
        if(explicitSteering) { mainVehicle.useDefaultPath=false; pControl->steering=0.25; }
        mainVehicle.drive();
        require(obs(0).speed==(explicitSteering ? 8 : 0),"invalid path did not stop or disabled explicit steering");
        require(obs(0).steering==(explicitSteering ? 0.25 : 0),"invalid path produced stale/nonfinite steering");
        require(obs(0).gear==(mainVehicle.reverse ? Gear::ESimOne_Gear_Mode_Reverse : Gear::ESimOne_Gear_Mode_Drive),
                "path failure stop overwritten by reverse/sliding");
        require(lightSends==1 && lastLights==4,"path failure lost light request");
        if(name == "output_empty_recovery") {
            targetPath={{0,0,0},{10,1,0},{20,2,0}};
            mainVehicle.update(); mainVehicle.drive();
            require(obs(1).speed==8,"valid path did not recover");
        }
        return true;
    }
    if (name == "output_follow_stop" || name == "output_follow_slow" || name == "output_follow_stop_resume") {
        const bool slow = name == "output_follow_slow";
        pControl->throttle = followingTargetForTest(slow ? 45 : 20, slow ? 2.0f : 0.1f);
        mainVehicle.drive();
        require(obs(0).speed == (slow ? 2 : 0), "following decision was raised before final SetDrive");
        require(obs(0).gear == Gear::ESimOne_Gear_Mode_Drive, "following command used wrong gear");
        if (name == "output_follow_stop_resume") {
            pControl->throttle = followingTargetForTest(45, 2.0f);
            mainVehicle.drive();
            require(obs(1).speed == 2, "following did not resume at the lead's low speed");
        }
        return true;
    }
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
