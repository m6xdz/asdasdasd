#include <impl/ui/studio.hxx>
#include <impl/ui/loader_view.hxx>
#include <fstream>
#include <vector>
#include <cstring>

// CPU renderer for the actual Dear ImGui triangle output. No mocked HTML UI.
int main(int argc,char** argv){
    if(argc<3)return 1;
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize=ImVec2(1180,780);io.DeltaTime=1.f/60;
    io.IniFilename=nullptr;io.LogFilename=nullptr;
    if(argc>3)band_ui::load_fonts(argv[3]);else io.Fonts->AddFontDefault();
    unsigned char* tex=nullptr;int tw=0,th=0;io.Fonts->GetTexDataAsRGBA32(&tex,&tw,&th);io.Fonts->SetTexID(static_cast<ImTextureID>(1));
    band_ui::model m;m.page=std::atoi(argv[2]);m.user="modeof19";m.plan="Beta";m.connected=true;m.map_loaded=true;m.lab_allowed=true;
    m.status="LAZER / BEATMAP READY";m.map="A new beginning [Insane]";m.objects=742;m.time_ms=32170;m.time=1.5f;
    const char* names[]={"Legit","Rage","Relax Legit","Relax Rage"};
    for(int i=0;i<4;++i){config::profile_meta_t p;p.name=names[i];p.author="modeof19";p.official=true;p.status="published";p.description="Проверка отображения описания конфига";m.profiles.push_back(p);}m.selected=1;
    if(argc>4)band_ui::prefs.theme=std::atoi(argv[4]);
    band_loader::model loader;loader.connected=loader.authorized=loader.lab_access=loader.osu_running=true;loader.user="modeof19";loader.plan="Beta";loader.page=m.page==5?1:0;
    config::settings_t s;s.aim_enabled=true;s.lab_enabled=true;s.hud_enabled=true;
    for(int i=0;i<2;++i){ImGui::NewFrame();if(m.page<4)band_ui::draw(m,s);else band_loader::draw(loader);ImGui::Render();}
    auto* data=ImGui::GetDrawData();const int w=1180,h=780;std::vector<unsigned char> image(w*h*3);
    for(int i=0;i<w*h;++i){image[i*3]=10;image[i*3+1]=10;image[i*3+2]=15;}
    auto edge=[](ImVec2 a,ImVec2 b,ImVec2 c){return (c.x-a.x)*(b.y-a.y)-(c.y-a.y)*(b.x-a.x);};
    for(int l=0;l<data->CmdListsCount;++l){const auto* list=data->CmdLists[l];for(const auto& cmd:list->CmdBuffer){
      if(cmd.UserCallback)continue;
      for(unsigned j=0;j<cmd.ElemCount;j+=3){const auto& a=list->VtxBuffer[list->IdxBuffer[cmd.IdxOffset+j]+cmd.VtxOffset];
        const auto& b=list->VtxBuffer[list->IdxBuffer[cmd.IdxOffset+j+1]+cmd.VtxOffset];const auto& c=list->VtxBuffer[list->IdxBuffer[cmd.IdxOffset+j+2]+cmd.VtxOffset];
        float area=edge(a.pos,b.pos,c.pos);if(std::abs(area)<1e-6f)continue;
        int minx=std::max({0,(int)std::floor(std::min({a.pos.x,b.pos.x,c.pos.x})),(int)cmd.ClipRect.x});
        int maxx=std::min({w-1,(int)std::ceil(std::max({a.pos.x,b.pos.x,c.pos.x})),(int)cmd.ClipRect.z-1});
        int miny=std::max({0,(int)std::floor(std::min({a.pos.y,b.pos.y,c.pos.y})),(int)cmd.ClipRect.y});
        int maxy=std::min({h-1,(int)std::ceil(std::max({a.pos.y,b.pos.y,c.pos.y})),(int)cmd.ClipRect.w-1});
        for(int y=miny;y<=maxy;++y)for(int x=minx;x<=maxx;++x){ImVec2 p(x+.5f,y+.5f);
          float u=edge(b.pos,c.pos,p)/area,v=edge(c.pos,a.pos,p)/area,q=1-u-v;if(u<0||v<0||q<0)continue;
          const float tx=(a.uv.x*u+b.uv.x*v+c.uv.x*q)*tw,ty=(a.uv.y*u+b.uv.y*v+c.uv.y*q)*th;
          int ti=(std::clamp((int)ty,0,th-1)*tw+std::clamp((int)tx,0,tw-1))*4;
          float alpha=(((a.col>>24)&255)*u+((b.col>>24)&255)*v+((c.col>>24)&255)*q)/255.f*tex[ti+3]/255.f;
          for(int k=0;k<3;++k){float col=(((a.col>>(k*8))&255)*u+((b.col>>(k*8))&255)*v+((c.col>>(k*8))&255)*q)*tex[ti+k]/255.f;
            auto& out=image[(y*w+x)*3+k];out=static_cast<unsigned char>(std::clamp(col*alpha+out*(1-alpha),0.f,255.f));}
        }
      }
    }}
    std::ofstream out(argv[1],std::ios::binary);out<<"P6\n"<<w<<" "<<h<<"\n255\n";out.write(reinterpret_cast<const char*>(image.data()),image.size());
    ImGui::DestroyContext();return out?0:2;
}
