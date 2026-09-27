#include <astra/host/canvas.hpp>
#include <astra/host/menu/sub_option.hpp>
#include "renderer.hpp"
#include "pointers.hpp"
#include "fonts/icon_list.hpp"

#include <gui.hpp>

namespace big
{

    std::vector<astra::control> canvas::window_controls(abstract_submenu* sub,
        std::vector<std::uint32_t> ancestors)
    {
        if (std::find(ancestors.begin(), ancestors.end(), sub->get_id()) != ancestors.end())
            return {};
        ancestors.push_back(sub->get_id());
        // A submenu can be linked from multiple groups; rebuilding it twice would
        // invalidate callbacks already collected for this frame.
        if (m_window_prepared.insert(sub).second)
        {
            sub->reset();
            sub->execute();
        }
        std::vector<astra::control> controls;
        for (std::size_t i = 0; i < sub->get_num_option(); ++i)
        {
            auto* opt = sub->get_option(i);
            if (!opt) continue;
            auto c = opt->describe_ui();
            c.id = c.label + "##" + std::to_string(i);
            if (opt->get_flag(OptionFlag::SidePanel))
                c.draw_details = [opt] { opt->draw_side_panel(); };
            if (auto* link = dynamic_cast<sub_option*>(opt))
            {
                for (const auto& child : m_all_submenu)
                    if (child->get_id() == link->submenu_id())
                    {
                        c.children = [this, child = child.get(), ancestors] {
                            return window_controls(child, ancestors);
                        };
                        break;
                    }
            }
            controls.push_back(std::move(c));
        }
        return controls;
    }

