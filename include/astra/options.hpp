#pragma once

#include <astra/host_option.hpp>

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace astra
{
    // A host-independent option. Hosts bind their own command system by passing
    // getters/setters/actions; this package never includes a project's commands.
    class bound_option : public abstract_option
    {
    public:
        explicit bound_option(control value)
            : m_control(std::move(value))
        {
        }

        const char* get_left_text() override { return m_control.label.c_str(); }
        const char* get_right_text() override { return m_control.value_text.c_str(); }
        int get_integer() override { return static_cast<int>(m_control.value); }
        int get_min_integer() override { return static_cast<int>(m_control.minimum); }
        int get_max_integer() override { return static_cast<int>(m_control.maximum); }
        float get_float() override { return static_cast<float>(m_control.value); }
        float get_min_float() override { return static_cast<float>(m_control.minimum); }
        float get_max_float() override { return static_cast<float>(m_control.maximum); }
        const char* get_description() override { return m_control.description.c_str(); }

        void handle_action(option_action action) override
        {
            if (m_read_value)
                m_control.value = m_read_value();
            if (m_read_checked)
                m_control.checked = m_read_checked();
            if (m_read_choice)
                m_control.choice = m_read_choice();

            if (action == option_action::EnterPress)
            {
                if (m_control.kind == control_kind::choice && !m_control.choices.empty() && m_control.set_choice)
                    m_control.set_choice((m_control.choice + 1) % static_cast<int>(m_control.choices.size()));
                else if (m_control.kind == control_kind::toggle && m_control.set_value)
                    m_control.set_value(m_control.checked ? 0.0 : 1.0);
                else if (m_control.activate)
                    m_control.activate();
                return;
            }

            if (m_control.kind == control_kind::choice && !m_control.choices.empty() && m_control.set_choice)
            {
                const int count = static_cast<int>(m_control.choices.size());
                const int delta = action == option_action::LeftPress ? count - 1 : 1;
                m_control.set_choice((m_control.choice + delta) % count);
                return;
            }
            if ((m_control.kind != control_kind::number && m_control.kind != control_kind::toggle_number) || !m_control.set_value)
                return;

            const double direction = action == option_action::LeftPress ? -1.0 : 1.0;
            const auto value = bounded_value(m_control.value + direction * m_control.step,
                m_control.minimum, m_control.maximum, m_control.integral);
            m_control.set_value(value);
        }

        bool get_flag(option_flag flag) override
        {
            if (flag == option_flag::Toggle)
                return m_control.kind == control_kind::toggle || m_control.kind == control_kind::toggle_number;
            if (flag == option_flag::Enterable)
                return m_control.kind == control_kind::submenu;
            if (flag == option_flag::Horizontal)
                return m_control.kind == control_kind::number || m_control.kind == control_kind::toggle_number || m_control.kind == control_kind::choice;
            return false;
        }

        control describe_ui() override
        {
            // Refresh live values so command-backed options reflect external changes.
            if (m_read_value)
                m_control.value = m_read_value();
            if (m_read_checked)
                m_control.checked = m_read_checked();
            if (m_read_choice)
                m_control.choice = m_read_choice();
            if (!m_control.activate)
                m_control.activate = [this] { handle_action(option_action::EnterPress); };
            return m_control;
        }

        static bound_option action(std::string label, std::string description, std::function<void()> invoke)
        {
            control c;
            c.kind = control_kind::action;
            c.label = std::move(label);
            c.description = std::move(description);
            c.activate = std::move(invoke);
            return bound_option(std::move(c));
        }

        static bound_option submenu(std::string label, std::string description, std::function<void()> enter)
        {
            control c;
            c.kind = control_kind::submenu;
            c.label = std::move(label);
            c.description = std::move(description);
            c.activate = std::move(enter);
            return bound_option(std::move(c));
        }

        static bound_option toggle(std::string label, std::string description,
            std::function<bool()> read, std::function<void(bool)> write)
        {
            control c;
            c.kind = control_kind::toggle;
            c.label = std::move(label);
            c.description = std::move(description);
            c.checked = read ? read() : false;
            c.set_value = [write = std::move(write)](double v) { if (write) write(v != 0.0); };
            auto option = bound_option(std::move(c));
            option.m_read_checked = std::move(read);
            return option;
        }

        static bound_option toggle_number(std::string label, std::string description,
            std::function<bool()> read_checked, std::function<void(bool)> write_checked,
            std::function<double()> read_value, std::function<void(double)> write_value,
            double minimum, double maximum, double step = 1.0, bool integral = false, int precision = 2)
        {
            control c;
            c.kind = control_kind::toggle_number;
            c.label = std::move(label);
            c.description = std::move(description);
            c.checked = read_checked ? read_checked() : false;
            c.value = read_value ? read_value() : 0.0;
            c.minimum = minimum;
            c.maximum = maximum;
            c.step = step;
            c.integral = integral;
            c.precision = precision;
            c.activate = [read_checked, write_checked] { if (write_checked) write_checked(!(read_checked && read_checked())); };
            c.set_value = [write_value, minimum, maximum, integral](double v) { if (write_value) write_value(bounded_value(v, minimum, maximum, integral)); };
            auto option = bound_option(std::move(c));
            option.m_read_checked = std::move(read_checked);
            option.m_read_value = std::move(read_value);
            return option;
        }

        static bound_option choice(std::string label, std::string description, std::vector<std::string> choices,
            std::function<int()> read, std::function<void(int)> write)
        {
            control c;
            c.kind = control_kind::choice;
            c.label = std::move(label);
            c.description = std::move(description);
            c.choices = std::move(choices);
            c.choice = read ? read() : 0;
            c.set_choice = [write = std::move(write), count = static_cast<int>(c.choices.size())](int value) {
                if (write && count > 0) write(std::clamp(value, 0, count - 1));
            };
            c.activate = [] {};
            auto option = bound_option(std::move(c));
            option.m_read_choice = std::move(read);
            return option;
        }
        template <typename T>
        static bound_option number(std::string label, std::string description,
            std::function<T()> read, std::function<void(T)> write,
            T minimum, T maximum, T step = T{1}, int precision = 2)
        {
            control c;
            c.kind = control_kind::number;
            c.label = std::move(label);
            c.description = std::move(description);
            c.minimum = static_cast<double>(minimum);
            c.maximum = static_cast<double>(maximum);
            c.step = static_cast<double>(step);
            c.integral = std::is_integral_v<T>;
            c.precision = precision;
            c.value = read ? static_cast<double>(read()) : 0.0;
            c.set_value = [write = std::move(write), minimum, maximum](double v) {
                if (write)
                    write(static_cast<T>(bounded_value(v, static_cast<double>(minimum),
                        static_cast<double>(maximum), std::is_integral_v<T>)));
            };
            auto option = bound_option(std::move(c));
            option.m_read_value = [read = std::move(read)] { return read ? static_cast<double>(read()) : 0.0; };
            return option;
        }

    private:
        control m_control;
        std::function<double()> m_read_value;
        std::function<bool()> m_read_checked;
        std::function<int()> m_read_choice;
    };

    template <typename Binding>
    class control_option final : public bound_option
    {
    public:
        template <typename... Args>
        explicit control_option(Args&&... args)
            : bound_option(Binding{}(std::forward<Args>(args)...))
        {
        }
    };
    template <typename T>
    bound_option bind_value(std::string label, std::string description, T* value,
        T minimum, T maximum, T step = T{1}, int precision = 2)
    {
        return bound_option::number<T>(std::move(label), std::move(description),
            [value] { return value ? *value : T{}; },
            [value](T next) { if (value) *value = next; }, minimum, maximum, step, precision);
    }

    inline bound_option bind_toggle(std::string label, std::string description, bool* value)
    {
        return bound_option::toggle(std::move(label), std::move(description),
            [value] { return value && *value; },
            [value](bool next) { if (value) *value = next; });
    }
}
