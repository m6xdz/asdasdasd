#include <impl/config/config_store.hxx>
#include <nlohmann/json.hpp>
#include <iostream>
int main(){
 nlohmann::json out=nlohmann::json::array();
 const char* styles[]={"Legit","Rage","Relax Legit","Relax Rage","Tap","Replay"};
 const char* descriptions[]={"Мягкая ограниченная коррекция курсора. Нажатия вручную.","Сильная коррекция курсора без легитного ограничения. Нажатия вручную.","Relax с разбросом тайминга. Курсор вручную.","Relax с небольшим разбросом тайминга. Курсор вручную.","Коррекция твоих нажатий; автоматические нажатия выключены.","Повтор с движением курсора. Выбери .osr и включи Replay в клиенте."};
 for(int i=0;i<6;++i){config::settings_t s;s.lab_enabled=true;
  if(i==0){s.aim_enabled=true;s.aim_legit_mode=true;s.aim_strength_x=s.aim_strength_y=3;s.aim_lerp=.18f;}
  if(i==1){s.aim_enabled=true;s.aim_legit_mode=false;s.aim_strength_x=7;s.aim_strength_y=6;s.aim_lerp=.28f;}
  if(i==2||i==3){s.relax_enabled=true;s.relax_ur=i==2?65:20;s.relax_tap_style=0;}
  if(i==4){s.tap_enabled=true;s.tap_assist_window=65;s.tap_randomization=8;}
  if(i==5){s.replay_enabled=false;s.replay_parse_buttons=false;s.replay_move_cursor=true;}
  std::ostringstream cfg;config::serialize_settings(cfg,styles[i],s);
  out.push_back({{"id","beta-standard-"+std::to_string(i)},{"name",styles[i]},{"style",styles[i]},{"description",descriptions[i]},{"cfg",cfg.str()}});
 }
 std::cout<<out.dump(2)<<'\n';
}
