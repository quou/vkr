#pragma once

#include <stdint.h>
#include <stdarg.h>

#include "common.hpp"
#include "maths.hpp"

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

	/* Class forward declarations. */
	class App;
	class Buffer;
	class IndexBuffer;
	class Shader;
	class VertexBuffer;
	class VideoContext;

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

	class VKR_API Pipeline {
	private:
		impl_Pipeline* handle;
	public:
		Pipeline(const VideoContext* video);
		~Pipeline();
	};

	struct Vertex {
		v2f position;
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
		VertexBuffer(VideoContext* video, Vertex* verts, usize count);
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

		friend class Buffer;
		friend class VertexBuffer;
		friend class IndexBuffer;
	public:
		impl_VideoContext* handle;

		VideoContext(const App& app, const char* app_name, bool enable_validation_layers, u32 extension_count, const char** extensions);
		~VideoContext();

		/* Waits for all the current operations to finish. */
		void wait_for_done() const;

		void begin();
		void end();
	};
};
