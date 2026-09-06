#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <ctime>
#include <tlhelp32.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <impl/cloud/client.hxx>
#include <impl/util/crash_report.hxx>
#include <impl/cloud/avatar.hxx>
#include <impl/ui/loader_view.hxx>
#include <future>
#include <chrono>
#include <cstring>
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dwmapi.lib")
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
HWND window=nullptr;ID3D11Device* gpu=nullptr;ID3D11DeviceContext* context=nullptr;IDXGISwapChain* swapchain=nullptr;ID3D11RenderTargetView* target=nullptr;
bool running=true;cloud::client client;band_loader::model view;cloud::json session,release;std::atomic<float> progress{0};
cloud::avatar_cache avatars;std::string avatar_url;uint64_t next_runtime=0;
std::string device_code;uint64_t next_poll=0,poll_deadline=0,next_process_check=0;
struct result {int kind=0;cloud::json data;std::string error;};std::future<result> job;
void task(int kind,std::function<cloud::json()> work){if(view.busy)return;view.busy=true;if(kind!=6)view.message.clear();job=std::async(std::launch::async,[kind,work](){try{return result{kind,work(),{}};}catch(const std::exception& e){return result{kind,{},e.what()};}});}
bool process_running(const wchar_t* name){PROCESSENTRY32W pe{};pe.dwSize=sizeof(pe);HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(h==INVALID_HANDLE_VALUE)return false;bool found=false;if(Process32FirstW(h,&pe))do{if(_wcsicmp(pe.szExeFile,name)==0){found=true;break;}}while(Process32NextW(h,&pe));CloseHandle(h);return found;}
void show_connection_page(){view.page=0;}
void refresh(){auto c=client;const std::string channel=view.lab_channel?"lab":"stable";task(3,[c,channel](){auto s=c.session(channel);cloud::json out={{"session",s}};if(s.value("authorized",false)){try{out["release"]=c.api("releases?channel="+channel).at("release");}catch(const std::exception& e){out["releaseError"]=e.what();}}return out;});}
void open_site(){try{std::string url=client.origin+"/dashboard";if(!view.code.empty())url+="?device="+view.code;if(!cloud::valid_origin(client.origin))throw std::runtime_error("Set the website address in Connection first");auto w=cloud::wide(url);if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",w.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)throw std::runtime_error("Could not open your browser");}catch(const std::exception& e){view.message=e.what();show_connection_page();}}
void collect(){if(!job.valid()||job.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;auto r=job.get();view.busy=false;
 if(!r.error.empty()){if(r.kind==6)return;view.message=r.error;if(r.kind==1||r.kind==2){device_code.clear();view.code.clear();}return;}
 if(r.kind==1){device_code=r.data.at("deviceCode");view.code=r.data.at("userCode");next_poll=GetTickCount64()+5000;poll_deadline=GetTickCount64()+300000;open_site();}
 if(r.kind==2){if(r.data.value("status","")=="approved"){client.access_token=r.data.at("accessToken");client.persist();device_code.clear();view.code.clear();refresh();}else next_poll=GetTickCount64()+5000;}
 if(r.kind==3){session=r.data.at("session");view.connected=true;view.authorized=session.value("authorized",false);view.user=session.value("account",cloud::json::object()).value("displayName","OSU!BAND member");view.lab_access=session.value("labAccess",false);
  view.plan=view.authorized?(session.value("plan","active")+" subscription"):session.value("code","Subscription inactive");
  if(view.authorized){if(session.contains("expiresAt")&&!session["expiresAt"].is_null()){std::time_t until=session["expiresAt"].get<int64_t>()/1000;std::tm t{};gmtime_s(&t,&until);char date[40]{};std::strftime(date,sizeof(date)," / until %Y-%m-%d",&t);view.plan+=date;}else view.plan+=" / lifetime";}release=r.data.value("release",cloud::json::object());view.version=release.value("version","No release published");avatar_url=session.value("account",cloud::json::object()).value("avatarUrl","");view.status=view.authorized?"Account checked. Ready to launch.":"Open your account to activate or resume your subscription.";if(r.data.contains("releaseError"))view.message=r.data["releaseError"].get<std::string>();}
 if(r.kind==6){view.appearance=cloud::runtime_settings::parse(r.data.at("settings"));}
 if(r.kind==5){view.message="OSU!BAND started.";if(view.close_after)running=false;}
}
void handle(const band_loader::actions& a){
 if(a.close)running=false;if(a.minimize)ShowWindow(window,SW_MINIMIZE);if(a.open_site)open_site();
 if(a.logout){client.disconnect();view.connected=false;view.authorized=false;session={};release={};avatar_url.clear();view.avatar=0;view.message="This loader has disconnected.";}
 if(a.connect){if(!cloud::valid_origin(client.origin)){show_connection_page();view.message="Enter your website address first.";}else{auto c=client;task(1,[c](){return c.start();});}}
 if(a.refresh||a.channel)refresh();
 if(a.launch){if(!view.osu_running){view.message="Start osu!lazer before launching OSU!BAND.";return;}if(process_running(L"OSUBAND.exe")){view.message="OSU!BAND is already running. Press F4 to open its menu.";return;}if(!release.is_object()||!release.contains("sha256")){view.message="No release is published for this channel.";return;}
  auto c=client;auto manifest=release;const std::string channel=view.lab_channel?"lab":"stable";view.status="Downloading and verifying the release...";progress=0;
  task(5,[c,manifest,channel](){auto s=c.session(channel);if(!s.value("authorized",false))throw std::runtime_error("Subscription is not active");auto path=cloud::install(manifest,progress);c.persist();std::ofstream config_file(path.parent_path()/L"channel.txt",std::ios::trunc);config_file<<channel;config_file.close();cloud::launch(path);return cloud::json{{"ok",true}};});}
 if(a.open_osu){wchar_t app[MAX_PATH]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",app,MAX_PATH);if(n&&n<MAX_PATH){auto path=std::filesystem::path(app)/L"osulazer"/L"current"/L"osu!.exe";if(std::filesystem::is_regular_file(path))cloud::launch(path);else view.message="osu!lazer was not found in its default folder. Open it manually.";}}
}
void target_cleanup(){if(target){target->Release();target=nullptr;}}
void target_create(){ID3D11Texture2D* back=nullptr;if(SUCCEEDED(swapchain->GetBuffer(0,IID_PPV_ARGS(&back)))){gpu->CreateRenderTargetView(back,nullptr,&target);back->Release();}}
LRESULT CALLBACK wndproc(HWND h,UINT m,WPARAM w,LPARAM l){if(ImGui_ImplWin32_WndProcHandler(h,m,w,l))return true;
 if(m==WM_NCHITTEST){POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);if(p.y<85&&p.x<805)return HTCAPTION;}
 if(m==WM_SIZE&&gpu&&w!=SIZE_MINIMIZED){target_cleanup();swapchain->ResizeBuffers(0,LOWORD(l),HIWORD(l),DXGI_FORMAT_UNKNOWN,0);target_create();return 0;}
 if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
 crash_report::install();
 SetProcessDPIAware();WNDCLASSEXW wc{sizeof(wc),CS_CLASSDC,wndproc,0,0,instance,nullptr,LoadCursor(nullptr,IDC_ARROW),nullptr,nullptr,L"OSUBAND.Loader",nullptr};RegisterClassExW(&wc);
 window=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,L"OSU!BAND Loader",WS_POPUP,(GetSystemMetrics(SM_CXSCREEN)-band_loader::width)/2,(GetSystemMetrics(SM_CYSCREEN)-band_loader::height)/2,band_loader::width,band_loader::height,nullptr,nullptr,instance,nullptr);if(!window)return 1;
 DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=2;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=window;sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
 if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&swapchain,&gpu,nullptr,&context)))return 2;target_create();if(!target)return 2;
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
 band_ui::load_fonts("C:\\Windows\\Fonts\\segoeui.ttf");
 ImGui_ImplWin32_Init(window);ImGui_ImplDX11_Init(gpu,context);MARGINS margins{-1,-1,-1,-1};DwmExtendFrameIntoClientArea(window,&margins);ShowWindow(window,SW_SHOW);
 band_ui::load_preferences();
 try{client=cloud::client::restore();if(!client.access_token.empty())refresh();else if(client.origin.empty())show_connection_page();}catch(const std::exception& e){view.message=e.what();show_connection_page();}
 while(running){MSG msg;while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);if(msg.message==WM_QUIT)running=false;}if(!running)break;
  try{collect();const auto now=GetTickCount64();if(now>=next_process_check){view.osu_running=process_running(L"osu!.exe");next_process_check=now+1500;}
   if(!view.busy&&!device_code.empty()&&now>=next_poll){if(now>poll_deadline){device_code.clear();view.code.clear();view.message="Connection code expired. Connect again.";}else{auto c=client;auto code=device_code;task(2,[c,code](){return c.poll(code);});}}
   if(!view.busy&&device_code.empty()&&now>=next_runtime){next_runtime=now+60000;auto c=client;task(6,[c]{return c.api("beta/runtime",nullptr,false);});}
   avatars.tick(gpu);view.avatar=reinterpret_cast<ImTextureID>(avatars.get(avatar_url));
   view.progress=progress.load();ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();handle(band_loader::draw(view));ImGui::Render();
   if(target){const float clear[]={.04f,.04f,.06f,1};context->OMSetRenderTargets(1,&target,nullptr);context->ClearRenderTargetView(target,clear);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());swapchain->Present(1,0);}
  }catch(const std::exception& e){view.busy=false;view.message=e.what();}Sleep(1);
 }
 if(job.valid())job.wait();avatars.clear();ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();target_cleanup();if(swapchain)swapchain->Release();if(context)context->Release();if(gpu)gpu->Release();DestroyWindow(window);UnregisterClassW(wc.lpszClassName,instance);return 0;
}
