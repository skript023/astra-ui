#pragma once
#include "common.hpp"
#include "class/vector.hpp"
#include "utility/utility.hpp"

#include <astra/key_settings.hpp>
#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace big
{
	// What the player sets up in the menu, saved as the "menu" object of settings.json. The canvas owns it; read and
	// change it through canvas::config(). Values that only last for one session do not belong here.
	struct menu_config
	{
		int layout = 0; // 0: List, 1: Window
		int theme = 4;  // Index into astra::preset_theme_names.
		int banner = 0; // Index into astra::banners().
		float transparency = 1.f;

		// The Custom theme (index 3) palette.
		Color custom_background{8, 12, 20, 255};
		Color custom_panel{15, 24, 38, 255};
		Color custom_text{225, 235, 250, 255};
		Color accent{255, 255, 255, 200};

		// Open, back, enter, up, down, left, right, previous tab, next tab: the order of astra::key_binding_labels.
		astra::key_bindings keys{VK_INSERT, VK_NUMPAD0, VK_NUMPAD5, VK_NUMPAD8, VK_NUMPAD2, VK_NUMPAD4, VK_NUMPAD6, VK_NUMPAD7, VK_NUMPAD9};

		Vector2 position{25.f, 25.f};
		float width = 450.f;
		std::size_t rows = 11;
		bool mouse_active = false;
		bool sounds = true;

		// Milliseconds between repeats while a List key is held.
		std::int32_t open_delay = 200;
		std::int32_t back_delay = 300;
		std::int32_t enter_delay = 300;
		std::int32_t vertical_delay = 120;
		std::int32_t horizontal_delay = 120;
		std::int32_t tab_delay = 200;

		int open_key() const
		{
			return keys[0];
		}
	};

	namespace menu_config_detail
	{
		inline constexpr std::array<const char*, 9> key_names{"open", "back", "enter", "up", "down", "left", "right", "previous_tab", "next_tab"};

		// A missing or mistyped value keeps its default instead of resetting the whole object.
		template <typename T>
		void read(const nlohmann::json& j, const char* key, T& value)
		{
			if (const auto it = j.find(key); it != j.end())
			{
				try
				{
					value = it->get<T>();
				}
				catch (const nlohmann::json::exception&)
				{
				}
			}
		}
	}

	inline void to_json(nlohmann::json& j, const menu_config& config)
	{
		auto keys = nlohmann::json::object();
		for (std::size_t i = 0; i < config.keys.size(); ++i)
			keys[menu_config_detail::key_names[i]] = config.keys[i];

		j = {
		    {"layout", config.layout},
		    {"theme", config.theme},
		    {"banner", config.banner},
		    {"transparency", config.transparency},
		    {"custom_background", config.custom_background},
		    {"custom_panel", config.custom_panel},
		    {"custom_text", config.custom_text},
		    {"accent", config.accent},
		    {"keys", std::move(keys)},
		    {"position", config.position},
		    {"width", config.width},
		    {"rows", config.rows},
		    {"mouse_active", config.mouse_active},
		    {"sounds", config.sounds},
		    {"open_delay", config.open_delay},
		    {"back_delay", config.back_delay},
		    {"enter_delay", config.enter_delay},
		    {"vertical_delay", config.vertical_delay},
		    {"horizontal_delay", config.horizontal_delay},
		    {"tab_delay", config.tab_delay},
		};
	}

	inline void from_json(const nlohmann::json& j, menu_config& config)
	{
		using menu_config_detail::read;
		if (!j.is_object())
			return;

		read(j, "layout", config.layout);
		read(j, "theme", config.theme);
		read(j, "banner", config.banner);
		read(j, "transparency", config.transparency);
		read(j, "custom_background", config.custom_background);
		read(j, "custom_panel", config.custom_panel);
		read(j, "custom_text", config.custom_text);
		read(j, "accent", config.accent);
		if (const auto keys = j.find("keys"); keys != j.end() && keys->is_object())
			for (std::size_t i = 0; i < config.keys.size(); ++i)
				read(*keys, menu_config_detail::key_names[i], config.keys[i]);
		read(j, "position", config.position);
		read(j, "width", config.width);
		read(j, "rows", config.rows);
		read(j, "mouse_active", config.mouse_active);
		read(j, "sounds", config.sounds);
		read(j, "open_delay", config.open_delay);
		read(j, "back_delay", config.back_delay);
		read(j, "enter_delay", config.enter_delay);
		read(j, "vertical_delay", config.vertical_delay);
		read(j, "horizontal_delay", config.horizontal_delay);
		read(j, "tab_delay", config.tab_delay);
	}
}

