#include "pch.h"
#include "snake.h"
#include "game.h"

void Snake::Init(xx::Camera* cam_, XY designSize, xx::Frame bodyFrame_, xx::Frame appleFrame_) {
	cam = cam_;
	bodyFrame = bodyFrame_;
	appleFrame = appleFrame_;

	// 计算游戏网格参数
	cols = static_cast<int32_t>(designSize.x) / cGridSize;
	rows = static_cast<int32_t>(designSize.y) / cGridSize;
	
	// 游戏区域起始位置（左上角）
	gridStartPos = XY{
		-designSize.x * 0.5f,
		-designSize.y * 0.5f
	};

	Reset();
}

void Snake::Reset() {
	bodyPath.clear();
	dir = Direction::Right;
	nextDir = Direction::Right;
	alive = true;
	score = 0;
	speedMultiplier = 1.0f;
	invincibleFrames = 0;
	bodyTraveledDistance = cBodySegmentLength * cInitBodyLen;

	// 初始蛇头位置：游戏区域中心
	int32_t cx = cols / 2;
	int32_t cy = rows / 2;
	headPos = GridPosToPixelPos(cx, cy);

	// 初始蛇身路径：蛇头向左延伸
	// 蛇头向右运动，所以身体在左边
	bodyPath.push_back(headPos - XY{ cBodySegmentLength * 3.0f, 0.0f }); // 蛇尾
	bodyPath.push_back(headPos - XY{ cBodySegmentLength * 2.0f, 0.0f });
	bodyPath.push_back(headPos - XY{ cBodySegmentLength * 1.0f, 0.0f }); // 蛇头前面的身体
	bodyPath.push_back(headPos); // 蛇头

	RecalculateSpeed();
	SpawnApple();
}

void Snake::SpawnApple() {
	// 随机生成苹果位置（网格坐标），确保不在蛇身上
	int maxAttempts = 100;
	int attempts = 0;
	
	while (attempts < maxAttempts) {
		int32_t rx = gg.rnd.Next(0, cols - 1);
		int32_t ry = gg.rnd.Next(0, rows - 1);
		applePos = GridPosToPixelPos(rx, ry);
		
		// 检查苹果是否与蛇身碰撞
		bool collision = false;
		for (float d = 0.0f; d < bodyTraveledDistance; d += 20.0f) {
			XY bodyPos = GetBodySegmentPos(d);
			XY diff = applePos - bodyPos;
			float distance = std::sqrtf(diff.x * diff.x + diff.y * diff.y);
			if (distance < cGridSize) {
				collision = true;
				break;
			}
		}
		
		if (!collision) {
			break;
		}
		
		attempts++;
	}
}

XY Snake::GridPosToPixelPos(int32_t gridX, int32_t gridY) const {
	return gridStartPos + XY{
		(static_cast<float>(gridX) + 0.5f) * cGridSize,
		(static_cast<float>(gridY) + 0.5f) * cGridSize
	};
}

void Snake::HandleInput(bool up, bool down, bool left, bool right) {
	// 处理输入：方向控制（防止反向运动）
	if (up) {
		if (dir != Direction::Down) nextDir = Direction::Up;
	}
	if (down) {
		if (dir != Direction::Up) nextDir = Direction::Down;
	}
	if (left) {
		if (dir != Direction::Right) nextDir = Direction::Left;
	}
	if (right) {
		if (dir != Direction::Left) nextDir = Direction::Right;
	}
}

void Snake::UpdateHeadPosition(float deltaTime) {
	// 更新蛇头方向
	dir = nextDir;

	// 计算蛇头移动距离
	float moveDistance = currentSpeed * speedMultiplier * deltaTime;

	// 计算移动方向向量
	XY moveDir{ 0.0f, 0.0f };
	switch (dir) {
	case Direction::Right:
		moveDir = { 1.0f, 0.0f };
		break;
	case Direction::Down:
		moveDir = { 0.0f, 1.0f };
		break;
	case Direction::Left:
		moveDir = { -1.0f, 0.0f };
		break;
	case Direction::Up:
		moveDir = { 0.0f, -1.0f };
		break;
	}

	// 更新蛇头位置
	headPos = headPos + moveDir * moveDistance;

	// 边界环绕
	XY gameAreaSize{ static_cast<float>(cols) * cGridSize, static_cast<float>(rows) * cGridSize };
	if (headPos.x < gridStartPos.x) headPos.x += gameAreaSize.x;
	if (headPos.x > gridStartPos.x + gameAreaSize.x) headPos.x -= gameAreaSize.x;
	if (headPos.y < gridStartPos.y) headPos.y += gameAreaSize.y;
	if (headPos.y > gridStartPos.y + gameAreaSize.y) headPos.y -= gameAreaSize.y;

	// 添加蛇头位置到路径
	bodyPath.push_back(headPos);
	
	// 更新蛇身长度
	UpdateBodyPath();
}

