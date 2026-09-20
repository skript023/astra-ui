#pragma once

#include <astra/menu.hpp>

namespace astra
{
    // Backend-independent menu canvas. Hosts provide a page snapshot and choose
    // layout settings; Astra owns the menu view and returns interaction events.
    class canvas
    {
        menu view_;

    public:
        event draw(layout mode, const char* id, const page& model, bool& open,
            const theme& colors, const list_style& list = {})
        {
            if (mode == layout::window)
                return view_.draw_window(id, model, open, colors);

            return view_.draw_list(model, colors, list);
        }

        void reset_theme() { view_.reset_theme(); }
    };
}
