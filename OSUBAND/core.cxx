#include <impl/includes.hxx>
#include <impl/ui/overlay.hxx>
#include <core/threads/cache.hxx>
#include <mmsystem.h>
#include <impl/cloud/client.hxx>
#include <impl/util/crash_report.hxx>
#include <impl/cloud/session_lease.hxx>
#include <impl/util/debug_log.hxx>
#include <thread>
#include <atomic>
#include <chrono>

#pragma comment( lib, "winmm.lib" )

static void enable_process_dpi_awareness( ) {
    typedef BOOL( WINAPI* PFN_SetProcessDpiAwarenessContext )( void* );
    if ( auto user32 = GetModuleHandleW( L"user32.dll" ) ) {
        if (auto SetProcessDpiAwarenessContextFn = ( PFN_SetProcessDpiAwarenessContext )GetProcAddress( user32, "SetProcessDpiAwarenessContext" ) ) {
            SetProcessDpiAwarenessContextFn( ( void* ) -4);
            return;
        }
    }
    typedef HRESULT( WINAPI* PFN_SetProcessDpiAwareness )( int );
    if ( auto shcore = LoadLibraryW( L"shcore.dll") ) {
        if (auto SetProcessDpiAwarenessFn = ( PFN_SetProcessDpiAwareness )GetProcAddress( shcore, "SetProcessDpiAwareness" ) ) {
            SetProcessDpiAwarenessFn( 2 );
            FreeLibrary( shcore );
            return;
        }
        FreeLibrary( shcore );
    }
    SetProcessDPIAware( );
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int ) {
    crash_report::install();
    enable_process_dpi_awareness( );
    struct timer_scope{timer_scope(){timeBeginPeriod(1);}~timer_scope(){timeEndPeriod(1);}} timer;

    cloud::client account;cloud::json entitlement;std::string channel="lab";
    #ifndef OSUBAND_DEVELOPMENT
    try{
        account=cloud::client::restore();
        // This source tree builds Beta only. A neighbouring channel.txt cannot
        // accidentally run the experimental executable as Stable.
        entitlement=account.session(channel);
        if(!entitlement.value("authorized",false))throw std::runtime_error("Subscription inactive. Open your OSU!BAND Loader and account.");
    }catch(const std::exception& e){auto message=cloud::wide(e.what());MessageBoxW(nullptr,message.c_str(),L"OSU!BAND / Account",MB_ICONINFORMATION|MB_OK);return 3;}
    #endif
    threads::c_cache cache;
    if ( !cache.init( ) ) {
        MessageBoxW( nullptr, L"init failed", L"OSU!BAND", MB_ICONERROR | MB_OK );
        return 1;
    }

    ui::c_overlay overlay;
    #ifdef OSUBAND_DEVELOPMENT
    overlay.set_account("LOCAL DEVELOPMENT","Local test build",true);
    #else
    overlay.set_account(entitlement.at("account").value("displayName","OSU!BAND member"),entitlement.value("plan","active"),entitlement.value("labAccess",false),entitlement.at("account").value("avatarUrl",""),entitlement.at("account").value("id",""));
    #endif
    if ( !overlay.create( instance ) ) {
        MessageBoxW( nullptr, L"create failed", L"OSU!BAND", MB_ICONERROR | MB_OK );
        return 1;
    }

    overlay.set_cache( &cache );
    overlay.set_snapshot_source( [ &cache ]() {
        return cache.get_ui_snapshot( );
    } );
    cache.set_module_tick( [ &overlay ]( const osu::game_snapshot_t& game, const osu::beatmap_data_t& beatmap ) {
        overlay.tick_modules( game, beatmap );
    } );

    cache.start( );
    std::atomic<bool> monitoring{true};
    std::thread monitor;
    #ifndef OSUBAND_DEVELOPMENT
    monitor=std::thread([&](){
        cloud::session_lease lease;
        auto accept = [&](const cloud::json& s) {
            const int64_t expiry=s.contains("expiresAt")&&!s["expiresAt"].is_null()?s["expiresAt"].get<int64_t>():0;
            lease.accept(GetTickCount64(),s.value("serverTime",int64_t(0)),expiry);
        };
        accept(entitlement);
        bool lab=entitlement.value("labAccess",false);
        overlay.set_authorized(true,lab,lease.deadline());
        while(monitoring){for(int i=0;i<300&&monitoring;++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));if(!monitoring)break;
            try {
                auto s=account.session(channel);
                const bool allowed=s.value("authorized",false);
                lab=s.value("labAccess",false);
                if(allowed)accept(s);else {lease.deny();dbg::log("session: access denied by server");}
                overlay.set_authorized(allowed,lab,lease.deadline());
            } catch(const cloud::api_error& e) {
                if(e.status==401||e.status==403)lease.deny();
                overlay.set_authorized(lease.valid(GetTickCount64()),lab,lease.deadline());
                dbg::log("session: HTTP %lu; retained=%d",e.status,lease.valid(GetTickCount64()));
            } catch(const std::exception&) {
                overlay.set_authorized(lease.valid(GetTickCount64()),lab,lease.deadline());
                dbg::log("session: network unavailable; retained=%d",lease.valid(GetTickCount64()));
            }
        }
    });
    #endif

    while ( overlay.pump( ) ) {
        if (!cache.healthy()) {
            overlay.set_authorized(false,false);
            cache.stop();
            overlay.reset_modules(cache.get_ui_snapshot().game);
            MessageBoxW(nullptr,L"A background task stopped. Restart OSU BAND; details are in debug.log.",L"OSU BAND Beta",MB_ICONERROR|MB_OK);
            break;
        }
        Sleep( 1 );
    }

    monitoring=false;if(monitor.joinable())monitor.join();
    cache.stop( );
    overlay.destroy( );


    return 0;
}
