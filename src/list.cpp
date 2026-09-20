#include <astra/menu.hpp>
#include <algorithm>
#include <cmath>
namespace astra
{
	event menu::draw_list(const page& model, const theme& target, const list_style& settings)
	{
		const auto colors = animate_theme(target);
		auto* draw = ImGui::GetBackgroundDrawList();
		const float x = settings.position.x, width = std::max(200.f, settings.width), rh = std::max(24.f, settings.row_height);
		float y = settings.position.y;
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
			draw->AddLine({x, y - 1}, {x + width, y - 1}, color(colors.accent), 2);
		}
		const auto tab_count = model.tabs.size();
		if (tab_count)
		{
			const auto vis = std::min<std::size_t>(3, tab_count);
			if (tab_count > vis)
			{
				if (model.selected_tab < tab_start_)
					tab_start_ = model.selected_tab;
				if (model.selected_tab >= tab_start_ + vis)
					tab_start_ = model.selected_tab - vis + 1;
				tab_start_ = std::min(tab_start_, tab_count - vis);
			}
			else
				tab_start_ = 0;
			tab_widths_.resize(vis, width / 3);
			const float base = width / 3, selw = base * 1.4f, norm = (width - selw) / 2;
			float tx = x;
			for (std::size_t s = 0; s < vis; s++)
			{
				auto i = tab_start_ + s;
				bool a = i == model.selected_tab;
				tab_widths_[s] += ((a ? selw : norm) - tab_widths_[s]) * .15f;
				draw->AddRectFilled({tx, y}, {tx + tab_widths_[s], y + 45}, color(a ? colors.selection : colors.field));
				auto label = (a && !model.title.empty()) ? model.title : model.tabs[i];
				text(label, tx + (tab_widths_[s] - ImGui::CalcTextSize(label.c_str()).x) * .5f, y + 14, color(a ? colors.selection_text : colors.text));
				tx += tab_widths_[s];
			}
			//draw->AddLine({x, y + 64}, {x + width, y + 64}, color(colors.accent), 2);
			y += 45;
		}
		const auto total = model.controls.size(), selected = total ? std::min(model.selected_option, total - 1) : 0, visible = std::min(total, (std::size_t)std::max(1, settings.rows));
		const std::string count = std::to_string(total ? selected + 1 : 0) + " / " + std::to_string(total);
		//row(model.title, count, false, 45);
		const float top = y, height = rh * float(visible ? visible : 1);
		draw->AddRectFilled({x, top}, {x + width, top + height}, color(colors.panel));
		const auto start = selected >= visible && visible ? selected - visible + 1 : 0;
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
			std::string right;
			if (c.kind == control_kind::submenu)
				right = ">>";
			else if (c.kind == control_kind::number || c.kind == control_kind::toggle_number || c.kind == control_kind::choice)
				right = c.value_text;
			auto ink = color(active ? colors.selection_text : colors.text);
			text(c.label, x + 14, y + (rh - ImGui::GetFontSize()) * .5f, ink);
			if (!right.empty() && !toggle && !slider)
			{
				auto shown = (c.kind == control_kind::submenu) ? right : (std::string("< ") + right + " >");
				text(shown, x + width - 14 - ImGui::CalcTextSize(shown.c_str()).x, y + (rh - ImGui::GetFontSize()) * .5f, ink);
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
				const float slider_width = std::clamp(width * 0.30f, 110.f, 180.f);
                const float r = x + width - 58.f, l = r - slider_width;
                double mn = std::min(c.minimum, c.maximum), mx = std::max(c.minimum, c.maximum);
				float q = mx > mn ? float(std::clamp((c.value - mn) / (mx - mn), 0.0, 1.0)) : 0, kn = l + (r - l) * q;
				draw->AddRectFilled({l, y + rh * .5f - 2}, {r, y + rh * .5f + 2}, color(colors.muted), 2);
				draw->AddRectFilled({l, y + rh * .5f - 2}, {kn, y + rh * .5f + 2}, color(colors.accent), 2);
				draw->AddCircleFilled({kn, y + rh * .5f}, 6, color(colors.accent), 20);
			}
			y += rh;
		}
		if (!total)
		{
			text("No options", x + 14, y + (rh - ImGui::GetFontSize()) * .5f, color(colors.muted));
			y += rh;
		}
		if (total > visible)
		{
			float tt = top + 4, tb = top + height - 4, th = std::max(18.f, (tb - tt) * float(visible) / float(total)), target = float(start) / float(total - visible);
			auto* st = ImGui::GetStateStorage();
			auto id = ImGui::GetID(("##list_scroll_" + model.id).c_str());
			float p = st->GetFloat(id, target);
			p += (target - p) * (1 - std::exp(-14 * ImGui::GetIO().DeltaTime));
			st->SetFloat(id, p);
			float py = tt + (tb - tt - th) * std::clamp(p, 0.f, 1.f);
			draw->AddRectFilled({x + width + 8, tt}, {x + width + 12, tb}, color(colors.field));
			draw->AddRectFilled({x + width + 8, py}, {x + width + 12, py + th}, color(colors.accent));
		}        if (total && !model.controls[selected].description.empty())
        {
            const float panel_x = x + width + 28.f;
            const float panel_y = std::clamp(selected_row_y_, top, top + height - rh);
            const float panel_w = 300.f;
            const float panel_h = std::max(rh + 18.f, ImGui::CalcTextSize(model.controls[selected].description.c_str(), nullptr, false, panel_w - 28.f).y + 24.f);
            draw->AddRectFilled({panel_x, panel_y}, {panel_x + panel_w, panel_y + panel_h}, color(colors.panel));
            const float arrow_y = std::clamp(selected_row_y_ + rh * .5f, panel_y + 14.f, panel_y + panel_h - 14.f);
            draw->AddTriangleFilled({panel_x, arrow_y - 13.f}, {panel_x - 16.f, arrow_y}, {panel_x, arrow_y + 13.f}, color(colors.panel));
            draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), {panel_x + 14, panel_y + 12}, color(colors.text), model.controls[selected].description.c_str(), nullptr, panel_w - 28.f);
        }
        y = top + height + 14.f;
        draw->AddRectFilled({x, y}, {x + width, y + 42.f}, color(colors.panel));
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), {x + 14, y + 12}, color(colors.muted), settings.footer.c_str());
		return {};
	}
}