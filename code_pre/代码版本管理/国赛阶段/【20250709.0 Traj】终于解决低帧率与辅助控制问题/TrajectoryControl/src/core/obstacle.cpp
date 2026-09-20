#include <set>
#include <unordered_set>
#include "UtilMath.h"
#include "define.h"
#include "logger.hpp"
#include "utility.h"
#include "obstacle.h"
#include "controller.hpp"
#include "prediction.h"
SSD::SimPoint3D calculateObstacleTL(const SimOne_Data_Obstacle_Entry& obstacle) // 左上
{
    float halfWidth = 0.5f * obstacle.width;
    float halfLength = 0.5f * obstacle.length;
    float azimuth = mainVehicle.laneAzimuth;

    if ((0 <= azimuth && azimuth <= 45) || (315 < azimuth && azimuth <= 360)) // 主车向东运动，道路是东西方向的
    {
        return SSD::SimPoint3D(obstacle.posX + halfLength, obstacle.posY + halfWidth, obstacle.posZ);
    }
    else if (135 < azimuth && azimuth <= 225) // 主车向西运动，道路是东西方向的
    {
        return SSD::SimPoint3D(obstacle.posX - halfLength, obstacle.posY - halfWidth, obstacle.posZ);
    }
    else if (45 < azimuth && azimuth <= 135) // 主车向北运动，道路是南北方向的
    {
        return SSD::SimPoint3D(obstacle.posX - halfWidth, obstacle.posY + halfLength, obstacle.posZ);
    }
    else // (225 < azimuth && azimuth <= 315) // 主车向南运动，道路是南北方向的
    {
        return SSD::SimPoint3D(obstacle.posX + halfWidth, obstacle.posY - halfLength, obstacle.posZ);
    }
}

SSD::SimPoint3D calculateObstacleTR(const SimOne_Data_Obstacle_Entry& obstacle) // 右上
{
    float halfWidth = 0.5f * obstacle.width;
    float halfLength = 0.5f * obstacle.length;
    float azimuth = mainVehicle.laneAzimuth;

    if ((0 <= azimuth && azimuth <= 45) || (315 < azimuth && azimuth <= 360))
    {
        return SSD::SimPoint3D(obstacle.posX + halfLength, obstacle.posY - halfWidth, obstacle.posZ);
    }
    else if (135 < azimuth && azimuth <= 225)
    {
        return SSD::SimPoint3D(obstacle.posX - halfLength, obstacle.posY + halfWidth, obstacle.posZ);
    }
    else if (45 < azimuth && azimuth <= 135)
    {
        return SSD::SimPoint3D(obstacle.posX + halfWidth, obstacle.posY + halfLength, obstacle.posZ);
    }
    else // (225 < azimuth && azimuth <= 315)
    {
        return SSD::SimPoint3D(obstacle.posX - halfWidth, obstacle.posY - halfLength, obstacle.posZ);
    }
}

SSD::SimPoint3D calculateObstacleBL(const SimOne_Data_Obstacle_Entry& obstacle) // 左下
{
    float halfWidth = 0.5f * obstacle.width;
    float halfLength = 0.5f * obstacle.length;
    float azimuth = mainVehicle.laneAzimuth;

    if ((0 <= azimuth && azimuth <= 45) || (315 < azimuth && azimuth <= 360))
    {
        return SSD::SimPoint3D(obstacle.posX - halfLength, obstacle.posY + halfWidth, obstacle.posZ);
    }
    else if (135 < azimuth && azimuth <= 225)
    {
        return SSD::SimPoint3D(obstacle.posX + halfLength, obstacle.posY - halfWidth, obstacle.posZ);
    }
    else if (45 < azimuth && azimuth <= 135)
    {
        return SSD::SimPoint3D(obstacle.posX - halfWidth, obstacle.posY - halfLength, obstacle.posZ);
    }
    else // (225 < azimuth && azimuth <= 315)
    {
        return SSD::SimPoint3D(obstacle.posX + halfWidth, obstacle.posY + halfLength, obstacle.posZ);
    }
}

