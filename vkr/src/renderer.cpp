#include "renderer.hpp"
#include "vkr.hpp"

namespace vkr {
	Renderer3D::Renderer3D(App* app, VideoContext* video, Shader* shader, Material* materials, usize material_count) :
		app(app) {

		Pipeline::Attribute attribs[] = {
			{ /* vec3 position. */
				.location = 0,
				.offset   = offsetof(Vertex, position),
				.type     = Pipeline::Attribute::Type::float3
			},
			{ /* vec2 UV. */
				.location = 1,
				.offset   = offsetof(Vertex, uv),
				.type     = Pipeline::Attribute::Type::float2
			},
			{ /* vec3 normal. */
				.location = 2,
				.offset   = offsetof(Vertex, normal),
				.type     = Pipeline::Attribute::Type::float3
			},
		};

		Pipeline::UniformBuffer ubuffers[] = {
			{
				.name    = "vertex_uniform_buffer",
				.binding = 0,
				.ptr     = &v_ub,
				.size    = sizeof(v_ub),
				.stage   = Pipeline::Stage::vertex
			},
			{
				.name    = "fragment_uniform_buffer",
				.binding = 1,
				.ptr     = &f_ub,
				.size    = sizeof(f_ub),
				.stage   = Pipeline::Stage::fragment
			},
		};

		Pipeline::PushConstantRange pc[] = {
			{
				.size = sizeof(v_pc),
				.start = 0,
				.stage = Pipeline::Stage::vertex
			}
		};

		usize sampler_binding_count = material_count * Material::get_texture_count();
		auto samplers = new Pipeline::SamplerBinding[sampler_binding_count];
		for (usize i = 0; i < material_count; i += Material::get_texture_count()) {
			Pipeline::SamplerBinding* albedo_binding = samplers + i;

			albedo_binding->name = "albedo";
			albedo_binding->binding = 0;
			albedo_binding->texture = materials[i].albedo;
			albedo_binding->stage = Pipeline::Stage::fragment;
		};

		pipeline = new Pipeline(video,
			Pipeline::Flags::depth_test | Pipeline::Flags::cull_back_face,
			shader,
			sizeof(Vertex),
			attribs, 3,
			ubuffers, 2,
			samplers, sampler_binding_count,
			pc, 1);

		delete[] samplers;

		/* TODO: Make this better. */
//		pipeline->make_default();
	}

	Renderer3D::~Renderer3D() {
		delete pipeline;
	}

	void Renderer3D::begin() {
		auto size = app->get_size();

		v3f camera_pos(0.0f, 0.0f, -5.0f);

		v_ub.projection = m4f::pers(70.0f, (f32)size.x / (f32)size.y, 0.1f, 100.0f);
		v_ub.view = m4f::translate(m4f::identity(), camera_pos);

		f_ub.camera_pos = camera_pos;

		f_ub.point_light_count = 0;
		for (usize i = 0; i < lights.size(); i++) {
			auto light = &lights[i];

			switch (light->type) {
				case Light::Type::point: {
					auto idx = f_ub.point_light_count++;
					f_ub.point_lights[idx].intensity = light->intensity;
					f_ub.point_lights[idx].diffuse   = light->diffuse;
					f_ub.point_lights[idx].specular  = light->specular;
					f_ub.point_lights[idx].position  = light->as.point.position;
					f_ub.point_lights[idx].range     = light->as.point.range;
				} break;
				default: break;
			}
		}

		pipeline->begin();
	}

	void Renderer3D::end() {
		pipeline->end();
	}

	void Renderer3D::draw(Model3D* model, m4f transform, usize material_id) {
		v_pc.transform = transform;
		for (auto mesh : model->meshes) {
			u32 samplers[] = {
				(u32)((material_id + 0) * Material::get_texture_count()) /* albedo */
			};

			pipeline->push_constant(Pipeline::Stage::vertex, v_pc);
			pipeline->bind_samplers(samplers, Material::get_texture_count());
			mesh->vb->bind();
			mesh->ib->draw();
		}
	}

	Mesh3D* Mesh3D::from_wavefront(VideoContext* video, WavefrontModel* wmodel, WavefrontModel::Mesh* wmesh) {
		auto verts = new Renderer3D::Vertex[wmesh->vertices.size()];
		auto indices = new u16[wmesh->vertices.size()];

		usize vert_count = 0;
		usize index_count = 0;

		for (auto vertex : wmesh->vertices) {
			auto pos = wmodel->positions[vertex.position];
			auto normal = wmodel->normals[vertex.normal];
			auto uv = wmodel->uvs[vertex.uv];

			bool is_new = true;

			for (usize i = 0; i < vert_count; i++) {
				if (
					pos    == verts[i].position &&
					normal == verts[i].normal &&
					uv     == verts[i].uv) {
					indices[index_count++] = i;
					is_new = false;
				}
			}

			if (is_new) {
				verts[vert_count].position = pos;
				verts[vert_count].normal = normal;
				verts[vert_count].uv = uv;
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
