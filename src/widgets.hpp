#pragma once
#include <astra/model.hpp>
namespace astra { std::function<void()> draw_control(const control& item); }
// Shared by the Window layout's controls and the public components in components.cpp.
namespace astra::detail
{
	void label_with_value(const char* label, const char* value_text);
	bool slider_track(double* value, double minimum, double maximum, const char* format);
}
