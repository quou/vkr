#pragma once

#include <vector>

#include <ecs/ecs.hpp>

#include "common.hpp"
#include "maths.hpp"
#include "wavefront.hpp"

namespace vkr {
	class Mesh3D;
	class Model3D;
	class Renderer3D;

	static usize constexpr max_point_lights = 32;

	class VKR_API PostProcessStep {
	private:
		Pipeline* pipeline;
		Framebuffer* framebuffer;

		Renderer3D* renderer;

		bool use_default_fb;
		usize dependency_count;
	public:
		struct Dependency {
			const char* name;
			Framebuffer* framebuffer;
			u32 attachment;
		};

		PostProcessStep(Renderer3D* renderer, Shader* shader, Dependency* dependencies, usize dependency_count, bool use_default_fb = false);
		~PostProcessStep();

		void execute();

		inline Framebuffer* get_framebuffer() { return framebuffer; }
	};

	class VKR_API Renderer3D {
	public:
		struct Material {
			Texture* diffuse;
			Texture* normal;

			static constexpr usize get_texture_count() { return 2; }
		};

		struct ShaderConfig {
			Shader* lit;
			Shader* tonemap;
			Shader* shadowmap;
		};
	private:
		struct impl_PointLight {
			alignas(4)  f32 intensity;
			alignas(4)  f32 range;
			alignas(16) v3f diffuse;
			alignas(16) v3f specular;
			alignas(16) v3f position;
		};

		struct impl_DirectionalLight {
			alignas(4)  f32 intensity;
			alignas(16) v3f diffuse;
			alignas(16) v3f specular;
			alignas(16) v3f direction;
		};

		struct {
			m4f view, projection, sun_matrix;
		} v_ub;

		struct {
			m4f view, projection;
		} shadow_v_ub;

		struct {
			alignas(16) v3f camera_pos;
			alignas(4) f32 near_plane;
			alignas(4) f32 far_plane;

			alignas(4) f32 fov;
			alignas(4) f32 aspect;

			impl_DirectionalLight sun;

			alignas(4) i32 point_light_count;
			impl_PointLight point_lights[max_point_lights];
		} f_ub;

		struct {
			v2f screen_size;
		} f_post_ub;

		struct {
			m4f transform;
		} v_pc;

		struct {
			f32 use_diffuse_map;
			f32 use_normal_map;
		} f_pc;

		VertexBuffer* fullscreen_tri;

		Pipeline* scene_pip;
		Pipeline* shadow_pip;
		App* app;

		PostProcessStep* tonemap;

		Texture* default_texture;

		Framebuffer* scene_fb;
		Framebuffer* shadow_fb;

		Sampler* shadow_sampler;
		Sampler* fb_sampler;

		Model3D* model;

		Material* materials;

		friend class PostProcessStep;
	public:
		struct {
			v3f direction;
			f32 intensity;
			v3f specular;
			v3f diffuse;
		} sun;

		Renderer3D(App* app, VideoContext* video, const ShaderConfig& shaders, Material* materials, usize material_count);
		~Renderer3D();

		void draw(ecs::World* world);
		void draw_to_default_framebuffer();

		struct Vertex {
			v3f position;
			v2f uv;
			v3f normal;
			v3f tangent;
			v3f bitangent;
		};
	};

	class VKR_API Mesh3D {
	private:
		VertexBuffer* vb;
		IndexBuffer* ib;

		friend class Renderer3D;
	public:
		static Mesh3D* from_wavefront(Model3D* model, VideoContext* video, WavefrontModel* wmodel, WavefrontModel::Mesh* wmesh);
		~Mesh3D();
	};

	class VKR_API Model3D {
	private:
		std::vector<Mesh3D*> meshes;

		AABB aabb;

		friend class Renderer3D;
		friend class Mesh3D;
	public:
		static Model3D* from_wavefront(VideoContext* video, WavefrontModel* wmodel);
		~Model3D();

		inline const AABB& get_aabb() const { return aabb; }
	};

	struct Transform {
		m4f m;
	};

	struct Renderable3D {
		Model3D* model;
		usize material_id;
	};

	struct PointLight {
		f32 intensity;
		v3f specular;
		v3f diffuse;
		f32 range;
	};

	struct Rect {
		i32 x, y, w, h;
	};

	/* An RGBA CPU texture. */
	struct Bitmap {
		void* data;
		v2i size;

		static Bitmap* from_file(const char* path);
		void free();
	};

	class Renderer2D {
	public:
	private:
		struct Vertex {
			v2f position;
			v4f color;
			v2f uv;
			f32 use_texture;
		};

		struct {
			m4f projection;
		} v_ub;

		VideoContext* video;
		Framebuffer* framebuffer;

		Pipeline* pipeline;
		VertexBuffer* vb;

		static constexpr usize max_quads = 500;
		static constexpr usize verts_per_quad = 6;
		usize quad_count;

		Texture* atlas;
		std::unordered_map<Bitmap*, Rect> sub_atlases;
	public:
		Renderer2D(VideoContext* video, Shader* shader, Bitmap** images, usize image_count, Framebuffer* framebuffer);
		~Renderer2D();

		/* If the quad count exceeds max_quads, begin/end will be
		 * called to draw the current vertices to the screen and
		 * the quad count reset, so don't rely on the explicit
		 * calls to begin/end to be the only calls to such methods. */
		void begin(v2i screen_size);
		void end();

		struct Quad {
			v2f position;
			v2f dimentions;
			v4f color;

			/* Image must be part of the images array passed
			 * into the constructor, so that the atlasing system
			 * can take care of it. */
			Bitmap* image;
		};

		void push(const Quad& quad);
	};
}
