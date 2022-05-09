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
	f32 rot = 0.0f;
public:
	SandboxApp() : App("Sandbox", vkr::v2i(800, 600)) {}

	void on_init() override {
		shader = Shader::from_file(video,
			"res/shaders/simple.vert.spv",
			"res/shaders/simple.frag.spv");

		auto monkey_obj = WavefrontModel::from_file("res/models/monkey.obj");

		monkey = Model3D::from_wavefront(video, monkey_obj);

		renderer = new Renderer3D(this, video, shader);

		delete monkey_obj;
	}

	void on_update(f64 ts) override {
		renderer->begin();
			renderer->draw(monkey, m4f::translate(m4f::identity(), v3f(-2.5f, 0.0f, 0.0f)));
			renderer->draw(monkey, m4f::rotate(m4f::identity(), rot, v3f(0.0f, 1.0f, 0.0f)));
		renderer->end();

		rot += 0.0001f;
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
