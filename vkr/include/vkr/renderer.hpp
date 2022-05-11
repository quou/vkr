#pragma once

#include <vector>

#include "common.hpp"
#include "maths.hpp"
#include "wavefront.hpp"

namespace vkr {
	class Mesh3D;
	class Model3D;

	class VKR_API Renderer3D {
	private:
		struct {
			m4f view, projection;
		} v_ub;

		struct {
			v3f camera_pos;
		} f_ub;

		struct {
			m4f transform;
		} v_pc;

		Pipeline* pipeline;
		App* app;
	public:
		struct Material {
			Texture* albedo;

			static inline usize get_texture_count() { return 1; }
		};

		Renderer3D(App* app, VideoContext* video, Shader* shader, Material* materials, usize material_count);
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
