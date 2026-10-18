#include <astra/components.hpp>
#include "widgets.hpp"
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <string>
#include <vector>

namespace astra
{
	namespace
	{
		// The suffix ends up inside a printf format, so a literal % is doubled.
		std::string slider_format(std::string number, const char* suffix)
		{
			for (const char* c = suffix ? suffix : ""; *c; ++c)
				number += *c == '%' ? std::string("%%") : std::string(1, *c);
			return number;
		}

		template <typename T>
		bool framed_slider(const char* label, ImGuiDataType type, T* value, T minimum, T maximum, const std::string& format)
		{
			const T low = std::min(minimum, maximum), high = std::max(minimum, maximum);
			return ImGui::SliderScalar(label, type, value, &low, &high, format.c_str(), ImGuiSliderFlags_AlwaysClamp);
		}

		std::string decimal_format(int precision)
		{
			return "%." + std::to_string(std::clamp(precision, 0, 6)) + "f";
		}

		// Lets ImGui grow a std::string while the player types, the way imgui_stdlib does.
		int resize_string(ImGuiInputTextCallbackData* data)
		{
			if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
			{
				auto* text = static_cast<std::string*>(data->UserData);
				text->resize(data->BufTextLen);
				data->Buf = text->data();
			}
			return 0;
		}
	}

	namespace detail
	{
		void label_with_value(const char* label, const char* value_text)
		{
			const char* label_end = ImGui::FindRenderedTextEnd(label);
			ImGui::TextUnformatted(label, label_end);
			const float value_width = ImGui::CalcTextSize(value_text).x;
			const float label_width = ImGui::CalcTextSize(label, label_end).x;
			if (ImGui::GetContentRegionAvail().x > label_width + value_width + 12)
			{
				ImGui::SameLine(0, ImGui::GetContentRegionAvail().x - label_width - value_width - 4);
				ImGui::TextDisabled("%s", value_text);
			}
		}

