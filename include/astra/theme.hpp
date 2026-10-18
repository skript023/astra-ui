#pragma once
#include <imgui.h>
#include <array>

namespace astra
{
	// Names for preset_theme indexes. Hosts save the index, so a new preset goes at the end.
	inline constexpr std::array<const char*, 6> preset_theme_names{"Emerald", "Violet", "Ocean", "Custom", "Studio", "Snow"};
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
	// The theme for a game's own panels, overlays, and notifications: the same palette with the List layout's
	// square corners on windows and controls alike.
	theme panel_theme(theme value);
	void set_srgb_output(bool enabled);
	ImU32 pack_color(ImVec4 color);
	// Sets ImGui's style to the theme until something changes it again; scoped_theme restores the previous style instead.
	void apply_theme(const theme& value);
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
