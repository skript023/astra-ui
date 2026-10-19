#pragma once
#include <astra/host/state_serializer.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <windows.h>

namespace big
{
	// Per-feature toggle keys from settings.json: "hotkeys": { "<command name>": <virtual-key code> }. The Astra
	// loader writes them; the mod only reads them, at start and whenever the file changes. A key switches its bool
	// command and shows an On or Off notification under the command's label.
	class hotkeys final : private state_serializer
	{
	public:
		static hotkeys& instance()
		{
			static hotkeys instance{};
			return instance;
		}

		// Call once a frame from the thread that switches features, the same one that calls settings::apply_external.
		// Keys count only while window is in front, ImGui is not taking text, and Alt is up, so typing and Alt+Tab
		// never switch a feature.
		static void tick(HWND window)
		{
			instance().tick_impl(window);
		}

	private:
		std::unordered_map<std::string, std::uint32_t> m_bindings;
		std::unordered_map<std::string, bool> m_was_down;

		hotkeys();
		void tick_impl(HWND window);
		virtual void save_state_impl(nlohmann::json& state) override;
		virtual void load_state_impl(nlohmann::json& state) override;
	};
}
