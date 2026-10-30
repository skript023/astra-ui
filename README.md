# Astra

A standalone C++23 menu library built on Dear ImGui. Navigation, data bindings, themes, List layout, and clickable Window layout are separate modules. There are no Unity/Unreal types, game globals, process hooks, or project precompiled headers in this package.

The Studio theme uses a light workspace, purple accents, a custom header, and thin sliders. Window layout places tabs across the header and outer submenus in the sidebar, with nested settings grouped in the content area. Emerald, Violet, and Ocean are dark alternatives. The host can supply any `theme` palette.

## Modules

| Target / header | Responsibility |
| --- | --- |
| `astra::core` | Header-only page/control data, value validation, per-tab navigation |
| `astra::ui` | ImGui widgets, theme scope, animated colors, List and Window views, notifications and overlay |
| `astra::banners` | Optional built-in header banners as encoded images, in a fixed order |
| `astra::win32` | Optional upstream ImGui Win32 input backend |
| `astra::dx9` | Optional upstream ImGui DirectX 9 renderer |
| `astra::dx11` | Optional upstream ImGui DirectX 11 renderer |
| `astra::dx12` | Optional upstream ImGui native DirectX 12 renderer |
| `astra::vulkan` | Optional upstream ImGui Vulkan renderer, dynamically supplied functions |
| `astra::opengl` | Optional upstream ImGui OpenGL 3/4 renderer |

The graphics targets compile official ImGui backends. They do not discover a game's device or install presentation hooks. The host owns device/context creation, backend initialization, texture lifetime, frame boundaries, render submission, resize/reset, synchronization, and shutdown. A UI package supporting an API is distinct from a particular game having an adapter for that API.

OpenGL here means the modern OpenGL3 backend; legacy fixed-function OpenGL2 is not included.

## CMake integration

Astra requires C++23 or newer. Linking `astra::core` or `astra::ui` propagates this requirement to the consuming target through CMake compile features. C++26 is not required because the currently installed MSVC/CMake toolchain does not advertise `cxx_std_26` support.

Use a local checkout:

```cmake
include(FetchContent)
FetchContent_Declare(astra SOURCE_DIR "/path/to/astra")
FetchContent_MakeAvailable(astra)
target_link_libraries(my_app PRIVATE astra::ui)
```

The package reuses an existing `imgui` target by default. That target must publish its matching ImGui include path. Set `ASTRA_IMGUI_TARGET` for a differently named target. Without an existing target, the package fetches its pinned ImGui commit and builds the core.

To build a backend:

```cmake
set(ASTRA_BACKENDS "dx11" CACHE STRING "")
# Or: "dx9;dx11;dx12;vulkan;opengl"
FetchContent_MakeAvailable(astra)
target_link_libraries(my_app PRIVATE astra::dx11)
```

If the host already compiles ImGui backends, leave `ASTRA_BACKENDS` empty and link `astra::ui` only. Compiling the same backend twice creates duplicate symbols. If supplying an existing ImGui target and enabling packaged backends, set `ASTRA_IMGUI_SOURCE_DIR` to that same version's source directory.

Vulkan needs headers, fetched at v1.3.290 if `Vulkan::Headers` does not exist. It does not require a Vulkan SDK import library. The host must load Vulkan functions before initializing its backend.

This folder can become its own repository without code changes. Once published, replace `SOURCE_DIR` with the actual `GIT_REPOSITORY` and a pinned commit in `GIT_TAG`. No remote URL has been invented or published by this change. CMake's `FETCHCONTENT_SOURCE_DIR_ASTRA` can override a fetched checkout during development.

## Model and rendering

```cpp
astra::canvas canvas;                    // persist across frames
astra::navigation<unsigned> navigation; // persist across frames
bool open = true;
bool enabled = false;

// Between ImGui::NewFrame() and ImGui::Render():
astra::page page;
page.id = "settings";                       // stable page identity
page.title = "Settings";
page.tabs = {"Settings"};
page.breadcrumbs = {"Settings"};

astra::control option;
option.id = "enabled";                      // stable within the page
option.label = "Enable feature";
option.kind = astra::control_kind::toggle;
option.checked = enabled;
option.activate = [&] { enabled = !enabled; };
page.controls.push_back(std::move(option));

const auto event = canvas.draw(
    astra::layout::window,
    "My menu###stable_menu_id", page, open, astra::preset_theme(4));
```