		bool slider_track(double* value, double minimum, double maximum, const char* format)
		{
			const auto id      = ImGui::GetID("##value");
			const bool editing = ImGui::TempInputIsActive(id);
			if (!editing)
			{
				// ImGui's own frame, grab, and text stay invisible; the track is drawn over them below.
				ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, {0, 0, 0, 0});
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, {0, 0, 0, 0});
				ImGui::PushStyleColor(ImGuiCol_SliderGrab, {0, 0, 0, 0});
				ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, {0, 0, 0, 0});
				ImGui::PushStyleColor(ImGuiCol_Text, {0, 0, 0, 0});
			}
			ImGui::SetNextItemWidth(-1);
			const bool changed = ImGui::SliderScalar("##value", ImGuiDataType_Double, value, &minimum, &maximum, format, ImGuiSliderFlags_AlwaysClamp);
			if (!editing)
				ImGui::PopStyleColor(6);
			if (!ImGui::TempInputIsActive(id))
			{
				const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
				const float y = (a.y + b.y) / 2, left = a.x + 8, right = b.x - 8;
				const float ratio = maximum > minimum ? static_cast<float>(std::clamp((*value - minimum) / (maximum - minimum), 0.0, 1.0)) : 0;
				const float x = left + (right - left) * ratio;
				auto* draw = ImGui::GetWindowDrawList();
				draw->AddRectFilled({left, y - 3}, {right, y + 3}, ImGui::GetColorU32(ImGuiCol_FrameBg), 3);
				draw->AddRectFilled({left, y - 3}, {x, y + 3}, ImGui::GetColorU32(ImGuiCol_CheckMark), 3);
				draw->AddCircleFilled({x, y}, 7, ImGui::GetColorU32(ImGuiCol_CheckMark), 24);
				draw->AddCircleFilled({x, y}, 3.5f, IM_COL32_WHITE, 24);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Drag to adjust. Ctrl+click to type a value.");
			return changed;
		}
	}

	bool slider(const char* label, int* value, int minimum, int maximum, const char* suffix)
	{
		return framed_slider(label, ImGuiDataType_S32, value, minimum, maximum, slider_format("%d", suffix));
	}

	bool slider(const char* label, float* value, float minimum, float maximum, int precision, const char* suffix)
	{
		return framed_slider(label, ImGuiDataType_Float, value, minimum, maximum, slider_format(decimal_format(precision), suffix));
	}

	bool slider(const char* label, double* value, double minimum, double maximum, int precision, const char* suffix)
	{
		return framed_slider(label, ImGuiDataType_Double, value, minimum, maximum, slider_format(decimal_format(precision), suffix));
	}

	bool input_number(const char* label, int* value, int step)
	{
		return ImGui::InputInt(label, value, step, step * 10);
	}

	bool input_number(const char* label, float* value, int precision, const char* suffix)
	{
		return ImGui::InputFloat(label, value, 0.f, 0.f, slider_format(decimal_format(precision), suffix).c_str());
	}

	bool input_vector3(const char* label, float* xyz, int precision, const char* suffix)
	{
		return ImGui::InputFloat3(label, xyz, slider_format(decimal_format(precision), suffix).c_str());
	}

	bool choice(const char* label, int* current, std::span<const char* const> items)
	{
		ImGui::PushID(label);
		const char* label_end = ImGui::FindRenderedTextEnd(label);
		if (label_end != label)
			ImGui::TextUnformatted(label, label_end);
		const int count  = static_cast<int>(items.size());
		const bool valid = *current >= 0 && *current < count;
		bool picked      = false;
		ImGui::SetNextItemWidth(-1);
		if (ImGui::BeginCombo("##choice", valid ? items[*current] : "No choices"))
		{
			for (int i = 0; i < count; ++i)
			{
				ImGui::PushID(i);
				if (ImGui::Selectable(items[i], *current == i))
				{
					*current = i;
					picked   = true;
				}
				if (*current == i)
					ImGui::SetItemDefaultFocus();
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
		return picked;
	}

	bool choice(const char* label, int* current, std::span<const std::string> items)
	{
		std::vector<const char*> names;
		names.reserve(items.size());
		for (const auto& item : items)
			names.push_back(item.c_str());
		return choice(label, current, std::span<const char* const>(names));
	}

	bool checkbox(const char* label, bool* value)
	{
		return ImGui::Checkbox(label, value);
	}

	bool color_picker(const char* label, ImVec4* color)
	{
		return ImGui::ColorEdit4(label, &color->x, ImGuiColorEditFlags_NoInputs);
	}

	bool button(const char* label, ImVec2 size)
	{
		return ImGui::Button(label, size);
	}

	bool small_button(const char* label)
	{
		return ImGui::SmallButton(label);
	}

	bool input_text(const char* label, char* buffer, std::size_t size, const char* hint, ImGuiInputTextFlags flags)
	{
		return hint ? ImGui::InputTextWithHint(label, hint, buffer, size, flags) : ImGui::InputText(label, buffer, size, flags);
	}

	bool input_text(const char* label, std::string* text, const char* hint, ImGuiInputTextFlags flags)
	{
		flags |= ImGuiInputTextFlags_CallbackResize;
		return hint ? ImGui::InputTextWithHint(label, hint, text->data(), text->capacity() + 1, flags, resize_string, text)
		            : ImGui::InputText(label, text->data(), text->capacity() + 1, flags, resize_string, text);
	}

	bool text_editor(const char* label, std::string* text, ImVec2 size, ImGuiInputTextFlags flags)
	{
		if (size.x == 0.f)
			size.x = -FLT_MIN;
		if (size.y == 0.f)
			size.y = ImGui::GetTextLineHeight() * 8 + ImGui::GetStyle().FramePadding.y * 2;
		flags |= ImGuiInputTextFlags_CallbackResize;
		return ImGui::InputTextMultiline(label, text->data(), text->capacity() + 1, size, flags, resize_string, text);
	}

	bool search_field(const char* id, char* buffer, std::size_t size, const char* hint)
	{
		ImGui::PushID(id);
		ImGui::SetNextItemWidth(-FLT_MIN);
		const bool changed = ImGui::InputTextWithHint("##search", hint, buffer, size);
		ImGui::PopID();
		return changed;
	}

	bool matches_search(std::string_view text, std::string_view query)
	{
		const auto same = [](char a, char b) {
			return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		};
		return std::search(text.begin(), text.end(), query.begin(), query.end(), same) != text.end();
	}

	int match_list(const char* id, int count, const std::function<list_item(int)>& item, const list_options& options)
	{
		ImGui::PushID(id);
		if (options.count_label)
			ImGui::TextDisabled("%s: %d", options.count_label, count);
		int clicked = -1;
		if (ImGui::BeginChild("##rows", {0.f, options.height}, ImGuiChildFlags_Borders))
		{
			if (count <= 0 && options.empty_text)
				ImGui::TextDisabled("%s", options.empty_text);
			const float row_height = std::max(options.icon_size, ImGui::GetTextLineHeight());
			ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, {0.f, .5f});
			ImGuiListClipper clipper;
			clipper.Begin(std::max(count, 0), row_height + ImGui::GetStyle().ItemSpacing.y);
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
				{
					const auto row = item(i);
					ImGui::PushID(i);
					if (options.icon_size > 0.f)
					{
						if (row.icon)
							ImGui::Image(row.icon, {options.icon_size, options.icon_size});
						else
							ImGui::Dummy({options.icon_size, options.icon_size});
						ImGui::SameLine();
					}
					if (ImGui::Selectable(row.label.c_str(), row.selected, ImGuiSelectableFlags_None, {0.f, row_height}))
						clicked = i;
					if (!row.tooltip.empty())
						tooltip(row.tooltip.c_str());
					ImGui::PopID();
				}
			}
			ImGui::PopStyleVar();
		}
		ImGui::EndChild();
		ImGui::PopID();
		return clicked;
	}

	row_action action_list(const char* id, int count, const std::function<action_row(int)>& row,
		std::span<const char* const> actions, const list_options& options)
	{
		ImGui::PushID(id);
		if (options.count_label)
			ImGui::TextDisabled("%s: %d", options.count_label, count);
		row_action clicked;
		if (ImGui::BeginChild("##rows", {0.f, options.height}, ImGuiChildFlags_Borders))
		{
			if (count <= 0)
			{
				if (options.empty_text)
					ImGui::TextDisabled("%s", options.empty_text);
			}
			else if (ImGui::BeginTable("##table", 2))
			{
				const auto& style = ImGui::GetStyle();
				float buttons     = 0.f;
				for (const char* action : actions)
					buttons += ImGui::CalcTextSize(action).x + style.FramePadding.x * 2;
				buttons += style.ItemSpacing.x * static_cast<float>(std::max<std::size_t>(actions.size(), 1) - 1);
				ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("actions", ImGuiTableColumnFlags_WidthFixed, buttons);
				ImGuiListClipper clipper;
				clipper.Begin(count);
				while (clipper.Step())
				{
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						const auto item = row(i);
						ImGui::PushID(i);
						ImGui::TextUnformatted(item.label.c_str());
						// The detail gets its own line, so long coordinates are not cut off by the buttons.
						if (!item.detail.empty())
							ImGui::TextDisabled("%s", item.detail.c_str());
						ImGui::TableSetColumnIndex(1);
						if (!item.detail.empty())
							ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetTextLineHeightWithSpacing() * .5f);
						for (int a = 0; a < static_cast<int>(actions.size()); ++a)
						{
							if (a > 0)
								ImGui::SameLine();
							if (small_button(actions[a]))
								clicked = {i, a};
						}
						ImGui::PopID();
					}
				}
				ImGui::EndTable();
			}
		}
		ImGui::EndChild();
		ImGui::PopID();
		return clicked;
	}

	bool log_view(const char* id, std::span<const log_entry> entries, log_filter& filter, float height, ImFont* font)
	{
		// Each entry names its level in words, so it reads in every theme without a colour of its own.
		const auto level_name = [](log_level level) {
			switch (level)
			{
			case log_level::warning: return "WARN";
			case log_level::error: return "ERROR";
			case log_level::input: return "IN";
			case log_level::output: return "OUT";
			default: return "INFO";
			}
		};
		const auto level_shown = [&](log_level level) {
			switch (level)
			{
			case log_level::warning: return filter.warning;
			case log_level::error: return filter.error;
			case log_level::input:
			case log_level::output: return filter.input_output;
			default: return filter.info;
			}
		};
		std::vector<const log_entry*> shown;
		for (const auto& entry : entries)
			if (level_shown(entry.level) && filter.text.PassFilter(entry.message.c_str()))
				shown.push_back(&entry);

		ImGui::PushID(id);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Log  %zu", entries.size());
		ImGui::SameLine();
		checkbox("Info", &filter.info);
		ImGui::SameLine();
		checkbox("Warn", &filter.warning);
		ImGui::SameLine();
		checkbox("Error", &filter.error);
		ImGui::SameLine();
		checkbox("In/Out", &filter.input_output);

		const auto& style = ImGui::GetStyle();
		const float auto_scroll = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize("Auto-scroll").x;
		const float copy        = ImGui::CalcTextSize("Copy").x + style.FramePadding.x * 2;
		const float clear       = ImGui::CalcTextSize("Clear").x + style.FramePadding.x * 2;
		ImGui::SetNextItemWidth(std::max(80.f, ImGui::GetContentRegionAvail().x - auto_scroll - copy - clear - style.ItemSpacing.x * 3));
		if (ImGui::InputTextWithHint("##filter", "Filter text", filter.text.InputBuf, IM_ARRAYSIZE(filter.text.InputBuf)))
			filter.text.Build();
		ImGui::SameLine();
		checkbox("Auto-scroll", &filter.auto_scroll);
		ImGui::SameLine();
		if (small_button("Copy"))
		{
			std::string text;
			for (const auto* entry : shown)
				text += "[" + entry->time + "] [" + level_name(entry->level) + "] " + entry->message + "\n";
			ImGui::SetClipboardText(text.c_str());
		}
		ImGui::SameLine();
		const bool clear_pressed = small_button("Clear");

		if (ImGui::BeginChild("##log_entries", {0.f, height}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar))
		{
			if (font)
				ImGui::PushFont(font);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6.f, 3.f});
			if (entries.empty())
				ImGui::TextDisabled("Nothing logged yet.");
			else if (shown.empty())
				ImGui::TextDisabled("No entries match the filters.");
			for (const auto* entry : shown)
			{
				ImGui::TextDisabled("[%s] [%s]", entry->time.c_str(), level_name(entry->level));
				ImGui::SameLine();
				ImGui::TextUnformatted(entry->message.c_str());
				tooltip(entry->message.c_str());
			}
			ImGui::PopStyleVar();
			if (font)
				ImGui::PopFont();
			if (filter.auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				ImGui::SetScrollHereY(1.f);
		}
		ImGui::EndChild();
		ImGui::PopID();
		return clear_pressed;
	}

	bool section(const char* label, bool open_by_default)
	{
		// The field colour at rest, like a button; the theme's header colour marks selected rows, which a section is not.
		ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
		const bool open = ImGui::CollapsingHeader(label, open_by_default ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
		ImGui::PopStyleColor();
		return open;
	}

	void progress_bar(float fraction, const char* text, float height)
	{
		if (height <= 0.f)
			height = text ? ImGui::GetFrameHeight() : 8.f;
		ImGui::ProgressBar(std::clamp(fraction, 0.f, 1.f), {-FLT_MIN, height}, text ? text : "");
	}

	void text_accent(const char* format, ...)
	{
		// The accent at full strength reads at WCAG AA or better on every preset's window background.
		auto color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
		color.w    = 1.f;
		va_list args;
		va_start(args, format);
		ImGui::TextColoredV(color, format, args);
		va_end(args);
	}

	void details(const char* id, std::span<const overlay_row> rows)
	{
		if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit))
			return;
		for (const auto& row : rows)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextDisabled("%s", row.label.c_str());
			ImGui::TableSetColumnIndex(1);
			if (row.value.empty())
				ImGui::TextDisabled("Not available");
			else
				ImGui::TextUnformatted(row.value.c_str());
		}
		ImGui::EndTable();
	}

	void tooltip(const char* text)
	{
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
			ImGui::SetTooltip("%s", text);
	}
}
