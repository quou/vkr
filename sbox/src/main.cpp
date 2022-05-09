#include <stdio.h>

#include <vkr/vkr.hpp>

using namespace vkr;

struct MatrixBuffer {
	m4f transform;
};

struct ColorBuffer {
	v3f color;
};

struct Vertex {
	v2f position;
};

class SandboxApp : public vkr::App {
private:
	Renderer3D* renderer;
	Model3D* monkey;

	Shader* shader;
public:
	SandboxApp() : App("Sandbox", vkr::v2i(800, 600)) {}

	void on_init() override {
		shader = Shader::from_file(video,
			"res/shaders/simple.vert.spv",
			"res/shaders/simple.frag.spv");

		auto monkey_obj = WavefrontModel::from_file("res/models/monkey.obj");

		monkey = Model3D::from_wavefront(video, monkey_obj);

		renderer = new Renderer3D(this, video, shader);
		renderer->drawlist.push_back(Renderer3D::DrawlistEntry { monkey, m4f::identity() });

		delete monkey_obj;
	}

	void on_update(f64 ts) override {
		renderer->draw();
	}

	void on_deinit() override {
		delete renderer;
		delete monkey;
		delete shader;
	}
};

i32 main() {
	SandboxApp* app = new SandboxApp();
	app->run();
	delete app;
}
