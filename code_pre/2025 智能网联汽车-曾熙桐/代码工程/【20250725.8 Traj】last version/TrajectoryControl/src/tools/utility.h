#pragma once
#include <cstdlib>
#include <windows.h>
#include <algorithm>
#include <Lmcons.h>
#include "SimOneServiceAPI.h"
#include "json.hpp"

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            globalLogger(Logger::Color::BrightMagenta) << "Assertion failed: (" #condition ")\n" << "Message: " << message << "\n" << "Location: " << __FILE__ << ":" << __LINE__ << " (" << __FUNCTION__ << ")"; \
            std::abort(); \
        } \
    } while (false)

#define IS_IN(value, container) (std::find((container).begin(), (container).end(), (value)) != (container).end())

#define INVALID_SIZE_T (std::numeric_limits<size_t>::max())

extern void getCaseSpeed(const nlohmann::json& js, int id, float& targetSpeed, float& minSpeed, float& maxSpeed);
extern int getCaseIdx(void);
extern SSD::SimString m_SampleGetNearMostLane(const SSD::SimPoint3D& pos);

extern void saveSimPoint3DVectorToTxt(const SSD::SimPoint3DVector& points, const std::string& filename);

extern float calculateFps(uint64_t frameCount, float currentTimeMs);