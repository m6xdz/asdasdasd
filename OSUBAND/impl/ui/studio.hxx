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
inline ImU32 panel=IM_COL32(25,25,32,255), panel2=IM_COL32(32,23,39,255), base=IM_COL32(16,16,22,255);
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
    int collection=0;
    bool cloud_busy=false,can_undo=false,waiting_menu=false;
};
struct actions {
    bool changed=false, save_private=false, submit_review=false, load=false, refresh=false;
    bool install=false, uninstall=false, uninject=false;
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
    base=prefs.theme==2?IM_COL32(248,243,248,255):IM_COL32(13,11,18,255);
    panel=prefs.theme==2?IM_COL32(255,250,253,255):IM_COL32(24,20,31,255);
    panel2=prefs.theme==2?IM_COL32(246,231,240,255):IM_COL32(35,24,42,255);
    ink=prefs.theme==2?IM_COL32(39,28,43,255):IM_COL32(247,241,250,255);
    muted=prefs.theme==2?IM_COL32(111,92,111,255):IM_COL32(166,151,174,255);
    border=prefs.theme==2?IM_COL32(176,92,132,38):IM_COL32(255,131,173,28);
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
    if(ms<=0)return "—";
    std::time_t raw=static_cast<std::time_t>(ms/1000);std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm,&raw);
#else
    localtime_r(&raw,&tm);
#endif
    char out[24]{};std::strftime(out,sizeof(out),"%Y-%m-%d",&tm);return out;
}

