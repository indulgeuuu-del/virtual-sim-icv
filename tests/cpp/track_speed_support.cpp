#include "main.h"
// The single-neighbor speed proposals are fixture inputs; the production
// arbitration statements and final real drive() execute without rewriting.
void trackSpeedForTest(float proposed) {
// @BEFORE_OVERTAKING@
    if (overtakeSwitcher.state) {
        pControl->throttle=proposed;
// @ARBITRATION_AND_FINAL_GUARD@
    mainVehicle.drive();
}
