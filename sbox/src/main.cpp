#include <stdio.h>

#include <vkr/vkr.hpp>

#include <vulkan/vulkan.h>
class SandboxApp : public vkr::App {
private:
	vkr::Pipeline* pipeline;

	vkr::VertexBuffer* vb;
	vkr::IndexBuffer* ib;
public:
	SandboxApp() : App("Sandbox", vkr::v2i(800, 600)) {
	}

	void on_init() override {
		pipeline = new vkr::Pipeline(video);

		vkr::Vertex verts[] = {
			{{-0.5f, -0.5f}},
			{{ 0.5f, -0.5f}},
			{{ 0.5f,  0.5f}},
			{{-0.5f,  0.5f}}
		};

		vkr::u16 indices[] = {
			0, 1, 2, 2, 3, 0
		};

		vb = new vkr::VertexBuffer(video, verts, sizeof(verts) / sizeof(*verts));
		ib = new vkr::IndexBuffer(video, indices, sizeof(indices) / sizeof(*indices));
	}

	void on_update(vkr::f64 ts) override {
		vb->bind();
		ib->draw();
	}

	void on_deinit() override {
		delete vb;
		delete ib;

		delete pipeline;
	}
};

vkr::i32 main() {
	SandboxApp* app = new SandboxApp();
	app->run();
	delete app;
}
