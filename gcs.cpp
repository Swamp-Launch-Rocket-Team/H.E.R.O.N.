#include "line_reader.h"
#include "rx_line.h"
#include "rasalhague_packet.h"
#include <cstring>
#include <string>
#include <cstdio>

int main(){
    LineReader reader;
    RxLine rx;
    TelemetryPacket pkt;
    char chunk[1024];
    std::string line;
    size_t goodPackets = 0;
    size_t skippedLines = 0;
    size_t wrongSizePkt = 0;
    while(std::fgets(chunk, sizeof chunk, stdin)){
        reader.feed(chunk, std::strlen(chunk));
        while(reader.next_line(line)){
            if(!parse_rx_line(line, rx)){
                skippedLines++;
                continue;
            }
            size_t n = rx.hex.size() / 2;
            uint8_t bytes[255];
            if(!hex_to_bytes(rx.hex, bytes, n)){
                skippedLines++;
                continue;
            } 
            if(!decodeTelemetryPacket(bytes, n, pkt)){
                wrongSizePkt++;
                continue;
            } 
            else goodPackets++;
            std::printf("[%7.1f s] %-7s alt %7.1f m  %5.2f V  fix %d, %2d sats  %.6f, %.6f  RSSI %.1f\n",
            pkt.timestamp_ms / 1000.0, flightStateToString(pkt.flight_state), pkt.baro_alt_m,
            pkt.batt_voltage_v, pkt.gps_fix_type, pkt.gps_num_sats,
            pkt.gps_latitude_deg, pkt.gps_longitude_deg, rx.rssi);
        }

    }
    std::printf("good %zu, skipped %zu, wrong size %zu, dropped %zu\n",
            goodPackets, skippedLines, wrongSizePkt, reader.dropped_Count());
    return 0;
}
