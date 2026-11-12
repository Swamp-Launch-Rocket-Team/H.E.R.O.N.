#pragma once
#include <rasalhague_packet.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>
#include <utility>

constexpr double PAD_SMOOTHING = 0.05;   // how far each pad reading pulls the reference
constexpr double PI_D = 3.14159265358979323846;   // not PI: raylib #defines PI as a float
inline double deg_to_rad(double deg){ return deg * PI_D / 180.0; }
constexpr size_t VSPEED_WINDOW = 5;      // packets used for vertical speed
struct FlightStats{
    bool have_pad = false; //is the pad refrence known?
    bool have_pad_pos = false;
    double pad_lat = 0;
    double pad_lon = 0;
    double pad_alt_m = 0; //baro altitude of the pad (m above sea level)
    double agl_m = 0; //height of the pad
    double peak_agl_m = 0;
    bool launched = false;
    uint32_t liftoff_ms = 0; // rocket clock at the first packet after PAD
    double t_plus_s = 0; //time since liftoff
    double vspeed_ms = 0; // m/s, up is positive
    double accel_total_g = 0;
    std::deque<std::pair<double,double>> recent; // (rocket time s, baro alt m)
    std::vector<double> track_e;
    std::vector<double> track_n;
};
inline void latlon_to_en(const FlightStats& s, double lat, double lon, double& east_m, double& north_m){
    north_m = (lat - s.pad_lat) * 111320.0;
    east_m  = (lon - s.pad_lon) * 111320.0  * std::cos(deg_to_rad(s.pad_lat));
}
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
inline double expected_interval_s(uint8_t state){
    switch(state){
        case STATE_PAD: return 0.185;
        case STATE_BOOST:
        case STATE_COAST: return 0.328;
        case STATE_DESCENT: return 2.30;
        case STATE_LANDED: return 2.30;
        default: return 2.30;
    }
}
// Call once for every NEW packet, never once per frame.
inline void     update_stats(FlightStats& s, const TelemetryPacket& p){
    if(p.flight_state == STATE_PAD && !s.launched){
        if(s.have_pad == false) {s.pad_alt_m = p.baro_alt_m; s.have_pad = true;}
        s.pad_alt_m += PAD_SMOOTHING *(p.baro_alt_m - s.pad_alt_m);
        if(p.gps_fix_type >= 3){
            if(!s.have_pad_pos) {s.pad_lat = p.gps_latitude_deg; s.pad_lon = p.gps_longitude_deg; s.have_pad_pos = true;}
            s.pad_lat += PAD_SMOOTHING * (p.gps_latitude_deg - s.pad_lat);
            s.pad_lon += PAD_SMOOTHING * (p.gps_longitude_deg - s.pad_lon);
        }
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