    void canvas::tick_impl()
    {
        std::lock_guard lock(m_mutex);
        check_for_input_impl();
        handle_input_impl();
        m_mouse_enabled = g_settings.window.layout == 1 || g_settings.window.mouse_active;
        m_capture_game = m_opened && m_mouse_enabled.load() &&
            (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard);
        if (!m_opened || active_history().empty()) return;

        const bool window_layout = g_settings.window.layout == 1;
        auto* sub = window_layout ? active_history().front() : active_history().back();
        sub->reset();
        sub->execute();
        astra::page page;
        page.id = std::to_string(sub->get_id());
        page.title = sub->get_name();
        page.selected_tab = m_navigation.selected_tab();
        page.selected_option = sub->get_selected_option();
        for (auto& tab : m_all_tabs) page.tabs.emplace_back(tab->get_name());
        for (auto* parent : active_history()) page.breadcrumbs.emplace_back(parent->get_name());
        for (std::size_t i = 0; i < sub->get_num_option(); ++i)
        {
            auto* opt = sub->get_option(i);
            if (!opt) continue;
            auto c = opt->describe_ui();
            // Label plus occurrence disambiguates duplicate rows without pointer IDs.
            c.id = c.label + "##" + std::to_string(i);
            if (opt->get_flag(OptionFlag::SidePanel))
                c.draw_details = [opt] { opt->draw_side_panel(); };
            page.controls.push_back(std::move(c));
        }
        if (window_layout)
        {
            m_window_prepared.clear();
            m_window_prepared.insert(sub);
            page.controls = window_controls(sub, {});
            page.breadcrumbs = {sub->get_name()};
        }
        platform_prepare_render();
        static int last_theme = -1;
        if (last_theme != g_settings.window.theme)
        {
            m_astra_canvas.reset_theme();
            last_theme = g_settings.window.theme;
        }
        std::uint64_t palette_key = static_cast<std::uint64_t>(g_settings.window.theme);
        if (g_settings.window.theme == 3)
        {
            const auto& bg = g_settings.window.custom_background;
            const auto& panel = g_settings.window.custom_panel;
            const auto& text = g_settings.window.custom_text;
            const auto& accent = g_settings.window.m_tab_selected_color;
            palette_key = (palette_key << 32) ^ (static_cast<std::uint64_t>(bg.r) << 24) ^ (static_cast<std::uint64_t>(bg.g) << 16) ^ (static_cast<std::uint64_t>(bg.b) << 8) ^ (static_cast<std::uint64_t>(panel.r) << 4) ^ static_cast<std::uint64_t>(panel.g) ^ (static_cast<std::uint64_t>(panel.b) << 40) ^ (static_cast<std::uint64_t>(text.r) << 36) ^ (static_cast<std::uint64_t>(text.g) << 28) ^ (static_cast<std::uint64_t>(text.b) << 20) ^ (static_cast<std::uint64_t>(accent.r) << 12) ^ (static_cast<std::uint64_t>(accent.g) << 4) ^ static_cast<std::uint64_t>(accent.b);
        }
        static std::uint64_t last_palette = 0;
        if (last_palette != palette_key)
        {
            m_astra_canvas.reset_theme();
            last_palette = palette_key;
        }
        auto theme = astra::preset_theme(g_settings.window.theme);
        const float ui_alpha = std::clamp(g_settings.window.transparency, 0.15f, 1.f);
        theme.background.w *= ui_alpha;
        theme.panel.w *= ui_alpha;
        theme.field.w *= ui_alpha;
        theme.accent.w *= ui_alpha;
        if (g_settings.window.theme == 3)
        {
            const auto& bg = g_settings.window.custom_background;
            const auto& panel = g_settings.window.custom_panel;
            const auto& text = g_settings.window.custom_text;
            const auto& accent = g_settings.window.m_tab_selected_color;
            theme.background = {bg.r / 255.f, bg.g / 255.f, bg.b / 255.f, ui_alpha};
            theme.panel = {panel.r / 255.f, panel.g / 255.f, panel.b / 255.f, ui_alpha};
            theme.field = theme.panel;
            theme.text = {text.r / 255.f, text.g / 255.f, text.b / 255.f, 1.f};
            theme.muted = {theme.text.x, theme.text.y, theme.text.z, .65f};
            theme.accent = {accent.r / 255.f, accent.g / 255.f, accent.b / 255.f, ui_alpha};
        }
        astra::list_style style;
        style.position = {g_settings.window.m_pos.x, g_settings.window.m_pos.y};
        style.width = g_settings.window.m_width;
        style.rows = static_cast<int>(g_settings.window.m_option_per_page);
        style.banner = (ImTextureID)g_gui.m_header;
        style.banner_height = m_header_height;
        style.row_height = m_option_height;
        style.footer = "Astra | Build 1.0.0";
        style.mouse_enabled = m_mouse_enabled.load();
        bool open = m_opened;
        astra::event event = m_astra_canvas.draw(
            g_settings.window.layout == 1 ? astra::layout::window : astra::layout::list,
            GAME_NAME "###astra_menu", page, open, theme, style);
        m_opened = open;
        switch (event.kind)
        {
        case astra::event_kind::tab: m_navigation.select_tab(event.index); break;
        case astra::event_kind::back: m_navigation.back(); break;
        case astra::event_kind::breadcrumb: m_navigation.to_depth(event.index); break;
        case astra::event_kind::option:
            if (!window_layout) sub->set_selected_option(event.index);
            break;
        default: break;
        }
    }

