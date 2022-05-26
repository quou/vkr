#include <stdio.h>

#include <vkr/vkr.hpp>
#include <ecs/ecs.hpp>

using namespace vkr;

class SandboxApp : public vkr::App {
private:
	Renderer3D* renderer;
	Model3D* monkey;
	Model3D* cube;

	f32 rot = 0.0f;
	f64 time = 0.0f;

	Renderer3D::ShaderConfig shaders;

	Texture* wall_a;
	Texture* wall_n;
	Texture* wood_a;

	ecs::World world;

	ecs::Entity monkey1, monkey2, monkey3;
	ecs::Entity ground;

	ecs::Entity red_light, blue_light;
public:
	SandboxApp() : App("Sandbox", vkr::v2i(800, 600)) {}

	void on_init() override {
		shaders.lit = Shader::from_file(video,
			"res/shaders/lit.vert.spv",
			"res/shaders/lit.frag.spv");
		shaders.tonemap = Shader::from_file(video,
			"res/shaders/tonemap.vert.spv",
			"res/shaders/tonemap.frag.spv");
		shaders.shadowmap = Shader::from_file(video,
			"res/shaders/shadowmap.vert.spv",
			"res/shaders/shadowmap.frag.spv");

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
				.diffuse = wall_a,
				.normal = wall_n
			},
			{
				.diffuse = wood_a,
			}
		};

		renderer = new Renderer3D(this, video, shaders, materials, 2);

		red_light = world.new_entity();
		red_light.add(Transform { m4f::translate(m4f::identity(), v3f(-2.0f, 1.0f, 1.0f)) });
		red_light.add(PointLight {
			.intensity = 10.0f,
			.specular = v3f(1.0f, 0.0f, 0.0f),
			.diffuse = v3f(1.0f, 0.0f, 0.0f),
			.range = 3.0f
		});

		blue_light = world.new_entity();
		blue_light.add(Transform{ m4f::translate(m4f::identity(), v3f(2.0f, -1.0f, 1.0f)) });
		blue_light.add(PointLight{
			.intensity = 10.0f,
			.specular = v3f(0.0f, 0.0f, 1.0f),
			.diffuse = v3f(0.0f, 0.0f, 1.0f),
			.range = 2.0f
			});
		
		renderer->sun.direction = v3f(0.3f, 1.0f, 0.8f);
		renderer->sun.intensity = 1.0f;
		renderer->sun.specular = v3f(1.0f, 1.0f, 1.0f);
		renderer->sun.diffuse = v3f(1.0f, 1.0f, 1.0f);

		monkey1 = world.new_entity();
		monkey1.add(Transform { m4f::translate(m4f::identity(), v3f(-2.5f, 0.0f, 0.0f)) });
		monkey1.add(Renderable3D { monkey, 0 });

		monkey2 = world.new_entity();
		monkey2.add(Transform { m4f::identity() });
		monkey2.add(Renderable3D { monkey, 1 });

		monkey3 = world.new_entity();
		monkey3.add(Transform { m4f::translate(m4f::identity(), v3f(2.5f, -1.5f, 0.0f)) });
		monkey3.add(Renderable3D { monkey, 1 });

		ground = world.new_entity();
		ground.add(Transform { m4f::translate(m4f::identity(), v3f(0.0f, -2.0f, 0.0f)) });
		ground.add(Renderable3D { cube, 0 });
	}

	void on_update(f64 ts) override {
		auto& t = monkey2.get<Transform>();
		t.m = m4f::rotate(m4f::identity(), rot, v3f(0.0f, 1.0f, 0.0f));

		auto& lt = blue_light.get<Transform>();
		lt.m = m4f::translate(m4f::identity(), v3f((f32)cos(time * 2.0f), -1.0f, (f32)sin(time * 2.0f)));

		renderer->draw(&world);

		rot += 1.0f * (f32)ts;
		time += ts;
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
		delete shaders.shadowmap;
	}
};

i32 main() {
	SandboxApp* app = new SandboxApp();
	app->run();
	delete app;
}
