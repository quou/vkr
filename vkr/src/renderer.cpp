#include "renderer.hpp"
#include "vkr.hpp"

namespace vkr {
	Renderer3D::Renderer3D(App* app, VideoContext* video, const ShaderConfig& shaders, Material* materials, usize material_count) :
		app(app), model(null) {

		Framebuffer::Attachment attachments[] = {
			{
				.type = Framebuffer::Attachment::Type::color,
				.format = Framebuffer::Attachment::Format::rgbaf16,
			},
			{
				.type = Framebuffer::Attachment::Type::depth,
				.format = Framebuffer::Attachment::Format::depth,
			},
		};

		scene_fb = new Framebuffer(video,
			Framebuffer::Flags::headless | Framebuffer::Flags::fit,
			app->get_size(), attachments, 2);

		Pipeline::Attribute attribs[] = {
			{
				.name     = "position",
				.location = 0,
				.offset   = offsetof(Vertex, position),
				.type     = Pipeline::Attribute::Type::float3
			},
			{
				.name     = "uv",
				.location = 1,
				.offset   = offsetof(Vertex, uv),
				.type     = Pipeline::Attribute::Type::float2
			},
			{
				.name     = "normal",
				.location = 2,
				.offset   = offsetof(Vertex, normal),
				.type     = Pipeline::Attribute::Type::float3
			},
			{
				.name     = "tangent",
				.location = 3,
				.offset   = offsetof(Vertex, tangent),
				.type     = Pipeline::Attribute::Type::float3
			},
			{
				.name     = "bitangent",
				.location = 4,
				.offset   = offsetof(Vertex, bitangent),
				.type     = Pipeline::Attribute::Type::float3
			},
		};

	/*	Pipeline::UniformBuffer ubuffers[] = {
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
		};*/

		Pipeline::PushConstantRange pc[] = {
			{
				.name = "transform",
				.size = sizeof(v_pc),
				.start = 0,
				.stage = Pipeline::Stage::vertex
			}
		};

		Pipeline::Descriptor uniform_descs[2];
		uniform_descs[0].name = "vertex_uniform_buffer";
		uniform_descs[0].binding = 0;
		uniform_descs[0].stage = Pipeline::Stage::vertex;
		uniform_descs[0].resource.type = Pipeline::ResourcePointer::Type::uniform_buffer;
		uniform_descs[0].resource.uniform.ptr  = &v_ub;
		uniform_descs[0].resource.uniform.size = sizeof(v_ub);

		uniform_descs[1].name = "vertex_uniform_buffer";
		uniform_descs[1].binding = 1;
		uniform_descs[1].stage = Pipeline::Stage::fragment;
		uniform_descs[1].resource.type = Pipeline::ResourcePointer::Type::uniform_buffer;
		uniform_descs[1].resource.uniform.ptr  = &f_ub;
		uniform_descs[1].resource.uniform.size = sizeof(f_ub);

		Pipeline::DescriptorSet desc_sets[] = {
			{
				.name = "uniforms",
				.descriptors = uniform_descs,
				.count = 2
			}
		};

		usize sampler_binding_count = material_count * Material::get_texture_count();
		auto samplers = new Pipeline::SamplerBinding[sampler_binding_count];

		usize mat_idx = 0;
		for (usize i = 0; i < sampler_binding_count; i += Material::get_texture_count(), mat_idx++) {
			auto albedo_binding = samplers + i;
			auto normal_binding = samplers + i + 1;

			albedo_binding->name = "albedo";
			albedo_binding->binding = 0;
			albedo_binding->type = Pipeline::SamplerBinding::Type::texture;
			albedo_binding->object = materials[mat_idx].albedo;
			albedo_binding->stage = Pipeline::Stage::fragment;

			normal_binding->name = "normal";
			normal_binding->binding = 1;
			normal_binding->type = Pipeline::SamplerBinding::Type::texture;
			normal_binding->object = materials[mat_idx].normal;
			normal_binding->stage = Pipeline::Stage::fragment;
		};

		scene_pip = new Pipeline(video,
			Pipeline::Flags::depth_test |
			Pipeline::Flags::cull_back_face,
			shaders.lit,
			sizeof(Vertex),
			attribs, 5,
			app->get_default_framebuffer(),
			desc_sets, 1,
			pc, 1);

#if 0
		Pipeline::Attribute post_attribs[] = {
			{ /* vec2 position. */
				.location = 0,
				.offset   = 0,
				.type     = Pipeline::Attribute::Type::float2
			},
			{ /* vec2 UV. */
				.location = 1,
				.offset   = sizeof(v2f),
				.type     = Pipeline::Attribute::Type::float2
			},
		};

		v2f tri_verts[] = {
			/* Position          UV */
			{ -1.0, -1.0 },      { 0.0f, 0.0f },
			{ -1.0,  3.0 },      { 0.0f, 2.0f },
			{  3.0, -1.0 },      { 2.0f, 0.0f }

		};

		fullscreen_tri = new VertexBuffer(video, tri_verts, sizeof(tri_verts));
#endif
	/*	Pipeline::UniformBuffer tonemap_ubuffers[] = {
			{
				.name    = "fragment_uniform_buffer",
				.binding = 0,
				.ptr     = &f_tonemap_ub,
				.size    = sizeof(f_tonemap_ub),
				.stage   = Pipeline::Stage::fragment
			},
		};

		Pipeline::SamplerBinding tonemap_samplers[] = {
			{
				.name = "input",
				.binding = 0,
				.stage = Pipeline::Stage::fragment,
				.type = Pipeline::SamplerBinding::Type::framebuffer_output,
				.object = scene_fb,
				.attachment = 0
			}
		};

		tonemap_pip = new Pipeline(video,
			Pipeline::Flags::depth_test |
			Pipeline::Flags::cull_back_face,
			shaders.tonemap,
			sizeof(v2f) * 2,
			post_attribs, 2,
			app->get_default_framebuffer(),
			tonemap_ubuffers, 1,
			tonemap_samplers, 1);

		delete[] samplers;*/
	}