SSD::SimPoint3D calculateObstacleBR(const SimOne_Data_Obstacle_Entry& obstacle) // 右下
{
    float halfWidth = 0.5f * obstacle.width;
    float halfLength = 0.5f * obstacle.length;
    float azimuth = mainVehicle.laneAzimuth;

    if ((0 <= azimuth && azimuth <= 45) || (315 < azimuth && azimuth <= 360))
    {
        return SSD::SimPoint3D(obstacle.posX - halfLength, obstacle.posY - halfWidth, obstacle.posZ);
    }
    else if (135 < azimuth && azimuth <= 225)
    {
        return SSD::SimPoint3D(obstacle.posX + halfLength, obstacle.posY + halfWidth, obstacle.posZ);
    }
    else if (45 < azimuth && azimuth <= 135)
    {
        return SSD::SimPoint3D(obstacle.posX + halfWidth, obstacle.posY - halfLength, obstacle.posZ);
    }
    else // (225 < azimuth && azimuth <= 315)
    {
        return SSD::SimPoint3D(obstacle.posX - halfWidth, obstacle.posY + halfLength, obstacle.posZ);
    }
}

void groupObstacleByDist(const std::vector<size_t>& indexList, float threshold, std::vector<std::vector<size_t>>& groups)
{
    groups.clear();
    size_t n = indexList.size();
    std::vector<std::vector<size_t>> adjacencyList(n);

    // 构建邻接矩阵
    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            size_t idx1 = indexList[i];
            size_t idx2 = indexList[j];

            if (UtilMath::calculateSpeed(pObstacle->obstacle[idx1].velX, pObstacle->obstacle[idx1].velY) < 1e-2 &&
                UtilMath::calculateSpeed(pObstacle->obstacle[idx2].velX, pObstacle->obstacle[idx2].velY) < 1e-2 &&
                UtilMath::distance(pObstacle->obstacle[idx1].posX, pObstacle->obstacle[idx1].posY,
                    pObstacle->obstacle[idx2].posX, pObstacle->obstacle[idx2].posY) <= threshold)
            {
                adjacencyList[i].push_back(j);
                adjacencyList[j].push_back(i);
            }
        }
    }

    // 使用 Lambda 进行深度优先搜索 (DFS)
    std::unordered_set<size_t> visited;
    auto graphDFS = [&](size_t index, auto& graphDFSRef, std::vector<size_t>& group) -> void {
        visited.insert(index);
        group.push_back(index);

        for (size_t neighbor : adjacencyList[index]) {
            if (visited.find(neighbor) == visited.end()) {
                graphDFSRef(neighbor, graphDFSRef, group);
            }
        }
        };

    // 遍历所有节点
    for (size_t i = 0; i < n; ++i)
    {
        if (visited.find(i) == visited.end())
        {
            std::vector<size_t> group;
            graphDFS(i, graphDFS, group);

            // 将 group 里的索引转换成原始索引
            for (size_t& idx : group) idx = indexList[idx];
            groups.push_back(group);
        }
    }
}

void groupObstacleByS(const std::vector<Obstacle>& obstacleList, const std::vector<size_t>& indexList, float threshold, std::vector<std::vector<size_t>>& groups)
{
    groups.clear();
    size_t n = indexList.size();
    std::vector<std::vector<size_t>> adjacencyList(n);

    // 构建邻接矩阵：根据 s 坐标是否在阈值内
    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            size_t idx1 = indexList[i];
            size_t idx2 = indexList[j];

            if (isSameRoadId(obstacleList[idx1].laneID, obstacleList[idx2].laneID) &&
                std::abs(obstacleList[idx1].sRelativeToRoad - obstacleList[idx2].sRelativeToRoad) <= threshold)
            {
                adjacencyList[i].push_back(j);
                adjacencyList[j].push_back(i);
            }
        }
    }

    // 深度优先搜索 (DFS)
    std::unordered_set<size_t> visited;
    auto graphDFS = [&](size_t index, auto& graphDFSRef, std::vector<size_t>& group) -> void {
        visited.insert(index);
        group.push_back(index);

        for (size_t neighbor : adjacencyList[index]) {
            if (visited.find(neighbor) == visited.end()) {
                graphDFSRef(neighbor, graphDFSRef, group);
            }
        }
        };

    // 遍历所有节点，构建分组
    for (size_t i = 0; i < n; ++i)
    {
        if (visited.find(i) == visited.end())
        {
            std::vector<size_t> group;
            graphDFS(i, graphDFS, group);

            // 将 group 中的局部索引转为原始索引
            for (size_t& idx : group) idx = indexList[idx];
            groups.push_back(group);
        }
    }
}

