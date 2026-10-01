#pragma once
#include <cstdint>
#include <cstddef>

constexpr uint8_t TLM_VERSION = 1;
constexpr size_t  TLM_PACKET_SIZE = 52;

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
