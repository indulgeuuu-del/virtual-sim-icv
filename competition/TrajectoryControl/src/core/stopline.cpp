#include "SimOneSensorAPI.h"
#include "vehicle.h"
#include "obstacle.h"
#include "manual.h"
#include "stopline.h"
#include <stdexcept>
#include <limits>

// 查询接口返回类型尚待本届SDK核验；先验证实际取得的首条停止线几何。
static bool readStopLineEndpoints(const SSD::SimVector<HDMapStandalone::MObject>& lines,
    SSD::SimPoint3D& src, SSD::SimPoint3D& dst)
{
    if (lines.size() == 0 || lines[0].boundaryKnots.size() < 2) return false;
    const auto& a = lines[0].boundaryKnots[0];
    const auto& b = lines[0].boundaryKnots[1];
    auto finite = [](const SSD::SimPoint3D& p) {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    };
    if (!finite(a) || !finite(b) || (a.x == b.x && a.y == b.y)) return false;
    src = a;
    dst = b;
    return true;
}

static void requireStopConstraint(StopLine& line, const char* reason)
{
    line.type = StopLine::Type::Whole;
    line.strategy = StopLine::Strategy::StopStart;
    line.isValid = line.isVechicleInSameLane = true;
    line.isBehind = false;
    line.toVehicleDist = line.offset = 0.0f;
    line.src = line.dst = line.mid = mainVehicle.pt;
    LOG << reason;
}

// 计算到主车最近的停止线
bool calculateNearestStopLine(const std::vector<StopLine>& list, StopLine& stopLine, float& distance)
{
    if (list.empty()) {
        return false;
    }

    int minIndex = -1;
    float minDistance = std::numeric_limits<float>::max();

    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i].isValid && !list[i].isBehind && list[i].isVechicleInSameLane && list[i].toVehicleDist < minDistance) {
            minDistance = list[i].toVehicleDist;
            minIndex = i;
        }
    }

    if (minIndex == -1) {
        return false;
    }

    stopLine = list[minIndex];
    distance = minDistance;
    return true;
}

// 默认构造函数
StopLine::StopLine()
    : type(Type::Whole),               // 默认类型为 Whole
    strategy(Strategy::StopStart),     // 默认策略为 StopStart
    isVechicleInSameLane(false),       // 默认不在同车道
    isBehind(false),                   // 默认不在主车后面
    isValid(false),
    toVehicleDist(0.0f),               // 默认距离为 0
    vx(0.0f), vy(0.0f),                // 默认速度为 0
    velocity(0.0f),                    // 默认速度为 0
    velocityPlanar(0.0f),              // 默认平面速度为 0
    offset(0.0f),                      // 距离障碍物 offset 时停车
    obstaclePtr(nullptr)               // 默认障碍物指针为空
{
    // 默认的 src, dst, mid 坐标可以初始化为原点
    src = SSD::SimPoint3D(0.0f, 0.0f, 0.0f);
    dst = SSD::SimPoint3D(0.0f, 0.0f, 0.0f);
    mid = SSD::SimPoint3D(0.0f, 0.0f, 0.0f);
}

// 通过交通信号灯创建停止线
StopLine::StopLine(const HDMapStandalone::MSignal& light) : 
    type(Type::Whole), strategy(Strategy::StopStart), vx(0.0f), vy(0.0f), velocity(0.0f), velocityPlanar(0.0f), isVechicleInSameLane(true), isBehind(false), obstaclePtr(nullptr), offset(0.0f), isValid(true)
{
    SSD::SimVector<HDMapStandalone::MObject> trafficLightStopLineList;
    SimOneAPI::GetStoplineList(light, mainVehicle.laneID, trafficLightStopLineList);

    if (!readStopLineEndpoints(trafficLightStopLineList, src, dst)) {
        requireStopConstraint(*this, "Traffic-light stop line unavailable: requesting stop");
        return;
    }
    mid = SSD::SimPoint3D(0.5 * (src.x + dst.x), 0.5 * (src.y + dst.y), 0.5 * (src.z + dst.z));

    toVehicleDist = calculateVehicleDistance(mainVehicle.pt);
}