	Renderer3D::~Renderer3D() {
		//delete fullscreen_tri;
		delete scene_fb;
	//	delete tonemap_pip;
		delete scene_pip;
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

		scene_pip->begin();
	}

	void Renderer3D::end() {
		scene_pip->end();

	/*	tonemap_pip->begin();

		u32 samplers[] = { 0 };

		tonemap_pip->bind_samplers(samplers, 1);

		fullscreen_tri->bind();
		fullscreen_tri->draw(3);

		tonemap_pip->end();*/
	}

	void Renderer3D::draw(Model3D* model, m4f transform, usize material_id) {
		this->model = model;

		v_pc.transform = transform;
		for (auto mesh : model->meshes) {
			scene_pip->push_constant(Pipeline::Stage::vertex, v_pc);
			scene_pip->bind_descriptor_set(0);
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

		for (u32 i = 0; i < index_count; i += 3) {
			v3f pos1 = verts[indices[i + 0]].position;
			v3f pos2 = verts[indices[i + 1]].position;
			v3f pos3 = verts[indices[i + 2]].position;

			v2f uv1 = verts[indices[i + 0]].uv;
			v2f uv2 = verts[indices[i + 1]].uv;
			v2f uv3 = verts[indices[i + 2]].uv;

			v2f delta_uv_1 = uv2 - uv1;
			v2f delta_uv_2 = uv3 - uv1;

			v3f edge_1 = pos2 - pos1;
			v3f edge_2 = pos3 - pos1;

			f32 f = 1.0f / (delta_uv_1.x * delta_uv_2.y - delta_uv_2.x * delta_uv_1.y);

			v3f tangent = {
				.x = f * (delta_uv_2.y * edge_1.x - delta_uv_1.y * edge_2.x),
				.y = f * (delta_uv_2.y * edge_1.y - delta_uv_1.y * edge_2.y),
				.z = f * (delta_uv_2.y * edge_1.z - delta_uv_1.y * edge_2.z)
			};

			v3f bitangent = {
				.x = f * (-delta_uv_2.x * edge_1.x + delta_uv_1.x * edge_2.x),
				.y = f * (-delta_uv_2.x * edge_1.y + delta_uv_1.x * edge_2.y),
				.z = f * (-delta_uv_2.x * edge_1.z + delta_uv_1.x * edge_2.z)
			};

			verts[indices[i + 0]].tangent = tangent;
			verts[indices[i + 1]].tangent = tangent;
			verts[indices[i + 2]].tangent = tangent;

			verts[indices[i + 0]].bitangent = bitangent;
			verts[indices[i + 1]].bitangent = bitangent;
			verts[indices[i + 2]].bitangent = bitangent;
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
