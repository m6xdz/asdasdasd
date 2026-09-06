#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <regex>
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace cloud {
struct runtime_settings {
    std::string title="OSU BAND",announcement,accent="#ff6f8e",logo="orbit";
    std::string connectLabel="Connect account",launchLabel="Launch OSU!BAND";
    bool showVersion=true,showPlan=true,showAnimation=true;
    float motion=1;
    static runtime_settings parse(const nlohmann::json& j){
        runtime_settings s;
        auto text=[&](const char* key,size_t max,bool empty=false){auto v=j.at(key).get<std::string>();
            if(v.size()>max||(!empty&&v.empty())||std::any_of(v.begin(),v.end(),[](unsigned char c){return c<32;}))throw std::runtime_error("Invalid loader text");return v;};
        s.title=text("title",160);s.announcement=text("announcement",640,true);
        s.connectLabel=text("connectLabel",160);s.launchLabel=text("launchLabel",160);
        s.accent=j.at("accent").get<std::string>();if(!std::regex_match(s.accent,std::regex("#[a-fA-F0-9]{6}")))throw std::runtime_error("Invalid loader color");
        s.logo=j.at("logo").get<std::string>();if(s.logo!="orbit"&&s.logo!="pulse")throw std::runtime_error("Invalid logo");
        s.showVersion=j.at("showVersion").get<bool>();s.showPlan=j.at("showPlan").get<bool>();s.showAnimation=j.at("showAnimation").get<bool>();
        s.motion=j.at("motion").get<float>();if(!std::isfinite(s.motion)||s.motion<.25f||s.motion>2.f)throw std::runtime_error("Invalid animation speed");
        return s;
    }
};
}
