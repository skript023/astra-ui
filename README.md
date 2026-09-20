# Astra

A standalone C++17 menu library built on Dear ImGui. Navigation, data bindings, themes, List layout, and clickable Window layout are separate modules. There are no Unity/Unreal types, game globals, process hooks, or project precompiled headers in this package.

The Studio theme uses a light workspace, purple accents, rounded cards, a custom header, icon sidebar, animated pill toggles, and thin sliders. Emerald, Violet, and Ocean are dark alternatives. The host can supply any `theme` palette.

## Modules

| Target / header | Responsibility |
| --- | --- |
| `astra::core` | Header-only page/control data, value validation, per-tab navigation |
| `astra::ui` | ImGui widgets, theme scope, animated colors, List and Window views |
| `astra::win32` | Optional upstream ImGui Win32 input backend |
| `astra::dx9` | Optional upstream ImGui DirectX 9 renderer |
| `astra::dx11` | Optional upstream ImGui DirectX 11 renderer |
| `astra::dx12` | Optional upstream ImGui native DirectX 12 renderer |
| `astra::vulkan` | Optional upstream ImGui Vulkan renderer, dynamically supplied functions |
| `astra::opengl` | Optional upstream ImGui OpenGL 3/4 renderer |

The graphics targets compile official ImGui backends. They do not discover a game's device or install presentation hooks. The host owns device/context creation, backend initialization, texture lifetime, frame boundaries, render submission, resize/reset, synchronization, and shutdown. A UI package supporting an API is distinct from a particular game having an adapter for that API.

OpenGL here means the modern OpenGL3 backend; legacy fixed-function OpenGL2 is not included.

## CMake integration

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

Call `canvas.draw(astra::layout::list, id, page, open, theme, list_style)` for the keyboard-oriented List view. Both layouts consume the same model through the shared `astra::canvas` facade. List input is routed by the host to its navigation and option bindings.

Window actions are deferred until the old window has finished rendering, then run once. Returned events tell the host which tab, breadcrumb, or selected option changed. The host applies navigation events to its `navigation` instance. Root Back is disabled. Revisiting an ancestor truncates the path rather than creating a cycle.

Control kinds: action, submenu, toggle, number, toggle + number, and choice. Number controls provide bounds, integer/floating-point mode, precision, and a direct setter; dragging and Ctrl+click typing share validation. `step` is host metadata for keyboard increments; Window sliders permit continuous changes within bounds, rounded for integral controls. Choice controls can also supply an explicit Apply callback for selectors that require confirmation.

Callbacks and captured objects must remain valid until the drawing call returns. Rebuild the model each frame to display current values. Stable IDs keep input focus and animations independent of object addresses.

The library never creates or destroys the host's ImGui context. Themes restore the host's complete ImGui style after drawing. View search and theme animation state are per `menu` instance.

## Build, tests, and demo

From a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -G Ninja -DASTRA_BUILD_TESTS=ON -DASTRA_BUILD_EXAMPLE=ON "-DASTRA_BACKENDS=dx9;dx11;dx12;vulkan;opengl"
cmake --build build -j 4
ctest --test-dir build --output-on-failure
./build/astra_demo.exe
```

The demo uses DX11 WARP and sample controls; it has no game dependency. It uses the Windows Segoe UI font, falling back to ImGui's font. Generate a preview without showing a window:

```powershell
./build/astra_demo.exe --snapshot preview.bmp
```

Regression tests exercise per-tab history, Back/root boundaries, ancestor breadcrumbs, invalid destinations, numeric bounds, actual mouse clicks on actions/toggles/sliders/dropdowns/sidebar/Back, Ctrl+click numeric text entry, style restoration, and empty List rendering.

All five optional renderer targets were compiled on Windows. DX11 demo rendering was exercised and visually inspected. Compiling the other targets does not constitute in-game validation of every API, swapchain, or driver.