Call `canvas.draw(astra::layout::list, id, page, open, theme, list_style)` for the raw-drawn List view. Both layouts consume the same model through the shared `astra::canvas` facade. The host routes keyboard input to navigation and option bindings. The List shows three tabs at a time with the selected tab in the middle, so the next tab always shows beside it: with Self, Mission, World, and ESP, selecting World shows Mission, World, and ESP. Only the first and last tabs sit at the edge.

List mouse input uses transparent hitboxes aligned with the existing drawing. Moving the cursor onto a row selects it without activating it; a cursor that rests on the list leaves keyboard selection alone, and a dragged slider keeps its own row selected. Click rows to activate actions/toggles; click the displayed number/choice arrows to decrement or increment; drag a toggle-number slider without changing its checkbox. Numeric arrows use `step`; slider dragging is continuous and rounds integral values. Wheel over rows scrolls the list, wheel over tabs changes tabs, and the scrollbar supports track clicks and dragging. Right-click inside a submenu to go back. Set `list_style::mouse_enabled=false` to disable mouse capture and interaction.

Apply returned `option` events to `page.selected_option` and `tab`/`back` events to navigation on the next frame. Scrolling can return an option event to keep selection visible; it never invokes the option callback. Refresh control values each frame after setters run. Mouse callbacks execute after drawing ends.

Window actions are deferred until the old window has finished rendering, then run once. The host applies tab and legacy breadcrumb events to its `navigation` instance.

For the grouped Window layout, supply the selected tab's root page each frame (one breadcrumb). Each outer `submenu` control supplies `children`, a function returning its current controls. These outer controls become sidebar entries; nested submenus with `children` become collapsible groups in the content area. Selection is remembered per root page and tab, with a fallback when the selected section disappears. Direct root controls remain accessible through General when the root also has submenus. Tabs with no submenus use the full content width.

`children` only describes settings: it must not navigate, invoke option actions, or invalidate controls returned earlier in the same frame. The renderer refreshes the selected section's tree each frame, so bindings stay live; search also finds controls inside nested groups. Inline groups do not call their submenu's `activate` callback. Leaf actions use their own callbacks; their Window `option` event is not an index into the original root's controls. List ignores `children` and keeps using `activate` for submenu navigation. Legacy flat pages without `children` keep button-based navigation, and only their root-level submenus enter the sidebar.

Enable `ImGuiConfigFlags_NavEnableKeyboard` in the host for Window keyboard navigation. Keep the host's list hotkeys scoped to List layout so they do not also activate a hidden list option while interacting with Window controls.

Control kinds: action, submenu, toggle, number, toggle + number, and choice. Number controls provide bounds, integer/floating-point mode, precision, and a direct setter; dragging and Ctrl+click typing share validation. `step` is host metadata for keyboard increments; Window sliders permit continuous changes within bounds, rounded for integral controls. Choice controls can also supply an explicit Apply callback for selectors that require confirmation.

Callbacks and captured objects must remain valid until the drawing call returns. Rebuild the model each frame to display current values. Stable IDs keep input focus and animations independent of object addresses.

The library never creates or destroys the host's ImGui context. Themes restore the host's complete ImGui style after drawing. View search and theme animation state are per `menu` instance.

## Game input

`host/canvas` decides which input the open menu keeps from the game, so every game host behaves the same way. Call `canvas::block_game_message` first in the game's window procedure, after ImGui has seen the message:

```cpp
LRESULT hooked_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    imgui_wndproc(hwnd, msg, wparam, lparam);
    if (auto result = big::canvas::block_game_message(hwnd, msg, wparam, lparam))
        return *result;
    return CallWindowProcW(original_wndproc, hwnd, msg, wparam, lparam);
}
```

