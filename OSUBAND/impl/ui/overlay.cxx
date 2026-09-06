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

    enum ACCENT_STATE {
        ACCENT_DISABLED = 0,
        ACCENT_ENABLE_GRADIENT = 1,
        ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
        ACCENT_ENABLE_BLURBEHIND = 3,
        ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
        ACCENT_ENABLE_HOSTBACKDROP = 5,
        ACCENT_INVALID_STATE = 6
    };

    struct ACCENT_POLICY {
        int State;
        int Flags;
        int GradientColor;
        int AnimationId;
    };

    struct WINCOMPATTRDATA {
        int Attribute;
        ACCENT_POLICY* Data;
        SIZE_T SizeOfData;
        int Reserved;
    };

    inline void enable_acrylic( HWND hwnd ) {
        HMODULE user32 = GetModuleHandleW( L"user32.dll" );
        if ( !user32 ) return;
        auto SetWindowCompositionAttribute = reinterpret_cast<BOOL (WINAPI*)( HWND, WINCOMPATTRDATA* )>(
            GetProcAddress( user32, "SetWindowCompositionAttribute" ) );
        if ( !SetWindowCompositionAttribute ) return;

        ACCENT_POLICY policy = { ACCENT_ENABLE_BLURBEHIND, 0, 0x00000000, 0 };
        WINCOMPATTRDATA data = { 19, &policy, sizeof( policy ), 0 };
        SetWindowCompositionAttribute( hwnd, &data );

        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea( hwnd, &margins );
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
            WS_EX_APPWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST,
            wc.lpszClassName, L"OSU!BAND", WS_POPUP,
            0, 0, MENU_W, MENU_H,
            nullptr, nullptr, instance, this );

        if ( !m_hwnd ) return false;

        enable_acrylic( m_hwnd );
        SetLayeredWindowAttributes( m_hwnd, 0, 255, LWA_ALPHA );

        ShowWindow( m_hwnd, SW_SHOWDEFAULT );
        UpdateWindow( m_hwnd );

        if ( !init_d3d( ) ) return false;

        IMGUI_CHECKVERSION( );
        ImGui::CreateContext( );
        ImGuiIO& io = ImGui::GetIO( );
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

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

    void c_overlay::handle_hotkeys( ) {
        const bool pause=(GetAsyncKeyState(m_emergency_key)&0x8000)!=0;
        if(pause&&!m_pause_key_down)m_modules_paused=!m_modules_paused;
        m_pause_key_down=pause;
        const bool menu_down = ( GetAsyncKeyState( m_menu_keybind ) & 0x8000 ) != 0;
        if ( menu_down && !m_f4_was_down )
            m_visible = !m_visible;
        m_f4_was_down = menu_down;
    }

    void c_overlay::update_overlay_position( ) {
        if ( !m_hwnd ) return;

        // Window geometry does not need to be recomputed every render tick.
        // Throttling this also avoids repeated virtual-desktop and Win32 calls.
        static uint64_t next_update_ms = 0;
        const auto now_ms = get_time_ms( );
        if ( now_ms < next_update_ms ) return;
        next_update_ms = now_ms + 100;

        const HWND osu_hwnd = input::target_window( );
        input::invalidate_virtual_desktop( );
        input::virtual_desktop( );

        int target_x = 0, target_y = 0, target_w = 0, target_h = 0;

        if ( !osu_hwnd || !IsWindow( osu_hwnd ) ) {
            target_w = GetSystemMetrics( SM_CXSCREEN );
            target_h = GetSystemMetrics( SM_CYSCREEN );
        }
        else {
            RECT client{};
            if ( playfield::get_playfield_rect( osu_hwnd, client ) ) {
                target_x = client.left;
                target_y = client.top;
                target_w = client.right - client.left;
                target_h = client.bottom - client.top;
            }
            else {
                target_w = GetSystemMetrics( SM_CXSCREEN );
                target_h = GetSystemMetrics( SM_CYSCREEN );
            }
        }

        static RECT previous{};
        const RECT current{ target_x, target_y, target_x + target_w, target_y + target_h };
        if ( !EqualRect( &previous, &current ) ) {
            SetWindowPos( m_hwnd, HWND_TOPMOST, target_x, target_y, target_w, target_h, SWP_NOACTIVATE );
            previous = current;
        }
    }

    void c_overlay::apply_visibility( ) {
        if ( !m_hwnd ) return;

        static bool prev_stream_proof = false;
        const bool stream_proof_changed = prev_stream_proof != stream_proof;
        prev_stream_proof = stream_proof;

        bool should_show = m_visible || (m_lab_enabled && m_hud_enabled);

        if ( should_show ) {
            LONG ex = GetWindowLongW( m_hwnd, GWL_EXSTYLE );
            if ( stream_proof ) {
                ex |= WS_EX_TOOLWINDOW;
                ex &= ~WS_EX_APPWINDOW;
            } else {
                ex |= WS_EX_APPWINDOW;
                ex &= ~WS_EX_TOOLWINDOW;
            }
            if ( m_visible ) {
                ex &= ~WS_EX_TRANSPARENT;
            } else {
                ex |= WS_EX_TRANSPARENT;
            }
            SetWindowLongW( m_hwnd, GWL_EXSTYLE, ex );
            ShowWindow( m_hwnd, SW_SHOWNA );

            if ( stream_proof_changed ) {
                ShowWindow( m_hwnd, SW_HIDE );
                ShowWindow( m_hwnd, SW_SHOWNA );
            }
        }
        else {
            ShowWindow( m_hwnd, SW_HIDE );
        }
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

    void c_overlay::render_frame( ) {
        bool should_show = m_visible || (m_lab_enabled && m_hud_enabled);
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

        if ( m_visible ) {
            ImDrawList* bg_dl = ImGui::GetBackgroundDrawList( );
            const ImVec2 disp = ImGui::GetIO( ).DisplaySize;
            const float bg_a = m_menu_open_anim * 0.2f;
            if ( bg_a > 0.01f ) {
                const int a = static_cast<int>( bg_a * 255 );
                bg_dl->AddRectFilledMultiColor(
                    ImVec2( 0, 0 ), disp,
                    IM_COL32( 2, 2, 12, a ), IM_COL32( 2, 2, 12, a ),
                    IM_COL32( 4, 2, 16, a ), IM_COL32( 4, 2, 16, a ) );
            }
        }

        if ( m_visible ) {
            draw_menu( snap );
            m_streamproof_hide_cursor = stream_proof && ( ImGui::GetIO( ).WantCaptureMouse );
        }
        else {
            m_streamproof_hide_cursor = false;
        }

        if(m_lab_enabled&&m_hud_enabled&&m_lab_access){
            auto* hud=ImGui::GetForegroundDrawList();
            band_ui::card(hud,ImVec2(18,18),ImVec2(330,74));
            char info[128];std::snprintf(info,sizeof(info),"OSU!BAND   /   %s",m_modules_paused?"PAUSED":snap.game.attached?"CONNECTED":"WAITING");
            band_ui::text(hud,ImVec2(33,31),info,14,band_ui::rose);
            std::snprintf(info,sizeof(info),"%zu objects    %d ms    %c / %c",snap.object_count,snap.game.cur_time,m_custom_left_key,m_custom_right_key);
            band_ui::text(hud,ImVec2(33,61),info,13,band_ui::muted);
        }
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
        m_studio.style=s.replay_enabled?5:s.tap_enabled?4:s.relax_enabled?(s.relax_ur<35?3:2):s.aim_enabled&&!s.aim_legit_mode?1:0;
        m_studio.connected=snap.game.attached;m_studio.compatible=!snap.game.offset_mismatch;
        m_studio.map_loaded=snap.beatmap.loaded;m_studio.objects=static_cast<int>(snap.object_count);
        m_studio.time_ms=snap.game.cur_time;m_studio.paused=m_modules_paused;
        m_studio.lab_allowed=m_lab_access;
        m_studio.status=snap.game.attached?(snap.game.is_replay?"REPLAY VIEW / MODULES SUSPENDED":snap.beatmap.loaded?"LAZER / BEATMAP READY":"LAZER / CONNECTED"):"WAITING FOR OSU!LAZER";
        m_studio.map=snap.game.beatmap_version.empty()?"No beatmap selected":snap.game.beatmap_version;
        {std::lock_guard<std::recursive_mutex> guard(m_settings_mutex);
        m_studio.replay_frames=static_cast<int>(m_replay.frame_count());m_studio.replay_player=m_replay.player_name();}
        if(!m_authorized)m_studio.message="Session unavailable. Reconnect your loader to continue.";
        const auto a=band_ui::draw(m_studio,s);
        if(a.pause)m_modules_paused=!m_modules_paused;
        if(a.close)m_visible=false;
        if(a.changed){apply_settings(s);m_studio.message="Settings applied. F8 pauses every module.";}
        if(a.refresh)refresh_profiles();
        if(a.save&&!m_studio.cloud_busy){
            // Replay paths are local-only; never publish someone’s filesystem paths.
            auto portable=s;portable.replay_path_utf8.clear();
            std::ostringstream out;config::serialize_settings(out,m_studio.profile_name,portable);
            const char* styles[]={"Legit","Rage","Relax Legit","Relax Rage","Relax + Aim","Aim + Assist","Autobot","Tap","Replay"};
            cloud_task(2,{{"name",m_studio.profile_name},{"description",m_studio.description},{"style",styles[m_studio.style]},
                          {"cfg",out.str()},{"submit",m_studio.submit}});
        }
        if(a.load&&!m_studio.cloud_busy&&m_studio.selected>=0&&m_studio.selected<static_cast<int>(m_studio.profiles.size())){
            const auto& p=m_studio.profiles[m_studio.selected];cloud_task(3,{{"id",p.id},{"revision",p.revision}});
        }
        if(a.undo&&m_undo_config){m_pending_config=m_undo_config;m_pending_name="Undo";m_studio.message="Config queued until the map ends.";}
        if(a.browse_replay){wchar_t file[32768]{};OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=m_hwnd;
            ofn.lpstrFilter=L"osu! replay (*.osr)\0*.osr\0\0";ofn.lpstrFile=file;ofn.nMaxFile=32768;
            ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
            if(GetOpenFileNameW(&ofn))WideCharToMultiByte(CP_UTF8,0,file,-1,m_studio.replay_path,sizeof(m_studio.replay_path),nullptr,nullptr);}
        if(a.load_replay){s.replay_path_utf8=m_studio.replay_path;
            if(s.replay_path_utf8==m_replay_path_utf8){std::lock_guard<std::recursive_mutex> guard(m_settings_mutex);m_replay.load_replay();}
            else apply_settings(s);
            m_studio.message=m_replay.replay_valid()?"Replay loaded. Choose playback mode and enable the module.":m_replay.last_load_error();}
        if(a.website){try{auto c=cloud::client::restore();auto url=cloud::wide(c.origin+"/beta");ShellExecuteW(m_hwnd,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}catch(...){m_studio.message="Open your account from the loader.";}}
    }
}
