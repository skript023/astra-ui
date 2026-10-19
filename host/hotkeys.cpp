#include <astra/host/hotkeys.hpp>
#include <astra/host/notification.hpp>
#include "commands/bool_command.hpp"
#include "commands/commands.hpp"
#include "utility/joaat.hpp"
#include <imgui.h>

namespace big
{
	// Registers before settings.json is read, so the bindings load with everything else.
	static hotkeys& g_hotkeys_registration = hotkeys::instance();

	hotkeys::hotkeys() :
	    state_serializer("hotkeys")
	{
	}

	void hotkeys::load_state_impl(nlohmann::json& state)
	{
		m_bindings.clear();
		for (auto& [name, value] : state.items())
		{
			if (!value.is_number_integer())
				continue;
			const auto vk = value.get<std::int64_t>();
			if (vk > 0 && vk < 255)
				m_bindings[name] = static_cast<std::uint32_t>(vk);
		}
	}

	void hotkeys::save_state_impl(nlohmann::json& state)
	{
		state = nlohmann::json::object();
		for (auto& [name, vk] : m_bindings)
			state[name] = vk;
	}

	void hotkeys::tick_impl(HWND window)
	{
		if (!window)
			return;

		const bool imgui_text_input = ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput;
		const bool accept = GetForegroundWindow() == window && !imgui_text_input && (GetAsyncKeyState(VK_MENU) & 0x8000) == 0;

		for (auto& [name, vk] : m_bindings)
		{
			const bool down = (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0;
			bool& was_down = m_was_down[name];
			if (down && !was_down && accept)
			{
				if (auto command = commands::get_bool_command(joaat(name)))
				{
					// set_state, not call: a game may run call() later on its script thread, and the card shows the new state.
					command->set_state(!command->get_state());
					notification::info(command->get_label(), command->get_state() ? "On" : "Off");
				}
			}
			was_down = down;
		}
	}
}