void getObstacleLaneSidePosition(const SSD::SimPoint3D& point, SSD::SimPoint3D& leftPoint, SSD::SimPoint3D& rightPoint)
{
    SSD::SimString obstacleLaneId = m_SampleGetNearMostLane(point);
    HDMapStandalone::MLaneInfo obstacleLaneInfo;
    SimOneAPI::GetLaneSample(obstacleLaneId, obstacleLaneInfo);
    const SSD::SimPoint3DVector& leftLine = obstacleLaneInfo.leftBoundary;
    const SSD::SimPoint3DVector& rightLine = obstacleLaneInfo.rightBoundary;

    double minDistL = std::numeric_limits<double>::max();
    int pointIndexL = leftLine.size();
    for (size_t j = 0, je = leftLine.size(); j < je; ++j)
    {
        double distTemp = UtilMath::planarDistance(leftLine[j], point);
        if (distTemp < minDistL)
        {
            minDistL = distTemp;
            pointIndexL = (int)j;
        }
    }

    double minDistR = std::numeric_limits<double>::max();
    int pointIndexR = rightLine.size();
    for (size_t j = 0, je = rightLine.size(); j < je; ++j)
    {
        double distTemp = UtilMath::planarDistance(rightLine[j], point);
        if (distTemp < minDistR)
        {
            minDistR = distTemp;
            pointIndexR = (int)j;
        }
    }

    leftPoint = leftLine[pointIndexL];
    rightPoint = rightLine[pointIndexR];
}

// 可以用 ST 坐标来计算距离来优化这个函数
void getObstacleRoadSidePosition(const SSD::SimPoint3D& point, SSD::SimPoint3D& leftPoint, SSD::SimPoint3D& rightPoint)
{
    SSD::SimString obstacleLaneId = m_SampleGetNearMostLane(point);
    HDMapStandalone::MLaneInfo obstacleLaneInfo;

    if (!SimOneAPI::GetLaneSample(obstacleLaneId, obstacleLaneInfo)) return;

    const SSD::SimPoint3DVector& midLine = obstacleLaneInfo.centerLine;
    double minDistTemp = std::numeric_limits<double>::max();
    int pointIndexTemp = midLine.size();

    // 找到距离障碍物最近的车道中点的索引
    for (size_t j = 0, je = midLine.size(); j < je; ++j)
    {
        double distTemp = UtilMath::planarDistance(midLine[j], point);
        if (distTemp < minDistTemp)
        {
            minDistTemp = distTemp;
            pointIndexTemp = static_cast<int>(j);
        }
    }

    std::set<SSD::SimString> visitedLanes;

    // Lambda 表达式：递归查找最左/最右侧车道
    auto findExtremeLane = [&](const SSD::SimString& laneId, bool findLeft, auto& findExtremeLaneRef) -> SSD::SimString {
        if (visitedLanes.count(laneId) > 0) return laneId;
        visitedLanes.insert(laneId);

        HDMapStandalone::MLaneLink link;
        if (SimOneAPI::GetLaneLink(laneId, link)) {
            SSD::SimString nextLane = findLeft ? link.leftNeighborLaneName : link.rightNeighborLaneName;
            if (!nextLane.Empty()) {
                return findExtremeLaneRef(nextLane, findLeft, findExtremeLaneRef);
            }
        }
        return laneId;
        };

    // 查找最左和最右的车道
    SSD::SimString leftExtremeLane = findExtremeLane(obstacleLaneId, true, findExtremeLane);
    SSD::SimString rightExtremeLane = findExtremeLane(obstacleLaneId, false, findExtremeLane);

    // 获取最左侧车道的左边界点
    HDMapStandalone::MLaneInfo leftLaneInfo;
    if (SimOneAPI::GetLaneSample(leftExtremeLane, leftLaneInfo)) {
        leftPoint = leftLaneInfo.leftBoundary[pointIndexTemp];
    }

    // 获取最右侧车道的右边界点
    HDMapStandalone::MLaneInfo rightLaneInfo;
    if (SimOneAPI::GetLaneSample(rightExtremeLane, rightLaneInfo)) {
        rightPoint = rightLaneInfo.rightBoundary[pointIndexTemp];
    }
}

