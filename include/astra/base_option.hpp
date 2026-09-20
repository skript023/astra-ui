#pragma once

#include <astra/host_option.hpp>
#include <cstring>
#include <functional>
#include <utility>

namespace astra
{
    template <typename Derived>
    class base_option : public abstract_option
    {
    public:
        const char* get_left_text() override { return m_left_text; }
        const char* get_right_text() override { return m_right_text; }
        const char* get_description() override { return m_description; }
        int get_integer() override { return m_integer; }
        int get_min_integer() override { return m_min_integer; }
        int get_max_integer() override { return m_max_integer; }
        float get_float() override { return m_float; }
        float get_min_float() override { return m_min_float; }
        float get_max_float() override { return m_max_float; }
        void handle_action(option_action action) override
        {
            if (action == option_action::EnterPress && m_action)
                m_action();
        }
        bool get_flag(option_flag) override { return false; }
        Derived& set_left_text(const char* text) { copy(m_left_text, text); return derived(); }
        Derived& set_right_text(const char* text) { copy(m_right_text, text); return derived(); }
        Derived& set_description(const char* text) { copy(m_description, text); return derived(); }
        Derived& set_integer(int value) { m_integer = value; return derived(); }
        Derived& set_min_integer(int value) { m_min_integer = value; return derived(); }
        Derived& set_max_integer(int value) { m_max_integer = value; return derived(); }
        Derived& set_float(float value) { m_float = value; return derived(); }
        Derived& set_min_float(float value) { m_min_float = value; return derived(); }
        Derived& set_max_float(float value) { m_max_float = value; return derived(); }
        Derived& set_action(std::function<void()> action) { m_action = std::move(action); return derived(); }
    protected:
        base_option() = default;
        ~base_option() override = default;
        static void copy(char (&target)[64], const char* source)
        {
            if (!source) { target[0] = '\0'; return; }
            std::strncpy(target, source, sizeof(target) - 1);
            target[sizeof(target) - 1] = '\0';
        }
        Derived& derived() { return static_cast<Derived&>(*this); }
        char m_left_text[64]{};
        char m_right_text[64]{};
        char m_description[64]{};
        int m_integer{};
        int m_min_integer{};
        int m_max_integer{};
        float m_float{};
        float m_min_float{};
        float m_max_float{};
        std::function<void()> m_action;
    };
}