#pragma once

#include <map>

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
			background2,
			hovered,
			hot,
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
				bind_font,
				set_clip
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

		struct SetClipCommand : public Command {
			v2f position, dimentions;
		};

		void cmd_draw_rect(v2f position, v2f dimentions, v4f color);
		void cmd_draw_text(const char* text, usize len, v2f position);
		void cmd_bind_font(Font* font, v4f color);
		void cmd_set_clip(v2f position, v2f dimentions);

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

		struct WindowMeta {
			v2f position;
			v2f dimentions;
			v2f content_offset;
			v2f max_content_dimentions;
			v2f content_dimentions;
		}* window;

		std::unordered_map<u64, WindowMeta> meta;

		Font* bound_font;
		v4f bound_font_color;

		bool anything_hovered;
		bool anything_hot;

		u64 dragging;
		v2f drag_offset;

		usize column_count;
		f32 column_size;

		usize item;
		f32 current_item_height;

		App* app;
	public:
		UIContext(App* app);
		~UIContext();

		void begin(v2i screen_size);
		void end();
		void draw(Renderer2D* renderer);

		bool begin_window(const char* title, v2f default_position = v2f(30, 30), v2f default_size = v2f(200, 300));
		void end_window();

		void use_font(Font* font, v4f color = v4f(1.0f, 1.0f, 1.0f, 1.0f));

		void label(const char* text);
		void text(const char* fmt, ...);
		bool button(const char* text);

		void columns(usize count, f32 size);
		f32 max_column_width();

		/* Advances the cursor position to the correct
		 * place to draw the next element. `last_height'
		 * describes the height of the last element
		 * drawn before the call to `advance'. */
		void advance(f32 last_height);

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

		bool rect_hovered(v2f position, v2f dimentions);
	};
}
