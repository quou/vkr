#pragma once

#include <stdint.h>
#include <stdarg.h>

#include "common.hpp"
#include "maths.hpp"
#include "renderer.hpp"
#include "ui.hpp"
#include "wavefront.hpp"

namespace vkr {
	/* Basic logging. */
	VKR_API void error(const char* fmt, ...);
	VKR_API void warning(const char* fmt, ...);
	VKR_API void info(const char* fmt, ...);
	VKR_API void abort_with(const char* fmt, ...);
	VKR_API void verror(const char* fmt, va_list args);
	VKR_API void vwarning(const char* fmt, va_list args);
	VKR_API void vinfo(const char* fmt, va_list args);
	VKR_API void vabort_with(const char* fmt, va_list args);

	VKR_API void init_packer(i32 argc, const char** argv);
	VKR_API void deinit_packer();

	VKR_API bool read_raw(const char* path, u8** buffer, usize* size);

	VKR_API u64 elf_hash(const u8* data, usize size);
	VKR_API u64 hash_string(const char* str);

	/* These structures are for storing information about the
	 * state of a class wrapper that might rely on external headers,
	 * namely glfw3.h and vulkan.h. */
	struct impl_App;
	struct impl_Buffer;
	struct impl_Framebuffer;
	struct impl_Pipeline;
	struct impl_Sampler;
	struct impl_Shader;
	struct impl_Texture;
	struct impl_VideoContext;

	enum Key {
		key_unknown = 0,
		key_space,
		key_apostrophe,
		key_comma,
		key_minus,
		key_period,
		key_slash,
		key_0,
		key_1,
		key_2,
		key_3,
		key_4,
		key_5,
		key_6,
		key_7,
		key_8,
		key_9,
		key_semicolon,
		key_equal,
		key_A,
		key_B,
		key_C,
		key_D,
		key_E,
		key_F,
		key_G,
		key_H,
		key_I,
		key_J,
		key_K,
		key_L,
		key_M,
		key_N,
		key_O,
		key_P,
		key_Q,
		key_R,
		key_S,
		key_T,
		key_U,
		key_V,
		key_W,
		key_X,
		key_Y,
		key_Z,
		key_backslash,
		key_grave_accent,
		key_escape,
		key_return,
		key_tab,
		key_backspace,
		key_insert,
		key_delete,
		key_right,
		key_left,
		key_down,
		key_up,
		key_page_up,
		key_page_down,
		key_home,
		key_end,
		key_f1,
		key_f2,
		key_f3,
		key_f4,
		key_f5,
		key_f6,
		key_f7,
		key_f8,
		key_f9,
		key_f10,
		key_f11,
		key_f12,
		key_shift,
		key_control,
		key_alt,
		key_super,
		key_menu,
		key_count
	};

	enum MouseButton {
		mouse_button_unknown = 0,
		mouse_button_left,
		mouse_button_middle,
		mouse_button_right,
		mouse_button_count
	};

	/* To be inherited by client applications to provide custom
	 * functionality and data. */
	class App {
	private:
		impl_App* handle;

		std::unordered_map<i32, Key>         keymap;
		std::unordered_map<i32, MouseButton> mousemap;

		const char* title;

		bool create_window_surface(const VideoContext& ctx) const;

		friend class VideoContext;
	public:
		v2i size, mouse_pos;
		VideoContext* video;

		VKR_API App(const char* title, v2i size);

		virtual void on_init() = 0;
		virtual void on_update(f64) = 0;
		virtual void on_deinit() = 0;

		VKR_API virtual ~App() {};

		VKR_API void run();

		VKR_API v2i get_size() const;
		VKR_API Framebuffer* get_default_framebuffer() const;

		VKR_API void lock_mouse();
		VKR_API void unlock_mouse();

		bool held_keys    [static_cast<i32>(key_count)];
		bool pressed_keys [static_cast<i32>(key_count)];
		bool released_keys[static_cast<i32>(key_count)];

		bool held_mouse_buttons    [static_cast<i32>(mouse_button_count)];
		bool pressed_mouse_buttons [static_cast<i32>(mouse_button_count)];
		bool released_mouse_buttons[static_cast<i32>(mouse_button_count)];

		inline Key key_from_key(i32 c)       { return keymap.count(c) != 0 ? keymap[c] : key_unknown; }
		inline MouseButton mb_from_mb(i32 c) { return mousemap.count(c) != 0 ? mousemap[c] : mouse_button_unknown; }

		inline bool key_pressed      (Key key) const { return held_keys    [static_cast<i32>(key)]; }
		inline bool key_just_pressed (Key key) const { return pressed_keys [static_cast<i32>(key)]; }
		inline bool key_just_released(Key key) const { return released_keys[static_cast<i32>(key)]; }