While the cursor is over the menu, or a mouse button holds one of its widgets, the game loses mouse messages, raw mouse input, and pointer wheel messages. Unity reads the wheel from `WM_POINTERWHEEL`; the menu passes those to `DefWindowProc`, which turns them into the `WM_MOUSEWHEEL` ImGui scrolls with. The keyboard is held back only while a menu text field is typing or the key editor is open, so the player keeps walking while clicking the menu. Key releases always reach the game, so a key held before the menu took the keyboard never sticks.

Engines also read input outside the window procedure. `canvas::captures_game_mouse()`, `captures_game_keyboard()`, and `captures_game_input()` (either one) report the same state for those hooks. The Valheim host, for example, stops mouse look in `PlayerController.TakeInput` on `captures_game_input()`, stops movement on `captures_game_keyboard()`, and drops attack and block in `Player.SetControls` on `captures_game_mouse()`. The RDR2 host disables the attack, aim, and weapon switch control actions on `captures_game_mouse()` and every control action on `captures_game_keyboard()`, once per script tick.

Call `canvas::prepare_imgui_mouse()` before `ImGui::NewFrame`. ImGui then draws its cursor and reads the mouse only while the menu is open and uses the mouse (Window layout, Draw Mouse, or the key editor), so a closed menu never leaves a cursor on screen and the Window layout stays clickable without Draw Mouse. While the menu does not use the mouse, ImGui also leaves the OS cursor to the game (`ImGuiConfigFlags_NoMouseCursorChange`). The menu's cursor reopens where the player left it, since the hidden cursor keeps moving with mouse look while the menu is closed; the canvas moves it back before the menu counts as open, so a host's `SetCursorPos` hook still lets that call through. A host that hooks `SetCursorPos` should block it while `canvas::uses_mouse()`, so the game cannot pull the cursor back to the centre. Unity also confines the cursor with `ClipCursor`; the Valheim host releases that clip under the same condition.

`block_game_message` replaces `captures_message(msg)`, which held the keyboard whenever ImGui held a widget and swallowed all raw input while the cursor was over the menu.

## Banners

Link `astra::banners` and include `<astra/banners.hpp>` to get the header banners every game menu offers. `astra::banners()` returns each banner's name and its encoded PNG or JPEG bytes; the host decodes and uploads them with its own renderer, the same way it owns every other texture.

```cpp
for (const auto& banner : astra::banners())
    load_banner(std::string(banner.name), banner.image.data(), banner.image.size());
```

Hosts save the chosen banner as an index into this list, so the order in `assets/banners/banners.txt` is fixed and a new banner goes at the end. To add one, put the image in `assets/banners` and append a line with its file name and display name. The build turns the images into byte arrays (`cmake/embed_banners.cmake`), and only hosts that link `astra::banners` compile them.

## Settings page

`canvas::add_settings_tab` registers the Settings tab every game shares, so a new menu setting is added once here instead of in each game. Rows only the game knows how to do come from the callback:

```cpp
canvas::add_settings_tab(SubmenuSettings, SubmenuSettingsInput, [](regular_submenu* sub) {
    sub->add_option<reguler_option>("Unload", nullptr, [] { g_running = false; });
    sub->add_option<reguler_option>("Exit", nullptr, [] { exit(0); });
});
```

The tab holds Menu Layout, Theme, Banner, Transparency, the Custom theme colours, Input (the navigation delays, registered as a submenu under the second id), Menu controls, List position and width, and Draw Mouse, followed by the callback's rows. Banner appears once the host has filled `g_gui.m_banner_names`, and picking one calls `g_gui.update_banner()`.

These rows edit `canvas::config()`, a `menu_config` from `<astra/host/menu_config.hpp>`, and the canvas saves it as the `menu` object of settings.json (see below), so a game keeps no menu settings of its own. Read and change it from the render thread; the canvas compares it with the saved copy a few times a second and marks the component dirty, and the next `settings::tick()` writes it. A value missing from the file keeps its default. Session-only state, such as whether a game's own text prompt is showing, stays out of it: such a prompt calls `canvas::hold_mouse(true)` while it shows, and the menu keeps the mouse and keyboard as if Draw Mouse were on. host/canvas.cpp therefore needs host/settings.cpp in the game's sources.

## Notifications and overlay

