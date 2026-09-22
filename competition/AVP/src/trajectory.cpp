#include "trajectory.h"
#include "process.h"
#include "UtilAVP.hpp"
#include "logger.hpp"
#include "process.h"
#include "define.h"

Trajectory::Trajectory(void) : startPoint({ 0.0, 0.0, 0.0 }), endPoint({ 0.0, 0.0, 0.0 }) { trajectory.clear(); }

Trajectory::Trajectory(const SSD::SimPoint3D& startPt, const SSD::SimPoint3D& endPt, const ParkingSlot& slot, Type type) : startPoint(startPt), endPoint(endPt)
{
    trajectory.clear();
    SSD::SimPoint3DVector controlPoints; // 控制点的集合

    auto& knots = slot.boundaryKnots;
    auto& center = targetParkingSpace.pt;
    auto& coordinate = slot.localCoordinate;

    float slotL = UtilAVP::PlanarDistance(knots[0], knots[1]);
    float slotW = UtilAVP::PlanarDistance(knots[0], knots[3]);

    if (type == Type::eForwarding)
    {
        float deltaX = std::abs(coordinate.toLocal(knots[0]).x - coordinate.toLocal(startPt).x);
        controlPoints.push_back(startPt);
        controlPoints.push_back(coordinate.translateInLocal(startPt, 0.25f * deltaX, 0.0f));
        float dx = deltaX / 10;
        for (int i = 0; i < 10; i++)
        {
            controlPoints.push_back(coordinate.translateInLocal(startPt, 0.5 * dx * i, 0.0f));
        }
        controlPoints.push_back(coordinate.translateInLocal(knots[0], 0.0f, 1.0f)); //（减速点）
        
        if (slot.type == ParkingSlot::Type::Vertical)
        {
            controlPoints.push_back(coordinate.translateInLocal(knots[3], 0.5f, 1.0f));
            controlPoints.push_back(coordinate.translateInLocal(knots[3], 1.0f, 2.0f));
        }
        else
        {
            controlPoints.push_back(coordinate.translateInLocal(knots[3], 0.0f, 1.0f));
            controlPoints.push_back(coordinate.translateInLocal(knots[3], 1.0f, 3.0f));
        }
    }
    else if (type == Type::eReversing)
    {
        if (slot.type == ParkingSlot::Type::Vertical)
        {
            controlPoints.push_back(coordinate.translateInLocal(knots[3], 1.0f, 3.0f));
            controlPoints.push_back(coordinate.translateInLocal(knots[3], 0.0f, 1.0f));

            float dx = 0.4f;
            SSD::SimPoint3D knot2Translate = coordinate.translateInLocal(knots[3], 0.0f, 1.5f);
            float dy = (coordinate.toLocal(knot2Translate).y - coordinate.toLocal(center).y) / 5.0f;

            for (int i = 1; i <= 5; ++i)
            {
                controlPoints.push_back(coordinate.translateInLocal(SSD::SimPoint3D(center.x, knots[3].y, 0.0), dx, 1.0f - i * dy));
            }
        }
        else
        {
            controlPoints.push_back(coordinate.translateInLocal(knots[3], -0.5f, 2.0f));
            controlPoints.push_back(coordinate.translateInLocal(knots[3], -1.5f, 1.0f));

            SSD::SimPoint3D centerTranslate = coordinate.translateInLocal(center, -0.5f, 0.0f);
            float dx = (coordinate.toLocal(knots[3]).x - coordinate.toLocal(centerTranslate).x) / 5.0f;

            for (int i = 1; i <= 5; ++i)
            {
                controlPoints.push_back(coordinate.translateInLocal(SSD::SimPoint3D(knots[3].x, center.y, 0.0), -1.5f - i * dx, 0.0f));
            }
        }
    }
    else if (type == Type::eLeaving)
    {
        if (slot.type == ParkingSlot::Type::Vertical)
        {
            controlPoints.push_back(coordinate.translateInLocal(knots[0], 0.5f * slotW, -0.5f * slotL));
            controlPoints.push_back(coordinate.translateInLocal(knots[0], 0.5f * slotW, 0.4f));
            controlPoints.push_back(coordinate.translateInLocal(SSD::SimPoint3D(knots[0].x, endPt.y, 0.0), 0.5f * slotW, 1.0f));

            float delta = coordinate.toLocal(endPt).x - coordinate.toLocal(knots[3]).x;
            float nDelta = delta / 10.0f;

            for (int i = 1; i <= 20; ++i)
            {
                controlPoints.push_back(coordinate.translateInLocal(knots[3], nDelta * i / 2.0f, 0.4f));
            }
        }
        else
        {
            controlPoints.push_back(center);
            controlPoints.push_back(coordinate.translateInLocal(center, 0.5f, 0.4f));
            controlPoints.push_back(coordinate.translateInLocal(knots[3], -0.5f, 3.0f));

            float delta = coordinate.toLocal(endPt).x - coordinate.toLocal(knots[3]).x;
            float nDelta = delta / 10.0f;

            for (int i = 1; i <= 20; ++i)
            {
                controlPoints.push_back(coordinate.translateInLocal(SSD::SimPoint3D(knots[3].x, endPt.y, 0.0), nDelta * i / 2.0f, 0.0f));
            }
        }
    }

    for (SSD::SimPoint3D& controlPoint : controlPoints)
    {
        if (slot.type == ParkingSlot::Type::Vertical) controlPoint = coordinate.translateInLocal(controlPoint, 0.06f, 0.0f);
        else controlPoint = coordinate.translateInLocal(controlPoint, 0.16f, -0.2f);
    }

    trajectory = controlPoints;
}