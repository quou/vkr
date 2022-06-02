#include <stdio.h>

#include <vkr/vkr.hpp>
#include <ecs/ecs.hpp>

using namespace vkr;

class SandboxApp : public vkr::App {
private:
	Renderer3D* renderer;
	Model3D* monkey;
	Model3D* cube;

	Renderer2D* renderer2d;
	Shader* sprite_shader;

	f32 rot = 0.0f;
	f64 time = 0.0;

	f64 fps = 0.0;
	f64 fps_update = 0.0;

	Renderer3D::ShaderConfig shaders;

	Bitmap* test_sprite;
	Bitmap* test_sprite2;

	Texture* wall_a;
	Texture* wall_n;
	Texture* wood_a;

	Font* dejavusans, * dejavusans_bold;

	UIContext* ui;

	ecs::World world;

	ecs::Entity monkey1, monkey2;
	ecs::Entity ground, monolith;

	ecs::Entity red_light, blue_light;
public:
	SandboxApp() : App("Sandbox", vkr::v2i(1920, 1080)) {}

	void on_init() override {
		ui = new UIContext(this);

		shaders.lit = Shader::from_file(video,
			"res/shaders/lit.vert.spv",
			"res/shaders/lit.frag.spv");
		shaders.tonemap = Shader::from_file(video,
			"res/shaders/tonemap.vert.spv",
			"res/shaders/tonemap.frag.spv");
		shaders.bright_extract = Shader::from_file(video,
			"res/shaders/bright_extract.vert.spv",
			"res/shaders/bright_extract.frag.spv");
		shaders.blur_v = Shader::from_file(video,
			"res/shaders/blur_v.vert.spv",
			"res/shaders/blur_v.frag.spv");
		shaders.blur_h = Shader::from_file(video,
			"res/shaders/blur_h.vert.spv",
			"res/shaders/blur_h.frag.spv");
		shaders.composite = Shader::from_file(video,
			"res/shaders/composite.vert.spv",
			"res/shaders/composite.frag.spv");
		shaders.shadowmap = Shader::from_file(video,
			"res/shaders/shadowmap.vert.spv",
			"res/shaders/shadowmap.frag.spv");
		sprite_shader = Shader::from_file(video,
			"res/shaders/2d.vert.spv",
			"res/shaders/2d.frag.spv");

		dejavusans      = new Font("res/fonts/DejaVuSans.ttf",      14.0f);
		dejavusans_bold = new Font("res/fonts/DejaVuSans-Bold.ttf", 14.0f);

		test_sprite = Bitmap::from_file("res/sprites/test.png");
		test_sprite2 = Bitmap::from_file("res/sprites/test2.png");

		Bitmap* sprites[] = {
			test_sprite, test_sprite2
		};

		renderer2d = new Renderer2D(video, sprite_shader, sprites, 2, get_default_framebuffer());

		auto monkey_obj = WavefrontModel::from_file("res/models/monkey.obj");
		monkey = Model3D::from_wavefront(video, monkey_obj);
		delete monkey_obj;

		auto cube_obj = WavefrontModel::from_file("res/models/cube.obj");
		cube = Model3D::from_wavefront(video, cube_obj);
		delete cube_obj;

		wall_a = Texture::from_file(video, "res/textures/walla.jpg");
		wall_n = Texture::from_file(video, "res/textures/walln.png");
		wood_a = Texture::from_file(video, "res/textures/wooda.jpg");

		Renderer3D::Material materials[] = {
			{
				.diffuse_map = wall_a,
				.normal_map = wall_n,
				.emissive = 0.0f,
				.diffuse = v3f(1.0f),
				.specular = v3f(1.0f),
				.ambient = v3f(1.0f)
			},
			{
				.diffuse_map = wood_a,
				.normal_map = null,
				.emissive = 0.0f,
				.diffuse = v3f(1.0f),
				.specular = v3f(1.0f),
				.ambient = v3f(1.0f)
			},
			{
				.diffuse_map = null,
				.normal_map = null,
				.emissive = 0.0f,
				.diffuse = v3f(1.0f),
				.specular = v3f(1.0f),
				.ambient = v3f(1.0f)
			},
			{
				.diffuse_map = null,
				.normal_map = null,
				.emissive = 5.0f,
				.diffuse = v3f(1.0f, 0.3f, 0.3f),
				.specular = v3f(1.0f, 0.3f, 0.3f),
				.ambient = v3f(1.0f, 0.3f, 0.3f)
			}
		};

		renderer = new Renderer3D(this, video, shaders, materials, 4);

		red_light = world.new_entity();
		red_light.add(Transform { m4f::translate(m4f::identity(), v3f(-2.5f, 0.0f, 0.0f)) });
		red_light.add(PointLight {
			.intensity = 50.0f,
			.specular = v3f(1.0f, 0.0f, 0.0f),
			.diffuse = v3f(1.0f, 0.0f, 0.0f),
			.range = 1.0f
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
		monkey1.add(Renderable3D { monkey, 3 });

		monkey2 = world.new_entity();
		monkey2.add(Transform { m4f::identity() });
		monkey2.add(Renderable3D { monkey, 0 });

		ground = world.new_entity();
		ground.add(Transform {
			m4f::translate(m4f::identity(), v3f(0.0f, -2.0f, 0.0f)) *
			m4f::scale(m4f::identity(), v3f(10.0f, 0.1f, 10.0f))});
		ground.add(Renderable3D { cube, 2 });

		monolith = world.new_entity();
		monolith.add(Transform {
			m4f::translate(m4f::identity(), v3f(2.5f, -2.0f, 0.0f)) *
			m4f::scale(m4f::identity(), v3f(1.0f, 5.0f, 1.0f))});
		monolith.add(Renderable3D { cube, 1 });
	}

	void on_update(f64 ts) override {
		ui->begin(get_size());
		ui->use_font(dejavusans);

		if (ui->begin_window("Debug", v2f(10.0f))) {
			if (fps_update <= 0.0) {
				fps = 1.0 / ts;
				fps_update = 1.0;
			}

			fps_update -= ts;

			ui->use_font(dejavusans);
			ui->text("FPS: %g", fps);
			ui->linebreak();

			ui->columns(1, 1.0);
			ui->use_font(dejavusans_bold);
			ui->label("Sun");
			ui->use_font(dejavusans);

			ui->columns(4, 0.3, 0.23, 0.23, 0.23);
			ui->label("Direction");
			ui->slider(&renderer->sun.direction.x, -1.0f, 1.0f);
			ui->slider(&renderer->sun.direction.y, -1.0f, 1.0f);
			ui->slider(&renderer->sun.direction.z, -1.0f, 1.0f);

			ui->columns(3, 0.3, 0.5, 0.2);
			ui->label("Shadow Bias");
			ui->slider(&renderer->sun.bias, -0.1f, 0.1f);
			ui->text("%.2f", renderer->sun.bias);

			ui->label("Shadow Softness");
			ui->slider(&renderer->sun.softness, 0.0f, 1.0f);
			ui->text("%.2f", renderer->sun.softness);

			ui->label("Blocker Samples");
			static f32 new_blocker_search_sample_count = 20;
			ui->slider(&new_blocker_search_sample_count, 0.0f, 128.0f);
			ui->text("%d", renderer->sun.blocker_search_sample_count);
			renderer->sun.blocker_search_sample_count = static_cast<i32>(new_blocker_search_sample_count);

			ui->label("PCF Samples");
			static f32 new_pcf_sample_count = 32;
			ui->slider(&new_pcf_sample_count, 0.0f, 128.0f);
			ui->text("%d", renderer->sun.pcf_sample_count);
			renderer->sun.pcf_sample_count = static_cast<i32>(new_pcf_sample_count);

			ui->linebreak();

			ui->use_font(dejavusans_bold);
			ui->columns(1, 1.0);
			ui->label("Bloom");
			ui->use_font(dejavusans);

			ui->columns(3, 0.30, 0.5, 0.20);
			ui->label("Threshold");
			ui->slider(&renderer->pp_config.bloom_threshold, 0.0f, 10.0f);
			ui->text("%.2f", renderer->pp_config.bloom_threshold);

			ui->label("Blur Intensity");
			ui->slider(&renderer->pp_config.bloom_blur_intensity, 0.0f, 1000.0f);
			ui->text("%.2f", renderer->pp_config.bloom_blur_intensity);

			ui->label("Intensity");
			ui->slider(&renderer->pp_config.bloom_intensity , 0.0f, 1.0f);
			ui->text("%.2f", renderer->pp_config.bloom_intensity);

			ui->end_window();
		}

		if (ui->begin_window("Test Window", v2f(10.0f, 320.0f))) {
			ui->columns(2, 0.5f, 0.5f);
			ui->label("Label");
			ui->button("Button");
			ui->label("Label");
			ui->button("Button");
			ui->columns(3, 0.33f, 0.33f, 0.33f);
			ui->label("Label");
			ui->label("Label");
			ui->label("Label");
			ui->label("Label");
			ui->label("Label");
			ui->label("Label");
			ui->label("Label");
			ui->label("Label");
			ui->label("Label");
			ui->label("Label");
			ui->label("Label");
			ui->label("Label");

			ui->end_window();
		}

		ui->end();

		auto& t = monkey2.get<Transform>();
		t.m = m4f::rotate(m4f::identity(), rot, v3f(0.0f, 1.0f, 0.0f));

		auto& lt = blue_light.get<Transform>();
		lt.m = m4f::translate(m4f::identity(), v3f((f32)cos(time * 2.0f), -1.0f, (f32)sin(time * 2.0f)));

		renderer->draw(&world);

		get_default_framebuffer()->begin();

		renderer->draw_to_default_framebuffer();

		renderer2d->begin(get_size());
		ui->draw(renderer2d);
		renderer2d->end();

		get_default_framebuffer()->end();

		rot += 1.0f * (f32)ts;
		time += ts;
	}

	void on_deinit() override {
		delete ui;
		delete renderer;
		delete monkey;
		delete cube;
		delete wall_a;
		delete wall_n;
		delete wood_a;
		delete shaders.lit;
		delete shaders.tonemap;
		delete shaders.bright_extract;
		delete shaders.blur_v;
		delete shaders.blur_h;
		delete shaders.composite;
		delete shaders.shadowmap;
		delete renderer2d;
		delete sprite_shader;
		delete dejavusans;
		delete dejavusans_bold;
		test_sprite->free();
		test_sprite2->free();
	}
};

i32 main() {
	SandboxApp* app = new SandboxApp();
	app->run();
	delete app;
}
