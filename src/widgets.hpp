#pragma once
#include <astra/model.hpp>
#include <imgui.h>
namespace astra { std::function<void()> draw_control(const control& item); }
// Shared by the Window layout's controls and the public components in components.cpp.
namespace astra::detail
{
	void label_with_value(const char* label, const char* value_text);
	bool slider_track(double* value, double minimum, double maximum, const char* format);
	// Raises the current window, the menu, in front of the game's own windows, then raises any open popup in front
	// of the menu so a dropdown opened from the menu or a panel is never hidden behind it. Call right after Begin.
	void keep_in_front();
	// color, mixed toward text in steps until it reads at WCAG AA on background, such as a notification's kind.
	ImVec4 readable(ImVec4 color, ImVec4 text, ImVec4 background);
}
