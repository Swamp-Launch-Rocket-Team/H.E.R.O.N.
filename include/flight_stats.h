#pragma once
#include "rasalhague_packet.h"
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
// Minimum satellites for a 3D fix to be used on the map. In a bench log, 4-5 satellite fixes
// (right after the GPS reacquired) were typically 50-90 m off and up to 448 m; 6+ were within ~17 m.
constexpr int GPS_MIN_SATS = 6;
inline bool gps_fix_usable(int fix_type, int sats){ return fix_type >= 3 && sats >= GPS_MIN_SATS; }
struct FlightStats{
    bool have_pad = false; //is the pad refrence known?
    bool have_pad_pos = false;
    double pad_lat = 0;
    double pad_lon = 0;
    double pad_alt_m = 0; //baro altitude of the pad (m above sea level)
    double agl_m = 0; //height of the pad
    double peak_agl_m = 0;
    bool launched = false;
    long long liftoff_ms = 0;     // continuous FC time at liftoff (was uint32_t: wrapped after a reboot)
    long long fc_offset_ms = 0;   // added to timestamp_ms so time keeps going after a reboot
    long long t_ms = 0;           // continuous flight-computer time; use this, not timestamp_ms
    uint32_t  last_fc_ms = 0;
    long long last_pc_ms = 0;
    bool      have_fc = false;
    int       reboots = 0;
    double t_plus_s = 0; //time since liftoff
    double vspeed_ms = 0; // m/s, up is positive
    double accel_total_g = 0;
    std::deque<std::pair<double,double>> recent; // (rocket time s, baro alt m)
    std::vector<double> track_e;
    std::vector<double> track_n;
    // Fixed origin of the ground-track plot: the first pad fix, never moved afterwards.
    // (Converting against pad_lat/pad_lon, which keeps being smoothed while on the pad, made the
    // map, the receiver and old track points all slide around as that estimate wandered.)
    bool have_origin = false;
    double origin_lat = 0;
    double origin_lon = 0;
};
// Metres east/north of the plot's fixed origin.
inline void latlon_to_en(const FlightStats& s, double lat, double lon, double& east_m, double& north_m){
    north_m = (lat - s.origin_lat) * 111320.0;
    east_m  = (lon - s.origin_lon) * 111320.0  * std::cos(deg_to_rad(s.origin_lat));
}
// Smoothed position of something standing still (rocket on the pad or landed, the receiver).
// Time-based, so it behaves the same whatever the packet rate. A GPS that isn't moving still
// wanders a few metres (up to ~17 m on the bench); this keeps the marker from jumping around.
// A fix farther than JUMP_M from the average is treated as a real move: the average restarts
// there instead of crawling over (bench GPS noise stayed within ~17 m of its mean).
struct SmoothedPos{
    static constexpr double JUMP_M = 30.0;
    bool have = false;
    double lat = 0, lon = 0;
    long long last_ms = 0;
    void add(double new_lat, double new_lon, long long ms, double tau_s){
        if(have){
            double dn = (new_lat - lat) * 111320.0;
            double de = (new_lon - lon) * 111320.0 * std::cos(deg_to_rad(lat));
            if(std::sqrt(dn*dn + de*de) > JUMP_M) have = false;
        }
        if(!have){ lat = new_lat; lon = new_lon; last_ms = ms; have = true; return; }
        double dt_s = std::max(0.0, (ms - last_ms) / 1000.0);
        double a = 1.0 - std::exp(-dt_s / tau_s);
        lat += a * (new_lat - lat);
        lon += a * (new_lon - lon);
        last_ms = ms;
    }
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
inline void update_stats(FlightStats& s, const TelemetryPacket& p, long long pc_ms){
    if (s.have_fc && p.timestamp_ms + 1000 < s.last_fc_ms) {        // clock went backwards = reboot
        s.fc_offset_ms = (s.last_fc_ms + s.fc_offset_ms) + (pc_ms - s.last_pc_ms) - p.timestamp_ms;
        s.reboots++;
        s.recent.clear();
    }
    s.t_ms = p.timestamp_ms + s.fc_offset_ms;
    s.last_fc_ms = p.timestamp_ms;  s.last_pc_ms = pc_ms;  s.have_fc = true;
    if(p.flight_state == STATE_PAD && !s.launched){
        if(s.have_pad == false) {s.pad_alt_m = p.baro_alt_m; s.have_pad = true;}
        s.pad_alt_m += PAD_SMOOTHING *(p.baro_alt_m - s.pad_alt_m);
        if(gps_fix_usable(p.gps_fix_type, p.gps_num_sats)){
            if(!s.have_pad_pos) {s.pad_lat = p.gps_latitude_deg; s.pad_lon = p.gps_longitude_deg; s.have_pad_pos = true;}
            if(!s.have_origin)  {s.origin_lat = p.gps_latitude_deg; s.origin_lon = p.gps_longitude_deg; s.have_origin = true;}
            s.pad_lat += PAD_SMOOTHING * (p.gps_latitude_deg - s.pad_lat);
            s.pad_lon += PAD_SMOOTHING * (p.gps_longitude_deg - s.pad_lon);
        }
    }
    s.agl_m = p.baro_alt_m - s.pad_alt_m;
    if(p.flight_state != STATE_PAD && !s.launched){ s.launched = true; s.liftoff_ms = s.t_ms; }
    if(s.launched){
        s.t_plus_s = (s.t_ms - s.liftoff_ms) / 1000.0;
        s.peak_agl_m = std::max(s.agl_m, s.peak_agl_m);
    }
    s.recent.push_back({s.t_ms/1000.0,p.baro_alt_m});
    if(s.recent.size() > VSPEED_WINDOW) s.recent.pop_front();
    s.vspeed_ms = slope_of(s.recent);
    s.accel_total_g = std::sqrt(p.accel_x_g*p.accel_x_g + p.accel_y_g*p.accel_y_g + p.accel_z_g*p.accel_z_g);
}