`<astra/hud.hpp>` draws the two things a game shows outside the menu, in the same theme as the menu. `astra::draw_notifications` stacks cards down the top-right corner, each built like the overlay window: the icon and title in a title bar of the theme's panel colour, the message wrapped below on the window colour, the theme's rounding (square in a game, which passes `panel_theme`), and a faint border. The icon, or a dot when there is none, takes the notification's kind colour, shifted toward the text colour as far as it takes to read at WCAG AA on the title bar; the title and message use the theme's text colour. Each card is as tall as its wrapped text, and `toast::alpha` fades the whole card out. `astra::draw_overlay` draws a read-only window of label and value rows in the same style as the game's other panels. While it is interactive it has a title bar and can be dragged; otherwise it is a bare read-out that takes no input, and the canvas wrapper makes it interactive only while the menu is open, so it never takes a click from play. The position and pivot place it only the first time; after that ImGui keeps where it was left and saves it in the host's ImGui ini file. The canvas wrapper opens it in the panel column described below.

Game hosts never build cards themselves. `<astra/host/notification.hpp>` gives every game the same calls, usable from any thread, such as a game or script thread:

| Call | Card |
| --- | --- |
| `notification::info(title, message)` | info colour, info icon |
| `notification::success(title, message)` | green, check icon |
| `notification::warning(title, message)` | amber, warning icon |
| `notification::error(title, message)` | red, cross icon |
| `notification::protection(message)` | blue, "Illegal Event Protection" |
| `notification::report(message)` | blue, "Report Protection" |

Each card shows for 3 seconds and then fades out over 0.6 seconds. A title that is still on screen keeps its card: the card takes the new kind, message, and icon where it is and starts its time again, so a feature toggled on and off by a hotkey shows one card with its latest state. `canvas::draw_notifications()`, called once a frame, draws them in the menu's theme. A host without the canvas uses `astra::notification_queue` and `astra::notice_color` from `<astra/hud.hpp>` directly; host/notification.cpp adds the icons from the game's `fonts/icon_list.hpp`, so a game adds it to its sources next to host/canvas.cpp.

Show a message the player should see through these calls rather than the game's own message HUD, so it looks the same in every game.

`astra::draw_info_window` is the details window a game shows for something selected, such as an online player: a heading above a separator, "Label: value" lines, and an optional note below a closing separator. A row with an empty value reads "Not available", so missing data never shows as a placeholder number, and `info_window::message` replaces everything else while nothing is selected. Game hosts call `canvas::draw_info_window(title, content)`, which opens it in the panel column.

```cpp
astra::info_window info;
info.heading = player.name;
info.rows = {{"", "Online player"}, {"Platform", player.platform}, {"Health", health_text}};
info.note = "Updates every second.";
canvas::draw_info_window("Player Info", info);
```

## Feature settings and hotkeys

`<astra/host/settings.hpp>` and `<astra/host/state_serializer.hpp>` hold settings.json in the mod folder, the file the Astra loader edits. Each `state_serializer` is one top-level object of the file, such as the game's feature states or its hotkeys, and constructing one registers it, so a static instance loads with everything else. The game supplies the components; Valheim calls its feature states `commands` and RDR2 calls them `features`, and the loader's profile for each game names which.

The loader can change the file while the game runs, and the mod applies the change without a restart:

| Step | Call | Thread |
| --- | --- | --- |
| Load every component, then stamp `"astra": {"live_reload": 1, "pid": <process id>}` | `settings::initialize(file)` | start-up |
| Save dirty components, and read keys that differ from what the mod last read or wrote | `settings::tick()` | any, once a second |
| Hand those keys to their components | `settings::apply_external()` | the one that switches features, once a frame |
| Switch features from their keys | `hotkeys::tick(game_window)` | the same one, once a frame |

The stamp tells the loader that its edits apply live, so it keeps the feature panel editable while the game runs. Only keys that changed on disk count as outside edits, so an in-game change that is not saved yet survives, and a component with edits still waiting is not saved over them. The file is written to a temporary file and renamed, so neither side ever reads half a file.

A component's `apply_external_impl(state, changes)` receives the whole component and the changed keys, with null for a removed key; by default it reloads the component. A game's command list overrides it to call `set_state` on a bool command, so the feature switches on or off exactly as a menu click does.

