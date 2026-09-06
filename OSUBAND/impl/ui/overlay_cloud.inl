// Included inside namespace ui. All state mutations occur on the UI thread.
void c_overlay::cloud_task(int kind,cloud::json payload) {
    if(m_studio.cloud_busy)return;
    try {
        auto account=cloud::client::restore();
        m_studio.cloud_busy=true;
        m_cloud_job=std::async(std::launch::async,[account,kind,payload] {
            try {
                if(kind==2)account.api("beta/configs",&payload);
                if(kind==3)account.api("beta/active",&payload);
                auto data=account.api("beta/configs");
                data["active"]=account.api("beta/active").value("config",cloud::json());
                return cloud_result{kind,std::move(data),{}};
            } catch(const std::exception& e){return cloud_result{kind,{},e.what()};}
        });
    } catch(const std::exception& e){m_studio.cloud_busy=false;m_studio.message=e.what();}
}
void c_overlay::refresh_profiles(){cloud_task(1);}
void c_overlay::cloud_tick() {
    m_avatars.tick(m_device);
    m_studio.avatar=reinterpret_cast<ImTextureID>(m_avatars.get(m_avatar_url));
    m_studio.profile_avatars.clear();
    for(const auto& p:m_studio.profiles)m_studio.profile_avatars.push_back(reinterpret_cast<ImTextureID>(m_avatars.get(p.avatar_url)));
    if(m_cloud_job.valid()&&m_cloud_job.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
        auto r=m_cloud_job.get();m_studio.cloud_busy=false;m_next_cloud_poll=GetTickCount64()+15000;
        if(!r.error.empty())m_studio.message=r.error;
        else try {
            auto selected=m_studio.selected>=0&&m_studio.selected<static_cast<int>(m_studio.profiles.size())?m_studio.profiles[m_studio.selected].id:std::string{};
            m_studio.profiles.clear();m_studio.selected=-1;
            m_studio.user_id=r.data.value("userId",m_studio.user_id);
            for(const auto& c:r.data.at("configs")){
                config::profile_meta_t p;p.id=c.at("id");p.owner_id=c.at("owner_id");p.name=c.at("name");
                p.author=c.value("author","");p.avatar_url=c.value("avatar","");p.description=c.value("description","");
                p.style=c.value("style","Legit");p.status=c.value("status","private");p.review_note=c.value("review_note","");
                p.official=c.value("official",0)!=0;p.revision=c.at("revision");p.channel="lab";
                if(p.id==selected)m_studio.selected=static_cast<int>(m_studio.profiles.size());
                m_studio.profiles.push_back(std::move(p));
            }
            const auto& active=r.data.at("active");
            if(active.is_object()){
                const auto stamp=active.at("id").get<std::string>()+":"+std::to_string(active.at("updated_at").get<int64_t>());
                if(stamp!=m_active_stamp){
                    config::settings_t next;std::istringstream in(active.at("cfg").get<std::string>());
                    if(!config::parse_settings(in,next))throw std::runtime_error("Invalid cloud config");
                    config::validate(next);next.replay_path_utf8.clear();next.lab_enabled=true;
                    m_pending_config=next;m_pending_name=active.at("name");m_active_stamp=stamp;
                    MessageBeep(MB_OK);
                    m_studio.message="Config queued until the map ends.";
                }
            }
            if(r.kind==2)m_studio.message=m_studio.submit?"Submitted for review.":"Saved to cloud.";
        }catch(const std::exception& e){m_studio.message=e.what();}
    }
    if(m_pending_config&&m_authorized){
        std::lock_guard<std::recursive_mutex> guard(m_settings_mutex);
        if(m_last_game.cur_state!=osu::game_state_t::play){
            auto previous=capture_settings();apply_settings(*m_pending_config);
            m_undo_config=previous;m_pending_config.reset();m_studio.can_undo=true;
            m_studio.message=std::string(band_ui::tr("Applied: "))+band_ui::tr(m_pending_name.c_str());
        }
    }
    if(!m_studio.cloud_busy&&GetTickCount64()>=m_next_cloud_poll){m_next_cloud_poll=GetTickCount64()+15000;refresh_profiles();}
}
