#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace astra
{
    enum class layout { list, window };
    enum class control_kind { action, submenu, toggle, number, toggle_number, choice };
    // Whether what a row stands for has happened yet, such as an objective or a one-off action. The List shows it as
    // a badge at the right of the row, before the row's own value; none shows nothing.
    enum class status_kind { none, pending, active, done, failed };
    struct control
    {
        std::string id, label, description, value_text;
        control_kind kind = control_kind::action;
        status_kind status = status_kind::none;
        std::string status_text; // Replaces the badge's word: Not yet, In progress, Done, or Failed.
        bool checked = false;
        bool integral = false;
        double value = 0, minimum = 0, maximum = 1, step = 1;
        int precision = 2;
        int choice = 0;
        std::vector<std::string> choices;
        std::function<void()> activate;
        std::function<void(double)> set_value;
        std::function<void(int)> set_choice;
        std::function<void()> draw_details;
        // Window-only inline submenu contents; list navigation still uses activate.
        std::function<std::vector<control>()> children;
    };
    inline double bounded_value(double value, double minimum, double maximum, bool integral)
    {
        if (!std::isfinite(value)) value = minimum;
        if (maximum < minimum) std::swap(minimum, maximum);
        value = std::clamp(value, minimum, maximum);
        return integral ? std::clamp(std::round(value), minimum, maximum) : value;
    }
    struct page
    {
        std::string id, title;
        std::vector<std::string> tabs, breadcrumbs;
        std::vector<control> controls;
        std::size_t selected_tab = 0, selected_option = 0;
    };
    enum class event_kind { none, tab, back, breadcrumb, option };
    struct event
    {
        event_kind kind = event_kind::none;
        std::size_t index = 0;
    };
}
