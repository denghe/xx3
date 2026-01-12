#include "pch.h"
#include "game.h"
#include "snake.h"
Game gg;

int32_t main() {
	return gg.Run();
}

void Game::Init() {
	DisableIME();
	title = "贪吃蛇 - Snake Game";
	windowSize = designSize = { 1024, 768 };
}

void Game::GLInit() {
	// 加载资源
	res.imgs[0] = LoadTexture("res/heart.png");   // 蛇身
	res.imgs[1] = LoadTexture("res/1.png");       // 食物
	res.imgs[2] = LoadTexture("res/2.png");       // 备用

	// 纹理打包
	{
		xx::RectPacker tp;
		for (int32_t i = 0; i < sizeof(res) / sizeof(xx::Frame); ++i) {
			tp.tfs.Add((xx::TinyFrame*)&((xx::Frame*)&res)[i]);
		}
		tp.AutoPack();
	}

	// 设置背景色为深灰色
	clearColor = xx::RGBA8{ 40, 40, 40, 255 };

	// 初始化摄像机
	cam.Init(scale, 1.f, {});

	// 初始化 UI
	ui.Emplace()->InitRoot(scale);

	// 标题
	ui->Make<xx::Label>()->Init(1, p5 + XY{ 0, -340 }, a5, 48)("Snake Game");

	// 分数显示
	ui->Make<xx::Label>()->Init(2, p5 + XY{ -400, -300 }, a5, 32)("Score: 0");

	// 速度按钮
	ui->Make<xx::LabelButton>()->Init(3, p5 + XY{ -400, -250 }, a5, 32)("Speed").onClicked = [this] {
		if (snake) snake->ChangeSpeed();
	};

	// 重置按钮
	ui->Make<xx::LabelButton>()->Init(4, p5 + XY{ -400, -200 }, a5, 32)("Reset").onClicked = [this] {
		if (snake) snake->Reset();
	};

	// 说明文本
	ui->Make<xx::Label>()->Init(5, p5 + XY{ 0, 330 }, a5, 24)
		("Arrow Keys or WASD to move | Speed/Reset buttons | ESC to exit");

	// 初始化贪吃蛇
	snake.Emplace()->Init(&cam, designSize, res.imgs[0], res.imgs[1]);
}

void Game::Update() {
	// 输入处理
	if (keyboard[GLFW_KEY_ESCAPE]) {
		running = false;
		return;
	}

	// 摄像机控制（可选）
	if (mouse[GLFW_MOUSE_BUTTON_WHEEL_UP](0.01f) || keyboard[GLFW_KEY_Z](0.01f)) {
		cam.SetLogicScale(cam.logicScale + 0.001f);
	}
	if (mouse[GLFW_MOUSE_BUTTON_WHEEL_DOWN](0.01f) || keyboard[GLFW_KEY_X](0.01f)) {
		cam.SetLogicScale(cam.logicScale - 0.001f);
	}

	// 处理蛇的输入
	if (snake) {
		bool up = keyboard[GLFW_KEY_UP] || keyboard[GLFW_KEY_W];
		bool down = keyboard[GLFW_KEY_DOWN] || keyboard[GLFW_KEY_S];
		bool left = keyboard[GLFW_KEY_LEFT] || keyboard[GLFW_KEY_A];
		bool right = keyboard[GLFW_KEY_RIGHT] || keyboard[GLFW_KEY_D];
		snake->HandleInput(up, down, left, right);
	}

	// 游戏逻辑更新（传递实际帧时间）
	if (snake) {
		snake->Update(delta);
	}

	// 绘制贪吃蛇
	if (snake) {
		snake->Draw();
	}

	// 绘制死亡提示
	if (snake && !snake->alive) {
		// 创建临时标签显示死亡信息
		if (!deathLabel) {
			deathLabel = ui->Make<xx::Label>();
			deathLabel->Init(100, p5 + XY{ 0, -380 }, a5, 64)("GAME OVER");
		}
	}

	// 绘制 UI
	SetBlendPremultipliedAlpha(false);
	DrawNode(ui);
}

void Game::Delay() {
	// 帧率控制（当前禁用）
#if 1
	SleepSecs(cDelta - (glfwGetTime() - time));
#endif
}

void Game::OnResize(bool modeChanged_) {
	ui->Resize(scale);
	cam.SetBaseScale(scale);
}

void Game::Stat() {
	printf("drawFPS = %d drawCall = %d drawVerts = %d snake.score = %d alive = %s\n"
		, drawFPS, drawCall, drawVerts, snake ? snake->score : 0, 
		(snake && snake->alive) ? "yes" : "no"
	);
}
