#pragma once
#include <cmath>

/* 位置式 PID 控制器 */
typedef class PositionalPID {
public:
	PositionalPID(float _kp, float _ki, float _kd) :
		kp(_kp), ki(_ki), kd(_kd), errorPrev(0.0f), integral(0.0f) {}

	float calculate(float current, float target)
	{
		float error = target - current;
		float output = kp * error + ki * integral + kd * (error - errorPrev);
		integral += error;
		errorPrev = error;
		return output;
	}

	float calculate(float error)
	{
		float output = kp * error + ki * integral + kd * (error - errorPrev);
		integral += error;
		errorPrev = error;
		return output;
	}

	void set(float _kp, float _ki, float _kd)
	{
		kp = _kp;
		ki = _ki;
		kd = _kd;
	}

	void reset(void)
	{
		errorPrev = 0.0f;
		integral = 0.0f;
	}

private:
	float kp; // 比例系数
	float ki; // 积分系数
	float kd; // 微分系数
	float errorPrev; // 上一次误差
	float integral; // 积分项
} posPid_t;

/* 增量式 PID 控制器 */
typedef class IncrementalPID {
public:
	IncrementalPID(float _kp, float _ki, float _kd) :
		kp(_kp), ki(_ki), kd(_kd), errorPrev(0.0f), errorPrevPrev(0.0f) {}

	float calculate(float current, float target)
	{
		float error = target - current;
		float output = kp * (error - errorPrev) + ki * error + kd * (error - 2 * errorPrev + errorPrevPrev);
		errorPrevPrev = errorPrev;
		errorPrev = error;
		return output;
	}

	float calculate(float error)
	{
		float output = kp * (error - errorPrev) + ki * error + kd * (error - 2 * errorPrev + errorPrevPrev);
		errorPrevPrev = errorPrev;
		errorPrev = error;
		return output;
	}

	void set(float _kp, float _ki, float _kd)
	{
		kp = _kp;
		ki = _ki;
		kd = _kd;
	}

	void reset(void)
	{
		errorPrev = 0.0f;
		errorPrevPrev = 0.0f;
	}

private:
	float kp; // 比例系数
	float ki; // 积分系数
	float kd; // 微分系数
	float errorPrev; // 上一次误差
	float errorPrevPrev; // 上上一次误差
} incPid_t;

/* 动态 PD 控制器 */
typedef class DynamicPD {
public:
	DynamicPD(float _baseKp, float _motionKp, float _kd) :
		baseKp(_baseKp), motionKp(_motionKp), kd(_kd), errorPrev(0.0f) { }

	float calculate(float current, float target)
	{
		float error = target - current;
		float output = (baseKp + motionKp * fabs(error)) * error + kd * (error - errorPrev);
		errorPrev = error;
		return output;
	}

	float calculate(float error)
	{
		float output = (baseKp + motionKp * fabs(error)) * error + kd * (error - errorPrev);
		errorPrev = error;
		return output;
	}

	void set(float _baseKp, float _motionKp, float _kd)
	{
		baseKp = _baseKp;
		motionKp = _motionKp;
		kd = _kd;
	}

	void reset(void)
	{
		errorPrev = 0.0f;
	}

private:
	float baseKp;     // 基础比例系数
	float motionKp;   // 动态比例系数
	float kd;         // 微分系数
	float errorPrev;  // 上一次误差
} dynamicPd_t;

/* 自适应跟车器 */
class AdaptiveFollowing {
public:
	AdaptiveFollowing(float kp, float ki, float kd, float _safeDistance) : pid(kp, ki, kd), safeDistance(_safeDistance), currentSpeed(0.0f) {}

	void update(float frontCarSpeed, float frontCarDistance)
	{
		float speedAdjustment = pid.calculate(frontCarDistance, safeDistance);
		currentSpeed = frontCarSpeed + speedAdjustment;
		if (currentSpeed < 0.0f) currentSpeed = 0.0f; // 防止倒车
	}

	float getSpeed() const { return currentSpeed; }

private:
	PositionalPID pid;
	float safeDistance;
	float currentSpeed;
};

/* 反向跟车器 */
class AdaptiveLeading {
public:
	AdaptiveLeading(float kp, float ki, float kd, float _safeDistance) : pid(kp, ki, kd), safeDistance(_safeDistance), currentSpeed(0.0f) {}

	void update(float rearCarSpeed, float rearCarDistance)
	{
		float speedAdjustment = pid.calculate(rearCarDistance, safeDistance);
		currentSpeed = rearCarSpeed - speedAdjustment;
		if (currentSpeed < 0.0f) currentSpeed = 0.0f; // 防止倒车
	}

	float getSpeed() const { return currentSpeed; }

private:
	PositionalPID pid;
	float safeDistance;
	float currentSpeed;
};