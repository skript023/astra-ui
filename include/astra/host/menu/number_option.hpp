#pragma once
#include <astra/host/menu/base_option.hpp>
#include "utility/utility.hpp"
#include "commands/commands.hpp"
#include "commands/number_command.hpp"

namespace big
{
	template <typename NumberType>
	class number_option : public base_option<number_option<NumberType>>
	{
	public:
		explicit number_option() = default;
		explicit number_option(const char* text, const char* description, NumberType* number, NumberType min, NumberType max, NumberType step = 1, std::size_t precision = 3, bool actionOnHorizontal = true, const char* prefix = "", const char* suffix = "", std::function<void()> action = [] {}) :
			m_action_on_horizontal(actionOnHorizontal),
			m_number(number),
			m_min(min),
			m_max(max),
			m_step(step),
			m_precision(precision)
		{
			Base::set_left_text(text);
			if (description)
				Base::set_description(description);
			Base::set_action(std::move(action));
			std::strncpy(&m_prefix[0], prefix, sizeof(m_prefix) - 1);
			std::strncpy(&m_suffix[0], suffix, sizeof(m_suffix) - 1);
		}
		explicit number_option(uint32_t id, NumberType step = 1, std::size_t precision = 3, bool actionOnHorizontal = true, const char* prefix = "", const char* suffix = "", std::function<void()> action = [] {}) :
			m_number(nullptr),
			m_action_on_horizontal(actionOnHorizontal),
			m_command(commands::get_command<number_command<NumberType>>(id)),
			m_step(step),
			m_precision(precision)
		{
			if (!m_command)
			{
				LOG(FATAL) << "Command " << id << " not found.";

				return;
			}

			m_min = m_command->get_minimum();
			m_max = m_command->get_maximum();

			auto& description = m_command->get_description();
			auto& text = m_command->get_label();

			Base::set_left_text(text.c_str());
			if (!description.empty())
				Base::set_description(description.c_str());
			Base::set_action(std::move(action));
			std::strncpy(&m_prefix[0], prefix, sizeof(m_prefix) - 1);
			std::strncpy(&m_suffix[0], suffix, sizeof(m_suffix) - 1);
		}

		~number_option() noexcept = default;
		number_option(number_option const&) = default;
		number_option& operator=(number_option const&) = default;
		number_option(number_option&&) = default;
		number_option& operator=(number_option&&) = default;

		const char* get_right_text() override
		{
			MemoryStringStream stream(Base::m_right_text);
			
			if (m_command)
			{
				stream
					<< std::setprecision(m_precision)
					<< std::fixed
					<< m_prefix
					<< m_command->get_state()
					<< m_suffix;
			}
			else
			{
				stream
					<< std::setprecision(m_precision)
					<< std::fixed
					<< m_prefix
					<< static_cast<DisplayType>(*m_number)
					<< m_suffix;
			}

			return Base::get_right_text();
		}

		void handle_action(OptionAction action) override
		{
			if (m_command)
			{
				auto number = m_command->get_state();

				if (action == OptionAction::LeftPress)
				{
					if (number - m_step < m_min)
						m_command->set_state(m_max);
					else
						m_command->set_state(number - m_step);

					if (m_action_on_horizontal && Base::m_action)
						std::invoke(Base::m_action);
				}
				else if (action == OptionAction::RightPress)
				{
					if (number + m_step > m_max)
						m_command->set_state(m_min);
					else
						m_command->set_state(number + m_step);

					if (m_action_on_horizontal && Base::m_action)
						std::invoke(Base::m_action);
				}
			}
			else
			{
				if (action == OptionAction::LeftPress)
				{
					if (*m_number - m_step < m_min)
						*m_number = m_max;
					else
						*m_number -= m_step;

					if (m_action_on_horizontal && Base::m_action)
						std::invoke(Base::m_action);
				}
				else if (action == OptionAction::RightPress)
				{
					if (*m_number + m_step > m_max)
						*m_number = m_min;
					else
						*m_number += m_step;

					if (m_action_on_horizontal && Base::m_action)
						std::invoke(Base::m_action);
				}
			}

			Base::handle_action(action);
		}

        astra::control describe_ui() override
        {
            auto c = Base::describe_ui();
            c.kind = astra::control_kind::number;
            c.value = m_command ? m_command->get_state() : (m_number ? *m_number : NumberType{});
            c.minimum = m_min;
            c.maximum = m_max;
            c.step = m_step;
            c.integral = std::is_integral_v<NumberType>;
            c.precision = static_cast<int>(m_precision);
            c.set_value = [this](double value) {
                auto next = static_cast<NumberType>(astra::bounded_value(value, m_min, m_max, std::is_integral_v<NumberType>));
                if (m_command) m_command->set_state(next);
                else if (m_number) *m_number = next;
                if (m_action_on_horizontal && Base::m_action) std::invoke(Base::m_action);
            };
            return c;
        }

	private:
		char m_prefix[32] = {};
		char m_suffix[32] = {};
		bool m_action_on_horizontal{};

		NumberType* m_number{};
		NumberType m_min{};
		NumberType m_max{};
		NumberType m_step{};
		std::size_t m_precision{};
		number_command<NumberType>* m_command{ nullptr };

		using Base = base_option<number_option<NumberType>>;
		using DisplayType = std::conditional_t<sizeof(NumberType) == 1, std::uint32_t, NumberType>;
	};
}
