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
	Model3D* cube;

	f32 rot = 0.0f;

	Renderer3D::ShaderConfig shaders;

	Texture* wall_a;
	Texture* wall_n;
	Texture* wood_a;
public:
	SandboxApp() : App("Sandbox", vkr::v2i(800, 600)) {}

	void on_init() override {
		shaders.lit = Shader::from_file(video,
			"res/shaders/lit.vert.spv",
			"res/shaders/lit.frag.spv");
		shaders.tonemap = Shader::from_file(video,
			"res/shaders/tonemap.vert.spv",
			"res/shaders/tonemap.frag.spv");

		auto monkey_obj = WavefrontModel::from_file("res/models/monkey.obj");
		monkey = Model3D::from_wavefront(video, monkey_obj);
		delete monkey_obj;

		auto cube_obj = WavefrontModel::from_file("res/models/floor.obj");
		cube = Model3D::from_wavefront(video, cube_obj);
		delete cube_obj;

		wall_a = Texture::from_file(video, "res/textures/walla.jpg");
		wall_n = Texture::from_file(video, "res/textures/walln.png");
		wood_a = Texture::from_file(video, "res/textures/wooda.jpg");

		Renderer3D::Material materials[] = {
			{
				.albedo = wall_a,
				.normal = wall_n
			},
			{
				.albedo = wood_a,
				.normal = wall_n
			},
			{
				.albedo = wall_n,
				.normal = wall_n
			}
		};

		renderer = new Renderer3D(this, video, shaders, materials, 3);
		renderer->lights.push_back(Renderer3D::Light {
			.type = Renderer3D::Light::Type::point,
			.intensity = 10.0f,
			.specular = v3f(1.0f, 1.0f, 1.0f),
			.diffuse = v3f(1.0f, 1.0f, 1.0f),
			.as = {
				.point = {
					.position = v3f(0.0f, 1.0f, 1.0f),
					.range = 2.0f
				}
			}
		});
		renderer->lights.push_back(Renderer3D::Light {
			.type = Renderer3D::Light::Type::point,
			.intensity = 10.0f,
			.specular = v3f(1.0f, 0.0f, 0.0f),
			.diffuse = v3f(1.0f, 0.0f, 0.0f),
			.as = {
				.point = {
					.position = v3f(-2.0f, 1.0f, 1.0f),
					.range = 2.0f
				}
			}
		});
		renderer->lights.push_back(Renderer3D::Light {
			.type = Renderer3D::Light::Type::point,
			.intensity = 10.0f,
			.specular = v3f(0.0f, 0.0f, 1.0f),
			.diffuse = v3f(0.0f, 0.0f, 1.0f),
			.as = {
				.point = {
					.position = v3f(2.0f, -1.0f, 1.0f),
					.range = 2.0f
				}
			}
		});
	}

	void on_update(f64 ts) override {
		renderer->begin();
			renderer->draw(monkey, m4f::translate(m4f::identity(), v3f(-2.5f, 0.0f, 0.0f)), 0);
			renderer->draw(monkey, m4f::rotate(m4f::identity(), rot, v3f(0.0f, 1.0f, 0.0f)), 1);
			renderer->draw(monkey, m4f::translate(m4f::identity(), v3f(2.5f, 0.0f, 0.0f)), 2);
			renderer->draw(cube, m4f::translate(m4f::identity(), v3f(0.0f, -2.0f, 0.0f)), 0);
		renderer->end();

		rot += 1.0f * (f32)ts;
	}

	void on_deinit() override {
		delete renderer;
		delete monkey;
		delete cube;
		delete wall_a;
		delete wall_n;
		delete wood_a;
		delete shaders.lit;
		delete shaders.tonemap;
	}
};

i32 main() {
	SandboxApp* app = new SandboxApp();
	app->run();
	delete app;
}