`<astra/host/hotkeys.hpp>` reads the `hotkeys` object, `{"<command name>": <virtual-key code>}`. A key press switches that bool command and shows an info card with the command's label and On or Off. It switches through `set_state` rather than `call`, because RDR2 runs `call` later on its script thread and the card must show the new state. Keys count only while the game window is in front, ImGui is not taking text, and Alt is up, so typing in a menu field and Alt+Tab never switch a feature. The hotkey does not stop the key from reaching the game.

host/settings.cpp and host/hotkeys.cpp go in the game's sources next to host/canvas.cpp. They use the game's `file_manager/file.hpp`, `commands/commands.hpp`, `commands/bool_command.hpp` with `commands::get_bool_command`, and `utility/joaat.hpp`.

## Components

`<astra/components.hpp>` holds the controls for a game's own panels, so every panel in every game looks alike. The Window layout draws its dropdowns, checkboxes, and buttons with the same code; its sliders keep the menu's thin track, while panels get a framed slider.

| Component | What it draws |
| --- | --- |
| `slider(label, &value, min, max, ...)` | a framed slider with the value inside the frame and the label on its right; Ctrl+click types a value, kept within the bounds. Overloads take `int`, `float`, or `double`, with an optional precision and unit suffix |
| `input_number(label, &value, ...)` | a typed number: `int` with - and + buttons, `float` with a precision and suffix |
| `input_vector3(label, xyz, ...)` | three numbers side by side, such as a position or rotation |
| `choice(label, &index, items)` | the label above a full-width dropdown; items are `const char*` or `std::string`. A label that is only an `##id` leaves out the line above |
| `checkbox(label, &value)` | the theme's checkbox |
| `color_picker(label, &color)` | a colour swatch beside its label; clicking it opens a picker |
| `button(label, size)`, `small_button(label)` | a button, and a compact one for actions inside a list row |
| `input_text(label, buffer, size, hint, flags)` | a text field over a `char` buffer |
| `input_text(label, &text, hint, flags)` | a text field over a `std::string` that grows as the player types |
| `text_editor(label, &text, size, flags)` | multi-line text, such as a script editor; `ImGuiInputTextFlags_ReadOnly` shows text to select and copy |
| `search_field(id, buffer, size, hint)` | a full-width search field |
| `matches_search(text, query)` | not drawn: true when `text` contains `query`, ignoring letter case |
| `match_list(id, count, item, options)` | "Matches: N" above a bordered, scrolling list of selectable rows, with an optional icon and tooltip per row and an empty message. Returns the clicked row, or -1 |
| `section(label, open_by_default)` | a header that folds away the controls under it |
| `progress_bar(fraction, text, height)` | a thin bar, or a full-height bar with text on it |
| `text_accent(format, ...)` | text in the theme's accent, for the one line that matters most |
| `details(id, rows)` | label and value rows in two columns, labels muted; an empty value reads "Not available" |
| `tooltip(text)` | a tooltip while the control drawn just before is hovered |

```cpp
astra::slider("Spawn distance", &distance, 1.f, 50.f, 0, " m");
astra::choice("World spawn position", &position, positions);
if (astra::button("Spawn in World"))
    spawn();
```

A search list keeps the filtering in the game, which knows what each row matches on, and leaves the drawing to `match_list`. `item(i)` runs only for the rows on screen, so a catalogue of thousands costs a few rows a frame. Selection is whatever the game tracks, such as a prefab name, so it survives the filter changing:

```cpp
astra::search_field("item_search", search, sizeof(search), "Search item name or prefab ID");
std::vector<const item_entry*> found;
for (const auto& item : items)
    if (astra::matches_search(item.name + " " + item.prefab, search))
        found.push_back(&item);
const int clicked = astra::match_list("items", static_cast<int>(found.size()), [&](int i) {
    return astra::list_item{.label = found[i]->name, .selected = found[i]->prefab == selected_prefab, .icon = icon_for(*found[i])};
}, {.height = 180.f, .icon_size = 32.f});
if (clicked >= 0)
    selected_prefab = found[clicked]->prefab;
```

