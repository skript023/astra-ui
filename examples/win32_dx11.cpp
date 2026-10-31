#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <astra/menu.hpp>
#include <astra/navigation.hpp>
#include <astra/key_settings.hpp>
#include <astra/banners.hpp>
#include <astra/hud.hpp>
#include <astra/components.hpp>
#include <algorithm>
#include <format>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
static UINT resize_width{}, resize_height{};
static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(window,message,wparam,lparam)) return 1;
    if (message == WM_SIZE && wparam != SIZE_MINIMIZED)
    { resize_width=LOWORD(lparam); resize_height=HIWORD(lparam); return 0; }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window,message,wparam,lparam);
}
static void checked(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("DX11 operation failed"); }
// Decodes one built-in banner (PNG or JPEG) with WIC, the way a game host would with its own decoder.
static ComPtr<ID3D11ShaderResourceView> upload_banner(IWICImagingFactory* wic, ID3D11Device* device, const astra::banner& banner)
{
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(banner.image.data(),static_cast<UINT>(banner.image.size())));
    if (!stream) throw std::runtime_error("Cannot read banner image");
    ComPtr<IWICBitmapDecoder> decoder; ComPtr<IWICBitmapFrameDecode> frame; ComPtr<IWICFormatConverter> rgba;
    checked(wic->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnDemand,&decoder));
    checked(decoder->GetFrame(0,&frame));
    checked(wic->CreateFormatConverter(&rgba));
    checked(rgba->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    UINT width{}, height{}; checked(rgba->GetSize(&width,&height));
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*4);
    checked(rgba->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data()));
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=width; desc.Height=height; desc.MipLevels=1; desc.ArraySize=1; desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1; desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    const D3D11_SUBRESOURCE_DATA data{pixels.data(),width*4,0};
    ComPtr<ID3D11Texture2D> texture; ComPtr<ID3D11ShaderResourceView> view;
    checked(device->CreateTexture2D(&desc,&data,&texture));
    checked(device->CreateShaderResourceView(texture.Get(),nullptr,&view));
    return view;
}
static void save_bitmap(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain, const char* path)
{
    ComPtr<ID3D11Texture2D> back, staging;
    checked(swapchain->GetBuffer(0,IID_PPV_ARGS(&back)));
    D3D11_TEXTURE2D_DESC desc{}; back->GetDesc(&desc);
    desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ; desc.MiscFlags=0;
    checked(device->CreateTexture2D(&desc,nullptr,&staging));
    context->CopyResource(staging.Get(),back.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    checked(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    BITMAPFILEHEADER file{}; file.bfType=0x4D42; file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);
    file.bfSize=file.bfOffBits+desc.Width*desc.Height*4;
    BITMAPINFOHEADER info{}; info.biSize=sizeof(info); info.biWidth=desc.Width; info.biHeight=-static_cast<LONG>(desc.Height);
    info.biPlanes=1; info.biBitCount=32; info.biCompression=BI_RGB;
    std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<char*>(&file),sizeof(file));
    output.write(reinterpret_cast<char*>(&info),sizeof(info));
    for (UINT y=0;y<desc.Height;++y)
        for (UINT x=0;x<desc.Width;++x)
        {
            auto* p=static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
            const char pixel[]{static_cast<char>(p[2]),static_cast<char>(p[1]),static_cast<char>(p[0]),0};
            output.write(pixel,4);
        }
    context->Unmap(staging.Get(),0);
    if (!output) throw std::runtime_error("Cannot save preview bitmap");
}
int main(int argc,char** argv)
{
    try
    {
        const bool list_snapshot=argc > 2 && std::string(argv[1]) == "--snapshot-list";
        const bool controls_snapshot=argc > 2 && std::string(argv[1]) == "--snapshot-controls";
        const bool custom_snapshot=argc > 2 && std::string(argv[1]) == "--snapshot-custom";
        const bool hud_snapshot=argc > 2 && std::string(argv[1]) == "--snapshot-hud";
        const bool components_snapshot=argc > 2 && std::string(argv[1]) == "--snapshot-components";
        const bool templates_snapshot=argc > 2 && std::string(argv[1]) == "--snapshot-templates";
        const bool snapshot=list_snapshot || controls_snapshot || custom_snapshot || hud_snapshot || components_snapshot || templates_snapshot
            || (argc > 2 && std::string(argv[1]) == "--snapshot");
        WNDCLASSW wc{}; wc.lpfnWndProc=window_proc; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"AstraDemo";
        RegisterClassW(&wc);
        HWND window=CreateWindowW(wc.lpszClassName,L"Astra - standalone demo",WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,CW_USEDEFAULT,1100,components_snapshot ? 1080 : 760,nullptr,nullptr,wc.hInstance,nullptr);
        if (!window) throw std::runtime_error("Cannot create demo window");
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width=1100; desc.BufferDesc.Height=components_snapshot ? 1040 : 720; desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count=1; desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount=2;
        desc.OutputWindow=window; desc.Windowed=TRUE; desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context; ComPtr<IDXGISwapChain> swapchain;
        checked(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
            &desc,&swapchain,&device,nullptr,&context));
        ComPtr<ID3D11RenderTargetView> target;
        auto create_target=[&] {
            ComPtr<ID3D11Texture2D> back; checked(swapchain->GetBuffer(0,IID_PPV_ARGS(&back)));
            checked(device->CreateRenderTargetView(back.Get(),nullptr,&target));
        };
        create_target();
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename=nullptr;
        ImFontConfig font; font.SizePixels=18;
        char system_directory[MAX_PATH]{};
        GetWindowsDirectoryA(system_directory,MAX_PATH);
        const std::string font_path = std::string(system_directory) + "/Fonts/segoeui.ttf";
        if (!ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path.c_str(),18.f))
            ImGui::GetIO().Fonts->AddFontDefault(&font);
        if (!ImGui_ImplWin32_Init(window) || !ImGui_ImplDX11_Init(device.Get(),context.Get()))
            throw std::runtime_error("Cannot initialize ImGui");
        checked(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
        ComPtr<IWICImagingFactory> wic;
        checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic)));
        std::vector<ComPtr<ID3D11ShaderResourceView>> banners;
        std::vector<std::string> banner_names;
        for (const auto& banner : astra::banners())
        {
            banners.push_back(upload_banner(wic.Get(),device.Get(),banner));
            banner_names.emplace_back(banner.name);
        }
        if (!snapshot) ShowWindow(window,SW_SHOWDEFAULT);
        astra::menu menu;
        astra::navigation<int> nav; nav.add_tab(0); nav.add_tab(1); nav.add_tab(2);
        bool running=true, open=!components_snapshot && !templates_snapshot, enabled=true, notifications=true, overlay=hud_snapshot,
            gallery=components_snapshot, templates=templates_snapshot;
        astra::key_settings key_settings;
        auto keys=astra::default_key_bindings;
        if (controls_snapshot) { nav.select_tab(2); key_settings.open(keys); }
        std::array<bool,9> was_down{};
        std::array<double,9> repeat_at{};
        double strength=65;
        int difficulty=1, theme=4, layout=list_snapshot || hud_snapshot || components_snapshot || templates_snapshot ? 0 : 1, actions=0, banner=0;
        std::size_t selected_option=0;
        astra::theme custom_theme=astra::preset_theme(0);
        // Snapshot the Custom path with an existing palette so the output can be compared
        // against --snapshot: identical files prove the host palette is what reaches the screen.
        if (custom_snapshot) { theme=3; custom_theme=astra::preset_theme(4); }
        if ((hud_snapshot || components_snapshot || templates_snapshot) && argc > 3) theme=std::stoi(argv[3]);
        // Sample data for the component gallery, shaped like a game's item spawner.
        const std::vector<std::string> gallery_items={"Bronze sword","Iron sword","Silver sword","Black metal sword","Wood","Fine wood",
            "Core wood","Stone","Flint","Leather scraps","Deer hide","Troll hide","Copper ore","Tin ore","Iron scraps","Silver ore"};
        const std::string gallery_categories[]={"All items","Weapons","Materials"};
        char gallery_search[64]{};
        std::string gallery_name, gallery_script="log(\"Hello from the script editor\");";
        int gallery_item=1, gallery_count=5, gallery_stack=20, gallery_category=0;
        float gallery_distance=12.f, gallery_position[3]{12.5f,40.f,-3.25f};
        bool gallery_tamed=true;
        ImVec4 gallery_color{0.85f,0.35f,0.30f,1.f};
        // Sample data for the templates, taken from the panels the game hosts already show.
        struct demo_bookmark { std::string name, where; };
        std::vector<demo_bookmark> bookmarks={{"Home base","X -104.2  Y 31.5  Z 2210.9"},{"Copper deposit","X 812.0  Y 44.1  Z -97.6"},
            {"Trader","X 2210.4  Y 30.0  Z 1890.2"},{"Swamp crypt","X -1532.8  Y 28.7  Z 640.3"}};
        const char* bookmark_actions[]={"Teleport","Delete"};
        std::vector<astra::log_entry> console_log={{"14:02:11",astra::log_level::info,"Script loaded: teleport.js"},
            {"14:02:12",astra::log_level::input,"player.position()"},{"14:02:12",astra::log_level::output,"{ x: -104.2, y: 31.5, z: 2210.9 }"},
            {"14:02:15",astra::log_level::warning,"Native timer fired 34 ms late"},{"14:02:20",astra::log_level::error,"ReferenceError: horse is not defined"}};
        astra::log_filter console_filter;
        const astra::inspector inspected{.name="Wolf",.tag="2 stars",.bar=.82f,
            .rows={{"Health","164 / 200 (82%)"},{"Distance","14.2 m"},{"Status","Taming 45% (~3m 10s)"},{"Mood","Calm"}},
            .hint="Press [T] to tame instantly",.actions={"Instant Tame","Heal Full"}};
        const astra::info_window player_info{.heading="Lyzism",
            .rows={{"RID","180047233"},{"Host","Yes"},{"Modder","No"},{"Country",""},{"Rank","42"},{"Latency","38 ms"}},
            .note="Location is looked up from the player's IP address and fills in once the lookup returns."};
        // The queue the game hosts push to through big::notification: shown 3 s, then a 0.6 s fade.
        astra::notification_queue toasts;
        auto push_toast=[&](std::string title,std::string message,astra::notice kind) {
            toasts.push(kind,std::move(title),std::move(message));
        };
        if (hud_snapshot)
        {
            push_toast("Run action","Ran 1 time.",astra::notice::success);
            push_toast("Menu controls","Press "+astra::menu_key_name(keys[0])+" to open or close the menu, and change the keys under Settings.",astra::notice::info);
            push_toast("Custom theme","Editing a colour in Demo controls switches the menu to Custom.",astra::notice::warning);
        }
        int frame=0;
        while (running)
        {
            MSG message;
            while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE))
            { TranslateMessage(&message); DispatchMessageW(&message); if (message.message == WM_QUIT) running=false; }
            if (!running) break;
            if (!snapshot && resize_width && resize_height)
            {
                context->OMSetRenderTargets(0,nullptr,nullptr); target.Reset();
                checked(swapchain->ResizeBuffers(0,resize_width,resize_height,DXGI_FORMAT_UNKNOWN,0));
                resize_width=resize_height=0; create_target();
            }
            ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame(); menu.keep_windows_out();
            // Custom keeps a host-editable palette; the library only reads what the host passes.
            const astra::theme colors=theme==3 ? custom_theme : astra::preset_theme(theme);
            // What a game host gives its own panels, the overlay, and notifications: the same palette, square like the List.
            const astra::theme panel_colors=astra::panel_theme(colors);
            std::array<bool,9> pressed{}, repeated{};
            for (std::size_t i=0; i<keys.size(); ++i)
            {
                const bool down=!snapshot && GetForegroundWindow()==window && (GetAsyncKeyState(keys[i]) & 0x8000);
                pressed[i]=down && !was_down[i];
                repeated[i]=pressed[i] || (down && ImGui::GetTime()>=repeat_at[i]);
                if (repeated[i]) repeat_at[i]=ImGui::GetTime()+(pressed[i] ? .35 : .12);
                was_down[i]=down;
            }
            const bool allow_keys=!key_settings.active() && !ImGui::GetIO().WantTextInput;
            if (allow_keys && pressed[0]) open=!open;
            astra::page page;
            page.tabs={"Overview","Tools","Settings"}; page.selected_tab=nav.selected_tab();
            page.id=std::to_string(nav.path().back());
            page.selected_option=selected_option;
            page.title=nav.path().back()==3 ? "Advanced options" : page.tabs[nav.selected_tab()];
            page.breadcrumbs={page.tabs[nav.selected_tab()]};
            if (nav.path().size()>1) page.breadcrumbs.push_back("Advanced");
            auto add=[&](const char* id,const char* label,astra::control_kind kind) -> astra::control& {
                page.controls.emplace_back(); auto& c=page.controls.back(); c.id=id; c.label=label; c.kind=kind; return c;
            };
            if (nav.selected_tab()==2)
            {
                auto& mode=add("layout","Menu layout",astra::control_kind::choice);
                mode.choices={"List","Window"}; mode.choice=layout; mode.set_choice=[&](int n){layout=n;};
                auto& palette=add("theme","Color theme",astra::control_kind::choice);
                palette.choices.assign(astra::preset_theme_names.begin(),astra::preset_theme_names.end()); palette.choice=theme; palette.set_choice=[&](int n){theme=n;};
                auto& header=add("banner","Banner",astra::control_kind::choice);
                header.choices=banner_names; header.choice=banner; header.set_choice=[&](int n){banner=n;};
                header.description="Change the List header banner.";
                auto& controls=add("menu_controls","Menu controls",astra::control_kind::action);
                controls.description="Change the menu hotkey and list navigation buttons.";
                controls.activate=[&] { key_settings.open(keys); };
                auto& info=add("overlay","Information overlay",astra::control_kind::toggle);
                info.checked=overlay; info.description="Show the display size, frame rate, and theme in the bottom-left corner.";
                info.activate=[&]{overlay=!overlay;};
                auto& components=add("gallery","Component gallery",astra::control_kind::toggle);
                components.checked=gallery; components.description="Show every panel component from astra/components.hpp in one window.";
                components.activate=[&]{gallery=!gallery;};
                auto& panels=add("templates","Template gallery",astra::control_kind::toggle);
                panels.checked=templates; panels.description="Show the panel, inspector, info window, action list, and console templates.";
                panels.activate=[&]{templates=!templates;};
            }
            else
            {
                auto& toggle=add("enabled","Enable feature",astra::control_kind::toggle);
                toggle.checked=enabled; toggle.description="A shared setting across both menu layouts.";
                toggle.activate=[&]{enabled=!enabled;};
                auto& slider=add("strength","Strength",astra::control_kind::number);
                slider.value=strength; slider.minimum=0; slider.maximum=100; slider.integral=true;
                slider.description=layout==0 ? "Click the arrows to decrease or increase the value." : "Drag the slider or Ctrl+click to enter an exact value.";
                slider.set_value=[&](double v){strength=v;};
                auto& choice=add("difficulty","Profile",astra::control_kind::choice);
                choice.choices={"Relaxed","Balanced","Advanced"}; choice.choice=difficulty; choice.set_choice=[&](int n){difficulty=n;};
                auto& notify=add("notify","Show notifications",astra::control_kind::toggle);
                notify.checked=notifications; notify.activate=[&]{notifications=!notifications;};
                notify.description="Run action reports each run in the top-right corner.";
                auto& sub=add("advanced","Advanced options",astra::control_kind::submenu);
                sub.activate=[&]{nav.push(3);};
                auto& action=add("run","Run action",astra::control_kind::action);
                action.description="Actions run once per click. Count: "+std::to_string(actions);
                // Not yet until the action first runs, then Done.
                action.status=actions ? astra::status_kind::done : astra::status_kind::pending;
                action.activate=[&]{ ++actions; push_toast("Run action",std::format("Ran {} time{}.",actions,actions==1 ? "" : "s"),astra::notice::success); };
            }
            if (open && layout==0 && allow_keys)
            {
                if (pressed[1]) { nav.back(); selected_option=0; }
                else if (repeated[7] || repeated[8])
                {
                    const auto count=page.tabs.size();
                    nav.select_tab((nav.selected_tab()+ (repeated[7] ? count-1 : 1)) % count);
                    selected_option=0;
                }
                else if (!page.controls.empty())
                {
                    selected_option=std::min(selected_option,page.controls.size()-1);
                    if (repeated[3]) selected_option=(selected_option+page.controls.size()-1)%page.controls.size();
                    else if (repeated[4]) selected_option=(selected_option+1)%page.controls.size();
                    else
                    {
                        const auto& c=page.controls[selected_option];
                        if (pressed[2] && c.activate) c.activate();
                        else if (repeated[5] || repeated[6])
                        {
                            const int direction=repeated[5] ? -1 : 1;
                            if (c.kind==astra::control_kind::choice && c.set_choice && !c.choices.empty())
                                c.set_choice((c.choice+direction+static_cast<int>(c.choices.size()))%static_cast<int>(c.choices.size()));
                            else if (c.set_value)
                                c.set_value(astra::bounded_value(c.value+direction*c.step,c.minimum,c.maximum,c.integral));
                        }
                    }
                    page.selected_option=selected_option;
                }
            }
            if (open && layout==1)
            {
                if (nav.selected_tab()!=2)
                {
                    const auto original = page.controls;
                    astra::control feature;
                    feature.id="feature"; feature.label="Feature";
                    feature.kind=astra::control_kind::submenu;
                    feature.children=[original] {
                        astra::control advanced;
                        advanced.id="advanced"; advanced.label="Advanced options";
                        advanced.kind=astra::control_kind::submenu;
                        advanced.children=[original] { return std::vector<astra::control>{original[3]}; };
                        return std::vector<astra::control>{original[0], original[1], original[2], advanced};
                    };
                    astra::control actions_section;
                    actions_section.id="actions"; actions_section.label="Actions";
                    actions_section.kind=astra::control_kind::submenu;
                    actions_section.children=[original] { return std::vector<astra::control>{original[5]}; };
                    page.controls={feature, actions_section};
                    page.id=std::to_string(nav.selected_tab());
                    page.breadcrumbs={page.tabs[nav.selected_tab()]};
                }
                ImGui::SetNextWindowPos({50,20},ImGuiCond_FirstUseEver);
                const auto event=menu.draw_window("Astra",page,open,colors);
                if (event.kind==astra::event_kind::tab) nav.select_tab(event.index);
                if (event.kind==astra::event_kind::back) nav.back();
                if (event.kind==astra::event_kind::breadcrumb) nav.to_depth(event.index);
            }
            else if (open)
            {
                astra::list_style list_style;
                list_style.mouse_enabled=!key_settings.active();
                if (!banners.empty()) list_style.banner=reinterpret_cast<ImTextureID>(banners[banner].Get());
                const auto event=menu.draw_list(page,colors,list_style);
                if (event.kind==astra::event_kind::tab) { nav.select_tab(event.index); selected_option=0; }
                if (event.kind==astra::event_kind::back) nav.back();
                if (event.kind==astra::event_kind::option) selected_option=event.index;
            }
            if (!snapshot)
            {
                ImGui::SetNextWindowPos({520,40},ImGuiCond_FirstUseEver);
                if (ImGui::Begin("Demo controls"))
                {
                    if (layout==0 && ImGui::Button("Return to Window")) layout=1;
                    if (!open)
                    {
                        ImGui::Text("Press %s to open the menu.",astra::menu_key_name(keys[0]).c_str());
                        if (ImGui::Button("Open menu")) open=true;
                    }
                    ImGui::SeparatorText("Custom theme");
                    ImGui::TextDisabled("Used while Color theme is Custom.");
                    bool palette_edited=false;
                    const auto palette=[&](const char* label,ImVec4& value) {
                        palette_edited |= ImGui::ColorEdit4(label,&value.x);
                    };
                    palette("Accent",custom_theme.accent);
                    palette("Background",custom_theme.background);
                    palette("Panel",custom_theme.panel);
                    palette("Field",custom_theme.field);
                    palette("Text",custom_theme.text);
                    palette("Muted",custom_theme.muted);
                    palette("Selection",custom_theme.selection);
                    palette("Selection text",custom_theme.selection_text);
                    if (palette_edited)
                    {
                        // Same contract as the game adapter: switch to Custom and snap instead of animating.
                        theme=3;
                        menu.reset_theme();
                    }
                }
                ImGui::End();
            }
            const auto cards=toasts.cards();
            if (notifications) astra::draw_notifications(cards,panel_colors);
            if (overlay)
            {
                const auto& io=ImGui::GetIO();
                const astra::overlay_row rows[]={
                    {"Display",std::format("{:.0f} x {:.0f}",io.DisplaySize.x,io.DisplaySize.y)},
                    {"Frame rate",std::format("{:.0f} fps",io.Framerate)},
                    {"Theme",astra::preset_theme_names[std::min<std::size_t>(theme,astra::preset_theme_names.size()-1)]}};
                // Starts at the bottom-left, clear of both menu layouts and the notifications.
                astra::draw_overlay("Demo status",rows,panel_colors,{18.f,io.DisplaySize.y-18.f},{0.f,1.f});
            }
            if (templates)
            {
                {
                    // A game gets this style from canvas::apply_panel_theme; the demo applies it to its own panels here.
                    astra::scoped_theme scoped(panel_colors);
                    if (astra::begin_panel("World bookmarks",nullptr,{20,20},{.size={460,300}}))
                    {
                        const auto clicked=astra::action_list("bookmarks",static_cast<int>(bookmarks.size()),[&](int i) {
                            return astra::action_row{bookmarks[i].name,bookmarks[i].where};
                        },bookmark_actions,{.count_label="Saved locations",.empty_text="No saved locations."});
                        if (clicked.action==0) push_toast("World bookmarks","Teleported to "+bookmarks[clicked.row].name+".",astra::notice::success);
                        if (clicked.action==1) bookmarks.erase(bookmarks.begin()+clicked.row);
                    }
                    astra::end_panel();
                    if (astra::begin_panel("Script console",nullptr,{20,340},{.size={460,360}}))
                    {
                        if (astra::log_view("console",console_log,console_filter)) console_log.clear();
                    }
                    astra::end_panel();
                }
                if (const int action=astra::draw_inspector("Inspector",inspected,panel_colors,{520,20}); action>=0)
                    push_toast("Inspector",std::string(inspected.actions[action])+" on "+inspected.name+".",astra::notice::success);
                astra::draw_info_window("Player Info",player_info,panel_colors,{520,370},{360,330});
            }
            if (gallery)
            {
                astra::scoped_theme scoped(panel_colors);
                if (astra::begin_panel("Component gallery",&gallery,{600,20},{.size={460,components_snapshot ? 1000.f : 700.f},.min_size={360,300}}))
                {
                    astra::search_field("gallery_search",gallery_search,sizeof(gallery_search),"Search items");
                    std::vector<int> found;
                    for (int i=0; i<static_cast<int>(gallery_items.size()); ++i)
                        if (astra::matches_search(gallery_items[i],gallery_search)) found.push_back(i);
                    const int hit=astra::match_list("gallery_items",static_cast<int>(found.size()),[&](int row) {
                        return astra::list_item{gallery_items[found[row]],found[row]==gallery_item};
                    },{.height=120.f});
                    if (hit>=0) gallery_item=found[hit];
                    astra::text_accent("Selected: %s",gallery_items[gallery_item].c_str());
                    astra::slider("Count",&gallery_count,1,50);
                    astra::slider("Spawn distance",&gallery_distance,1.f,50.f,0," m");
                    astra::input_number("Stack",&gallery_stack);
                    astra::input_vector3("Position",gallery_position);
                    astra::choice("Category",&gallery_category,gallery_categories);
                    astra::checkbox("Spawn tamed",&gallery_tamed);
                    ImGui::SameLine();
                    astra::color_picker("Marker colour",&gallery_color);
                    astra::input_text("Pet name",&gallery_name,"Optional name");
                    if (astra::section("Script",true)) astra::text_editor("##gallery_script",&gallery_script,{0.f,60.f});
                    astra::progress_bar(.68f);
                    const astra::overlay_row facts[]={{"Health","68 / 100"},{"Level","2 stars"},{"Owner",""}};
                    astra::details("gallery_details",facts);
                    astra::button("Spawn in world");
                    ImGui::SameLine();
                    astra::small_button("Add favorite");
                    astra::tooltip("Keeps this item at the top of the list.");
                }
                astra::end_panel();
            }
            if (const auto updated=key_settings.draw(colors))
            {
                keys=*updated;
                for (std::size_t i=0; i<keys.size(); ++i)
                {
                    was_down[i]=(GetAsyncKeyState(keys[i]) & 0x8000)!=0;
                    repeat_at[i]=ImGui::GetTime()+.35;
                }
            }
            ImGui::Render();
            const float clear[]{0.025f,0.032f,0.045f,1};
            auto* rt=target.Get(); context->OMSetRenderTargets(1,&rt,nullptr); context->ClearRenderTargetView(rt,clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            if (snapshot && ++frame==4) { save_bitmap(device.Get(),context.Get(),swapchain.Get(),argv[2]); break; }
            swapchain->Present(snapshot ? 0 : 1,0);
        }
        ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
        banners.clear(); wic.Reset(); CoUninitialize();
        target.Reset(); swapchain.Reset(); context.Reset(); device.Reset();
        DestroyWindow(window); UnregisterClassW(wc.lpszClassName,wc.hInstance);
        return 0;
    }
    catch(const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