		inline bool mouse_button_pressed      (MouseButton mb) const { return held_mouse_buttons    [static_cast<i32>(mb)]; }
		inline bool mouse_button_just_pressed (MouseButton mb) const { return pressed_mouse_buttons [static_cast<i32>(mb)]; }
		inline bool mouse_button_just_released(MouseButton mb) const { return released_mouse_buttons[static_cast<i32>(mb)]; }
	};

	class VKR_API Framebuffer {
	private:
		VideoContext* video;
		impl_Framebuffer* handle;

		bool depth_enable;

		v2i size;
		v2i drawable_size;
		f32 scale;

		bool is_recreating;

		friend class VideoContext;
		friend class Pipeline;
	public:
		enum class Flags {
			default_fb    = 1 << 0, /* To be managed by the video context only. */
			headless      = 1 << 1, /* Creates a sampler to be sampled from a shader. */
			fit           = 1 << 2, /* Fit the framebuffer to the window (Re-create it on window resize). */
		} flags;

		struct Attachment {
			enum class Type {
				color,
				depth
			} type;

			enum class Format {
				depth,
				red8,
				rgb8,
				rgba8,
				redf32,
				rgbf32,
				rgbaf32,
				redf16,
				rgbf16,
				rgbaf16
			} format;
		};

		/* The scale parameter allows for supersampling. */
		Framebuffer(VideoContext* video, Flags flags, v2i size,
			Attachment* attachments, usize attachment_count, f32 scale = 1.0f, bool is_recreating = false);
		~Framebuffer();

		inline v2i get_size() const { return size; }
		inline v2i get_scaled_size() const { return v2i((i32)((f32)size.x * scale), (i32)((f32)size.y * scale)); }
		inline v2i get_drawable_size() const { return drawable_size; }

		void resize(v2i size);

		void begin();
		void end();
	private:
		Attachment* attachments;
		usize attachment_count;
	};

	inline Framebuffer::Flags operator|(Framebuffer::Flags a, Framebuffer::Flags b) {
		return static_cast<Framebuffer::Flags>(static_cast<i32>(a) | static_cast<i32>(b));
	}

	inline i32 operator&(Framebuffer::Flags a, Framebuffer::Flags b) {
		return static_cast<i32>(a) & static_cast<i32>(b);
	}

	/* The Pipeline class and its children take care of
	 * of managing a Vulkan pipeline and render pass. */
	class VKR_API Pipeline {
	protected:
		VideoContext* video;
		impl_Pipeline* handle;

		Framebuffer* framebuffer;

		bool is_recreating = false;

		friend class IndexBuffer;
	public:
		enum class Stage {
			vertex, fragment
		};

		struct Attribute {
			const char* name;
			u32 location;
			usize offset;

			enum class Type {
				float1, float2, float3, float4
			} type;
		};

		struct ResourcePointer {
			enum class Type {
				texture,
				framebuffer_output,
				uniform_buffer
			} type;

			union {
				struct {
					void* ptr;
					usize size;
				} uniform;

				struct {
					Texture* ptr;
				} texture;

				struct {
					Framebuffer* ptr;
					Sampler* sampler;
					u32 attachment;
				} framebuffer;
			};
		};

		struct Descriptor {
			const char* name;

			u32 binding;

			Stage stage;

			ResourcePointer resource;
		};

		struct DescriptorSet {
			const char* name;

			Descriptor* descriptors;
			usize count;
		};

		struct PushConstantRange {
			const char* name;
			usize size;
			usize start;
			Stage stage;
		};

		enum class Flags {
			none                         = 1 << 0,
			depth_test                   = 1 << 1,
			cull_back_face               = 1 << 2,
			cull_front_face              = 1 << 3,
			front_face_clockwise         = 1 << 4,
			front_face_counter_clockwise = 1 << 5,
			blend                        = 1 << 6,
			dynamic_scissor              = 1 << 7,
		} flags;

		Pipeline(VideoContext* video, Flags flags, Shader* shader, usize stride,
			Attribute* attribs, usize attrib_count,
			Framebuffer* framebuffer,
			DescriptorSet* desc_sets = null, usize desc_set_count = 0,
			PushConstantRange* pcranges = null, usize pcrange_count = 0, bool is_recreating = false);
		virtual ~Pipeline();

		void clear();

		void begin();
		void end();

		void set_scissor(v4i rect);

		void push_constant(Stage stage, const void* ptr, usize size, usize offset = 0);
		void bind_descriptor_set(usize target, usize index);

		template <typename T>
		void push_constant(Stage stage, const T& c, usize offset = 0) {
			push_constant(stage, &c, sizeof(T), offset);
		}

		void recreate();

	private:
		/* Cache for all of the constructor arguments for re-creating
		 * the pipeline whenever the window is resized. */
		Shader* shader;
		usize stride;
		Attribute* attribs;
		usize attrib_count;
		DescriptorSet* descriptor_sets;
		usize descriptor_set_count;
		PushConstantRange* pcranges;
		usize pcrange_count;

