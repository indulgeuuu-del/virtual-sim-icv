#pragma once
#include <vector>
#include "algorithm"
#include "obstacle.h"
#include "define.h"
#include "SSD/SimPoint3D.h"

// 单车道变道/停止线识别器
class ModeRecognizer {
public:
    enum class Mode {
        CREATE_STOP_LINE, // 创建停止线
        SINGE_LANE_CHANGE // 单车道变道
    };

    struct FeasibleSegment {
        double length;
        double s;
        double t;
        SSD::SimString laneID;

        FeasibleSegment(double l, double s_, double t_, SSD::SimString id) : length(l), s(s_), t(t_), laneID(id) { }
    };

    // 处理 S 坐标相同组的障碍物
    static Mode process(const std::vector<size_t>& inputIndex, SSD::SimPoint3D& feasiblePoint, float threshold = MAIN_VEHICLE_WIDTH)
    {
        if (obstacleList.empty() || inputIndex.empty()) return Mode::CREATE_STOP_LINE;

        std::vector<Obstacle> cluster;
        for (size_t i = 0, ie = inputIndex.size(); i < ie; ++i) cluster.emplace_back(obstacleList.at(inputIndex[i]));

        // 将组内障碍物按 T 坐标从大到小排列
        std::sort(cluster.begin(), cluster.end(), [](auto& a, auto& b) { return a.tRelativeToLane > b.tRelativeToLane; });

        double halfWidth = 0.5 * cluster.front().laneWidth;
        double sFeasible = cluster.front().sRelativeToLane; // 以第一个障碍物的 S 坐标为初始值

        /* 计算每一段之间的距离 */
        std::vector<FeasibleSegment> feasibleSegments;

        // 1）道路上边界 -> 第一个障碍物
        {
            double distanceUp = halfWidth - cluster[0].tRelativeToLane - 0.5 * cluster[0].width;
            if (distanceUp > threshold) // 距离大于阈值，则代表主车可以通过
            {
                // 计算控制点的 ST 坐标（中点）
                double tFeasible = halfWidth - 0.5 * distanceUp;
                feasibleSegments.emplace_back(distanceUp, sFeasible, tFeasible, cluster[0].laneID);
            }
        }

        // 2）障碍物两两之间
        for (size_t i = 0; i + 1 < cluster.size(); ++i)
        {
            double distanceBetween = std::abs(cluster[i].tRelativeToLane - cluster[i + 1].tRelativeToLane) - 0.5 * (cluster[i].width + cluster[i + 1].width);
            if (distanceBetween > threshold)
            {
                double tFeasible = 0.5 * (cluster[i].tRelativeToLane + cluster[i + 1].tRelativeToLane);
                feasibleSegments.emplace_back(distanceBetween, sFeasible, tFeasible, cluster[i].laneID);
            }
        }

        // 3）最后一个障碍物 -> 道路下边界
        double distanceLow = halfWidth + cluster.back().tRelativeToLane - 0.5 * cluster.back().width;
        if (distanceLow > threshold)
        {
            double tFeasible = 0.5 * distanceLow - halfWidth;
            feasibleSegments.emplace_back(distanceLow, sFeasible, tFeasible, cluster.back().laneID);
        }

        // 找出最长的可行段
        if (!feasibleSegments.empty())
        {
            const FeasibleSegment* maxSegment = &feasibleSegments.front();
            for (const auto& segmant : feasibleSegments)
            {
                if (segmant.length > maxSegment->length)
                {
                    maxSegment = &segmant;
                }
            }

            SSD::SimPoint3D dir;
            SimOneAPI::GetInertialFromLaneST(maxSegment->laneID, maxSegment->s, maxSegment->t, feasiblePoint, dir);
            return Mode::SINGE_LANE_CHANGE;
        }

        // 所有段都小于阈值，就使用停止线模式
        return Mode::CREATE_STOP_LINE;
    }
};