#include <string.h> /* memcpy */
#include <math.h>

#include <stb_image.h>
#include <stb_truetype.h>
#include <stb_rect_pack.h>

#include "renderer.hpp"
#include "vkr.hpp"

static const vkr::u32 default_texture_data[2][2] = {
	0xffff00ff, 0xff000000,
	0xff000000, 0xffff00ff
};

namespace vkr {
	PostProcessStep::PostProcessStep(
			Renderer3D* renderer, Shader* shader,
			Dependency* dependencies, usize dependency_count,
			bool use_default_fb,
			void* uniform_buffer,
			usize uniform_buffer_size,
			void* pc, usize pc_size) : framebuffer(null), use_default_fb(use_default_fb),
			dependency_count(dependency_count), renderer(renderer),
			pc(pc), pc_size(pc_size) {

		if (!use_default_fb) {
			Framebuffer::Attachment attachments[] = {
				{
					.type = Framebuffer::Attachment::Type::color,
					.format = Framebuffer::Attachment::Format::rgbaf16,
				}
			};

			framebuffer = new Framebuffer(renderer->app->video,
				Framebuffer::Flags::headless | Framebuffer::Flags::fit,
				renderer->app->get_size(), attachments, 1);
		} else {
			framebuffer = renderer->app->get_default_framebuffer();
		}


		Pipeline::Attribute post_attribs[] = {
			{
				.name = "position",
				.location = 0,
				.offset = 0,
				.type = Pipeline::Attribute::Type::float2
			},
			{
				.name = "uv",
				.location = 1,
				.offset = sizeof(v2f),
				.type = Pipeline::Attribute::Type::float2
			},
		};


		Pipeline::Descriptor uniform_descs[2];
		uniform_descs[0].name = "fragment_uniform_buffer";
		uniform_descs[0].binding = 0;
		uniform_descs[0].stage = Pipeline::Stage::fragment;
		uniform_descs[0].resource.type = Pipeline::ResourcePointer::Type::uniform_buffer;
		uniform_descs[0].resource.uniform.ptr = &renderer->f_post_ub;
		uniform_descs[0].resource.uniform.size = sizeof(renderer->f_post_ub);

		uniform_descs[1].name = "fragment_uniform_buffer";
		uniform_descs[1].binding = 1;
		uniform_descs[1].stage = Pipeline::Stage::fragment;
		uniform_descs[1].resource.type = Pipeline::ResourcePointer::Type::uniform_buffer;
		uniform_descs[1].resource.uniform.ptr = uniform_buffer;
		uniform_descs[1].resource.uniform.size = uniform_buffer_size;

		auto sampler_descs = new Pipeline::Descriptor[dependency_count]();
		for (usize i = 0; i < dependency_count; i++) {
			sampler_descs[i].name = "input";
			sampler_descs[i].binding = static_cast<u32>(i);
			sampler_descs[i].stage = Pipeline::Stage::fragment;
			sampler_descs[i].resource.type = Pipeline::ResourcePointer::Type::framebuffer_output;
			sampler_descs[i].resource.framebuffer.ptr = dependencies[i].framebuffer;
			sampler_descs[i].resource.framebuffer.sampler = renderer->fb_sampler;
			sampler_descs[i].resource.framebuffer.attachment = dependencies[i].attachment;
		}

		Pipeline::DescriptorSet desc_sets[] = {
			{
				.name = "uniforms",
				.descriptors = uniform_descs,
				.count = (uniform_buffer_size > 0) ? 2u : 1u,
			},
			{
				.name = "samplers",
				.descriptors = sampler_descs,
				.count = dependency_count
			}
		};

		Pipeline::PushConstantRange pc_range[] = {
			{
				.name = "push_data",
				.size = pc_size,
				.start = 0,
				.stage = Pipeline::Stage::fragment
			}
		};

		pipeline = new Pipeline(renderer->app->video,
			Pipeline::Flags::cull_back_face,
			shader,
			sizeof(v2f) * 2,
			post_attribs, 2,
			framebuffer,
			desc_sets, 2,
			pc_range, pc_size > 0 ? 1 : 0);

		delete[] sampler_descs;
	}

	PostProcessStep::~PostProcessStep() {
		delete pipeline;
		if (!use_default_fb) {
			delete framebuffer;
		}
	}

	void PostProcessStep::execute() {
		pipeline->begin();
		
		if (!use_default_fb) {
			framebuffer->begin();
		}

		pipeline->bind_descriptor_set(0, 0);
		pipeline->bind_descriptor_set(1, 1);

		if (pc_size > 0) {
			pipeline->push_constant(Pipeline::Stage::fragment, pc, pc_size, 0);
		}

		renderer->fullscreen_tri->bind();
		renderer->fullscreen_tri->draw(3);
		pipeline->end();

		if (!use_default_fb) {
			framebuffer->end();
		}
	}

