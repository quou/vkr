#version 450

#begin VERTEX

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;

layout (binding = 0) uniform VertexBuffer {
	mat4 view;
	mat4 projection;
	mat4 sun_matrix;
} data;

layout (push_constant) uniform PushData {
	mat4 transform;
} push_data;

layout (location = 0) out VertexOut {
	mat3 tbn;
	vec3 world_pos;
	vec2 uv;
	vec4 sun_pos;
} vs_out;

void main() {
	vs_out.world_pos = vec3(push_data.transform * vec4(position, 1.0));
	vs_out.uv = uv;

	vec3 t = normalize(vec3(push_data.transform * vec4(tangent, 0.0)));
	vec3 n = normalize(vec3(push_data.transform * vec4(normal, 0.0)));
	vec3 b = normalize(vec3(push_data.transform * vec4(bitangent, 0.0)));

	vs_out.tbn = mat3(t, b, n);
	vs_out.sun_pos = data.sun_matrix * vec4(vs_out.world_pos, 1.0);

	gl_Position = data.projection * data.view * vec4(vs_out.world_pos, 1.0);
}

#end VERTEX

#begin FRAGMENT

/* Poisson sampling is used in the blocker search to uniformly
 * sample the depth buffer to determine blockers. */
#include "poisson_disk.glsl"

#define max_point_lights 32

struct PointLight {
	float intensity, range;
	vec3 diffuse, specular;
	vec3 position;
};

struct DirectionalLight {
	float intensity;
	float bias;
	float softness;
	vec3 diffuse;
	vec3 specular;
	vec3 direction;
};

layout (location = 0) out vec4 color;

layout (binding = 1) uniform FragmentData {
	vec3 camera_pos;

	float near_plane;
	float far_plane;

	float fov, aspect;

	int blocker_search_sample_count;
	int pcf_sample_count;

	DirectionalLight sun;

	int point_light_count;
	PointLight point_lights[max_point_lights];
} data;

layout (location = 0) in VertexOut {
	mat3 tbn;
	vec3 world_pos;
	vec2 uv;
	vec4 sun_pos;
} fs_in;

struct Material {
	vec3 diffuse;
	vec3 specular;
	vec3 ambient;
	float emissive;
};

layout (push_constant) uniform PushData {
	layout(offset = 64)
	Material material;
	float use_diffuse_map;
	float use_normal_map;
} push_data;

layout (set = 1, binding = 0) uniform sampler2D diffuse_map;
layout (set = 1, binding = 1) uniform sampler2D normal_map;

layout (set = 0, binding = 2) uniform sampler2D blockermap;
layout (set = 0, binding = 3) uniform sampler2DShadow shadowmap;

vec3 compute_point_light(vec3 normal, vec3 view_dir, PointLight light) {
	vec3 light_dir = normalize(light.position - fs_in.world_pos);
	vec3 reflect_dir = reflect(light_dir, normal);

	float dist = length(light.position - fs_in.world_pos);

	float attenuation = 1.0 / (pow((dist / light.range) * 5.0, 2.0) + 1);

	vec3 diffuse =
		attenuation *
		light.diffuse *
		light.intensity *
		push_data.material.diffuse *
		max(dot(light_dir, normal), 0.0);

	vec3 specular =
		attenuation *
		light.specular *
		light.intensity *
		push_data.material.specular * 
		pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

	return diffuse + specular;
}

/* Find the average depth of the light blockers. */
float blocker_dist(vec3 coords, float size, float bias) {
	int blocker_count = 0;
	float r = 0.0;
	float width = size * (coords.z - data.near_plane) / data.camera_pos.z;

	for (int i = 0; i < data.blocker_search_sample_count; i++) {
		float z = texture(blockermap, coords.xy + poisson_disk[i % 64] * width).r;
		if (z < coords.z + bias) {
			blocker_count++;
			r += z;
		}
	}

	if (blocker_count > 0) {
		return r / float(blocker_count);
	} else {
		return -1.0;
	}
}

float pcf(vec3 coords, float radius, float bias) {
	float r = 0.0;

	for (int i = 0; i < data.pcf_sample_count; i++) {
		vec2 rotation = vec2(random(coords.xyz, i), random(coords.xyz, i + data.pcf_sample_count));

		vec2 p_offset = vec2(
			rotation.x * poisson_disk[i].x - rotation.y * poisson_disk[i].y,
			rotation.y * poisson_disk[i].x + rotation.x * poisson_disk[i].y
		);

		float z = texture(shadowmap, vec3(coords.xy + p_offset * radius, coords.z + bias)).r;
		r += (z < coords.z) ? 1.0 : 0.0;
	}

	return r / float(data.pcf_sample_count);
}

vec3 compute_directional_light(vec3 normal, vec3 view_dir, DirectionalLight light) {
	vec3 light_dir = normalize(light.direction);
	vec3 reflect_dir = reflect(light_dir, normal);

	vec3 diffuse =
		light.diffuse *
		light.intensity *
		push_data.material.diffuse *
		max(dot(light_dir, normal), 0.0);

	vec3 specular =
		light.specular *
		light.intensity *
		push_data.material.specular * 
		pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

	float light_size = light.softness;

	/* Shadow calculation. */
	vec3 coords = fs_in.sun_pos.xyz / fs_in.sun_pos.w;
	coords.xy = coords.xy * 0.5 + 0.5;

	float blocker = blocker_dist(coords, light_size, data.sun.bias);
	if (blocker == -1.0) {
		return diffuse + specular;
	}

	/* This formula to estimate the penumbra size from the
	 * average of blockers comes from:
	 * https://developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf */
	float penumbra = ((coords.z - blocker) * light_size) / blocker;

	float texel_size = 1.0 / textureSize(shadowmap, 0).x;

	float pcf_radius = penumbra * light_size * data.near_plane / coords.z;
	float shadow = pcf(coords, pcf_radius, data.sun.bias);

	return (1.0 - shadow) * (diffuse + specular);
}

void main() {
	vec3 normal;

	if (push_data.use_normal_map > 0.0) {
		normal = texture(normal_map, fs_in.uv).rgb * 2.0 - 1.0;
		normal = normalize(fs_in.tbn * normal);
	} else {
		normal = normalize(fs_in.tbn[2]);
	}

	vec3 view_dir = normalize(data.camera_pos - fs_in.world_pos);

	vec3 lighting_result = vec3(0.1 * push_data.material.ambient + (push_data.material.ambient * push_data.material.emissive));

	for (int i = 0; i < data.point_light_count; i++) {
		lighting_result += compute_point_light(normal, view_dir, data.point_lights[i]);
	}

	lighting_result += compute_directional_light(normal, view_dir, data.sun);

	vec4 texture_color = vec4(1.0);
	if (push_data.use_diffuse_map > 0.0) {
		texture_color = texture(diffuse_map, fs_in.uv);
	}

	color = texture_color * vec4(lighting_result, 1.0);
}
#end FRAGMENT
