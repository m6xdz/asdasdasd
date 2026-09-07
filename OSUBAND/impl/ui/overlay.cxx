#include <impl/ui/overlay.hxx>
#include <impl/ui/animations.hxx>

#include <impl/memory/input.hxx>
#include <impl/util/playfield.hxx>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <commdlg.h>
#include <shellapi.h>
#include <impl/cloud/client.hxx>
#include <ShlObj.h>
#include <shobjidl.h>
#include <algorithm>
#include <random>
#include <cstring>

#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
#pragma comment( lib, "dwmapi.lib" )
#pragma comment( lib, "shell32.lib" )
#pragma comment( lib, "ole32.lib" )
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

namespace {

    inline uint64_t get_time_ms( ) {
        static LARGE_INTEGER freq;
        static bool init = false;
        if ( !init ) {
            QueryPerformanceFrequency( &freq );
            init = true;
        }
        LARGE_INTEGER time;
        QueryPerformanceCounter( &time );
        return static_cast<uint64_t>( static_cast<double>( time.QuadPart ) / static_cast<double>( freq.QuadPart ) * 1000.0 );
    }

    bool overlay_aim_hook_transform(
        void* ctx, POINT pen, const MSLLHOOKSTRUCT& raw, POINT* out ) {
        (void)pen;
        if ( !ctx || !out ) return false;
        auto* overlay = static_cast<ui::c_overlay*>( ctx );
        const auto adjusted = overlay->handle_mouse( pen, raw );
        if ( !adjusted ) return false;
        *out = *adjusted;
        return true;
    }

    bool overlay_keyboard_hook_callback( void* ctx, int vk, bool is_down, int64_t press_qpc ) {
        if ( !ctx ) return false;
        auto* overlay = static_cast<ui::c_overlay*>( ctx );
        return overlay->handle_keyboard( vk, is_down, press_qpc );
    }

    inline float& hover_anim( uint64_t key ) {
        static std::unordered_map<uint64_t, float> anims;
        return anims[ key ];
    }


}

namespace ui {

    bool c_overlay::create( HINSTANCE instance ) {
        ImGui_ImplWin32_EnableDpiAwareness( );

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof( wc );
        wc.style = CS_CLASSDC;
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = instance;
        wc.lpszClassName = L"OSUBAND.UI";
        RegisterClassExW( &wc );

        m_hwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"OSU!BAND", WS_POPUP,
            0, 0, MENU_W, MENU_H,
            nullptr, nullptr, instance, this );

        if ( !m_hwnd ) return false;

        // Use an explicit black color-key for the untouched DX11 backbuffer.
        // The previous full-window LWA_ALPHA=255 path could make transparent pixels
        // opaque black in fullscreen/compositor edge cases. UI panels are near-black,
        // never exact RGB(0,0,0), so the color key only removes the clear surface.
        SetLayeredWindowAttributes( m_hwnd, RGB(0,0,0), 0, LWA_COLORKEY );
        MARGINS glass{ -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea( m_hwnd, &glass );

        ShowWindow( m_hwnd, SW_HIDE );
        UpdateWindow( m_hwnd );

        if ( !init_d3d( ) ) return false;

        IMGUI_CHECKVERSION( );
        ImGui::CreateContext( );
        ImGuiIO& io = ImGui::GetIO( );
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        band_ui::load_fonts("C:\\Windows\\Fonts\\segoeui.ttf");

        ImGuiStyle& style = ImGui::GetStyle( );
        style.WindowRounding = 12.f;
        style.WindowBorderSize = 0.f;

        ImGui_ImplWin32_Init( m_hwnd );
        ImGui_ImplDX11_Init( m_device, m_context );

        m_mouse_hook.set_filter_injected_only( true );
        m_mouse_hook.set_transform( overlay_aim_hook_transform, this );
        m_mouse_hook.install( );

        m_keyboard_hook.set_callback( overlay_keyboard_hook_callback, this );
        m_keyboard_hook.install( );

        band_ui::load_preferences();
        refresh_profiles();
        m_aim.start();

        return true;
    }

