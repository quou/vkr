#include <GLFW/glfw3.h>

#include "vkr.hpp"
#include "internal.hpp"

namespace vkr {
	struct impl_App {
		GLFWwindow* window;
	};

	static void on_resize(GLFWwindow* window, i32 w, i32 h) {
		auto app = static_cast<App*>(glfwGetWindowUserPointer(window));

		app->size = v2i(w, h);

		app->video->resize(*app, app->size);
	}

	App::App(const char* title, v2i size) : handle(null), title(title), size(size) {}

	void App::run() {
		handle = new impl_App();

		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

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

		glfwSetWindowUserPointer(handle->window, this);
		glfwSetWindowSizeCallback(handle->window, on_resize);

		on_init();

		f64 now = glfwGetTime(), last = now;
		f64 ts = 0.0;

		while (!glfwWindowShouldClose(handle->window)) {
			glfwPollEvents();

			video->begin();
			on_update(ts);
			video->end();

			now = glfwGetTime();
			ts = now - last;
			last = now;
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

	Framebuffer* App::get_default_framebuffer() const {
		return video->default_fb;
	}
}
