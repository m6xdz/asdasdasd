// Included inside namespace ui. All state mutations occur on the UI thread.
void c_overlay::cloud_task(int kind,cloud::json payload) {
    if(m_studio.cloud_busy)return;
    try {
        auto account=cloud::client::restore();
        m_studio.cloud_busy=true;
        m_cloud_job=std::async(std::launch::async,[account,kind,payload] {
            try {
                if(kind==2)account.api("beta/configs",&payload,true,"lab");
                if(kind==3)account.api("beta/active",&payload,true,"lab");
                if(kind==4)account.api("beta/configs/install",&payload,true,"lab");
                if(kind==5)account.api("beta/configs/uninstall",&payload,true,"lab");
                auto data=account.api("beta/configs",nullptr,true,"lab");
                data["active"]=account.api("beta/active",nullptr,true,"lab").value("config",cloud::json());
                return cloud_result{kind,std::move(data),{}};
            } catch(const std::exception& e){return cloud_result{kind,{},e.what()};}
        });
    } catch(const std::exception& e){m_studio.cloud_busy=false;m_studio.message=e.what();notify("Cloud",e.what(),false);}
}
void c_overlay::refresh_profiles(){cloud_task(1);}
void c_overlay::cloud_tick() {
    m_avatars.tick(m_device);
    m_studio.avatar=reinterpret_cast<ImTextureID>(m_avatars.get(m_avatar_url));
    m_studio.profile_avatars.clear();
    for(const auto& p:m_studio.profiles)m_studio.profile_avatars.push_back(reinterpret_cast<ImTextureID>(m_avatars.get(p.avatar_url)));
    if(m_cloud_job.valid()&&m_cloud_job.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
        auto r=m_cloud_job.get();m_studio.cloud_busy=false;m_next_cloud_poll=GetTickCount64()+8000;
        if(!r.error.empty()){m_studio.message=r.error;notify("Cloud",r.error,false);}
        else try {
            auto selected=m_studio.selected>=0&&m_studio.selected<static_cast<int>(m_studio.profiles.size())?m_studio.profiles[m_studio.selected].id:std::string{};
            std::unordered_map<std::string,std::string> previous=m_known_review_status;
            m_studio.profiles.clear();m_studio.selected=-1;
            m_studio.user_id=r.data.value("userId",m_studio.user_id);
            for(const auto& c:r.data.at("configs")){
                config::profile_meta_t p;p.id=c.at("id");p.owner_id=c.at("owner_id");p.name=c.at("name");
                p.author=c.value("author","");p.avatar_url=c.value("avatar","");p.description=c.value("description","");
                p.status=c.value("status","private");p.review_note=c.value("review_note","");
                p.author_role=c.value("author_role","user");p.updated_at=c.value("updated_at",int64_t(0));
                p.reviewed_at=c.value("reviewed_at",int64_t(0));p.reviewed_by_name=c.value("reviewed_by_name","");
                p.official=c.value("official",0)!=0;p.installed=c.value("installed",false);p.revision=c.at("revision");p.channel=c.value("channel","lab");
                if(p.channel!="lab")continue; // defense in depth: Beta never renders Stable configs.
                if(p.id==selected)m_studio.selected=static_cast<int>(m_studio.profiles.size());
                if(p.owner_id==m_studio.user_id){
                    auto it=previous.find(p.id);
                    if(it!=previous.end()&&it->second!=p.status&&(p.status=="published"||p.status=="rejected")){
                        const auto admin=p.reviewed_by_name.empty()?std::string("Administrator"):p.reviewed_by_name;
                        if(p.status=="published")notify("Config approved",admin+" · "+p.name,true);
                        else notify("Config rejected",admin+" · "+p.name,false);
                    }
                    m_known_review_status[p.id]=p.status;
                }
                m_studio.profiles.push_back(std::move(p));
            }
            const auto& review=r.data.value("reviewTarget",cloud::json());
            if(review.is_object()&&review.contains("config_id")){
                const auto stamp=review.at("config_id").get<std::string>()+":"+std::to_string(review.at("revision").get<int>());
                if(stamp!=m_review_stamp){
                    config::settings_t next;std::istringstream in(review.at("cfg").get<std::string>());
                    if(config::parse_settings(in,next)){config::validate(next);next.replay_path_utf8.clear();next.lab_enabled=true;m_pending_config=next;m_pending_name=review.value("name","Review config");m_review_stamp=stamp;notify("Review config ready",m_pending_name+" · "+review.value("author","") );}
                }
            }
            const auto& active=r.data.at("active");
            if(active.is_object()){
                const auto stamp=active.at("id").get<std::string>()+":"+std::to_string(active.at("updated_at").get<int64_t>());
                if(stamp!=m_active_stamp){
                    config::settings_t next;std::istringstream in(active.at("cfg").get<std::string>());
                    if(!config::parse_settings(in,next))throw std::runtime_error("Invalid cloud config");
                    config::validate(next);next.replay_path_utf8.clear();next.lab_enabled=true;
                    m_pending_config=next;m_pending_name=active.at("name");m_active_stamp=stamp;
                    m_studio.message="Config queued until the map ends.";notify("Config queued",m_pending_name);
                }
            }
            if(r.kind==2){m_studio.message="Config saved in cloud.";notify("Config uploaded",m_studio.profile_name);}
            if(r.kind==3)notify("Config selected",m_pending_name.empty()?"Cloud config":m_pending_name);
            if(r.kind==4){m_studio.message="Config installed from cloud.";notify("Config installed",m_studio.selected>=0&&m_studio.selected<(int)m_studio.profiles.size()?m_studio.profiles[m_studio.selected].name:"Cloud config");}
            if(r.kind==5){m_studio.message="Config removed from your library.";notify("Config removed","You can install it again later.");}
        }catch(const std::exception& e){m_studio.message=e.what();notify("Cloud",e.what(),false);}
    }
    if(m_pending_config&&m_authorized){
        std::lock_guard<std::recursive_mutex> guard(m_settings_mutex);
        if(m_last_game.cur_state!=osu::game_state_t::play){
            apply_settings(*m_pending_config);m_pending_config.reset();
            m_studio.message=std::string(band_ui::tr("Applied: "))+band_ui::tr(m_pending_name.c_str());
            notify("Config applied",m_pending_name);
        }
    }
    if(!m_studio.cloud_busy&&GetTickCount64()>=m_next_cloud_poll){m_next_cloud_poll=GetTickCount64()+8000;refresh_profiles();}
}
