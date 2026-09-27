#include <astra/key_settings.hpp>
#include <imgui.h>
#include <algorithm>

namespace astra {
const std::vector<key_entry>& menu_keys() {
    static const auto keys = [] {
        std::vector<key_entry> result{
            {0x2D, "Insert", ImGuiKey_Insert}, {0x2E, "Delete", ImGuiKey_Delete},
            {0x24, "Home", ImGuiKey_Home}, {0x23, "End", ImGuiKey_End},
            {0x21, "Page Up", ImGuiKey_PageUp}, {0x22, "Page Down", ImGuiKey_PageDown},
            {0x08, "Backspace", ImGuiKey_Backspace}, {0x09, "Tab", ImGuiKey_Tab},
            {0x0D, "Enter", ImGuiKey_Enter}, {0x1B, "Escape", ImGuiKey_Escape},
            {0x20, "Space", ImGuiKey_Space}, {0x26, "Up", ImGuiKey_UpArrow},
            {0x28, "Down", ImGuiKey_DownArrow}, {0x25, "Left", ImGuiKey_LeftArrow},
            {0x27, "Right", ImGuiKey_RightArrow}};
        for (int i = 0; i < 24; ++i)
            result.push_back({0x70 + i, "F" + std::to_string(i + 1), static_cast<ImGuiKey>(ImGuiKey_F1 + i)});
        for (int i = 'A'; i <= 'Z'; ++i)
            result.push_back({i, std::string(1, static_cast<char>(i)), static_cast<ImGuiKey>(ImGuiKey_A + i - 'A')});
        for (int i = '0'; i <= '9'; ++i)
            result.push_back({i, std::string(1, static_cast<char>(i)), static_cast<ImGuiKey>(ImGuiKey_0 + i - '0')});
        for (int i = 0; i < 10; ++i)
            result.push_back({0x60 + i, "Numpad " + std::to_string(i), static_cast<ImGuiKey>(ImGuiKey_Keypad0 + i)});
        result.insert(result.end(), {
            {0x6A, "Numpad *", ImGuiKey_KeypadMultiply}, {0x6B, "Numpad +", ImGuiKey_KeypadAdd},
            {0x6D, "Numpad -", ImGuiKey_KeypadSubtract}, {0x6E, "Numpad .", ImGuiKey_KeypadDecimal},
            {0x6F, "Numpad /", ImGuiKey_KeypadDivide}, {0x0D, "Enter", ImGuiKey_KeypadEnter},
            {0xA0, "Left Shift", ImGuiKey_LeftShift}, {0xA1, "Right Shift", ImGuiKey_RightShift},
            {0xA2, "Left Ctrl", ImGuiKey_LeftCtrl}, {0xA3, "Right Ctrl", ImGuiKey_RightCtrl},
            {0xA4, "Left Alt", ImGuiKey_LeftAlt}, {0xA5, "Right Alt", ImGuiKey_RightAlt},
            {0x5B, "Left Windows", ImGuiKey_LeftSuper}, {0x5C, "Right Windows", ImGuiKey_RightSuper},
            {0x14, "Caps Lock", ImGuiKey_CapsLock}, {0x90, "Num Lock", ImGuiKey_NumLock},
            {0x91, "Scroll Lock", ImGuiKey_ScrollLock}, {0x13, "Pause", ImGuiKey_Pause},
            {0x2C, "Print Screen", ImGuiKey_PrintScreen}, {0x5D, "Menu", ImGuiKey_Menu},
            {0xBA, ";", ImGuiKey_Semicolon}, {0xBB, "=", ImGuiKey_Equal},
            {0xBC, ",", ImGuiKey_Comma}, {0xBD, "-", ImGuiKey_Minus},
            {0xBE, ".", ImGuiKey_Period}, {0xBF, "/", ImGuiKey_Slash},
            {0xC0, "Backtick", ImGuiKey_GraveAccent}, {0xDB, "[", ImGuiKey_LeftBracket},
            {0xDC, "Backslash", ImGuiKey_Backslash}, {0xDD, "]", ImGuiKey_RightBracket},
            {0xDE, "Apostrophe", ImGuiKey_Apostrophe}, {0xE2, "Non-US backslash", ImGuiKey_Oem102}});
        return result;
    }();
    return keys;
}

std::string menu_key_name(int code) {
    for (const auto& key : menu_keys())
        if (key.code == code) return key.name;
    return "Key " + std::to_string(code);
}

std::string validate_key_bindings(const key_bindings& keys) {
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] < 8 || keys[i] > 254)
            return std::string(key_binding_labels[i]) + ": choose a keyboard key.";
        for (std::size_t j = 0; j < i; ++j)
            if (keys[i] == keys[j])
                return std::string(key_binding_labels[i]) + " and " + key_binding_labels[j] +
                    " both use " + menu_key_name(keys[i]) + ". Choose different keys.";
    }
    return {};
}

