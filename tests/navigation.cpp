#include <astra/menu.hpp>
#include <astra/options.hpp>
#include <astra/navigation.hpp>
#include <astra/key_settings.hpp>
#include <astra/banners.hpp>
#include <astra/hud.hpp>
#include <astra/components.hpp>
#include "../src/widgets.hpp"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

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

static void key_settings_tests()
{
    astra::key_settings editor;
    auto current=astra::default_key_bindings;
    require(astra::validate_key_bindings(current).empty(), "default keys are unique");
    editor.open(current);
    editor.draft()[0]=0x75;
    require(current[0]==0x2D, "editing draft does not replace active hotkey");
    editor.draft()[1]=0x75;
    require(!editor.apply() && editor.active(), "duplicate keys rejected without closing editor");
    editor.draft()[1]=0;
    require(!editor.apply(), "unbound navigation key rejected");
    editor.draft()[1]=current[1];
    auto updated=editor.apply();
    require(updated && (*updated)[0]==0x75 && !editor.active(), "valid hotkey applies atomically");
    require(!editor.apply(), "apply cannot fire again after closing");
    editor.open(*updated);
    editor.reset();
    require(editor.active() && editor.draft()==astra::default_key_bindings, "reset remains a draft");
    editor.cancel();
    require(!editor.apply() && (*updated)[0]==0x75, "cancel preserves previous active bindings");
    require(astra::menu_key_name(0x75)=="F6", "key names match virtual-key codes");
    require(astra::menu_key_name(0x60)=="Numpad 0", "numpad is distinct from number row");
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
static void key_settings_ui_tests()
{
    ui_fixture f;
    astra::key_settings editor;
    const auto flags=ImGui::GetIO().ConfigFlags;
    editor.open(astra::default_key_bindings);
    require((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard)!=0,
        "key editor enables keyboard navigation while open");
    for (int i=0; i<2; ++i)
    {
        ImGui::NewFrame();
        editor.draw(astra::preset_theme(4));
        ImGui::Render();
    }
    auto* panel=ImGui::FindWindowByName("Menu controls###astra_key_settings");
    require(panel && panel->Active && ImGui::GetDrawData()->TotalVtxCount>0,
        "key editor renders its panel");
    const auto frame = [&] {
        ImGui::NewFrame();
        editor.draw(astra::preset_theme(4));
        ImGui::Render();
    };
    ImVec2 binding;
    {
        astra::scoped_theme style(astra::preset_theme(4));
        const float text_height=ImGui::CalcTextSize(
            "Click a binding, then press one keyboard key. Escape cancels recording. Apply confirms your changes.",
            nullptr,false,panel->WorkRect.GetWidth()).y;
        binding={panel->DC.CursorStartPos.x+60,
            panel->DC.CursorStartPos.y+text_height+2*ImGui::GetStyle().ItemSpacing.y+ImGui::GetFrameHeight()/2};
    }
    auto& io=ImGui::GetIO();
    io.AddMousePosEvent(binding.x,binding.y); frame();
    io.AddMouseButtonEvent(0,true); frame();
    io.AddMouseButtonEvent(0,false); frame();
    require(editor.recording(), "click binding starts key recording");
    require(!editor.apply(), "apply is blocked while recording");
    io.AddKeyEvent(ImGuiKey_F6,true); frame();
    require(!editor.recording() && editor.draft()[0]==0x75, "pressed key records Windows virtual-key value");
    editor.record(1);
    frame();
    require(editor.recording() && editor.draft()[1]==0x08, "held key is not recorded for next binding");
    io.AddKeyEvent(ImGuiKey_F6,false); frame();
    io.AddKeyEvent(ImGuiKey_F7,true); frame();
    require(!editor.recording() && editor.draft()[1]==0x76, "fresh key press updates selected binding");
    io.AddKeyEvent(ImGuiKey_F7,false); frame();
    editor.record(2);
    io.AddKeyEvent(ImGuiKey_Escape,true); frame();
    require(!editor.recording() && editor.draft()[2]==0x0D, "Escape cancels recording without changing binding");
    io.AddKeyEvent(ImGuiKey_Escape,false); frame();
    editor.record(3);
    io.AddKeyEvent(ImGuiKey_Tab,true); frame();
    require(!editor.recording() && editor.draft()[3]==0x09, "Tab records instead of moving UI focus");
    io.AddKeyEvent(ImGuiKey_Tab,false); frame();
    editor.record(4);
    io.AddKeyEvent(ImGuiKey_Keypad3,true); frame();
    require(editor.draft()[4]==0x63, "numpad key is recorded distinctly");
    io.AddKeyEvent(ImGuiKey_Keypad3,false); frame();
    editor.record(5);
    io.AddFocusEvent(false); frame();
    require(!editor.recording() && editor.draft()[5]==0x25, "focus loss cancels recording");
    io.AddFocusEvent(true); frame();
    editor.cancel();
    require(ImGui::GetIO().ConfigFlags==flags, "cancel restores host keyboard navigation setting");
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    editor.open(astra::default_key_bindings);
    editor.apply();
    require((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard)!=0,
        "apply preserves pre-existing host keyboard navigation");
}

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
    ImGuiWindow* tabs=nullptr;
    for (auto* w : ImGui::GetCurrentContext()->Windows)
        if (std::string(w->Name).find("##tabs") != std::string::npos) tabs=w;
    require(tabs != nullptr, "horizontal tabs exist");
    const auto p=tabs->DC.CursorStartPos;
    f.click({p.x+ImGui::CalcTextSize("Home").x+60,p.y+17});
    require(f.event.kind == astra::event_kind::tab && f.event.index == 1, "header changes tab");

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
static void window_hierarchy_tests()
{
    ui_fixture f;
    int actions=0, navigations=0, first_loads=0, second_loads=0;
    astra::control leaf;
    leaf.id="leaf"; leaf.label="Nested toggle"; leaf.kind=astra::control_kind::toggle;
    leaf.activate=[&] { ++actions; };
    f.mark(leaf);
    astra::control nested;
    nested.id="nested"; nested.label="Nested group"; nested.kind=astra::control_kind::submenu;
    nested.activate=[&] { ++navigations; };
    nested.children=[&] { return std::vector<astra::control>{leaf}; };
    astra::control first=nested;
    first.id="first"; first.label="First section";
    first.children=[&] { ++first_loads; return std::vector<astra::control>{nested}; };
    astra::control second=first;
    second.id="second"; second.label="Second section";
    second.children=[&] { ++second_loads; return std::vector<astra::control>{leaf}; };
    f.page.controls={first,second};
    f.frame(); f.frame();
    require(first_loads==2 && second_loads==0, "only selected outer submenu loads");
    f.click(f.center());
    require(actions==1 && navigations==0, "nested control acts inline without submenu activation");
    ImGuiWindow* sidebar=nullptr;
    for (auto* w : ImGui::GetCurrentContext()->Windows)
        if (std::string(w->Name).find("##sidebar")!=std::string::npos) sidebar=w;
    require(sidebar && sidebar->ContentSize.y<120, "sidebar contains only outer submenus");
    const auto start=sidebar->DC.CursorStartPos;
    f.click({start.x+40,start.y+34+10+17});
    require(second_loads==1 && navigations==0, "sidebar changes inline section without navigation");
    f.click(f.center());
    require(actions==2, "second section callback runs once");
    const int before=second_loads;
    f.page.selected_tab=1; f.page.id="other";
    f.frame();
    require(second_loads==before, "new tab starts with first outer section");
    f.page.selected_tab=0; f.page.id="root";
    f.frame();
    require(second_loads==before+1, "tab restores its sidebar selection");
    f.page.controls={first};
    f.frame();
    require(second_loads==before+1, "removed selected submenu falls back safely");
    f.page.controls.clear();
    f.frame();
    require(f.event.kind==astra::event_kind::none, "empty window has no stale action");
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
    io.AddMousePosEvent(left - 100, top + 18); f.frame();
    require(f.page.selected_option == 1, "slider drag over another row keeps its row selected");
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
    io.AddMousePosEvent(100,top + 54); f.frame();
    require(f.page.selected_option == 1 && actions == 1, "moving cursor selects hovered row without activation");
    f.page.selected_option = 2;
    f.frame(); f.frame();
    require(f.page.selected_option == 2, "resting cursor leaves keyboard selection");
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
    f.frame(); f.frame();
    f.click({83 + 420 - 5,67 + 75});
    require(f.page.selected_tab == 3, "the tab after the selected one shows beside it");
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
    require(actions == before + 1, "a game window dragged over the List stays behind it, so the List still takes the click");
    f.overlay = false;
    f.list.banner = 1; f.list.banner_height = 130;
    f.list.position = {170,100}; f.list.row_height = 48; f.list.width = 300; f.list.rows = 3;
    f.page.id = "new-page"; f.page.tabs.clear(); f.page.controls = {action}; f.page.selected_option = 99;
    f.frame(); f.frame();
    f.click({190,100 + 130 + 24});
    require(actions == before + 2 && f.event.index == 0, "banner and resized list use matching screen coordinates");
    f.page.controls.clear(); f.frame();
    io.AddMouseWheelEvent(0,-20); f.frame();
    require(f.event.kind == astra::event_kind::none, "empty list safely ignores wheel");
}
static bool draw_data_has_highlight(ImU32 first, ImU32 second)
{
    const auto* data = ImGui::GetDrawData();
    if (!data) return false;
    for (int i = 0; i < data->CmdListsCount; ++i)
        for (int v = 0; v < data->CmdLists[i]->VtxBuffer.Size; ++v)
        {
            const auto col = data->CmdLists[i]->VtxBuffer[v].col;
            if (col == first || col == second) return true;
        }
    return false;
}
// The List highlight must follow the active theme, including colours a host
// only overrides at runtime (accent, selection text).
static void theme_color_tests()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1280,800};
    io.DeltaTime = 1.f/60.f;
    unsigned char* pixels; int w,h;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    io.Fonts->SetTexID(1);

    astra::menu view;
    astra::page page;
    page.id = "colors"; page.title = "Colors"; page.tabs = {"One","Two"};
    astra::control row; row.id = "row"; row.label = "Row"; page.controls = {row};
    astra::list_style list;

    astra::theme first = astra::preset_theme(0);
    first.selection = {1.f,0.f,0.f,1.f};
    first.selection_text = {0.f,0.f,0.f,1.f};
    astra::theme second = astra::preset_theme(0);
    second.selection = {0.f,0.f,1.f,1.f};
    second.selection_text = {0.f,0.f,0.f,1.f};

    const auto render = [&](const astra::theme& colors,int frames)
    {
        for (int i=0;i<frames;++i)
        {
            ImGui::NewFrame();
            view.draw_list(page,colors,list);
            ImGui::Render();
        }
    };
    render(first,4);
    require(draw_data_has_highlight(IM_COL32(255,0,0,255),0), "highlight uses the initial theme colour");
    render(second,90);
    require(draw_data_has_highlight(IM_COL32(0,0,255,255),0), "highlight follows a runtime theme colour change");
    // Hosts save the theme as an index, so each name keeps its preset.
    const char* saved[] = {"Emerald","Violet","Ocean","Custom","Studio","Snow"};
    require(astra::preset_theme_names.size() >= std::size(saved), "every saved theme index still exists");
    for (std::size_t i = 0; i < std::size(saved); ++i)
        require(std::string_view(astra::preset_theme_names[i]) == saved[i], "saved theme indexes keep their name");
    for (int index=0; index<static_cast<int>(astra::preset_theme_names.size()); ++index)
    {
        const auto preset = astra::preset_theme(index);
        require(astra::pack_color(preset.selection) != astra::pack_color(preset.panel), "List highlight differs from the List row colour");
        require(astra::pack_color(preset.selection_text) != astra::pack_color(preset.selection), "List highlight text differs from the highlight");
    }
    const auto studio = astra::preset_theme(4);
    astra::apply_theme(studio);
    const auto& style = ImGui::GetStyle();
    require(style.Colors[ImGuiCol_Button].x == studio.field.x && style.Colors[ImGuiCol_TitleBgCollapsed].x == studio.panel.x, "game panels take the menu theme");
    { astra::scoped_theme menu_window(astra::preset_theme(0)); }
    require(ImGui::GetStyle().Colors[ImGuiCol_Button].x == studio.field.x, "the Window layout's scoped theme leaves the panel theme in place");
    ImGui::DestroyContext();
}
static void banner_tests()
{
    // Hosts already saved these as indexes, so their order is fixed.
    const char* saved[] = {"Quantum Green","Ellohim","Ellohim Green","Astra Standard","Ellohim Purple","Ellohim Cool",
        "Quantum","Quantum Blue","Halloween 2024","UnknownCheats","Wuthering Wave"};
    const auto list = astra::banners();
    require(list.size() >= std::size(saved), "every saved banner index still exists");
    for (std::size_t i = 0; i < std::size(saved); ++i)
        require(list[i].name == saved[i], "saved banner indexes keep their banner");
    for (std::size_t i = 0; i < list.size(); ++i)
    {
        const auto image = list[i].image;
        const auto n = image.size();
        const bool png = n > 16 && image[0] == 0x89 && image[1] == 'P' && image[n - 4] == 0xAE && image[n - 1] == 0x82;
        const bool jpeg = n > 4 && image[0] == 0xFF && image[1] == 0xD8 && image[n - 2] == 0xFF && image[n - 1] == 0xD9;
        require(png || jpeg, "banner embeds a whole PNG or JPEG");
        for (std::size_t j = 0; j < i; ++j)
            require(list[j].name != list[i].name, "banner names are unique");
    }
}
struct drawn_box { float left, top, right, bottom; };
// Rectangles AddRectFilled painted in one colour, plus every vertex of another colour (text glyphs).
static void collect_draw_data(ImU32 fill, ImU32 ink, std::vector<drawn_box>& boxes, std::vector<ImVec2>& glyphs)
{
    boxes.clear(); glyphs.clear();
    const auto* data = ImGui::GetDrawData();
    for (int i = 0; data && i < data->CmdListsCount; ++i)
    {
        const auto& vtx = data->CmdLists[i]->VtxBuffer;
        for (int v = 0; v < vtx.Size; ++v)
        {
            if (vtx[v].col == ink) glyphs.push_back(vtx[v].pos);
            if (vtx[v].col == fill && v + 3 < vtx.Size && vtx[v + 1].col == fill && vtx[v + 2].col == fill && vtx[v + 3].col == fill)
            {
                boxes.push_back({vtx[v].pos.x, vtx[v].pos.y, vtx[v + 2].pos.x, vtx[v + 2].pos.y});
                v += 3;
            }
        }
    }
}
static void hud_tests()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1280,800};
    io.DeltaTime = 1.f/60.f;
    unsigned char* pixels; int w,h;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    io.Fonts->SetTexID(1);
    auto colors = astra::preset_theme(0);
    colors.panel = {0.10f,0.20f,0.30f,1.f};
    colors.field = {0.30f,0.20f,0.10f,1.f};
    colors.text = {0.90f,0.80f,0.70f,1.f};
    const ImU32 panel = astra::pack_color(colors.panel), ink = astra::pack_color(colors.text);
    std::vector<drawn_box> boxes;
    std::vector<ImVec2> glyphs;
    auto frame = [&](auto&& draw) { ImGui::NewFrame(); draw(); ImGui::Render(); };

    // Square corners keep each fill a plain quad, which collect_draw_data can find.
    auto square = colors;
    square.rounding = 0.f;
    square.background = {0.20f,0.10f,0.30f,1.f};
    const ImU32 window_fill = astra::pack_color(square.background);
    std::vector<astra::toast> toasts = {
        {"Ghost Mode", "On", {0.29f,0.69f,0.34f,1.f}},
        {"Teleport failed", "The waypoint is outside the loaded world, so the player stays where they are until the area around the "
            "marker finishes loading and the waypoint is set again.", {0.69f,0.29f,0.29f,1.f}},
    };
    std::vector<drawn_box> bars, bodies;
    frame([&]{ astra::draw_notifications(toasts, square); });
    collect_draw_data(window_fill, ink, bodies, glyphs);
    collect_draw_data(panel, ink, bars, glyphs);
    require(bars.size() == 2 && bodies.size() == 2, "each notification draws a title bar in the panel colour over a body in the window colour");
    require(bars[0].right == 1270.f && bars[0].top == 10.f && bodies[0].top == bars[0].bottom,
        "notifications start in the top-right corner with the message right under the title bar");
    require(bars[1].top >= bodies[0].bottom + 10.f, "notification cards stack without overlapping");
    require(bodies[1].bottom - bodies[1].top >= (bodies[0].bottom - bodies[0].top) + 3.f * ImGui::GetFontSize() - .5f,
        "a long message grows its card by every wrapped line");
    require(!glyphs.empty(), "notification text uses the theme text colour");
    std::vector<drawn_box> cards;
    for (std::size_t i = 0; i < bars.size(); ++i)
        cards.push_back({bars[i].left, bars[i].top, bars[i].right, bodies[i].bottom});
    for (const auto& g : glyphs)
        require(std::any_of(cards.begin(), cards.end(), [&](const drawn_box& c) {
            return g.x >= c.left + 11.f && g.x <= c.right - 11.f && g.y >= c.top + 7.f && g.y <= c.bottom - 7.f;
        }), "notification text stays inside its card's padding");

    auto studio = astra::preset_theme(4);
    studio.rounding = 0.f;
    const std::vector<astra::toast> info_toast = {{"Enable ESP", "On", astra::notice_color(astra::notice::info)}};
    frame([&]{ astra::draw_notifications(info_toast, studio); });
    // On a white title bar the pale info colour would be invisible; the dot is the one opaque colour that is not a fill or text.
    ImU32 dot = 0;
    const ImU32 known[] = {astra::pack_color(studio.panel), astra::pack_color(studio.background), astra::pack_color(studio.text)};
    for (int i = 0; i < ImGui::GetDrawData()->CmdListsCount; ++i)
        for (const auto& v : ImGui::GetDrawData()->CmdLists[i]->VtxBuffer)
            if ((v.col >> IM_COL32_A_SHIFT) == 0xFF && std::find(std::begin(known), std::end(known), v.col) == std::end(known))
                dot = v.col;
    require(dot != 0 && astra::contrast_ratio(ImGui::ColorConvertU32ToFloat4(dot), studio.panel) >= 4.5f,
        "the kind colour is shifted until it reads at AA on the title bar");

    toasts.resize(1);
    toasts[0].alpha = .5f;
    frame([&]{ astra::draw_notifications(toasts, square); });
    collect_draw_data(astra::pack_color({colors.panel.x, colors.panel.y, colors.panel.z, .5f}), ink, boxes, glyphs);
    require(boxes.size() == 1 && glyphs.empty(), "an expiring notification fades its card and text");
    toasts[0].alpha = 0.f;
    frame([&]{ astra::draw_notifications(toasts, square); });
    collect_draw_data(panel, ink, boxes, glyphs);
    require(boxes.empty() && glyphs.empty(), "an expired notification draws nothing");

    const astra::overlay_row rows[] = {{"Health", "250 / 250"}, {"Position", "-104.2, 31.5, 2210.9"}};
    bool interactive = true;
    auto overlay = [&]{ astra::draw_overlay("Player status", rows, colors, {18.f,18.f}, {0.f,0.f}, interactive); };
    frame(overlay); frame(overlay);
    const auto* window = ImGui::FindWindowByName("###astra_overlay");
    require(window && window->Pos.x == 18.f && window->Pos.y == 18.f && window->Size.x >= 240.f, "the overlay opens at its start position with its minimum width");
    collect_draw_data(panel, ink, boxes, glyphs);
    require(!glyphs.empty(), "overlay values use the theme text colour");
    for (const auto& g : glyphs)
        require(g.x >= window->Pos.x && g.x <= window->Pos.x + window->Size.x && g.y >= window->Pos.y && g.y <= window->Pos.y + window->Size.y,
            "overlay text stays inside its window");

    io.AddMousePosEvent(40.f, 28.f); frame(overlay);
    io.AddMouseButtonEvent(0, true); frame(overlay);
    io.AddMousePosEvent(140.f, 128.f); frame(overlay);
    io.AddMouseButtonEvent(0, false); frame(overlay);
    require(window->Pos.x == 118.f && window->Pos.y == 118.f, "dragging the overlay moves it, and the start position no longer applies");

    interactive = false;
    io.AddMousePosEvent(150.f, 150.f); frame(overlay); frame(overlay);
    require(!io.WantCaptureMouse, "with the menu closed the overlay takes no input");

    astra::info_window info;
    info.heading = "Player";
    info.rows = {{"", "Online player"}, {"Health", ""}, {"Position", "-104.2, 31.5, 2210.9"}};
    info.note = "Updates every second.";
    auto details = [&]{ astra::draw_info_window("Player Info", info, colors, {300.f, 40.f}, {360.f, 390.f}); };
    frame(details); frame(details);
    const auto* info_window = ImGui::FindWindowByName("Player Info");
    require(info_window && info_window->Pos.x == 300.f && info_window->Pos.y == 40.f && info_window->Size.x == 360.f && info_window->Size.y == 390.f,
        "the info window opens at its start position and size");
    collect_draw_data(panel, ink, boxes, glyphs);
    require(!glyphs.empty(), "info window text uses the theme text colour");
    for (const auto& g : glyphs)
        require(g.x >= 300.f && g.x <= 660.f && g.y >= 40.f && g.y <= 430.f, "info window text stays inside its window");
    ImGui::DestroyContext();
}
static void component_tests()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1280,800};
    io.DeltaTime = 1.f/60.f;
    unsigned char* pixels; int w,h;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    io.Fonts->SetTexID(1);
    int count = 5;
    bool changed = false;
    ImVec2 track_min{}, track_max{};
    auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize({320,200});
        ImGui::Begin("Panel", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        ImGui::SetNextItemWidth(200.f);
        changed |= astra::slider("Count##spawn", &count, 1, 50);
        track_min = ImGui::GetItemRectMin();
        track_max = {track_min.x + 200.f, ImGui::GetItemRectMax().y};
        ImGui::End();
        ImGui::Render();
    };
    frame();
    require(!changed && count == 5, "a slider leaves its value alone until dragged");
    const ImVec2 right_end{track_max.x - 2.f, (track_min.y + track_max.y) * .5f};
    io.AddMousePosEvent(right_end.x, right_end.y); frame();
    io.AddMouseButtonEvent(0, true); frame();
    io.AddMouseButtonEvent(0, false); frame();
    require(changed && count == 50, "dragging a component slider to its end sets the maximum as a whole number");
    ImGui::DestroyContext();
}
static void panel_component_tests()
{
    require(astra::matches_search("Iron Sword", "sWoRd"), "a search ignores letter case");
    require(!astra::matches_search("Iron Sword", "axe"), "a search leaves out text without the query");
    require(astra::matches_search("Iron Sword", ""), "an empty search matches everything");
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1280,800};
    io.DeltaTime = 1.f/60.f;
    unsigned char* pixels; int w,h;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    io.Fonts->SetTexID(1);
    std::string name = "ab";
    int picked = -1, snippet = 0, asked = 0;
    bool open = false;
    float choice_gap = -1.f;
    ImVec2 field_min{}, field_max{};
    std::vector<ImVec2> rows(3);
    auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize({400,700});
        ImGui::Begin("Panel", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        astra::input_text("##name", &name);
        field_min = ImGui::GetItemRectMin();
        field_max = ImGui::GetItemRectMax();
        const float before = ImGui::GetCursorScreenPos().y;
        const std::string snippets[] = {"Hello", "Teleport"};
        astra::choice("##snippet", &snippet, snippets);
        choice_gap = ImGui::GetItemRectMin().y - before;
        open = astra::section("Advanced", true);
        const int hit = astra::match_list("matches", 3, [&](int i) {
            rows[i] = ImGui::GetCursorScreenPos();
            return astra::list_item{"Row " + std::to_string(i), picked == i};
        }, {.height = 120.f});
        if (hit >= 0) picked = hit;
        astra::match_list("long", 10000, [&](int) { ++asked; return astra::list_item{"Item"}; }, {.height = 120.f, .count_label = nullptr});
        ImGui::End();
        ImGui::Render();
    };
    frame();
    require(open, "a section opened by default starts open");
    require(std::fabs(choice_gap) < .5f, "a choice whose label is only an ##id leaves out the line above the dropdown");
    asked = 0;
    frame();
    require(asked > 0 && asked < 40, "a long list asks only for the rows on screen");
    io.AddMousePosEvent(rows[1].x + 30.f, rows[1].y + 4.f); frame();
    io.AddMouseButtonEvent(0, true); frame();
    io.AddMouseButtonEvent(0, false); frame();
    require(picked == 1, "clicking a list row returns its index");
    io.AddMousePosEvent(field_max.x - 4.f, (field_min.y + field_max.y) * .5f); frame();
    io.AddMouseButtonEvent(0, true); frame();
    io.AddMouseButtonEvent(0, false); frame();
    const std::string typed(40, 'x');
    io.AddInputCharactersUTF8(typed.c_str()); frame();
    require(name == "ab" + typed, "a std::string field grows to hold what the player types");
    ImGui::DestroyContext();
}
static void menu_front_tests()
{
    ui_fixture f;
    f.list_mode = true;
    f.list.position = {40,40};
    f.list.width = 300;
    f.list.rows = 3;
    astra::control action;
    action.id = "action"; action.label = "Run";
    f.page.controls = {action};
    const auto display_index = [](const char* name) {
        const auto& windows = GImGui->Windows;
        for (int i = 0; i < windows.Size; ++i)
            if (std::strstr(windows[i]->Name, name)) return i;
        return -1;
    };
    bool open_popup = false;
    const auto frame = [&] {
        ImGui::NewFrame();
        f.menu.draw_list(f.page, astra::preset_theme(0), f.list);
        ImGui::SetNextWindowPos({120,80});
        ImGui::SetNextWindowSize({300,200});
        ImGui::Begin("Game panel");
        if (open_popup) { ImGui::OpenPopup("choices"); open_popup = false; }
        if (ImGui::BeginPopup("choices")) { ImGui::TextUnformatted("Choice"); ImGui::EndPopup(); }
        ImGui::End();
        ImGui::Render();
    };
    frame();
    ImGui::FocusWindow(ImGui::FindWindowByName("Game panel"));
    frame(); frame();
    require(display_index("##astra_list_input_root") > display_index("Game panel"), "the List stays in front of a game panel the player clicked");
    open_popup = true;
    frame(); frame();
    require(display_index("##Popup_") > display_index("##astra_list_input_root"), "a popup opened from a panel stays in front of the List");
}
static void menu_keep_out_tests()
{
    ui_fixture f;
    f.list_mode = true;
    f.list.position = {40,40};
    f.list.width = 300;
    f.list.rows = 3;
    astra::control action;
    action.id = "action"; action.label = "Run";
    f.page.controls = {action};
    // No banner: 60 for the header, 45 for the tabs, one row, then the 42 footer; the scrollbar sits 12 past the rows.
    const ImRect list{{40.f,40.f}, {40.f + 300.f + 12.f, 40.f + 60.f + 45.f + 45.f + 42.f}};
    bool menu_open = true;
    const auto frame = [&] {
        ImGui::NewFrame();
        f.menu.keep_windows_out();
        if (menu_open) f.menu.draw_list(f.page, astra::preset_theme(0), f.list);
        ImGui::SetNextWindowPos({100,60}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({200,150}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Game panel");
        ImGui::End();
        ImGui::SetNextWindowPos({120,90});
        ImGui::Begin("Censor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMouseInputs);
        ImGui::End();
        ImGui::Render();
    };
    const auto panel_box = [] {
        const auto* panel = ImGui::FindWindowByName("Game panel");
        return ImRect{panel->Pos, {panel->Pos.x + panel->Size.x, panel->Pos.y + panel->Size.y}};
    };
    frame(); frame(); frame();
    require(!panel_box().Overlaps(list), "a panel placed on the open List moves off it");
    const auto* censor = ImGui::FindWindowByName("Censor");
    require(censor->Pos.x == 120.f && censor->Pos.y == 90.f, "a window without a title bar or mouse input stays where the game put it");

    auto& io = ImGui::GetIO();
    const ImVec2 title{panel_box().Min.x + 30.f, panel_box().Min.y + 10.f};
    io.AddMousePosEvent(title.x, title.y); frame();
    io.AddMouseButtonEvent(0, true); frame();
    io.AddMousePosEvent(150.f, 120.f); frame(); frame();
    require(!panel_box().Overlaps(list) && panel_box().Min.x == title.x - 30.f + 20.f,
        "a panel dragged onto the List follows the mouse along its edge but stops there");
    io.AddMouseButtonEvent(0, false); frame();

    // Pressed against the List's right edge and pulled up, the panel used to jump below the List.
    const float right_edge = list.Max.x + 8.f;
    ImGui::SetWindowPos("Game panel", {right_edge, 60.f});
    frame(); frame();
    io.AddMousePosEvent(right_edge + 30.f, 70.f); frame();
    io.AddMouseButtonEvent(0, true); frame();
    io.AddMousePosEvent(130.f, 120.f); frame(); frame();
    require(panel_box().Min.x == right_edge && panel_box().Min.y == 110.f,
        "a panel pushed deep into the List from its side stays on that side and still follows the mouse up and down");
    io.AddMouseButtonEvent(0, false); frame();

    menu_open = false;
    frame();
    ImGui::SetWindowPos("Game panel", {100,60});
    frame(); frame();
    require(panel_box().Min.x == 100.f && panel_box().Min.y == 60.f, "a closed menu keeps nothing out");
}
static void panel_theme_tests()
{
    ImGui::CreateContext();
    astra::apply_theme(astra::preset_theme(4));
    const auto& style = ImGui::GetStyle();
    require(style.WindowRounding == 16.f && style.FrameRounding == 6.f, "the menu theme keeps its rounded windows and controls");
    astra::apply_theme(astra::panel_theme(astra::preset_theme(4)));
    require(style.WindowRounding == 0.f && style.ChildRounding == 0.f && style.FrameRounding == 0.f && style.GrabRounding == 0.f
        && style.ScrollbarRounding == 0.f && style.TabRounding == 0.f && style.PopupRounding == 0.f,
        "the panel theme squares every corner, like the List layout");
    ImGui::DestroyContext();
}
static void notification_queue_tests()
{
    using namespace std::chrono_literals;
    astra::notification_queue queue;
    const auto start = astra::notification_queue::clock::time_point{} + 1h;
    queue.push(astra::notice::info, "Enable ESP", "On", "i", 3.f, start);
    queue.push(astra::notice::success, "Teleport", "Teleported to spawn point.", {}, 3.f, start + 100ms);
    auto cards = queue.cards(start + 1s);
    require(cards.size() == 2 && cards[0].title == "Enable ESP" && cards[1].title == "Teleport", "cards keep the order they were pushed in");
    require(cards[1].color.y == astra::notice_color(astra::notice::success).y && cards[0].alpha == 1.f && cards[0].icon == "i",
        "a card takes its kind's colour and icon and shows fully at first");
    queue.push(astra::notice::info, "Enable ESP", "Off", "i", 3.f, start + 3s);
    cards = queue.cards(start + 3400ms);
    require(cards.size() == 2 && cards[0].title == "Enable ESP" && cards[0].message == "Off" && cards[0].alpha == 1.f,
        "a title still on screen keeps its card, which takes the new message in place and starts its time again");
    require(std::fabs(cards[1].alpha - .5f) < .01f, "a card fades out over 0.6 s once its time is up");
    cards = queue.cards(start + 3800ms);
    require(cards.size() == 1 && cards[0].title == "Enable ESP", "a card that has faded out is dropped");
}
static void template_tests()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1280,800};
    io.DeltaTime = 1.f/60.f;
    unsigned char* pixels; int w,h;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    io.Fonts->SetTexID(1);
    const char* actions[] = {"Teleport", "Delete"};
    astra::row_action hit{};
    std::vector<ImRect> last_button(3);
    const std::vector<astra::log_entry> log = {
        {"14:02:11", astra::log_level::info, "Script loaded"},
        {"14:02:15", astra::log_level::warning, "Timer fired late"},
        {"14:02:20", astra::log_level::error, "ReferenceError: horse is not defined"}};
    astra::log_filter filter;
    bool interactive = false;
    const auto colors = astra::preset_theme(0);
    const astra::inspector wolf{.name = "Wolf", .tag = "2 stars", .bar = .5f, .rows = {{"Health", "100 / 200"}}, .actions = {"Instant Tame"}};
    auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize({460,780});
        ImGui::Begin("Panel", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        const auto clicked = astra::action_list("bookmarks", 3, [&](int i) {
            // Called at the start of each row, so the last item is still the previous row's final button.
            if (i > 0) last_button[i - 1] = {ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
            return astra::action_row{"Place " + std::to_string(i), "X 1  Y 2"};
        }, actions, {.height = 140.f});
        if (clicked.row >= 0) hit = clicked;
        astra::log_view("console", log, filter, 200.f);
        ImGui::End();
        astra::draw_inspector("Inspector", wolf, colors, {700,40}, {0,0}, interactive);
        astra::begin_panel("Bookmarks panel", nullptr, {900,300}, {.size = {300,200}, .collapsible = false});
        astra::end_panel();
        ImGui::Render();
    };
    const auto log_height = [] {
        for (auto* window : GImGui->Windows)
            if (std::strstr(window->Name, "##log_entries")) return window->ContentSize.y;
        return -1.f;
    };
    frame(); frame();
    const auto target = last_button[1].GetCenter();
    io.AddMousePosEvent(target.x, target.y); frame();
    io.AddMouseButtonEvent(0, true); frame();
    io.AddMouseButtonEvent(0, false); frame();
    require(hit.row == 1 && hit.action == 1, "clicking a row's second button reports that row and action");
    const float every_level = log_height();
    filter.warning = filter.error = false;
    frame(); frame();
    const float info_only = log_height();
    require(every_level > 0 && info_only > 0 && info_only < every_level, "turning off log levels hides their entries");
    auto* card = ImGui::FindWindowByName("Inspector");
    require(card && (card->Flags & ImGuiWindowFlags_NoInputs) && std::fabs(card->Size.x - 300.f) < .5f,
        "an inspector outside the menu is a 300-wide card that takes no input");
    interactive = true; frame();
    require(!(ImGui::FindWindowByName("Inspector")->Flags & ImGuiWindowFlags_NoInputs), "an interactive inspector takes input");
    auto* panel = ImGui::FindWindowByName("Bookmarks panel");
    require(panel && panel->Pos.x == 900.f && panel->Pos.y == 300.f && panel->Size.x == 300.f && panel->Size.y == 200.f
        && (panel->Flags & ImGuiWindowFlags_NoCollapse), "a panel opens where it is placed, at its size, and can be kept from collapsing");
    ImGui::SetWindowSize("Bookmarks panel", {420.f, 260.f}); frame();
    require(panel->Size.x == 420.f && panel->Size.y == 260.f, "the player can make a panel bigger");
    ImGui::SetWindowSize("Bookmarks panel", {120.f, 80.f}); frame();
    require(panel->Size.x == 300.f && panel->Size.y == 200.f, "a panel cannot be made smaller than the size it opens at");
    ImGui::DestroyContext();
}
int main()
{
    try { navigation_tests(); option_binding_tests(); key_settings_tests(); key_settings_ui_tests(); interaction_tests(); window_hierarchy_tests(); list_mouse_tests(); theme_color_tests(); banner_tests(); hud_tests(); component_tests(); panel_component_tests(); menu_front_tests(); menu_keep_out_tests(); panel_theme_tests(); notification_queue_tests(); template_tests(); std::puts("Astra regression tests passed"); return 0; }
    catch(const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
