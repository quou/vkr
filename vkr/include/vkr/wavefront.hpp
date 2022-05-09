#pragma once

#include <vector>

#include "common.hpp"
#include "maths.hpp"

namespace vkr {
	struct WavefrontModel {
		struct Vertex {
			usize position, uv, normal;
		};

		struct Mesh {
			std::vector<Vertex> vertices;
		};

		Mesh root_mesh;
		bool has_root_mesh;
		std::vector<Mesh> meshes;

		std::vector<v3f> positions;
		std::vector<v3f> normals;
		std::vector<v2f> uvs;

		static WavefrontModel* from_file(const char* filename);
		~WavefrontModel();
	};
}
