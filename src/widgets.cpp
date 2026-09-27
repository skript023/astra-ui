#include "widgets.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <cmath>

namespace astra
{
	static bool toggle(const control& c)
	{
		bool value = c.checked;
		return ImGui::Checkbox(c.label.c_str(), &value);
	}
	std::function<void()> draw_control(const control& c)
	{
		std::function<void()> action;
		switch (c.kind)
		{
		case control_kind::toggle:
		case control_kind::toggle_number:
		{
			if (toggle(c))
				action = c.activate;
			if (c.kind == control_kind::toggle)
				break;
			[[fallthrough]];
		}
		case control_kind::number:
		{
			char format[16], value_text[64];
			std::snprintf(format, sizeof(format), "%%.%df", c.integral ? 0 : std::clamp(c.precision, 0, 6));
			std::snprintf(value_text, sizeof(value_text), format, c.value);
			if (c.kind == control_kind::number)
			{
				ImGui::TextUnformatted(c.label.c_str());
				const float value_width = ImGui::CalcTextSize(value_text).x;
				const float label_width = ImGui::CalcTextSize(c.label.c_str()).x;
				if (ImGui::GetContentRegionAvail().x > label_width + value_width + 12)
				{
					ImGui::SameLine(0, ImGui::GetContentRegionAvail().x - label_width - value_width - 4);
					ImGui::TextDisabled("%s", value_text);
				}
			}
			double value = c.value;
			const double minimum = std::min(c.minimum, c.maximum), maximum = std::max(c.minimum, c.maximum);
			const auto id = ImGui::GetID("##value");
			const bool editing = ImGui::TempInputIsActive(id);
			if (!editing)
			{
				ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, {0, 0, 0, 0});
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, {0, 0, 0, 0});
				ImGui::PushStyleColor(ImGuiCol_SliderGrab, {0, 0, 0, 0});
				ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, {0, 0, 0, 0});
				ImGui::PushStyleColor(ImGuiCol_Text, {0, 0, 0, 0});
			}
			ImGui::SetNextItemWidth(-1);
			if (ImGui::SliderScalar("##value", ImGuiDataType_Double, &value, &minimum, &maximum, format, ImGuiSliderFlags_AlwaysClamp) && c.set_value)
			{
				value = bounded_value(value, minimum, maximum, c.integral);
				action = [fn = c.set_value, value] {
					fn(value);
				};
			}
			if (!editing)
				ImGui::PopStyleColor(6);
			if (!ImGui::TempInputIsActive(id))
			{
				const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
				const float y = (a.y + b.y) / 2, left = a.x + 8, right = b.x - 8;
				const float ratio = maximum > minimum ? static_cast<float>(std::clamp((value - minimum) / (maximum - minimum), 0.0, 1.0)) : 0;
				const float x = left + (right - left) * ratio;
				auto* draw = ImGui::GetWindowDrawList();
				draw->AddRectFilled({left, y - 3}, {right, y + 3}, ImGui::GetColorU32(ImGuiCol_FrameBg), 3);
				draw->AddRectFilled({left, y - 3}, {x, y + 3}, ImGui::GetColorU32(ImGuiCol_CheckMark), 3);
				draw->AddCircleFilled({x, y}, 7, ImGui::GetColorU32(ImGuiCol_CheckMark), 24);
				draw->AddCircleFilled({x, y}, 3.5f, IM_COL32_WHITE, 24);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Drag to adjust. Ctrl+click to type a value.");
			break;
		}
		case control_kind::choice:
		{
			ImGui::TextUnformatted(c.label.c_str());
			const bool valid = c.choice >= 0 && c.choice < static_cast<int>(c.choices.size());
			ImGui::SetNextItemWidth(-1);
			if (ImGui::BeginCombo("##choice", valid ? c.choices[c.choice].c_str() : "No choices"))
			{
				for (int i = 0; i < static_cast<int>(c.choices.size()); ++i)
				{
					ImGui::PushID(i);
					if (ImGui::Selectable(c.choices[i].c_str(), c.choice == i) && c.set_choice)
						action = [fn = c.set_choice, i] {
							fn(i);
						};
					if (c.choice == i)
						ImGui::SetItemDefaultFocus();
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			if (c.activate && ImGui::Button("Apply"))
				action = c.activate;
			break;
		}
		case control_kind::submenu:
        {
            const std::string label = c.label + "  >";
            if (ImGui::Button(label.c_str(), {-1, 42}))
                action = c.activate;
            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            break;
        }
        default:
            ImGui::TextUnformatted(c.label.c_str());
            if (ImGui::Button("Run action", {-1, 34}))
                action = c.activate;
            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            break;
		}
		if (c.draw_details)
			c.draw_details();
		return action;
	}
}
