#pragma once
#include "common.hpp"
#include "class/vector.hpp"
#include "imgui.h"
#include "imgui_internal.h"

#include <astra/host/menu/abstract_submenu.hpp>
#include <astra/host/menu/tabs_menu.hpp>
#include <astra/canvas.hpp>
#include <astra/navigation.hpp>

namespace big
{
	class Timer
	{
	public:
		explicit Timer(std::chrono::milliseconds delay) :
			m_Timer(std::chrono::high_resolution_clock::now()),
			m_Delay(std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(delay))
		{
		}

		bool Update()
		{
			auto now = std::chrono::high_resolution_clock::now();
			if ((now.time_since_epoch() - m_Timer.time_since_epoch()).count() >= m_Delay.count())
			{
				m_Timer = now;
				return true;
			}

			return false;
		}

		void SetDelay(std::chrono::milliseconds delay)
		{
			m_Delay = delay;
		}
	private:
		std::chrono::high_resolution_clock::time_point m_Timer;
		std::chrono::high_resolution_clock::duration m_Delay;
	};

	class canvas
	{
		std::vector<std::unique_ptr<abstract_submenu>> m_all_tabs;
		astra::navigation<abstract_submenu*> m_navigation;
		astra::canvas m_astra_canvas;

		static canvas& instance()
		{
			static canvas i{};

			return i;
		}
	public:
		static void tick() { instance().tick_impl(); }
		static void check_for_input() { instance().check_for_input_impl(); }
		static void handle_input() { instance().handle_input_impl(); }
		template <typename SubmenuType, typename ...TArgs>
		static void add_submenu(TArgs&&... args) { instance().add_submenu_impl<SubmenuType>(std::forward<TArgs>(args)...); }
		template <typename TabmenuType, typename... Args>
		static void add_tab(Args&&... args) { instance().add_tab_impl<TabmenuType>(std::forward<Args>(args)...); }



		static void switch_to_submenu(std::uint32_t id) { instance().switch_to_submenu_impl(id); }
		static void switch_to_tabmenu(std::uint32_t id) { instance().switch_to_tabmenu_impl(id); }
		static bool is_opened() { return instance().m_opened; }
        static bool uses_mouse() { return is_opened() && instance().m_mouse_enabled.load(); }
        static bool captures_game_input() { return is_opened() && instance().m_capture_game.load(); }
        static bool captures_message(UINT msg);
		static void draw_line(float x1, float y1, float x2, float y2, Color color, float thickness) { instance().draw_line_impl(x1, y1, x2, y2, color, thickness); }
		static void draw_stroke_text(float x, float y, Color color, std::string_view str) { instance().draw_stroke_text_impl(x, y, color, str); }
		static void draw_filled_rect(float x, float y, float w, float h, Color color) { instance().draw_filled_rect_impl(x, y, w, h, color); }
		static void draw_circle_filled(float x, float y, float radius, Color color) { instance().draw_circle_filled_impl(x, y, radius, color); }
		static void draw_circle(float x, float y, float radius, Color color, int segments) { instance().draw_circle_impl(x, y, radius, color, segments); }
		static void draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, Color color, float thickness) { instance().draw_triangle_impl(x1, y1, x2, y2, x3, y3, color, thickness); }
		static void draw_triangle_filled(float x1, float y1, float x2, float y2, float x3, float y3, Color color) { instance().draw_triangle_filled_impl(x1, y1, x2, y2, x3, y3, color); }
		static void draw_corner_box(float x, float y, float w, float h, float borderPx, Color color) { instance().draw_corner_box_impl(x, y, w, h, borderPx, color); }
		static void draw_box(float x, float y, float w, float h, float borderPx, Color color) { instance().draw_box_impl(x, y, w, h, borderPx, color); }
		static void draw_box_outlined(float x, float y, float w, float h, float borderPx, Color color) { instance().draw_box_outlined_impl(x, y, w, h, borderPx, color); }
		static void draw_filled_box(float x, float y, float w, float h, float borderPx, Color color) { instance().draw_filled_box_impl(x, y, w, h, color); }
		static void draw_cube(ImVec2 const& screen_location, float yaw, ImVec2 const& size, Color colour) { instance().draw_cube_impl(screen_location, yaw, size, colour); }
		static ImVec2 rotate_point_2d(const ImVec2& point, const ImVec2& center, float yaw) { return instance().rotate_point_2d_impl(point, center, yaw); }
	private:
		explicit canvas() = default;
		~canvas() noexcept = default;

		canvas(canvas const&) = delete;
		canvas& operator=(canvas const&) = delete;

