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
	struct impl_VideoContext;
	struct impl_Shader;

	/* To be inherited by client applications to provide custom
	 * functionality and data. */
	class VKR_API App {
	private:
		impl_App* handle;

		const char* title;
		v2i size;

		bool create_window_surface(const VideoContext& ctx) const;

		friend class VideoContext;
	protected:
		VideoContext* video;
	public:
		App(const char* title, v2i size);

		virtual void on_init() = 0;
		virtual void on_update(f64) = 0;
		virtual void on_deinit() = 0;

		virtual ~App() {};

		void run();

		v2i get_size() const;
	};

	/* The Pipeline class and its children take care of
	 * of managing a Vulkan pipeline and render pass. */
	class VKR_API Pipeline {
	protected:
		VideoContext* video;
		impl_Pipeline* handle;

		usize uniform_count;

		friend class IndexBuffer;
	public:
		enum class Stage {
			vertex, fragment
		};
	
		struct Attribute {
			u32 location;
			usize offset;

			enum class Type {
				float1, float2, float3, float4
			} type;
		};

		struct UniformBuffer {
			u32 binding;
			void* ptr;
			usize size;

			Stage stage;
		};

		struct PushConstantRange {
			usize size;
			usize start;
			Stage stage;
		};

		Pipeline(VideoContext* video, Shader* shader, usize stride,
			Attribute* attribs, usize attrib_count,
			UniformBuffer* uniforms = null, usize uniform_count = 0,
			PushConstantRange* pcranges = null, usize pcrange_count = 0);
		virtual ~Pipeline();

		void make_default();

		void begin();
		void end();

		void push_constant(Stage stage, const void* ptr, usize size, usize offset = 0);

		template <typename T>
		void push_constant(Stage stage, const T& c, usize offset = 0) {
			push_constant(stage, &c, sizeof(T), offset);
		}
	};

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
	};

	class VKR_API IndexBuffer : public Buffer {
	private:
		usize count;
	public:
		IndexBuffer(VideoContext* video, u16* indices, usize count);
		~IndexBuffer();

		void draw();
	};

	class VKR_API VideoContext {
	private:
		bool validation_layers_supported();
		void record_commands(u32 image);

		u32 current_frame;
		u32 image_id;

		usize object_count;

		Pipeline* pipeline;

		friend class Buffer;
		friend class IndexBuffer;
		friend class Pipeline;
		friend class VertexBuffer;
	public:
		impl_VideoContext* handle;

		VideoContext(const App& app, const char* app_name, bool enable_validation_layers, u32 extension_count, const char** extensions);
		~VideoContext();

		/* Waits for all the current operations to finish. */
		void wait_for_done() const;

		void begin();
		void end();
	};

	class Shader {
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