Tabs, tables, and child windows need no wrapper: `apply_panel_theme` already gives ImGui's own versions the theme.

## Templates

The windows and panel parts the Valheim and RDR2 hosts already show, so the next game starts from them instead of rebuilding them. The data stays in the game; the template draws it.

| Template | Taken from | Game call |
| --- | --- | --- |
| Panel window | every spawner, browser, and console panel | `canvas::begin_panel(title, &open, options)` ... `canvas::end_panel()` |
| Status overlay | Player status (Valheim), Information Overlay (RDR2) | `canvas::draw_overlay(title, rows)` |
| Info window | Player Info (both) | `canvas::draw_info_window(title, info)` |
| Inspector | Animal Inspector (Valheim) | `canvas::draw_inspector(title, content)` |
| Notifications | `notification_service` (both) | `notification::info(title, message)` and the rest; `canvas::draw_notifications()` |
| Search list | Item, Creature, and Prefab Spawner; Inventory Editor | `astra::search_field` + `astra::match_list` |
| Action list | World Bookmarks, Global Key Editor, Status Effect Browser | `astra::action_list(id, count, row, actions, options)` |
| Console log | Javascript Console (both) | `astra::log_view(id, entries, filter)` |

`begin_panel` opens the panel at `options.size` in the column beside the menu the first time, like `place_panel`. The player can resize it, but not below `options.min_size`, which defaults to the opening size, so the content is never squeezed until labels are cut off. The floor also lifts a smaller size saved in the ini file from before. Pick an opening size that fits the content; give a smaller `min_size` only when the content still reads at it, such as text that wraps. `fit_content = true` sizes a small panel with fixed content to that content instead, with no resize grip. Panels fold to their title bar by default; set `collapsible = false` for one that should not. A field that shares a row with a button should leave exactly the button's width (`GetContentRegionAvail().x - button - ItemSpacing.x`), so the button never runs past the panel's edge. Call `end_panel` whatever `begin_panel` returns:

```cpp
static bool open = false;
if (canvas::begin_panel("World Bookmarks", &open, {.size = {520.f, 460.f}}))
{
    const char* actions[] = {"Teleport", "Delete"};
    const auto clicked = astra::action_list("bookmarks", static_cast<int>(saved.size()), [&](int i) {
        return astra::action_row{saved[i].name, std::format("X {:.1f}  Y {:.1f}  Z {:.1f}", saved[i].x, saved[i].y, saved[i].z)};
    }, actions, {.count_label = "Saved locations", .empty_text = "No saved locations."});
    if (clicked.action == 0)
        teleport(saved[clicked.row]);
    if (clicked.action == 1)
        remove(clicked.row);
}
canvas::end_panel();
```

`draw_inspector` is the card for what the player is looking at: a name with a muted tag, an optional bar, label and value rows, one accent hint, and action buttons. It starts centred just below the crosshair. While the menu is closed it is a bare card that takes no input and hides its buttons; while the menu is open it can be dragged and its buttons work:

```cpp
const astra::inspector card{.name = "Wolf", .tag = "2 stars", .bar = health / max_health,
    .rows = {{"Health", "164 / 200"}, {"Status", "Taming 45%"}},
    .hint = "Press [T] to tame instantly", .actions = {"Instant Tame", "Heal Full"}};
if (const int action = canvas::draw_inspector("Animal Inspector", card); action == 0)
    tame();
```

`log_view` draws the console: level checkboxes, a text filter, Auto-scroll, Copy, and Clear, over the entries with their time and level in words. Keep the `log_filter` between frames; `log_view` returns true when the player presses Clear, and the game empties its own log. Here `to_astra_level` stands for the game's own mapping from its log levels onto `astra::log_level`:

```cpp
static astra::log_filter filter;
std::vector<astra::log_entry> entries;
for (const auto& e : javascript_manager::get_console_entries())
    entries.push_back({e.time, to_astra_level(e.level), e.message});
if (astra::log_view("console", entries, filter, 0.f, g_renderer->m_monospace_font))
    javascript_manager::clear_console();
```

