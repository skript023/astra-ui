#pragma once
#include "model.hpp"
#include "theme.hpp"
#include <array>
#include <vector>
#include <unordered_map>

namespace astra
{
	struct list_style
	{
		ImVec2 position{25, 25};
		float width = 450, row_height = 45;
		int rows = 11;
		ImTextureID banner{};
		float banner_height = 100;
        std::string footer = "Astra | Build 1.0.0";
        bool mouse_enabled = true;
	};
	// One view per menu instance. No game globals or graphics-device ownership.
	class menu
	{
		std::array<char, 128> search_{};
		std::string search_page_;
		std::unordered_map<std::string, std::string> window_sections_;
		theme displayed_theme_{};
		bool theme_initialized_ = false;
		std::vector<float> tab_widths_;
		float selected_row_y_ = 0.f;
		std::string list_page_;
		std::size_t list_start_ = 0;
		float list_wheel_ = 0.f, tab_wheel_ = 0.f;
		float list_scroll_position_ = 0.f, list_scroll_grab_ = 0.f;
		// Where the menu was on screen the last frame it drew, and its own window.
		ImVec2 area_min_{}, area_max_{};
		int area_frame_ = -1;
		ImGuiID area_window_ = 0;
		// Where keep_windows_out left each window last frame, to tell which side of the menu it came from.
		std::unordered_map<ImGuiID, ImVec2> kept_positions_;
		theme animate_theme(const theme& target);

	public:
		event draw_window(const char* id, const page& model, bool& open, const theme& colors);
		event draw_list(const page& model, const theme& colors, const list_style& style);
		void reset_theme();
		// Call right after ImGui::NewFrame, before any window is drawn. Keeps every window the player can drag out of
		// the area the menu covered last frame. A window dragged onto the menu stops at the edge it came from and
		// slides along it; a window found on the menu, such as one the menu opened over, moves to the nearest side that
		// stays on screen. Windows without a title bar or mouse input, popups, tooltips, and the Menu controls dialog
		// stay where they are. A menu that did not draw last frame is closed and keeps nothing out.
		void keep_windows_out();
	};
}