bool isObstacleAmongCrosswalk(const SSD::SimPoint3D& tl, const SSD::SimPoint3D& tr, const SSD::SimPoint3D& bl, const SSD::SimPoint3D& br, const SSD::SimPoint3D& vertex1, const SSD::SimPoint3D& vertex2)
{
    // Lambda 表达式：判断一个数是否在两个数之间（不包含边界）
    auto isBetween = [](float val, float p1, float p2) {
        return (p1 < val && val < p2) || (p2 < val && val < p1);
        };

    // Lambda 表达式：判断点是否在矩形内部
    auto inRectangle = [&](const SSD::SimPoint3D& pt) {
        return isBetween(pt.x, vertex1.x, vertex2.x) && isBetween(pt.y, vertex1.y, vertex2.y);
        };

    // 计算矩形四条边的中点
    SSD::SimPoint3D topMid((tl.x + tr.x) / 2, (tl.y + tr.y) / 2, (tl.z + tr.z) / 2);
    SSD::SimPoint3D bottomMid((bl.x + br.x) / 2, (bl.y + br.y) / 2, (bl.z + br.z) / 2);
    SSD::SimPoint3D leftMid((tl.x + bl.x) / 2, (tl.y + bl.y) / 2, (tl.z + bl.z) / 2);
    SSD::SimPoint3D rightMid((tr.x + br.x) / 2, (tr.y + br.y) / 2, (tr.z + br.z) / 2);

    // 判断四个角点和四条边的中点是否在矩形内部
    return inRectangle(tl) || inRectangle(tr) || inRectangle(bl) || inRectangle(br) || inRectangle(topMid) || inRectangle(bottomMid) || inRectangle(leftMid) || inRectangle(rightMid);
}