Call `canvas::apply_panel_theme()` before `ImGui::NewFrame` so the game's own windows take the menu theme, the same style the Window layout uses (`astra::apply_theme`), with the List layout's square corners (`astra::panel_theme`). The canvas draws the overlay, info window, inspector, and notifications with that square theme too, so every window a game shows matches the List menu: the theme's background, panel, field, and accent replace ImGui's colours, child window borders fade to a faint line, and the menu's transparency applies to them too. A window that pushes its own colours keeps them. A panel that highlights text, such as the selected item in a list, uses `astra::text_accent`, which draws the accent at full alpha; every preset's accent reads at WCAG AA or better on its window background.

Call `canvas::place_panel(name)` right before `ImGui::Begin(name, ...)` for any other window the game opens, such as a player info or spawner window. A window that has no saved position yet opens in a column to the right of the menu, level with the List's tabs (or the top of the Window layout's window), below the panels already showing there; when the column runs out of room it continues in a new column to its right. Once the player drags a window, ImGui keeps and saves that position. Call `canvas::keep_windows_off_menu()` right after `ImGui::NewFrame()`, before anything is drawn: while the menu is open, a window the player drags onto it stops at the edge it came from and slides along that edge, and a window already on it moves to the nearest side that stays on screen. It moves only windows the player can drag; windows without a title bar or mouse input, popups, and tooltips stay where the game put them. The menu also stays in front of these windows in both layouts; a dropdown or popup opened from a panel or the menu still shows in front of the menu, and the Menu controls dialog stays in front of the menu. The List draws the highlighted row's description card behind the game's windows, so it never covers a panel; a panel beside the List covers the card instead. Option labels and descriptions have no length limit; only the right-hand value text is a 64-byte buffer.

Game hosts call the canvas wrappers, which use the menu's current theme and transparency:

```cpp
notification::success("Teleport", "Teleported to spawn point.");
canvas::draw_notifications();

const astra::overlay_row rows[] = {{"Health", "250 / 250"}, {"Position", "-104.2, 31.5, 2210.9"}};
canvas::draw_overlay("Player status", rows);
```

Notification text uses the theme's text colour on its panel and window colours, which reads at WCAG AA on every preset theme. The overlay rows use the text colour rather than `muted`, which falls below AA on Studio's white panel.

## Theme colours

`astra::theme` carries the palette plus the List highlight pair. `preset_theme` index 0 is Emerald, 1 Violet, 2 Ocean, 3 Custom (a palette the host supplies), 4 Studio, and 5 Snow (dark navy with a white highlight); `preset_theme_names` holds these names for a theme picker. Index 3 has no built-in look, so it equals the Emerald base until the host overrides it. `theme.selection` defaults to the accent and `theme.selection_text` to the background.

| Field | List layout | Window layout |
| --- | --- | --- |
| `background` | selected row's checkbox border only | window background |
| `panel` | rows, footer, description card | panel and child background |
| `field` | inactive tabs, scrollbar track | input frames, buttons |
| `text` / `muted` | row, tab, and footer labels | widget labels, disabled text |
| `accent` | toggle check, slider, scrollbar grab, section separators | check marks, slider grabs, hover/active frames |
| `selection` / `selection_text` | selected row and active tab fill and label | not used |

A host that only overrides `background` therefore sees no change in the List body: the List paints its body with `panel`. Every field above is animated when the target palette changes; call `canvas.reset_theme()` to snap to a new palette instead of animating, which is what a colour picker wants while it is being dragged.

## Build, tests, and demo

### Menu controls

The demo exposes **Settings > Menu controls** in both layouts. The game adapter shows **Menu controls** on the shared Settings tab, below Input. Click the current binding, then press the new keyboard key when it shows **Press a key...**. Select **Apply** to confirm. **Escape** or **Stop recording** cancels recording without changing that binding; Escape is reserved for cancellation while recording. Keys already held when recording starts are ignored until released and pressed again. Recording stops if the panel loses focus. Bindings are single keys, not modifier combinations. **Cancel** leaves existing keys unchanged; **Reset defaults** changes the draft and still requires Apply. Conflicting assignments are rejected, including conflicts with the menu open key.

