#pragma once
#include <impl/ui/studio.hxx>
#include <impl/cloud/runtime_settings.hxx>
namespace band_loader {
inline constexpr int width=920,height=590;
struct model {
 int page=0,selected=-1;bool connected=false,authorized=false,busy=false,can_stable=false,can_beta=false,lab_access=false,lab_channel=false,close_after=true,osu_running=false;
 std::string user="Your next session",plan="Connect your OSU!BAND account",code,version="1.1.0",status="Ready when you are.",message;
 float progress=0,time=0;ImTextureID avatar=0;cloud::runtime_settings appearance;
};
struct actions {bool connect=false,refresh=false,open_site=false,launch=false,close=false,minimize=false,channel=false,logout=false,open_osu=false;};
inline actions draw(model& m){using namespace band_ui;actions a;apply_theme();auto& io=ImGui::GetIO();
 const auto& remote=m.appearance;const auto rgb=std::stoul(remote.accent.substr(1),nullptr,16);
 rose=IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255);
 if(prefs.animations&&remote.showAnimation)m.time+=io.DeltaTime*prefs.motion*remote.motion;
 ImGui::SetNextWindowPos(ImVec2(0,0),ImGuiCond_Always);ImGui::SetNextWindowSize(ImVec2(width,height),ImGuiCond_Always);
 ImGui::Begin("OSU!BAND Loader",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
 auto* d=ImGui::GetWindowDrawList();auto P=[](float x,float y){return ImVec2(x,y);};card(d,P(0,0),P(width,height),base,18);
 d->AddRectFilledMultiColor(P(1,1),P(918,165),panel,panel,base,base);
 const auto saved=prefs;prefs.animations=prefs.animations&&remote.showAnimation;prefs.motion*=remote.motion;
 if(remote.logo=="pulse"){const float k=prefs.animations?1.f+.12f*std::sin(m.time*3):1.f;d->AddCircle(P(46,43),16*k,rose,48,3);d->AddCircleFilled(P(46,43),5,ink,24);}else brand(d,P(29,26),34);
 prefs=saved;
 d->PushClipRect(P(76,20),P(805,65),true);text(d,P(76,27),remote.title.c_str(),24);d->PopClipRect();
 if(button("##min","-",P(823,27),P(29,28)))a.minimize=true;
 if(button("##close","x",P(864,27),P(29,28)))a.close=true;
 const char* tabs[]={"Session","Settings"};for(int i=0;i<2;++i)if(button(tabs[i],tabs[i],P(29+i*151,100),P(140,37),m.page==i))m.page=i;
 if(m.connected&&button("##account","My account",P(745,100),P(147,37)))a.open_site=true;
 d->AddLine(P(29,156),P(891,156),border);
 if(m.page==0){
  text(d,P(30,189),m.connected?"All set.":"Ready when",36);text(d,P(30,235),m.connected?"Find your rhythm.":"you are.",36,rose);
  if(m.connected&&m.avatar)d->AddImageRounded(m.avatar,P(32,290),P(80,338),ImVec2(0,0),ImVec2(1,1),IM_COL32_WHITE,24);
  d->PushClipRect(P(m.connected?94:32,292),P(365,329),true);text(d,P(m.connected?94:32,301),m.connected?m.user.c_str():"One account. One place to start.",16);d->PopClipRect();
  if(remote.showPlan){d->PushClipRect(P(32,351),P(365,375),true);text(d,P(32,354),m.plan.c_str(),13,muted);d->PopClipRect();}
  for(int i=0;i<32;++i){float h=8+22*(.5f+.5f*std::sin(i*.47f+m.time*1.5f));d->AddLine(P(34+i*8.5f,413-h),P(34+i*8.5f,413+h),IM_COL32(255,111,142,30+i*2),3);}
  d->PushClipRect(P(32,451),P(362,507),true);d->AddText(ImGui::GetFont(),13,P(32,455),muted,remote.announcement.c_str(),nullptr,324);d->PopClipRect();
  card(d,P(381,181),P(511,323));
  if(!m.connected){
   text(d,P(405,205),"Connect your account",24);text(d,P(405,245),"Confirm this loader in your browser.",14,muted);
   if(!m.code.empty()){
    text(d,P(407,287),"YOUR CONNECTION CODE",11,muted);text(d,P(407,319),(m.code.substr(0,4)+" "+m.code.substr(4)).c_str(),34,rose);
    text(d,P(407,375),"Waiting for confirmation on the website...",14,muted);
    if(button("##browser","Open website",P(406,430),P(461,45),true,!m.busy))a.open_site=true;
   }else{
    text(d,P(406,295),"Your password stays in the browser.",15);text(d,P(406,328),"Subscription and configs follow your account.",14,muted);
    if(button("##connect",m.busy?"Connecting...":remote.connectLabel.c_str(),P(406,430),P(461,45),true,!m.busy))a.connect=true;
   }
  }else{
   text(d,P(405,205),"OSU BAND",24);if(remote.showVersion)text(d,P(405,241),m.version.c_str(),13,muted);
   const char* state=m.authorized?"SUBSCRIPTION ACTIVE":"SUBSCRIPTION REQUIRED";text(d,P(667,242),state,11,m.authorized?IM_COL32(155,214,175,255):rose);
   if(button("##stable","Stable",P(406,283),P(223,38),!m.lab_channel,!m.busy&&m.can_stable)){m.lab_channel=false;a.channel=true;}
   if(button("##lab","Beta",P(643,283),P(223,38),m.lab_channel,!m.busy&&m.can_beta)){m.lab_channel=true;a.channel=true;}
   if(!m.can_beta) text(d,P(645,326),"Beta subscription required",11,muted);
   text(d,P(407,343),m.osu_running?"osu!lazer is running":"Open osu!lazer before starting",14,m.osu_running?ink:muted);
   if(m.busy){ImGui::SetCursorScreenPos(P(406,378));ImGui::ProgressBar(m.progress,P(461,7),"");text(d,P(406,397),m.status.substr(0,59).c_str(),12,muted);}
   if(button("##launch",m.busy?"Preparing your session...":m.authorized?remote.launchLabel.c_str():"Open account to activate a key",P(406,444),P(286,39),true,!m.busy)) {if(m.authorized)a.launch=true;else a.open_site=true;}
   if(button("##open-osu-session","Open osu!lazer",P(704,444),P(162,39),false,!m.busy))a.open_osu=true;
   ImGui::SetCursorScreenPos(P(31,518));ImGui::Checkbox(tr("Close loader after launch"),&m.close_after);
  }
 }else{
  text(d,P(30,181),"Settings",27);
  card(d,P(30,232),P(450,306));preference_controls(P(54,251),402);
  card(d,P(500,232),P(392,306));text(d,P(524,258),"My account",22);
  d->PushClipRect(P(524,302),P(866,330),true);text(d,P(524,306),m.user.c_str(),17);d->PopClipRect();
  if(m.connected&&button("##refresh-account","Refresh",P(524,359),P(344,38),false,!m.busy))a.refresh=true;
  if(m.connected&&button("##logout","Disconnect this loader",P(524,415),P(344,38),false,!m.busy))a.logout=true;
 }
 d->AddLine(P(29,554),P(891,554),border);ImGui::SetCursorScreenPos(P(30,565));ImGui::PushTextWrapPos(891);ImGui::TextColored(ImVec4(.74f,.61f,.68f,1),"%s",tr((m.message.empty()?m.status:m.message).c_str()));ImGui::PopTextWrapPos();ImGui::End();return a;
}
}
