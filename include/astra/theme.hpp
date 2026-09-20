#pragma once
#include <imgui.h>

namespace astra
{
	struct theme
	{
		ImVec4 background, panel, field, text, muted, accent;
		ImVec4 selection{0.16f, 0.82f, 0.59f, 1.f};
		        ImVec4 selection_text{0.055f, 0.066f, 0.082f, 1.f};
		float alpha = 1.f;
		float rounding = 10.f;
		ImVec2 spacing{12.f, 10.f};
	};
	theme preset_theme(int index);
	void set_srgb_output(bool enabled);
	ImU32 pack_color(ImVec4 color);
	class scoped_theme
	{
		ImGuiStyle saved_;

	public:
		explicit scoped_theme(const theme& value);
		~scoped_theme();
		scoped_theme(const scoped_theme&) = delete;
		scoped_theme& operator=(const scoped_theme&) = delete;
	};
}
