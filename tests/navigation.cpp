#include <astra/menu.hpp>
#include <astra/options.hpp>
#include <astra/navigation.hpp>
#include "../src/widgets.hpp"
#include <imgui_internal.h>
#include <cmath>
#include <cstdio>
#include <stdexcept>

static void require(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
static void navigation_tests()
{
    astra::navigation<int> n;
    require(!n.back() && n.path().empty(), "empty navigation");
    n.set_root(99);
    require(!n.back(), "root back");
    n.add_tab(1); n.add_tab(2);
    n.push(11); n.push(12);
    require(n.back() && n.path().back() == 11, "nested back");
    n.select_tab(1); n.push(21);
    n.select_tab(0);
    require(n.path() == std::vector<int>({1,11}), "tab restores updated path");
    for (int i=0;i<1000;++i) { n.select_tab(1); n.select_tab(0); }
    require(n.path().size() == 2, "switching does not grow history");
    n.push(11);
    require(n.path().size() == 2, "no duplicate current page");
    n.push(12); n.push(1);
    require(n.path() == std::vector<int>({1}), "ancestor navigation removes cycle");
    require(!n.to_depth(9) && !n.select_tab(100), "invalid destination ignored");
    n.select_tab(1); n.to_depth(0);
    require(n.path() == std::vector<int>({2}), "breadcrumb preserves root");
    require(astra::bounded_value(999,0,10,true) == 10, "numeric upper bound");
    require(astra::bounded_value(-99,0,10,true) == 0, "numeric lower bound");
    require(astra::bounded_value(2.8,0,10,true) == 3, "integer conversion");
    require(astra::bounded_value(NAN,0,10,false) == 0, "nonfinite input");
}

static void option_binding_tests()
{
    bool enabled = false;
    auto toggle = astra::bind_toggle("Enabled", "Command-backed toggle", &enabled);
    auto toggle_control = toggle.describe_ui();
    require(toggle_control.kind == astra::control_kind::toggle && !toggle_control.checked, "toggle control description");
    toggle_control.activate();
    require(enabled && toggle.describe_ui().checked, "toggle callback reads and writes live state");
    toggle.handle_action(astra::option_action::EnterPress);
    require(!enabled, "toggle supports list action");

    int value = 3;
    auto number = astra::bind_value("Amount", "Command-backed number", &value, 0, 10, 2, 0);
    number.handle_action(astra::option_action::RightPress);
    require(value == 5, "number horizontal action respects step");
    auto number_control = number.describe_ui();
    number_control.set_value(100);
    require(value == 10 && number.describe_ui().value == 10, "number callback clamps and refreshes value");
    bool active = false;
    double amount = 4;
    auto combined = astra::bound_option::toggle_number("Limit", "Enabled numeric control",
        [&] { return active; }, [&](bool next) { active = next; },
        [&] { return amount; }, [&](double next) { amount = next; }, 0, 10, 2);
    auto combined_control = combined.describe_ui();
    require(combined_control.kind == astra::control_kind::toggle_number && !combined_control.checked, "bool-slider control description");
    combined_control.activate();
    combined.describe_ui().set_value(50);
    require(active && amount == 10, "bool-slider callbacks toggle and clamp values");

    int selected = 0;
    auto choice = astra::bound_option::choice("Mode", "Select a mode", {"A", "B"},
        [&] { return selected; }, [&](int next) { selected = next; });
    choice.handle_action(astra::option_action::RightPress);
    require(selected == 1 && choice.describe_ui().choice == 1, "choice binding cycles and refreshes live state");
}
struct ui_fixture
{
    astra::menu menu;
    astra::page page;
    bool open = true;
    bool list_mode = false;
    bool overlay = false;
    astra::list_style list;
    ImVec2 item_min{}, item_max{};
    astra::event event;
    ui_fixture()
    {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1280,800};
        io.DeltaTime = 1.f/60.f;
        io.ConfigInputTrickleEventQueue = false;
        unsigned char* pixels; int w,h;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        io.Fonts->SetTexID(1);
        page.id = "root";
        page.title = "Overview";
        page.tabs = {"Home","Settings"};
        page.breadcrumbs = {"Home"};
    }
    ~ui_fixture() { ImGui::DestroyContext(); }
    void frame()
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({40,40},ImGuiCond_Always);
        event = list_mode ? menu.draw_list(page,astra::preset_theme(0),list)
                         : menu.draw_window("Fixture",page,open,astra::preset_theme(0));
        if (list_mode && event.kind == astra::event_kind::option) page.selected_option = event.index;
        if (list_mode && event.kind == astra::event_kind::tab) page.selected_tab = event.index;
        if (overlay) {
            ImGui::SetNextWindowPos(list.position);
            ImGui::SetNextWindowSize({list.width,400});
            ImGui::Begin("Overlay",nullptr,ImGuiWindowFlags_NoDecoration);
            ImGui::TextUnformatted("Overlay");
            ImGui::End();
        }
        ImGui::Render();
    }
    void click(ImVec2 p)
    {
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(p.x,p.y);
        frame();
        io.AddMouseButtonEvent(0,true);
        frame();
        io.AddMouseButtonEvent(0,false);
        frame();
    }
    void mark(astra::control& c)
    {
        c.draw_details = [this] { item_min = ImGui::GetItemRectMin(); item_max = ImGui::GetItemRectMax(); };
    }
    ImVec2 center() { return {(item_min.x+item_max.x)/2,(item_min.y+item_max.y)/2}; }
};
static void interaction_tests()
{
    ui_fixture f;
    int actions = 0;
    astra::control button;
    button.id="action"; button.label="Run"; button.activate=[&]{++actions;};
    f.mark(button); f.page.controls={button};
    const auto old = ImGui::GetStyle();
    f.frame(); f.frame();
    require(ImGui::GetDrawData()->TotalVtxCount > 0, "window generates geometry");
    require(ImGui::GetStyle().WindowPadding.x == old.WindowPadding.x, "theme restored after drawing");
    f.click(f.center());
    require(actions == 1, "one click invokes action once");
    require(f.event.kind == astra::event_kind::option, "action selection event");

    auto& c = f.page.controls[0];
    c.kind = astra::control_kind::toggle; c.label="Enabled";
    bool enabled=false;
    c.activate=[&] {enabled=!enabled;};
    f.frame(); f.frame(); f.click(f.center());
    require(enabled && actions == 1, "checkbox uses toggle callback");

    c.kind=astra::control_kind::number; c.label="Amount"; c.minimum=0; c.maximum=100; c.value=0;
    double edited=-1;
    c.set_value=[&](double v){edited=v;};
    f.frame(); f.frame();
    f.click({f.item_min.x+(f.item_max.x-f.item_min.x)*0.75f, (f.item_min.y+f.item_max.y)/2});
    require(edited > 60 && edited < 90, "slider click updates numeric value");

    auto& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl,true);
    io.AddMousePosEvent(f.center().x,f.center().y);
    io.AddMouseButtonEvent(0,true); f.frame();
    io.AddMouseButtonEvent(0,false); io.AddKeyEvent(ImGuiMod_Ctrl,false); f.frame();
    require(io.WantTextInput || ImGui::TempInputIsActive(ImGui::GetActiveID()), "ctrl click enables numeric typing");
    io.AddKeyEvent(ImGuiMod_Ctrl,true); io.AddKeyEvent(ImGuiKey_A,true); f.frame();
    io.AddKeyEvent(ImGuiKey_A,false); io.AddKeyEvent(ImGuiMod_Ctrl,false);
    io.AddInputCharactersUTF8("37"); f.frame();
    io.AddKeyEvent(ImGuiKey_Enter,true); f.frame();
    io.AddKeyEvent(ImGuiKey_Enter,false); f.frame();
    require(std::abs(edited-37) < .01, "typed numeric value applied");

    c.kind=astra::control_kind::choice; c.label="Mode"; c.choices={"First","Second"};
    c.activate={}; c.choice=0;
    int chosen=-1;
    c.set_choice=[&](int v){chosen=v;};
    f.frame(); f.frame(); f.click(f.center());
    ImGuiWindow* popup=nullptr;
    for (auto* w : ImGui::GetCurrentContext()->Windows)
        if ((w->Flags & ImGuiWindowFlags_Popup) && w->Active) popup=w;
    require(popup != nullptr, "combo opens on click");
    const auto pos=popup->DC.CursorStartPos;
    f.click({pos.x+30,pos.y+(ImGui::GetFontSize()+10.f)*1.5f});
    require(chosen == 1, "dropdown selects second entry");

    f.page.controls.clear();
    f.frame(); f.frame();
    ImGuiWindow* sidebar=nullptr;
    for (auto* w : ImGui::GetCurrentContext()->Windows)
        if (std::string(w->Name).find("##sidebar") != std::string::npos) sidebar=w;
    require(sidebar != nullptr, "sidebar exists");
    // Sidebar navigation starts at 36px with 44px rows and 10px spacing.
    const auto p=sidebar->Pos;
    f.click({p.x+55,p.y+36+54+22});
    require(f.event.kind == astra::event_kind::tab && f.event.index == 1, "sidebar changes tab");

    f.page.breadcrumbs={"Home","Child"};
    f.frame(); f.frame();
    ImGuiWindow* content=nullptr;
    for (auto* w : ImGui::GetCurrentContext()->Windows)
        if (std::string(w->Name).find("##content") != std::string::npos && std::string(w->Name).find("##options") == std::string::npos) content=w;
    require(content != nullptr, "content exists");
    f.click({content->DC.CursorStartPos.x+30,content->DC.CursorStartPos.y+6});
    require(f.event.kind == astra::event_kind::back, "clickable back");

    ImGui::NewFrame();
    astra::list_style list; list.rows=0;
    f.menu.draw_list(f.page, astra::preset_theme(1), list);
    ImGui::Render();
    require(ImGui::GetDrawData()->TotalVtxCount > 0, "empty list layout safe");
    f.page.controls={button};
    f.page.selected_option=999;
    ImGui::NewFrame();
    f.menu.draw_list(f.page, astra::preset_theme(2), list);
    ImGui::Render();
    require(ImGui::GetDrawData()->TotalVtxCount > 0, "list selection clamps");
}
static void list_mouse_tests()
{
    ui_fixture f;
    f.list_mode = true;
    f.list.position = {83,67};
    f.list.width = 420;
    f.list.row_height = 36;
    f.list.rows = 3;
    f.page.tabs = {"One","Two","Three","Four","Five"};
    const float top = 67 + 60 + 45;
    int actions = 0;
    bool checked = false;
    double value = 0;
    int choice = 0;
    astra::control action;
    action.id = "action"; action.label = "Run"; action.activate = [&] { ++actions; };
    astra::control slider;
    slider.id = "slider"; slider.label = "Limit"; slider.kind = astra::control_kind::toggle_number;
    slider.minimum = 0; slider.maximum = 100; slider.integral = true;
    slider.activate = [&] { checked = !checked; };
    slider.set_value = [&](double v) { value = v; f.page.controls[1].value = v; };
    astra::control number;
    number.id = "number"; number.label = "Amount"; number.kind = astra::control_kind::number;
    number.minimum = 0; number.maximum = 100; number.step = 5;
    number.set_value = [&](double v) { value = v; f.page.controls[2].value = v; };
    f.page.controls = {action, slider, number};
    f.frame(); f.frame();
    f.click({100,top + 18});
    require(actions == 1, "list action fires once");
    auto& io = ImGui::GetIO();
    const float right = 83 + 420 - 58, left = right - 126;
    io.AddMousePosEvent((left + right) * .5f, top + 54); f.frame();
    io.AddMouseButtonEvent(0,true); f.frame();
    require(value == 50 && !checked, "raw slider midpoint does not toggle");
    io.AddMousePosEvent(right + 100, top + 200); f.frame();
    require(value == 100 && !checked, "slider drag outside captures and clamps maximum");
    io.AddMousePosEvent(left - 100, top + 200); f.frame();
    require(value == 0, "slider drag clamps minimum");
    io.AddMouseButtonEvent(0,false); f.frame();
    f.click({83 + 420 - 19,top + 54});
    require(checked, "checkbox remains separate from slider");
    value = 0;
    f.click({83 + 420 - 17,top + 90});
    require(value == 5, "number next arrow uses step");
    f.page.controls[2].value = 5;
    const auto shown = std::string("< 5.00 >");
    f.click({83 + 420 - 14 - ImGui::CalcTextSize(shown.c_str()).x + 2,top + 90});
    require(value == 0, "number previous arrow uses drawn coordinates");
    auto& c = f.page.controls[2];
    c.kind = astra::control_kind::choice; c.choices = {"Alpha","Beta"}; c.choice = 0;
    c.set_choice = [&](int next) { choice = next; };
    f.frame(); f.frame();
    f.click({83 + 420 - 17,top + 90});
    require(choice == 1, "choice next arrow changes value");
    c.choice = 1;
    f.click({83 + 420 - 17,top + 90});
    require(choice == 0, "choice cycles at end");
    f.page.controls.clear();
    for (int i = 0; i < 12; ++i) {
        auto item = action; item.id = std::to_string(i); f.page.controls.push_back(item);
    }
    f.page.selected_option = 0;
    f.frame(); f.frame();
    io.AddMousePosEvent(100,top + 18); f.frame();
    io.AddMouseWheelEvent(0,-5); f.frame();
    require(f.page.selected_option == 5 && actions == 1, "wheel reveals hidden rows without activation");
    f.click({100,top + 54});
    require(f.event.index == 6 && actions == 2, "scrolled row maps to correct option");
    f.click({100,top + 18});
    require(f.event.index == 5 && actions == 3, "click preserves viewport");
    io.AddMousePosEvent(83 + 420 + 10,top + 100); f.frame();
    io.AddMouseButtonEvent(0,true); f.frame();
    io.AddMousePosEvent(83 + 420 + 50,top + 200); f.frame();
    io.AddMouseButtonEvent(0,false); f.frame();
    f.click({100,top + 90});
    require(f.event.index == 11 && actions == 4, "scrollbar drag reaches final option");
    f.page.selected_option = 0;
    f.frame(); f.frame(); f.click({100,top + 18});
    require(f.event.index == 0, "keyboard selection brings row into view");
    io.AddMousePosEvent(100,67 + 75); f.frame();
    io.AddMouseWheelEvent(0,-4); f.frame();
    require(f.page.selected_tab == 4, "wheel reaches fifth tab");
    f.frame(); f.frame();
    f.click({90,67 + 75});
    require(f.page.selected_tab == 2, "animated tab hitbox matches visible tab");
    const auto previous = f.page.selected_option;
    io.AddMousePosEvent(900,700); f.frame();
    io.AddMouseWheelEvent(0,-4); f.frame();
    require(f.page.selected_option == previous && f.page.selected_tab == 2, "outside wheel leaves menu untouched");
    f.list.rows = 1; f.list.row_height = 24;
    f.frame(); f.frame();
    io.AddMousePosEvent(83 + 420 + 10,top + 18); f.frame();
    io.AddMouseButtonEvent(0,true); f.frame();
    io.AddMousePosEvent(83 + 420 + 10,top + 30); f.frame();
    io.AddMouseButtonEvent(0,false); f.frame();
    require(f.page.selected_option == 11, "minimum height scrollbar remains usable");
    f.page.breadcrumbs = {"Root","Child"};
    io.AddMousePosEvent(100,top + 12); f.frame();
    io.AddMouseButtonEvent(1,true); f.frame();
    require(f.event.kind == astra::event_kind::back, "right click returns from submenu");
    io.AddMouseButtonEvent(1,false); f.frame();
    f.page.breadcrumbs = {"Root"};
    io.AddMouseButtonEvent(1,true); f.frame();
    require(f.event.kind == astra::event_kind::none, "right click at root does nothing");
    io.AddMouseButtonEvent(1,false); f.frame();
    const int before = actions;
    f.list.mouse_enabled = false;
    f.frame(); f.frame(); f.click({100,top + 12});
    require(actions == before && f.event.kind == astra::event_kind::none, "disabled mouse cannot activate rows");
    f.list.mouse_enabled = true;
    f.overlay = true;
    f.frame(); f.frame(); f.click({100,top + 12});
    require(actions == before, "covering window blocks raw list clicks");
    f.overlay = false;
    f.list.banner = 1; f.list.banner_height = 130;
    f.list.position = {170,100}; f.list.row_height = 48; f.list.width = 300; f.list.rows = 3;
    f.page.id = "new-page"; f.page.tabs.clear(); f.page.controls = {action}; f.page.selected_option = 99;
    f.frame(); f.frame();
    f.click({190,100 + 130 + 24});
    require(actions == before + 1 && f.event.index == 0, "banner and resized list use matching screen coordinates");
    f.page.controls.clear(); f.frame();
    io.AddMouseWheelEvent(0,-20); f.frame();
    require(f.event.kind == astra::event_kind::none, "empty list safely ignores wheel");
}
int main()
{
    try { navigation_tests(); option_binding_tests(); interaction_tests(); list_mouse_tests(); std::puts("Astra regression tests passed"); return 0; }
    catch(const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
