#include <astra/menu.hpp>
#include <imgui_internal.h>
#include <cfloat>
#include <cstring>

namespace astra
{
    void menu::reset_theme()
    {
        theme_initialized_ = false;
    }

    void menu::keep_windows_out()
    {
        if (area_frame_ != ImGui::GetFrameCount() - 1)
        {
            kept_positions_.clear();
            return;
        }
        constexpr float gap = 8.f;
        constexpr ImGuiWindowFlags fixed = ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoMouseInputs;
        // The menu with the gap around it; no window may enter it.
        const ImRect area{{area_min_.x - gap, area_min_.y - gap}, {area_max_.x + gap, area_max_.y + gap}};
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        std::unordered_map<ImGuiID, ImVec2> kept;
        for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
        {
            if (!window->WasActive || (window->Flags & fixed) || window->ID == area_window_ || std::strstr(window->Name, "###astra_key_settings"))
                continue;
            const ImVec2 size = window->Size;
            const auto box_at = [&](ImVec2 at) { return ImRect{at, {at.x + size.x, at.y + size.y}}; };
            ImVec2 pos = window->Pos;
            if (box_at(pos).Overlaps(area))
            {
                const auto last = kept_positions_.find(window->ID);
                if (last != kept_positions_.end() && !box_at(last->second).Overlaps(area))
                {
                    // Coming from outside, as when dragged: each axis of the move stops on its own at the edge the
                    // window came from, so it slides along the menu instead of jumping to another side.
                    const ImVec2 from = last->second;
                    ImVec2 next{pos.x, from.y};
                    if (box_at(next).Overlaps(area))
                        next.x = from.x + size.x <= area.Min.x ? area.Min.x - size.x : area.Max.x;
                    next.y = pos.y;
                    if (box_at(next).Overlaps(area))
                        next.y = from.y + size.y <= area.Min.y ? area.Min.y - size.y : area.Max.y;
                    pos = next;
                }
                else
                {
                    // Already on the menu: the nearest side that keeps the whole window on screen, or the nearest side.
                    const ImVec2 spots[] = {
                        {area.Max.x, pos.y},
                        {area.Min.x - size.x, pos.y},
                        {pos.x, area.Max.y},
                        {pos.x, area.Min.y - size.y},
                    };
                    ImVec2 best = spots[0];
                    float best_distance = FLT_MAX;
                    bool best_fits = false;
                    for (const auto& spot : spots)
                    {
                        const bool fits = spot.x >= 0.f && spot.y >= 0.f && spot.x + size.x <= display.x && spot.y + size.y <= display.y;
                        const float distance = ImLengthSqr(ImVec2{spot.x - pos.x, spot.y - pos.y});
                        if ((fits && !best_fits) || (fits == best_fits && distance < best_distance))
                        {
                            best = spot;
                            best_distance = distance;
                            best_fits = fits;
                        }
                    }
                    pos = best;
                }
                ImGui::SetWindowPos(window, pos, ImGuiCond_Always);
            }
            kept[window->ID] = pos;
        }
        kept_positions_ = std::move(kept);
    }
}