    void c_overlay::destroy( ) {
        if(m_cloud_job.valid())m_cloud_job.wait();
        m_avatars.clear();
        m_aim.stop( );
        m_mouse_hook.uninstall( );
        m_keyboard_hook.uninstall( );

        ImGui_ImplDX11_Shutdown( );
        ImGui_ImplWin32_Shutdown( );
        ImGui::DestroyContext( );
        cleanup_d3d( );
        if ( m_hwnd ) {
            DestroyWindow( m_hwnd );
            m_hwnd = nullptr;
        }
    }

    bool c_overlay::pump( ) {
        MSG msg;
        while ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
            if ( msg.message == WM_QUIT )
                return false;
        }

        // Display affinity is a window policy, not a per-frame operation.
        // Re-applying it every tick caused needless kernel/user transitions.
        static bool display_affinity_initialized = false;
        static bool previous_stream_proof = false;
        static HWND affinity_window = nullptr;
        if ( affinity_window != m_hwnd || !display_affinity_initialized || previous_stream_proof != stream_proof ) {
            SetWindowDisplayAffinity( m_hwnd, stream_proof ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE );
            previous_stream_proof = stream_proof;
            display_affinity_initialized = true;
            affinity_window = m_hwnd;
        }

        cloud_tick();
        handle_hotkeys( );
        update_overlay_position( );
        apply_visibility( );
        render_frame( );
        return true;
    }

    bool c_overlay::osu_foreground() const {
        const HWND osu=input::target_window();
        if(!osu||!IsWindow(osu)||IsIconic(osu))return false;
        const HWND fg=GetForegroundWindow();
        if(!fg)return false;
        return GetAncestor(fg,GA_ROOT)==GetAncestor(osu,GA_ROOT);
    }

    void c_overlay::handle_hotkeys( ) {
        // The menu is a desktop overlay again: it may be opened even when osu! is
        // minimized/not focused. Gameplay hooks still check the game state separately.
        const bool pause=(GetAsyncKeyState(m_emergency_key)&0x8000)!=0;
        if(pause&&!m_pause_key_down)m_modules_paused=!m_modules_paused;
        m_pause_key_down=pause;
        if(m_waiting_menu){m_f4_was_down=false;return;}
        const bool menu_down = ( GetAsyncKeyState( m_menu_keybind ) & 0x8000 ) != 0;
        if ( menu_down && !m_f4_was_down ) m_visible = !m_visible;
        m_f4_was_down = menu_down;
    }

    void c_overlay::update_overlay_position( ) {
        if ( !m_hwnd ) return;
        static uint64_t next_update_ms = 0;
        const uint64_t now_ms = GetTickCount64();
        if(now_ms < next_update_ms) return;
        next_update_ms = now_ms + 100;

        int x=0,y=0,w=0,h=0;
        const HWND osu=input::target_window();
        RECT client{};
        if(osu && IsWindow(osu) && !IsIconic(osu) && playfield::get_playfield_rect(osu,client)) {
            x=client.left; y=client.top; w=client.right-client.left; h=client.bottom-client.top;
        } else if(m_visible) {
            // When osu! is minimized/closed, keep the settings menu on the monitor
            // the user is currently working on instead of forcibly closing it.
            HWND anchor=GetForegroundWindow();
            HMONITOR mon=MonitorFromWindow(anchor?anchor:m_hwnd,MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{};mi.cbSize=sizeof(mi);
            if(GetMonitorInfoW(mon,&mi)){x=mi.rcWork.left;y=mi.rcWork.top;w=mi.rcWork.right-mi.rcWork.left;h=mi.rcWork.bottom-mi.rcWork.top;}
        }
        if(w<=0||h<=0)return;
        static RECT previous{};
        RECT current{x,y,x+w,y+h};
        if(!EqualRect(&previous,&current)){SetWindowPos(m_hwnd,HWND_TOPMOST,x,y,w,h,SWP_NOACTIVATE|SWP_SHOWWINDOW);previous=current;}
    }

    void c_overlay::apply_visibility( ) {
        if(!m_hwnd)return;
        const HWND osu=input::target_window();
        const bool game_window=osu&&IsWindow(osu)&&!IsIconic(osu);
        const bool show=m_visible||(game_window&&(m_lab_enabled&&m_hud_enabled&&m_lab_access))||!m_toasts.empty();
        if(!show){ShowWindow(m_hwnd,SW_HIDE);return;}
        LONG ex=GetWindowLongW(m_hwnd,GWL_EXSTYLE);
        ex|=WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_NOACTIVATE;
        ex&=~WS_EX_APPWINDOW;
        if(m_visible)ex&=~WS_EX_TRANSPARENT;else ex|=WS_EX_TRANSPARENT;
        SetWindowLongW(m_hwnd,GWL_EXSTYLE,ex);
        ShowWindow(m_hwnd,SW_SHOWNOACTIVATE);
    }

    LRESULT CALLBACK c_overlay::wnd_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp ) {
        if ( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wp, lp ) )
            return true;

        if ( msg == WM_NCCREATE ) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>( lp );
            SetWindowLongPtrW( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( cs->lpCreateParams ) );
        }

        auto* self = reinterpret_cast<c_overlay*>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );
        if ( msg == WM_DESTROY ) {
            PostQuitMessage( 0 );
            return 0;
        }
        if ( msg == WM_MOUSEACTIVATE ) return MA_NOACTIVATE;
        if ( msg == WM_ACTIVATE ) return 0;
        if ( msg == WM_SETCURSOR ) {
            if ( self && self->stream_proof && self->m_streamproof_hide_cursor ) {
                SetCursor( nullptr );
                return 1;
            }
            SetCursor( LoadCursorW( nullptr, IDC_ARROW ) );
            return 1;
        }
        if ( msg == WM_SIZE && self && self->m_swap_chain ) {
            UINT w = LOWORD( lp ), h = HIWORD( lp );
            if ( w == 0 || h == 0 ) return 0;
            if ( self->m_rtv ) {
                self->m_rtv->Release( );
                self->m_rtv = nullptr;
            }
            self->m_swap_chain->ResizeBuffers( 0, w, h, DXGI_FORMAT_UNKNOWN, 0 );
            ID3D11Texture2D* back = nullptr;
            if ( SUCCEEDED( self->m_swap_chain->GetBuffer( 0, IID_PPV_ARGS( &back ) ) ) && back ) {
                self->m_device->CreateRenderTargetView( back, nullptr, &self->m_rtv );
                back->Release( );
            }
        }
        return DefWindowProcW( hwnd, msg, wp, lp );
    }

    bool c_overlay::init_d3d( ) {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = m_hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL level{};
        if ( D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                D3D11_SDK_VERSION, &sd, &m_swap_chain, &m_device, &level, &m_context ) != S_OK )
            return false;

        ID3D11Texture2D* back = nullptr;
        m_swap_chain->GetBuffer( 0, IID_PPV_ARGS( &back ) );
        if ( !back ) return false;
        m_device->CreateRenderTargetView( back, nullptr, &m_rtv );
        back->Release( );
        return m_rtv != nullptr;
    }

    void c_overlay::cleanup_d3d( ) {
        if ( m_rtv ) { m_rtv->Release( ); m_rtv = nullptr; }
        if ( m_swap_chain ) { m_swap_chain->Release( ); m_swap_chain = nullptr; }
        if ( m_context ) { m_context->Release( ); m_context = nullptr; }
        if ( m_device ) { m_device->Release( ); m_device = nullptr; }
    }

    void c_overlay::notify(std::string title,std::string detail,bool positive) {
        if(m_toasts.size()>=5)m_toasts.erase(m_toasts.begin());
        m_toasts.push_back({std::move(title),std::move(detail),GetTickCount64(),positive});
    }

    void c_overlay::draw_toasts() {
        if(m_toasts.empty())return;
        auto* d=ImGui::GetForegroundDrawList();const auto disp=ImGui::GetIO().DisplaySize;const uint64_t now=GetTickCount64();
        float y=20.f;
        for(size_t i=0;i<m_toasts.size();){auto& t=m_toasts[i];const float age=float(now-t.born)/1000.f;if(age>4.4f){m_toasts.erase(m_toasts.begin()+i);continue;}
            float a=1.f;if(age<.28f)a=age/.28f;else if(age>3.8f)a=std::max(0.f,(4.4f-age)/.6f);
            const float slide=band_ui::prefs.animations?(1.f-a)*32.f:0.f;const float w=360.f,h=t.detail.empty()?54.f:72.f;const ImVec2 p(disp.x-w-22.f+slide,y);
            const ImU32 bg=IM_COL32(20,20,27,int(238*a));const ImU32 bd=t.positive?IM_COL32(255,111,142,int(160*a)):IM_COL32(255,160,120,int(175*a));
            d->AddRectFilled(p,ImVec2(p.x+w,p.y+h),bg,12.f);d->AddRect(p,ImVec2(p.x+w,p.y+h),bd,12.f);d->AddRectFilled(p,ImVec2(p.x+3.f,p.y+h),bd,2.f);
            d->AddText(ImVec2(p.x+18,p.y+13),IM_COL32(245,242,247,int(255*a)),t.title.c_str());
            if(!t.detail.empty())d->AddText(ImVec2(p.x+18,p.y+38),IM_COL32(155,151,165,int(255*a)),t.detail.c_str());
            y+=h+10.f;++i;}
    }

    void c_overlay::render_frame( ) {
        const HWND osu_hwnd=input::target_window();
        const bool game_window=osu_hwnd&&IsWindow(osu_hwnd)&&!IsIconic(osu_hwnd);
        bool should_show = m_visible || (game_window && m_lab_enabled && m_hud_enabled&&m_lab_access) || !m_toasts.empty();
        if ( !should_show ) {
            // Do not copy the full beatmap snapshot or submit an empty frame
            // while the overlay is hidden.  The window is already hidden by
            // apply_visibility(), so a short wait is all that is needed.
            ::Sleep( 8 );
            return;
        }

        osu::full_snapshot_t snap;
        if ( m_snapshot_fn )
            snap = m_snapshot_fn( );

        ImGui_ImplDX11_NewFrame( );
        ImGui_ImplWin32_NewFrame( );
        ImGui::NewFrame( );

        const float dt = ImGui::GetIO( ).DeltaTime;
        tick_animations( dt );
        m_anim_time = g_time;

        if ( m_visible && !m_menu_was_open ) {
            m_menu_open_anim = 0.f;
            m_menu_was_open = true;
        }
        else if ( !m_visible && m_menu_was_open ) {
            m_menu_was_open = false;
        }

        if ( m_visible ) {
            if(band_ui::prefs.animations)m_menu_open_anim += (1.f-m_menu_open_anim)*std::min(dt*6.f*band_ui::prefs.motion,1.f);
            else m_menu_open_anim=1.f;
        }

        // Deliberately do not tint the full game/desktop surface. The menu itself
        // carries its own background; this avoids black/blur remnants after closing.

        if ( m_visible ) {
            draw_menu( snap );
            m_streamproof_hide_cursor = stream_proof && ( ImGui::GetIO( ).WantCaptureMouse );
        }
        else {
            m_streamproof_hide_cursor = false;
        }

        if(m_lab_enabled&&m_hud_enabled&&m_lab_access){
            auto* hud=ImGui::GetForegroundDrawList();
            SYSTEMTIME st{};GetLocalTime(&st);char clock[16]{};std::snprintf(clock,sizeof(clock),"%02u:%02u:%02u",st.wHour,st.wMinute,st.wSecond);
            const auto line=std::string("OSU!BAND Beta [osuband.dev] / ")+clock+" / "+m_studio.user;
            const float old_scale=band_ui::ui_scale;band_ui::ui_scale=1.f;
            const float w=std::clamp(28.f+static_cast<float>(line.size())*7.4f,310.f,760.f);
            band_ui::card(hud,ImVec2(18,18),ImVec2(w,36),band_ui::panel,10);
            band_ui::text(hud,ImVec2(31,29),line.c_str(),13,band_ui::rose);
            band_ui::ui_scale=old_scale;
        }
        draw_toasts();
        ImGui::Render( );

        if ( m_context && m_rtv ) {
            const float clear_color[ 4 ]{ 0.0f, 0.0f, 0.0f, 0.0f };
            m_context->OMSetRenderTargets( 1, &m_rtv, nullptr );
            m_context->ClearRenderTargetView( m_rtv, clear_color );
        }

        ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );
        // Let the compositor pace the visible menu instead of rendering as
        // fast as the CPU allows.
        if ( m_swap_chain ) m_swap_chain->Present( 1, 0 );
    }

    void c_overlay::apply_custom_keys( osu::game_snapshot_t& game ) const {
        game.left_key = m_custom_left_key;
        game.right_key = m_custom_right_key;
    }

    config::settings_t c_overlay::capture_settings( ) const {
        std::lock_guard<std::recursive_mutex> guard(m_settings_mutex);
        config::settings_t s{};
        s.aim_enabled = m_aim.enabled;
        s.aim_ignore_sliders = m_aim.ignore_sliders;
        s.aim_tablet_mode = m_aim.tablet_mode;

        s.aim_strength_x = m_aim.strength_x;
        s.aim_strength_y = m_aim.strength_y;
        s.aim_decay_far = m_aim.decay_far;
        s.aim_lerp = m_aim.aim_lerp;
        s.aim_freeze_lerp = m_aim.freeze_lerp;
        s.aim_window = m_aim.aim_window;
        s.aim_legit_mode = m_aim.legit_mode;
        s.aim_legit_clamp = m_aim.legit_clamp;

        s.relax_enabled = m_relax.enabled;
        s.relax_ur = m_relax.ur;
        s.relax_tap_style = m_relax.tap_style;
        s.relax_singletap_bpm_cap = m_relax.singletap_bpm_cap;
        s.relax_k1_hold_center = m_relax.k1_hold_center;
        s.relax_k1_hold_spread = m_relax.k1_hold_spread;
        s.relax_k2_hold_center = m_relax.k2_hold_center;
        s.relax_k2_hold_spread = m_relax.k2_hold_spread;
        s.relax_hold_floor = m_relax.hold_floor;
        s.relax_hold_ceiling = m_relax.hold_ceiling;
        s.relax_manual_offset_ms = m_relax.manual_offset_ms;

        s.replay_enabled = m_replay.enabled;
        s.replay_path_utf8 = m_replay_path_utf8;
        s.replay_parse_buttons = m_replay.parse_buttons;
        s.replay_move_cursor=m_replay.move_cursor;
        s.lab_enabled=m_lab_enabled; s.hud_enabled=m_hud_enabled; s.emergency_key=m_emergency_key;

        s.autobot_enabled = m_autobot.enabled;
        s.autobot_aim_spread = m_autobot.aim_spread;
        s.autobot_curve_strength = m_autobot.curve_strength;
        s.autobot_drift_amount = m_autobot.drift_amount;
        s.autobot_momentum = m_autobot.momentum;
        s.autobot_slider_laziness = m_autobot.slider_laziness;
        s.autobot_spinner_rpm = m_autobot.spinner_rpm;

        s.tap_enabled = m_tap_assist.enabled;
        s.tap_assist_window = m_tap_assist.assist_window;
        s.tap_randomization = m_tap_assist.randomization;
        s.tap_ignore_sliders = m_tap_assist.ignore_sliders;

        s.custom_left_key = m_custom_left_key;
        s.custom_right_key = m_custom_right_key;
        s.menu_keybind = m_menu_keybind;
        s.stream_proof = stream_proof;

        return s;
    }

    void c_overlay::apply_settings( const config::settings_t& input_settings ) {
        std::lock_guard<std::recursive_mutex> guard(m_settings_mutex);
        auto s=input_settings;config::validate(s);
        if(!m_lab_access){s.lab_enabled=false;s.hud_enabled=false;}
        reset_modules(m_last_game);
        m_aim.enabled = s.aim_enabled;
        m_aim.ignore_sliders = s.aim_ignore_sliders;
        m_aim.tablet_mode = s.aim_tablet_mode;

        m_aim.strength_x = s.aim_strength_x;
        m_aim.strength_y = s.aim_strength_y;
        m_aim.decay_far = s.aim_decay_far;
        m_aim.aim_lerp = s.aim_lerp;
        m_aim.freeze_lerp = s.aim_freeze_lerp;
        m_aim.aim_window = s.aim_window;
        m_aim.legit_mode = s.aim_legit_mode;
        m_aim.legit_clamp = s.aim_legit_clamp;

        m_relax.enabled = s.relax_enabled;
        m_relax.ur = s.relax_ur;
        m_relax.tap_style = s.relax_tap_style;
        m_relax.singletap_bpm_cap = s.relax_singletap_bpm_cap;
        m_relax.k1_hold_center = s.relax_k1_hold_center;
        m_relax.k1_hold_spread = s.relax_k1_hold_spread;
        m_relax.k2_hold_center = s.relax_k2_hold_center;
        m_relax.k2_hold_spread = s.relax_k2_hold_spread;
        m_relax.hold_floor = s.relax_hold_floor;
        m_relax.hold_ceiling = s.relax_hold_ceiling;
        m_relax.manual_offset_ms = s.relax_manual_offset_ms;

        m_replay.enabled = s.replay_enabled;
        if (s.replay_path_utf8 != m_replay_path_utf8) {
            strncpy_s(m_replay_path_utf8,s.replay_path_utf8.c_str(),_TRUNCATE);
            const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.replay_path_utf8.c_str(),-1,nullptr,0);
            if(n>0){std::wstring wide(static_cast<size_t>(n),L'\0');
                MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.replay_path_utf8.c_str(),-1,wide.data(),n);
                wide.resize(static_cast<size_t>(n-1));m_replay.replay_path=wide;
            }else m_replay.replay_path.clear();
            if(!m_replay.load_replay())m_replay.enabled=false;
        }
        m_replay.move_cursor=s.replay_move_cursor;
        m_lab_enabled=s.lab_enabled;m_hud_enabled=s.hud_enabled;m_emergency_key=s.emergency_key;
        m_replay.parse_buttons = s.replay_parse_buttons;

        m_autobot.enabled = s.autobot_enabled;
        m_autobot.aim_spread = s.autobot_aim_spread;
        m_autobot.curve_strength = s.autobot_curve_strength;
        m_autobot.drift_amount = s.autobot_drift_amount;
        m_autobot.momentum = s.autobot_momentum;
        m_autobot.slider_laziness = s.autobot_slider_laziness;
        m_autobot.spinner_rpm = s.autobot_spinner_rpm;

        m_tap_assist.enabled = s.tap_enabled;
        m_tap_assist.assist_window = s.tap_assist_window;
        m_tap_assist.randomization = s.tap_randomization;
        m_tap_assist.ignore_sliders = s.tap_ignore_sliders;

        m_custom_left_key = s.custom_left_key;
        m_custom_right_key = s.custom_right_key;
        m_menu_keybind = s.menu_keybind;
        stream_proof = s.stream_proof;

    }

    void c_overlay::reset_modules( const osu::game_snapshot_t& game ) {
        m_game_time_stall_start_ms = 0;
        m_aim.set_user_input_blocked( false );
        m_aim.on_leave_play( );
        osu::game_snapshot_t mod_game = game;
        apply_custom_keys( mod_game );
        m_relax.on_leave_play( mod_game );
        m_replay.on_leave_play( mod_game );
        m_autobot.on_leave_play( mod_game );
        m_tap_assist.on_leave_play( mod_game );
    }

    void c_overlay::tick_modules( const osu::game_snapshot_t& game, const osu::beatmap_data_t& beatmap ) {
        std::lock_guard<std::recursive_mutex> guard(m_settings_mutex);
        m_last_game=game;
        if(GetTickCount64()>=m_auth_deadline.load())m_authorized=false;
        if(m_modules_paused||!m_authorized){reset_modules(game);m_aim.set_user_input_blocked(true);return;}
        const bool in_play = game.cur_state == osu::game_state_t::play;
        DWORD foreground_pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground_pid);
        if(in_play&&foreground_pid!=static_cast<DWORD>(game.pid)){
            reset_modules(game);m_aim.set_user_input_blocked(true);m_prev_game_time=-1;return;
        }
        const bool replay_active = game.is_replay;
        const bool was_play = m_prev_state == osu::game_state_t::play;
        const std::string map_sig =
            std::to_string( game.map_id ) + "|" + std::to_string( game.set_id ) + "|" +
            game.map_folder + "|" + game.map_file + "|" + game.beatmap_hash + "|" +
            game.beatmap_version;

        if ( replay_active ) {
            reset_modules(game);
            m_prev_game_time = -1;
            m_prev_map_id = -1;
            m_prev_map_sig.clear( );
            m_game_time_stall_start_ms = 0;
            m_aim.set_user_input_blocked( true );
            return;
        }

        if ( was_play && !in_play ) {
            reset_modules( game );
            m_prev_game_time = -1;
            m_prev_map_id = -1;
            m_prev_map_sig.clear( );
            m_game_time_stall_start_ms = 0;
        }

        if ( in_play && !map_sig.empty( ) && !m_prev_map_sig.empty( ) && map_sig != m_prev_map_sig )
            reset_modules( game );

        if ( in_play && m_prev_map_id > 0 && game.map_id != 0 && game.map_id != m_prev_map_id )
            reset_modules( game );

        if ( was_play && in_play && m_prev_game_time >= 0 && game.cur_time < m_prev_game_time - 200 )
            reset_modules( game );

        m_prev_state = game.cur_state;

        if ( in_play && beatmap.loaded && !beatmap.objects.empty( ) ) {
            osu::game_snapshot_t mod_game = game;
            apply_custom_keys( mod_game );

            constexpr uint64_t k_pause_stall_ms = 120;
            const uint64_t     now_ms = get_time_ms( );
            const int32_t      cur_time = mod_game.cur_time;
            const bool         time_stalled = m_prev_game_time >= 0 && cur_time == m_prev_game_time;

            if ( time_stalled ) {
                if ( m_game_time_stall_start_ms == 0 )
                    m_game_time_stall_start_ms = now_ms;
            }
            else {
                m_game_time_stall_start_ms = 0;
            }

            const bool map_paused = m_game_time_stall_start_ms != 0
                                    && ( now_ms - m_game_time_stall_start_ms ) >= k_pause_stall_ms;

            m_aim.set_user_input_blocked( map_paused );

            if(map_paused){
                m_relax.on_leave_play(mod_game);m_tap_assist.on_leave_play(mod_game);
            }else{
                m_aim.update(mod_game,beatmap);m_relax.update(mod_game,beatmap);m_tap_assist.update(mod_game,beatmap);
            }
            m_replay.update( mod_game, beatmap, map_paused );
            m_autobot.update( mod_game, beatmap, map_paused );

        }
        else {
            // A reader/map reload may become unavailable without leaving Play.
            // Do not leave keys pressed while no valid map is available.
            reset_modules(game);
        }

        if ( in_play ) {
            m_prev_map_id = game.map_id;
            m_prev_game_time = game.cur_time;
            if ( !map_sig.empty( ) )
                m_prev_map_sig = map_sig;
        }
    }

    #include "overlay_cloud.inl"

    void c_overlay::draw_menu(const osu::full_snapshot_t& snap) {
        auto s=capture_settings();
        m_studio.connected=snap.game.attached;m_studio.compatible=!snap.game.offset_mismatch;
        m_studio.map_loaded=snap.beatmap.loaded;m_studio.objects=static_cast<int>(snap.object_count);
        m_studio.time_ms=snap.game.cur_time;m_studio.paused=m_modules_paused;
        m_studio.lab_allowed=m_lab_access;
        m_studio.status=snap.game.attached?(snap.game.is_replay?"REPLAY VIEW / MODULES SUSPENDED":snap.beatmap.loaded?"LAZER / BEATMAP READY":"LAZER / CONNECTED"):"WAITING FOR OSU!LAZER";
        m_studio.map=snap.game.beatmap_version.empty()?"No beatmap selected":snap.game.beatmap_version;
        {std::lock_guard<std::recursive_mutex> guard(m_settings_mutex);
        m_studio.replay_frames=static_cast<int>(m_replay.frame_count());m_studio.replay_player=m_replay.player_name();}
        if(!m_authorized)m_studio.message="Session unavailable. Reconnect your loader to continue.";
        m_studio.waiting_menu=m_waiting_menu;
        const auto a=band_ui::draw(m_studio,s);
        if(a.pause)m_modules_paused=!m_modules_paused;
        if(a.close)m_visible=false;
        if(a.changed){apply_settings(s);m_studio.message="Settings applied. F8 pauses every module.";}
        if(a.bind_menu){m_waiting_menu=true;m_studio.waiting_menu=true;m_studio.message="Press a key for the menu. Esc cancels.";}
        if(m_waiting_menu&&!a.bind_menu){
            for(int vk=8;vk<=254;++vk){if((GetAsyncKeyState(vk)&1)==0)continue;
                if(vk==VK_ESCAPE){m_waiting_menu=false;m_studio.waiting_menu=false;m_studio.message="Menu key unchanged.";break;}
                if(vk==m_emergency_key){m_studio.message="F8 is reserved for pausing modules.";break;}
                s.menu_keybind=vk;apply_settings(s);m_waiting_menu=false;m_studio.waiting_menu=false;m_studio.message=std::string("Menu key: ")+band_ui::key_name(vk);break;
            }
        }
        if(a.refresh)refresh_profiles();
        if((a.save_private||a.submit_review)&&!m_studio.cloud_busy){
            auto portable=s;portable.replay_path_utf8.clear();
            std::ostringstream out;config::serialize_settings(out,m_studio.profile_name,portable);
            cloud_task(2,{{"name",m_studio.profile_name},{"description",m_studio.description},
                          {"cfg",out.str()},{"submit",a.submit_review}});
        }
        if(m_studio.selected>=0&&m_studio.selected<static_cast<int>(m_studio.profiles.size())){
            const auto& p=m_studio.profiles[m_studio.selected];
            if(a.install&&!m_studio.cloud_busy)cloud_task(4,{{"id",p.id},{"revision",p.revision}});
            if(a.uninstall&&!m_studio.cloud_busy)cloud_task(5,{{"id",p.id}});
            if(a.load&&!m_studio.cloud_busy)cloud_task(3,{{"id",p.id},{"revision",p.revision}});
        }
        if(a.uninject){notify("OSU!BAND","Closing cleanly…");PostMessageW(m_hwnd,WM_CLOSE,0,0);return;}
        if(a.browse_replay){wchar_t file[32768]{};OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=m_hwnd;
            ofn.lpstrFilter=L"osu! replay (*.osr)\0*.osr\0\0";ofn.lpstrFile=file;ofn.nMaxFile=32768;
            ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
            if(GetOpenFileNameW(&ofn))WideCharToMultiByte(CP_UTF8,0,file,-1,m_studio.replay_path,sizeof(m_studio.replay_path),nullptr,nullptr);}
        if(a.load_replay){s.replay_path_utf8=m_studio.replay_path;
            if(s.replay_path_utf8==m_replay_path_utf8){std::lock_guard<std::recursive_mutex> guard(m_settings_mutex);m_replay.load_replay();}
            else apply_settings(s);
            m_studio.message=m_replay.replay_valid()?"Replay loaded. Choose playback mode and enable the module.":m_replay.last_load_error();}
    }
}
