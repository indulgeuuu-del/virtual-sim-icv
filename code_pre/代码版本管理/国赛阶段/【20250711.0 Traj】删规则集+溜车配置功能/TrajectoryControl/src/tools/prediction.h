#include <vector>
#include <cmath>
#include "Service/SimOneIOStruct.h"


// 对障碍物的预测轨迹进行延长，GM(1,1)
void predictTrajectory(
    const SimOne_Data_Vec3f trajectory[],   // 原始轨迹点数组
    int inputSize,                          // 原始轨迹点个数
    int predictSize,                        // 要预测的点数
    std::vector<SimOne_Data_Vec3f>& predictedPoints // 输出结果
);