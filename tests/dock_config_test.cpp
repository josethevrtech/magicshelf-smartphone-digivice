#include "object_dock.h"
#include <cassert>
int main(){
 auto dir=std::filesystem::temp_directory_path()/("power-toys-config-"+std::to_string(getpid()));std::filesystem::create_directories(dir);int version=0;
 auto write=[&](std::string text){std::ofstream(dir/"object-dock.json")<<text;std::filesystem::last_write_time(dir/"object-dock.json",std::filesystem::file_time_type::clock::now()+std::chrono::seconds(++version));};
 write(R"({"slot_spacing_m":0.13,"rail_height_m":0.0678,"objects":["local.androidphone.device","local.digitalpartner.device"]})");
 ObjectDock dock(nullptr,dir.string());assert(dock.slot==(AndroidOnly?0:1));assert(dock.layout.count==2);
 auto reload=[&](std::string text){write(text);dock.nextConfig={};dock.reload();};
 reload(R"({"slot_spacing_m":0.16,"objects":["future.toy","local.androidphone.device","local.digitalpartner.device"]})");assert(dock.slot==(AndroidOnly?1:2));assert(dock.layout.count==3&&fabsf(dock.layout.spacing-.16f)<.00001f);
 reload(R"({"slot_spacing_m":0.1,"objects":["local.androidphone.device"]})");assert(dock.layout.count==3);
 reload(R"({"objects":["local.androidphone.device","local.androidphone.device"]})");assert(dock.layout.count==3);
 reload(R"({"objects":[7]})");assert(dock.layout.count==3);
 reload("{broken");assert(dock.layout.count==3);
 reload(R"({"style":"shelf","launcher_only":true,"opacity":0.72,"depth_m":0.459,"join_yaw_degrees":56,"lean_degrees":-6,"objects":["local.androidphone.device","local.digitalpartner.device"]})");assert(dock.layout.shelf&&dock.layout.launcherOnly&&dock.layout.count==2&&fabsf(dock.layout.opacity-.72f)<.00001f);
 for(auto bad:{R"({"style":"unknown","objects":[]})",R"({"launcher_only":"true","objects":[]})",R"({"opacity":1.1,"objects":[]})",R"({"style":"shelf","curve_radius_m":1,"objects":[]})"}){reload(bad);assert(dock.layout.shelf&&dock.layout.count==2);}
 reload(R"({"schema":4,"objects":[]})");assert(dock.layout.shelf&&dock.layout.count==2);
 reload(R"({"objects":[]})");assert(dock.layout.count==0&&dock.slot==-1);
 std::filesystem::remove_all(dir);std::cout<<AppTitle<<": config order/reload/invalid-input/empty-module checks passed\n";
}
