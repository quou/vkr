layout (binding = 0) uniform PostProcessConfig {
	float bloom_threshold;
	float bloom_blur_intensity;
	float bloom_intensity;
	vec2 screen_size;
	vec3 camera_pos;
} config;
