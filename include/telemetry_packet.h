#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

constexpr uint8_t TLM_VERSION = 1;
constexpr size_t  TLM_PACKET_SIZE = 52;

// Scale factors: count = value * scale (encode), value = count / scale (decode)
constexpr double TLM_ACCEL_SCALE   = 100.0;       // counts per g
constexpr double TLM_GYRO_SCALE    = 10.0;        // counts per deg/s
constexpr double TLM_TEMP_SCALE    = 100.0;       // counts per deg C
constexpr double TLM_HEIGHT_SCALE  = 100.0;       // counts per m (stored as cm)
constexpr double TLM_VSPEED_SCALE  = 10.0;        // counts per m/s
constexpr double TLM_LATLON_SCALE  = 10000000.0;  // counts per degree
constexpr double TLM_GPS_ALT_SCALE = 1000.0;      // counts per m (stored as mm)
constexpr double TLM_GPS_AGE_SCALE = 10.0;        // counts per s
constexpr double TLM_BATT_SCALE    = 1000.0;      // counts per V (stored as mV)
constexpr double TLM_TIME_SCALE    = 1000.0;      // counts per s (stored as ms)
// pressure is already in Pa, so it has no scale factor
enum class FlightStage : uint8_t{
    Pad = 0,
    Boost = 1,
    Coast = 2,
    Apogee = 3,
    Drogue = 4,
    Main = 5,
    Landed = 6
};
constexpr uint8_t TLM_FLAG_GPS_FIX = 1u << 0;  // GPS has a 3D fix
constexpr uint8_t TLM_FLAG_ARMED = 1u << 2;
#pragma pack(push,1)
struct TelemetryPacketV1{
    uint8_t version; //Packet Version
    uint16_t seq; //sequence of packets, +1 for each packet
    uint32_t time_ms; //ms since boot
    uint8_t stage; //flight stage number
    uint8_t flags; //8 on/off bits
    int16_t accel_x, accel_y, accel_z; //g × 100
    int16_t gyro_x, gyro_y, gyro_z; // °/s × 10
    uint32_t pressure; //Pa
    int16_t temperature; //	°C × 100
    int32_t height; //cm above ground
    int16_t vspeed; // m/s × 10
    int32_t lat, lon; //latitude longitude degrees × 10,000,000
    int32_t gps_alt; //	mm above sea level
    uint8_t sat_count; //satellite count
    uint8_t fix_type; //GPS fix code 0–5
    uint8_t gps_age; //seconds × 10
    uint16_t battery; //mV
    uint16_t crc; //checksum
};
static_assert(sizeof(TelemetryPacketV1) == TLM_PACKET_SIZE, "packet size changed");
#pragma pack(pop)
//Cyclic redundancy check; a 16-bit fingerprint of the packet's data
inline uint16_t tlm_crc16(const uint8_t* data, size_t len){
    uint16_t crc = 0xFFFF;
   for(size_t i = 0; i < len; i++){
    crc ^= data[i]<< 8;
    for(size_t j = 0; j < 8; j++){
        if(crc & 0x8000){
            crc = crc<<1;
            crc ^= 0x1021;
        }
        else crc = crc<<1;
    }
   }
   return crc;
};
inline void tlm_finalize(TelemetryPacketV1& pkt){
    pkt.version = TLM_VERSION;
    pkt.crc = tlm_crc16(reinterpret_cast<const uint8_t*>(&pkt),offsetof(TelemetryPacketV1, crc));
};
enum class TlmResult : uint8_t{
        OK = 0,
        BadLength = 1,
        BadVersion = 2,
        BadCRC = 3
};
inline TlmResult tlm_check(const uint8_t* buf, size_t len, TelemetryPacketV1& out){
    if(len != TLM_PACKET_SIZE) return TlmResult::BadLength;
    if(buf[0] != TLM_VERSION) return TlmResult::BadVersion;
    std::memcpy(&out, buf, TLM_PACKET_SIZE);
    uint16_t crc = tlm_crc16(buf, offsetof(TelemetryPacketV1,crc));
    if(crc != out.crc) return TlmResult::BadCRC;
    return TlmResult::OK;
}