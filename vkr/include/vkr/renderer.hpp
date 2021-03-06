#pragma once

#include <vector>
#include <unordered_map>

#include <ecs/ecs.hpp>

#include "common.hpp"
#include "maths.hpp"
#include "wavefront.hpp"

namespace vkr {
	class Mesh3D;
	class Model3D;
	class Renderer3D;

	static usize constexpr max_point_lights = 256;

	class VKR_API PostProcessStep {
	private:
		Pipeline* pipeline;
		Framebuffer* framebuffer;

		Renderer3D* renderer;

		bool use_default_fb;
		usize dependency_count;

		void* pc;
		usize pc_size;
	public:
		struct Dependency {
			const char* name;
			Framebuffer* framebuffer;
			u32 attachment;
		};

		PostProcessStep(Renderer3D* renderer, Shader* shader, Dependency* dependencies, usize dependency_count, bool use_default_fb = false,
			void* uniform_buffer = null, usize uniform_buffer_size = 0, void* pc = null, usize pc_size = 0);
		~PostProcessStep();

		void execute();

		inline Framebuffer* get_framebuffer() { return framebuffer; }
	};

	class VKR_API Renderer3D {
	public:
		struct Material {
			Texture* diffuse_map;
			Texture* normal_map;

			f32 emissive;
			v3f diffuse;
			v3f specular;
			v3f ambient;

			static constexpr usize get_texture_count() { return 2; }
		};

		struct ShaderConfig {
			Shader* lit;
			Shader* lighting;
			Shader* tonemap;
			Shader* bright_extract;
			Shader* blur_v;
			Shader* blur_h;
			Shader* composite;
			Shader* shadowmap;
		};
	private:
		struct impl_PointLight {
			alignas(16) v3f diffuse;
			alignas(16) v3f specular;
			alignas(16) v3f position;
			alignas(4)  f32 intensity;
			alignas(4)  f32 range;
			u8 padding[4];
		};

		struct impl_DirectionalLight {
			alignas(4)  f32 intensity;
			alignas(4)  f32 bias;
			alignas(4)  f32 softness;
			alignas(16) v3f diffuse;
			alignas(16) v3f specular;
			alignas(16) v3f direction;
		};

		struct impl_Material {
			alignas(16) v3f diffuse;
			alignas(16) v3f specular;
			alignas(16) v3f ambient;
			alignas(4)  f32 emissive;
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

			alignas(4) i32 blocker_search_sample_count;
			alignas(4) i32 pcf_sample_count;

			impl_DirectionalLight sun;
		} f_ub;

		struct {
			alignas(4) i32 point_light_count;
			alignas(16) impl_PointLight point_lights[max_point_lights];
		} light_ub;

		struct {
			alignas(4)  f32 bloom_threshold;
			alignas(4)  f32 bloom_blur_intensity;
			alignas(4)  f32 bloom_intensity;
			alignas(8)  v2f screen_size;
			alignas(16) v3f camera_pos;
		} f_post_ub;

		struct {
			m4f transform;
		} v_pc;

		struct {
			impl_Material material;
			alignas(4) f32 use_diffuse_map;
			alignas(4) f32 use_normal_map;
		} f_pc;

		VertexBuffer* fullscreen_tri;

		Pipeline* scene_pip;
		Pipeline* shadow_pip;
		App* app;

		PostProcessStep* lighting; /* Deferred lighting. */
		PostProcessStep* bright_extract;
		PostProcessStep* blur_v;
		PostProcessStep* blur_h;
		PostProcessStep* blur_v2;
		PostProcessStep* blur_h2;
		PostProcessStep* tonemap;
		PostProcessStep* composite;

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
			f32 bias;
			f32 softness;
			v3f specular;
			v3f diffuse;

			int blocker_search_sample_count;
			int pcf_sample_count;
		} sun;

		/* Post processing config. */
		struct {
			f32 bloom_threshold;
			f32 bloom_blur_intensity;
			f32 bloom_intensity;
		} pp_config;

		Renderer3D(App* app, VideoContext* video, const ShaderConfig& shaders, Material* materials, usize material_count);
		~Renderer3D();

		void draw(ecs::World* world, ecs::Entity camera_ent);
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

	class Model3D {
	private:
		std::vector<Mesh3D*> meshes;

		AABB aabb;

		friend class Renderer3D;
		friend class Mesh3D;
	public:
		static VKR_API Model3D* from_wavefront(VideoContext* video, WavefrontModel* wmodel);
		VKR_API ~Model3D();

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

	struct Camera {
		v3f position;
		v3f rotation;

		bool active;

		f32 fov;
		f32 near;
		f32 far;
	};

	/* An RGBA CPU texture. */
	struct VKR_API Bitmap {
		void* data;
		v2i size;

		static Bitmap* from_file(const char* path);
		static Bitmap* from_data(void* data, v2i size);
		void free();
	};

	struct impl_Font;

	class VKR_API Font {
	private:
		impl_Font* handle;

		void* get_glyph_set(u32 c);

		friend class Renderer2D;
	public:
		Font(const char* path, f32 size);
		~Font();

		v2f dimentions(const char* text);
		f32 height();
	};

	class Renderer2D {
	public:
		struct Pixel {
			u8 r, g, b, a;
		};
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
		usize quad_offset;

		Texture* atlas;
		std::unordered_map<Bitmap*, Rect> sub_atlases;

		Shader* shader;

		v2i screen_size;

		void create_atlas();
		void create_pipeline();

		bool want_recreate;
	public:
		VKR_API Renderer2D(VideoContext* video, Shader* shader, Bitmap** images, usize image_count, Framebuffer* framebuffer);
		VKR_API ~Renderer2D();

		VKR_API void begin(v2i screen_size);
		VKR_API void end();

		struct Quad {
			v2f position;
			v2f dimentions;
			v4f color;

			Rect rect;

			/* For best performance, image should be part of the images
			 * array passed into the constructor, so that the atlasing system
			 * can take care of it before the rendering begins. */
			Bitmap* image;
		};

		VKR_API void push(const Quad& quad);
		VKR_API void push(Font* font, const char* text, v2f position, v4f color = v4f(1.0f, 1.0f, 1.0f, 1.0f));

		VKR_API void set_clip(Rect clip);
	};
}
