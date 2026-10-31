#include <astra/menu.hpp>
#include <astra/hud.hpp>
#include "widgets.hpp"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace astra
{
	namespace
	{
		const char* status_word(status_kind kind)
		{
			switch (kind)
			{
			case status_kind::active: return "In progress";
			case status_kind::done: return "Done";
			case status_kind::failed: return "Failed";
			default: return "Not yet";
			}
		}

		// The notification colours, mixed toward the text until they read at WCAG AA on the List's rows.
		ImVec4 status_color(status_kind kind, const theme& colors)
		{
			ImVec4 base = colors.muted;
			if (kind == status_kind::active)
				base = notice_color(notice::warning);
			else if (kind == status_kind::done)
				base = notice_color(notice::success);
			else if (kind == status_kind::failed)
				base = notice_color(notice::error);
			return detail::readable(base, colors.text, colors.panel);
		}
	}

	event menu::draw_list(const page& model, const theme& target, const list_style& settings)
	{
		const auto colors = animate_theme(target);
		event result;
		std::function<void()> pending;
		auto* draw = ImGui::GetBackgroundDrawList();
		const float x = settings.position.x, width = std::max(200.f, settings.width), rh = std::max(24.f, settings.row_height);
		float y = settings.position.y;
		const auto total = model.controls.size();
		const auto visible = std::min(total, (std::size_t)std::max(1, settings.rows));
		const float input_height = (settings.banner ? settings.banner_height : 60.f) +
			(model.tabs.empty() ? 0.f : 45.f) + rh * float(visible ? visible : 1);
		const auto input_id = "##astra_list_input_" + model.id;
		ImGui::SetNextWindowPos(settings.position);
		ImGui::SetNextWindowSize({width + 24.f, input_height});
		ImGui::SetNextWindowBgAlpha(0.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
		const bool input_visible = ImGui::Begin(input_id.c_str(), nullptr,
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			(settings.mouse_enabled ? 0 : ImGuiWindowFlags_NoMouseInputs));
		ImGui::PopStyleVar(2);
		// The List draws into its own window, kept in front, so the game's panels and overlay never cover it.
		// The draw list takes the full screen: the footer and scrollbar sit outside the input window's rows.
		detail::keep_in_front();
		draw = ImGui::GetWindowDrawList();
		draw->PushClipRectFullScreen();
		auto& io = ImGui::GetIO();
		auto hitbox = [&](const std::string& id, ImVec2 position, ImVec2 size, auto&& clicked) {
			if (!input_visible || !settings.mouse_enabled || size.x <= 0 || size.y <= 0)
				return false;
			ImGui::SetCursorScreenPos({x + position.x, settings.position.y + position.y});
			ImGui::PushID(id.c_str());
			if (ImGui::InvisibleButton("##hitbox", size) && result.kind == event_kind::none)
				clicked();
			const bool active = ImGui::IsItemActive();
			if (ImGui::IsItemHovered() || active)
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			ImGui::PopID();
			return active;
		};
		auto hovered = [&](ImVec2 lo, ImVec2 hi) {
			return input_visible && settings.mouse_enabled && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(lo, hi);
		};
		if (model.breadcrumbs.size() > 1 && hovered(settings.position, {x + width + 24, settings.position.y + input_height}) && ImGui::IsMouseClicked(1))
			result = {event_kind::back};
		if (list_page_ != model.id)
		{
			list_page_ = model.id;
			list_start_ = 0;
			list_wheel_ = tab_wheel_ = list_scroll_position_ = 0.f;
			selected_row_y_ = 0.f;
		}
		auto color = [](ImVec4 c) {
			return pack_color(c);
		};
		auto text = [&](const std::string& s, float px, float py, ImU32 c) {
			draw->AddText({px, py}, c, s.c_str());
		};
		auto row = [&](const std::string& left, const std::string& right, bool active, float h) {
			draw->AddRectFilled({x, y}, {x + width, y + h}, color(colors.panel));
			auto ink = color(active ? colors.selection_text : colors.text);
			text(left, x + 14, y + (h - ImGui::GetFontSize()) * .5f, ink);
			if (!right.empty())
				text(right, x + width - 14 - ImGui::CalcTextSize(right.c_str()).x, y + (h - ImGui::GetFontSize()) * .5f, ink);
			y += h;
		};
		if (settings.banner)
		{
			draw->AddImage(settings.banner, {x, y}, {x + width, y + settings.banner_height});
			y += settings.banner_height;
		}
		else
		{
			row(model.title, "ASTRA", false, 60);
		}
		const auto tab_count = model.tabs.size();
		if (tab_count)
		{
			if (hovered({x, y}, {x + width, y + 45}) && !ImGui::IsMouseDown(0))
			{
				tab_wheel_ -= io.MouseWheel + io.MouseWheelH;
				const int steps = static_cast<int>(tab_wheel_);
				tab_wheel_ -= float(steps);
				if (steps && result.kind == event_kind::none)
					result = {event_kind::tab, static_cast<std::size_t>(std::clamp(
						double(model.selected_tab) + steps, 0.0, double(tab_count - 1)))};
			}
			const auto vis = std::min<std::size_t>(3, tab_count);
			// The selected tab sits in the middle while another tab follows it, so the next tab always shows beside it
			// and a player sees there are more than three: Self | Mission | World becomes Mission | World | ESP on World.
			const auto tab_start = std::min(model.selected_tab ? model.selected_tab - 1 : 0, tab_count - vis);
			tab_widths_.resize(vis, width / 3);
			const float base = width / 3, selw = base * 1.4f, norm = (width - selw) / 2;
			float tx = x;
			for (std::size_t s = 0; s < vis; s++)
			{
				auto i = tab_start + s;
				bool a = i == model.selected_tab;
				tab_widths_[s] += ((a ? selw : norm) - tab_widths_[s]) * .15f;
				hitbox("tab_" + std::to_string(i), {tx - x, y - settings.position.y}, {tab_widths_[s], 45}, [&] {
					result = {event_kind::tab, i};
				});
				draw->AddRectFilled({tx, y}, {tx + tab_widths_[s], y + 45}, color(a ? colors.selection : colors.field));
				auto label = (a && !model.title.empty()) ? model.title : model.tabs[i];
				text(label, tx + (tab_widths_[s] - ImGui::CalcTextSize(label.c_str()).x) * .5f, y + 14, color(a ? colors.selection_text : colors.text));
				tx += tab_widths_[s];
			}
			y += 45;
		}
		auto selected = total ? std::min(model.selected_option, total - 1) : 0;
		const std::string count = std::to_string(total ? selected + 1 : 0) + " / " + std::to_string(total);
		//row(model.title, count, false, 45);
		const float top = y, height = rh * float(visible ? visible : 1);
		draw->AddRectFilled({x, top}, {x + width, top + height}, color(colors.panel));
		const auto max_start = total - visible;
		list_start_ = std::min(list_start_, max_start);
		if (total)
		{
			if (selected < list_start_) list_start_ = selected;
			if (selected >= list_start_ + visible) list_start_ = selected - visible + 1;
		}
		auto scroll_to = [&](std::size_t next) {
			list_start_ = std::min(next, max_start);
			if (total)
			{
				const auto next_selected = std::clamp(selected, list_start_, list_start_ + visible - 1);
				if (next_selected != selected && result.kind == event_kind::none)
					result = {event_kind::option, next_selected};
				selected = next_selected;
			}
		};
		if (hovered({x, top}, {x + width + 24, top + height}) && !ImGui::IsMouseDown(0))
		{
			list_wheel_ -= io.MouseWheel;
			const int steps = static_cast<int>(list_wheel_);
			list_wheel_ -= float(steps);
			if (steps) scroll_to(static_cast<std::size_t>(std::clamp(double(list_start_) + steps, 0.0, double(max_start))));
		}
		if (max_start)
		{
			const float tt = top + 4, tb = top + height - 4;
			const float th = std::min(tb - tt, std::max(18.f, (tb - tt) * float(visible) / float(total)));
			const float travel = tb - tt - th;
			list_scroll_position_ += (float(list_start_) / float(max_start) - list_scroll_position_) * (1 - std::exp(-14 * io.DeltaTime));
			float py = tt + travel * std::clamp(list_scroll_position_, 0.f, 1.f);
			const bool dragging = hitbox("scrollbar", {width + 4, tt - settings.position.y}, {16, tb - tt}, [] {});
			if (dragging)
			{
				if (ImGui::IsItemActivated())
					list_scroll_grab_ = io.MousePos.y >= py && io.MousePos.y < py + th ? io.MousePos.y - py : th * .5f;
				const float p = travel > 0 ? std::clamp((io.MousePos.y - tt - list_scroll_grab_) / travel, 0.f, 1.f) : std::clamp((io.MousePos.y - tt) / (tb - tt), 0.f, 1.f);
				scroll_to(static_cast<std::size_t>(std::round(p * float(max_start))));
				list_scroll_position_ = float(list_start_) / float(max_start);
				py = tt + travel * list_scroll_position_;
			}
			draw->AddRectFilled({x + width + 8, tt}, {x + width + 12, tb}, color(colors.field));
			draw->AddRectFilled({x + width + 8, py}, {x + width + 12, py + th}, color(colors.accent));
		}
		const auto start = list_start_;
		if (total)
		{
			float target_y = top + rh * float(selected - start);
			if (!selected_row_y_)
				selected_row_y_ = target_y;
			selected_row_y_ += (target_y - selected_row_y_) * .2f;
			draw->AddRectFilled({x, selected_row_y_}, {x + width, selected_row_y_ + rh}, color(colors.selection));
		}
		for (std::size_t i = start; i < start + visible; i++)
		{
			const auto& c = model.controls[i];
			const bool active = i == selected, toggle = c.kind == control_kind::toggle || c.kind == control_kind::toggle_number, slider = c.kind == control_kind::toggle_number;
			const std::string row_id = "option_" + (c.id.empty() ? std::to_string(i) : c.id);
			auto select = [&] { result = {event_kind::option, i}; };
			auto activate = [&] {
				select();
				pending = c.activate;
				if (!pending && c.kind == control_kind::toggle && c.set_value)
					pending = [fn = c.set_value, checked = c.checked] { fn(checked ? 0.0 : 1.0); };
			};
			std::string right;
			if (c.kind == control_kind::submenu)
				right = ">>";
			else if (c.kind == control_kind::number || c.kind == control_kind::toggle_number || c.kind == control_kind::choice)
				right = c.value_text;
			if (right.empty() && c.kind == control_kind::choice && c.choice >= 0 && c.choice < static_cast<int>(c.choices.size()))
				right = c.choices[c.choice];
			if (right.empty() && c.kind == control_kind::number)
			{
				std::ostringstream value;
				value << std::fixed << std::setprecision(c.integral ? 0 : std::clamp(c.precision, 0, 12)) << c.value;
				right = value.str();
			}
			const float local_y = y - settings.position.y;
			const float slider_width = std::clamp(width * 0.30f, 110.f, 180.f);
			const float r = x + width - 58.f, l = r - slider_width;
			if (slider)
			{
				hitbox(row_id + "/label", {0, local_y}, {l - x - 6, rh}, activate);
				hitbox(row_id + "/toggle", {r - x + 6, local_y}, {x + width - r - 6, rh}, activate);
				const bool dragging = hitbox(row_id + "/slider", {l - x - 6, local_y}, {r - l + 12, rh}, [] {});
				if (dragging && c.set_value && result.kind == event_kind::none)
				{
					const double mn = std::min(c.minimum, c.maximum), mx = std::max(c.minimum, c.maximum);
					const double q = std::clamp(double((io.MousePos.x - l) / (r - l)), 0.0, 1.0);
					const double value = bounded_value(mn + (mx - mn) * q, mn, mx, c.integral);
					select();
					if (value != c.value) pending = [fn = c.set_value, value] { fn(value); };
				}
			}
			else if (c.kind == control_kind::number || c.kind == control_kind::choice)
			{
				const auto shown = std::string("< ") + right + " >";
				const float right_edge = width - 14;
				const float left_edge = std::max(0.f, right_edge - ImGui::CalcTextSize(shown.c_str()).x);
				const float arrow = std::min(24.f, (right_edge - left_edge) * .5f);
				auto step = [&](int direction) {
					select();
					if (c.kind == control_kind::number && c.set_value)
					{
						const double value = bounded_value(c.value + direction * c.step, c.minimum, c.maximum, c.integral);
						pending = [fn = c.set_value, value] { fn(value); };
					}
					else if (c.set_choice && !c.choices.empty())
					{
						const int count = static_cast<int>(c.choices.size());
						const int value = (std::clamp(c.choice, 0, count - 1) + direction + count) % count;
						pending = [fn = c.set_choice, value] { fn(value); };
					}
				};
				hitbox(row_id + "/label", {0, local_y}, {left_edge, rh}, activate);
				hitbox(row_id + "/previous", {left_edge, local_y}, {arrow, rh}, [&] { step(-1); });
				hitbox(row_id + "/value", {left_edge + arrow, local_y}, {right_edge - left_edge - 2 * arrow, rh}, activate);
				hitbox(row_id + "/next", {right_edge - arrow, local_y}, {width - right_edge + arrow, rh}, [&] { step(1); });
			}
			else hitbox(row_id, {0, local_y}, {width, rh}, activate);
			// Only a moving cursor takes the selection, so a cursor resting on the list never undoes keyboard navigation.
			const bool cursor_moved = io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f;
			if (!active && cursor_moved && result.kind == event_kind::none && !ImGui::IsAnyItemActive() && hovered({x, y}, {x + width, y + rh}))
				select();
			auto ink = color(active ? colors.selection_text : colors.text);
			text(c.label, x + 14, y + (rh - ImGui::GetFontSize()) * .5f, ink);
			if (!right.empty() && !toggle && !slider)
			{
				auto shown = (c.kind == control_kind::submenu) ? right : (std::string("< ") + right + " >");
				text(shown, x + width - 14 - ImGui::CalcTextSize(shown.c_str()).x, y + (rh - ImGui::GetFontSize()) * .5f, ink);
			}
			if (c.status != status_kind::none)
			{
				// The badge ends where the row's own value, checkbox, or slider begins.
				float edge = x + width - 14;
				if (slider)
					edge = l - 12;
				else if (toggle)
					edge = x + width - 40;
				else if (!right.empty())
					edge -= ImGui::CalcTextSize((c.kind == control_kind::submenu ? right : "< " + right + " >").c_str()).x + 12;
				const char* word = c.status_text.empty() ? status_word(c.status) : c.status_text.c_str();
				const float word_x = edge - ImGui::CalcTextSize(word).x, dot_x = word_x - 10, mid = y + rh * .5f;
				// On the highlighted row the badge takes the highlight's text colour, like the checkbox and slider.
				const auto mark = color(active ? colors.selection_text : status_color(c.status, colors));
				if (c.status == status_kind::pending)
					draw->AddCircle({dot_x, mid}, 4, mark, 16, 1.5f);
				else
					draw->AddCircleFilled({dot_x, mid}, 4, mark, 16);
				text(word, word_x, y + (rh - ImGui::GetFontSize()) * .5f, mark);
			}
			if (toggle)
			{
				ImVec2 b{x + width - 28, y + (rh - 18) * .5f};
				draw->AddRect(b, {b.x + 18, b.y + 18}, color(active ? colors.background : colors.muted), 3, 0, 2);
				if (c.checked)
				{
					draw->AddLine({b.x + 4, b.y + 9}, {b.x + 8, b.y + 13}, color(active ? colors.selection_text : colors.accent), 2);
					draw->AddLine({b.x + 8, b.y + 13}, {b.x + 15, b.y + 5}, color(active ? colors.selection_text : colors.accent), 2);
				}
			}
			if (slider)
			{
                double mn = std::min(c.minimum, c.maximum), mx = std::max(c.minimum, c.maximum);
				float q = mx > mn ? float(std::clamp((c.value - mn) / (mx - mn), 0.0, 1.0)) : 0, kn = l + (r - l) * q;
				// On the highlighted row the slider takes the highlight's text colour, like the checkbox, since the
				// highlight is often the accent itself.
				const auto track = active ? ImVec4{colors.selection_text.x, colors.selection_text.y, colors.selection_text.z, colors.selection_text.w * .35f} : colors.muted;
				const auto fill = active ? colors.selection_text : colors.accent;
				draw->AddRectFilled({l, y + rh * .5f - 2}, {r, y + rh * .5f + 2}, color(track), 2);
				draw->AddRectFilled({l, y + rh * .5f - 2}, {kn, y + rh * .5f + 2}, color(fill), 2);
				draw->AddCircleFilled({kn, y + rh * .5f}, 6, color(fill), 20);
			}
			y += rh;
		}
		if (!total)
		{
			text("No options", x + 14, y + (rh - ImGui::GetFontSize()) * .5f, color(colors.muted));
			y += rh;
		}
		if (total && !model.controls[selected].description.empty())
        {
            const float panel_x = x + width + 28.f;
            const float panel_y = std::clamp(selected_row_y_, top, top + height - rh);
            const float panel_w = 300.f;
            const float panel_h = std::max(rh + 18.f, ImGui::CalcTextSize(model.controls[selected].description.c_str(), nullptr, false, panel_w - 28.f).y + 24.f);
            // Behind the game's windows: a panel beside the List covers the card rather than the card covering the panel.
            auto* card = ImGui::GetBackgroundDrawList();
            card->AddRectFilled({panel_x, panel_y}, {panel_x + panel_w, panel_y + panel_h}, color(colors.panel));
            const float arrow_y = std::clamp(selected_row_y_ + rh * .5f, panel_y + 14.f, panel_y + panel_h - 14.f);
            card->AddTriangleFilled({panel_x, arrow_y - 13.f}, {panel_x - 16.f, arrow_y}, {panel_x, arrow_y + 13.f}, color(colors.panel));
            card->AddText(ImGui::GetFont(), ImGui::GetFontSize(), {panel_x + 14, panel_y + 12}, color(colors.text), model.controls[selected].description.c_str(), nullptr, panel_w - 28.f);
        }
        y = top + height;
        draw->AddRectFilled({x, y}, {x + width, y + 42.f}, color(colors.panel));
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), {x + 14, y + 12}, color(colors.muted), settings.footer.c_str());
        if (tab_count)
            draw->AddLine({x, top}, {x + width, top}, color(colors.accent), 2.f);
        draw->AddLine({x, y}, {x + width, y}, color(colors.accent), 2.f);
		draw->PopClipRect();
		// The rows, the scrollbar beside them, and the footer; panels may sit over the description card beside them.
		area_min_ = {x, settings.position.y};
		area_max_ = {x + width + 12.f, y + 42.f};
		area_frame_ = ImGui::GetFrameCount();
		area_window_ = ImGui::GetCurrentWindow()->ID;
		ImGui::End();
		if (pending && result.kind == event_kind::option)
			pending();
		return result;
	}
}
