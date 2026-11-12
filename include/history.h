#pragma once
#include <vector>
#include <rasalhague_packet.h>
#include <flight_stats.h>
#include <cmath>

struct History {
    std::vector<double> t_s;        // rocket time, timestamp_ms / 1000
    std::vector<double> agl_m;      // NAN until the pad reference exists
    std::vector<double> vs_baro;    // stats.vspeed_ms
    std::vector<double> vs_gps;     // -pkt.gps_vel_down_ms
    std::vector<double> accel_g;    // stats.accel_total_g
    void add(const TelemetryPacket& p, const FlightStats& s);
    void clear();
};

inline void History::add(const TelemetryPacket& p, const FlightStats& s){
   t_s.push_back(p.timestamp_ms/1000.0);
   if(s.have_pad) agl_m.push_back(s.agl_m);
   else{agl_m.push_back(NAN);}
   vs_baro.push_back(s.vspeed_ms);
   vs_gps.push_back(-p.gps_vel_down_ms);
   accel_g.push_back(s.accel_total_g);
}
inline void History::clear(){
    t_s.clear();
    agl_m.clear();
    vs_baro.clear();
    vs_gps.clear();
    accel_g.clear();
}