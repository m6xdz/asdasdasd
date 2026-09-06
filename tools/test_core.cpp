#include <impl/cloud/session_lease.hxx>
#include <impl/config/config_store.hxx>
#include <cassert>
#include <iostream>
int main(){
 cloud::session_lease s;
 assert(!s.valid(0));s.accept(1000,500000);assert(s.valid(120999));assert(!s.valid(121000));
 s.accept(1000,500000,505000);assert(s.valid(5999));assert(!s.valid(6000));
 s.accept(1000,500000,499999);assert(!s.valid(1000));
 s.accept(1000,500000);s.deny();assert(!s.valid(1001));
 config::settings_t a;a.relax_enabled=true;a.relax_ur=48;a.aim_strength_x=3;
 std::ostringstream out;config::serialize_settings(out,"test",a);
 config::settings_t b;std::istringstream in(out.str());assert(config::parse_settings(in,b));config::validate(b);
 assert(b.relax_enabled&&b.relax_ur==48&&b.aim_strength_x==3);
 std::cout<<"PASS: session lease, explicit denial, expiry, config roundtrip\n";
}