// 通过障碍物创建停止线
StopLine::StopLine(Obstacle& obstacle) :
    vx(obstacle.vx), vy(obstacle.vy), velocity(obstacle.velocity), velocityPlanar(obstacle.velocityPlanar), obstaclePtr(&obstacle)
{
    if (obstacle.type == Obstacle::Type::AmongCrosswalk) // 在斑马线上的障碍物
    {
        type = Type::Whole;
        strategy = Strategy::StopStart;

        SSD::SimVector<HDMapStandalone::MObject> crosswalkStopLineList;
        SimOneAPI::GetSpecifiedLaneStoplineList(mainVehicle.laneID, crosswalkStopLineList);
        if (!readStopLineEndpoints(crosswalkStopLineList, src, dst)) {
            requireStopConstraint(*this, "Crosswalk stop line unavailable: requesting stop");
            return;
        }

        offset = 0.0f;
        isValid = true;
    }
    else if (obstacle.type == Obstacle::Type::Horizental) // 障碍物是沿着车道运动的
    {
        if ((mainVehicle.leftLaneExist || mainVehicle.rightLaneExist) && IS_SINGLE_STOPLINE(obstacle)) // 如果是半停止线
        {
            type = Type::Single;
            strategy = Strategy::LaneChange;
            src = obstacle.leftLanePt;
            dst = obstacle.rightLanePt;

            if (obstacle.laneID == mainVehicle.laneID && std::fabs(obstacle.tRelativeToVehicle) < 0.5f * obstacle.laneWidth) isValid = true;
            else isValid = false;
        }
        else // 如果是全停止线
        {
            type = Type::Whole;
            strategy = Strategy::StopStart;
            src = obstacle.leftRoadPt;
            dst = obstacle.rightRoadPt;
            isValid = true;
        }

        offset = 0.5f * obstacle.length;
    }
    else if (obstacle.type == Obstacle::Type::Vertical) // 障碍物是垂直车道运动的
    {
        type = Type::Whole;
        strategy = Strategy::StopStart;
        src = obstacle.leftRoadPt;
        dst = obstacle.rightRoadPt;

        offset = 0.5f * obstacle.width;
        isValid = true;
    }
    else if (obstacle.type == Obstacle::Type::Static) // 障碍物是静止的
    {
        /*LOG << "要求：if (obstacle.width < lessThanLaneFactor * obstacle.laneWidth) return true; if (obstacle.width > moreThanRoadFactor * obstacle.roadWidth) return false; return true;";
        LOG << "obstacle.lane = " << obstacle.laneWidth;
        LOG << "obstacle.width = " << obstacle.width;
        LOG << "lessThanLaneFactor * obstacle.laneWidth = " << lessThanLaneFactor * obstacle.laneWidth;
        LOG << "moreThanRoadFactor * obstacle.roadWidth = " << moreThanRoadFactor * obstacle.roadWidth;*/

        // 如果是半停止线且可以变道（如果是半停止线且不是单车道），那就可以创建半停止线，注解：单车道不创建半停止线
        if ((mainVehicle.leftLaneExist || mainVehicle.rightLaneExist) && IS_SINGLE_STOPLINE(obstacle))
        {
            type = Type::Single;
            strategy = Strategy::LaneChange;
            src = obstacle.leftLanePt;
            dst = obstacle.rightLanePt;

            if (obstacle.laneID == mainVehicle.laneID && std::fabs(obstacle.tRelativeToVehicle) < 0.5f * obstacle.laneWidth) isValid = true;
            else isValid = false;
        }
        else // 如果是全停止线，或者如果是单车道，那就都要创建全停止线
        {
            type = Type::Whole;
            strategy = Strategy::StopStart;
            src = obstacle.leftRoadPt;
            dst = obstacle.rightRoadPt;
            isValid = true;
        }

        // 如果是单车道（亦即不能变道），且障碍物在车道外，则该停止线不合法，亦即不允许创建任何停止线
        if (!(mainVehicle.leftLaneExist || mainVehicle.rightLaneExist) && (std::abs(obstacle.tRelativeToLane) > 0.5f * obstacle.laneWidth))
        {
            isValid = false;
        }

        offset = 0.5f * obstacle.length;
    }

    mid = SSD::SimPoint3D(0.5 * (src.x + dst.x), 0.5 * (src.y + dst.y), 0.5 * (src.z + dst.z));
    isBehind = isStopLineBehindVehicle(src, dst, mainVehicle.pt, obstacle.tl, obstacle.br);
    try {
        isVechicleInSameLane = checkVechicleInSameLane(*obstaclePtr);
    }
    catch (const std::domain_error& error) {
        // 道路关系未知时不能忽略目标或进入变道，生成当前帧的停车约束。
        type = Type::Whole;
        strategy = Strategy::StopStart;
        isValid = isVechicleInSameLane = true;
        isBehind = false;
        toVehicleDist = offset = 0.0f;
        LOG << error.what();
        return;
    }
    toVehicleDist = calculateVehicleDistance(mainVehicle.pt);
}

