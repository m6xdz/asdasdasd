#pragma once
#include <imgui.h>
#include <impl/config/config_store.hxx>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <ctime>
#include <impl/ui/preferences.hxx>

// The same renderer is used by the Windows client and the screenshot harness.
namespace band_ui {
inline constexpr float width=1120, height=720;
inline ImU32 ink=IM_COL32(242,239,245,255), muted=IM_COL32(142,140,153,255);
inline ImU32 rose=IM_COL32(255,111,142,255), border=IM_COL32(255,255,255,19);
inline ImU32 panel=IM_COL32(25,25,32,255), base=IM_COL32(16,16,22,255);
inline ImFont* heading_font=nullptr;
inline float ui_scale=1.f;
inline float S(float v){return v*ui_scale;}
inline ImVec2 SV(ImVec2 v){return ImVec2(S(v.x),S(v.y));}
inline void load_fonts(const char* path){
    auto& io=ImGui::GetIO();ImFontConfig cfg{};cfg.OversampleH=cfg.OversampleV=2;
    if(!io.Fonts->AddFontFromFileTTF(path,16,&cfg,io.Fonts->GetGlyphRangesCyrillic()))io.Fonts->AddFontDefault();
    heading_font=io.Fonts->AddFontFromFileTTF(path,32,&cfg,io.Fonts->GetGlyphRangesCyrillic());
}
struct model {
    int page=0, module=0, selected=-1;
    std::string user="Local workspace", plan="Not connected", status="Waiting for osu!lazer";
    std::string version="2026.804.2", map="No beatmap selected", message;
    bool lab_allowed=false, connected=false, compatible=true, map_loaded=false, paused=false, authorized=false;
    int objects=0, time_ms=0, replay_frames=0;
    std::string replay_player;
    std::vector<config::profile_meta_t> profiles;
    char profile_name[96]="Legit", recipient[80]="You", description[240]="";
    char replay_path[512]="";
    ImVec2 offset{};
    float time=0;
    ImTextureID avatar=0;
    std::vector<ImTextureID> profile_avatars;
    std::string user_id;
    int collection=0,style=0;
    bool cloud_busy=false,submit=false,can_undo=false,waiting_menu=false;
};
struct actions {
    bool changed=false, save=false, load=false, refresh=false, folder=false;
    bool browse_replay=false, load_replay=false, close=false, pause=false, bind_menu=false;
};
inline void text(ImDrawList* d,ImVec2 p,const char* s,float size=16,ImU32 color=0) {
    d->AddText(size>20&&heading_font?heading_font:ImGui::GetFont(),S(size),p,color?color:ink,tr(s));
}
inline void card(ImDrawList* d,ImVec2 p,ImVec2 size,ImU32 fill=0,float radius=14) {
    size=SV(size);radius=S(radius);
    d->AddRectFilled(p,ImVec2(p.x+size.x,p.y+size.y),fill?fill:panel,radius);
    d->AddRect(p,ImVec2(p.x+size.x,p.y+size.y),border,radius);
}
inline bool button(const char* id,const char* label,ImVec2 p,ImVec2 size,bool primary=false,bool enabled=true) {
    label=tr(label);
    size=SV(size);
    ImGui::SetCursorScreenPos(p); ImGui::BeginDisabled(!enabled);
    const bool hit=ImGui::InvisibleButton(id,size); auto* d=ImGui::GetWindowDrawList();
    const bool hover=ImGui::IsItemHovered();
    ImU32 fill=primary?(hover?IM_COL32(255,145,166,255):rose):(hover?ImGui::GetColorU32(ImGuiCol_FrameBgHovered):ImGui::GetColorU32(ImGuiCol_FrameBg));
    if(!enabled)fill=panel;
    d->AddRectFilled(p,ImVec2(p.x+size.x,p.y+size.y),fill,S(9));
    if(!primary)d->AddRect(p,ImVec2(p.x+size.x,p.y+size.y),border,S(9));
    auto ts=ImGui::CalcTextSize(label);
    d->AddText(ImVec2(p.x+(size.x-ts.x)/2,p.y+(size.y-ts.y)/2),enabled?(primary?IM_COL32(24,18,28,255):ink):muted,label);
    ImGui::EndDisabled(); return hit;
}
inline bool toggle(const char* id,const char* label,bool& value,ImVec2 p,float w,const char* note=nullptr) {
    const float sw=S(w);ImGui::SetCursorScreenPos(p);bool changed=ImGui::InvisibleButton(id,ImVec2(sw,S(note?51:31)));
    if(changed)value=!value;
    auto* d=ImGui::GetWindowDrawList();text(d,p,label,16);
    if(note)text(d,ImVec2(p.x,p.y+S(25)),note,13,muted);
    ImVec2 a(p.x+sw-S(42),p.y),b(a.x+S(40),a.y+S(22));
    d->AddRectFilled(a,b,value?rose:IM_COL32(67,65,78,255),S(11));
    d->AddCircleFilled(ImVec2(a.x+S(value?29:11),a.y+S(11)),S(7),value?base:ink,24);
    return changed;
}
inline bool slider(const char* label,const char* id,float& value,float lo,float hi,ImVec2 p,float w,const char* format="%.2f") {
    auto* d=ImGui::GetWindowDrawList();text(d,p,label,14,muted);
    ImGui::SetCursorScreenPos(ImVec2(p.x,p.y+S(25))); ImGui::SetNextItemWidth(S(w));
    return ImGui::SliderFloat(id,&value,lo,hi,format,ImGuiSliderFlags_AlwaysClamp);
}
inline bool slider_i(const char* label,const char* id,int& value,int lo,int hi,ImVec2 p,float w,const char* format="%d") {
    auto* d=ImGui::GetWindowDrawList();text(d,p,label,14,muted);
    ImGui::SetCursorScreenPos(ImVec2(p.x,p.y+S(25)));ImGui::SetNextItemWidth(S(w));
    return ImGui::SliderInt(id,&value,lo,hi,format,ImGuiSliderFlags_AlwaysClamp);
}
inline void input(const char* label,char* data,size_t n,ImVec2 p,float w) {
    ImGui::SetCursorScreenPos(p);ImGui::SetNextItemWidth(S(w));ImGui::InputText(label,data,n);
}
inline void brand(ImDrawList* d,ImVec2 p,float size=34) {
    size=S(size);const ImVec2 c(p.x+size/2,p.y+size/2);
    const float t=prefs.animations?static_cast<float>(ImGui::GetTime())*prefs.motion:0.f;
    d->AddCircleFilled(c,size*.49f,panel,48);
    d->PathArcTo(c,size*.43f,t,t+4.9f,48);d->PathStroke(rose,0,size*.08f);
    d->PathArcTo(c,size*.28f,-t+1,-t+5.3f,36);d->PathStroke(ink,0,size*.065f);
    d->AddCircleFilled(c,size*.095f,rose,24);
}
inline void apply_theme() {
    rose=prefs.theme==1?IM_COL32(83,182,255,255):IM_COL32(255,111,142,255);
    base=prefs.theme==2?IM_COL32(204,208,218,255):IM_COL32(14,15,23,255);
    panel=prefs.theme==2?IM_COL32(224,227,234,255):IM_COL32(24,26,38,255);
    ink=prefs.theme==2?IM_COL32(32,35,46,255):IM_COL32(242,244,251,255);
    muted=prefs.theme==2?IM_COL32(82,88,104,255):IM_COL32(158,167,190,255);
    border=prefs.theme==2?IM_COL32(38,45,61,42):IM_COL32(185,204,255,27);
    auto& s=ImGui::GetStyle();s.WindowPadding=ImVec2(0,0);s.WindowBorderSize=0;s.WindowRounding=18;
    s.FramePadding=ImVec2(12,8);s.FrameRounding=7;s.GrabRounding=4;s.GrabMinSize=10;
    s.Colors[ImGuiCol_Text]=ImVec4(.95f,.94f,.96f,1);
    s.Colors[ImGuiCol_TextDisabled]=ImVec4(.56f,.55f,.60f,1);
    s.Colors[ImGuiCol_FrameBg]=ImVec4(.19f,.18f,.23f,1);
    s.Colors[ImGuiCol_FrameBgHovered]=ImVec4(.25f,.22f,.28f,1);
    s.Colors[ImGuiCol_FrameBgActive]=ImVec4(.3f,.24f,.29f,1);
    s.Colors[ImGuiCol_SliderGrab]=ImVec4(1,.43f,.55f,1);
    s.Colors[ImGuiCol_SliderGrabActive]=ImVec4(1,.65f,.74f,1);
    s.Colors[ImGuiCol_CheckMark]=ImVec4(1,.43f,.55f,1);
    s.Colors[ImGuiCol_PopupBg]=ImVec4(.12f,.12f,.16f,1);
    s.Colors[ImGuiCol_Header]=ImVec4(.32f,.19f,.25f,1);
    s.Colors[ImGuiCol_HeaderHovered]=ImVec4(.39f,.22f,.29f,1);
    s.Colors[ImGuiCol_Button]=ImVec4(.20f,.19f,.24f,1);
    s.Colors[ImGuiCol_ButtonHovered]=ImVec4(.29f,.24f,.31f,1);
    s.Colors[ImGuiCol_Text]=ImGui::ColorConvertU32ToFloat4(ink);
    s.Colors[ImGuiCol_TextDisabled]=ImGui::ColorConvertU32ToFloat4(muted);
    if(prefs.theme==2){
        s.Colors[ImGuiCol_FrameBg]=ImVec4(.76f,.78f,.83f,1);
        s.Colors[ImGuiCol_FrameBgHovered]=ImVec4(.72f,.74f,.80f,1);
        s.Colors[ImGuiCol_FrameBgActive]=ImVec4(.68f,.71f,.78f,1);
        s.Colors[ImGuiCol_PopupBg]=ImVec4(.84f,.85f,.89f,1);
        s.Colors[ImGuiCol_Header]=ImVec4(.82f,.70f,.76f,1);
        s.Colors[ImGuiCol_HeaderHovered]=ImVec4(.86f,.74f,.80f,1);
        s.Colors[ImGuiCol_Button]=ImVec4(.76f,.78f,.83f,1);
    }
    s.Colors[ImGuiCol_SliderGrab]=ImGui::ColorConvertU32ToFloat4(rose);
    s.Colors[ImGuiCol_CheckMark]=ImGui::ColorConvertU32ToFloat4(rose);
}
inline void preference_controls(ImVec2 p,float w){
    bool dirty=false;auto* d=ImGui::GetWindowDrawList();
    text(d,p,"Language",16);
    if(button("##ru","Русский",ImVec2(p.x,p.y+S(30)),ImVec2(w/2-S(6),35),prefs.russian)){prefs.russian=true;dirty=true;}
    if(button("##en","English",ImVec2(p.x+S(w)/2+S(6),p.y+S(30)),ImVec2(w/2-S(6),35),!prefs.russian)){prefs.russian=false;dirty=true;}
    text(d,ImVec2(p.x,p.y+S(84)),"Theme",16);
    const char* themes[]={"Rose","Ocean","Light"};for(int i=0;i<3;++i)if(button(themes[i],themes[i],ImVec2(p.x+i*S(w)/3,p.y+S(110)),ImVec2(w/3-S(8),34),prefs.theme==i)){prefs.theme=i;dirty=true;}
    dirty|=toggle("##motion","Animations",prefs.animations,ImVec2(p.x,p.y+S(165)),w);
    dirty|=slider("Animation speed","##motion-speed",prefs.motion,.25f,2.f,ImVec2(p.x,p.y+S(205)),w,"%.2fx");
    if(dirty)save_preferences();
}
inline std::string key_name(int vk){
    if(vk>='A'&&vk<='Z')return std::string(1,static_cast<char>(vk));
    if(vk>='0'&&vk<='9')return std::string(1,static_cast<char>(vk));
    if(vk>=0x70&&vk<=0x87)return "F"+std::to_string(vk-0x6f);
    switch(vk){case 0x2D:return "Insert";case 0x24:return "Home";case 0x23:return "End";case 0x21:return "Page Up";case 0x22:return "Page Down";case 0x09:return "Tab";case 0x20:return "Space";default:return "VK "+std::to_string(vk);}
}
inline std::string date_label(int64_t ms){
    if(ms<=0)return "—";std::time_t raw=static_cast<std::time_t>(ms/1000);std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm,&raw);
#else
    localtime_r(&raw,&tm);
#endif
    char out[24]{};std::strftime(out,sizeof(out),"%Y-%m-%d",&tm);return out;
}

