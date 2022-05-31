#include <time.h>
#include <stdlib.h>

#include <GLFW/glfw3.h>

#include "vkr.hpp"
#include "internal.hpp"

namespace vkr {
	struct impl_App {
		GLFWwindow* window;
	};

	static void on_framebuffer_resize(GLFWwindow* window, i32 w, i32 h) {
		auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));

		app->size = v2i(w, h);

		app->video->want_recreate = true;
	}

	static void on_key_event(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) {
		auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));

		auto app_key = static_cast<i32>(app->key_from_key(key));

		app->held_keys    [app_key] = action == GLFW_PRESS || action == GLFW_REPEAT;
		app->pressed_keys [app_key] = action == GLFW_PRESS;
		app->released_keys[app_key] = action == GLFW_RELEASE;
	}

	static void on_mouse_button_event(GLFWwindow* window, i32 button, i32 action, i32 mods) {
		auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));

		auto app_mb = static_cast<i32>(app->mb_from_mb(button));

		app->held_mouse_buttons    [app_mb] = action == GLFW_PRESS || action == GLFW_REPEAT;
		app->pressed_mouse_buttons [app_mb] = action == GLFW_PRESS;
		app->released_mouse_buttons[app_mb] = action == GLFW_RELEASE;
	}

	static void on_mouse_move(GLFWwindow* window, f64 x, f64 y) {	
		auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));

		app->mouse_pos = v2i(static_cast<i32>(x), static_cast<i32>(y));
	}

	App::App(const char* title, v2i size) : handle(null), title(title), size(size) {
		/* Setup keybinds. Because my autism won't let me make the user include glfw3.h. */
		keymap[GLFW_KEY_UNKNOWN]       = key_unknown;
		keymap[GLFW_KEY_SPACE]         = key_space;
		keymap[GLFW_KEY_APOSTROPHE]    = key_apostrophe;
		keymap[GLFW_KEY_COMMA]         = key_comma;
		keymap[GLFW_KEY_MINUS]         = key_minus;
		keymap[GLFW_KEY_PERIOD]        = key_period;
		keymap[GLFW_KEY_SLASH]         = key_slash;
		keymap[GLFW_KEY_0]             = key_0;
		keymap[GLFW_KEY_1]             = key_1;
		keymap[GLFW_KEY_2]             = key_2;
		keymap[GLFW_KEY_3]             = key_3;
		keymap[GLFW_KEY_4]             = key_4;
		keymap[GLFW_KEY_5]             = key_5;
		keymap[GLFW_KEY_6]             = key_6;
		keymap[GLFW_KEY_7]             = key_7;
		keymap[GLFW_KEY_8]             = key_8;
		keymap[GLFW_KEY_9]             = key_9;
		keymap[GLFW_KEY_SEMICOLON]     = key_semicolon;
		keymap[GLFW_KEY_EQUAL]         = key_equal;
		keymap[GLFW_KEY_A]             = key_A;
		keymap[GLFW_KEY_B]             = key_B;
		keymap[GLFW_KEY_C]             = key_C;
		keymap[GLFW_KEY_D]             = key_D;
		keymap[GLFW_KEY_E]             = key_E;
		keymap[GLFW_KEY_F]             = key_F;
		keymap[GLFW_KEY_G]             = key_G;
		keymap[GLFW_KEY_H]             = key_H;
		keymap[GLFW_KEY_I]             = key_I;
		keymap[GLFW_KEY_J]             = key_J;
		keymap[GLFW_KEY_K]             = key_K;
		keymap[GLFW_KEY_L]             = key_L;
		keymap[GLFW_KEY_M]             = key_M;
		keymap[GLFW_KEY_N]             = key_N;
		keymap[GLFW_KEY_O]             = key_O;
		keymap[GLFW_KEY_P]             = key_P;
		keymap[GLFW_KEY_Q]             = key_Q;
		keymap[GLFW_KEY_R]             = key_R;
		keymap[GLFW_KEY_S]             = key_S;
		keymap[GLFW_KEY_T]             = key_T;
		keymap[GLFW_KEY_U]             = key_U;
		keymap[GLFW_KEY_V]             = key_V;
		keymap[GLFW_KEY_W]             = key_W;
		keymap[GLFW_KEY_X]             = key_X;
		keymap[GLFW_KEY_Y]             = key_Y;
		keymap[GLFW_KEY_Z]             = key_Z;
		keymap[GLFW_KEY_BACKSLASH]     = key_backslash;
		keymap[GLFW_KEY_GRAVE_ACCENT]  = key_grave_accent;
		keymap[GLFW_KEY_ESCAPE]        = key_escape;
		keymap[GLFW_KEY_ENTER]         = key_return;
		keymap[GLFW_KEY_TAB]           = key_tab;
		keymap[GLFW_KEY_BACKSPACE]     = key_backspace;
		keymap[GLFW_KEY_INSERT]        = key_insert;
		keymap[GLFW_KEY_DELETE]        = key_delete;
		keymap[GLFW_KEY_RIGHT]         = key_right;
		keymap[GLFW_KEY_LEFT]          = key_left;
		keymap[GLFW_KEY_DOWN]          = key_down;
		keymap[GLFW_KEY_UP]            = key_up;
		keymap[GLFW_KEY_PAGE_UP]       = key_page_up;
		keymap[GLFW_KEY_PAGE_DOWN]     = key_page_down;
		keymap[GLFW_KEY_HOME]          = key_home;
		keymap[GLFW_KEY_END]           = key_end;
		keymap[GLFW_KEY_F1]            = key_f1;
		keymap[GLFW_KEY_F2]            = key_f2;
		keymap[GLFW_KEY_F3]            = key_f3;
		keymap[GLFW_KEY_F4]            = key_f4;
		keymap[GLFW_KEY_F5]            = key_f5;
		keymap[GLFW_KEY_F6]            = key_f6;
		keymap[GLFW_KEY_F7]            = key_f7;
		keymap[GLFW_KEY_F8]            = key_f8;
		keymap[GLFW_KEY_F9]            = key_f9;
		keymap[GLFW_KEY_F10]           = key_f10;
		keymap[GLFW_KEY_F11]           = key_f11;
		keymap[GLFW_KEY_F12]           = key_f12;
		keymap[GLFW_KEY_LEFT_SHIFT]    = key_shift;
		keymap[GLFW_KEY_RIGHT_SHIFT]   = key_shift;
		keymap[GLFW_KEY_LEFT_CONTROL]  = key_control;
		keymap[GLFW_KEY_RIGHT_CONTROL] = key_control;
		keymap[GLFW_KEY_LEFT_ALT]      = key_alt;
		keymap[GLFW_KEY_RIGHT_ALT]     = key_alt;
		keymap[GLFW_KEY_LEFT_SUPER]    = key_super;
		keymap[GLFW_KEY_RIGHT_SUPER]   = key_super;
		keymap[GLFW_KEY_MENU]          = key_menu;

		mousemap[GLFW_MOUSE_BUTTON_LEFT]   = mouse_button_left;
		mousemap[GLFW_MOUSE_BUTTON_MIDDLE] = mouse_button_middle;
		mousemap[GLFW_MOUSE_BUTTON_RIGHT]  = mouse_button_right;
	}

	void App::run() {
		srand(time(0));

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
		glfwSetFramebufferSizeCallback(handle->window, on_framebuffer_resize);
		glfwSetKeyCallback(handle->window, on_key_event);
		glfwSetMouseButtonCallback(handle->window, on_mouse_button_event);
		glfwSetCursorPosCallback(handle->window, on_mouse_move);

		on_init();

		f64 now = glfwGetTime(), last = now;
		f64 ts = 0.0;

		while (!glfwWindowShouldClose(handle->window)) {
			memset(pressed_keys,  0, static_cast<usize>(key_count) * sizeof(bool));
			memset(released_keys, 0, static_cast<usize>(key_count) * sizeof(bool));
			memset(pressed_mouse_buttons,  0, static_cast<usize>(mouse_button_count) * sizeof(bool));
			memset(released_mouse_buttons, 0, static_cast<usize>(mouse_button_count) * sizeof(bool));

			glfwPollEvents();

			while (video->want_recreate && (size.x == 0 || size.y == 0)) {
				glfwGetFramebufferSize(handle->window, &size.x, &size.y);
				glfwWaitEvents();
			}

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