	Renderer3D::Renderer3D(App* app, VideoContext* video, const ShaderConfig& shaders, Material* materials, usize material_count) :
		app(app), model(null) {

		pp_config.bloom_threshold = 2.0f;
		pp_config.bloom_blur_intensity = 350.0f;
		pp_config.bloom_intensity = 0.2f;
		sun.bias = 0.0f;
		sun.softness = 0.15f;
		sun.pcf_sample_count = 64;
		sun.blocker_search_sample_count = 36;

		shadow_sampler = new Sampler(video, Sampler::Flags::filter_linear | Sampler::Flags::shadow);
		fb_sampler     = new Sampler(video, Sampler::Flags::filter_none | Sampler::Flags::clamp);

		default_texture = new Texture(video, (const void*)default_texture_data, v2i(2, 2),
			Texture::Flags::dimentions_2 | Texture::Flags::filter_none | Texture::Flags::format_rgba8);

		Framebuffer::Attachment attachments[] = {
			{
				.type = Framebuffer::Attachment::Type::color,
				.format = Framebuffer::Attachment::Format::rgbaf16,
			},
			{
				.type = Framebuffer::Attachment::Type::color,
				.format = Framebuffer::Attachment::Format::rgbaf16,
			},
			{
				.type = Framebuffer::Attachment::Type::color,
				.format = Framebuffer::Attachment::Format::rgbaf16,
			},
			{
				.type = Framebuffer::Attachment::Type::depth,
				.format = Framebuffer::Attachment::Format::depth,
			},
		};

		Framebuffer::Attachment shadow_attachment = {
			.type = Framebuffer::Attachment::Type::depth,
			.format = Framebuffer::Attachment::Format::depth
		};

		scene_fb = new Framebuffer(video,
			Framebuffer::Flags::headless | Framebuffer::Flags::fit,
			app->get_size(), attachments, 4);

		shadow_fb = new Framebuffer(video,
			Framebuffer::Flags::headless,
			v2i(2048, 2048), &shadow_attachment, 1);

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

		Pipeline::PushConstantRange pc[] = {
			{
				.name = "transform",
				.size = sizeof(v_pc),
				.start = 0,
				.stage = Pipeline::Stage::vertex
			},
			{
				.name = "frag_data",
				.size = sizeof(f_pc),
				.start = sizeof(v_pc),
				.stage = Pipeline::Stage::fragment
			}
		};

		this->materials = new Material[material_count]();
		memcpy(this->materials, materials, material_count * sizeof(Material));

		Pipeline::Descriptor uniform_descs[4];
		uniform_descs[0].name = "vertex_uniform_buffer";
		uniform_descs[0].binding = 0;
		uniform_descs[0].stage = Pipeline::Stage::vertex;
		uniform_descs[0].resource.type = Pipeline::ResourcePointer::Type::uniform_buffer;
		uniform_descs[0].resource.uniform.ptr  = &v_ub;
		uniform_descs[0].resource.uniform.size = sizeof(v_ub);

		uniform_descs[1].name = "fragment_uniform_buffer";
		uniform_descs[1].binding = 1;
		uniform_descs[1].stage = Pipeline::Stage::fragment;
		uniform_descs[1].resource.type = Pipeline::ResourcePointer::Type::uniform_buffer;
		uniform_descs[1].resource.uniform.ptr  = &f_ub;
		uniform_descs[1].resource.uniform.size = sizeof(f_ub);

		uniform_descs[2].name = "blockermap";
		uniform_descs[2].binding = 2;
		uniform_descs[2].stage = Pipeline::Stage::fragment;
		uniform_descs[2].resource.type = Pipeline::ResourcePointer::Type::framebuffer_output;
		uniform_descs[2].resource.framebuffer.ptr = shadow_fb;
		uniform_descs[2].resource.framebuffer.sampler = fb_sampler;
		uniform_descs[2].resource.framebuffer.attachment = 0;

		uniform_descs[3].name = "shadowmap";
		uniform_descs[3].binding = 3;
		uniform_descs[3].stage = Pipeline::Stage::fragment;
		uniform_descs[3].resource.type = Pipeline::ResourcePointer::Type::framebuffer_output;
		uniform_descs[3].resource.framebuffer.ptr = shadow_fb;
		uniform_descs[3].resource.framebuffer.sampler = shadow_sampler;
		uniform_descs[3].resource.framebuffer.attachment = 0;

		auto desc_sets = new Pipeline::DescriptorSet[1 + material_count]();
		desc_sets[0].name = "uniforms";
		desc_sets[0].descriptors = uniform_descs;
		desc_sets[0].count = 4;

		for (usize i = 0; i < material_count; i++) {
			auto set = desc_sets + i + 1;

			auto tex_descs = new Pipeline::Descriptor[Material::get_texture_count()]();

			tex_descs[0].name = "diffuse";
			tex_descs[0].binding = 0;
			tex_descs[0].stage = Pipeline::Stage::fragment;
			tex_descs[0].resource.type = Pipeline::ResourcePointer::Type::texture;
			tex_descs[0].resource.texture.ptr = materials[i].diffuse_map != null ? materials[i].diffuse_map : default_texture;

			tex_descs[1].name = "normal";
			tex_descs[1].binding = 1;
			tex_descs[1].stage = Pipeline::Stage::fragment;
			tex_descs[1].resource.type = Pipeline::ResourcePointer::Type::texture;
			tex_descs[1].resource.texture.ptr = materials[i].normal_map != null ? materials[i].normal_map : default_texture;

			set->descriptors = tex_descs;
			set->count = Material::get_texture_count();
		}

		scene_pip = new Pipeline(video,
			Pipeline::Flags::depth_test |
			Pipeline::Flags::cull_back_face,
			shaders.lit,
			sizeof(Vertex),
			attribs, 5,
			scene_fb,
			desc_sets, material_count + 1,
			pc, 2);

		Pipeline::Descriptor shadow_uniform_descs[1];
		shadow_uniform_descs[0].name = "vertex_uniform_buffer";
		shadow_uniform_descs[0].binding = 0;
		shadow_uniform_descs[0].stage = Pipeline::Stage::vertex;
		shadow_uniform_descs[0].resource.type = Pipeline::ResourcePointer::Type::uniform_buffer;
		shadow_uniform_descs[0].resource.uniform.ptr  = &shadow_v_ub;
		shadow_uniform_descs[0].resource.uniform.size = sizeof(shadow_v_ub);

		Pipeline::DescriptorSet shadow_desc_set = {
			.name = "uniforms",
			.descriptors = shadow_uniform_descs,
			.count = 1
		};

		shadow_pip = new Pipeline(video,
			Pipeline::Flags::depth_test |
			Pipeline::Flags::cull_front_face |
			Pipeline::Flags::front_face_clockwise,
			shaders.shadowmap,
			sizeof(Vertex),
			attribs, 1,
			shadow_fb,
			&shadow_desc_set, 1,
			pc, 1);

		v2f tri_verts[] = {
			/* Position          UV */
			{ -1.0, -1.0 },      { 0.0f, 0.0f },
			{ -1.0,  3.0 },      { 0.0f, 2.0f },
			{  3.0, -1.0 },      { 2.0f, 0.0f }
		};

		fullscreen_tri = new VertexBuffer(video, tri_verts, sizeof(tri_verts));

		PostProcessStep::Dependency lighting_deps[] = {
			{
				.name = "color",
				.framebuffer = scene_fb,
				.attachment = 0
			},
			{
				.name = "normals",
				.framebuffer = scene_fb,
				.attachment = 1
			},
			{
				.name = "positions",
				.framebuffer = scene_fb,
				.attachment = 2
			}
		};

		lighting = new PostProcessStep(this, shaders.lighting, lighting_deps, 3, false, &light_ub, sizeof(light_ub), &f_pc, sizeof(f_pc));

		PostProcessStep::Dependency bright_extract_deps[] = {
			{
				.name = "color",
				.framebuffer = lighting->get_framebuffer(),
				.attachment = 0
			}
		};

		bright_extract = new PostProcessStep(this, shaders.bright_extract, bright_extract_deps, 1);

		PostProcessStep::Dependency blur_v_deps[] = {
			{
				.name = "color",
				.framebuffer = bright_extract->get_framebuffer(),
				.attachment = 0
			}
		};

		blur_v = new PostProcessStep(this, shaders.blur_v, blur_v_deps, 1);

		PostProcessStep::Dependency blur_h_deps[] = {
			{
				.name = "color",
				.framebuffer = blur_v->get_framebuffer(),
				.attachment = 0
			}
		};

		blur_h = new PostProcessStep(this, shaders.blur_h, blur_h_deps, 1);

		PostProcessStep::Dependency blur_v2_deps[] = {
			{
				.name = "color",
				.framebuffer = blur_h->get_framebuffer(),
				.attachment = 0
			}
		};

		blur_v2 = new PostProcessStep(this, shaders.blur_v, blur_v2_deps, 1);

		PostProcessStep::Dependency blur_h2_deps[] = {
			{
				.name = "color",
				.framebuffer = blur_v2->get_framebuffer(),
				.attachment = 0
			}
		};

		blur_h2 = new PostProcessStep(this, shaders.blur_h, blur_h2_deps, 1);

		PostProcessStep::Dependency tonemap_deps[] = {
			{
				.name = "color",
				.framebuffer = lighting->get_framebuffer(),
				.attachment = 0
			}
		};

		tonemap = new PostProcessStep(this, shaders.tonemap, tonemap_deps, 1);

		PostProcessStep::Dependency composite_deps[] = {
			{
				.name = "tonemapped_scene",
				.framebuffer = tonemap->get_framebuffer(),
				.attachment = 0
			},
			{
				.name = "bloom",
				.framebuffer = blur_h2->get_framebuffer(),
				.attachment = 0
			}
		};

		composite = new PostProcessStep(this, shaders.composite, composite_deps, 2, true);

		for (usize i = 0; i < material_count; i++) {
			delete[] desc_sets[i + 1].descriptors;
		}

		delete[] desc_sets;
	}

