#include <stdio.h>

#include <random>

#include <vkr/vkr.hpp>
#include <ecs/ecs.hpp>

#define camera_speed 3.0f

using namespace vkr;

void update_camera(App* app, ecs::Entity cam_ent);

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

	ecs::Entity camera;

	ecs::Entity monkey1, monkey2;
	ecs::Entity ground, monolith;

	ecs::Entity red_light, blue_light;

	v2i old_mouse;
	bool first_move;
	bool camera_active;
public:
	SandboxApp() : App("Sandbox", vkr::v2i(1920, 1080)) {}

	void on_init() override {
		lock_mouse();
		first_move = true;
		camera_active = true;

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
		shaders.lighting = Shader::from_file(video,
			"res/shaders/lighting.vert.spv",
			"res/shaders/lighting.frag.spv");
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

		wall_a = Texture::from_file(video, "res/textures/walla.jpg", Texture::Flags::filter_linear);
		wall_n = Texture::from_file(video, "res/textures/walln.png", Texture::Flags::filter_linear);
		wood_a = Texture::from_file(video, "res/textures/wooda.jpg", Texture::Flags::filter_linear);

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

		camera = world.new_entity();
		camera.add(Camera {
			.position = { 0.0f, 0.0f, 0.0f },
			.rotation = { 0.0f, 180.0f, 0.0f },
			.active = true,
			.fov = 70.0f,
			.near = 0.1f,
			.far = 100.0f
		});

		blue_light = world.new_entity();
		blue_light.add(Transform{ m4f::translate(m4f::identity(), v3f(2.0f, -1.0f, 1.0f)) });
		blue_light.add(PointLight{
			.intensity = 10.0f,
			.specular = v3f(0.0f, 0.0f, 1.0f),
			.diffuse = v3f(0.0f, 0.0f, 1.0f),
			.range = 2.0f
			});

		red_light = world.new_entity();
		red_light.add(Transform { m4f::translate(m4f::identity(), v3f(-2.5f, 0.0f, 0.0f)) });
		red_light.add(PointLight {
			.intensity = 50.0f,
			.specular = v3f(1.0f, 0.0f, 0.0f),
			.diffuse = v3f(1.0f, 0.0f, 0.0f),
			.range = 1.0f
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

		std::random_device rdevice;
		std::mt19937 rng(rdevice());
		std::uniform_real_distribution<f32> color_dist(0.0f, 1.0f);
		std::uniform_real_distribution<f32> position_dist(-10.0f, 10.0f);
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

			if (camera_active) {
				ui->text("Press <Esc> to unlock the mouse.");
			} else {
				ui->text("Left click on the scene to use the fly camera.");
			}

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
			static f32 new_blocker_search_sample_count = 36;
			ui->slider(&new_blocker_search_sample_count, 0.0f, 128.0f);
			ui->text("%d", renderer->sun.blocker_search_sample_count);
			renderer->sun.blocker_search_sample_count = static_cast<i32>(new_blocker_search_sample_count);

			ui->label("PCF Samples");
			static f32 new_pcf_sample_count = 64;
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

		/* Update the camera. */
		if (camera_active) {
			Camera& cam = camera.get<Camera>();

			i32 change_x = mouse_pos.x - old_mouse.x;
			i32 change_y = old_mouse.y - mouse_pos.y;

			if (first_move) {
				old_mouse = mouse_pos;
				change_x = change_y = 0;
				first_move = false;
			}

			old_mouse = mouse_pos;

			cam.rotation.y -= (f32)change_x * 0.1f;
			cam.rotation.x += (f32)change_y * 0.1f;

			if (cam.rotation.x >= 89.0f) {
				cam.rotation.x = 89.0f;
			}

			if (cam.rotation.x <= -89.0f) {
				cam.rotation.x = -89.0f;
			}

			v3f cam_dir = v3f(
				cosf(to_rad(cam.rotation.x)) * sinf(to_rad(cam.rotation.y)),
				sinf(to_rad(cam.rotation.x)),
				cosf(to_rad(cam.rotation.x)) * cosf(to_rad(cam.rotation.y))
			);

			if (key_pressed(key_S)) {
				cam.position -= cam_dir * camera_speed * (f32)ts;
			}

			if (key_pressed(key_W)) {
				cam.position += cam_dir * camera_speed * (f32)ts;
			}

			if (key_pressed(key_D)) {
				cam.position += v3f::cross(cam_dir, v3f(0.0f, 1.0f, 0.0f)) * camera_speed * (f32)ts;
			}

			if (key_pressed(key_A)) {
				cam.position -= v3f::cross(cam_dir, v3f(0.0f, 1.0f, 0.0f)) * camera_speed * (f32)ts;
			}
		}

		if (key_just_pressed(key_escape)) {
			unlock_mouse();
			camera_active = false;
		}

		if (!camera_active && mouse_button_just_pressed(mouse_button_left) && !ui->any_windows_hovered()) {
			first_move = true;
			camera_active = true;
			lock_mouse();
		}

		auto& t = monkey2.get<Transform>();
		t.m = m4f::rotate(m4f::identity(), rot, v3f(0.0f, 1.0f, 0.0f));

		auto& lt = blue_light.get<Transform>();
		lt.m = m4f::translate(m4f::identity(), v3f((f32)cos(time * 2.0f), -1.0f, (f32)sin(time * 2.0f)));

		renderer->draw(&world, camera);

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
		delete shaders.lighting;
		delete renderer2d;
		delete sprite_shader;
		delete dejavusans;
		delete dejavusans_bold;
		test_sprite->free();
		test_sprite2->free();
	}
};

i32 main(i32 argc, const char** argv) {
	vkr::init_packer(argc, argv);

	SandboxApp* app = new SandboxApp();
	app->run();
	delete app;

	vkr::deinit_packer();
}
