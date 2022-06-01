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

/* TODO: Make this a texture that's generated at runtime instead.
 *
 * Poisson sampling is used in the blocker search to uniformly
 * sample the depth buffer to determine blockers. */
vec2 poisson_disk[64] = vec2[]( 
	vec2(-0.5119625f, -0.4827938f),
	vec2(-0.2171264f, -0.4768726f),
	vec2(-0.7552931f, -0.2426507f),
	vec2(-0.7136765f, -0.4496614f),
	vec2(-0.5938849f, -0.6895654f),
	vec2(-0.3148003f, -0.7047654f),
	vec2(-0.42215f, -0.2024607f),
	vec2(-0.9466816f, -0.2014508f),
	vec2(-0.8409063f, -0.03465778f),
	vec2(-0.6517572f, -0.07476326f),
	vec2(-0.1041822f, -0.02521214f),
	vec2(-0.3042712f, -0.02195431f),
	vec2(-0.5082307f, 0.1079806f),
	vec2(-0.08429877f, -0.2316298f),
	vec2(-0.9879128f, 0.1113683f),
	vec2(-0.3859636f, 0.3363545f),
	vec2(-0.1925334f, 0.1787288f),
	vec2(0.003256182f, 0.138135f),
	vec2(-0.8706837f, 0.3010679f),
	vec2(-0.6982038f, 0.1904326f),
	vec2(0.1975043f, 0.2221317f),
	vec2(0.1507788f, 0.4204168f),
	vec2(0.3514056f, 0.09865579f),
	vec2(0.1558783f, -0.08460935f),
	vec2(-0.0684978f, 0.4461993f),
	vec2(0.3780522f, 0.3478679f),
	vec2(0.3956799f, -0.1469177f),
	vec2(0.5838975f, 0.1054943f),
	vec2(0.6155105f, 0.3245716f),
	vec2(0.3928624f, -0.4417621f),
	vec2(0.1749884f, -0.4202175f),
	vec2(0.6813727f, -0.2424808f),
	vec2(-0.6707711f, 0.4912741f),
	vec2(0.0005130528f, -0.8058334f),
	vec2(0.02703013f, -0.6010728f),
	vec2(-0.1658188f, -0.9695674f),
	vec2(0.4060591f, -0.7100726f),
	vec2(0.7713396f, -0.4713659f),
	vec2(0.573212f, -0.51544f),
	vec2(-0.3448896f, -0.9046497f),
	vec2(0.1268544f, -0.9874692f),
	vec2(0.7418533f, -0.6667366f),
	vec2(0.3492522f, 0.5924662f),
	vec2(0.5679897f, 0.5343465f),
	vec2(0.5663417f, 0.7708698f),
	vec2(0.7375497f, 0.6691415f),
	vec2(0.2271994f, -0.6163502f),
	vec2(0.2312844f, 0.8725659f),
	vec2(0.4216993f, 0.9002838f),
	vec2(0.4262091f, -0.9013284f),
	vec2(0.2001408f, -0.808381f),
	vec2(0.149394f, 0.6650763f),
	vec2(-0.09640376f, 0.9843736f),
	vec2(0.7682328f, -0.07273844f),
	vec2(0.04146584f, 0.8313184f),
	vec2(0.9705266f, -0.1143304f),
	vec2(0.9670017f, 0.1293385f),
	vec2(0.9015037f, -0.3306949f),
	vec2(-0.5085648f, 0.7534177f),
	vec2(0.9055501f, 0.3758393f),
	vec2(0.7599946f, 0.1809109f),
	vec2(-0.2483695f, 0.7942952f),
	vec2(-0.4241052f, 0.5581087f),
	vec2(-0.1020106f, 0.6724468f)
);

float random(vec3 seed, int i) {
	vec4 seed4 = vec4(seed, i);
	float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
	return fract(sin(dot_product) * 43758.5453);
}

#define max_point_lights 32

struct PointLight {
	float intensity, range;
	vec3 diffuse, specular;
	vec3 position;
};

struct DirectionalLight {
	float intensity;
	float bias;
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

layout (push_constant) uniform PushData {
	layout(offset = 64)
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
		max(dot(light_dir, normal), 0.0);

	vec3 specular =
		attenuation *
		light.specular *
		light.intensity *
		pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

	return diffuse + specular;
}

/* Find the average depth of the light blockers. */
float blocker_dist(vec3 coords, float size, float bias) {
	const int sample_count = 64;

	int blocker_count = 0;
	float r = 0.0;
	float width = size * (coords.z - data.near_plane) / data.camera_pos.z;

	for (int i = 0; i < sample_count; i++) {
		float z = texture(blockermap, coords.xy + poisson_disk[i] * width).r;
		if (z < coords.z + bias) {
			blocker_count++;
			r += z;
		}
	}

	if (blocker_count > 0) {
		return r / blocker_count;
	} else {
		return -1.0;
	}
}

float pcf(vec3 coords, float radius, float bias) {
	const int sample_count = 64;

	float r = 0.0;

	for (int i = 0; i < sample_count; i++) {
		float z = texture(shadowmap, vec3(coords.xy + poisson_disk[i] * radius, coords.z + bias)).r;
		r += (z < coords.z) ? 1.0 : 0.0;
	}

	return r / sample_count;
}

vec3 compute_directional_light(vec3 normal, vec3 view_dir, DirectionalLight light) {
	vec3 light_dir = normalize(light.direction);
	vec3 reflect_dir = reflect(light_dir, normal);

	vec3 diffuse =
		light.diffuse *
		light.intensity *
		max(dot(light_dir, normal), 0.0);

	vec3 specular =
		light.specular *
		light.intensity *
		pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

	const float light_size = 0.2;

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

	vec3 lighting_result = vec3(0.1);

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
