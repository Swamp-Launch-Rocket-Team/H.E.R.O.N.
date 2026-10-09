#include <rasalhague_packet.h>
#include <rx_line.h>
#include <cstdio>
#include <random>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <cmath>
// Flight settings: change these to change the flight
constexpr double PAD_TIME_S    = 10.0;   // s on the pad before liftoff
constexpr double BOOST_ACCEL   = 60.0;   // m/s^2 while the motor burns
constexpr double BURN_TIME_S   = 1.5;    // s
constexpr double G             = 9.81;   // m/s^2, coast deceleration (no drag)
constexpr double DROGUE_RATE   = 20.0;   // m/s down under the drogue
constexpr double MAIN_ALT_M    = 150.0;  // m above the pad where the main opens
constexpr double MAIN_RATE     = 6.0;    // m/s down under the main
constexpr double PAD_MSL_M     = 50.0;   // m, pad altitude above sea level
constexpr double WIND_EAST_MS  = 4.0;    // m/s drift during descent
constexpr double PI            = 3.14159265358979;

constexpr double V_BURN = BOOST_ACCEL * BURN_TIME_S; //speed at burnout
constexpr double H_BURN = 0.5 * BURN_TIME_S * BURN_TIME_S * BOOST_ACCEL; //height at burnout
constexpr double T_APOGEE = BURN_TIME_S + V_BURN/G; // burn time plus the time for gravity to slow V_BURN to zero.
constexpr double H_APOGEE = H_BURN + (V_BURN * V_BURN)/(2*G); //
constexpr double T_MAIN = T_APOGEE + (H_APOGEE-MAIN_ALT_M)/DROGUE_RATE; //apogee time plus the drogue fall from H_APOGEE down to MAIN_ALT_M
constexpr double T_LAND = T_MAIN + (MAIN_ALT_M)/MAIN_RATE;

void flight_at(double t, double& h, double& v, uint8_t& state){
    if(t < 0){
        h = 0;
        v = 0;
        state = STATE_PAD;
    }
    else if( t < BURN_TIME_S){
        h = 0.5 * BOOST_ACCEL *t *t;
        v = BOOST_ACCEL * t;
        state = STATE_BOOST;
    }
    else if(t < T_APOGEE){
        double tau = t - BURN_TIME_S;
        v = V_BURN - G * tau;
        h = H_BURN + V_BURN * tau - 0.5 * G * tau * tau;
        state = STATE_COAST;
    }
    else if(t < T_MAIN){
        h = H_APOGEE - DROGUE_RATE * (t - T_APOGEE);
        v = -DROGUE_RATE;
        state = STATE_DESCENT;
    }
    else if(t < T_LAND){
        h = MAIN_ALT_M - MAIN_RATE * (t - T_MAIN);
        v = -MAIN_RATE;
        state = STATE_DESCENT;
    }
    else{
        h = 0;
        v = 0;
        state = STATE_LANDED;
    }
}

int main(){
    TelemetryPacket pkt{};
    std::strcpy(pkt.callsign, "N0CALL");
    pkt.gps_fix_type = 3;
    pkt.gps_num_sats = 11;
    pkt.gps_latitude_deg = 29.6516;
    pkt.gps_longitude_deg = -82.3248;
    std::fprintf(stderr,"V_Burn %.1f m/s, H_Burn %.1f m, T_Apogee %.2f s, H_Apogee %.1fm T_Main %.1fs T_Land %.1fs \n", V_BURN, H_BURN, T_APOGEE, H_APOGEE, T_MAIN, T_LAND);
    
    std::mt19937 rng(42);                                 // random number generator, seed 42
    std::normal_distribution<double> noise(0.0, 1.0);    // bell curve: mean 0, spread 1
    for(size_t i = 0; ; i++){
        double t = i * 0.2 - PAD_TIME_S;   // seconds since liftoff
        double h, v;
        flight_at(t, h, v, pkt.flight_state);
        pkt.baro_alt_m = 0.3 * noise(rng) + (PAD_MSL_M +h);
        pkt.baro_pressure_hpa = 1013.25 * pow((1 - pkt.baro_alt_m / 44330),5.255);
        pkt.baro_temp_c	= 0.05 * noise(rng)+(25 - 0.0065 * h);
        pkt.gps_alt_msl_m = PAD_MSL_M + h + 2*noise(rng);
        pkt.batt_voltage_v = 4.15 - 0.001*t +0.005*noise(rng);
        pkt.gps_vel_down_ms = -v;
        if(pkt.flight_state == STATE_PAD) pkt.accel_z_g = 1.0 + 0.02*noise(rng);
        if(pkt.flight_state == STATE_BOOST) pkt.accel_z_g = (BOOST_ACCEL +G)/G + 0.02*noise(rng);
        if(pkt.flight_state == STATE_COAST) pkt.accel_z_g = 0.0 + 0.02*noise(rng);
        if(pkt.flight_state == STATE_DESCENT){
            pkt.accel_z_g = 1.0 + 0.02*noise(rng);
            pkt.gps_longitude_deg += WIND_EAST_MS * 0.2 / (111320.0 * std::cos(pkt.gps_latitude_deg * PI / 180.0));
            pkt.gps_vel_east_ms = WIND_EAST_MS;
            pkt.gps_ground_speed_ms = WIND_EAST_MS;
            pkt.gps_heading_deg = 90;
        }

        if(pkt.flight_state == STATE_LANDED){
            pkt.accel_z_g = 1.0 + 0.02*noise(rng);
            pkt.gps_vel_east_ms = 0;
            pkt.gps_ground_speed_ms = 0;
        }
        if(t > T_LAND + 10.0) {
            break;
        }
        pkt.timestamp_ms = i * 200;
        

        std::string hex = bytes_to_hex(reinterpret_cast<const uint8_t*>(&pkt), sizeof pkt);
        std::printf("{\"type\":\"rx\",\"rssi\":%.1f,\"snr\":%.1f,\"hex\":\"%s\"}\n", -87.5, 9.5, hex.c_str());
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        
    }
    return 0;
}