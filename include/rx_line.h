#pragma once
#include <string>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cctype>

// One line from the ground receiver, for example:
//   {"type":"rx","rssi":-87.5,"snr":9.5,"hex":"b94a06...0000"}
// The receiver writes compact JSON with no spaces (as ArduinoJson does), which is
// what lets parse_rx_line() find each key with a plain text search.

struct RxLine{
    float rssi;        // signal strength in dBm, e.g. -87.5 (more negative = weaker)
    float snr;         // signal-to-noise ratio in dB, e.g. 9.5 (how far the signal is above the noise)
    std::string hex;   // the packet itself, 2 hex characters per byte (214 for Alex's 107-byte packet)
}; 
// Pulls rssi, snr and hex out of one receiver line.
// Returns false for anything that isn't a well-formed "rx" line (status lines,
// ESP32 boot messages, garbage). On false, out may be partly filled in,
// so only use it when this returns true.

inline bool parse_rx_line(const std::string& line, RxLine& out){
    // rssi: a decimal number in dBm (the radio reports half-dB steps, e.g. -87.5)
    const std::string rssi_key = R"("rssi":)";            // the text to look for: "rssi":
    size_t pos = line.find(rssi_key);
    if (pos == std::string::npos) return false;      // key missing -> not a valid rx line
    const char* start = line.c_str() + pos + rssi_key.size();  // points just past the colon
    char* end = nullptr;
    float value = std::strtod(start, &end);          // read a decimal number from there
    if (end == start) return false;                  // strtod read nothing → no number
    out.rssi = value;

    // Only "rx" lines carry packets; status lines and boot messages stop here.
    if(line.find(R"("type":"rx")") == std::string::npos) return false;
    // snr: a decimal number in dB
    const std::string snr_key = R"("snr":)";                              // the text to look for: "snr":
    pos = line.find(snr_key);
    if (pos == std::string::npos) return false;      // key missing -> not a valid rx line
    start = line.c_str() + pos + snr_key.size();         // points just past the colon
    end = nullptr;
    float val = std::strtod(start, &end);            // read a decimal number, e.g. 9.5
    if (end == start) return false;                  // strtod read nothing → no number
    out.snr = val;
    
    // hex: the raw packet, 2 hex characters per byte, between quotes
    const std::string hex_key = R"("hex":")";
    pos = line.find(hex_key);
    if (pos == std::string::npos) return false;      // key missing -> not a valid rx line
    size_t hex_start = pos + hex_key.size();   // index just past "hex":"
    size_t hex_end = line.find('"', hex_start);   // the closing quote
    if(hex_end == std::string::npos) return false;
    out.hex = line.substr(hex_start, hex_end-hex_start);
    // Any whole number of bytes up to 255 (the LoRa maximum). The decoder checks
    // the exact size, so this parser doesn't need to know which packet it carries.
    if(out.hex.empty() || out.hex.size() % 2 != 0 || out.hex.size() > 510) return false;
    // Every character must be 0-9, a-f or A-F. The unsigned char cast keeps
    // isxdigit safe for bytes above 127.
    for(char c : out.hex){
        if(!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}   
inline int hex_value(char c){  
    if('0' <= c && c <= '9') return c - '0';
    if('A' <= c && c <= 'F') return c - 'A' + 10;
    if('a' <= c && c <= 'f') return c - 'a' + 10;
    return -1;
}

inline bool hex_to_bytes(const std::string& hex, uint8_t* out, size_t n){
    if(hex.size() != 2*n) return false;
    for(size_t i = 0; i < n; i++){
        int hi = hex_value(hex[2*i]);
        int lo = hex_value(hex[2*i +1]);
        if(hi == -1 || lo == -1) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;

}
inline std::string bytes_to_hex(const uint8_t* data, size_t n){
    const char* digits = "0123456789abcdef";
    std::string hex_string = "";
    for(size_t i = 0; i < n; i++){
        uint8_t byte = data[i];
        uint8_t hi = byte / 16;
        uint8_t lo = byte % 16;
        hex_string.push_back(digits[hi]);
        hex_string.push_back(digits[lo]);
    }
    return hex_string;
}