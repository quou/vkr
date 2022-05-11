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

	Texture* wall_a;
	Texture* wood_a;
public:
	SandboxApp() : App("Sandbox", vkr::v2i(800, 600)) {}

	void on_init() override {
		shader = Shader::from_file(video,
			"res/shaders/lit.vert.spv",
			"res/shaders/lit.frag.spv");

		auto monkey_obj = WavefrontModel::from_file("res/models/monkey.obj");

		monkey = Model3D::from_wavefront(video, monkey_obj);

		wall_a = Texture::from_file(video, "res/textures/walla.jpg");
		wood_a = Texture::from_file(video, "res/textures/wooda.jpg");

		Renderer3D::Material materials[] = {
			{
				.albedo = wall_a
			},
			{
				.albedo = wood_a
			}
		};

		renderer = new Renderer3D(this, video, shader, materials, 2);

		delete monkey_obj;
	}

	void on_update(f64 ts) override {
		renderer->begin();
			renderer->draw(monkey, m4f::translate(m4f::identity(), v3f(-2.5f, 0.0f, 0.0f)), 0);
			renderer->draw(monkey, m4f::rotate(m4f::identity(), rot, v3f(0.0f, 1.0f, 0.0f)), 1);
		renderer->end();

		rot += 1.0f * (f32)ts;
	}

	void on_deinit() override {
		delete renderer;
		delete monkey;
		delete wall_a;
		delete wood_a;
		delete shader;
	}
};

i32 main() {
	SandboxApp* app = new SandboxApp();
	app->run();
	delete app;
}
