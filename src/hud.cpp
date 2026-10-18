#include <astra/hud.hpp>
#include <astra/components.hpp>
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace astra
{
	namespace
	{
		ImU32 faded(ImVec4 color, float alpha)
		{
			color.w *= alpha;
			return pack_color(color);
		}

		float luminance(ImVec4 color)
		{
			const auto channel = [](float c) { return c <= .03928f ? c / 12.92f : std::pow((c + .055f) / 1.055f, 2.4f); };
			return .2126f * channel(color.x) + .7152f * channel(color.y) + .0722f * channel(color.z);
		}

		// The kind colour, mixed toward the text colour in steps until it reads at WCAG AA on the background.
		ImVec4 readable(ImVec4 color, ImVec4 text, ImVec4 background)
		{
			for (int step = 0; step <= 10; ++step)
			{
				const float t = step / 10.f;
				const ImVec4 mixed{color.x + (text.x - color.x) * t, color.y + (text.y - color.y) * t, color.z + (text.z - color.z) * t, 1.f};
				if (contrast_ratio(mixed, background) >= 4.5f)
					return mixed;
			}
			return {text.x, text.y, text.z, 1.f};
		}
	}

	float contrast_ratio(ImVec4 a, ImVec4 b)
	{
		const float la = luminance(a), lb = luminance(b);
		return (std::max(la, lb) + .05f) / (std::min(la, lb) + .05f);
	}

	void draw_notifications(std::span<const toast> toasts, const theme& colors)
	{
		// The overlay window's measurements: title bar padding, the theme's rounding, and the border colour.
		constexpr float width = 320.f, margin = 10.f, pad_x = 12.f, bar_pad_y = 8.f, body_pad_y = 12.f, dot = 4.f, lead_gap = 7.f;
		auto* draw = ImGui::GetBackgroundDrawList();
		auto* font = ImGui::GetFont();
		const float size = ImGui::GetFontSize(), wrap = width - pad_x * 2.f;
		const float x = ImGui::GetIO().DisplaySize.x - margin - width;
		const ImVec4 border{colors.muted.x, colors.muted.y, colors.muted.z, .16f};
		float y = margin;
		for (const auto& item : toasts)
		{
			const float alpha = std::clamp(item.alpha, 0.f, 1.f);
			if (alpha <= 0.f)
				continue;
			const ImVec4 kind = readable(item.color, colors.text, colors.panel);
			const float lead = (item.icon.empty() ? dot * 2.f : font->CalcTextSizeA(size, FLT_MAX, 0.f, item.icon.c_str()).x) + lead_gap;
			// Heights come from the same wrapping AddText uses, so long text never spills out of the card.
			const float bar_h  = font->CalcTextSizeA(size, FLT_MAX, wrap - lead, item.title.c_str()).y + bar_pad_y * 2.f;
			const float body_h = item.message.empty() ? 0.f : font->CalcTextSizeA(size, FLT_MAX, wrap, item.message.c_str()).y + body_pad_y * 2.f;
			const ImVec2 bar_end{x + width, y + bar_h}, card_end{x + width, y + bar_h + body_h};
			draw->AddRectFilled({x, y}, bar_end, faded(colors.panel, alpha), colors.rounding,
				body_h > 0.f ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll);
			if (body_h > 0.f)
				draw->AddRectFilled({x, y + bar_h}, card_end, faded(colors.background, alpha), colors.rounding, ImDrawFlags_RoundCornersBottom);
			draw->AddRect({x, y}, card_end, faded(border, alpha), colors.rounding);
			const ImVec2 title_at{x + pad_x, y + bar_pad_y};
			if (item.icon.empty())
				draw->AddCircleFilled({title_at.x + dot, title_at.y + size * .5f}, dot, faded(kind, alpha), 16);
			else
				draw->AddText(font, size, title_at, faded(kind, alpha), item.icon.c_str());
			const ImU32 ink = faded(colors.text, alpha);
			draw->AddText(font, size, {title_at.x + lead, title_at.y}, ink, item.title.c_str(), nullptr, wrap - lead);
			if (body_h > 0.f)
				draw->AddText(font, size, {x + pad_x, y + bar_h + body_pad_y}, ink, item.message.c_str(), nullptr, wrap);
			y = card_end.y + margin;
		}
	}

	ImVec4 notice_color(notice kind)
	{
		switch (kind)
		{
		case notice::success: return {0.29f, 0.69f, 0.34f, 1.f};
		case notice::warning: return {0.69f, 0.49f, 0.29f, 1.f};
		case notice::error: return {0.69f, 0.29f, 0.29f, 1.f};
		case notice::protection: return {0.29f, 0.34f, 0.69f, 1.f};
		default: return {0.80f, 0.80f, 0.83f, 1.f};
		}
	}

	void notification_queue::push(notice kind, std::string title, std::string message, std::string icon, float seconds, clock::time_point now)
	{
		std::lock_guard lock(m_mutex);
		const auto shown = std::ranges::find(m_entries, title, &entry::title);
		if (shown == m_entries.end())
		{
			m_entries.push_back({kind, std::move(title), std::move(message), std::move(icon), now, seconds});
			return;
		}
		shown->kind     = kind;
		shown->message  = std::move(message);
		shown->icon     = std::move(icon);
		shown->shown_at = now;
		shown->seconds  = seconds;
	}

	std::vector<toast> notification_queue::cards(clock::time_point now)
	{
		std::lock_guard lock(m_mutex);
		std::vector<toast> shown;
		std::vector<entry> kept;
		for (auto& item : m_entries)
		{
			const float age   = std::chrono::duration<float>(now - item.shown_at).count();
			const float alpha = age <= item.seconds ? 1.f : 1.f - (age - item.seconds) / fade_seconds;
			if (alpha <= 0.f)
				continue;
			shown.push_back({item.title, item.message, notice_color(item.kind), alpha, item.icon});
			kept.push_back(std::move(item));
		}
		m_entries = std::move(kept);
		return shown;
	}

	void notification_queue::clear()
	{
		std::lock_guard lock(m_mutex);
		m_entries.clear();
	}

	void draw_overlay(std::string_view title, std::span<const overlay_row> rows, const theme& colors, ImVec2 position, ImVec2 pivot, bool interactive)
	{
		scoped_theme style(colors);
		ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver, pivot);
		// A floor on the width keeps the window from twitching as the values change length.
		ImGui::SetNextWindowSizeConstraints({240.f, 0.f}, {FLT_MAX, FLT_MAX});
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
		if (!interactive)
			flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration;
		const std::string label = std::string(title) + "###astra_overlay";
		if (ImGui::Begin(label.c_str(), nullptr, flags))
			details("##astra_overlay_rows", rows);
		ImGui::End();
	}

	void draw_info_window(const char* title, const info_window& content, const theme& colors, ImVec2 position, ImVec2 size)
	{
		scoped_theme style(colors);
		const auto display = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize({std::min(size.x, display.x), std::min(size.y, display.y)}, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints({std::min(240.f, size.x), 180.f}, display);
		if (ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::PushTextWrapPos(0.f);
			if (!content.message.empty())
			{
				ImGui::TextUnformatted(content.message.c_str());
			}
			else
			{
				if (!content.heading.empty())
				{
					ImGui::TextUnformatted(content.heading.c_str());
					ImGui::Separator();
				}
				for (const auto& row : content.rows)
				{
					const char* value = row.value.empty() ? "Not available" : row.value.c_str();
					if (row.label.empty())
						ImGui::TextUnformatted(value);
					else
						ImGui::Text("%s: %s", row.label.c_str(), value);
				}
				if (!content.note.empty())
				{
					ImGui::Separator();
					ImGui::TextUnformatted(content.note.c_str());
				}
			}
			ImGui::PopTextWrapPos();
		}
		ImGui::End();
	}

	int draw_inspector(const char* title, const inspector& content, const theme& colors, ImVec2 position, ImVec2 pivot, bool interactive, float width)
	{
		scoped_theme style(colors);
		ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver, pivot);
		// A height of 0 lets the card fit its rows while the width stays put, so it does not jump as values change.
		ImGui::SetNextWindowSize({width, 0.f}, ImGuiCond_Always);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
		if (!interactive)
			flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav;
		int clicked = -1;
		if (ImGui::Begin(title, nullptr, flags))
		{
			ImGui::TextUnformatted(content.name.c_str());
			if (!content.tag.empty())
			{
				ImGui::SameLine();
				ImGui::TextDisabled("%s", content.tag.c_str());
			}
			if (content.bar >= 0.f)
				progress_bar(content.bar);
			details("##inspector_rows", content.rows);
			if (!content.hint.empty())
			{
				ImGui::Separator();
				text_accent("%s", content.hint.c_str());
			}
			if (interactive)
			{
				for (int i = 0; i < static_cast<int>(content.actions.size()); ++i)
				{
					ImGui::PushID(i);
					if (i > 0)
						ImGui::SameLine();
					if (button(content.actions[i].c_str()))
						clicked = i;
					ImGui::PopID();
				}
			}
		}
		ImGui::End();
		return clicked;
	}

	bool begin_panel(const char* title, bool* open, ImVec2 position, const panel_options& options)
	{
		const auto display = ImGui::GetIO().DisplaySize;
		const ImVec2 floor{options.min_size.x > 0.f ? options.min_size.x : options.size.x, options.min_size.y > 0.f ? options.min_size.y : options.size.y};
		ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(options.size, ImGuiCond_FirstUseEver);
		// The floor also lifts a size saved in the ini file from before, however small the player had made it.
		ImGui::SetNextWindowSizeConstraints({std::min(floor.x, display.x), std::min(floor.y, display.y)}, {FLT_MAX, FLT_MAX});
		ImGuiWindowFlags flags = options.fit_content ? ImGuiWindowFlags_AlwaysAutoResize : ImGuiWindowFlags_None;
		if (!options.collapsible)
			flags |= ImGuiWindowFlags_NoCollapse;
		return ImGui::Begin(title, open, flags);
	}

	void end_panel()
	{
		ImGui::End();
	}
}
