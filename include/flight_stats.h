#pragma once
#include <rasalhague_packet.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <utility>

constexpr double PAD_SMOOTHING = 0.05;   // how far each pad reading pulls the reference
constexpr size_t VSPEED_WINDOW = 5;      // packets used for vertical speed
struct FlightStats{
    bool have_pad = false; //is the pad refrence known?
    double pad_alt_m = 0; //baro altitude of the pad (m above sea level)
    double agl_m = 0; //height of the pad
    double peak_agl_m = 0;
    bool launched = false;
    uint32_t liftoff_ms = 0; // rocket clock at the first packet after PAD
    double t_plus_s = 0; //time since liftoff
    double vspeed_ms = 0; // m/s, up is positive
    double accel_total_g = 0;
    std::deque<std::pair<double,double>> recent; // (rocket time s, baro alt m)
};

// Slope (m/s) of the best-fit line through the (time, altitude) points.
inline double slope_of(const std::deque<std::pair<double,double>>& pts){
    int size = pts.size();
    if(size < 2) return 0.0;
    double t_bar = 0; //average time
    double h_bar = 0; //average altitude
    for(int i = 0; i < size; i++){
        t_bar += pts[i].first;
        h_bar += pts[i].second;
    }
    t_bar /= (double)size;
    h_bar /= (double)size;
    double num = 0;
    double denom = 0;
    for(int i = 0; i < size; i++){
        num += (pts[i].first - t_bar)*(pts[i].second - h_bar);
        denom += (pts[i].first - t_bar)*(pts[i].first - t_bar);
    }
    if(denom == 0) return 0;
    return num/denom;
}

// Call once for every NEW packet, never once per frame.
inline void update_stats(FlightStats& s, const TelemetryPacket& p){
    if(p.flight_state == STATE_PAD){
        if(s.have_pad == false) {s.pad_alt_m = p.baro_alt_m; s.have_pad = true;}
        s.pad_alt_m += PAD_SMOOTHING *(p.baro_alt_m - s.pad_alt_m);
    }
    s.agl_m = p.baro_alt_m - s.pad_alt_m;
    if(p.flight_state != STATE_PAD){
        if(!s.launched){ s.launched = true; s.liftoff_ms = p.timestamp_ms; }
        s.t_plus_s = (p.timestamp_ms -s.liftoff_ms) / 1000.0;
        s.peak_agl_m = std::max(s.agl_m,s.peak_agl_m);
    }
    s.recent.push_back({p.timestamp_ms/1000.0,p.baro_alt_m});
    if(s.recent.size() > VSPEED_WINDOW) s.recent.pop_front();
    s.vspeed_ms = slope_of(s.recent);
    s.accel_total_g = std::sqrt(p.accel_x_g*p.accel_x_g + p.accel_y_g*p.accel_y_g + p.accel_z_g*p.accel_z_g);
}