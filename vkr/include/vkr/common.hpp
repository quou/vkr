#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
	#define VKR_EXPORT_SYM __declspec(dllexport)
	#define VKR_IMPORT_SYM __declspec(dllimport)
	#ifdef VKR_EXPORT_SYMBOLS
		#define VKR_API VKR_EXPORT_SYM
	#else
		#define VKR_API VKR_IMPORT_SYM
	#endif
#else
	#define VKR_EXPORT_SYM
	#define VKR_IMPORT_SYM
	#define VKR_API
#endif

#ifndef null
#define null nullptr
#endif

namespace vkr {
	typedef int8_t   i8;
	typedef int16_t  i16;
	typedef int32_t  i32;
	typedef int64_t  i64;
	typedef uint8_t  u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;

	typedef size_t usize;

	typedef float f32;
	typedef double f64;

	/* Class forward declarations. */
	class App;
	class Buffer;
	class IndexBuffer;
	class Framebuffer;
	class Pipeline;
	class Texture;
	class Shader;
	class UniformBuffer;
	class VertexBuffer;
	class VideoContext;
}