// 通过手动创建停止线
StopLine::StopLine(const ManualStopLineReservoir& reservoir) :
    vx(0.0f), vy(0.0f), velocity(0.0f), velocityPlanar(0.0f), obstaclePtr(nullptr), offset(0.0f), isValid(true), isBehind(false), isVechicleInSameLane(true)
{
    if (reservoir.command == 0)
    {
        type = StopLine::Type::Whole;
        strategy = Strategy::StopStart;
    }
    else
    {
        type = StopLine::Type::Single;
        strategy = Strategy::LaneChange;
    }
    
    src = reservoir.srcPos;
    dst = reservoir.dstPos;
    mid = SSD::SimPoint3D(0.5 * (src.x + dst.x), 0.5 * (src.y + dst.y), 0.5 * (src.z + dst.z));
    toVehicleDist = calculateVehicleDistance(mainVehicle.pt);
}

StopLine StopLine::AttractLine(Obstacle& obstacle, const SSD::SimPoint3D& pt)
{
    StopLine attractLine;
    attractLine.vx = attractLine.vy = attractLine.velocity = attractLine.velocityPlanar = attractLine.offset = 0.0f;
    attractLine.type = Type::Attract;
    attractLine.strategy = Strategy::LaneChange;
    attractLine.obstaclePtr = &obstacle;
    attractLine.isValid = true;

    // 计算在垂直于道路方向上，距离 pt 点一定距离的两个点；pt1 正方向；pt2 负方向
    auto getPerpendicular = [](const SSD::SimPoint3D& pt, float azimuthDeg, float distance, SSD::SimPoint3D& pt1, SSD::SimPoint3D& pt2)
        {
            float theta = azimuthDeg * 0.017453f;

            float dx = std::sin(theta) * distance;
            float dy = -std::cos(theta) * distance;

            pt1.x = pt.x + dx;
            pt1.y = pt.y + dy;
            pt1.z = pt.z;

            pt2.x = pt.x - dx;
            pt2.y = pt.y - dy;
            pt2.z = pt.z;
        };

    getPerpendicular(pt, obstacle.laneAzimuth, 0.5f * MAIN_VEHICLE_WIDTH, attractLine.src, attractLine.dst);
    attractLine.mid = pt;

    attractLine.isBehind = attractLine.isStopLineBehindVehicle(attractLine.src, attractLine.dst, mainVehicle.pt, obstacle.tl, obstacle.br);
    //attractLine.isVechicleInSameLane = attractLine.checkVechicleInSameLane(*attractLine.obstaclePtr);
    attractLine.isVechicleInSameLane = true; // 允许吸引线可以不跨过主车
    attractLine.toVehicleDist = attractLine.calculateVehicleDistance(mainVehicle.pt);

    return attractLine;
}

