#pragma once

#include "common.hpp"
#include "maths.hpp"
#include "renderer.hpp"

namespace vkr {
	/* Simple immediate mode GUI, for basic tools and
	 * for displaying debug information. */
	class VKR_API UIContext {
	private:
		v2i screen_size;
		v2i cursor_pos;

		struct Command {
			enum class Type {
				draw_rect,
				draw_text
			} type;

			usize size;
		};

		struct DrawTextCommand : public Command {
			v2i position;
			const char* text;
		};

		/* Fixed-size command buffer; allows one megabyte of commands.
		 *
		 * This GUI works by pushing draw commands to the command
		 * buffer, which is then iterated in the `draw' function. This
		 * means that the `begin'/`end' functions don't necessarily
		 * have to be within a framebuffer `begin'/`end', allowing for
		 * more flexibility in the way that the GUI code is written. */
		u8 command_buffer[1024 * 1024];

	public:
		UIContext();
		~UIContext();

		void begin(v2i screen_size);
		void end();
		void draw(Renderer2D* renderer);

		bool begin_window(const char* title, v2i default_size = v2i(200, 300));
		void end_window();

		void use_font(Font* font, v4f color = v4f(1.0f, 1.0f, 1.0f, 1.0f));

		void label(const char* text);
	};
}
