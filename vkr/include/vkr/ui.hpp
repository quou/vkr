#pragma once

#include <map>
#include <functional>

#include "common.hpp"
#include "maths.hpp"
#include "renderer.hpp"

namespace vkr {
	/* Simple immediate mode GUI, for basic tools and
	 * for displaying debug information. */
	class UIContext {
	public:
		enum class StyleVar : u32 {
			padding = 0,
			border_width,
			count
		};

		enum class StyleColor : u32 {
			background = 0,
			background2,
			background3,
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
				draw_rect = 0,
				draw_text,
				bind_font,
				set_clip,
				begin_window
			} type;

			usize size;
		};

		struct DrawTextCommand : public Command {
			v2f position, dimentions;
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

		struct BeginWindowCommand : public Command {
			usize beginning_idx, end_idx;
		};

		void cmd_draw_rect(v2f position, v2f dimentions, v4f color);
		void cmd_draw_text(const char* text, usize len, v2f position, v2f dimentions);
		void cmd_bind_font(Font* font, v4f color);
		void cmd_set_clip(v2f position, v2f dimentions);
		BeginWindowCommand* cmd_begin_window();

		bool rect_outside_clip(v2f position, v2f dimentions, Rect rect);

		/* Fixed-size command buffer; allows one megabyte of commands.
		 *
		 * This GUI works by pushing draw commands to the command
		 * buffer, which is then iterated in the `draw' function. This
		 * means that the `begin'/`end' functions don't necessarily
		 * have to be within a framebuffer `begin'/`end', allowing for
		 * more flexibility in the way that the GUI code is written.
		 *
		 * The `sorted_command_buffer' contains the commands for
		 * windows' elements in a back-to-front order, updated in the
		 * draw function. */
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

			f32 z;

			v2f border_positions[4];
			v2f border_dimentions[4];

			u64 id;
			bool is_top;
			BeginWindowCommand* beginning;
		}* window;

		std::vector<WindowMeta*> sorted_windows;
		std::vector<f32> column_widths;

		std::unordered_map<u64, WindowMeta> meta;

		Font* bound_font;
		v4f bound_font_color;

		bool anything_hovered;
		bool anything_hot;

		u64 hot_item;
		u64 hovered_item;

		u64 dragging;
		v2f drag_offset;

		usize column_count;

		usize column;
		f32 current_item_height;

		u64 current_item_id;

		App* app;
	public:
		VKR_API UIContext(App* app);
		VKR_API ~UIContext();

		VKR_API void begin(v2i screen_size);
		VKR_API void end();
		VKR_API void draw(Renderer2D* renderer);

		VKR_API bool begin_window(const char* title, v2f default_position = v2f(30, 30), v2f default_size = v2f(500, 300));
		VKR_API void end_window();

		VKR_API void use_font(Font* font, v4f color = v4f(1.0f, 1.0f, 1.0f, 1.0f));

		VKR_API void label(const char* text);
		VKR_API void text(const char* fmt, ...);
		VKR_API bool button(const char* text);
		VKR_API void linebreak();
		VKR_API void slider(f32* val, f32 min = 0.0, f32 max = 1.0);

		VKR_API u64 next_item_id();

		VKR_API void columns(usize count, ...);

		/* Advances the cursor position to the correct
		 * place to draw the next element. `last_height'
		 * describes the height of the last element
		 * drawn before the call to `advance'. */
		VKR_API void advance(f32 last_height);

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

		VKR_API bool rect_hovered(v2f position, v2f dimentions);
		VKR_API bool rect_overlap(v2f ap, v2f ad, v2f bp, v2f bd);

		VKR_API bool any_windows_hovered();
	};
}