// 计算停止线到主车的距离
double StopLine::calculateVehicleDistance(const SSD::SimPoint3D& vehiclePosition) const
{
    float dx = dst.x - src.x;
    float dy = dst.y - src.y;

    float numerator = std::abs(dy * vehiclePosition.x - dx * vehiclePosition.y + dst.x * src.y - dst.y * src.x);
    float denominator = std::sqrt(dx * dx + dy * dy);

    return (denominator > 1e-6f) ? (numerator / denominator) : 0.0;
}

// 判断停止线是否在主车后方
bool StopLine::isStopLineBehindVehicle(const SSD::SimPoint3D& srcPos, const SSD::SimPoint3D& dstPos, const SSD::SimPoint3D& mainVehiclePos, const SSD::SimPoint3D& tlPos, const SSD::SimPoint3D& brPos)
{
    // 使用二维叉积函数，忽略 z 坐标
    float crossPos = crossProduct(srcPos, dstPos, mainVehiclePos);
    float crossTL = crossProduct(srcPos, dstPos, tlPos);
    float crossBR = crossProduct(srcPos, dstPos, brPos);

    // 判断停车线是否在主车后方
    if ((crossPos >= 0 && crossTL >= 0) || (crossPos <= 0 && crossTL <= 0)) {
        return true;  // 停车线在主车后方
    }
    if ((crossPos >= 0 && crossBR >= 0) || (crossPos <= 0 && crossBR <= 0)) {
        return false; // 停车线不在主车后方
    }

    return false;  // 默认返回 false，表示停车线不在主车后方
}

// 查询停止线是否与主车同车道
bool StopLine::checkVechicleInSameLane(const Obstacle& obstacle) const
{
    // 转入 Frenet 坐标系下处理，判断停止线的起点和终点是否均在主车的同一侧，如果分别在主车的两侧，则停止线与主车同车道
    // s：沿车道中心线的距离 t：垂直于车道中心线的距离 z：输入点在局部 ENU 坐标系中的高度值
    double sSrc = 0.0, tSrc = 0.0;
    double sDst = 0.0, tDst = 0.0;
    double sVeh = 0.0, tVeh = 0.0;
    double zTemp = 0.0;

    // 计算停止线起点和终点以及主车相对于道路参考线的 ST 坐标
    auto project = [&](const SSD::SimPoint3D& point, double& s, double& t) {
        s = t = zTemp = std::numeric_limits<double>::quiet_NaN();
        if (!SimOneAPI::GetRoadST(mainVehicle.laneID, point, s, t, zTemp)
            || !std::isfinite(s) || !std::isfinite(t) || !std::isfinite(zTemp))
            throw std::domain_error("Stop-line road projection unavailable: requesting stop");
    };
    project(src, sSrc, tSrc);
    project(dst, sDst, tDst);
    project(mainVehicle.pt, sVeh, tVeh);

    // 如果停止线的起点和终点均在主车的同一侧
    float dt1 = tSrc - tVeh;
    float dt2 = tDst - tVeh;

    if (dt1 * dt2 < 0) return true;
    return false;
}