inline actions draw(model& m,config::settings_t& s) {
    actions a;auto& io=ImGui::GetIO();if(prefs.animations)m.time+=io.DeltaTime*prefs.motion;apply_theme();
    const float desired=std::clamp(prefs.menu_scale,0.75f,1.35f);
    const float fit=std::max(0.75f,std::min((io.DisplaySize.x-20.f)/width,(io.DisplaySize.y-20.f)/height));
    ui_scale=std::min(desired,fit);const float sw=S(width),sh=S(height);
    ImVec2 origin(std::max(0.f,(io.DisplaySize.x-sw)/2)+m.offset.x,std::max(0.f,(io.DisplaySize.y-sh)/2)+m.offset.y);
    ImGui::SetNextWindowPos(origin,ImGuiCond_Always);ImGui::SetNextWindowSize(ImVec2(sw,sh),ImGuiCond_Always);
    ImGui::Begin("OSU!BAND Studio",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
    ImGui::SetWindowFontScale(ui_scale);auto* d=ImGui::GetWindowDrawList();auto P=[&](float x,float y){return ImVec2(origin.x+S(x),origin.y+S(y));};
    card(d,origin,ImVec2(width,height),base,18);
    d->AddRectFilledMultiColor(P(1,1),P(width-1,94),panel,panel,base,base);
    ImGui::SetCursorScreenPos(P(0,0));ImGui::InvisibleButton("##drag",SV(ImVec2(880,83)));
    if(ImGui::IsItemActive()){m.offset.x+=io.MouseDelta.x;m.offset.y+=io.MouseDelta.y;}
    brand(d,P(28,24),44);text(d,P(85,29),"OSU BAND",26);
    if(m.avatar)d->AddImageRounded(m.avatar,P(819,20),P(865,66),ImVec2(0,0),ImVec2(1,1),IM_COL32_WHITE,23);
    text(d,P(876,24),m.user.substr(0,22).c_str(),15);text(d,P(876,47),m.plan.substr(0,23).c_str(),12,muted);
    if(button("##close","x",P(1060,23),ImVec2(32,32)))a.close=true;
    const char* pages[]={"Play","Profiles","Beta","System"};
    for(int i=0;i<4;++i){if(button(pages[i],pages[i],P(28+i*124,99),ImVec2(112,39),m.page==i))m.page=i;}
    if(button("##pause",m.paused?"Resume modules":"Pause modules  F8",P(843,99),ImVec2(249,39)))a.pause=true;
    d->AddLine(P(28,157),P(1092,157),border);
    if(m.page==0) {
        text(d,P(28,181),"Play",28);
        const char* names[]={"Aim Assist","Relax","Tap Assist","Replay","Autobot"};
        const char* desc[]={"Cursor correction","Automatic key timing","Physical tap adjustment","Your .osr playback","Full path automation"};
        const bool on[]={s.aim_enabled,s.relax_enabled,s.tap_enabled,s.replay_enabled,s.autobot_enabled};
        for(int i=0;i<5;++i){float y=266+i*67.f;
            ImGui::SetCursorScreenPos(P(28,y));if(ImGui::InvisibleButton(names[i],SV(ImVec2(247,57))))m.module=i;
            card(d,P(28,y),ImVec2(247,57),m.module==i?ImGui::GetColorU32(ImGuiCol_Header):panel,10);
            char n[4];std::snprintf(n,sizeof(n),"%02d",i+1);text(d,P(44,y+18),n,14,m.module==i?rose:muted);
            text(d,P(79,y+10),names[i],16);text(d,P(79,y+32),desc[i],12,muted);
            if(on[i])d->AddCircleFilled(P(256,y+19),3,rose,16);
        }
        card(d,P(297,181),ImVec2(795,433));
        text(d,P(321,203),names[m.module],24);text(d,P(321,239),desc[m.module],14,muted);
        d->AddLine(P(321,273),P(1067,273),border);
        bool* enabled[]={&s.aim_enabled,&s.relax_enabled,&s.tap_enabled,&s.replay_enabled,&s.autobot_enabled};
        if(toggle("##module-enable","Enable module",*enabled[m.module],P(870,209),195)){
            if(*enabled[m.module]) {
                if(m.module==0){s.replay_enabled=false;s.autobot_enabled=false;}
                if(m.module==1){s.tap_enabled=false;s.replay_enabled=false;s.autobot_enabled=false;}
                if(m.module==2){s.relax_enabled=false;s.replay_enabled=false;s.autobot_enabled=false;}
                if(m.module==3){s.aim_enabled=false;s.relax_enabled=false;s.tap_enabled=false;s.autobot_enabled=false;}
                if(m.module==4){s.aim_enabled=false;s.relax_enabled=false;s.tap_enabled=false;s.replay_enabled=false;}
            }a.changed=true;
        }
        if(m.module==0){
            a.changed|=slider("Horizontal strength","##sx",s.aim_strength_x,1,15,P(321,295),341,"%.1f");
            a.changed|=slider("Vertical strength","##sy",s.aim_strength_y,1,15,P(700,295),367,"%.1f");
            a.changed|=slider("Smoothing","##smooth",s.aim_lerp,.15f,.4f,P(321,367),341);
            a.changed|=slider("Approach window","##window",s.aim_window,75,110,P(700,367),367,"%.0f ms");
            a.changed|=slider("Distance falloff","##falloff",s.aim_decay_far,.01f,.5f,P(321,439),341);
            a.changed|=slider("Freeze smoothing","##freeze",s.aim_freeze_lerp,.1f,.3f,P(700,439),367);
            a.changed|=toggle("##tablet","Tablet mode",s.aim_tablet_mode,P(321,516),341);
            a.changed|=toggle("##sliders","Ignore sliders",s.aim_ignore_sliders,P(700,516),367);
            a.changed|=toggle("##clamp","Limit correction",s.aim_legit_mode,P(321,561),341);
            if(s.aim_legit_mode){ImGui::SetCursorScreenPos(P(700,552));ImGui::SetNextItemWidth(S(367));a.changed|=ImGui::SliderFloat("##clampval",&s.aim_legit_clamp,1.05f,1.8f,"Clamp %.2f",ImGuiSliderFlags_AlwaysClamp);}
        }else if(m.module==1){
            a.changed|=slider("Timing variation / UR","##ur",s.relax_ur,0,200,P(321,295),341,"%.0f");
            a.changed|=slider_i("Manual timing offset","##offset",s.relax_manual_offset_ms,-100,100,P(700,295),367,"%d ms");
            bool st=s.relax_tap_style==1;if(toggle("##single","Prefer single tap",st,P(321,377),341)){s.relax_tap_style=st?1:0;a.changed=true;}
            a.changed|=slider_i("Single tap BPM ceiling","##bpm",s.relax_singletap_bpm_cap,100,300,P(700,367),367,"%d BPM");
            a.changed|=slider("K1 hold","##k1",s.relax_k1_hold_center,30,120,P(321,438),341,"%.0f ms");
            a.changed|=slider("K2 hold","##k2",s.relax_k2_hold_center,30,120,P(700,438),367,"%.0f ms");
            a.changed|=slider("K1 spread","##k1spread",s.relax_k1_hold_spread,2,30,P(321,510),341,"%.0f ms");
            a.changed|=slider("K2 spread","##k2spread",s.relax_k2_hold_spread,2,30,P(700,510),367,"%.0f ms");
        }else if(m.module==2){
            a.changed|=slider_i("Assist window","##tapwindow",s.tap_assist_window,0,250,P(321,299),341,"%d ms");
            a.changed|=slider_i("Timing variation","##taprandom",s.tap_randomization,0,40,P(700,299),367,"%d ms");
            a.changed|=toggle("##tapsliders","Ignore sliders",s.tap_ignore_sliders,P(321,394),341);
            text(d,P(321,462),"Works with your physical taps.",22);text(d,P(321,500),"Relax and Tap Assist use separate input paths.",14,muted);
            text(d,P(321,526),"Enabling one automatically turns the other off.",14,muted);
        }else if(m.module==3){
            text(d,P(321,296),"REPLAY FILE",12,muted);input("##replaypath",m.replay_path,sizeof(m.replay_path),P(321,320),595);
            if(button("##browse","Browse",P(930,320),ImVec2(137,35)))a.browse_replay=true;
            if(button("##loadreplay","Load replay",P(321,370),ImVec2(166,38),true))a.load_replay=true;
            char stats[128];std::snprintf(stats,sizeof(stats),"%d frames  /  %s",m.replay_frames,m.replay_player.empty()?"No replay loaded":m.replay_player.c_str());text(d,P(507,382),stats,14,muted);
            text(d,P(321,441),"Playback mode",16);
            const char* modes[]={"Full playback","Cursor only","Keys only"};
            for(int i=0;i<3;++i){bool selected=(i==0&&s.replay_move_cursor&&s.replay_parse_buttons)||(i==1&&s.replay_move_cursor&&!s.replay_parse_buttons)||(i==2&&!s.replay_move_cursor&&s.replay_parse_buttons);
                if(button(modes[i],modes[i],P(321+i*251,477),ImVec2(235,42),selected)) {s.replay_move_cursor=i!=2;s.replay_parse_buttons=i!=1;a.changed=true;}}
            text(d,P(321,555),"Replay must match the selected beatmap.",14,muted);
        }else{
            a.changed|=slider("Aim spread","##spread",s.autobot_aim_spread,0,1,P(321,299),341);
            a.changed|=slider("Curve strength","##curve",s.autobot_curve_strength,0,1,P(700,299),367);
            a.changed|=slider("Drift amount","##drift",s.autobot_drift_amount,0,5,P(321,390),341);
            a.changed|=slider("Momentum","##momentum",s.autobot_momentum,0,.95f,P(700,390),367);
            a.changed|=slider("Slider follow","##lazy",s.autobot_slider_laziness,0,1,P(321,481),341);
            a.changed|=slider("Spinner speed","##rpm",s.autobot_spinner_rpm,200,477,P(700,481),367,"%.0f RPM");
        }
    }else if(m.page==1){
        text(d,P(28,180),"OSU!BAND configs",28);
        if(button("##refresh","Refresh",P(952,180),ImVec2(140,37),false,!m.cloud_busy))a.refresh=true;
        card(d,P(28,235),ImVec2(636,364));
        ImGui::SetCursorScreenPos(P(40,247));ImGui::BeginChild("##cloudlist",SV(ImVec2(612,340)));
        bool any=false;
        for(size_t i=0;i<m.profiles.size();++i){const auto& profile=m.profiles[i];
            any=true;const auto p=ImGui::GetCursorScreenPos();auto* row=ImGui::GetWindowDrawList();
            ImGui::PushID(static_cast<int>(i));if(ImGui::InvisibleButton("##config",SV(ImVec2(599,115))))m.selected=static_cast<int>(i);
            card(row,p,ImVec2(599,107),m.selected==static_cast<int>(i)?ImGui::GetColorU32(ImGuiCol_Header):panel,10);
            const float x12=S(12),x46=S(46),x56=S(56),x425=S(425),x438=S(438),x584=S(584);
            if(i<m.profile_avatars.size()&&m.profile_avatars[i])row->AddImageRounded(m.profile_avatars[i],ImVec2(p.x+x12,p.y+S(12)),ImVec2(p.x+x46,p.y+S(46)),ImVec2(0,0),ImVec2(1,1),IM_COL32_WHITE,S(17));
            else brand(row,ImVec2(p.x+x12,p.y+S(12)),34);
            row->PushClipRect(ImVec2(p.x+x56,p.y),ImVec2(p.x+x425,p.y+S(58)),true);
            text(row,ImVec2(p.x+x56,p.y+S(10)),profile.name.c_str(),17);
            const auto role=profile.author_role=="admin"?std::string(tr("Administrator")):std::string(tr("Player"));
            const auto by=profile.author+" · "+role+" · v"+std::to_string(profile.revision);text(row,ImVec2(p.x+x56,p.y+S(35)),by.c_str(),12,muted);row->PopClipRect();
            text(row,ImVec2(p.x+x438,p.y+S(12)),profile.channel=="lab"?"Beta":"Stable",13,rose);
            const auto stamp=date_label(profile.updated_at);text(row,ImVec2(p.x+x438,p.y+S(35)),stamp.c_str(),12,muted);
            row->PushClipRect(ImVec2(p.x+x12,p.y+S(56)),ImVec2(p.x+x584,p.y+S(101)),true);
            text(row,ImVec2(p.x+x12,p.y+S(60)),profile.description.c_str(),13,muted);
            if(!profile.review_note.empty())text(row,ImVec2(p.x+x12,p.y+S(82)),profile.review_note.c_str(),12,rose);
            row->PopClipRect();ImGui::PopID();
        }
        if(!any)ImGui::TextWrapped("%s",tr(m.cloud_busy?"Loading...":"No configs yet"));ImGui::EndChild();
        card(d,P(686,235),ImVec2(406,364));text(d,P(708,254),"Your settings",21);
        text(d,P(708,292),"Name",13,muted);input("##name",m.profile_name,sizeof(m.profile_name),P(708,313),360);
        text(d,P(708,354),"Style",13,muted);ImGui::SetCursorScreenPos(P(708,375));ImGui::SetNextItemWidth(S(360));
        ImGui::BeginDisabled();ImGui::Combo("##style",&m.style,"Legit\0Rage\0Relax Legit\0Relax Rage\0Tap\0Replay\0");ImGui::EndDisabled();
        text(d,P(708,416),"Description",13,muted);input("##description",m.description,sizeof(m.description),P(708,438),360);
        toggle("##submit","Submit for review",m.submit,P(708,486),360);
        if(button("##save","Save to cloud",P(708,541),ImVec2(360,37),true,!m.cloud_busy))a.save=true;
        if(button("##apply","Apply",P(28,613),ImVec2(196,36),true,!m.cloud_busy&&m.selected>=0))a.load=true;
        text(d,P(242,623),"Changes apply between maps",13,muted);
    }else if(m.page==2){
        text(d,P(28,181),"Beta",28);
        card(d,P(28,250),ImVec2(1064,355));
        text(d,P(52,276),"Beta",22);
        if(!s.lab_enabled){s.lab_enabled=true;a.changed=true;}
        a.changed|=toggle("##watermark","Watermark",s.hud_enabled,P(52,330),470);
        text(d,P(52,390),"OSU!BAND Beta [osuband.dev]",18,rose);
        const auto preview=std::string("00:00:00 / ")+m.user.substr(0,28);text(d,P(52,425),preview.c_str(),15,muted);
        text(d,P(52,485),m.paused?"MODULES PAUSED":"Modules ready",17,m.paused?rose:ink);
        text(d,P(52,530),"F8  /  Pause all modules",16,muted);
        text(d,P(590,330),"Beta channel",17);
        text(d,P(590,370),m.lab_allowed?"Beta access active":"Beta access unavailable",15,m.lab_allowed?ink:rose);
        text(d,P(590,420),"Watermark shows version, local time and account name.",14,muted);
    }else{
        text(d,P(28,181),"Settings",28);
        card(d,P(28,264),ImVec2(518,350));card(d,P(566,264),ImVec2(526,350));
        preference_controls(P(52,287),470);
        if(slider("Menu scale","##menu-scale",prefs.menu_scale,.75f,1.35f,P(52,548),470,"%.2fx")){prefs.menu_scale=std::clamp(prefs.menu_scale,.75f,1.35f);save_preferences();}
        text(d,P(590,288),"INPUT",12,muted);
        const char* keys[]={"Z / X","S / D","A / S","K / L"};const int left[]={'Z','S','A','K'},right[]={'X','D','S','L'};
        text(d,P(590,327),"Gameplay keys",17);
        for(int i=0;i<4;++i){if(button(keys[i],keys[i],P(590+i*121,369),ImVec2(109,38),s.custom_left_key==left[i]&&s.custom_right_key==right[i])){s.custom_left_key=left[i];s.custom_right_key=right[i];a.changed=true;}}
        const auto menuText=m.waiting_menu?std::string(tr("Press a key...")):std::string(tr("Menu key"))+" · "+key_name(s.menu_keybind);
        if(button("##menu-key",menuText.c_str(),P(590,438),ImVec2(478,39),false,true))a.bind_menu=true;
        text(d,P(590,493),"F8  /  Pause all modules",17);
        a.changed|=toggle("##capture","Exclude menu from capture",s.stream_proof,P(590,548),478);
    }
    if(!m.message.empty()){d->PushClipRect(P(28,655),P(1092,682),true);text(d,P(29,660),m.message.c_str(),13,rose);d->PopClipRect();}
    d->AddLine(P(28,684),P(1092,684),border);
    text(d,P(29,697),m.paused?"MODULES PAUSED":m.status.c_str(),11,m.paused?rose:muted);
    text(d,P(505,697),"OSU!BAND / 1.1.0",11,muted);
    text(d,P(908,697),"BETA · 1.1.0-beta.2",11,rose);
    ImGui::End();return a;
}
}