| Action | Reset default |
| --- | --- |
| Open / close menu | Insert |
| Back | Backspace |
| Select / activate | Enter |
| Move through list | Up / Down |
| Change value or choice | Left / Right |
| Previous / next tab | Page Up / Page Down |

The panel pauses menu hotkeys and list input while editing, enables mouse input even for a keyboard-only list, and temporarily enables ImGui keyboard navigation. Existing controller bindings are unchanged. The List renderer and its appearance are unchanged.

The adapter writes `canvas::config().keys` after Apply, and the canvas saves them with the rest of the menu settings in the `menu` object of settings.json, as `"keys": {"open": 45, "back": 96, ...}`. Demo changes last for the current run. Standalone hosts can include `astra/key_settings.hpp`, call `open(current_keys)`, then commit the optional value returned by `draw(theme)`. Values use Windows virtual-key codes. Custom submenu implementations outside `base_submenu` must implement `add_ui_option` to accept the adapter's controls entry.

From a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -G Ninja -DASTRA_BUILD_TESTS=ON -DASTRA_BUILD_EXAMPLE=ON "-DASTRA_BACKENDS=dx9;dx11;dx12;vulkan;opengl"
cmake --build build -j 4
ctest --test-dir build --output-on-failure
./build/astra_demo.exe
```

The demo uses DX11 WARP and sample controls; it has no game dependency. It uses the Windows Segoe UI font, falling back to ImGui's font. **Demo controls** edits the Custom palette and selects Custom as soon as a colour changes, so the same hand-off the game adapter performs can be tried without a game. The List layout shows the built-in banners the way the game menus do: **Settings > Banner** picks one, and the demo decodes them with Windows Imaging Component, standing in for a game's own decoder. Generate a preview without showing a window:

```powershell
./build/astra_demo.exe --snapshot preview.bmp
./build/astra_demo.exe --snapshot-list list-preview.bmp
./build/astra_demo.exe --snapshot-custom custom-preview.bmp
./build/astra_demo.exe --snapshot-hud hud-preview.bmp 5
./build/astra_demo.exe --snapshot-components components-preview.bmp 4
./build/astra_demo.exe --snapshot-templates templates-preview.bmp 4
```

`--snapshot-hud` shows sample notifications and the information overlay in the theme index given after the file name. In the running demo, **Run action** reports each run as a notification while **Show notifications** is on, and **Settings > Information overlay** shows the display size, frame rate, and theme.

`--snapshot-components` renders every panel component in one window, in the theme index given after the file name, on a taller 1100 x 1040 frame. In the running demo, **Settings > Component gallery** opens the same window.

`--snapshot-templates` renders the templates with sample data: a bookmarks panel built from `action_list`, a console built from `log_view`, an inspector, and an info window. In the running demo, **Settings > Template gallery** opens them; Teleport and the inspector's buttons report a notification, and Delete and Clear change the sample data.

`--snapshot-custom` renders the Custom index with a host palette, so its output matching `--snapshot` shows that the palette argument, not the built-in fallback, is what reaches the screen.

Regression tests exercise per-tab history, Back/root boundaries, ancestor breadcrumbs, invalid destinations, numeric bounds, actual mouse clicks on actions/toggles/sliders/dropdowns/sidebar/Back, List hover selection, Ctrl+click numeric text entry, style restoration, theme colour animation, empty List rendering, notification and overlay layout (wrapped text stays inside its card, cards stack and fade, dragging moves the overlay), the notification queue (order, one card per repeated message, fading, dropping), panel components (case-insensitive search, clicking a list row, a long list asking only for the rows on screen, a `std::string` field growing as text is typed, a hidden-label dropdown, a section open by default), and templates (clicking a row's button in an action list, log levels hiding their entries, an inspector taking input only while interactive, a panel opening where and at the size it is placed, growing when resized, and refusing to shrink below its floor).

Build into a fresh directory when an older `build/` tree was generated by a different CMake generator; reusing it fails with a generator mismatch instead of rebuilding.

All five optional renderer targets were compiled on Windows. DX11 demo rendering was exercised and visually inspected. Compiling the other targets does not constitute in-game validation of every API, swapchain, or driver.
