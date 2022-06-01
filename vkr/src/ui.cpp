#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ui.hpp"
#include "vkr.hpp"

namespace vkr {
	bool UIContext::rect_hovered(v2f position, v2f dimentions) {
		v2f fmp(static_cast<f32>(app->mouse_pos.x), static_cast<f32>(app->mouse_pos.y));

		return fmp > position && fmp < position + dimentions;
	}

	void UIContext::cmd_draw_rect(v2f position, v2f dimentions, v4f color) {
		DrawRectCommand* cmd = reinterpret_cast<DrawRectCommand*>(command_buffer + command_buffer_idx);
		cmd->type = Command::Type::draw_rect;
		cmd->size = sizeof(*cmd);
		cmd->position = position;
		cmd->dimentions = dimentions;
		cmd->color = color;

		command_buffer_idx += cmd->size;
	}

	void UIContext::cmd_draw_text(const char* text, usize len, v2f position, v2f dimentions) {
		DrawTextCommand* cmd = reinterpret_cast<DrawTextCommand*>(command_buffer + command_buffer_idx);
		cmd->type = Command::Type::draw_text;
		cmd->size = sizeof(*cmd) + len + 1;
		cmd->position = position;
		cmd->dimentions = dimentions;
		cmd->text = reinterpret_cast<char*>(cmd + 1);

		memcpy(cmd->text, text, len + 1);

		command_buffer_idx += cmd->size;
	}

	void UIContext::cmd_bind_font(Font* font, v4f color) {
		BindFontCommand* cmd = reinterpret_cast<BindFontCommand*>(command_buffer + command_buffer_idx);
		cmd->type = Command::Type::bind_font;
		cmd->size = sizeof(*cmd);
		cmd->font = font;
		cmd->color = color;

		command_buffer_idx += cmd->size;
	}

	void UIContext::cmd_set_clip(v2f position, v2f dimentions) {
		SetClipCommand* cmd = reinterpret_cast<SetClipCommand*>(command_buffer + command_buffer_idx);
		cmd->type = Command::Type::set_clip;
		cmd->size = sizeof(*cmd);
		cmd->position = position;
		cmd->dimentions = dimentions;

		command_buffer_idx += cmd->size;
	}

	UIContext::BeginWindowCommand* UIContext::cmd_begin_window() {
		BeginWindowCommand* cmd = reinterpret_cast<BeginWindowCommand*>(command_buffer + command_buffer_idx);
		cmd->type = Command::Type::begin_window;
		cmd->size = sizeof(*cmd);
		cmd->beginning_idx = command_buffer_idx;
		cmd->end_idx = 0;

		command_buffer_idx += cmd->size;

		return cmd;
	}

	bool UIContext::rect_outside_clip(v2f position, v2f dimentions, Rect clip) {
		return
			static_cast<i32>(position.x) + static_cast<i32>(dimentions.x) > clip.x + clip.w ||
			static_cast<i32>(position.x)                                  < clip.x          ||
			static_cast<i32>(position.y) + static_cast<i32>(dimentions.y) > clip.y + clip.h ||
			static_cast<i32>(position.y)                                  < clip.y;
	}

	UIContext::UIContext(App* app) : app(app), dragging(0), anything_hovered(false), anything_hot(false),
		hot_item(0), hovered_item(0) {
		set_style_var(StyleVar::padding, 3.0f);

		set_style_color(StyleColor::background,  make_color(0x1a1a1a, 150));
		set_style_color(StyleColor::background2, make_color(0x2d2d2d, 255));
		set_style_color(StyleColor::hovered,     make_color(0x242533, 255));
		set_style_color(StyleColor::hot,         make_color(0x393d5b, 255));
		set_style_color(StyleColor::border,      make_color(0x0f0f0f, 200));
	}

	UIContext::~UIContext() {

	}

	void UIContext::begin(v2i screen_size) {
		this->screen_size = screen_size;
		command_buffer_idx = 0;
		current_item_height = 0.0f;
		item = 0;

		current_item_id = 1;

		hovered_item = 0;
	}

