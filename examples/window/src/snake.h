#pragma once
#include "pch.h"
#include <deque>

using XY = xx::XY;

struct Snake {
	// 配置常数
	static constexpr int32_t cGridSize{ 64 };      // 显示网格大小（用于游戏区域划分）
	static constexpr int32_t cInitBodyLen{ 3 };
	static constexpr float cMaxPixelsPerSecond{ 128.0f };  // 基础速度：128 像素/秒
	static constexpr float cBodySegmentLength{ 64.0f };    // 每个身体段的长度（像素）
	
	// 方向枚举
	enum class Direction : int32_t {
		Right = 0,
		Down = 1,
		Left = 2,
		Up = 3
	};

	// 贪吃蛇状态
	xx::Frame bodyFrame;      // 蛇身纹理
	xx::Frame appleFrame;     // 苹果纹理
	xx::Camera* cam{};        // 摄像机指针
	XY gridStartPos;          // 游戏区域起始位置
	int32_t cols{}, rows{};   // 游戏网格尺寸（用于边界和苹果生成）

	// 蛇的像素级位置
	XY headPos;               // 蛇头当前位置（像素坐标）
	Direction dir{ Direction::Right };     // 当前方向
	Direction nextDir{ Direction::Right }; // 下一方向（缓冲）
	
	// 蛇身追踪：记录蛇身经过的路径点
	std::deque<XY> bodyPath;  // 蛇身路径点（按时间顺序）
	float bodyTraveledDistance{}; // 从蛇头往后计算的蛇身总长度

	// 食物位置
	XY applePos;              // 苹果像素坐标
	
	// 速度控制
	float currentSpeed{ cMaxPixelsPerSecond }; // 当前速度（像素/秒）
	float speedMultiplier{ 1.0f }; // 速度倍数
	
	bool alive{ true };       // 是否存活
	int32_t score{};          // 得分
	
	int32_t invincibleFrames{}; // 无敌帧数（吃到苹果后的短暂无敌时间）
	static constexpr int32_t cInvincibleFrameDuration{ 10 }; // 无敌持续帧数

	void Init(xx::Camera* cam_, XY designSize, xx::Frame bodyFrame_, xx::Frame appleFrame_);
	void HandleInput(bool up, bool down, bool left, bool right);
	void Update(float deltaTime);
	void Draw();
	void ChangeSpeed();  // 切换速度
	void Reset();        // 重置游戏

private:
	void SpawnApple();
	XY GridPosToPixelPos(int32_t gridX, int32_t gridY) const;
	void UpdateHeadPosition(float deltaTime);
	void UpdateBodyPath();
	XY GetBodySegmentPos(float distanceFromHead) const;
	bool CheckAppleCollision() const;
	bool CheckSelfCollision() const;
	void RecalculateSpeed();
};