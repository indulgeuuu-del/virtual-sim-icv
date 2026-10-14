// Included after manual test helpers. GPS timestamp is milliseconds; wall time
// deliberately continues during paused simulation. No SDK/dynamics claim.
bool stopTimingTest(const std::string& name) {
    if (name.rfind("timing_", 0) != 0) return false;
    strategyPoint.push_back(point(0, 6, -1, 0.5));
    frameLimit = 13;
    for (int i=0; i<frameLimit; ++i) frameInputs.push_back({0, 0, i*100});
    size_t resume = 5;
    if (name == "timing_projection_drive" || name == "timing_projection_switch") {
        strategyPoint.clear(); frameLimit=3; frameInputs.resize(3);
        const bool switching=name=="timing_projection_switch";
        if(switching) { strategyPoint.push_back(point(0,6)); strategyPoint.push_back(point(100,9)); failingDistanceReads={3}; }
        else { strategyPoint.push_back(point(100,6)); frameInputs[0].distanceValid=false; }
        runFrames();
        require(obs(switching ? 1 : 0).speed==0,"invalid projection did not stop moving strategy");
        require(obs(2).speed==(switching ? 9 : 6),"valid projection failed to recover");
        return true;
    }
    if (name == "timing_moving") {
        for (int i=0; i<4; ++i) frameInputs[i].speed = 2;
        resume = 9;
    } else if (name == "timing_paused") {
        for (auto& f:frameInputs) f.timestamp = 0;
        resume = frameLimit;
    } else if (name == "timing_roll_again") {
        frameInputs[3].speed = 0.2;
        resume = 9;
    } else if (name == "timing_leave_zone") {
        frameInputs[3].x = 2;
        resume = 9;
    } else if (name == "timing_bad_gps") {
        frameInputs[3].valid = false;
        resume = 9;
    } else if (name == "timing_bad_obstacle") {
        frameInputs[3].obstacleValid = false;
        resume = 9;
    } else if (name == "timing_bad_projection") {
        frameInputs[3].distanceValid = false;
        resume = 9;
    } else if (name == "timing_clock_backwards") {
        for (int i=3;i<frameLimit;++i) frameInputs[i].timestamp = (i-3)*100;
        resume = 9; // rollback sample invalid, first fresh stationary sample at 4
    } else if (name == "timing_gap") {
        for (int i=3;i<frameLimit;++i) frameInputs[i].timestamp += 2000;
        resume = 9; // missing interval cannot be credited as observed standstill
    } else if (name == "timing_boundary") {
        for (auto& f:frameInputs) f.x = achieveThres;
    } else if (name == "timing_switch_stop") {
        strategyPoint.clear();
        strategyPoint.push_back(point(0,6));
        strategyPoint.push_back(point(0.5,6,-1,0.5));
        frameLimit=2; frameInputs.resize(2);
        runFrames();
        require(obs(0).speed==6 && obs(1).speed==0, "switch frame skipped new point stop");
        return true;
    } else throw std::runtime_error("unknown timing test");
    runFrames();
    for (size_t i=0;i<resume;++i) require(obs(i).speed==0, "resumed before continuous stationary simulation interval");
    if (resume < static_cast<size_t>(frameLimit)) require(obs(resume).speed==6, "did not resume after observed dwell");
    return true;
}