    bool canvas::captures_message(UINT msg)
    {
        if (!uses_mouse() || !ImGui::GetCurrentContext()) return false;
        const auto& io = ImGui::GetIO();
        if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) return io.WantCaptureMouse;
        if (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) return io.WantCaptureKeyboard;
        if (msg == WM_INPUT) return io.WantCaptureMouse || io.WantCaptureKeyboard;
        return false;
    }

	void canvas::game_tick()
	{
		// if (m_opened)
		// 	PAD::DISABLE_CONTROL_ACTION(0, 27, true); // Disable phone

		// if (m_opened/* && g_Settings.m_LockMouse*/)
		// 	PAD::DISABLE_ALL_CONTROL_ACTIONS(0); // Disable Everything
	}

	void canvas::check_for_input_impl()
	{
		reset_input();

		const bool open_down = platform_is_key_pressed(g_settings.window.open_key) || platform_is_controller_pressed(XINPUT_GAMEPAD_BACK);
        m_open_key_pressed = open_down && !m_open_was_down;
        m_open_was_down = open_down;
		m_back_key_pressed = platform_is_key_pressed(g_settings.window.back_key) || platform_is_controller_pressed(XINPUT_GAMEPAD_B);
		m_enter_key_pressed = platform_is_key_pressed(g_settings.window.enter_key) || platform_is_controller_pressed(XINPUT_GAMEPAD_A);
		m_up_key_pressed = platform_is_key_pressed(g_settings.window.up_key) || platform_is_controller_pressed(XINPUT_GAMEPAD_DPAD_UP);
		m_down_key_pressed = platform_is_key_pressed(g_settings.window.down_key) || platform_is_controller_pressed(XINPUT_GAMEPAD_DPAD_DOWN);
		m_left_key_pressed = platform_is_key_pressed(g_settings.window.left_key) || platform_is_controller_pressed(XINPUT_GAMEPAD_DPAD_LEFT);
		m_right_key_pressed = platform_is_key_pressed(g_settings.window.right_key) || platform_is_controller_pressed(XINPUT_GAMEPAD_DPAD_RIGHT);
		m_left_tab_pressed = platform_is_key_pressed(g_settings.window.left_tab_key) || (platform_is_controller_pressed(XINPUT_GAMEPAD_LEFT_SHOULDER));
		m_right_tab_pressed = platform_is_key_pressed(g_settings.window.right_tab_key) || platform_is_controller_pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER);
	}

	void canvas::handle_input_impl()
	{
		static Timer openTimer(0ms);
		openTimer.SetDelay(std::chrono::milliseconds(g_settings.window.m_open_delay));
		if (m_open_key_pressed && openTimer.Update())
		{
			m_opened = !m_opened;

			if (g_settings.window.m_sounds)
				play_sound(m_opened ? "SELECT" : "BACK");
		}

		if (ImGui::GetIO().WantTextInput || g_settings.window.layout == 1) return;

		static Timer backTimer(0ms);
		backTimer.SetDelay(std::chrono::milliseconds(g_settings.window.m_back_delay));
		if (m_opened && m_back_key_pressed && backTimer.Update())
		{
			if (g_settings.window.m_sounds)
				play_sound("BACK");

			if (active_history().size() <= 1)
			{
				return;
			}
			else
			{
				m_navigation.back();
			}
			return;
		}

		if (m_opened && !active_history().empty())
		{
			auto sub = active_history().back();

			static Timer enterTimer(0ms);
			enterTimer.SetDelay(std::chrono::milliseconds(g_settings.window.m_enter_delay));
			if (m_enter_key_pressed && sub->get_num_option() != 0 && enterTimer.Update())
			{
				if (g_settings.window.m_sounds)
					play_sound("SELECT");

				if (const auto opt = sub->get_option(sub->get_selected_option()))
					opt->handle_action(OptionAction::EnterPress);
				return;
			}

			static Timer upTimer(0ms);
			upTimer.SetDelay(std::chrono::milliseconds(g_settings.window.m_vectical_delay));
			if (m_up_key_pressed && sub->get_num_option() != 0 && upTimer.Update())
			{
				if (g_settings.window.m_sounds)
					play_sound("NAV_UP_DOWN");

				sub->ScrollBackward();
			}

			static Timer downTimer(0ms);
			downTimer.SetDelay(std::chrono::milliseconds(g_settings.window.m_vectical_delay));
			if (m_down_key_pressed && sub->get_num_option() != 0 && downTimer.Update())
			{
				if (g_settings.window.m_sounds)
					play_sound("NAV_UP_DOWN");

				sub->ScrollForward();
			}

			static Timer leftTimer(0ms);
			leftTimer.SetDelay(std::chrono::milliseconds(g_settings.window.m_horizontal_delay));
			if (m_left_key_pressed && sub->get_num_option() != 0 && leftTimer.Update())
			{
				if (g_settings.window.m_sounds)
					play_sound("NAV_LEFT_RIGHT");

				if (const auto opt = sub->get_option(sub->get_selected_option()))
					opt->handle_action(OptionAction::LeftPress);
			}

			static Timer rightTimer(0ms);
			rightTimer.SetDelay(std::chrono::milliseconds(g_settings.window.m_horizontal_delay));
			if (m_right_key_pressed && sub->get_num_option() != 0 && rightTimer.Update())
			{
				if (g_settings.window.m_sounds)
					play_sound("NAV_LEFT_RIGHT");

				if (const auto opt = sub->get_option(sub->get_selected_option()))
					opt->handle_action(OptionAction::RightPress);
			}

			static Timer tabSwitchTimer(0ms);
			tabSwitchTimer.SetDelay(std::chrono::milliseconds(g_settings.window.m_tabbar_switch)); // Delay between switches

			if (!m_all_tabs.empty() && m_right_tab_pressed && tabSwitchTimer.Update())
			{
				if (m_navigation.selected_tab() < m_all_tabs.size() - 1)
				{


					switch_to_tabmenu(m_all_tabs[m_navigation.selected_tab() + 1]->get_id());
				}
			}

			if (!m_all_tabs.empty() && m_left_tab_pressed && tabSwitchTimer.Update())
			{
				if (m_navigation.selected_tab() > 0)
				{


					switch_to_tabmenu(m_all_tabs[m_navigation.selected_tab() - 1]->get_id());
				}
			}
		}
	}

	void canvas::reset_input()
	{
		m_open_key_pressed = false;
		m_back_key_pressed = false;
		m_enter_key_pressed = false;
		m_up_key_pressed = false;
		m_down_key_pressed = false;
		m_left_key_pressed = false;
		m_right_key_pressed = false;
        m_left_tab_pressed = m_right_tab_pressed = false;
	}

	void canvas::draw_rect(float x, float y, float width, float height, Color color, ImDrawList* draw_list)
	{
		const auto Position = ImVec2(x, y);
		const auto Size = ImVec2(width, height);
		const auto Rect = get_rect(Position, Size);

		draw_list->AddRectFilled(Rect.Max, Rect.Min, IM_COL32(color.r, color.g, color.b, color.a));
	}

	void canvas::draw_sprite(ImTextureID image, float x, float y, float width, float height, Color color, ImDrawList* drawlist)
	{
		const auto Position = ImVec2(x, y);
		const auto Size = ImVec2(width, height);
		const auto Rect = get_rect(Position, Size);

		if (!image) return;
		drawlist->AddImage(image,
			Rect.Min,
			Rect.Max,
			ImVec2(0, 0), ImVec2(1, 1),
			IM_COL32(color.r, color.g, color.b, color.a)
		);
	}

	void canvas::draw_sprite(D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle, float x, float y, float width, float height, Color color, ImDrawList* drawlist)
	{
		const auto Position = ImVec2(x, y);
		const auto Size = ImVec2(width, height);
		const auto Rect = get_rect(Position, Size);

		drawlist->AddImage((ImTextureID)gpu_handle.ptr,
			Rect.Min,
			Rect.Max,
			ImVec2(0, 0), ImVec2(1, 1),
			IM_COL32(color.r, color.g, color.b, color.a)
		);
	}

	void canvas::draw_left_text(const char* text, float x, float y, Color color, ImFont* font, ImDrawList* draw_list)
	{
		const auto Position = ImVec2(x, y);
		ImGui::PushFont(font);
		draw_list->AddText(Position, IM_COL32(color.r, color.g, color.b, color.a), text);
		ImGui::PopFont();
	}

	void canvas::draw_centered_text(const char* text, float x, float y, Color color, ImFont* font, ImDrawList* draw_list)
	{
		const auto Position = ImVec2((x - (ImGui::CalcTextSize(text).x / 2.f)), y);
		ImGui::PushFont(font);
		draw_list->AddText(Position, IM_COL32(color.r, color.g, color.b, color.a), text);
		ImGui::PopFont();
	}

	void canvas::draw_right_text(const char* text, float x, float y, Color color, ImFont* font, ImDrawList* draw_list)
	{
		const auto Position = ImVec2((x - ImGui::CalcTextSize(text).x), y);
		ImGui::PushFont(font);
		draw_list->AddText(Position, IM_COL32(color.r, color.g, color.b, color.a), text);
		ImGui::PopFont();
	}

	Vector2 canvas::get_sprite_scale(float size)
	{
        const auto resolution = platform_resolution();
        int x = static_cast<int>(resolution.x);
        int y = static_cast<int>(resolution.y);

		Vector2 sz = { (static_cast<float>(y) / static_cast<float>(x)) * size, size };

		return sz;
	}

	ImRect canvas::get_rect(ImVec2 pos, ImVec2 size)
	{
		const auto ItemSize = ImGui::CalcItemSize(ImVec2(size.x, size.y), 0.0f, 0.0f);
		return ImRect(ImVec2(pos.x, pos.y), ImVec2(pos.x + ItemSize.x, pos.y + ItemSize.y));
	}

	void canvas::play_sound(const char* name)
	{

	}

	void canvas::draw_stroke_text_impl(float x, float y, Color color, std::string_view str)
	{
		ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y - 1.f), ImGui::ColorConvertFloat4ToU32(ImVec4(1.f / 255.0f, 1.f / 255.0f, 1.f / 255.0f, 255 / 255.0f)), str.data());
		ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y + 1.f), ImGui::ColorConvertFloat4ToU32(ImVec4(1.f / 255.0f, 1.f / 255.0f, 1.f / 255.0f, 255 / 255.0f)), str.data());
		ImGui::GetForegroundDrawList()->AddText(ImVec2(x - 1.f, y), ImGui::ColorConvertFloat4ToU32(ImVec4(1.f / 255.0f, 1.f / 255.0f, 1.f / 255.0f, 255 / 255.0f)), str.data());
		ImGui::GetForegroundDrawList()->AddText(ImVec2(x + 1.f, y), ImGui::ColorConvertFloat4ToU32(ImVec4(1.f / 255.0f, 1.f / 255.0f, 1.f / 255.0f, 255 / 255.0f)), str.data());
		ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f)), str.data());
	}

	void canvas::draw_filled_rect_impl(float x, float y, float w, float h, Color color)
	{
		ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), ImGui::ColorConvertFloat4ToU32(ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f)), 0, 0);
	}

	void canvas::draw_circle_filled_impl(float x, float y, float radius, Color color)
	{
		ImGui::GetForegroundDrawList()->AddCircleFilled(ImVec2(x, y), radius, ImGui::ColorConvertFloat4ToU32(ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f)));
	}

	void canvas::draw_circle_impl(float x, float y, float radius, Color color, int segments)
	{
		ImGui::GetForegroundDrawList()->AddCircle(ImVec2(x, y), radius, ImGui::ColorConvertFloat4ToU32(ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f)), segments);
	}

	void canvas::draw_triangle_impl(float x1, float y1, float x2, float y2, float x3, float y3, Color color, float thickne)
	{
		ImGui::GetForegroundDrawList()->AddTriangle(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ImGui::ColorConvertFloat4ToU32(ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f)), thickne);
	}

	void canvas::draw_triangle_filled_impl(float x1, float y1, float x2, float y2, float x3, float y3, Color color)
	{
		ImGui::GetForegroundDrawList()->AddTriangleFilled(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ImGui::ColorConvertFloat4ToU32(ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f)));
	}

	void canvas::draw_line_impl(float x1, float y1, float x2, float y2, Color color, float thickness)
	{
		ImGui::GetForegroundDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), ImGui::ColorConvertFloat4ToU32(ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f)), thickness);
	}

	void canvas::draw_corner_box_impl(float x, float y, float w, float h, float borderPx, Color color)
	{
		draw_filled_rect_impl(x + borderPx, y, w / 3, borderPx, color);
		draw_filled_rect_impl(x + w - w / 3 + borderPx, y, w / 3, borderPx, color);
		draw_filled_rect_impl(x, y, borderPx, h / 3, color);
		draw_filled_rect_impl(x, y + h - h / 3 + borderPx * 2, borderPx, h / 3, color);
		draw_filled_rect_impl(x + borderPx, y + h + borderPx, w / 3, borderPx, color);
		draw_filled_rect_impl(x + w - w / 3 + borderPx, y + h + borderPx, w / 3, borderPx, color);
		draw_filled_rect_impl(x + w + borderPx, y, borderPx, h / 3, color);
		draw_filled_rect_impl(x + w + borderPx, y + h - h / 3 + borderPx * 2, borderPx, h / 3, color);
	}
	void canvas::draw_box_impl(float x, float y, float w, float h, float thickness, Color color)
	{
		auto draw_list = ImGui::GetForegroundDrawList();
		auto col = ImGui::ColorConvertFloat4ToU32(
			ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f)
		);

		draw_list->AddRect(
			ImVec2(x, y),
			ImVec2(x + w, y + h),
			col,
			0.0f,       // rounding
			0,          // flags
			thickness   // thickness
		);
	}

	void canvas::draw_filled_box_impl(float x, float y, float w, float h, Color color)
	{
		auto draw_list = ImGui::GetForegroundDrawList();
		auto col = ImGui::ColorConvertFloat4ToU32(
			ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f)
		);

		draw_list->AddRectFilled(
			ImVec2(x, y),
			ImVec2(x + w, y + h),
			col
		);
	}

	void canvas::draw_box_outlined_impl(float x, float y, float w, float h, float thickness, Color color)
	{
		// outer (hitam)
		draw_box_impl(x - 1, y - 1, w + 2, h + 2, thickness, Color(0, 0, 0, 255));

		// main
		draw_box_impl(x, y, w, h, thickness, color);

		// inner (optional)
		draw_box_impl(x + 1, y + 1, w - 2, h - 2, thickness, Color(0, 0, 0, 255));
	}

	void canvas::draw_cube_impl(ImVec2 const& screen_location, float yaw, ImVec2 const& size, Color colour)
	{
		auto draw_list = ImGui::GetForegroundDrawList();
		auto color = ImGui::ColorConvertFloat4ToU32(ImVec4(colour.r / 255.0f, colour.g / 255.0f, colour.b / 255.0f, colour.a / 255.0f));

		ImVec2 vertices[8] = {
			ImVec2(screen_location.x, screen_location.y),
			ImVec2(screen_location.x + size.x, screen_location.y),
			ImVec2(screen_location.x, screen_location.y + size.y),
			ImVec2(screen_location.x + size.x, screen_location.y + size.y),
			ImVec2(screen_location.x + size.x * 0.5f, screen_location.y + size.y * 0.5f),
			ImVec2(screen_location.x + size.x * 1.5f, screen_location.y + size.y * 0.5f),
			ImVec2(screen_location.x + size.x * 0.5f, screen_location.y + size.y * 1.5f),
			ImVec2(screen_location.x + size.x * 1.5f, screen_location.y + size.y * 1.5f)
		};

		ImVec2 center = ImVec2(screen_location.x + size.x / 2, screen_location.y + size.y / 2);

		for (int i = 0; i < 8; ++i)
		{
			vertices[i] = rotate_point_2d_impl(vertices[i], center, yaw);
		}

		draw_list->AddLine(vertices[0], vertices[1], color);
		draw_list->AddLine(vertices[1], vertices[3], color);
		draw_list->AddLine(vertices[3], vertices[2], color);
		draw_list->AddLine(vertices[2], vertices[0], color);

		draw_list->AddLine(vertices[4], vertices[5], color);
		draw_list->AddLine(vertices[5], vertices[7], color);
		draw_list->AddLine(vertices[7], vertices[6], color);
		draw_list->AddLine(vertices[6], vertices[4], color);

		draw_list->AddLine(vertices[0], vertices[4], color);
		draw_list->AddLine(vertices[1], vertices[5], color);
		draw_list->AddLine(vertices[2], vertices[6], color);
		draw_list->AddLine(vertices[3], vertices[7], color);
	}

	ImVec2 canvas::rotate_point_2d_impl(const ImVec2& point, const ImVec2& center, float yaw)
	{
		float s = sinf(yaw);
		float c = cosf(yaw);

		ImVec2 translated = ImVec2(point.x - center.x, point.y - center.y);

		ImVec2 rotated = ImVec2(translated.x * c - translated.y * s, translated.x * s + translated.y * c);

		return ImVec2(rotated.x + center.x, rotated.y + center.y);
	}

}