		usize uniform_count, sampler_count;
	};

	inline Pipeline::Flags operator|(Pipeline::Flags a, Pipeline::Flags b) {
		return static_cast<Pipeline::Flags>(static_cast<i32>(a) | static_cast<i32>(b));
	}

	inline i32 operator&(Pipeline::Flags a, Pipeline::Flags b) {
		return static_cast<i32>(a) & static_cast<i32>(b);
	}

	class VKR_API Buffer {
	protected:
		VideoContext* video;
		impl_Buffer* handle;

		Buffer(VideoContext* video);
		virtual ~Buffer();
	};

	class VKR_API VertexBuffer : public Buffer {
	private:
		bool dynamic;
	public:
		VertexBuffer(VideoContext* video, void* verts, usize size, bool dynamic = false);
		~VertexBuffer();

		void bind();
		void draw(usize count, usize offset = 0);
		void update(void* verts, usize size, usize offset);
	};

	class VKR_API IndexBuffer : public Buffer {
	private:
		usize count;
	public:
		IndexBuffer(VideoContext* video, u16* indices, usize count);
		~IndexBuffer();

		void draw();
	};

	class VKR_API Sampler {
	private:
		impl_Sampler* handle;

		VideoContext* video;

		friend class Pipeline;
	public:
		enum Flags {
			filter_linear = 1 << 0,
			filter_none   = 1 << 1,
			shadow        = 1 << 2,
			clamp         = 1 << 3,
			repeat        = 1 << 4,
		} flags;

		Sampler(VideoContext* video, Flags flags);
		~Sampler();
	};

	inline Sampler::Flags operator|(Sampler::Flags a, Sampler::Flags b) {
		return static_cast<Sampler::Flags>(static_cast<i32>(a) | static_cast<i32>(b));
	}

	inline i32 operator&(Sampler::Flags a, Sampler::Flags b) {
		return static_cast<i32>(a) & static_cast<i32>(b);
	}

	class VKR_API Texture {
	private:
		VideoContext* video;
		impl_Texture* handle;

		v2i size;

		friend class Pipeline;
	public:
		enum class Flags {
			dimentions_1  = 1 << 0,
			dimentions_2  = 1 << 1,
			dimentions_3  = 1 << 2,
			filter_linear = 1 << 3,
			filter_none   = 1 << 4,
			format_grey8  = 1 << 5,
			format_rgb8   = 1 << 6,
			format_rgba8  = 1 << 7,
			format_grey16 = 1 << 8,
			format_rgb16  = 1 << 9,
			format_rgba16 = 1 << 10,
			format_grey32 = 1 << 11,
			format_rgb32  = 1 << 12,
			format_rgba32 = 1 << 13
		} flags;

		Texture(VideoContext* video, const void* data, v2i size, Flags flags);
		~Texture();

		static Texture* from_file(VideoContext* video, const char* file_path, Flags flags = Flags::filter_none);

		inline v2i get_size() const { return size; }
	};

	inline Texture::Flags operator|(Texture::Flags a, Texture::Flags b) {
		return static_cast<Texture::Flags>(static_cast<i32>(a) | static_cast<i32>(b));
	}

	inline i32 operator&(Texture::Flags a, Texture::Flags b) {
		return static_cast<i32>(a) & static_cast<i32>(b);
	}

	class VideoContext {
	private:
		bool validation_layers_supported();

		u32 current_frame;
		u32 image_id;

		usize object_count;

		Framebuffer* default_fb;

		Pipeline* pipeline;

		friend class App;
		friend class Buffer;
		friend class IndexBuffer;
		friend class Pipeline;
		friend class VertexBuffer;
		friend class Framebuffer;

		void init_swapchain();
		void deinit_swapchain();

		const App& app;

		/* Stored to iterate over and re-create when the
		 * window is resized. */
		std::vector<Framebuffer*> framebuffers;
		std::vector<Pipeline*> pipelines;

		bool skip_frame;

		bool validation_layers_enabled;
	public:
		impl_VideoContext* handle;
		bool want_recreate;

		VKR_API VideoContext(const App& app, const char* app_name, bool enable_validation_layers, u32 extension_count, const char** extensions);
		VKR_API ~VideoContext();

		/* Waits for all the current operations to finish. */
		VKR_API void wait_for_done() const;

		VKR_API void begin();
		VKR_API void end();

		VKR_API void resize(v2i new_size);

		inline bool are_validation_layers_enabled() const { return validation_layers_enabled; }
	};

	class VKR_API Shader {
	private:
		impl_Shader* handle;
		VideoContext* video;

		friend class Pipeline;
	public:
		Shader(VideoContext* video, const u8* v_buf, const u8* f_buf, usize v_size, usize f_size);
		static Shader* from_file(VideoContext* video, const char* vert_path, const char* frag_path);
		~Shader();
	};
};
