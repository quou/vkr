#include <string.h>
#include <stdio.h>

#include "wavefront.hpp"
#include "vkr.hpp"

namespace vkr {
	static bool is_digit(char c) {
		return c >= '0' && c <= '9';
	}

	static char* copy_string(const char* str) {
		usize len = strlen(str);

		char* r = new char[len + 1];
		memcpy(r, str, len);
		r[len] = '\0';
		return r;
	}

	static const char* parse_float(const char* start, f32* out) {
		/* This is longer than a single call to `strtod` because it must
		 * find the length of the float so that the functions that come
		 * after it might know how much to advance by. We return a pointer
		 * to the character after the number. */

		const char* c = start;

		if (*c == '-') {
			c++;
		}

		while (*c && is_digit(*c)) {
			c++;
		}

		if (*c == '.') {
			c++;
		}

		while (*c && is_digit(*c)) {
			c++;
		}

		*out = (f32)strtod(start, null);

		return c + 1;
	}

	static v3f parse_v3(const char* start) {
		v3f r;

		start = parse_float(start, &r.x);
		start = parse_float(start, &r.y);
		start = parse_float(start, &r.z);

		return r;
	}

	static v2f parse_v2(const char* start) {
		v2f r;

		start = parse_float(start, &r.x);
		start = parse_float(start, &r.y);

		return r;
	}

	static WavefrontModel::Vertex parse_vertex(const char* start) {
		char* mod = copy_string(start);

		WavefrontModel::Vertex r;

		char* save = mod;
		char* token;

		usize i = 0;

		while ((token = strtok_r(save, "/", &save)) && i < 3) {
			usize* v = null;

			switch (i) {
				case 0:
					v = &r.position;
					break;
				case 1:
					v = &r.uv;
					break;
				case 2:
					v = &r.normal;
					break;
				default: break;
			}

			*v = strtol(token, null, 10);
			*v -= 1;

			i++;
		}

		delete[] mod;

		return r;
	}

	static void parse_face(WavefrontModel::Mesh* mesh, std::vector<WavefrontModel::Vertex>* verts, const char* start) {
		char* mod = copy_string(start);

		char* save = mod;
		char* token;

		std::vector<char*> tokens;
		
		/* Split by space. */
		while ((token = strtok_r(save, " ", &save))) {
			tokens.push_back(copy_string(token));
		}

		if (tokens.size() == 3) {
			/* The face is already a triangle. */
			verts->push_back(parse_vertex(tokens[0]));
			verts->push_back(parse_vertex(tokens[1]));
			verts->push_back(parse_vertex(tokens[2]));
		} else {
			/* The face is not a triangle; Use a trangle fan to
			 * triangulate it. This would probably not work if the
			 * face has a hole in it. Too bad. It seems to work fine
			 * for most meshes. */
			for (usize i = 0; i < tokens.size() - 1; i++) {
				verts->push_back(parse_vertex(tokens[0]));
				verts->push_back(parse_vertex(tokens[i]));
				verts->push_back(parse_vertex(tokens[i + 1]));
			}
		}

		for (usize i = 0; i < tokens.size(); i++) {
			delete[] tokens[i];
		}

		delete[] mod;
	}

	WavefrontModel* WavefrontModel::from_file(const char* filename) {
		FILE* file = fopen(filename, "r");
		if (!file) {
			error("Failed fopen `%s'.", filename);
			return null;
		}

		WavefrontModel* model = new WavefrontModel();

		char* line = new char[256];

		Mesh* current_mesh = &model->root_mesh;

		while (fgets(line, 256, file)) {
			u32 line_len = (u32)strlen(line);

			/* Strip the newline that fgets reads as well as the line. */
			if (line[line_len - 1] == '\n') {
				line[line_len - 1] = '\0';
			}

			switch (line[0]) {
				case 'o':
					/* Start a new object. */
					model->meshes.push_back(Mesh {});
					current_mesh = &model->meshes[model->meshes.size() - 1];
					break;
				case 'v':
					switch (line[1]) {
						case 't': /* Texture coordinate. */
							model->uvs.push_back(parse_v2(line + 3));
							break;
						case 'n': /* Vertex normal. */
							model->normals.push_back(v3f::normalised(parse_v3(line + 3)));
							break;
						case ' ':
						case '\t':
							/* Vertex position. */
							model->positions.push_back(parse_v3(line + 2));
							break;
						default: break;
					}
					break;
				case 'f': /* A face. */
					parse_face(current_mesh, &current_mesh->vertices, line + 2);
					break;
				default: break;
			}
		}
			
		model->has_root_mesh = model->root_mesh.vertices.size() > 0;

		delete[] line;

		return model;
	}

	WavefrontModel::~WavefrontModel() {

	}
}
