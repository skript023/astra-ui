#include "widgets.hpp"
#include <astra/menu.hpp>
#include <cctype>

namespace astra {
static bool matches(const std::string &text, const char *query) {
  auto lower = [](std::string value) {
    for (auto &c : value)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
  };
  return lower(text).find(lower(query)) != std::string::npos;
}

static ImU32 ink(ImVec4 c, float alpha = 1.f) {
  c.w *= alpha;
  return pack_color(c);
}

struct window_group {
  control item;
  std::vector<window_group> children;
};

// Resolve each visible branch once per frame so callbacks keep their owners.
static std::vector<window_group> resolve_groups(
    const std::vector<control> &controls, unsigned depth = 0) {
  std::vector<window_group> groups;
  for (const auto &c : controls) {
    window_group group{c, {}};
    if (c.children && depth < 32)
      group.children = resolve_groups(c.children(), depth + 1);
    groups.push_back(std::move(group));
  }
  return groups;
}

static bool group_matches(const window_group &group, const char *query) {
  if (matches(group.item.label + " " + group.item.description, query))
    return true;
  for (const auto &child : group.children)
    if (group_matches(child, query))
      return true;
  return false;
}

static void draw_group(const window_group &group, const char *query,
                       std::function<void()> &pending) {
  const auto &c = group.item;
  if (!group_matches(group, query))
    return;
  ImGui::PushID(c.id.c_str());
  if (c.kind == control_kind::submenu && c.children) {
    if (*query)
      ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    if (ImGui::CollapsingHeader(c.label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
      const char *child_query = matches(c.label + " " + c.description, query) ? "" : query;
      ImGui::Indent(10);
      if (group.children.empty())
        ImGui::TextDisabled("No options in this section.");
      for (const auto &child : group.children)
        draw_group(child, child_query, pending);
      ImGui::Unindent(10);
    }
  } else {
    auto action = draw_control(c);
    if (action && !pending)
      pending = std::move(action);
    if (ImGui::IsItemHovered() && !c.description.empty())
      ImGui::SetTooltip("%s", c.description.c_str());
    ImGui::Spacing();
  }
  ImGui::PopID();
}

event menu::draw_window(const char *id, const page &model, bool &open,
                        const theme &target) {
  const auto colors = animate_theme(target);
  scoped_theme style(colors);
  event result;
  std::function<void()> pending;
  const std::string tab_key = model.id + "##" + std::to_string(model.selected_tab);
  auto &selected = window_sections_[tab_key];
  std::vector<std::size_t> sections;
  bool has_root_controls = false;
  for (std::size_t i = 0; i < model.controls.size(); ++i) {
    if (model.controls[i].kind == control_kind::submenu && model.breadcrumbs.size() <= 1)
      sections.push_back(i);
    else
      has_root_controls = true;
  }
  const auto selected_exists = [&] {
    for (auto i : sections)
      if (model.controls[i].id == selected)
        return true;
    return false;
  };
  if (!selected_exists() && !(selected.empty() && has_root_controls))
    selected = sections.empty() ? "" : model.controls[sections.front()].id;

  ImGui::SetNextWindowSize({1000, 680}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints({760, 520}, {FLT_MAX, FLT_MAX});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
  const bool visible = ImGui::Begin(
      id, &open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleVar();
  if (visible) {
    const auto origin = ImGui::GetWindowPos();
    const auto size = ImGui::GetWindowSize();
    auto *draw = ImGui::GetWindowDrawList();
    const float sidebar_width = sections.empty() ? 0.f : 200.f;
    const auto edge = ink(colors.muted, 0.15f);
    draw->AddRectFilled(origin, {origin.x + size.x, origin.y + 72},
                        ink(colors.panel), colors.rounding, ImDrawFlags_RoundCornersTop);
    draw->AddLine({origin.x, origin.y + 72}, {origin.x + size.x, origin.y + 72}, edge);
    if (sidebar_width) {
      draw->AddRectFilled({origin.x, origin.y + 72},
                          {origin.x + sidebar_width, origin.y + size.y},
                          ink(colors.panel), colors.rounding, ImDrawFlags_RoundCornersBottomLeft);
      draw->AddLine({origin.x + sidebar_width, origin.y + 72},
                    {origin.x + sidebar_width, origin.y + size.y}, edge);
    }
    ImGui::SetCursorPos({0, 0});
    ImGui::InvisibleButton("##drag", {180, 72});
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
      ImGui::SetWindowPos({origin.x + ImGui::GetIO().MouseDelta.x,
                           origin.y + ImGui::GetIO().MouseDelta.y});
    draw->AddText(ImGui::GetFont(), 23, {origin.x + 24, origin.y + 23},
                  ink(colors.text), "Astra");

    ImGui::SetCursorPos({200, 18});
    if (ImGui::BeginChild("##tabs", {size.x - 264, 48}, 0,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_HorizontalScrollbar)) {
      for (std::size_t i = 0; i < model.tabs.size(); ++i) {
        if (i) ImGui::SameLine();
        ImGui::PushID(static_cast<int>(i));
        const bool active = i == model.selected_tab;
        ImGui::PushStyleColor(ImGuiCol_Button, active ? colors.field : ImVec4{0, 0, 0, 0});
        if (ImGui::Button(model.tabs[i].c_str(), {0, 34}))
          result = {event_kind::tab, i};
        ImGui::PopStyleColor();
        if (active) {
          const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
          ImGui::GetWindowDrawList()->AddLine({a.x, b.y}, b, ink(colors.accent), 2);
        }
        ImGui::PopID();
      }
    }
    ImGui::EndChild();
    ImGui::SetCursorPos({size.x - 48, 22});
    if (ImGui::Button("X##close", {28, 28}))
      open = false;

    if (sidebar_width) {
      ImGui::SetCursorPos({12, 88});
      if (ImGui::BeginChild("##sidebar", {sidebar_width - 24, size.y - 104}, 0,
                            ImGuiWindowFlags_NoBackground)) {
        if (has_root_controls && ImGui::Selectable("General", selected.empty(), 0, {0, 34}))
          selected.clear();
        for (auto i : sections) {
          const auto &c = model.controls[i];
          ImGui::PushID(c.id.c_str());
          if (ImGui::Selectable(c.label.c_str(), selected == c.id, 0, {0, 34})) {
            selected = c.id;
            // Legacy hosts without inline contents can still open their submenu.
            if (!c.children && c.activate && result.kind == event_kind::none) {
              pending = c.activate;
              result = {event_kind::option, i};
            }
          }
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", c.description.empty() ? c.label.c_str() : c.description.c_str());
          ImGui::PopID();
        }
      }
      ImGui::EndChild();
    }

    const control *section = nullptr;
    for (auto i : sections)
      if (model.controls[i].id == selected) section = &model.controls[i];
    const auto search_key = tab_key + "/" + selected;
    if (search_page_ != search_key) {
      search_.fill(0);
      search_page_ = search_key;
    }
    std::vector<control> controls;
    if (section && section->children)
      controls = section->children();
    else if (!section)
      for (const auto &c : model.controls)
        if (c.kind != control_kind::submenu || model.breadcrumbs.size() > 1) controls.push_back(c);
    const auto groups = resolve_groups(controls);

    ImGui::SetCursorPos({sidebar_width + 20, 88});
    if (ImGui::BeginChild("##content", {size.x - sidebar_width - 40, size.y - 108}, 0,
                          ImGuiWindowFlags_NoBackground)) {
      // Breadcrumbs remain available to hosts that still supply flat pages.
      if (model.breadcrumbs.size() > 1) {
        if (ImGui::SmallButton("< Back")) result = {event_kind::back};
        for (std::size_t i = 0; i < model.breadcrumbs.size(); ++i) {
          ImGui::SameLine();
          ImGui::PushID(static_cast<int>(i));
          if (ImGui::SmallButton(model.breadcrumbs[i].c_str()))
            result = {event_kind::breadcrumb, i};
          ImGui::PopID();
        }
      }
      ImGui::TextUnformatted(section ? section->label.c_str() : model.title.c_str());
      ImGui::SetNextItemWidth(-1);
      ImGui::InputTextWithHint("##search", "Search this section...", search_.data(), search_.size());
      ImGui::Spacing();
      ImGui::PushID(search_key.c_str());
      if (ImGui::BeginChild("##options", {0, 0}, 0, ImGuiWindowFlags_NoBackground)) {
        const int columns = ImGui::GetContentRegionAvail().x >= 600 ? 2 : 1;
        std::size_t shown = 0;
        if (ImGui::BeginTable("##groups", columns, ImGuiTableFlags_SizingStretchSame)) {
          for (int column = 0; column < columns; ++column) {
            ImGui::TableNextColumn();
            std::size_t matched = 0;
            for (std::size_t i = 0; i < groups.size(); ++i) {
              if (!group_matches(groups[i], search_.data())) continue;
              if (matched++ % columns != static_cast<std::size_t>(column)) continue;
              ++shown;
              const bool had_action = static_cast<bool>(pending);
              draw_group(groups[i], search_.data(), pending);
              if (!had_action && pending && result.kind == event_kind::none)
                result = {event_kind::option, i};
            }
          }
          ImGui::EndTable();
        }
        if (!shown)
          ImGui::TextDisabled(controls.empty() ? "No options in this section." : "No matching options.");
      }
      ImGui::EndChild();
      ImGui::PopID();
    }
    ImGui::EndChild();
  }
  ImGui::End();
  if (pending && open && result.kind == event_kind::option)
    pending();
  return result;
}
} // namespace astra
