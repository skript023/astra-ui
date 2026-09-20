#include <astra/menu.hpp>
#include "widgets.hpp"
#include <cctype>

namespace astra
{
	static bool matches(const std::string& text, const char* query)
	{
		auto lower = [](std::string value) {
			for (auto& c : value)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return value;
		};
		return lower(text).find(lower(query)) != std::string::npos;
	}
	static ImU32 ink(ImVec4 c, float alpha = 1.f)
	{
		c.w *= alpha;
		return pack_color(c);
	}
	static void category_icon(ImDrawList* draw, ImVec2 p, std::size_t index, ImU32 color)
	{
		if (index == 0)
		{
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					draw->AddRect({p.x + x * 8, p.y + y * 8}, {p.x + x * 8 + 5, p.y + y * 8 + 5}, color, 1.5f, 0, 1.4f);
		}
		else if (index == 1)
		{
			draw->AddCircle({p.x + 7, p.y + 7}, 7, color, 20, 1.4f);
			draw->AddTriangle({p.x + 10, p.y + 3}, {p.x + 8, p.y + 9}, {p.x + 3, p.y + 11}, color, 1.3f);
		}
		else
		{
			for (int y = 0; y < 3; ++y)
			{
				draw->AddLine({p.x, p.y + y * 6}, {p.x + 15, p.y + y * 6}, color, 1.4f);
				draw->AddCircleFilled({p.x + (y == 1 ? 10.f : 5.f), p.y + y * 6}, 2.5f, color);
			}
		}
	}
	event menu::draw_window(const char* id, const page& model, bool& open, const theme& target)
	{
		const auto colors = animate_theme(target);
		scoped_theme style(colors);
		event result;
		std::function<void()> pending;
		if (search_page_ != model.id)
		{
			search_.fill(0);
			search_page_ = model.id;
		}
		ImGui::SetNextWindowSize({1000, 680}, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints({760, 520}, {FLT_MAX, FLT_MAX});
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
		const bool visible = ImGui::Begin(id, &open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleVar();
		if (visible)
		{
			const auto origin = ImGui::GetWindowPos();
			const auto size = ImGui::GetWindowSize();
			auto* draw = ImGui::GetWindowDrawList();
			const float sidebar_width = 212;
			const auto edge = ink(colors.muted, 0.15f);
			draw->AddRectFilled(origin, {origin.x + size.x, origin.y + 72}, ink(colors.panel), colors.rounding, ImDrawFlags_RoundCornersTop);
			draw->AddRectFilled({origin.x, origin.y + 72}, {origin.x + sidebar_width, origin.y + size.y}, ink(colors.panel), colors.rounding, ImDrawFlags_RoundCornersBottomLeft);
			draw->AddLine({origin.x, origin.y + 72}, {origin.x + size.x, origin.y + 72}, edge);
			draw->AddLine({origin.x + sidebar_width, origin.y + 72}, {origin.x + sidebar_width, origin.y + size.y}, edge);
			// Explicit header drag area keeps moving independent of host ImGui settings.
			ImGui::SetCursorPos({0, 0});
			ImGui::InvisibleButton("##drag", {size.x - 70, 72});
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
				ImGui::SetWindowPos({origin.x + ImGui::GetIO().MouseDelta.x, origin.y + ImGui::GetIO().MouseDelta.y});
			draw->AddRectFilled({origin.x + 24, origin.y + 20}, {origin.x + 56, origin.y + 52}, ink(colors.accent), 9);
			draw->AddText(ImGui::GetFont(), 21, {origin.x + 32, origin.y + 23}, IM_COL32_WHITE, "Q");
			draw->AddText(ImGui::GetFont(), 23, {origin.x + 69, origin.y + 20}, ink(colors.text), "Astra");
			draw->AddText(ImGui::GetFont(), 12, {origin.x + 71, origin.y + 45}, ink(colors.muted), "YOUR WORKSPACE");
			ImGui::SetCursorPos({size.x - 49, 22});
			if (ImGui::InvisibleButton("##close", {28, 28}))
				open = false;
			const auto close = ImGui::GetItemRectMin();
			if (ImGui::IsItemHovered())
			{
				draw->AddCircleFilled({close.x + 14, close.y + 14}, 14, ink(colors.field));
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			}
			draw->AddLine({close.x + 10, close.y + 10}, {close.x + 18, close.y + 18}, ink(colors.muted), 1.5f);
			draw->AddLine({close.x + 18, close.y + 10}, {close.x + 10, close.y + 18}, ink(colors.muted), 1.5f);

			ImGui::SetCursorPos({16, 98});
			if (ImGui::BeginChild("##sidebar", {sidebar_width - 32, size.y - 120}, 0, ImGuiWindowFlags_NoBackground))
			{
				ImGui::TextDisabled("WORKSPACE");
				ImGui::SetCursorPosY(36);
				for (std::size_t i = 0; i < model.tabs.size(); ++i)
				{
					ImGui::PushID(static_cast<int>(i));
					const auto p = ImGui::GetCursorScreenPos();
					const float width = ImGui::GetContentRegionAvail().x;
					const bool selected = i == model.selected_tab;
					ImGui::PushStyleColor(ImGuiCol_Header, {0, 0, 0, 0});
					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0, 0, 0, 0});
					ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0, 0, 0, 0});
					if (ImGui::Selectable("##tab", selected, 0, {width, 44}))
						result = {event_kind::tab, i};
					ImGui::PopStyleColor(3);
					auto* nav = ImGui::GetWindowDrawList();
					if (selected || ImGui::IsItemHovered())
						nav->AddRectFilled(p, {p.x + width, p.y + 44}, selected ? ink(colors.accent, 0.10f) : ink(colors.field), 8);
					category_icon(nav, {p.x + 14, p.y + 15}, i, ink(selected ? colors.selection : colors.muted));
					nav->PushClipRect({p.x + 38, p.y}, {p.x + width - 8, p.y + 44}, true);
					nav->AddText({p.x + 42, p.y + (44 - ImGui::GetFontSize()) / 2}, ink(selected ? colors.selection_text : colors.text), model.tabs[i].c_str());
					nav->PopClipRect();
					if (ImGui::IsItemHovered())
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					ImGui::PopID();
				}
				ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY() + 24, ImGui::GetWindowHeight() - 70));
				ImGui::Separator();
				ImGui::Spacing();
				ImGui::TextDisabled("Make it yours.");
				ImGui::PushTextWrapPos(0);
				ImGui::TextDisabled("Explore themes in Settings.");
				ImGui::PopTextWrapPos();
			}
			ImGui::EndChild();

			ImGui::SetCursorPos({sidebar_width + 28, 94});
			if (ImGui::BeginChild("##content", {size.x - sidebar_width - 56, size.y - 116}, 0, ImGuiWindowFlags_NoBackground))
			{
				ImGui::BeginDisabled(model.breadcrumbs.size() <= 1);
				if (ImGui::SmallButton("< Back"))
					result = {event_kind::back};
				ImGui::EndDisabled();
				for (std::size_t i = 0; i < model.breadcrumbs.size(); ++i)
				{
					if (ImGui::GetContentRegionAvail().x > ImGui::CalcTextSize(model.breadcrumbs[i].c_str()).x + 40)
						ImGui::SameLine();
					ImGui::PushID(static_cast<int>(i));
					ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
					if (ImGui::SmallButton(model.breadcrumbs[i].c_str()))
						result = {event_kind::breadcrumb, i};
					ImGui::PopStyleColor();
					ImGui::PopID();
				}
				ImGui::Dummy({0, 10});
				const auto heading = ImGui::GetCursorScreenPos();
				auto* content = ImGui::GetWindowDrawList();
				content->AddText(ImGui::GetFont(), 29, heading, ink(colors.text), model.title.c_str());
				ImGui::Dummy({0, 37});
				ImGui::TextDisabled("Your settings, just the way you like them.");
				ImGui::Dummy({0, 8});
				ImGui::SetNextItemWidth(-1);
				ImGui::InputTextWithHint("##search", "Search this page...", search_.data(), search_.size());
				ImGui::Dummy({0, 10});
				                bool has_submenus = false;
                for (const auto& c : model.controls)
                    has_submenus |= c.kind == control_kind::submenu;
                if (has_submenus)
                {
                    ImGui::BeginGroup();
                    for (std::size_t i = 0; i < model.controls.size(); ++i)
                    {
                        const auto& c = model.controls[i];
                        if (c.kind != control_kind::submenu)
                            continue;
                        ImGui::PushID(c.id.c_str());
                        if (i != 0)
                            ImGui::SameLine(0, 8);
                        if (ImGui::Button(c.label.c_str(), {ImGui::CalcTextSize(c.label.c_str()).x + 34.f, 34.f}))
                        {
                            if (c.activate && result.kind == event_kind::none)
                                pending = c.activate;
                            result = {event_kind::option, i};
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndGroup();
                    ImGui::Dummy({0, 8});
                }
				ImGui::PushID(model.id.c_str());
				if (ImGui::BeginChild("##options", {0, 0}, 0, ImGuiWindowFlags_NoBackground))
				{
					const int columns = ImGui::GetContentRegionAvail().x >= 640 ? 2 : 1;
					std::size_t shown = 0;
					if (ImGui::BeginTable("##cards", columns, ImGuiTableFlags_SizingStretchSame))
					{
						for (std::size_t i = 0; i < model.controls.size(); ++i)
						{
							const auto& c = model.controls[i];
							if (c.kind == control_kind::submenu)
							    continue;
							if (!matches(c.label + " " + c.description, search_.data()))
								continue;
							++shown;
							ImGui::TableNextColumn();
							ImGui::PushID(c.id.c_str());
							ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {18, 16});
							if (ImGui::BeginChild("##card", {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding))
							{
								auto action = draw_control(c);
								if (action && result.kind == event_kind::none)
								{
									pending = std::move(action);
									result = {event_kind::option, i};
								}
							}
							ImGui::EndChild();
                            if (ImGui::IsItemHovered() && !c.description.empty())
                                ImGui::SetTooltip("%s", c.description.c_str());
							ImGui::PopStyleVar();
							ImGui::Dummy({0, 4});
							ImGui::PopID();
						}
						ImGui::EndTable();
					}
					if (!shown && !has_submenus)
						ImGui::TextDisabled(model.controls.empty() ? "No options on this page." : "No matching options.");
				}
				ImGui::EndChild();
				ImGui::PopID();
			}
			ImGui::EndChild();
		}
		ImGui::End();
		if (pending && open && result.kind == event_kind::option)
			pending();
		return result;
	}
}
