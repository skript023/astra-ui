#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>
#include "theme.hpp"

namespace astra {
// Windows virtual-key values, matching the game adapter's existing settings.
using key_bindings = std::array<int, 9>;
inline constexpr key_bindings default_key_bindings{0x2D, 0x08, 0x0D, 0x26, 0x28, 0x25, 0x27, 0x21, 0x22};
inline constexpr std::array<const char*, 9> key_binding_labels{
    "Open / close menu", "Back", "Select / activate", "Move up", "Move down",
    "Decrease / previous value", "Increase / next value", "Previous tab", "Next tab"};
struct key_entry { int code; std::string name; ImGuiKey input = ImGuiKey_None; };
const std::vector<key_entry>& menu_keys();
std::string menu_key_name(int code);
std::string validate_key_bindings(const key_bindings& keys);

class key_settings {
    bool open_ = false;
    key_bindings draft_ = default_key_bindings;
    std::string error_;
    ImGuiContext* context_ = nullptr;
    bool added_keyboard_navigation_ = false;
    int recording_ = -1;
    bool recording_navigation_ = false;
    std::vector<ImGuiKey> held_keys_;
    void stop_recording();
    void restore_navigation();
public:
    void open(const key_bindings& current);
    bool active() const { return open_; }
    key_bindings& draft() { return draft_; }
    bool recording() const { return recording_ >= 0; }
    void record(std::size_t index);
    void cancel();
    void reset() { stop_recording(); draft_ = default_key_bindings; error_.clear(); }
    std::optional<key_bindings> apply();
    std::optional<key_bindings> draw(const theme& colors = preset_theme(0));
};
}
