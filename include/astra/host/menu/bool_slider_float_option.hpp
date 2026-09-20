#pragma once
#include <astra/host/menu/base_option.hpp>
#include "fiber_pool.hpp"

#include "commands/commands.hpp"
#include "commands/bool_command.hpp"
#include "commands/float_command.hpp"


namespace big
{
	class bool_slider_float_option : public base_option<bool_slider_float_option>
	{
	public:
		explicit bool_slider_float_option(const char* text, const char* description, bool* b00l, float* number, float min, float max, float step = 1.f, std::size_t precision = 3, bool actionOnHorizontal = true, std::function<void()> action = [] {}) :
			m_bool(b00l),
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
		}
		explicit bool_slider_float_option(uint32_t id_bool, uint32_t id_float, float step = 1.f, std::size_t precision = 3, bool actionOnHorizontal = true, std::function<void()> action = [] {}) :
			m_bool_command(commands::get_command<bool_command>(id_bool)),
			m_float_command(commands::get_command<float_command>(id_float)),
			m_action_on_horizontal(actionOnHorizontal),
			m_step(step),
			m_precision(precision)
		{
			if (!m_bool_command || !m_float_command)
			{
				LOG(FATAL) << "Command " << id_bool << " or " << id_float << " not found.";

				return;
			}

			m_min = m_float_command->get_minimum();
			m_max = m_float_command->get_maximum();

			auto& description = m_bool_command->get_description();
			auto& text = m_bool_command->get_label();

			Base::set_left_text(text.c_str());
			if (!description.empty())
				Base::set_description(description.c_str());
			Base::set_action(std::move(action));
		}

		~bool_slider_float_option() noexcept = default;
		bool_slider_float_option(bool_slider_float_option const&) = default;
		bool_slider_float_option& operator=(bool_slider_float_option const&) = default;
		bool_slider_float_option(bool_slider_float_option&&) = default;
		bool_slider_float_option& operator=(bool_slider_float_option&&) = default;

		float get_float() override
		{
			if (m_float_command)
				return m_float_command->get_state();

			return m_number ? *m_number : 0.f;
		}

		float get_min_float() override
		{
			return m_min;
		}

		float get_max_float() override
		{
			return m_max;
		}

		void handle_action(OptionAction action) override
		{
			if (action == OptionAction::EnterPress)
			{
				if (m_bool_command)
					m_bool_command->set_state(!m_bool_command->get_state());
				else
					*m_bool = !*m_bool;
				
			}
			else if (action == OptionAction::LeftPress)
			{
				if (m_float_command)
				{
					auto min = m_float_command->get_minimum();
					auto max = m_float_command->get_maximum();

					if (m_float_command->get_state() - m_step < min)
						m_float_command->set_state(max);
					else
						m_float_command->set_state(m_float_command->get_state() - m_step);

					if (m_action_on_horizontal && Base::m_action)
						std::invoke(Base::m_action);
				}
				else
				{
					if (*m_number - m_step < m_min)
						*m_number = m_max;
					else
						*m_number -= m_step;

					if (m_action_on_horizontal && Base::m_action)
						std::invoke(Base::m_action);
				}
			}
			else if (action == OptionAction::RightPress)
			{
				if (m_float_command)
				{
					auto min = m_float_command->get_minimum();
					auto max = m_float_command->get_maximum();

					if (m_float_command->get_state() + m_step > max)
						m_float_command->set_state(min);
					else
						m_float_command->set_state(m_float_command->get_state() + m_step);
				}
				else
				{
					if (*m_number + m_step > m_max)
						*m_number = m_min;
					else
						*m_number += m_step;
				}

				if (m_action_on_horizontal && Base::m_action)
					std::invoke(Base::m_action);
			}

			Base::handle_action(action);
		}

		bool get_flag(OptionFlag flag) override
        {
            return flag == OptionFlag::BoolSliderFloat || Base::get_flag(flag);
        }

        astra::control describe_ui() override
        {
            auto c = Base::describe_ui();
            c.kind = astra::control_kind::toggle_number;
            c.checked = m_bool_command ? m_bool_command->get_state() : (m_bool && *m_bool);
            c.value = m_float_command ? m_float_command->get_state() : (m_number ? *m_number : 0);
            c.minimum = m_min;
            c.maximum = m_max;
            c.step = m_step;
            c.integral = false;
            c.precision = static_cast<int>(m_precision);
            c.value_text = std::to_string(c.value);
            c.set_value = [this](double value) {
                auto next = static_cast<float>(astra::bounded_value(value, m_min, m_max, false));
                if (m_float_command) m_float_command->set_state(next);
                else if (m_number) *m_number = next;
                if (m_action_on_horizontal && Base::m_action) std::invoke(Base::m_action);
            };
            return c;
        }

	private:
		bool* m_bool{};
		bool m_action_on_horizontal{};
		float* m_number{};
		float m_min{};
		float m_max{};
		float m_step{};
		std::size_t m_precision{};
		bool_command* m_bool_command{ nullptr };
		float_command* m_float_command{ nullptr };
		using Base = base_option<bool_slider_float_option>;
	};
}
