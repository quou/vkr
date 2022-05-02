#include <stdio.h>

#include <vkr/vkr.hpp>

class SandboxApp : public vkr::App {
private:
public:
	SandboxApp() : App("Sandbox", vkr::v2i(800, 600)) {
	}

	void on_init() override {

	}

	void on_update(vkr::f64 ts) override {

	}

	void on_deinit() override {

	}
};

vkr::i32 main() {
	SandboxApp* app = new SandboxApp();
	app->run();
	delete app;
}
