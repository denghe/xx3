#pragma once
#include "pch.h"
using XY = xx::XY;

struct Snake;
struct Game : xx::GameBase {
	static constexpr float cFps{ 120 };
	static constexpr float cDelta{ 1.f / cFps };

	struct {
		std::array<xx::Frame, 3> imgs;
	} res;

	xx::Shared<xx::Node> ui;
	xx::Camera cam;
	xx::Shared<Snake> snake;
	xx::Rnd rnd;
	xx::Shared<xx::Label> deathLabel;  // 死亡提示标签

	void Init() override;
	void GLInit() override;
	void Update() override;
	void Delay() override;
	void Stat() override;
	void OnResize(bool modeChanged_) override;
};
extern Game gg;