inline void small_tag(ImDrawList* d,ImVec2 p,const char* label,ImU32 color){
    const auto ts=ImGui::CalcTextSize(label);const float w=ts.x+S(22),h=S(24);
    d->AddRectFilled(p,{p.x+w,p.y+h},IM_COL32((color)&255,(color>>8)&255,(color>>16)&255,28),S(12));
    d->AddRect(p,{p.x+w,p.y+h},IM_COL32((color)&255,(color>>8)&255,(color>>16)&255,90),S(12));
    d->AddText({p.x+S(11),p.y+S(5)},color,label);
}
inline void stat_line(ImDrawList* d,ImVec2 p,const char* label,const char* value,ImU32 value_color=0){
    text(d,p,label,12,muted);text(d,{p.x,p.y+S(21)},value,15,value_color?value_color:ink);
}
inline actions draw(model& m,config::settings_t& s) {
    actions a;auto& io=ImGui::GetIO();if(prefs.animations)m.time+=io.DeltaTime*prefs.motion;apply_theme();
    const float desired=std::clamp(prefs.menu_scale,0.75f,1.35f);
    const float fit=std::max(0.75f,std::min((io.DisplaySize.x-20.f)/width,(io.DisplaySize.y-20.f)/height));
    ui_scale=std::min(desired,fit);const float sw=S(width),sh=S(height);
    ImVec2 origin(std::max(0.f,(io.DisplaySize.x-sw)/2)+m.offset.x,std::max(0.f,(io.DisplaySize.y-sh)/2)+m.offset.y);
    ImGui::SetNextWindowPos(origin,ImGuiCond_Always);ImGui::SetNextWindowSize({sw,sh},ImGuiCond_Always);
    ImGui::Begin("OSU!BAND",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
    ImGui::SetWindowFontScale(ui_scale);auto* d=ImGui::GetWindowDrawList();auto P=[&](float x,float y){return ImVec2(origin.x+S(x),origin.y+S(y));};

    // shell
    card(d,origin,{width,height},base,22);
    d->AddRectFilledMultiColor(P(0,0),P(1120,170),panel2,panel2,base,base);
    card(d,P(18,18),{190,684},panel,18);card(d,P(222,18),{880,684},base,18);
    ImGui::SetCursorScreenPos(P(222,18));ImGui::InvisibleButton("##drag",SV({620,92}));if(ImGui::IsItemActive()){m.offset.x+=io.MouseDelta.x;m.offset.y+=io.MouseDelta.y;}

    // brand rail
    brand(d,P(40,38),48);text(d,P(98,41),"OSU!BAND",22);small_tag(d,P(98,69),"BETA",rose);
    text(d,P(40,118),"CONTROL",10,muted);
    const char* pages[]={"Play","Config","Beta","Settings"};
    const char* sub[]={"Modules","Cloud library","Preview channel","Client options"};
    for(int i=0;i<4;++i){const float y=145+i*66.f;ImGui::SetCursorScreenPos(P(30,y));if(ImGui::InvisibleButton(pages[i],SV({166,54})))m.page=i;
        const bool active=m.page==i;d->AddRectFilled(P(30,y),P(196,y+54),active?panel2:panel,S(12));if(active){d->AddRectFilled(P(30,y+10),P(34,y+44),rose,S(2));}
        text(d,P(48,y+9),pages[i],15,active?ink:muted);text(d,P(48,y+31),sub[i],10,muted);
    }
    d->AddLine(P(38,445),P(188,445),border);
    text(d,P(40,466),"SESSION",10,muted);text(d,P(40,492),m.paused?"Paused":"Ready",16,m.paused?rose:ink);
    text(d,P(40,518),m.status.substr(0,22).c_str(),11,muted);
    if(button("##railpause",m.paused?"Resume  F8":"Pause  F8",P(38,558),{152,36},false,true))a.pause=true;
    text(d,P(40,657),"1.2.0-beta",11,muted);text(d,P(40,676),"osuband.dev",11,rose);

    // top command/account strip
    text(d,P(246,39),m.page==0?"Control deck":m.page==1?"Cloud configs":m.page==2?"Beta channel":"Preferences",27);
    text(d,P(246,73),m.page==0?"Tune the active module without leaving the session.":m.page==1?"Everything stays in cloud. Install, apply or publish.":m.page==2?"Experimental runtime status and visual options.":"Appearance, input and client behaviour.",12,muted);
    card(d,P(760,31),{310,64},panel,15);
    if(m.avatar!=ImTextureID_Invalid)d->AddImageRounded(m.avatar,P(772,41),P(816,85),{0,0},{1,1},IM_COL32_WHITE,S(22));else brand(d,P(772,41),44);
    d->AddCircleFilled(P(812,80),S(4),m.authorized?IM_COL32(95,231,173,255):rose,20);
    text(d,P(828,42),m.user.substr(0,20).c_str(),15);text(d,P(828,64),m.plan.substr(0,24).c_str(),11,muted);small_tag(d,P(982,51),"BETA",rose);
    if(button("##close","x",P(1072,34),{22,22}))a.close=true;
    d->AddLine(P(238,112),P(1084,112),border);

    if(m.page==0){
        const char* names[]={"Aim Assist","Relax","Tap Assist","Replay","Autobot"};
        const char* desc[]={"Cursor","Timing","Tap fix","Replay","Route"};
        const bool on[]={s.aim_enabled,s.relax_enabled,s.tap_enabled,s.replay_enabled,s.autobot_enabled};
        for(int i=0;i<5;++i){float x=238+i*166.f;ImGui::SetCursorScreenPos(P(x,130));if(ImGui::InvisibleButton(names[i],SV({154,50})))m.module=i;
            const bool active=m.module==i;d->AddRectFilled(P(x,130),P(x+154,180),active?panel2:panel,S(12));d->AddRect(P(x,130),P(x+154,180),active?IM_COL32(255,111,142,95):border,S(12));
            if(on[i]) d->AddCircleFilled(P(x+136,146),S(3.5f),rose,20);
            text(d,P(x+14,140),names[i],14,active?ink:muted);
            text(d,P(x+14,160),desc[i],10,muted);
        }
        card(d,P(238,198),{600,456},panel,16);card(d,P(852,198),{232,456},panel2,16);
        text(d,P(260,218),names[m.module],23);text(d,P(260,250),"MODULE SETTINGS",10,muted);
        bool* enabled[]={&s.aim_enabled,&s.relax_enabled,&s.tap_enabled,&s.replay_enabled,&s.autobot_enabled};
        if(toggle("##module-enable","Enabled",*enabled[m.module],P(657,218),155)){
            if(*enabled[m.module]){if(m.module==0){s.replay_enabled=false;s.autobot_enabled=false;}if(m.module==1){s.tap_enabled=false;s.replay_enabled=false;s.autobot_enabled=false;}if(m.module==2){s.relax_enabled=false;s.replay_enabled=false;s.autobot_enabled=false;}if(m.module==3){s.aim_enabled=false;s.relax_enabled=false;s.tap_enabled=false;s.autobot_enabled=false;}if(m.module==4){s.aim_enabled=false;s.relax_enabled=false;s.tap_enabled=false;s.replay_enabled=false;}}a.changed=true;
        }
        d->AddLine(P(260,282),P(816,282),border);
        if(m.module==0){
            text(d,P(260,296),"Movement",12,rose);a.changed|=slider("Horizontal strength","##sx",s.aim_strength_x,1,15,P(260,322),255,"%.1f");a.changed|=slider("Vertical strength","##sy",s.aim_strength_y,1,15,P(548,322),255,"%.1f");
            a.changed|=slider("Smoothing","##smooth",s.aim_lerp,.15f,.4f,P(260,394),255);a.changed|=slider("Approach window","##window",s.aim_window,75,110,P(548,394),255,"%.0f ms");
            a.changed|=slider("Distance falloff","##falloff",s.aim_decay_far,.01f,.5f,P(260,466),255);a.changed|=slider("Freeze smoothing","##freeze",s.aim_freeze_lerp,.1f,.3f,P(548,466),255);
            a.changed|=toggle("##tablet","Tablet mode",s.aim_tablet_mode,P(260,546),255,"Raw tablet-friendly correction");a.changed|=toggle("##sliders","Slider safety",s.aim_ignore_sliders,P(548,546),255,"Never target slider heads");
            a.changed|=toggle("##clamp","Limit correction",s.aim_legit_mode,P(260,606),255);if(s.aim_legit_mode){ImGui::SetCursorScreenPos(P(548,603));ImGui::SetNextItemWidth(S(255));a.changed|=ImGui::SliderFloat("##clampval",&s.aim_legit_clamp,1.05f,1.8f,"Clamp %.2f",ImGuiSliderFlags_AlwaysClamp);}
        } else if(m.module==1){
            text(d,P(260,296),"Stable timing core",12,rose);a.changed|=slider("Timing variation / UR","##ur",s.relax_ur,0,200,P(260,322),255,"%.0f");a.changed|=slider_i("Manual offset","##offset",s.relax_manual_offset_ms,-100,100,P(548,322),255,"%d ms");
            bool st=s.relax_tap_style==1;if(toggle("##single","Prefer single tap",st,P(260,403),255)){s.relax_tap_style=st?1:0;a.changed=true;}a.changed|=slider_i("Single tap ceiling","##bpm",s.relax_singletap_bpm_cap,100,300,P(548,394),255,"%d BPM");
            a.changed|=slider("K1 hold","##k1",s.relax_k1_hold_center,30,120,P(260,466),255,"%.0f ms");a.changed|=slider("K2 hold","##k2",s.relax_k2_hold_center,30,120,P(548,466),255,"%.0f ms");
            a.changed|=slider("K1 spread","##k1spread",s.relax_k1_hold_spread,2,30,P(260,538),255,"%.0f ms");a.changed|=slider("K2 spread","##k2spread",s.relax_k2_hold_spread,2,30,P(548,538),255,"%.0f ms");
            text(d,P(260,615),"Slider holds are isolated from cursor input and resync after timeline stalls.",11,muted);
        } else if(m.module==2){
            text(d,P(260,296),"Physical-input correction",12,rose);a.changed|=slider_i("Assist window","##tapwindow",s.tap_assist_window,0,250,P(260,326),255,"%d ms");a.changed|=slider_i("Timing variation","##taprandom",s.tap_randomization,0,40,P(548,326),255,"%d ms");
            a.changed|=toggle("##tapsliders","Ignore sliders",s.tap_ignore_sliders,P(260,416),255);card(d,P(260,480),{543,120},panel2,12);text(d,P(280,500),"Input ownership",15);text(d,P(280,528),"Tap Assist and Relax cannot own the same key path.",12,muted);text(d,P(280,552),"Switching one on automatically releases the other.",12,muted);
        } else if(m.module==3){
            text(d,P(260,298),"Replay file",12,rose);input("##replaypath",m.replay_path,sizeof(m.replay_path),P(260,324),410);if(button("##browse","Browse",P(684,324),{118,35}))a.browse_replay=true;
            if(button("##loadreplay","Load replay",P(260,376),{148,38},true)) a.load_replay=true;
            char stats[128];
            std::snprintf(stats,sizeof(stats),"%d frames / %s",m.replay_frames,m.replay_player.empty()?tr("No replay loaded"):m.replay_player.c_str());
            text(d,P(426,387),stats,12,muted);
            text(d,P(260,445),"Playback",12,rose);const char* modes[]={"Full","Cursor","Keys"};for(int i=0;i<3;++i){bool selected=(i==0&&s.replay_move_cursor&&s.replay_parse_buttons)||(i==1&&s.replay_move_cursor&&!s.replay_parse_buttons)||(i==2&&!s.replay_move_cursor&&s.replay_parse_buttons);if(button(modes[i],modes[i],P(260+i*181,474),{165,42},selected)){s.replay_move_cursor=i!=2;s.replay_parse_buttons=i!=1;a.changed=true;}}
            text(d,P(260,545),"Large timeline jumps seek directly to the current replay frame.",12,muted);
        } else {
            text(d,P(260,296),"Automation",12,rose);a.changed|=slider("Aim spread","##spread",s.autobot_aim_spread,0,1,P(260,322),255);a.changed|=slider("Curve strength","##curve",s.autobot_curve_strength,0,1,P(548,322),255);a.changed|=slider("Drift","##drift",s.autobot_drift_amount,0,5,P(260,394),255);a.changed|=slider("Momentum","##momentum",s.autobot_momentum,0,.95f,P(548,394),255);a.changed|=slider("Slider follow","##lazy",s.autobot_slider_laziness,0,1,P(260,466),255);a.changed|=slider("Spinner speed","##rpm",s.autobot_spinner_rpm,200,477,P(548,466),255,"%.0f RPM");text(d,P(260,552),"Scheduler is bounded to a short look-ahead window to avoid catch-up stalls.",12,muted);
        }
        text(d,P(874,221),"LIVE SESSION",10,muted);stat_line(d,P(874,252),"Account",m.user.substr(0,19).c_str());stat_line(d,P(874,304),"Plan",m.plan.substr(0,19).c_str(),rose);stat_line(d,P(874,356),"Beatmap",m.map.substr(0,19).c_str());
        text(d,P(874,418),"Active",11,muted);int ay=443;for(int i=0;i<5;++i)if(on[i]){d->AddCircleFilled(P(878,(float)ay+7),S(3),rose);text(d,P(890,(float)ay),names[i],12);ay+=23;}if(ay==443)text(d,P(874,443),"No modules",12,muted);
        d->AddLine(P(872,552),P(1064,552),border);text(d,P(874,572),"INPUT SAFETY",10,muted);text(d,P(874,595),"Slider guard",12,IM_COL32(104,229,176,255));text(d,P(874,617),"Queue guard",12,IM_COL32(104,229,176,255));
    } else if(m.page==1){
        card(d,P(238,132),{598,522},panel,16);card(d,P(850,132),{234,522},panel2,16);text(d,P(258,151),"Library",20);if(button("##refresh","Refresh",P(708,145),{106,34},false,!m.cloud_busy))a.refresh=true;
        ImGui::SetCursorScreenPos(P(250,194));ImGui::BeginChild("##cloudlist",SV({574,444}));bool any=false;for(size_t i=0;i<m.profiles.size();++i){const auto& profile=m.profiles[i];any=true;auto p=ImGui::GetCursorScreenPos();auto* row=ImGui::GetWindowDrawList();ImGui::PushID((int)i);if(ImGui::InvisibleButton("##config",SV({560,88})))m.selected=(int)i;card(row,p,{560,80},m.selected==(int)i?panel2:base,10);
            if(i<m.profile_avatars.size()&&m.profile_avatars[i]!=ImTextureID_Invalid)row->AddImageRounded(m.profile_avatars[i],{p.x+S(10),p.y+S(12)},{p.x+S(46),p.y+S(48)},{0,0},{1,1},IM_COL32_WHITE,S(18));else brand(row,{p.x+S(10),p.y+S(12)},36);
            text(row,{p.x+S(58),p.y+S(10)},profile.name.c_str(),15);auto by=profile.author+" · v"+std::to_string(profile.revision);text(row,{p.x+S(58),p.y+S(32)},by.c_str(),11,muted);text(row,{p.x+S(430),p.y+S(12)},profile.channel=="lab"?"BETA":"STABLE",10,profile.channel=="lab"?rose:IM_COL32(90,210,255,255));if(profile.installed)small_tag(row,{p.x+S(430),p.y+S(42)},"INSTALLED",rose);text(row,{p.x+S(58),p.y+S(55)},profile.description.substr(0,52).c_str(),11,muted);ImGui::PopID();}
        if(!any) ImGui::TextWrapped("%s",tr(m.cloud_busy?"Loading...":"No configs yet"));
        ImGui::EndChild();
        text(d,P(870,154),"Publish",18);text(d,P(870,190),"Name",11,muted);input("##name",m.profile_name,sizeof(m.profile_name),P(870,210),194);text(d,P(870,253),"Description",11,muted);input("##description",m.description,sizeof(m.description),P(870,273),194);
        text(d,P(870,322),"Private",11,rose);text(d,P(870,342),"Only your account",11,muted);if(button("##private","Save private",P(870,374),{194,36},false,!m.cloud_busy))a.save_private=true;
        text(d,P(870,430),"Public",11,rose);text(d,P(870,450),"Admin review first",11,muted);if(button("##review","Submit review",P(870,482),{194,36},true,!m.cloud_busy))a.submit_review=true;
        bool can=!m.cloud_busy&&m.selected>=0&&m.selected<(int)m.profiles.size();if(can){const auto& sel=m.profiles[(size_t)m.selected];if(sel.installed){if(button("##apply","Apply",P(870,548),{92,36},true))a.load=true;if(button("##remove","Remove",P(972,548),{92,36}))a.uninstall=true;}else if(button("##install","Install",P(870,548),{194,36},true))a.install=true;}else button("##none","Select a config",P(870,548),{194,36},false,false);
        text(d,P(870,605),"Cloud only",11,muted);
    } else if(m.page==2){
        card(d,P(238,132),{846,522},panel,16);text(d,P(260,154),"Beta runtime",24);small_tag(d,P(970,154),m.lab_allowed?"ACCESS ACTIVE":"NO ACCESS",m.lab_allowed?rose:muted);
        card(d,P(260,210),{250,150},panel2,14);text(d,P(280,230),"Watermark",12,muted);a.changed|=toggle("##watermark","Show overlay",s.hud_enabled,P(280,260),205);text(d,P(280,314),"OSU!BAND Beta",16,rose);
        card(d,P(528,210),{250,150},panel2,14);text(d,P(548,230),"Runtime",12,muted);text(d,P(548,262),m.paused?"Paused":"Modules ready",19,m.paused?rose:ink);text(d,P(548,298),"F8 toggles all modules",11,muted);
        card(d,P(796,210),{266,150},panel2,14);text(d,P(816,230),"Stability",12,muted);text(d,P(816,262),"Slider guard",14,IM_COL32(104,229,176,255));text(d,P(816,290),"Timeline resync",14,IM_COL32(104,229,176,255));text(d,P(816,318),"Bounded queues",14,IM_COL32(104,229,176,255));
        text(d,P(260,404),"Experimental channel",12,rose);text(d,P(260,434),"Beta keeps the extra modules, but the input paths are supervised so one",14);text(d,P(260,458),"module cannot silently take ownership from another after a slider or stall.",14);text(d,P(260,500),"Watermark preview",12,muted);auto preview=std::string("OSU!BAND Beta [osuband.dev] / 00:00 / ")+m.user.substr(0,22);text(d,P(260,528),preview.c_str(),14,rose);
    } else {
        card(d,P(238,132),{410,522},panel,16);card(d,P(664,132),{420,522},panel,16);text(d,P(260,154),"Appearance",20);preference_controls(P(260,194),366);if(slider("Menu scale","##menu-scale",prefs.menu_scale,.75f,1.35f,P(260,462),366,"%.2fx")){prefs.menu_scale=std::clamp(prefs.menu_scale,.75f,1.35f);save_preferences();}
        text(d,P(686,154),"Input",20);text(d,P(686,196),"Gameplay keys",12,muted);const char* keys[]={"Z / X","S / D","A / S","K / L"};const int left[]={'Z','S','A','K'},right[]={'X','D','S','L'};for(int i=0;i<4;++i)if(button(keys[i],keys[i],P(686+(i%2)*184,222+(i/2)*48),{172,38},s.custom_left_key==left[i]&&s.custom_right_key==right[i])){s.custom_left_key=left[i];s.custom_right_key=right[i];a.changed=true;}
        auto menuText=m.waiting_menu?std::string(tr("Press a key...")):std::string(tr("Menu key"))+" · "+key_name(s.menu_keybind);if(button("##menu-key",menuText.c_str(),P(686,334),{356,40}))a.bind_menu=true;a.changed|=toggle("##capture","Exclude menu from capture",s.stream_proof,P(686,397),356);if(button("##uninject","UNINJECT",P(686,486),{356,40},false,true))a.uninject=true;text(d,P(686,548),"Closes hooks, releases held keys and exits cleanly.",11,muted);
    }

    if(!m.message.empty()){d->PushClipRect(P(238,663),P(1084,688),true);text(d,P(240,667),m.message.c_str(),12,rose);d->PopClipRect();}
    text(d,P(970,680),"BETA",10,rose);ImGui::End();return a;
}
}
