#include <stdio.h>

#include <vkr/vkr.hpp>

using namespace vkr;

class SandboxApp : public vkr::App {
private:
	VertexBuffer* vb;
	IndexBuffer* ib;

	Shader* shader;

	Pipeline* pip;
public:
	SandboxApp() : App("Sandbox", vkr::v2i(800, 600)) {}

	void on_init() override {
		shader = Shader::from_file(video, 
			"res/shaders/simple.vert.spv",
			"res/shaders/simple.frag.spv");

		Pipeline::Attribute attribs[] = {
			{
				0,
				offsetof(Vertex, position),
				Pipeline::Attribute::Type::float2
			}
		};

		pip = new Pipeline(video, shader, sizeof(Vertex), attribs, 1);
		pip->make_default();

		Vertex verts[] = {
			{{-0.5f, -0.5f}},
			{{ 0.5f, -0.5f}},
			{{ 0.5f,  0.5f}},
			{{-0.5f,  0.5f}}
		};

		u16 indices[] = {
			0, 1, 2, 2, 3, 0
		};

		vb = new VertexBuffer(video, verts, sizeof(verts) / sizeof(*verts));
		ib = new IndexBuffer(video, indices, sizeof(indices) / sizeof(*indices));
	}

	void on_update(f64 ts) override {
		pip->begin();
			vb->bind();
			ib->draw();
		pip->end();
	}

	void on_deinit() override {
		delete vb;
		delete ib;

		delete pip;
		delete shader;
	}
};

i32 main() {
	SandboxApp* app = new SandboxApp();
	app->run();
	delete app;
}