	Renderer3D::~Renderer3D() {
		delete shadow_sampler;
		delete fb_sampler;

		delete fullscreen_tri;
		delete scene_fb;
		delete shadow_pip;
		delete shadow_fb;
		delete tonemap;
		delete lighting;
		delete composite;
		delete bright_extract;
		delete blur_v;
		delete blur_h;
		delete blur_v2;
		delete blur_h2;
		delete scene_pip;

		delete[] materials;

		delete default_texture;
	}

	void Renderer3D::draw(ecs::World* world, ecs::Entity camera_ent) {
		auto size = app->get_size();

		AABB scene_aabb = {
			.min = { INFINITY, INFINITY, INFINITY },
			.max = { -INFINITY, -INFINITY, -INFINITY }
		};

		for (auto view = world->new_view<Transform, Renderable3D>(); view.valid(); view.next()) {
			auto& trans = view.get<Transform>();
			auto& renderable = view.get<Renderable3D>();

			const auto& model_aabb = m4f::transform(trans.m, renderable.model->get_aabb());

			scene_aabb.min.x = std::min(scene_aabb.min.x, model_aabb.min.x);
			scene_aabb.min.y = std::min(scene_aabb.min.y, model_aabb.min.y);
			scene_aabb.min.z = std::min(scene_aabb.min.z, model_aabb.min.z);
			scene_aabb.max.x = std::max(scene_aabb.max.x, model_aabb.max.x);
			scene_aabb.max.y = std::max(scene_aabb.max.y, model_aabb.max.y);
			scene_aabb.max.z = std::max(scene_aabb.max.z, model_aabb.max.z);
		}

		shadow_v_ub.view = m4f::lookat(
			sun.direction,
			v3f(0.0f, 0.0f, 0.0f),
			v3f(0.0f, 1.0f, 0.0f));

		scene_aabb = m4f::transform(shadow_v_ub.view, scene_aabb);

		float z_mul = 3.0f;
		if (scene_aabb.min.z < 0.0f) {
			scene_aabb.min.z *= z_mul;
		} else {
			scene_aabb.min.z /= z_mul;
		}

		if (scene_aabb.max.z < 0.0f) {
			scene_aabb.max.z /= z_mul;
		} else {
			scene_aabb.max.z *= z_mul;
		}

		shadow_v_ub.projection = m4f::orth(
			scene_aabb.min.x, scene_aabb.max.x,
			scene_aabb.min.y, scene_aabb.max.y,
			scene_aabb.min.z, scene_aabb.max.z);

		v_ub.sun_matrix = shadow_v_ub.projection * shadow_v_ub.view;

		shadow_fb->begin();
		shadow_pip->begin();

		for (auto view = world->new_view<Transform, Renderable3D>(); view.valid(); view.next()) {
			auto& trans = view.get<Transform>();
			auto& renderable = view.get<Renderable3D>();

			shadow_pip->bind_descriptor_set(0, 0);

			v_pc.transform = trans.m;

			v_pc.transform = trans.m;
			for (auto mesh : renderable.model->meshes) {
				scene_pip->push_constant(Pipeline::Stage::vertex, v_pc);
				mesh->vb->bind();
				mesh->ib->draw();
			}
		}

		shadow_pip->end();
		shadow_fb->end();

		const auto& camera = camera_ent.get<Camera>();

		v3f cam_dir = v3f(
			cosf(to_rad(camera.rotation.x)) * sinf(to_rad(camera.rotation.y)),
			sinf(to_rad(camera.rotation.x)),
			cosf(to_rad(camera.rotation.x)) * cosf(to_rad(camera.rotation.y))
		);

		v_ub.projection = m4f::pers(camera.fov, (f32)size.x / (f32)size.y, camera.near, camera.far);
		v_ub.view = m4f::lookat(camera.position, camera.position + cam_dir, v3f(0.0f, 1.0f, 0.0f));

		f_ub.camera_pos = camera.position;
		f_ub.near_plane = camera.near;
		f_ub.far_plane = camera.far;
		f_ub.aspect = (f32)size.x / (f32)size.y;
		f_ub.fov = to_rad(camera.fov);

		light_ub.point_light_count = 0;
		for (ecs::View view = world->new_view<Transform, PointLight>(); view.valid(); view.next()) {
			auto& trans = view.get<Transform>();
			auto& light = view.get<PointLight>();

			auto idx = light_ub.point_light_count++;
			light_ub.point_lights[idx].intensity = light.intensity;
			light_ub.point_lights[idx].diffuse = light.diffuse;
			light_ub.point_lights[idx].specular = light.specular;
			light_ub.point_lights[idx].position = trans.m.get_translation();
			light_ub.point_lights[idx].range = light.range;
		}

		f_ub.sun.direction = sun.direction;
		f_ub.sun.intensity = sun.intensity;
		f_ub.sun.bias = sun.bias;
		f_ub.sun.softness = sun.softness;
		f_ub.sun.diffuse = sun.diffuse;
		f_ub.sun.specular = sun.specular;

		f_ub.blocker_search_sample_count = sun.blocker_search_sample_count;
		f_ub.pcf_sample_count = sun.pcf_sample_count;

		scene_pip->begin();
		scene_fb->begin();

		for (auto view = world->new_view<Transform, Renderable3D>(); view.valid(); view.next()) {
			auto& trans = view.get<Transform>();
			auto& renderable = view.get<Renderable3D>();
			
			auto model = renderable.model;
			auto material_id = renderable.material_id;

			this->model = model;

			scene_pip->bind_descriptor_set(0, 0);
			scene_pip->bind_descriptor_set(1, 1 + material_id);

			auto& material = materials[material_id];

			f_pc.use_diffuse_map = material.diffuse_map == null ? 0.0f : 1.0f;
			f_pc.use_normal_map = material.normal_map == null ? 0.0f : 1.0f;

			f_pc.material.emissive = material.emissive;
			f_pc.material.diffuse = material.diffuse;
			f_pc.material.specular = material.specular;
			f_pc.material.ambient = material.ambient;

			v_pc.transform = trans.m;
			for (auto mesh : model->meshes) {
				scene_pip->push_constant(Pipeline::Stage::vertex, v_pc);
				scene_pip->push_constant(Pipeline::Stage::fragment, f_pc, sizeof(v_pc));
				mesh->vb->bind();
				mesh->ib->draw();
			}
		}

		scene_pip->end();
		scene_fb->end();

		f_post_ub.screen_size = v2f(static_cast<f32>(size.x), static_cast<f32>(size.y));
		f_post_ub.camera_pos = camera.position;
		f_post_ub.bloom_threshold = pp_config.bloom_threshold;
		f_post_ub.bloom_blur_intensity = pp_config.bloom_blur_intensity;
		f_post_ub.bloom_intensity = pp_config.bloom_intensity;

		lighting->execute();
		tonemap->execute();
		bright_extract->execute();
		blur_v->execute();
		blur_h->execute();
		blur_v2->execute();
		blur_h2->execute();
	}

