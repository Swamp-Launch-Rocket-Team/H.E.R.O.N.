#include <rasalhague_packet.h>
#include <rx_line.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>




int main(){
    TelemetryPacket pkt{};
    std::strcpy(pkt.callsign, "N0CALL");
    pkt.gps_fix_type = 3;
    pkt.gps_num_sats = 11;
    pkt.gps_latitude_deg = 29.6516;
    pkt.gps_longitude_deg = -82.3248;

    for(size_t i = 0; i < 300; i++){

        if(i < 25) pkt.flight_state = STATE_PAD;
        else if(i < 75) {
            pkt.flight_state = STATE_BOOST;
        }
        else if(i < 125) pkt.flight_state = STATE_COAST;
        else if(i < 300) pkt.flight_state = STATE_DESCENT;
        pkt.timestamp_ms = i * 200;

        std::string hex = bytes_to_hex(reinterpret_cast<const uint8_t*>(&pkt), sizeof pkt);
        std::printf("{\"type\":\"rx\",\"rssi\":%.1f,\"snr\":%.1f,\"hex\":\"%s\"}\n", -87.5, 9.5, hex.c_str());
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        
    }
    return 0;
}