// 对于静止的停止线的变道操作（使用 Liy 变道法）
bool getLaneChangePath(StopLine& stopLine)
{
    // Callers historically ignore the boolean. Failure must explicitly request
    // Speed zero before the main loop snapshots its pre-arbitration ceiling.
    auto fail = [](const char* reason) {
        LOG << "Lane change unavailable: " << reason << "; stop and retry";
        pControl->throttleMode = ESimOne_Throttle_Mode::ESimOne_Throttle_Mode_Speed;
        pControl->throttle = 0;
        pLight->signalLights = ESimOne_Signal_Light::ESimOne_Signal_Light_None;
        return false;
    };
    auto finitePoint = [](const SSD::SimPoint3D& point) {
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    };
    auto validPath = [&](const SSD::SimPoint3DVector& path) {
        if (path.size() < 2) return false;
        bool distinct = false;
        for (size_t i = 0; i < path.size(); ++i) {
            if (!finitePoint(path[i])) return false;
            if (i && (path[i].x != path[0].x || path[i].y != path[0].y)) distinct = true;
        }
        return distinct;
    };
    if (!stopLine.isVechicleInSameLane) return false;
    if (!std::isfinite(stopLine.toVehicleDist) || !std::isfinite(laneChangeDistThres)
        || laneChangeDistThres <= 0 || !std::isfinite(completeLaneChangeTThres)
        || completeLaneChangeTThres <= 0) return fail("invalid distance or threshold");
    if (stopLine.toVehicleDist >= laneChangeDistThres) return false;
    if (!finitePoint(mainVehicle.pt) || !std::isfinite(pGps->angVelZ))
        return fail("invalid vehicle pose");

    HDMapStandalone::MLaneLink link;
    if (!SimOneAPI::GetLaneLink(mainVehicle.laneID, link)) return fail("lane link query failed");
    SSD::SimString lane;
    auto signal = ESimOne_Signal_Light::ESimOne_Signal_Light_None;
    if (mainVehicle.leftLaneExist && mainVehicle.neighborhood.left.empty()) {
        lane = link.leftNeighborLaneName;
        signal = ESimOne_Signal_Light::ESimOne_Signal_Light_LeftBlinker;
    } else if (mainVehicle.rightLaneExist && mainVehicle.neighborhood.right.empty()) {
        lane = link.rightNeighborLaneName;
        signal = ESimOne_Signal_Light::ESimOne_Signal_Light_RightBlinker;
    } else return fail("no available adjacent lane");
    if (lane == SSD::SimString("")) return fail("empty adjacent lane identity");
    HDMapStandalone::MLaneInfo info;
    if (!SimOneAPI::GetLaneSample(lane, info)) return fail("lane sample query failed");
    if (!(info.laneName == lane) || !validPath(info.centerLine))
        return fail("invalid adjacent lane geometry or identity");
    double s = std::numeric_limits<double>::quiet_NaN();
    double t = std::numeric_limits<double>::quiet_NaN();
    if (!SimOneAPI::GetLaneST(lane, mainVehicle.pt, s, t) || !std::isfinite(s) || !std::isfinite(t))
        return fail("lane projection failed");

    SSD::SimPoint3DVector candidatePath = info.centerLine;
    SSD::SimPoint3DVector candidateInput;
    const bool complete = std::abs(t) < completeLaneChangeTThres;
    if (complete) {
        if (initialPath.empty() || !finitePoint(initialPath.back())) return fail("missing destination");
        candidateInput.push_back(mainVehicle.pt);
        candidateInput.push_back(initialPath.back());
        candidatePath.clear();
        SSD::SimVector<int> candidateValid;
        if (!SimOneAPI::GenerateRoute(candidateInput, candidateValid, candidatePath)
            || !validPath(candidatePath)) return fail("route generation failed or invalid route");
    }
    // Commit only after all SDK outputs are usable. Partial failed writes stay
    // local so the active route and steering state survive for a later retry.
    targetPath = candidatePath;
    mainVehicle.steeringOffset = -pGps->angVelZ;
    if (complete) {
        initialPath = candidateInput;
        steerKpUse += 0.5;
        signal = ESimOne_Signal_Light::ESimOne_Signal_Light_None;
    }
    pLight->signalLights = signal;
    return true;
}

// Both entry points use the same occupancy and SDK-failure protections.
bool getLaneChangePath(StopLine& stopLine, float velocityPlanar)
{
    (void)velocityPlanar; // inherited entry has no separate speed planning
    return getLaneChangePath(stopLine);
}

//// 对于运动的停止线的变道操作
//bool getLaneChangePath(StopLine& stopLine, float velocityPlanar)
//{
//
//}
