#include "ui.hpp"

namespace vkr {
	UIContext::UIContext() {

	}

	UIContext::~UIContext() {

	}

	void UIContext::begin(v2i screen_size) {
		this->screen_size = screen_size;
	}

	void UIContext::end() {

	}

	void UIContext::draw(Renderer2D* renderer) {

	}

	bool UIContext::begin_window(const char* title, v2i default_size) {
		return false;
	}

	void UIContext::end_window() {

	}

	void UIContext::use_font(Font* font, v4f color) {

	}

	void UIContext::label(const char* text) {

	}
}
