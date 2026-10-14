// SDK state only. Function bodies and AdaptiveFollowing/PID are inserted from
// production sources by test_competition_control.py, including --revision runs.
#include "main.h"
// @PRODUCTION_CONTROLLER@
struct Obstacle {
    enum class Type { Static, Vertical, Horizental, AmongCrosswalk };
    Type type = Type::Horizental;
    double sRelativeToVehicle = 0;
    float velocity = 0;
    bool isSameLaneWithMainVehicle = true, isSameRoadWithMainVehicle = true;
};
// Same fixture gains as the repository configuration. Distances are lane-s,
// not vehicle body gap. No physics or scoring boundary is asserted here.
int caseStop = 25;
float followingLimitSpeed = 3.5f;
AdaptiveFollowing adaptiveFollowing(-0.2f, 0, 0, 45);
// @PRODUCTION_NEAREST@
// @PRODUCTION_FOLLOWING@
float followingTargetForTest(double distance, float leadSpeed) {
    Obstacle lead;
    lead.sRelativeToVehicle = mainVehicle.s + distance;
    lead.velocity = leadSpeed;
    return caculateFollowingSpeed({lead}, 2);
}
