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
	VertexBuffer* vb;
	IndexBuffer* ib;

	Shader* shader;

	Pipeline* pip;

	MatrixBuffer matrices;
	ColorBuffer colors;
public:
	SandboxApp() : App("Sandbox", vkr::v2i(800, 600)) {}

	void on_init() override {
		shader = Shader::from_file(video, 
			"res/shaders/simple.vert.spv",
			"res/shaders/simple.frag.spv");

		matrices.transform = m4f::identity();

		Pipeline::Attribute attribs[] = {
			{
				.location = 0,
				.offset = offsetof(Vertex, position),
				.type = Pipeline::Attribute::Type::float2
			}
		};

		Pipeline::UniformBuffer ubuffers[] = {
			{
				.binding = 0,
				.ptr     = &matrices,
				.size    = sizeof(MatrixBuffer),
				.stage   = Pipeline::Stage::vertex
			},
			{
				.binding = 1,
				.ptr     = &colors,
				.size    = sizeof(ColorBuffer),
				.stage   = Pipeline::Stage::fragment
			}
		};

		Pipeline::PushConstantRange pcranges[] = {
			{
				.size = sizeof(m4f),
				.stage = Pipeline::Stage::vertex
			}
		};

		pip = new Pipeline(video, shader, sizeof(Vertex),
			attribs, 1, ubuffers, 2, pcranges, 1);
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

		vb = new VertexBuffer(video, verts, sizeof(verts));
		ib = new IndexBuffer(video, indices, sizeof(indices) / sizeof(*indices));
	}

	void on_update(f64 ts) override {
		colors.color = v3f(1.0f, 1.0, 0.0f);

		m4f transform;

		pip->begin();
			vb->bind();

			transform = m4f::translate(m4f::identity(), v3f(-0.5f, -0.5f, 0.0f));
			pip->push_constant(Pipeline::Stage::vertex, transform);
			ib->draw();

			transform = m4f::translate(m4f::identity(), v3f(0.5f, 0.5f, 0.0f));
			pip->push_constant(Pipeline::Stage::vertex, transform);
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