	void UIContext::end() {
		anything_hot = hot_item != 0;
		anything_hovered = hovered_item != 0;

		sorted_windows.clear();

		for (auto& m : meta) {
			m.second.id = m.first;
			sorted_windows.push_back(&m.second);
		}

		/* Bring the top-most clicked window to the top and initiate a drag. */
		if (app->mouse_button_just_pressed(mouse_button_left)) {
			std::sort(sorted_windows.begin(), sorted_windows.end(),
				[](WindowMeta* a, WindowMeta* b){
					return a->z < b->z;
				});

			for (auto win : sorted_windows) {
				if (rect_hovered(win->position, win->dimentions)) {
					win->z = 0.0f;
					top_window = win->id;

					if (!dragging && !anything_hovered) {
						dragging = win->id;
						drag_offset = v2f(app->mouse_pos.x, app->mouse_pos.y) - win->position;
					}

					for (auto& m : meta) { if (&m.second != win) { m.second.z += 1.0f; } }

					break;
				}
			}

			/* For bringing windows to the top, they must be in the reverse order
			 * that rendering requires, so we sort them front to back and then reverse
			 * them again once the pick is done. This is a bit bad, but it's fine
			 * because it only ever happens on a mouse click, not every frame. */
			std::reverse(sorted_windows.begin(), sorted_windows.end());
		} else {
			std::sort(sorted_windows.begin(), sorted_windows.end(),
				[](WindowMeta* a, WindowMeta* b) {
					return a->z > b->z;
				});
		}
	}

	void UIContext::draw(Renderer2D* renderer) {
		#define commit_clip(cmd_) \
					if (rect_outside_clip((cmd_)->position, (cmd_)->dimentions, current_clip)) { \
						renderer->set_clip(current_clip); \
					}

		for (const auto& win : sorted_windows) {
			Command* cmd = reinterpret_cast<Command*>(command_buffer + win->beginning->beginning_idx);
			Command* end = reinterpret_cast<Command*>(command_buffer + win->beginning->end_idx);

			Rect current_clip = {
				static_cast<i32>(win->position.x), static_cast<i32>(win->position.y),
				static_cast<i32>(win->dimentions.x), static_cast<i32>(win->dimentions.y)
			};
			renderer->set_clip(current_clip);

			while (cmd != end) {
				switch (cmd->type) {
					case Command::Type::draw_rect: {
						auto draw_rect_cmd = static_cast<DrawRectCommand*>(cmd);

						commit_clip(draw_rect_cmd);

						renderer->push(Renderer2D::Quad {
							.position = draw_rect_cmd->position,
							.dimentions = draw_rect_cmd->dimentions,
							.color = draw_rect_cmd->color
						});
					} break;
					case Command::Type::bind_font: {
						auto bind_font_cmd = static_cast<BindFontCommand*>(cmd);

						bound_font = bind_font_cmd->font;
						bound_font_color = bind_font_cmd->color;
					} break;
					case Command::Type::draw_text: {
						auto draw_text_cmd = static_cast<DrawTextCommand*>(cmd);

						commit_clip(draw_text_cmd);

						renderer->push(bound_font, draw_text_cmd->text, draw_text_cmd->position, bound_font_color);
					} break;
					case Command::Type::set_clip: {
						auto set_clip_cmd = static_cast<SetClipCommand*>(cmd);

						current_clip = Rect {
							(i32)set_clip_cmd->position.x,   (i32)set_clip_cmd->position.y,
							(i32)set_clip_cmd->dimentions.x, (i32)set_clip_cmd->dimentions.y,
						};
					} break;
					default: break;
				}

				cmd = reinterpret_cast<Command*>((reinterpret_cast<u8*>(cmd)) + cmd->size);
			}
		}

		#undef commit_clip
	}