void Snake::UpdateBodyPath() {
	// 计算从蛇头往后需要保留的路径长度
	float pathDistance = 0.0f;
	int32_t keepIndex = bodyPath.size() - 1;

	// 从蛇头往后遍历，计算距离直到达到蛇身总长度
	for (int32_t i = (int32_t)bodyPath.size() - 1; i > 0; --i) {
		XY diff = bodyPath[i] - bodyPath[i - 1];
		float segmentDist = std::sqrtf(diff.x * diff.x + diff.y * diff.y);
		pathDistance += segmentDist;

		if (pathDistance >= bodyTraveledDistance) {
			keepIndex = i - 1;
			break;
		}
	}

	// 移除超出蛇身长度的路径点
	while (bodyPath.size() > 1 && keepIndex > 0) {
		bodyPath.pop_front();
		keepIndex--;
	}
}

XY Snake::GetBodySegmentPos(float distanceFromHead) const {
	// 获取距蛇头指定距离的身体段位置
	if (bodyPath.empty()) return headPos;
	if (distanceFromHead <= 0.0f) return headPos;

	float remainingDistance = distanceFromHead;
	
	// 从蛇头往后遍历路径
	for (int32_t i = (int32_t)bodyPath.size() - 1; i > 0; --i) {
		XY segStart = bodyPath[i];
		XY segEnd = bodyPath[i - 1];
		XY diff = segEnd - segStart;
		float segmentDist = std::sqrtf(diff.x * diff.x + diff.y * diff.y);

		if (remainingDistance <= segmentDist) {
			// 在这个线段内
			float ratio = remainingDistance / segmentDist;
			return segStart + (segEnd - segStart) * ratio;
		}

		remainingDistance -= segmentDist;
	}

	// 超出蛇身长度，返回蛇尾
	return bodyPath.front();
}

bool Snake::CheckAppleCollision() const {
	// 检查蛇头与苹果的碰撞（距离小于64像素，即一个网格大小）
	XY diff = headPos - applePos;
	float distance = std::sqrtf(diff.x * diff.x + diff.y * diff.y);
	return distance < cGridSize * 0.5f;  // 碰撞半径为半个网格
}

bool Snake::CheckSelfCollision() const {
	// 检查蛇头与蛇身的碰撞
	// 从蛇头往后跳过一定距离（给蛇头一些宽松度）
	float checkDistance = cBodySegmentLength * 3.0f; // 至少跳过3个身体段
	
	for (float d = checkDistance; d < bodyTraveledDistance; d += 10.0f) {
		XY bodyPos = GetBodySegmentPos(d);
		XY diff = headPos - bodyPos;
		float distance = std::sqrtf(diff.x * diff.x + diff.y * diff.y);
		if (distance < 16.0f) // 碰撞半径
			return true;
	}
	return false;
}

void Snake::Update(float deltaTime) {
	if (!alive) return;

	// 更新蛇头位置
	UpdateHeadPosition(deltaTime);

	// 检查苹果碰撞
	if (CheckAppleCollision()) {
		score++;
		bodyTraveledDistance += cBodySegmentLength;
		invincibleFrames = cInvincibleFrameDuration; // 设置无敌时间
		SpawnApple();
	}

	// 减少无敌帧计数
	if (invincibleFrames > 0) {
		invincibleFrames--;
	}

	// 检查自碰撞（无敌帧内不检查）
	if (invincibleFrames <= 0 && CheckSelfCollision()) {
		alive = false;
	}
}

void Snake::Draw() {
	if (!cam) return;

	// 目标显示尺寸：64x64 像素
	static constexpr float cTargetRenderSize{ 64.0f };

	// 绘制蛇身（从蛇头开始，每隔一定距离绘制一个节点）
	int32_t segmentCount = static_cast<int32_t>(bodyTraveledDistance / cBodySegmentLength) + 1;
	for (int32_t i = 0; i < segmentCount; ++i) {
		float distFromHead = i * cBodySegmentLength;
		if (distFromHead > bodyTraveledDistance) break;

		XY segmentPos = GetBodySegmentPos(distFromHead);

		// 获取蛇身纹理的实际大小
		XY frameSize = bodyFrame.Size();
		
		// 计算缩放因子：目标尺寸 / 纹理实际尺寸
		float scale = cTargetRenderSize / frameSize.x;
		
		gg.Quad().DrawFrame(bodyFrame, cam->ToGLPos(segmentPos), scale * cam->scale);
	}

	// 绘制苹果
	{
		// 获取苹果纹理的实际大小
		XY frameSize = appleFrame.Size();
		
		// 计算缩放因子：目标尺寸 / 纹理实际尺寸
		float scale = cTargetRenderSize / frameSize.x;
		
		gg.Quad().DrawFrame(appleFrame, cam->ToGLPos(applePos), scale * cam->scale);
	}
}

void Snake::ChangeSpeed() {
	// 在三个速度倍数之间循环：0.5x、1.0x、1.5x
	if (speedMultiplier < 1.0f) {
		speedMultiplier = 1.0f;  // 正常速度
	} else if (speedMultiplier < 1.5f) {
		speedMultiplier = 1.5f;  // 快速
	} else {
		speedMultiplier = 0.5f;  // 慢速
	}
}

void Snake::RecalculateSpeed() {
	// 基础速度恒定 128 像素/秒
	currentSpeed = cMaxPixelsPerSecond;
}