// 通过 SimOne_Data_Obstacle_Entry 来构造
Obstacle::Obstacle(const SimOne_Data_Obstacle_Entry& obstacle) : 
	width(obstacle.width), length(obstacle.length), vx(obstacle.velX), vy(obstacle.velY), vz(obstacle.velZ), yaw(obstacle.oriZ), predictionPtr(std::make_shared<Prediction>(obstacle.prediction))
{
	velocity = UtilMath::calculateSpeed(vx, vy, vz);
	velocityPlanar = UtilMath::calculateSpeed(vx, vy);
    predictTrajectory(obstacle.prediction.trajectory, 10, 50, predictionGM);
	pt = SSD::SimPoint3D(obstacle.posX, obstacle.posY, obstacle.posZ);
    calculateObstacleCorners(obstacle, tl, tr, bl, br);

	laneID = m_SampleGetNearMostLane(pt);
    laneAzimuth = getLaneAzimuth(laneID);
	isSameLaneWithMainVehicle = (laneID == mainVehicle.laneID);
	isSameRoadWithMainVehicle = isSameRoadId(laneID, mainVehicle.laneID);

    /* 计算相对于主车的 ST 坐标 */
    if (isSameRoadWithMainVehicle) // 与主车同道路的时候才能计算 ST 坐标
    {
        SimOneAPI::GetLaneST(mainVehicle.laneID, pt, sRelativeToVehicle, tRelativeToVehicle);
    }
    else
    {
        tRelativeToVehicle = -1.0;

        // 这里是将两个点之间的 ENU 距离投影成 ST 距离
        auto computeRoadDistance = [](const SSD::SimPoint3D& p1, const SSD::SimPoint3D& p2, double azimuth) {
            double theta = azimuth * M_PI / 180.0;
            double dx = p2.x - p1.x;
            double dy = p2.y - p1.y;
            return std::abs(dx * cos(theta) + dy * sin(theta));
            };
        sRelativeToVehicle = mainVehicle.s + computeRoadDistance(mainVehicle.pt, pt, mainVehicle.laneAzimuth);
    }

    /* 计算相对于自身所在车道的 ST 坐标 */
    SimOneAPI::GetLaneST(laneID, pt, sRelativeToLane, tRelativeToLane);

    /* 计算相对于自身所在道路的 ST 坐标 */
    double z = 0.0f;
    SimOneAPI::GetRoadST(laneID, pt, sRelativeToRoad, tRelativeToRoad, z);

	SimOneAPI::GetLaneWidth(laneID, pt, laneWidth);
	roadWidth = getRoadWidth(laneID, pt);

	getObstacleLaneSidePosition(obstacle, leftLanePt, rightLanePt);
    getObstacleRoadSidePosition(obstacle, leftRoadPt, rightRoadPt);
    LOG << "velocityPlanar:" << velocityPlanar;
    if (FlagType::isCrosswalkExist && isObstacleAmongCrosswalk(tl, tr, bl, br))
    {
        type = Type::AmongCrosswalk;
    }
    else if (velocityPlanar < 1e-2)
    {
        type = Type::Static;
    }
    else
    {
        float obstacleTheta = calculateResultantAzimuth(vx, vy);
        float laneTheta = laneAzimuth;
        float deltaTheta = std::fabs(obstacleTheta - laneTheta);
        float angle = std::min(deltaTheta, 360.0f - deltaTheta);
        float thresTemp = 10.0f;

        if (std::fabs(angle - 90.0f) <= thresTemp || std::fabs(angle - 270.0f) <= thresTemp) // 近似垂直
        {
            type = Type::Vertical;
        }
        else if (std::fabs(angle - 0.0f) <= thresTemp || std::fabs(angle - 180.0f) <= thresTemp) // 近似平行
        {
            type = Type::Horizental;
        }
        else // 既不平行也不垂直
        {
            type = Type::Vertical;
        }
    }
}

// 通过索引来构造
Obstacle::Obstacle(size_t index) { *this = Obstacle(pObstacle->obstacle[index]); }

