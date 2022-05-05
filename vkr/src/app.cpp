#include <GLFW/glfw3.h>

#include "vkr.hpp"
#include "internal.hpp"

namespace vkr {
	struct impl_App {
		GLFWwindow* window;
	};

	App::App(const char* title, v2i size) : handle(null), title(title), size(size) {}

	void App::run() {
		handle = new impl_App();

		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		bool enable_validation_layers = 
#ifdef DEBUG
			true
#else
			false
#endif
		;

		u32 ext_count = 0;
		const char** exts = glfwGetRequiredInstanceExtensions(&ext_count);

		handle->window = glfwCreateWindow(size.x, size.y, title, null, null);
		if (!handle->window) {
			abort_with("Failed to create window.");
		}

		video = new VideoContext(*this, title, enable_validation_layers, ext_count, exts);

		on_init();

		while (!glfwWindowShouldClose(handle->window)) {
			glfwPollEvents();

			video->begin();
			on_update(1.0);
			video->end();
		}

		on_deinit();

		video->wait_for_done();

		glfwDestroyWindow(handle->window);

		delete video;

		glfwTerminate();

		delete handle;
	}

	bool App::create_window_surface(const VideoContext& ctx) const {
		return glfwCreateWindowSurface(ctx.handle->instance, handle->window, null, &ctx.handle->surface) == VK_SUCCESS;
	}

	v2i App::get_size() const {
		return size;
	}
}
