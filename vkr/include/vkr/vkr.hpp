#pragma once

#include <stdint.h>
#include <stdarg.h>

#include "common.hpp"
#include "maths.hpp"
#include "renderer.hpp"
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

	VKR_API bool read_raw(const char* path, u8** buffer, usize* size);
	VKR_API bool read_raw_text(const char* path, char** buffer);
	VKR_API bool write_raw(const char* path, u8* buffer, usize* size);
	VKR_API bool write_raw_text(const char* path, char* buffer);

	/* These structures are for storing information about the
	 * state of a class wrapper that might rely on external headers,
	 * namely glfw3.h and vulkan.h. */
	struct impl_App;
	struct impl_Buffer;
	struct impl_Pipeline;
	struct impl_Shader;
	struct impl_Texture;
	struct impl_VideoContext;
	struct impl_Framebuffer;

	/* To be inherited by client applications to provide custom
	 * functionality and data. */
	class VKR_API App {
	private:
		impl_App* handle;

		const char* title;

		bool create_window_surface(const VideoContext& ctx) const;

		friend class VideoContext;
	public:
		v2i size;
		VideoContext* video;

		App(const char* title, v2i size);

		virtual void on_init() = 0;
		virtual void on_update(f64) = 0;
		virtual void on_deinit() = 0;

		virtual ~App() {};

		void run();

		v2i get_size() const;
		Framebuffer* get_default_framebuffer() const;
	};

	class VKR_API Framebuffer {
	private:
		VideoContext* video;
		impl_Framebuffer* handle;

		bool depth_enable;

		v2i size;
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
		inline v2i get_scaled_size() const { return v2i((i32)((f32)size.x) * scale, (i32)((f32)size.y) * scale); }

		void resize(v2i size);
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
			depth_test       = 1 << 0,
			cull_back_face   = 1 << 1,
			cull_front_face  = 1 << 2,
		} flags;

		Pipeline(VideoContext* video, Flags flags, Shader* shader, usize stride,
			Attribute* attribs, usize attrib_count,
			Framebuffer* framebuffer,
			DescriptorSet* desc_sets = null, usize desc_set_count = 0,
			PushConstantRange* pcranges = null, usize pcrange_count = 0, bool is_recreating = false);
		virtual ~Pipeline();

		void begin();
		void end();

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
	public:
		VertexBuffer(VideoContext* video, void* verts, usize size);
		~VertexBuffer();

		void bind();
		void draw(usize count);
	};

	class VKR_API IndexBuffer : public Buffer {
	private:
		usize count;
	public:
		IndexBuffer(VideoContext* video, u16* indices, usize count);
		~IndexBuffer();

		void draw();
	};

	class VKR_API Texture {
	private:
		VideoContext* video;
		impl_Texture* handle;

		v2i size;
		u32 component_count;

		friend class Pipeline;
	public:
		Texture(VideoContext* video, void* data, v2i size, u32 component_count);
		~Texture();

		static Texture* from_file(VideoContext* video, const char* file_path);

		inline v2i get_size() const { return size; }
		inline u32 get_component_count() const { return component_count; }
	};

	class VKR_API VideoContext {
	private:
		bool validation_layers_supported();
		void record_commands(u32 image);

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

		bool skip_frame;

		/* Stored to iterate over and re-create when the
		 * window is resized. */
		std::vector<Framebuffer*> framebuffers;
		std::vector<Pipeline*> pipelines;
	public:
		impl_VideoContext* handle;
		bool want_recreate;

		VideoContext(const App& app, const char* app_name, bool enable_validation_layers, u32 extension_count, const char** extensions);
		~VideoContext();

		/* Waits for all the current operations to finish. */
		void wait_for_done() const;

		void begin();
		void end();

		void resize(v2i new_size);
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