std::optional<key_bindings> key_settings::apply() {
    if (!open_ || recording()) return {};
    error_ = validate_key_bindings(draft_);
    if (!error_.empty()) return {};
    open_ = false;
    restore_navigation();
    return draft_;
}

void key_settings::open(const key_bindings& current) {
    stop_recording();
    if (!open_) {
        context_ = ImGui::GetCurrentContext();
        added_keyboard_navigation_ = context_ &&
            !(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard);
        if (added_keyboard_navigation_)
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }
    draft_ = current;
    error_.clear();
    open_ = true;
}

void key_settings::restore_navigation() {
    if (added_keyboard_navigation_ && context_ && ImGui::GetCurrentContext() == context_)
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    added_keyboard_navigation_ = false;
    context_ = nullptr;
}

void key_settings::cancel() {
    stop_recording();
    open_ = false;
    error_.clear();
    restore_navigation();
}

void key_settings::record(std::size_t index) {
    if (!open_ || index >= draft_.size() || !ImGui::GetCurrentContext()) return;
    stop_recording();
    recording_ = static_cast<int>(index);
    error_.clear();
    recording_navigation_ = (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) != 0;
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    for (const auto& key : menu_keys())
        if (ImGui::IsKeyDown(key.input)) held_keys_.push_back(key.input);
}

void key_settings::stop_recording() {
    if (recording_navigation_ && context_ && ImGui::GetCurrentContext() == context_)
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    recording_navigation_ = false;
    recording_ = -1;
    held_keys_.clear();
}

std::optional<key_bindings> key_settings::draw(const theme& colors) {
    if (!open_) return {};
    scoped_theme style(colors);
    std::optional<key_bindings> result;
    ImGui::SetNextWindowSize({530, 640}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({400, 360}, {FLT_MAX, FLT_MAX});
    if (ImGui::Begin("Menu controls###astra_key_settings", &open_, ImGuiWindowFlags_NoCollapse)) {
        bool captured = false;
        if (recording()) {
            if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::GetIO().AppFocusLost) {
                stop_recording();
            } else {
                held_keys_.erase(std::remove_if(held_keys_.begin(), held_keys_.end(),
                    [](ImGuiKey key) { return !ImGui::IsKeyDown(key); }), held_keys_.end());
                for (const auto& key : menu_keys()) {
                    if (!ImGui::IsKeyPressed(key.input, false) ||
                        std::find(held_keys_.begin(), held_keys_.end(), key.input) != held_keys_.end()) continue;
                    if (key.input != ImGuiKey_Escape) draft_[recording_] = key.code;
                    stop_recording();
                    captured = true;
                    break;
                }
            }
        }
        ImGui::TextWrapped("Click a binding, then press one keyboard key. Escape cancels recording. Apply confirms your changes.");
        ImGui::Spacing();
        for (std::size_t i = 0; i < draft_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const auto name = recording_ == static_cast<int>(i) ? std::string("Press a key...") : menu_key_name(draft_[i]);
            ImGui::BeginDisabled(recording() || captured);
            if (ImGui::Button((name + "###binding").c_str(), {160, 0})) record(i);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextUnformatted(key_binding_labels[i]);
            ImGui::PopID();
        }
        ImGui::Spacing();
        if (recording() && ImGui::Button("Stop recording")) stop_recording();
        if (!error_.empty()) ImGui::TextWrapped("%s", error_.c_str());
        ImGui::BeginDisabled(recording() || captured);
        if (ImGui::Button("Apply")) result = apply();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) cancel();
        ImGui::SameLine();
        if (ImGui::Button("Reset defaults")) reset();
        ImGui::TextWrapped("Reset changes the choices above; Apply confirms them. Controller buttons are unchanged.");
    }
    ImGui::End();
    if (!open_) cancel();
    return result;
}
}