// 通过索引列表来构造
Obstacle::Obstacle(const std::vector<size_t>& index)
{

    if (index.size() == 1) // 如果当前组内只有一个障碍物
    {
        *this = Obstacle(pObstacle->obstacle[index.at(0)]);
    }
    else // 如果不止一个障碍物
    {
        std::vector<SSD::SimPoint3D> tlList, brList; // 分别计算每个障碍物的 tl 和 br
        SSD::SimPoint3D minTL, maxBR;

        for (size_t j = 0, je = index.size(); j < je; ++j)
        {
            tlList.push_back(calculateObstacleTL(pObstacle->obstacle[index.at(j)]));
            brList.push_back(calculateObstacleBR(pObstacle->obstacle[index.at(j)]));
        }

        auto findBoundingBox = [](const std::vector<SSD::SimPoint3D>& tlList, const std::vector<SSD::SimPoint3D>& brList, SSD::SimPoint3D& minTL, SSD::SimPoint3D& maxBR, float azimuth)
            {
                if (tlList.empty() || brList.empty()) return; // 避免空向量访问

                // 计算初始边界值
                auto initBoundingBox = [](float azimuth, SSD::SimPoint3D& minTL, SSD::SimPoint3D& maxBR, double z) {
                    using Limits = std::numeric_limits<float>;
                    if ((0 <= azimuth && azimuth <= 45) || (315 < azimuth && azimuth <= 360)) {
                        minTL = { Limits::lowest(), Limits::lowest(), z };
                        maxBR = { Limits::max(), Limits::max(), z };  
                    }
                    else if (135 < azimuth && azimuth <= 225) {
                        minTL = { Limits::max(), Limits::max(), z };   
                        maxBR = { Limits::lowest(), Limits::lowest(), z };
                    }
                    else if (45 < azimuth && azimuth <= 135) {
                        minTL = { Limits::max(), Limits::lowest(), z };
                        maxBR = { Limits::lowest(), Limits::max(), z };
                    }
                    else if (225 < azimuth && azimuth <= 315) {
                        minTL = { Limits::lowest(), Limits::max(), z };
                        maxBR = { Limits::max(), Limits::lowest(), z };
                    }
                    };

                initBoundingBox(azimuth, minTL, maxBR, tlList[0].z); // 初始化 minTL 和 maxBR

                // 计算最小包围盒
                auto updateBoundingBox = [&](const SSD::SimPoint3D& tl, const SSD::SimPoint3D& br) {
                    if ((0 <= azimuth && azimuth <= 45) || (315 < azimuth && azimuth <= 360)) {
                        minTL.x = std::max(minTL.x, tl.x);
                        minTL.y = std::max(minTL.y, tl.y);
                        maxBR.x = std::min(maxBR.x, br.x);
                        maxBR.y = std::min(maxBR.y, br.y);
                    }
                    else if (135 < azimuth && azimuth <= 225) {
                        minTL.x = std::min(minTL.x, tl.x);
                        minTL.y = std::min(minTL.y, tl.y);
                        maxBR.x = std::max(maxBR.x, br.x);
                        maxBR.y = std::max(maxBR.y, br.y);
                    }
                    else if (45 < azimuth && azimuth <= 135) {
                        minTL.x = std::min(minTL.x, tl.x);
                        minTL.y = std::max(minTL.y, tl.y);
                        maxBR.x = std::max(maxBR.x, br.x);
                        maxBR.y = std::min(maxBR.y, br.y);
                    }
                    else if (225 < azimuth && azimuth <= 315) {
                        minTL.x = std::max(minTL.x, tl.x);
                        minTL.y = std::min(minTL.y, tl.y);
                        maxBR.x = std::min(maxBR.x, br.x);
                        maxBR.y = std::max(maxBR.y, br.y);
                    }
                    };

                for (size_t i = 0; i < tlList.size(); ++i) {
                    updateBoundingBox(tlList[i], brList[i]);
                }
            };

        /*float azimuth = mainVehicle.laneAzimuth;*/
        SimOne_Data_Obstacle_Entry& getAzimuthObstacle = pObstacle->obstacle[index.at(0)];
        SSD::SimPoint3D getAzimuthPos = SSD::SimPoint3D(getAzimuthObstacle.posX, getAzimuthObstacle.posY, getAzimuthObstacle.posZ);
        float azimuth = getLaneAzimuth(m_SampleGetNearMostLane(getAzimuthPos));
        findBoundingBox(tlList, brList, minTL, maxBR, azimuth); // 将不同障碍物的 bounding box 合成一个

        tl = minTL;
        br = maxBR;

        if ((0 <= azimuth && azimuth <= 45 || 315 < azimuth <= 360)) // 主车向东运动，道路是东西方向的
        {
            tr = SSD::SimPoint3D(tl.x, br.y, tl.z); // 右上
            bl = SSD::SimPoint3D(br.x, tl.y, tl.z); // 左下
            width = tl.y - br.y;
            length = tl.x - br.x;
        }
        else if (135 < azimuth && azimuth <= 225) // 主车向西运动，道路是东西方向的
        {
            tr = SSD::SimPoint3D(tl.x, br.y, tl.z); // 右上
            bl = SSD::SimPoint3D(br.x, tl.y, tl.z); // 左下
            width = br.y - tl.y;
            length = br.x - tl.x;
        }
        else if (45 < azimuth && azimuth <= 135) // 主车向北运动，道路是南北方向的
        {
            tr = SSD::SimPoint3D(br.x, tl.y, tl.z); // 右上
            bl = SSD::SimPoint3D(tl.x, br.y, tl.z); // 左下
            width = br.x - tl.x;
            length = tl.y - br.y;
        }
        else // 主车向南运动，道路是南北方向的
        {
            tr = SSD::SimPoint3D(br.x, tl.y, tl.z); // 右上
            bl = SSD::SimPoint3D(tl.x, br.y, tl.z); // 左下
            width = tl.x - br.x;
            length = br.y - tl.y;
        }

        pt = SSD::SimPoint3D(0.5f * (tl.x + br.x), 0.5f * (tl.y + br.y), tl.z);
        velocity = velocityPlanar = vx = vy = vz = yaw = 0.0f;

        laneID = m_SampleGetNearMostLane(pt);
        laneAzimuth = getLaneAzimuth(laneID);
        isSameLaneWithMainVehicle = (laneID == mainVehicle.laneID);
        isSameRoadWithMainVehicle = isSameRoadId(laneID, mainVehicle.laneID);

        /* 计算相对于主车的 ST 坐标 */
        if (isSameRoadWithMainVehicle) // 与主车同道路的时候才能计算 ST 坐标
        {
            SimOneAPI::GetLaneST(mainVehicle.laneID, pt, sRelativeToVehicle, tRelativeToVehicle);
        }
        else
        {
            tRelativeToVehicle = -1.0;

            // 这里是将两个点之间的 ENU 距离投影成 ST 距离
            auto computeRoadDistance = [](const SSD::SimPoint3D& p1, const SSD::SimPoint3D& p2, double azimuth) {
                double theta = azimuth * M_PI / 180.0;
                double dx = p2.x - p1.x;
                double dy = p2.y - p1.y;
                return std::abs(dx * cos(theta) + dy * sin(theta));
                };
            sRelativeToVehicle = mainVehicle.s + computeRoadDistance(mainVehicle.pt, pt, mainVehicle.laneAzimuth);
        }

        /* 计算相对于自身所在车道的 ST 坐标 */
        SimOneAPI::GetLaneST(laneID, pt, sRelativeToLane, tRelativeToLane);

        /* 计算相对于自身所在道路的 ST 坐标 */
        double z = 0.0f;
        SimOneAPI::GetRoadST(laneID, pt, sRelativeToRoad, tRelativeToRoad, z);

        SimOneAPI::GetLaneWidth(laneID, pt, laneWidth);
        roadWidth = getRoadWidth(laneID, pt);

        getObstacleLaneSidePosition(pt, leftLanePt, rightLanePt);
        getObstacleRoadSidePosition(pt, leftRoadPt, rightRoadPt);

        if (FlagType::isCrosswalkExist && isObstacleAmongCrosswalk(tl, tr, bl, br))
        {
            type = Type::AmongCrosswalk;
        }
        else
        {
            type = Type::Static;
        }
    }
}

