#include <GLFW/glfw3.h>

#include "vkr.hpp"

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

		video = new VideoContext(title, enable_validation_layers, ext_count, exts);

		handle->window = glfwCreateWindow(size.x, size.y, title, null, null);
		if (!handle->window) {
			abort_with("Failed to create window.");
		}

		while (!glfwWindowShouldClose(handle->window)) {
			glfwPollEvents();
		}

		glfwTerminate();

		delete video;
		delete handle;
	}
}
