#pragma once

#include <vector>

#include "common.hpp"
#include "maths.hpp"
#include "wavefront.hpp"

namespace vkr {
	class Mesh3D;
	class Model3D;

	static usize constexpr max_point_lights = 32;

	class VKR_API Renderer3D {
	private:
		struct impl_PointLight {
			alignas(4)  float intensity;
			alignas(4)  float range;
			alignas(16) v3f diffuse;
			alignas(16) v3f specular;
			alignas(16) v3f position;
		};

		struct {
			m4f view, projection;
		} v_ub;

		struct {
			alignas(16) v3f camera_pos;

			i32 point_light_count;
			impl_PointLight point_lights[max_point_lights];
		} f_ub;

		struct {
			v2f screen_size;
		} f_tonemap_ub;

		struct {
			m4f transform;
		} v_pc;

		VertexBuffer* fullscreen_tri;

		Pipeline* scene_pip;
		Pipeline* tonemap_pip;
		App* app;

		Framebuffer* scene_fb;

		Model3D* model;
	public:
		struct Material {
			Texture* albedo;

			static inline usize get_texture_count() { return 1; }
		};

		struct Light {
			enum class Type {
				point
			} type;

			f32 intensity;
			v3f specular;
			v3f diffuse;

			union {
				struct {
					v3f position;
					f32 range;
				} point;
			} as;
		};

		struct ShaderConfig {
			Shader* lit;
			Shader* tonemap;
		};

		std::vector<Light> lights;

		Renderer3D(App* app, VideoContext* video, const ShaderConfig& shaders, Material* materials, usize material_count);
		~Renderer3D();

		void begin();
		void end();
		void draw(Model3D* model, m4f transform, usize material_id);

		struct Vertex {
			v3f position;
			v2f uv;
			v3f normal;
		};
	};

	class VKR_API Mesh3D {
	private:
		VertexBuffer* vb;
		IndexBuffer* ib;

		friend class Renderer3D;
	public:
		static Mesh3D* from_wavefront(VideoContext* video, WavefrontModel* wmodel, WavefrontModel::Mesh* wmesh);
		~Mesh3D();
	};

	class VKR_API Model3D {
	private:
		std::vector<Mesh3D*> meshes;

		friend class Renderer3D;
	public:
		static Model3D* from_wavefront(VideoContext* video, WavefrontModel* wmodel);
		~Model3D();
	};
}
