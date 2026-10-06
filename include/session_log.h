#pragma once
#include <filesystem>
#include <rasalhague_packet.h>
#include <rx_line.h>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <string>
#include <cmath>


class SessionLog{
    std::FILE* raw = nullptr; //every line exactly as recieved
    std::FILE* csv = nullptr; //one row per line
    public:
        bool open(); // make logs/, open both files, write the CSV header
        void line(const std::string& text); //Raw log: PC time + the line, good or bad
        void packet(const TelemetryPacket &p, const RxLine& rx); //One CSV row
        ~SessionLog();          //Closes both files
        static long long now_ms();
};
inline long long SessionLog::now_ms(){
    return std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count();
}
inline bool SessionLog::open(){
    std::error_code ec;
    std::filesystem::create_directories("logs", ec);   // error_code version: reports failure instead of throwing
    if(ec) return false;
    std::time_t now = std::time(nullptr);
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y%m%d_%H%M%S", std::localtime(&now));
    std::string base = std::string("logs/") + stamp;
    raw = std::fopen((base + "_raw.log").c_str(), "w");
    csv = std::fopen((base + "_decoded.csv").c_str(), "w");
    if(!raw || !csv) return false;
    std::fprintf(csv, "pc_time_ms,time_s,flight_state,state_name,batt_v,baro_pressure_hpa,baro_alt_m,baro_temp_c,"
    "accel_x_g,accel_y_g,accel_z_g,accel_mag_g,gyro_x_dps,gyro_y_dps,gyro_z_dps,"
    "mag_x_gauss,mag_y_gauss,mag_z_gauss,gps_fix,gps_sats,gps_lat,gps_lon,gps_alt_m,"
    "gps_speed_ms,gps_vel_north_ms,gps_vel_east_ms,gps_vel_down_ms,gps_heading_deg,"
    "callsign,rssi_dbm,snr_db\n");
    std::fflush(csv);
    return true;
}
inline void SessionLog::line(const std::string& text){
    std::fprintf(raw, "%lld\t%s\n", now_ms(), text.c_str());
    std::fflush(raw);
}
inline void SessionLog::packet(const TelemetryPacket &p, const RxLine& rx){
    std::fprintf(csv, "%lld,%.3f,%d,%s,", now_ms(), p.timestamp_ms / 1000.0,
                p.flight_state, flightStateToString(p.flight_state));
    std::fprintf(csv, "%.3f,%.3f,%.3f,%.3f,", p.batt_voltage_v, p.baro_pressure_hpa,
                p.baro_alt_m, p.baro_temp_c);
    float accel_mag = std::sqrt(p.accel_x_g * p.accel_x_g + p.accel_y_g * p.accel_y_g + p.accel_z_g * p.accel_z_g);
    std::fprintf(csv, "%.3f,%.3f,%.3f,%.3f,", p.accel_x_g, p.accel_y_g, p.accel_z_g, accel_mag);
    std::fprintf(csv, "%.3f,%.3f,%.3f,", p.gyro_x_dps, p.gyro_y_dps, p.gyro_z_dps);
    std::fprintf(csv, "%.3f,%.3f,%.3f,", p.mag_x_gauss, p.mag_y_gauss, p.mag_z_gauss);
    std::fprintf(csv, "%d,%d,%.7f,%.7f,%.3f,", p.gps_fix_type, p.gps_num_sats,
                p.gps_latitude_deg, p.gps_longitude_deg, p.gps_alt_msl_m);
    std::fprintf(csv, "%.3f,%.3f,%.3f,%.3f,%.3f,", p.gps_ground_speed_ms, p.gps_vel_north_ms,
                p.gps_vel_east_ms, p.gps_vel_down_ms, p.gps_heading_deg);
    std::fprintf(csv, "%s,%.1f,%.1f\n", p.callsign, rx.rssi, rx.snr);   // last group: no trailing comma, then \n
    std::fflush(csv);
  

}
inline SessionLog::~SessionLog(){
    if(raw) std::fclose(raw);
    if(csv) std::fclose(csv);
}