		canvas(canvas&&) = delete;
		canvas& operator=(canvas&&) = delete;

		template <typename SubmenuType, typename ...TArgs>
		void add_submenu_impl(TArgs&&... args)
		{
			auto sub = std::make_unique<SubmenuType>(std::forward<TArgs>(args)...);
			m_navigation.set_root(sub.get());

			m_all_submenu.push_back(std::move(sub));
		}

		template<typename TabmenuType, typename... Args>
		void add_tab_impl(Args&&... args)
		{
			auto tab = std::make_unique<TabmenuType>(std::forward<Args>(args)...);
			// register tab
			m_all_tabs.push_back(std::move(tab));

			m_navigation.add_tab(m_all_tabs.back().get());
		}

		void switch_to_submenu_impl(std::uint32_t id)
		{
			for (auto&& sub : m_all_submenu)
			{
				if (sub->get_id() == id)
				{
					m_navigation.push(sub.get());

					return;
				}
			}
		}

		void switch_to_tabmenu_impl(std::uint32_t id)
		{
			for (size_t i = 0; i < m_all_tabs.size(); ++i)
			{
				if (m_all_tabs[i]->get_id() == id)
				{
					m_navigation.select_tab(i);

					return;
				}
			}
		}

		const std::vector<abstract_submenu*>& active_history() const
		{
			return m_navigation.path();
		}

		static bool platform_is_key_pressed(int key);
        static bool platform_is_controller_pressed(int button);
        static Vector2 platform_resolution();
        static void platform_prepare_render();
        void tick_impl();
		void game_tick();
	public:
		void draw_stroke_text_impl(float x, float y, Color color, std::string_view str);
		void draw_filled_rect_impl(float x, float y, float w, float h, Color color);
		void draw_circle_filled_impl(float x, float y, float radius, Color color);
		void draw_circle_impl(float x, float y, float radius, Color color, int segments);
		void draw_triangle_impl(float x1, float y1, float x2, float y2, float x3, float y3, Color color, float thickne);
		void draw_triangle_filled_impl(float x1, float y1, float x2, float y2, float x3, float y3, Color color);
		void draw_line_impl(float x1, float y1, float x2, float y2, Color color, float thickness);
		void draw_corner_box_impl(float x, float y, float w, float h, float borderPx, Color color);
		void draw_box_impl(float x, float y, float w, float h, float thickness, Color color);
		void draw_filled_box_impl(float x, float y, float w, float h, Color color);
		void draw_box_outlined_impl(float x, float y, float w, float h, float thickness, Color color);
		void draw_cube_impl(ImVec2 const& screen_location, float yaw, ImVec2 const& size, Color colour);
		ImVec2 rotate_point_2d_impl(const ImVec2& point, const ImVec2& center, float yaw);
	public:
		std::mutex m_mutex;

		std::atomic_bool m_opened{true};
        std::atomic_bool m_mouse_enabled{false};
        std::atomic_bool m_capture_game{false};

        float m_header_height = 100.f;
        float m_option_height = 45.f;

		void check_for_input_impl();
		void handle_input_impl();
	private:
		bool m_open_key_pressed = false;
		bool m_open_was_down = false;
		bool m_back_key_pressed = false;
		bool m_enter_key_pressed = false;
		bool m_up_key_pressed = false;
		bool m_down_key_pressed = false;
		bool m_left_key_pressed = false;
		bool m_right_key_pressed = false;
		bool m_left_tab_pressed = false;
		bool m_right_tab_pressed = false;
		void reset_input();

		void draw_rect(float x, float y, float width, float height, Color color, ImDrawList* draw_list = ImGui::GetBackgroundDrawList());
		void draw_sprite(ImTextureID image, float x, float y, float width, float height, Color color, ImDrawList* drawlist = ImGui::GetBackgroundDrawList());
		void draw_sprite(D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle, float x, float y, float width, float height, Color color, ImDrawList* drawlist = ImGui::GetBackgroundDrawList());
		void draw_left_text(const char* text, float x, float y, Color color, ImFont* font, ImDrawList* draw_list = ImGui::GetForegroundDrawList());
		void draw_centered_text(const char* text, float x, float y, Color color, ImFont* font, ImDrawList* draw_list = ImGui::GetForegroundDrawList());
		void draw_right_text(const char* text, float x, float y, Color color, ImFont* font, ImDrawList* draw_list = ImGui::GetForegroundDrawList());
		Vector2 get_sprite_scale(float size);

		// Helpers
		ImRect get_rect(ImVec2 pos, ImVec2 size);
		void play_sound(const char* name);

		std::vector<std::unique_ptr<abstract_submenu>> m_all_submenu;

	};
}
