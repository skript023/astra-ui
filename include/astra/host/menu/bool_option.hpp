#pragma once
#include <astra/host/menu/base_option.hpp>
#include "commands/commands.hpp"
#include "commands/bool_command.hpp"

namespace big
{
	template <typename BoolType = bool>
	class bool_option : public base_option<bool_option<BoolType>>
	{
	public:
		explicit bool_option(const char* text, const char* description, BoolType* b00l, std::function<void()> action = [] {}) :
			m_bool(b00l),
			m_command(nullptr)
		{
			Base::set_left_text(text);
			if (description)
				Base::set_description(description);
			Base::set_action(std::move(action));
		}
		explicit bool_option(uint32_t id, std::function<void()> action = [] {}) :
			m_bool(nullptr),
			m_command(commands::get_command<bool_command>(id))
		{
			auto description = m_command->get_description().c_str();
			auto text = m_command->get_label().c_str();

			Base::set_left_text(text);
			if (description)
				Base::set_description(description);
			Base::set_action(std::move(action));
		}

		~bool_option() noexcept = default;
		bool_option(bool_option const&) = default;
		bool_option& operator=(bool_option const&) = default;
		bool_option(bool_option&&) = default;
		bool_option& operator=(bool_option&&) = default;

		void handle_action(OptionAction action) override
		{
			if (action == OptionAction::EnterPress)
			{
				if (m_command)
				{
					m_command->set_state(!m_command->get_state());
				}
				else
				{
					*m_bool = !*m_bool;
				}
			}

			Base::handle_action(action);
		}

		bool get_flag(OptionFlag flag) override
        {
            return flag == OptionFlag::Toggle || Base::get_flag(flag);
        }

        astra::control describe_ui() override
        {
            auto c = Base::describe_ui();
            c.kind = astra::control_kind::toggle;
            c.checked = m_command ? m_command->get_state() : (m_bool && *m_bool);
            return c;
        }

	private:
		BoolType* m_bool;
		bool_command* m_command{ nullptr };
		using Base = base_option<bool_option<BoolType>>;
	};
}