	void Renderer3D::draw_to_default_framebuffer() {
		composite->execute();
	}

	Mesh3D* Mesh3D::from_wavefront(Model3D* model, VideoContext* video, WavefrontModel* wmodel, WavefrontModel::Mesh* wmesh) {
		auto verts = new Renderer3D::Vertex[wmesh->vertices.size()];
		auto indices = new u16[wmesh->vertices.size()];

		usize vert_count = 0;
		usize index_count = 0;

		for (auto vertex : wmesh->vertices) {
			auto pos = wmodel->positions[vertex.position];
			auto normal = wmodel->normals[vertex.normal];
			auto uv = wmodel->uvs[vertex.uv];

			model->aabb.min.x = std::min(pos.x, model->aabb.min.x);
			model->aabb.min.y = std::min(pos.y, model->aabb.min.y);
			model->aabb.min.z = std::min(pos.z, model->aabb.min.z);
			model->aabb.max.x = std::max(pos.x, model->aabb.max.x);
			model->aabb.max.y = std::max(pos.y, model->aabb.max.y);
			model->aabb.max.z = std::max(pos.z, model->aabb.max.z);

			bool is_new = true;

			for (usize i = 0; i < vert_count; i++) {
				if (
					pos    == verts[i].position &&
					normal == verts[i].normal &&
					uv     == verts[i].uv) {
					indices[index_count++] = static_cast<u16>(i);
					is_new = false;
				}
			}

			if (is_new) {
				verts[vert_count].position = pos;
				verts[vert_count].normal = normal;
				verts[vert_count].uv = uv;
				indices[index_count++] = static_cast<u16>(vert_count);
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
				f * (delta_uv_2.y * edge_1.x - delta_uv_1.y * edge_2.x),
				f * (delta_uv_2.y * edge_1.y - delta_uv_1.y * edge_2.y),
				f * (delta_uv_2.y * edge_1.z - delta_uv_1.y * edge_2.z)
			};

			v3f bitangent = {
				f * (-delta_uv_2.x * edge_1.x + delta_uv_1.x * edge_2.x),
				f * (-delta_uv_2.x * edge_1.y + delta_uv_1.x * edge_2.y),
				f * (-delta_uv_2.x * edge_1.z + delta_uv_1.x * edge_2.z)
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

		model->aabb = AABB {
			.min = { INFINITY, INFINITY, INFINITY },
			.max = { -INFINITY, -INFINITY, -INFINITY }
		};

		if (wmodel->has_root_mesh) {
			model->meshes.push_back(Mesh3D::from_wavefront(model, video, wmodel, &wmodel->root_mesh));
		}

		for (auto& mesh : wmodel->meshes) {
			model->meshes.push_back(Mesh3D::from_wavefront(model, video, wmodel, &mesh));
		}

		return model;
	}

	Model3D::~Model3D() {
		for (auto& mesh : meshes) {
			delete mesh;
		}
	}

	Bitmap* Bitmap::from_file(const char* path) {
		u8* raw_data;
		usize raw_size;

		if (!read_raw(path, &raw_data, &raw_size)) {
			return null;
		}

		i32 w, h, channels;
		void* data = stbi_load_from_memory(raw_data, static_cast<i32>(raw_size), &w, &h, &channels, 4);
		if (!data) {
			error("Failed to load `%s': %s.", path, stbi_failure_reason());
			return null;
		}

		auto bitmap = new Bitmap;
		bitmap->size = v2i(w, h);
		bitmap->data = data;

		delete[] raw_data;

		return bitmap;
	}

	Bitmap* Bitmap::from_data(void* pixels, v2i size) {
		auto bitmap = new Bitmap();

		bitmap->data = pixels;
		bitmap->size = size;

		return bitmap;
	}

	void Bitmap::free() {
		stbi_image_free(data);
		delete this;
	}

	#define max_glyphset 256

	struct GlyphSet;

	struct impl_Font {
		u8* data;
		stbtt_fontinfo info;
		GlyphSet* sets[max_glyphset];
		f32 size;
		f32 height;
	};

	struct GlyphSet {
		Bitmap* atlas;
		stbtt_bakedchar glyphs[256];

		static GlyphSet* load(impl_Font* font, i32 idx) {
			auto set = new GlyphSet();

			v2i size(128, 128);

			Renderer2D::Pixel* pixels;

			/* Continually retries until all of the glyphs
			 * fit inside the bitmap. */
		retry:
			pixels = new Renderer2D::Pixel[size.x * size.y];

			f32 s = stbtt_ScaleForMappingEmToPixels(&font->info, 1) /
				stbtt_ScaleForPixelHeight(&font->info, 1);
			i32 r = stbtt_BakeFontBitmap(font->data, 0, font->size * s,
				(u8*)pixels, size.x, size.y, idx * 256, 256, set->glyphs);

			if (r <= 0) {
				size.x *= 2;
				size.y *= 2;
				delete[] pixels;
				goto retry;
			}

			i32 ascent, descent, linegap, scaled_ascent;
			stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &linegap);
			f32 scale = stbtt_ScaleForMappingEmToPixels(&font->info, font->size);
			scaled_ascent = (i32)(ascent * scale + 0.5);
			for (usize i = 0; i < 256; i++) {
				set->glyphs[i].yoff += scaled_ascent;
				set->glyphs[i].xadvance = (f32)floor(set->glyphs[i].xadvance);
			}

			for (i32 i = size.x * size.y - 1; i >= 0; i--) {
				u8 n = *((u8*)pixels + i);
				pixels[i] = Renderer2D::Pixel { 255, 255, 255, n };
			}

			set->atlas = Bitmap::from_data(pixels, size);

			return set;
		}
	};

	/* This font renderer actually supports UTF-8! This function takes a
 	 * UTF-8 encoded string and gives the position in the glyph set of the
 	 * character. Note that the character is not guaranteed to exist in
 	 * the font, so null text may be rendered, however the font defines that.
 	 *
 	 * This implementation is based on section seven of the man page for utf-8 */
	static const char* utf8_to_codepoint(const char* p, u32* dst) {
		u32 res, n;
		switch (*p & 0xf0) {
			case 0xf0 : res = *p & 0x07; n = 3; break;
			case 0xe0 : res = *p & 0x0f; n = 2; break;
			case 0xd0 :
			case 0xc0 : res = *p & 0x1f; n = 1; break;
			default   : res = *p;        n = 0; break;
		}
		while (n--) {
			res = (res << 6) | (*(++p) & 0x3f);
		}
		*dst = res;
		return p + 1;
	}

	Font::Font(const char* path, f32 size) {
		handle = new impl_Font();

		usize filesize;
		if (!read_raw(path, &handle->data, &filesize)) {
			return;
		}

		i32 r = stbtt_InitFont(&handle->info, handle->data, 0);
		if (!r) {
			abort_with("Failed to init font.");
		}

		handle->size = size;

		i32 ascent, descent, linegap;
		stbtt_GetFontVMetrics(&handle->info, &ascent, &descent, &linegap);
		f32 scale = stbtt_ScaleForMappingEmToPixels(&handle->info, size);
		handle->height = (ascent - descent + linegap) * scale + 0.5f;

		stbtt_bakedchar* g = reinterpret_cast<GlyphSet*>(get_glyph_set('\n'))->glyphs;
		g['\t'].x1 = g['\t'].x0;
		g['\n'].x1 = g['\n'].x0;
	}

	void* Font::get_glyph_set(u32 c) {
		i32 idx = (c >> 8) % max_glyphset;
		if (!handle->sets[idx]) {
			handle->sets[idx] = GlyphSet::load(handle, idx);
		}
		return handle->sets[idx];
	}

	v2f Font::dimentions(const char* text) {
		f32 x = 0.0f;
		f32 width = 0.0f;
		f32 y = handle->height;

		const char* p = text;
		while (*p) {
			if (*p == '\n') {
				width = std::max(width, x);
				x = 0;
				y += handle->height;
				p++;
				continue;
			}

			u32 codepoint;
			p = utf8_to_codepoint(p, &codepoint);

			auto set = reinterpret_cast<GlyphSet*>(get_glyph_set(codepoint));
			auto g = &set->glyphs[codepoint & 0xff];

			f32 w = (f32)(g->x1 - g->x0);
			f32 h = (f32)(g->y1 - g->y0);

			x += g->xadvance;
		}

		width = std::max(width, x);

		return v2f(width, y);
	}

	f32 Font::height() {
		return handle->height;
	}

	Font::~Font() {
		for (usize i = 0; i < max_glyphset; i++) {
			auto set = handle->sets[i];
			if (set) {
				delete[] (Renderer2D::Pixel*)set->atlas->data;
				delete set->atlas;
				delete set;
			}
		}

		delete[] handle->data;
		delete handle;
	}

	Renderer2D::Renderer2D(VideoContext* video, Shader* shader, Bitmap** images, usize image_count, Framebuffer* framebuffer)
		: video(video), framebuffer(framebuffer), shader(shader), want_recreate(false) {

		for (usize i = 0; i < image_count; i++) {
			sub_atlases[images[i]] = Rect{};
		}

		/* Set up the pipline. */
		vb = new VertexBuffer(video, null, max_quads * verts_per_quad * sizeof(Vertex), true);

		create_atlas();

		create_pipeline();
	}

	Renderer2D::~Renderer2D() {
		delete atlas;
		delete pipeline;
		delete vb;
	}

	void Renderer2D::create_atlas() {	
		/* Dumb rectangle packing algorithm. It just lines up all of the bitmaps
		 * next to each other in a single texture. It's super wasteful and stupid but
		 * I don't know if I can be bothered making a proper algorithm for it. */
		v2i final_size(1, 1);
		for (const auto& pair : sub_atlases) {
		   auto image = pair.first;

		   final_size.x += image->size.x;
		   final_size.y = std::max(final_size.y, image->size.y);
		}

		Pixel* atlas_data = new Pixel[final_size.x * final_size.y];
		v2i dst_pos(1, 0);
		for (auto& pair : sub_atlases) {
			auto image = pair.first;

			Pixel* src = ((Pixel*)image->data);
			Pixel* dst = atlas_data + (dst_pos.x + dst_pos.y * final_size.x);

			i32 dx = final_size.x - image->size.x;

			for (i32 y = 0; y < image->size.y; y++) {
				for (i32 x = 0; x < image->size.x; x++) {
					*dst = *src;

					dst++;
					src++;
				}

				dst += dx;
			}

			sub_atlases[pair.first] = Rect { dst_pos.x, dst_pos.y, image->size.x, image->size.y };

			dst_pos.x += image->size.x;
		}

		atlas = new Texture(video, atlas_data, final_size,
			Texture::Flags::dimentions_2 | Texture::Flags::filter_none | Texture::Flags::format_rgba8);

		delete[] atlas_data;
	}

	void Renderer2D::create_pipeline() {
		Pipeline::Attribute attribs[] = {
			{
				.name = "position",
				.location = 0,
				.offset = offsetof(Vertex, position),
				.type = Pipeline::Attribute::Type::float2
			},
			{
				.name = "color",
				.location = 1,
				.offset = offsetof(Vertex, color),
				.type = Pipeline::Attribute::Type::float4
			},
			{
				.name = "uv",
				.location = 2,
				.offset = offsetof(Vertex, uv),
				.type = Pipeline::Attribute::Type::float2
			},
			{
				.name = "use_texture",
				.location = 3,
				.offset = offsetof(Vertex, use_texture),
				.type = Pipeline::Attribute::Type::float1
			}
		};

		Pipeline::Descriptor descs[2];
		descs[0].name = "data";
		descs[0].binding = 0;
		descs[0].stage = Pipeline::Stage::vertex;
		descs[0].resource.type = Pipeline::ResourcePointer::Type::uniform_buffer;
		descs[0].resource.uniform.ptr = &v_ub;
		descs[0].resource.uniform.size = sizeof(v_ub);

		descs[1].name = "atlas";
		descs[1].binding = 1;
		descs[1].stage = Pipeline::Stage::fragment;
		descs[1].resource.type = Pipeline::ResourcePointer::Type::texture;
		descs[1].resource.texture.ptr = atlas;

		Pipeline::DescriptorSet desc_set = {
			.name = "uniforms",
			.descriptors = descs,
			.count = 2
		};

		pipeline = new Pipeline(video,
			Pipeline::Flags::blend | Pipeline::Flags::dynamic_scissor,
			shader,
			sizeof(Vertex),
			attribs, 4,
			framebuffer,
			&desc_set, 1);
	}

	void Renderer2D::push(const Quad& quad) {
		if (quad_count >= max_quads) {
			warning("Too many quads.");
		}

		auto x = roundf(quad.position.x);
		auto y = roundf(quad.position.y);
		auto w = roundf(quad.dimentions.x);
		auto h = roundf(quad.dimentions.y);

		Rect rect;

		if (quad.image) {
			if (sub_atlases.count(quad.image) == 0) {
				sub_atlases[quad.image] = Rect{};
				want_recreate = true;
				return;
			}

			Rect bitmap_rect = sub_atlases[quad.image];

			rect.x = bitmap_rect.x + quad.rect.x;
			rect.y = bitmap_rect.y + quad.rect.y;
			rect.w = std::min(bitmap_rect.w, quad.rect.w);
			rect.h = std::min(bitmap_rect.h, quad.rect.h);
		} else {
			rect = quad.rect;
		}

		if (want_recreate) { return; }

		f32 tx = 0.0f, ty = 0.0f, tw = 0.0f, th = 0.0f;

		if (quad.image) {
			tx = (f32)rect.x / (f32)atlas->get_size().x;
			ty = (f32)rect.y / (f32)atlas->get_size().y;
			tw = (f32)rect.w / (f32)atlas->get_size().x;
			th = (f32)rect.h / (f32)atlas->get_size().y;
		}

		f32 use_texture = quad.image ? 1.0f : 0.0f;

		Vertex vertices[verts_per_quad] = {
			{ { x,     y },     quad.color, { tx,      ty      }, use_texture },
			{ { x + w, y + h }, quad.color, { tx + tw, ty + th }, use_texture },
			{ { x,     y + h }, quad.color, { tx,      ty + th }, use_texture },
			{ { x,     y },     quad.color, { tx,      ty      }, use_texture },
			{ { x + w, y },     quad.color, { tx + tw, ty      }, use_texture },
			{ { x + w, y + h }, quad.color, { tx + tw, ty + th }, use_texture },
		};

		vb->update(vertices, sizeof(vertices), (quad_offset + quad_count) * verts_per_quad * sizeof(Vertex));

		quad_count++;
	}

	void Renderer2D::push(Font* font, const char* text, v2f position, v4f color) {
		f32 x = position.x;
		f32 y = position.y;

		f32 ori_x = x;

		const char* p = text;
		while (*p) {
			if (*p == '\n') {
				x = ori_x;
				y += font->handle->height;
				p++;
				continue;
			}

			u32 codepoint;
			p = utf8_to_codepoint(p, &codepoint);

			auto set = reinterpret_cast<GlyphSet*>(font->get_glyph_set(codepoint));
			auto g = &set->glyphs[codepoint & 0xff];

			f32 w = (f32)(g->x1 - g->x0);
			f32 h = (f32)(g->y1 - g->y0);

			push(Quad {
				.position = { x + g->xoff, y + g->yoff },
				.dimentions = { w, h },
				.color = color,
				.rect = { g->x0, g->y0, (i32)w, (i32)h },
				.image = set->atlas
			});

			x += g->xadvance;
		}
	}

	void Renderer2D::set_clip(Rect clip) {
		if (want_recreate) { return; }

		if (quad_count > 0) {
			pipeline->bind_descriptor_set(0, 0);
			vb->bind();
			vb->draw(quad_count * verts_per_quad, quad_offset * verts_per_quad);

			quad_offset += quad_count;
			quad_count = 0;
		}

		pipeline->set_scissor(v4i(clip.x, clip.y, clip.w, clip.h));
	}

	void Renderer2D::begin(v2i screen_size) {
		this->screen_size = screen_size;

		if (want_recreate) {
			/* Update the texture atlas. */
			delete pipeline;
			delete atlas;

			create_atlas();
			create_pipeline();

			want_recreate = false;
		}

		quad_count = 0;
		quad_offset = 0;
		v_ub.projection = m4f::orth(0.0f, (f32)screen_size.x, 0.0f, (f32)screen_size.y, -1.0f, 1.0f);

		pipeline->begin();
		pipeline->set_scissor(v4i(0, 0, screen_size.x, screen_size.y));
	}

	void Renderer2D::end() {
		if (want_recreate) { return; }

		pipeline->bind_descriptor_set(0, 0);
		vb->bind();
		vb->draw(quad_count * verts_per_quad, quad_offset * verts_per_quad);
		pipeline->end();
	}
}
