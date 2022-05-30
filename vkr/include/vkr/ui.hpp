#pragma once

#include "common.hpp"
#include "maths.hpp"
#include "renderer.hpp"

namespace vkr {
	/* Simple immediate mode GUI, for basic tools and
	 * for displaying debug information. */
	class VKR_API UIContext {
	public:
		enum class StyleVar : u32 {
			padding = 0,
			count
		};

		enum class StyleColor : u32 {
			background = 0,
			border,
			count
		};
	private:
		v2i screen_size;
		v2f cursor_pos;

		struct Command {
			enum class Type : u8 {
				draw_rect,
				draw_text,
				bind_font
			} type;

			usize size;
		};

		struct DrawTextCommand : public Command {
			v2f position;
			char* text;
		};

		struct DrawRectCommand : public Command {
			v2f position, dimentions;
			v4f color;
		};

		struct BindFontCommand : public Command {
			Font* font;
			v4f color;
		};

		void cmd_draw_rect(v2f position, v2f dimentions, v4f color);
		void cmd_draw_text(const char* text, v2f position);
		void cmd_bind_font(Font* font, v4f color);

		/* Fixed-size command buffer; allows one megabyte of commands.
		 *
		 * This GUI works by pushing draw commands to the command
		 * buffer, which is then iterated in the `draw' function. This
		 * means that the `begin'/`end' functions don't necessarily
		 * have to be within a framebuffer `begin'/`end', allowing for
		 * more flexibility in the way that the GUI code is written. */
		u8 command_buffer[1024 * 1024];
		usize command_buffer_idx;

		f32 style_vars[static_cast<u32>(StyleVar::count)];
		v4f style_colors[static_cast<u32>(StyleColor::count)];

		struct {
			v2f position;
			v2f content_offset;
			v2f max_content_dimentions;
			v2f content_dimentions;
		} window;

		Font* bound_font;
		v4f bound_font_color;
	public:
		UIContext();
		~UIContext();

		void begin(v2i screen_size);
		void end();
		void draw(Renderer2D* renderer);

		bool begin_window(const char* title, v2f default_position = v2f(30, 30), v2f default_size = v2f(200, 300));
		void end_window();

		void use_font(Font* font, v4f color = v4f(1.0f, 1.0f, 1.0f, 1.0f));

		void label(const char* text);

		inline void set_style_var(StyleVar v, f32 value) {
			if (v >= StyleVar::count) { return; }

			style_vars[static_cast<u32>(v)] = value;
		}

		inline void set_style_color(StyleColor v, v4f value) {
			if (v >= StyleColor::count) { return; }

			style_colors[static_cast<u32>(v)] = value;
		}

		inline f32 get_style_var(StyleVar v) {
			if (v >= StyleVar::count) { return 0.0f; }

			return style_vars[static_cast<u32>(v)];
		}

		inline v4f get_style_color(StyleColor v) {
			if (v >= StyleColor::count) { return v4f(); }

			return style_colors[static_cast<u32>(v)];
		}
	};
}
