#version 450

#begin VERTEX

#include "pp_vertex.glsl"

#end VERTEX

#begin FRAGMENT

#include "pp_common.glsl"
#include "pp_config.glsl"
#include "material.glsl"

layout (set = 1, binding = 0) uniform sampler2D in_color;
layout (set = 1, binding = 1) uniform sampler2D normal;
layout (set = 1, binding = 2) uniform sampler2D position;

#define max_point_lights 256

struct PointLight {
	vec3 diffuse, specular;
	vec3 position;
	float intensity, range;
};

layout (binding = 1) uniform LightingData {
	int point_light_count;
	PointLight point_lights[max_point_lights];
} lights;

layout (push_constant) uniform PushData {
	Material material;
	float use_diffuse_map;
	float use_normal_map;
} push_data;

vec3 compute_point_light(vec3 n, vec3 world_pos, vec3 view_dir, PointLight light) {
	vec3 light_dir = normalize(light.position - world_pos);
	vec3 reflect_dir = reflect(-light_dir, n);

	float dist = length(light.position - world_pos);

	float attenuation = 1.0 / (pow((dist / light.range) * 5.0, 2.0) + 1);

	vec3 diffuse =
		attenuation *
		light.diffuse *
		light.intensity *
		push_data.material.diffuse *
		max(dot(light_dir, n), 0.0);

	vec3 specular =
		attenuation *
		light.specular *
		light.intensity *
		push_data.material.specular * 
		pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

	return diffuse + specular;
}

void main() {
	vec3 result = vec3(0.0);

	vec3 world_pos = texture(position, fs_in.uv).rgb;
	vec3 world_normal = texture(normal, fs_in.uv).rgb;

	vec3 view_dir = normalize(config.camera_pos - world_pos);

	for (int i = 0; i < lights.point_light_count; i++) {
		result += compute_point_light(world_normal, world_pos, view_dir, lights.point_lights[i]);
	}

	/* To make sure that the clear colour doesn't get lit. */
	vec4 multiplier = vec4(world_normal != vec3(0.0) ? 1.0 : 0.0);

	color = (multiplier * vec4(result, 1.0)) + texture(in_color, fs_in.uv);
}

#end FRAGMENT
