#include <string.h>

#include "ui.hpp"
#include "vkr.hpp"

namespace vkr {
	void UIContext::cmd_draw_rect(v2f position, v2f dimentions, v4f color) {
		DrawRectCommand* cmd = reinterpret_cast<DrawRectCommand*>(command_buffer + command_buffer_idx);
		cmd->type = Command::Type::draw_rect;
		cmd->size = sizeof(*cmd);
		cmd->position = position;
		cmd->dimentions = dimentions;
		cmd->color = color;

		command_buffer_idx += cmd->size;
	}

	void UIContext::cmd_draw_text(const char* text, v2f position) {
		usize len = strlen(text);

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

	UIContext::UIContext() {
		set_style_var(StyleVar::padding, 3.0f);

		set_style_color(StyleColor::background, v4f(0.1949f, 0.1949f, 0.2184f, 1.0f));
		set_style_color(StyleColor::border,     v4f(0.0100f, 0.0100f, 0.0100f, 1.0f));
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
				default: break;
			}

			cmd = reinterpret_cast<Command*>((reinterpret_cast<u8*>(cmd)) + cmd->size);
		}
	}

	bool UIContext::begin_window(const char* title, v2f default_position, v2f default_size) {
		auto text_dimentions = bound_font->dimentions(title);

		auto padding = get_style_var(StyleVar::padding);

		window.position = default_position;
		window.content_offset = v2f(padding, text_dimentions.y + padding);
		window.max_content_dimentions = default_size - v2f(padding);
		window.content_dimentions = v2f();

		cursor_pos = window.position + window.content_offset;

		cmd_draw_rect(window.position, default_size, get_style_color(StyleColor::background));
		cmd_draw_rect(window.position - v2f(1.0f), default_size + v2f(2.0f), get_style_color(StyleColor::background));
		cmd_draw_text(title,
			v2f(window.position.x + window.content_offset.x + (window.max_content_dimentions.x / 2.0f) - (text_dimentions.x / 2.0f),
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

		cmd_draw_text(text, cursor_pos);

		cursor_pos.y += dim.y;
	}
}
