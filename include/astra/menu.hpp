#pragma once
#include "model.hpp"
#include "theme.hpp"
#include <array>
#include <vector>

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
	};
	// One view per menu instance. No game globals or graphics-device ownership.
	class menu
	{
		std::array<char, 128> search_{};
		std::string search_page_;
		theme displayed_theme_{};
		bool theme_initialized_ = false;
		std::vector<float> tab_widths_;
		std::size_t tab_start_ = 0;
		float selected_row_y_ = 0.f;
		theme animate_theme(const theme& target);

	public:
		event draw_window(const char* id, const page& model, bool& open, const theme& colors);
		event draw_list(const page& model, const theme& colors, const list_style& style);
		void reset_theme();
	};
}