// 计算到主车最近的障碍物
bool calculateNearestObstacle(const std::vector<Obstacle>& list, Obstacle& obstacle, double& distance)
{
    if (list.empty()) { return false; }

    int minIndex = -1;
    float minDistance = std::numeric_limits<float>::max();

    for (size_t i = 0; i < list.size(); ++i) {
        float distance = std::fabs(list[i].sRelativeToVehicle - mainVehicle.s);
        if (distance < minDistance) {
            minDistance = distance;
            minIndex = i;
        }
    }

    if (minIndex == -1) { return false; }

    obstacle = list[minIndex];
    distance = minDistance;
    return true;
}

// 找到所有在主车后方的障碍物
void findObstacleBehind(const std::vector<Obstacle>& list, std::vector<size_t>& behindIndexList)
{
    for (size_t i = 0, ie = list.size(); i < ie; ++i)
    {
        Obstacle& obstacle = obstacleList[i];

        if (obstacle.isSameRoadWithMainVehicle && obstacle.sRelativeToVehicle < mainVehicle.s && obstacle.type == Obstacle::Type::Horizental)
        {
            behindIndexList.push_back(i);
        }
    }
}

// 找到主车后方距离主车最近的障碍物
bool findNearestObstacleBehind(const std::vector<Obstacle>& list, const std::vector<size_t>& indexList, size_t& index, float& distance)
{
    if (indexList.empty() || list.empty()) return false;

    int minIndex = 0;
    float minDistance = std::numeric_limits<float>::max();
    for (size_t i = 0; i < indexList.size(); ++i) {
        float distance = mainVehicle.s - list[indexList.at(i)].sRelativeToVehicle;
        if (distance < minDistance) {
            minDistance = distance;
            minIndex = indexList.at(i);
        }
    }
    index = minIndex;
    distance = minDistance;

    return true;
}

