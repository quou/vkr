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

	void UIContext::cmd_draw_text(const char* text, usize len, v2f position) {
		DrawTextCommand* cmd = reinterpret_cast<DrawTextCommand*>(command_buffer + command_buffer_idx);
		cmd->type = Command::Type::draw_text;
		cmd->size = sizeof(*cmd) + len + 1;
		cmd->position = position;
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

	UIContext::UIContext(App* app) : app(app) {
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
	}

	void UIContext::end() {

	}

	void UIContext::draw(Renderer2D* renderer) {
		Command* cmd = reinterpret_cast<Command*>(command_buffer);
		Command* end = reinterpret_cast<Command*>(command_buffer + command_buffer_idx);

		while (cmd != end) {
			switch (cmd->type) {
				case Command::Type::draw_rect: {
					auto draw_rect_cmd = static_cast<DrawRectCommand*>(cmd);

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

					renderer->push(bound_font, draw_text_cmd->text, draw_text_cmd->position, bound_font_color);
				} break;
				case Command::Type::set_clip: {
					auto set_clip_cmd = static_cast<SetClipCommand*>(cmd);

					renderer->set_clip(Rect {
						(i32)set_clip_cmd->position.x,   (i32)set_clip_cmd->position.y,
						(i32)set_clip_cmd->dimentions.x, (i32)set_clip_cmd->dimentions.y,
					});
				} break;
				default: break;
			}

			cmd = reinterpret_cast<Command*>((reinterpret_cast<u8*>(cmd)) + cmd->size);
		}
	}

	bool UIContext::begin_window(const char* title, v2f default_position, v2f default_size) {
		usize title_len = strlen(title);
		auto title_hash = elf_hash(reinterpret_cast<const u8*>(title), title_len);

		auto text_dimentions = bound_font->dimentions(title);
		auto padding = get_style_var(StyleVar::padding);

		if (meta.count(title_hash) == 0) {
			meta[title_hash] = {
				.position = default_position,
				.dimentions = default_size,
				.content_offset = v2f(padding, text_dimentions.y + padding),
				.max_content_dimentions = default_size - v2f(padding),
				.content_dimentions = v2f()
			};

			window = &meta[title_hash];
		}

		cursor_pos = window->position + window->content_offset;

		cmd_draw_rect(window->position - v2f(1.0f), window->dimentions + v2f(2.0f), get_style_color(StyleColor::border));
		cmd_draw_rect(window->position, window->dimentions, get_style_color(StyleColor::background));
		cmd_set_clip(window->position + v2f(padding), window->dimentions - v2f(padding) * 2.0f);
		cmd_draw_text(title, title_len,
			v2f(window->position.x + window->content_offset.x + (window->max_content_dimentions.x / 2.0f) - (text_dimentions.x / 2.0f),
			default_position.y + padding));

		return true;
	}

	void UIContext::end_window() {
		
	}

	void UIContext::use_font(Font* font, v4f color) {
		bound_font = font;
		bound_font_color = color;
		cmd_bind_font(font, color);
	}

	void UIContext::label(const char* text) {
		auto dim = bound_font->dimentions(text);

		cmd_draw_text(text, strlen(text), cursor_pos);

		cursor_pos.y += dim.y;
	}

	void UIContext::text(const char* fmt, ...) {
		char str[1024];

		va_list args;
		va_start(args, fmt);
		usize len = vsnprintf(str, 1024, fmt, args);
		va_end(args);

		auto dim = bound_font->dimentions(str);

		cmd_draw_text(str, len, cursor_pos);

		cursor_pos.y += dim.y;
	}

	bool UIContext::button(const char* text) {
		auto text_dim = bound_font->dimentions(text);

		auto padding = get_style_var(StyleVar::padding);

		v2f position = cursor_pos;
		v2f dimentions = text_dim + padding * 2.0f;

		auto hovered = rect_hovered(position, dimentions);
		auto hot = hovered && app->mouse_button_pressed(mouse_button_left);

		auto color = get_style_color(StyleColor::background2);

		if (hot) {
			color = get_style_color(StyleColor::hot);
		} else if (hovered) {
			color = get_style_color(StyleColor::hovered);
		}

		cmd_draw_rect(position, dimentions, color);
		cmd_draw_text(text, strlen(text), position + padding);

		cursor_pos.y += text_dim.y + padding * 3.0f;

		return hovered && app->mouse_button_just_released(mouse_button_left);
	}
}
