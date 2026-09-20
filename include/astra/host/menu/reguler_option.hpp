#pragma once
#include <astra/host/menu/base_option.hpp>
#include "commands/commands.hpp"
#include "commands/bool_command.hpp"

namespace big
{
	class reguler_option : public base_option<reguler_option>
	{
	public:
		explicit reguler_option() = default;
		explicit reguler_option(const char* text, const char* description = nullptr, std::function<void()> action = [] {}): m_command(nullptr)
		{
			set_left_text(text);
			if (description)
				set_description(description);
			set_action(std::move(action));
		}

		explicit reguler_option(uint32_t cmd): 
			m_command(commands::get_command<bool_command>(cmd))
		{
			auto& text = m_command->get_label();
			auto& description = m_command->get_description();

			set_left_text(text.c_str());
			if (!description.empty())
				set_description(description.c_str());
			set_action([this] { m_command->call(); });
		}

		~reguler_option() noexcept = default;
		reguler_option(reguler_option const&) = default;
		reguler_option& operator=(reguler_option const&) = default;
		reguler_option(reguler_option&&) = default;
		reguler_option& operator=(reguler_option&&) = default;

		command* m_command;
	};
}