// 计算跟车的速度
float caculateFollowingSpeed(const std::vector<Obstacle>& obstacleList, int index)
{
    Obstacle potentialObstacle;
    double minDistance;
    float speed = 3.0f;
    float distance_expect = (index == case_stop_followID) ? 45 : 2;
    LOG << "distance_expect:" << distance_expect;
    if (calculateNearestObstacle(obstacleList, potentialObstacle, minDistance))
    {
        if (potentialObstacle.type == Obstacle::Type::Static)
        {
            float d0 = 0.05272 * mainVehicle.speed * mainVehicle.speed + 0.06714 * mainVehicle.speed - 0.02188; // d0 是刹车距离
            LOG << "minDistance:" << minDistance;
            LOG << "d0:" << d0;
            if ((minDistance - 4.38046) <= (d0 + distance_expect)) // d0 + x 表示要在停止线 x 米前停下来， - 4.38046 代表减去质心到车头的距离
            {
                speed = 0; // 立即停止加速（刹车）
            }
        }
        else
        {
            LOG << "potentialObstacle.velocity = " << potentialObstacle.velocity;
            LOG << "minDistance = " << minDistance;

            AdaptiveFollowing adaptiveFollowing(followingKp, followingKi, followingKd, 45); // 跟车 PID
            adaptiveFollowing.update(potentialObstacle.velocity, minDistance); // 跟车速度环
            speed = (adaptiveFollowing.getSpeed() > 3.5) ? adaptiveFollowing.getSpeed() : 3.5;//给到速度,并且使得速度大于最小速度
        }
    }
    return speed;
}

// 计算超车速度（这个只能处理主车即将变道，而主车后方又有一辆快速行驶的对手车辆的情况，不能处理移动的半停止线）
void calculateOvertakingSpeed(std::vector<Obstacle>& obstacleList)
{
    std::vector<size_t> obstacleBehindIndexList;
    size_t potentialObstacleBehindIndex = 0;
    float potentialObstacleBehindDistance = 0.0f;
    findObstacleBehind(obstacleList, obstacleBehindIndexList);

    if (findNearestObstacleBehind(obstacleList, obstacleBehindIndexList, potentialObstacleBehindIndex, potentialObstacleBehindDistance))
    {
        Obstacle& obstacle = obstacleList[potentialObstacleBehindIndex];
        if (isPathRequireLaneChange(targetPath) && isTrajInterfere(targetPath, obstacle))
        {
            float v1 = obstacle.velocityPlanar;
            float L0 = 4.7987f;
            float L1 = obstacle.length;
            float Lx = 1.f;
            float baseLa = -((mainVehicle.s - obstacle.sRelativeToVehicle) - 0.5f * (L0 + L1) - Lx);
            float limitBaseLa = MAX(baseLa, 0);
            float La = limitBaseLa + chasingLimitDist;
            float v0 = calculateTargetOvertakingVelocity(v1, mainVehicle.s, obstacle.sRelativeToVehicle, L0, L1, La, Lx);
            pControl->throttle = v0;
        }
    }
}