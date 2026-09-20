#include "prediction.h"
#include "logger.hpp"

// 累加生成
std::vector<double> AGO(const std::vector<double>& data) {
    std::vector<double> result(data.size());
    result[0] = data[0];
    for (size_t i = 1; i < data.size(); ++i) {
        result[i] = result[i - 1] + data[i];
    }
    return result;
}

// 均值生成
std::vector<double> calcMeanSeq(const std::vector<double>& AGOdata) {
    std::vector<double> Z(AGOdata.size() - 1);
    for (size_t i = 1; i < AGOdata.size(); ++i) {
        Z[i - 1] = 0.5 * (AGOdata[i - 1] + AGOdata[i]);
    }
    return Z;
}

std::pair<double, double> estimateAB(
    const std::vector<double>& original,
    const std::vector<double>& Z
) {
    double sumZ = 0, sumZ2 = 0, sumY = 0, sumZY = 0;
    for (size_t i = 0; i < Z.size(); ++i) {
        sumZ += Z[i];
        sumZ2 += Z[i] * Z[i];
        sumY += original[i + 1];
        sumZY += Z[i] * original[i + 1];
    }
    double N = (double)Z.size();
    double D = N * sumZ2 - sumZ * sumZ;
    if (fabs(D) < 1e-8) {
        // 样本退化：常数序列或数据不足 → a=0, b取最后一个值
        return { 0.0, original.back() };
    }
    double a = -(N * sumZY - sumZ * sumY) / D;
    double b = (sumY + a * sumZ) / N;
    return { a, b };
}

// GM(1,1)预测
std::vector<double> predictGM(
    const std::vector<double>& original,
    int predictNum
) {
    int n = original.size();
    if (n < 4) {
        // 样本太少，直接常数外推
        return std::vector<double>(predictNum, original.back());
    }
    auto AGOdata = AGO(original);
    auto Z = calcMeanSeq(AGOdata);
    auto [a, b] = estimateAB(original, Z);

    if (fabs(a) < 1e-8) {
        // a≈0 → 退化常数模型
        return std::vector<double>(predictNum, original.back());
    }

    // 正常的 GM(1,1) 预测
    std::vector<double> x1_hat(predictNum + 1);
    x1_hat[0] = original[0];
    for (int k = 1; k <= predictNum; ++k) {
        x1_hat[k] = (original[0] - b / a) * exp(-a * k) + b / a;
    }
    std::vector<double> x0_hat(predictNum);
    for (int i = 0; i < predictNum; ++i) {
        x0_hat[i] = x1_hat[i + 1] - x1_hat[i];
    }
    return x0_hat;
}

// 线性残差判断
double linearResidual(const std::vector<double>& data) {
    int n = data.size();
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (int i = 0; i < n; ++i) {
        sumX += i;
        sumY += data[i];
        sumXY += i * data[i];
        sumX2 += i * i;
    }
    double a = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    double b = (sumY - a * sumX) / n;

    double error = 0;
    for (int i = 0; i < n; ++i) {
        double y_fit = a * i + b;
        error += pow((a * i + b) - data[i], 2);
    }
    return error / n;
}

void predictTrajectory(
    const SimOne_Data_Vec3f trajectory[],   // 原始轨迹点数组
    int inputSize,                          // 原始轨迹点个数
    int predictSize,                        // 要预测的点数
    std::vector<SimOne_Data_Vec3f>& predictedPoints // 输出结果
) {
    std::vector<double> xList, yList;
    for (int i = 0; i < inputSize; ++i) {
        xList.push_back(trajectory[i].x);
        yList.push_back(trajectory[i].y);
    }

    std::vector<double> predX = predictGM(xList, predictSize);
    std::vector<double> predY = predictGM(yList, predictSize);

    predictedPoints.clear();
    for (int i = 0; i < predictSize; ++i) {
        SimOne_Data_Vec3f pt;
        pt.x = predX[i];
        pt.y = predY[i];
        pt.z = 0;
        predictedPoints.push_back(pt);
    }
}
