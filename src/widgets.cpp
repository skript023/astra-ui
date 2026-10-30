#include "widgets.hpp"
#include <astra/components.hpp>
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <vector>

namespace astra
{
	static bool toggle(const control& c)
	{
		bool value = c.checked;
		return checkbox(c.label.c_str(), &value);
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
				detail::label_with_value(c.label.c_str(), value_text);
			double value = c.value;
			const double minimum = std::min(c.minimum, c.maximum), maximum = std::max(c.minimum, c.maximum);
			if (detail::slider_track(&value, minimum, maximum, format) && c.set_value)
			{
				value = bounded_value(value, minimum, maximum, c.integral);
				action = [fn = c.set_value, value] {
					fn(value);
				};
			}
			break;
		}
		case control_kind::choice:
		{
			std::vector<const char*> names;
			for (const auto& name : c.choices)
				names.push_back(name.c_str());
			int picked = c.choice;
			if (choice(c.label.c_str(), &picked, names) && c.set_choice)
				action = [fn = c.set_choice, picked] {
					fn(picked);
				};
			if (c.activate && button("Apply"))
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