	bool UIContext::begin_window(const char* title, v2f default_position, v2f default_size) {
		usize title_len = strlen(title);
		auto id = next_item_id();

		auto text_dimentions = bound_font->dimentions(title);
		auto padding = get_style_var(StyleVar::padding);

		if (meta.count(id) == 0) {
			meta[id] = {
				.position = default_position,
				.dimentions = default_size,
				.content_offset = v2f(padding, text_dimentions.y + padding),
				.max_content_dimentions = default_size - v2f(padding),
				.content_dimentions = v2f(),
				.z = 1.0f
			};
		}

		window = &meta[id];

		cursor_pos = window->position + window->content_offset;

		if (dragging == id) {
			window->position = v2f(app->mouse_pos.x, app->mouse_pos.y) - drag_offset;

			if (app->mouse_button_just_released(mouse_button_left)) {
				dragging = 0;
			}
		}

		columns(1, max_column_width());

		window->beginning = cmd_begin_window();
		cmd_draw_rect(window->position - v2f(1.0f), window->dimentions + v2f(2.0f), get_style_color(StyleColor::border));
		cmd_draw_rect(window->position, window->dimentions, get_style_color(StyleColor::background));
		cmd_set_clip(window->position + v2f(padding), window->dimentions - v2f(padding) * 2.0f);
		cmd_draw_text(title, title_len,
			v2f(window->position.x + window->content_offset.x + (window->max_content_dimentions.x / 2.0f) - (text_dimentions.x / 2.0f),
			window->position.y + padding), text_dimentions);

		return true;
	}

	void UIContext::end_window() {
		window->beginning->end_idx = command_buffer_idx;
		window = null;
	}

	u64 UIContext::next_item_id() {
		u64 id = current_item_id++;

		return elf_hash(reinterpret_cast<const u8*>(&id), sizeof(usize));
	}

	void UIContext::use_font(Font* font, v4f color) {
		bound_font = font;
		bound_font_color = color;
		cmd_bind_font(font, color);
	}

	void UIContext::label(const char* text) {
		auto id = next_item_id();

		auto dim = bound_font->dimentions(text);

		cmd_draw_text(text, strlen(text), cursor_pos, dim);

		advance(dim.y);
	}

	void UIContext::text(const char* fmt, ...) {
		auto id = next_item_id();

		char str[1024];

		va_list args;
		va_start(args, fmt);
		usize len = vsnprintf(str, 1024, fmt, args);
		va_end(args);

		auto dim = bound_font->dimentions(str);

		cmd_draw_text(str, len, cursor_pos, dim);

		advance(dim.y);
	}

	bool UIContext::button(const char* text) {
		auto id = next_item_id();

		auto text_dim = bound_font->dimentions(text);

		auto padding = get_style_var(StyleVar::padding);

		v2f position = cursor_pos;
		v2f dimentions = text_dim + padding * 2.0f;

		auto hovered = window->id == top_window && rect_hovered(position, dimentions);
		if (hovered) {
			hovered_item = id;

			if (app->mouse_button_just_pressed(mouse_button_left)) {
				hot_item = id;
			}
		}

		bool clicked = false;

		if (hot_item == id && app->mouse_button_just_released(mouse_button_left)) {
			if (hovered) {
				clicked = true;
			}

			hot_item = 0;
		}

		bool hot = window->id == top_window && hot_item == id;

		auto color = get_style_color(StyleColor::background2);

		if (hot) {
			color = get_style_color(StyleColor::hot);
		} else if (hovered) {
			color = get_style_color(StyleColor::hovered);
			anything_hovered = true;
		}

		cmd_draw_rect(position, dimentions, color);
		cmd_draw_text(text, strlen(text), position + padding, text_dim);

		advance(text_dim.y + padding * 3.0f);

		return clicked && window->id == top_window;
	}

	void UIContext::columns(usize count, f32 size) {
		column_count = count;
		column_size = size;
	}

	f32 UIContext::max_column_width() {
		return window->max_content_dimentions.x;
	}

	void UIContext::advance(f32 last_height) {
		item++;

		if (last_height > current_item_height) {
			current_item_height = last_height;
		}

		if (item >= column_count) {
			cursor_pos.x = window->position.x + window->content_offset.x;
			cursor_pos.y += current_item_height;
			current_item_height = 0;
			item = 0;
		} else {
			cursor_pos.x += column_size;
		}
	}
}
