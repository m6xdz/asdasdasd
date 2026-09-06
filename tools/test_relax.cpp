#include <core/relax/relax.hxx>
#include <cassert>
#include <set>
#include <iostream>
int main(){
 relax::c_relax r;r.enabled=true;r.ur=0;r.k1_hold_spread=r.k2_hold_spread=2;
 osu::game_snapshot_t g;g.cur_state=osu::game_state_t::play;g.left_key='Z';g.right_key='X';
 osu::beatmap_data_t map;map.loaded=true;map.od=5;
 for(int t=0;t<3600000;t+=125){osu::hit_object_t obj{};obj.start_time=t;obj.end_time=t;obj.type=1;map.objects.push_back(obj);}
 size_t max_queue=0;std::set<int> down;size_t presses=0;
 auto consume=[&]{for(auto e:fake_input::events){if(e.ki.dwFlags&KEYEVENTF_KEYUP){assert(down.erase(e.ki.wVk)==1);}else{assert(down.insert(e.ki.wVk).second);++presses;}}fake_input::events.clear();};
 for(int t=-100;t<3600200;t+=5){g.cur_time=t;r.update(g,map);max_queue=std::max(max_queue,r.queue_size());consume();}
 r.on_leave_play(g);consume();assert(down.empty());assert(max_queue<24);assert(presses>28000);
 // A retry resets history; changing the mapping still releases the held key.
 g.cur_time=0;r.update(g,map);g.cur_time=10;r.update(g,map);consume();
 g.left_key='A';g.right_key='S';r.on_leave_play(g);consume();assert(down.empty());
 // Failed Win32 input must not be counted as held, or emit unmatched releases.
 fake_input::fail=true;g.cur_time=0;r.update(g,map);g.cur_time=10;r.update(g,map);fake_input::fail=false;r.on_leave_play(g);consume();assert(down.empty());
 // Overlapping same-key slider retriggers must not release somebody else's note.
 map.objects.clear();for(int t=0;t<1000;t+=100){osu::hit_object_t o{};o.type=2;o.start_time=t;o.end_time=t+300;map.objects.push_back(o);}
 r.tap_style=1;r.singletap_bpm_cap=99999;
 for(int t=0;t<1500;t+=5){g.cur_time=t;r.update(g,map);consume();}r.on_leave_play(g);consume();assert(down.empty());
 std::cout<<"PASS: 60-minute simulated map, "<<presses<<" key presses, max queue "<<max_queue<<", retry, changed keys, failed input, overlapping holds\n";
}
