#include <astra/menu.hpp>
#include <cmath>
#include <algorithm>

namespace astra
{
	namespace
	{
		bool srgb_output = false;
		ImVec4 linearize(ImVec4 color)
		{
			if (!srgb_output)
				return color;
			color.x = std::pow(std::clamp(color.x, 0.f, 1.f), 2.2f);
			color.y = std::pow(std::clamp(color.y, 0.f, 1.f), 2.2f);
			color.z = std::pow(std::clamp(color.z, 0.f, 1.f), 2.2f);
			return color;
		}
	}
	void set_srgb_output(bool enabled)
	{
		srgb_output = enabled;
	}
	ImU32 pack_color(ImVec4 color)
	{
		return ImGui::ColorConvertFloat4ToU32(linearize(color));
	}
	theme preset_theme(int index)
	{
		theme t{{0.055f, 0.066f, 0.082f, 1}, {0.08f, 0.095f, 0.12f, 1}, {0.115f, 0.135f, 0.165f, 1}, {0.91f, 0.94f, 0.97f, 1}, {0.52f, 0.58f, 0.66f, 1}, {0.16f, 0.82f, 0.59f, 1}};
		        t.selection_text = t.background;
		        if (index == 1)
		    t.accent = {0.65f, 0.47f, 0.98f, 1};
		if (index == 2)
			t.accent = {0.22f, 0.65f, 0.98f, 1};
        t.selection = t.accent;
        if (index == 4)
		{
			t.background = {0.966f, 0.970f, 0.980f, 1};
			t.panel = {1, 1, 1, 1};
			t.field = {0.93f, 0.937f, 0.955f, 1};
			t.text = {0.12f, 0.12f, 0.18f, 1};
			t.muted = {0.46f, 0.48f, 0.55f, 1};
			t.accent = {0.259f, 0.165f, 0.835f, 1};
			            t.selection = {1.f, 1.f, 1.f, 1.f};
			            t.selection_text = t.text;
			t.rounding = 16;
		}
		if (index == 5)
		{
			t.background = {0.035f, 0.05f, 0.08f, 1};
			t.panel = {0.06f, 0.085f, 0.13f, 1};
			t.field = {0.10f, 0.14f, 0.20f, 1};
			t.text = {0.86f, 0.92f, 1.f, 1};
			t.muted = {0.48f, 0.60f, 0.75f, 1};
			t.accent = {0.35f, 0.70f, 1.f, 1};
			t.selection = {1.f, 1.f, 1.f, 1.f};
			t.selection_text = {0.03f, 0.05f, 0.08f, 1.f};
		}
		return t;
	}
	scoped_theme::scoped_theme(const theme& t) :
	    saved_(ImGui::GetStyle())
	{
		auto& s = ImGui::GetStyle();
		s.WindowPadding = {20, 20};
		s.CellPadding = {6, 6};
		s.FramePadding = {12, 8};
		s.ItemSpacing = t.spacing;
		s.WindowRounding = t.rounding;
		s.ChildRounding = t.rounding;
		s.FrameRounding = 6;
		s.GrabRounding = 6;
		s.ScrollbarRounding = 8;
		s.ScrollbarSize = 7;
		s.FrameBorderSize = 0;
		s.ChildBorderSize = 1;
		s.WindowBorderSize = 1;
		auto& c = s.Colors;
		c[ImGuiCol_WindowBg] = t.background;
		c[ImGuiCol_ChildBg] = t.panel;
		c[ImGuiCol_PopupBg] = t.panel;
		c[ImGuiCol_Text] = t.text;
		c[ImGuiCol_TextDisabled] = t.muted;
		c[ImGuiCol_Border] = {t.muted.x, t.muted.y, t.muted.z, 0.16f};
		c[ImGuiCol_FrameBg] = t.field;
		c[ImGuiCol_FrameBgHovered] = {t.accent.x, t.accent.y, t.accent.z, 0.20f};
		c[ImGuiCol_FrameBgActive] = {t.accent.x, t.accent.y, t.accent.z, 0.30f};
		c[ImGuiCol_Button] = t.field;
		c[ImGuiCol_ButtonHovered] = c[ImGuiCol_FrameBgHovered];
		c[ImGuiCol_ButtonActive] = c[ImGuiCol_FrameBgActive];
		c[ImGuiCol_Header] = c[ImGuiCol_FrameBgActive];
		c[ImGuiCol_HeaderHovered] = c[ImGuiCol_FrameBgHovered];
		c[ImGuiCol_HeaderActive] = c[ImGuiCol_FrameBgActive];
		c[ImGuiCol_CheckMark] = t.accent;
		c[ImGuiCol_SliderGrab] = t.accent;
		c[ImGuiCol_SliderGrabActive] = t.accent;
		c[ImGuiCol_TitleBg] = t.panel;
		c[ImGuiCol_TitleBgActive] = t.panel;
		c[ImGuiCol_Separator] = c[ImGuiCol_Border];
		c[ImGuiCol_ResizeGrip] = c[ImGuiCol_FrameBgHovered];
		c[ImGuiCol_ResizeGripHovered] = t.accent;
		c[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
		c[ImGuiCol_ScrollbarGrab] = {t.muted.x, t.muted.y, t.muted.z, 0.25f};
		c[ImGuiCol_ScrollbarGrabHovered] = {t.muted.x, t.muted.y, t.muted.z, 0.4f};
		c[ImGuiCol_ScrollbarGrabActive] = t.accent;
		if (srgb_output)
			for (auto& color : c)
				color = linearize(color);
	}
	scoped_theme::~scoped_theme()
	{
		ImGui::GetStyle() = saved_;
	}
}

namespace astra
{
	theme menu::animate_theme(const theme& target)
	{
		if (!theme_initialized_)
		{
			displayed_theme_ = target;
			theme_initialized_ = true;
		}
		const float t = 1.f - std::exp(-12.f * ImGui::GetIO().DeltaTime);
		auto blend = [t](ImVec4 a, ImVec4 b) {
			return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
		};
		displayed_theme_.background = blend(displayed_theme_.background, target.background);
		displayed_theme_.panel = blend(displayed_theme_.panel, target.panel);
		displayed_theme_.field = blend(displayed_theme_.field, target.field);
		displayed_theme_.text = blend(displayed_theme_.text, target.text);
		displayed_theme_.muted = blend(displayed_theme_.muted, target.muted);
		displayed_theme_.accent = blend(displayed_theme_.accent, target.accent);
		// The List highlights rows and tabs with selection/selection_text, so a
		// theme change that skips them leaves the old highlight colour on screen.
		displayed_theme_.selection = blend(displayed_theme_.selection, target.selection);
		displayed_theme_.selection_text = blend(displayed_theme_.selection_text, target.selection_text);
		displayed_theme_.alpha = target.alpha;
		displayed_theme_.rounding = target.rounding;
		displayed_theme_.spacing = target.spacing;
		return displayed_theme_;
	}
}
