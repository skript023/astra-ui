#include <astra/host/canvas.hpp>
#include <astra/host/input_service.hpp>
#include <astra/host/notification.hpp>
#include <astra/host/menu/bool_option.hpp>
#include <astra/host/menu/choose_option.hpp>
#include <astra/host/menu/number_option.hpp>
#include <astra/host/menu/reguler_submenu.hpp>
#include <astra/host/menu/sub_option.hpp>
#include "renderer.hpp"
#include "pointers.hpp"
#include "fonts/icon_list.hpp"

#include <gui.hpp>

namespace big
{
    // Constructs the canvas before settings.json is read, so the menu settings load with everything else.
    static menu_config& g_menu_config_registration = canvas::config();

    canvas::canvas() :
        state_serializer("menu")
    {
    }

    void canvas::load_state_impl(nlohmann::json& state)
    {
        std::scoped_lock lock(m_mutex, m_config_mutex);
        menu_config loaded;
        from_json(state, loaded);
        m_config = loaded;
        m_saved_config = m_config;
        state = m_saved_config;
    }

    void canvas::save_state_impl(nlohmann::json& state)
    {
        std::lock_guard lock(m_config_mutex);
        state = m_saved_config;
    }

    void canvas::track_config_changes()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now < m_next_config_check)
            return;
        m_next_config_check = now + std::chrono::milliseconds(250);

        nlohmann::json current = m_config;
        std::lock_guard lock(m_config_mutex);
        if (current == m_saved_config)
            return;
        m_saved_config = std::move(current);
        mark_state_dirty();
    }

    bool canvas::wants_mouse() const
    {
        return m_key_settings.active() || m_config.layout == 1 || m_config.mouse_active || m_mouse_held;
    }

    static astra::key_bindings current_menu_keys()
    {
        return canvas::config().keys;
    }

    // The theme the menu, notifications, and overlay share: the chosen preset or Custom palette at the menu's transparency.
    static astra::theme menu_theme()
    {
        const auto& config = canvas::config();
        auto theme = astra::preset_theme(config.theme);
        const float ui_alpha = std::clamp(config.transparency, 0.15f, 1.f);
        theme.background.w *= ui_alpha;
        theme.panel.w *= ui_alpha;
        theme.field.w *= ui_alpha;
        theme.accent.w *= ui_alpha;
        if (config.theme == 3)
        {
            const auto& bg = config.custom_background;
            const auto& panel = config.custom_panel;
            const auto& text = config.custom_text;
            const auto& accent = config.accent;
            theme.background = {bg.r / 255.f, bg.g / 255.f, bg.b / 255.f, ui_alpha};
            theme.panel = {panel.r / 255.f, panel.g / 255.f, panel.b / 255.f, ui_alpha};
            theme.field = theme.panel;
            theme.text = {text.r / 255.f, text.g / 255.f, text.b / 255.f, 1.f};
            theme.muted = {theme.text.x, theme.text.y, theme.text.z, .65f};
            theme.accent = {accent.r / 255.f, accent.g / 255.f, accent.b / 255.f, ui_alpha};
            // List paints selected rows and the active tab with selection/selection_text;
            // preset_theme(3) only supplies defaults, so the custom accent must drive them.
            theme.selection = theme.accent;
            theme.selection_text = theme.background;
        }
        return theme;
    }

    void canvas::apply_panel_theme()
    {
        astra::apply_theme(astra::panel_theme(menu_theme()));
    }

    astra::theme canvas::current_theme()
    {
        return menu_theme();
    }

    void canvas::draw_notifications()
    {
        astra::draw_notifications(notification::queue().cards(), astra::panel_theme(menu_theme()));
    }

    void canvas::draw_input()
    {
        input_service::draw();
    }

    void canvas::draw_overlay(std::string_view title, std::span<const astra::overlay_row> rows)
    {
        astra::draw_overlay(title, rows, astra::panel_theme(menu_theme()), instance().panel_slot("###astra_overlay"), {0.f, 0.f}, is_opened());
    }

    void canvas::draw_info_window(const char* title, const astra::info_window& content)
    {
        astra::draw_info_window(title, content, astra::panel_theme(menu_theme()), instance().panel_slot(title), {360.f, 390.f});
    }

    int canvas::draw_inspector(const char* title, const astra::inspector& content)
    {
        const auto* viewport = ImGui::GetMainViewport();
        const ImVec2 below_crosshair{viewport->WorkPos.x + viewport->WorkSize.x * .5f, viewport->WorkPos.y + viewport->WorkSize.y * .58f};
        return astra::draw_inspector(title, content, astra::panel_theme(menu_theme()), below_crosshair, {.5f, 0.f}, is_opened());
    }

    ImVec2 canvas::panel_slot(const char* window_name)
    {
        m_panels.emplace(window_name);
        constexpr float gap = 20.f, stack_gap = 12.f, min_room = 200.f;
        const auto& settings = m_config;
        // Level with the List's tabs, or with the top of the Window layout's window.
        ImVec2 at{settings.position.x + settings.width + gap, settings.position.y + (g_gui.m_header ? m_header_height : 60.f)};
        if (auto* menu = ImGui::FindWindowByName("###astra_menu"); menu && settings.layout == 1)
            at = {menu->Pos.x + menu->Size.x + gap, menu->Pos.y};
        const auto display = ImGui::GetIO().DisplaySize;
        // Stack under the panels still showing in this column; a full column continues in the next one.
        for (;;)
        {
            float bottom = at.y, right = at.x;
            for (const auto& name : m_panels)
            {
                const auto* other = ImGui::FindWindowByName(name.c_str());
                if (!other || !(other->Active || other->WasActive) || std::fabs(other->Pos.x - at.x) > 1.f)
                    continue;
                bottom = std::max(bottom, other->Pos.y + other->Size.y + stack_gap);
                right = std::max(right, other->Pos.x + other->Size.x);
            }
            if (bottom + min_room <= display.y || right + gap + min_room >= display.x)
                return {at.x, bottom};
            at.x = right + gap;
        }
    }

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

    void canvas::add_settings_tab_impl(std::uint32_t id, std::uint32_t input_id, std::function<void(regular_submenu*)> game_options)
    {
        add_tab_impl<regular_submenu>("Settings", id, [this, input_id, game_options = std::move(game_options)](regular_submenu* sub) {
            static constexpr std::array<const char*, 2> layouts{"List", "Window"};
            auto& window = m_config;
            sub->add_option<choose_option<const char*, int>>("Menu Layout", "Switch between keyboard list and clickable window.", &layouts, &window.layout, true);
            sub->add_option<choose_option<const char*, int>>("Theme", "Shared palette for both menu layouts.", &astra::preset_theme_names, &window.theme, true);
            if (!g_gui.m_banner_names.empty())
                sub->add_option<choose_option<const char*, int>>("Banner", "Change menu header banner.", &g_gui.m_banner_names, &window.banner, true, [] { g_gui.update_banner(); });
            sub->add_option<number_option<float>>("Transparency", "Opacity of the List and Window UI.", &window.transparency, 0.15f, 1.f, 0.05f, 2);
            sub->add_option<number_option<std::uint8_t>>("Accent Red", "Used by the Custom theme.", &window.accent.r, 0, 255);
            sub->add_option<number_option<std::uint8_t>>("Accent Green", "Used by the Custom theme.", &window.accent.g, 0, 255);
            sub->add_option<number_option<std::uint8_t>>("Accent Blue", "Used by the Custom theme.", &window.accent.b, 0, 255);
            sub->add_option<number_option<std::uint8_t>>("Custom Background R", "Custom theme background red channel.", &window.custom_background.r, 0, 255);
            sub->add_option<number_option<std::uint8_t>>("Custom Background G", "Custom theme background green channel.", &window.custom_background.g, 0, 255);
            sub->add_option<number_option<std::uint8_t>>("Custom Background B", "Custom theme background blue channel.", &window.custom_background.b, 0, 255);
            sub->add_option<number_option<std::uint8_t>>("Custom Panel R", "Custom theme panel red channel.", &window.custom_panel.r, 0, 255);
            sub->add_option<number_option<std::uint8_t>>("Custom Panel G", "Custom theme panel green channel.", &window.custom_panel.g, 0, 255);
            sub->add_option<number_option<std::uint8_t>>("Custom Panel B", "Custom theme panel blue channel.", &window.custom_panel.b, 0, 255);
            sub->add_option<number_option<std::uint8_t>>("Custom Text R", "Custom theme text red channel.", &window.custom_text.r, 0, 255);
            sub->add_option<number_option<std::uint8_t>>("Custom Text G", "Custom theme text green channel.", &window.custom_text.g, 0, 255);
            sub->add_option<number_option<std::uint8_t>>("Custom Text B", "Custom theme text blue channel.", &window.custom_text.b, 0, 255);
            sub->add_option<sub_option>("Input", nullptr, input_id);
            astra::control controls;
            controls.id = "astra_menu_controls";
            controls.label = "Menu controls";
            controls.description = "Change the menu hotkey and list navigation buttons.";
            controls.activate = [this] {
                m_key_settings.open(current_menu_keys());
                m_wait_keys_release = true;
            };
            sub->add_ui_option(std::move(controls));
            sub->add_option<number_option<float>>("List X Position", nullptr, &window.position.x, 0.f, 2000.f, 25.f, 2);
            sub->add_option<number_option<float>>("List Y Position", nullptr, &window.position.y, 0.f, 2000.f, 25.f, 2);
            sub->add_option<number_option<float>>("List Width", nullptr, &window.width, 0.f, 1000.f, 50.f, 2);
            sub->add_option<bool_option<bool>>("Draw Mouse", nullptr, &window.mouse_active);
            if (game_options)
                game_options(sub);
        });
        add_submenu_impl<regular_submenu>("Input", input_id, [this](regular_submenu* sub) {
            auto& window = m_config;
            sub->add_option<number_option<std::int32_t>>("Open Delay", nullptr, &window.open_delay, 10, 1000, 10, 0);
            sub->add_option<number_option<std::int32_t>>("Back Delay", nullptr, &window.back_delay, 10, 1000, 10, 0);
            sub->add_option<number_option<std::int32_t>>("Enter Delay", nullptr, &window.enter_delay, 10, 1000, 10, 0);
            sub->add_option<number_option<std::int32_t>>("Vertical Delay", nullptr, &window.vertical_delay, 10, 1000, 10, 0);
            sub->add_option<number_option<std::int32_t>>("Horizontal Delay", nullptr, &window.horizontal_delay, 10, 1000, 10, 0);
        });
    }

    void canvas::tick_impl()
    {
        std::lock_guard lock(m_mutex);
        track_config_changes();
        check_for_input_impl();
        handle_input_impl();
        m_mouse_enabled = wants_mouse();
        const bool menu_input = (m_opened || m_mouse_held) && m_mouse_enabled.load();
        // WantCaptureKeyboard is also set while a mouse button holds a widget, so only typing and the key
        // editor take the keyboard; the player keeps walking while clicking the menu.
        m_capture_mouse = menu_input && ImGui::GetIO().WantCaptureMouse;
        m_capture_keyboard = menu_input && (ImGui::GetIO().WantTextInput || m_key_settings.active());
        if (!m_opened || active_history().empty()) return;

        const bool window_layout = m_config.layout == 1;
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
        if (last_theme != m_config.theme)
        {
            m_astra_canvas.reset_theme();
            last_theme = m_config.theme;
        }
        std::uint64_t palette_key = static_cast<std::uint64_t>(m_config.theme);
        if (m_config.theme == 3)
        {
            const auto& bg = m_config.custom_background;
            const auto& panel = m_config.custom_panel;
            const auto& text = m_config.custom_text;
            const auto& accent = m_config.accent;
            palette_key = (palette_key << 32) ^ (static_cast<std::uint64_t>(bg.r) << 24) ^ (static_cast<std::uint64_t>(bg.g) << 16) ^ (static_cast<std::uint64_t>(bg.b) << 8) ^ (static_cast<std::uint64_t>(panel.r) << 4) ^ static_cast<std::uint64_t>(panel.g) ^ (static_cast<std::uint64_t>(panel.b) << 40) ^ (static_cast<std::uint64_t>(text.r) << 36) ^ (static_cast<std::uint64_t>(text.g) << 28) ^ (static_cast<std::uint64_t>(text.b) << 20) ^ (static_cast<std::uint64_t>(accent.r) << 12) ^ (static_cast<std::uint64_t>(accent.g) << 4) ^ static_cast<std::uint64_t>(accent.b);
        }
        static std::uint64_t last_palette = 0;
        if (last_palette != palette_key)
        {
            m_astra_canvas.reset_theme();
            last_palette = palette_key;
        }
        const auto theme = menu_theme();
        astra::list_style style;
        style.position = {m_config.position.x, m_config.position.y};
        style.width = m_config.width;
        style.rows = static_cast<int>(m_config.rows);
        style.banner = (ImTextureID)g_gui.m_header;
        style.banner_height = m_header_height;
        style.row_height = m_option_height;
        style.footer = "Astra | Build 1.0.0";
        style.mouse_enabled = m_mouse_enabled.load() && !m_key_settings.active();
        bool open = m_opened;
        astra::event event = m_astra_canvas.draw(
            window_layout ? astra::layout::window : astra::layout::list,
            GAME_NAME "###astra_menu", page, open, theme, style);
        set_opened(open);
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
        if (!open) m_key_settings.cancel();
        if (const auto keys = m_key_settings.draw(theme))
            m_config.keys = *keys;
    }

    std::optional<LRESULT> canvas::block_game_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        const bool mouse = captures_game_mouse(), keyboard = captures_game_keyboard();
        if (!mouse && !keyboard) return std::nullopt;
        // A release always reaches the game, so a key held before the menu took the keyboard never sticks.
        if (msg >= WM_KEYFIRST && msg <= WM_KEYLAST)
            return keyboard && msg != WM_KEYUP && msg != WM_SYSKEYUP ? std::optional<LRESULT>(0) : std::nullopt;
        if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)
            return mouse ? std::optional<LRESULT>(0) : std::nullopt;
        // Unity reads the wheel from pointer messages; DefWindowProc turns them into the WM_MOUSEWHEEL ImGui scrolls with.
        if (msg == WM_POINTERWHEEL || msg == WM_POINTERHWHEEL)
            return mouse ? std::optional<LRESULT>(DefWindowProcW(hwnd, msg, wparam, lparam)) : std::nullopt;
        if (msg != WM_INPUT) return std::nullopt;

        RAWINPUT raw{};
        UINT size = sizeof(raw);
        bool held = mouse || keyboard;
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1))
        {
            if (raw.header.dwType == RIM_TYPEMOUSE) held = mouse;
            if (raw.header.dwType == RIM_TYPEKEYBOARD) held = keyboard && !(raw.data.keyboard.Flags & RI_KEY_BREAK);
        }
        // Held raw input still goes to DefWindowProc, which frees it.
        return held ? std::optional<LRESULT>(DefWindowProcW(hwnd, msg, wparam, lparam)) : std::nullopt;
    }

    void canvas::set_opened(bool open)
    {
        if (open == m_opened)
            return;
        // While the menu is closed the hidden cursor keeps moving with mouse look, so it would come back wherever
        // that left it. It is moved before m_opened is set, while a host's SetCursorPos hook still lets calls through.
        const bool mouse = wants_mouse();
        if (POINT cursor{}; !open && uses_mouse() && GetCursorPos(&cursor))
            m_menu_cursor = cursor;
        else if (open && mouse && m_menu_cursor)
            SetCursorPos(m_menu_cursor->x, m_menu_cursor->y);
        m_opened = open;
    }

    void canvas::prepare_imgui_mouse()
    {
        auto& io = ImGui::GetIO();
        io.MouseDrawCursor = uses_mouse();
        // A closed menu also leaves the OS cursor to the game. ImGui's Win32 backend would otherwise set an arrow on
        // the first frame without its own cursor and on every WM_SETCURSOR, and a game that sets its cursor only when
        // its own state changes, such as Ready or Not, keeps that arrow on screen.
        constexpr auto mouse_flags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
        if (io.MouseDrawCursor)
            io.ConfigFlags &= ~mouse_flags;
        else
            io.ConfigFlags |= mouse_flags;
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

        if (!m_key_settings.active() && m_wait_keys_release)
        {
            for (auto key : current_menu_keys())
                if (platform_is_key_pressed(key)) return;
            m_wait_keys_release = false;
            m_open_was_down = false;
        }

		const bool open_down = platform_is_key_pressed(m_config.keys[0]) || platform_is_controller_pressed(XINPUT_GAMEPAD_BACK);
        m_open_key_pressed = open_down && !m_open_was_down;
        m_open_was_down = open_down;
		m_back_key_pressed = platform_is_key_pressed(m_config.keys[1]) || platform_is_controller_pressed(XINPUT_GAMEPAD_B);
		m_enter_key_pressed = platform_is_key_pressed(m_config.keys[2]) || platform_is_controller_pressed(XINPUT_GAMEPAD_A);
		m_up_key_pressed = platform_is_key_pressed(m_config.keys[3]) || platform_is_controller_pressed(XINPUT_GAMEPAD_DPAD_UP);
		m_down_key_pressed = platform_is_key_pressed(m_config.keys[4]) || platform_is_controller_pressed(XINPUT_GAMEPAD_DPAD_DOWN);
		m_left_key_pressed = platform_is_key_pressed(m_config.keys[5]) || platform_is_controller_pressed(XINPUT_GAMEPAD_DPAD_LEFT);
		m_right_key_pressed = platform_is_key_pressed(m_config.keys[6]) || platform_is_controller_pressed(XINPUT_GAMEPAD_DPAD_RIGHT);
		m_left_tab_pressed = platform_is_key_pressed(m_config.keys[7]) || (platform_is_controller_pressed(XINPUT_GAMEPAD_LEFT_SHOULDER));
		m_right_tab_pressed = platform_is_key_pressed(m_config.keys[8]) || platform_is_controller_pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER);
	}

	void canvas::handle_input_impl()
	{
        if (m_key_settings.active() || m_wait_keys_release) return;
		static Timer openTimer(0ms);
		openTimer.SetDelay(std::chrono::milliseconds(m_config.open_delay));
		if (m_open_key_pressed && openTimer.Update())
		{
			set_opened(!m_opened);

			if (m_config.sounds)
				play_sound(m_opened ? "SELECT" : "BACK");
		}

		if (ImGui::GetIO().WantTextInput || m_config.layout == 1) return;

		static Timer backTimer(0ms);
		backTimer.SetDelay(std::chrono::milliseconds(m_config.back_delay));
		if (m_opened && m_back_key_pressed && backTimer.Update())
		{
			if (m_config.sounds)
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
			enterTimer.SetDelay(std::chrono::milliseconds(m_config.enter_delay));
			if (m_enter_key_pressed && sub->get_num_option() != 0 && enterTimer.Update())
			{
				if (m_config.sounds)
					play_sound("SELECT");

				if (const auto opt = sub->get_option(sub->get_selected_option()))
					opt->handle_action(OptionAction::EnterPress);
				return;
			}

			static Timer upTimer(0ms);
			upTimer.SetDelay(std::chrono::milliseconds(m_config.vertical_delay));
			if (m_up_key_pressed && sub->get_num_option() != 0 && upTimer.Update())
			{
				if (m_config.sounds)
					play_sound("NAV_UP_DOWN");

				sub->ScrollBackward();
			}

			static Timer downTimer(0ms);
			downTimer.SetDelay(std::chrono::milliseconds(m_config.vertical_delay));
			if (m_down_key_pressed && sub->get_num_option() != 0 && downTimer.Update())
			{
				if (m_config.sounds)
					play_sound("NAV_UP_DOWN");

				sub->ScrollForward();
			}

			static Timer leftTimer(0ms);
			leftTimer.SetDelay(std::chrono::milliseconds(m_config.horizontal_delay));
			if (m_left_key_pressed && sub->get_num_option() != 0 && leftTimer.Update())
			{
				if (m_config.sounds)
					play_sound("NAV_LEFT_RIGHT");

				if (const auto opt = sub->get_option(sub->get_selected_option()))
					opt->handle_action(OptionAction::LeftPress);
			}

			static Timer rightTimer(0ms);
			rightTimer.SetDelay(std::chrono::milliseconds(m_config.horizontal_delay));
			if (m_right_key_pressed && sub->get_num_option() != 0 && rightTimer.Update())
			{
				if (m_config.sounds)
					play_sound("NAV_LEFT_RIGHT");

				if (const auto opt = sub->get_option(sub->get_selected_option()))
					opt->handle_action(OptionAction::RightPress);
			}

			static Timer tabSwitchTimer(0ms);
			tabSwitchTimer.SetDelay(std::chrono::milliseconds(m_config.tab_delay)); // Delay between switches

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
