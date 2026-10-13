#include "line_reader.h"
#include "session_log.h"
#include "rx_line.h"
#include "rasalhague_packet.h"
#include <cstring>
#include <string>
#include <cstdio>
#include <windows.h>

// Opens and sets up a COM port. Returns INVALID_HANDLE_VALUE if it isn't there (yet).
HANDLE open_port(const std::string& name){
    std::string path = "\\\\.\\" + name;          // \\.\COM4 also works for COM10 and up
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if(h == INVALID_HANDLE_VALUE) return h;

    DCB dcb{};
    dcb.DCBlength = sizeof dcb;
    GetCommState(h, &dcb);
    dcb.BaudRate = CBR_115200;              // ignored over native USB, but never 1200
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;   // "a terminal is connected"; the board's USB waits for it
    dcb.fRtsControl = RTS_CONTROL_ENABLE;   // same as PlatformIO's monitor, which worked
    SetCommState(h, &dcb);

    COMMTIMEOUTS to{};
    to.ReadIntervalTimeout = 50;            // return 50 ms after the last byte...
    to.ReadTotalTimeoutConstant = 250;      // ...or after 250 ms with nothing
    SetCommTimeouts(h, &to);
    return h;
}



int main(int argc, char** argv){
    SessionLog log;
    LineReader reader;
    RxLine rx;
    TelemetryPacket pkt;
    char chunk[1024];
    std::string line;
    size_t goodPackets = 0;
    size_t skippedLines = 0;
    size_t wrongSizePkt = 0;
    
    if(!log.open()){
        std::fprintf(stderr, "Could not open the log files in logs/\n");
        return 1;
    }
    auto handle_line = [&](const std::string& line){
        log.line(line);
        if(!parse_rx_line(line, rx)){
            skippedLines++;
            return;
        }
        size_t n = rx.hex.size() / 2;
        uint8_t bytes[255];
        if(!hex_to_bytes(rx.hex, bytes, n)){
            skippedLines++;
            return;
        } 
        if(!decodeTelemetryPacket(bytes, n, pkt)){
            wrongSizePkt++;
            return;
        } 
        else goodPackets++;
        std::printf("[%7.1fs] %-7s alt %7.1f m  %5.2f V  fix %d, %2d sats  %.6f, %.6f  RSSI %.1f\n",
        pkt.timestamp_ms / 1000.0, flightStateToString(pkt.flight_state), pkt.baro_alt_m,
        pkt.batt_voltage_v, pkt.gps_fix_type, pkt.gps_num_sats,
        pkt.gps_latitude_deg, pkt.gps_longitude_deg, rx.rssi);
        log.packet(pkt,rx);
    };
    
    if(argc > 1){
        std::string port = argv[1];
        HANDLE h = INVALID_HANDLE_VALUE;      // INVALID = "port not open"
        while(true){
            if(h == INVALID_HANDLE_VALUE){ // no port: try to open
                h = open_port(port);
                if(h == INVALID_HANDLE_VALUE){Sleep(250); continue;}
                log.line("# " + port + " opened");
            }
            char buf[1024];
            DWORD got = 0;   
            if(!ReadFile(h, buf, sizeof buf, &got, nullptr)){
                CloseHandle(h);
                h = INVALID_HANDLE_VALUE;
                log.line("# " + port + " lost");
                reader = LineReader();
                continue;
            }
            reader.feed(buf, got);
            while(reader.next_line(line)) handle_line(line);
        }
    }
        
    while(std::fgets(chunk, sizeof chunk, stdin)){
        reader.feed(chunk, std::strlen(chunk));
        while(reader.next_line(line)) handle_line(line);
    }
    std::printf("good %zu, skipped %zu, wrong size %zu, dropped %zu\n",
            goodPackets, skippedLines, wrongSizePkt, reader.dropped_Count());
    return 0;
}
