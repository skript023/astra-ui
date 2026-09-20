#pragma once

#include <astra/model.hpp>

namespace astra
{
    enum class option_action
    {
        LeftPress,
        RightPress,
        EnterPress
    };

    enum class option_flag
    {
        Horizontal = (1 << 0),
        Enterable = (1 << 1),
        Toggle = (1 << 2),
        BoolSliderInt = (1 << 3),
        BoolSliderFloat = (1 << 4),
        SidePanel = (1 << 5)
    };

    class abstract_option
    {
    public:
        virtual ~abstract_option() noexcept = default;
        virtual const char* get_left_text() = 0;
        virtual const char* get_right_text() = 0;
        virtual int get_integer() = 0;
        virtual int get_min_integer() = 0;
        virtual int get_max_integer() = 0;
        virtual float get_float() = 0;
        virtual float get_min_float() = 0;
        virtual float get_max_float() = 0;
        virtual const char* get_description() = 0;
        virtual void handle_action(option_action action) = 0;
        virtual bool get_flag(option_flag flag) = 0;
        virtual void draw_side_panel() {}
        virtual control describe_ui()
        {
            control c;
            c.label = get_left_text();
            c.description = get_description();
            c.value_text = get_right_text();
            if (get_flag(option_flag::Enterable))
                c.kind = control_kind::submenu;
            c.activate = [this] { handle_action(option_action::EnterPress); };
            return c;
        }
    protected:
        abstract_option() = default;
    };
}