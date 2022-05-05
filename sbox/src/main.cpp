#include <stdio.h>

#include <vkr/vkr.hpp>

#include <vulkan/vulkan.h>
class SandboxApp : public vkr::App {
private:
	vkr::Pipeline* pipeline;
public:
	SandboxApp() : App("Sandbox", vkr::v2i(800, 600)) {
	}

	void on_init() override {
		pipeline = new vkr::Pipeline(video);
	}

	void on_update(vkr::f64 ts) override {
		video->draw();
	}

	void on_deinit() override {
		delete pipeline;
	}
};

vkr::i32 main() {
	SandboxApp* app = new SandboxApp();
	app->run();
	delete app;
}
