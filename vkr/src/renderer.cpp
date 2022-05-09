#include "renderer.hpp"
#include "vkr.hpp"

namespace vkr {
	Renderer3D::Renderer3D(App* app, VideoContext* video, Shader* shader) :
		app(app) {

		Pipeline::Attribute attribs[] = {
			{
				.location = 0,
				.offset = offsetof(Vertex, position),
				.type = Pipeline::Attribute::Type::float3
			}
		};

		Pipeline::UniformBuffer ubuffers[] = {
			{
				.binding = 0,
				.ptr     = &v_ub,
				.size    = sizeof(v_ub),
				.stage   = Pipeline::Stage::vertex
			}
		};

		Pipeline::PushConstantRange pc[] = {
			{
				.size = sizeof(v_pc),
				.start = 0,
				.stage = Pipeline::Stage::vertex
			}
		};

		pipeline = new Pipeline(video, shader, sizeof(Vertex),
			attribs, 1,
			ubuffers, 1,
			pc, 1);

		/* TODO: Make this better. */
		pipeline->make_default();
	}

	Renderer3D::~Renderer3D() {
		delete pipeline;
	}

	void Renderer3D::draw() {
		auto size = app->get_size();

		v_ub.projection = m4f::pers(70.0f, (f32)size.x / (f32)size.y, 0.1f, 100.0f);
		v_ub.view = m4f::translate(m4f::identity(), v3f(0.0f, 0.0f, -5.0f));

		pipeline->begin();
		for (auto& entry : drawlist) {
			auto model = entry.model;
			v_pc.transform = entry.transform;
			for (auto mesh : model->meshes) {
				pipeline->push_constant(Pipeline::Stage::vertex, &v_pc);
				mesh->vb->bind();
				mesh->ib->draw();
			}
		}
		pipeline->end();
	}

	Mesh3D* Mesh3D::from_wavefront(VideoContext* video, WavefrontModel* wmodel, WavefrontModel::Mesh* wmesh) {
		auto verts = new Renderer3D::Vertex[wmesh->vertices.size()];
		auto indices = new u16[wmesh->vertices.size()];

		usize vert_count = 0;
		usize index_count = 0;

		for (auto vertex : wmesh->vertices) {
			auto pos = wmodel->positions[vertex.position];

			bool is_new = true;

			for (usize i = 0; i < vert_count; i++) {
				if (pos == verts[i].position) {
					indices[index_count++] = i;
					is_new = false;
				}
			}

			if (is_new) {
				verts[vert_count].position = pos;
				indices[index_count++] = vert_count;
				vert_count++;
			}
		}

		Mesh3D* r = new Mesh3D();

		r->vb = new VertexBuffer(video, verts, vert_count * sizeof(Renderer3D::Vertex));
		r->ib = new IndexBuffer(video, indices, index_count);

		delete[] verts;
		delete[] indices;

		return r;
	}

	Mesh3D::~Mesh3D() {
		delete vb;
		delete ib;
	}

	Model3D* Model3D::from_wavefront(VideoContext* video, WavefrontModel* wmodel) {
		Model3D* model = new Model3D;

		if (wmodel->has_root_mesh) {
			model->meshes.push_back(Mesh3D::from_wavefront(video, wmodel, &wmodel->root_mesh));
		}

		for (auto& mesh : wmodel->meshes) {
			model->meshes.push_back(Mesh3D::from_wavefront(video, wmodel, &mesh));
		}

		return model;
	}

	Model3D::~Model3D() {
		for (auto mesh : meshes) {
			delete mesh;
		}
	}
}
