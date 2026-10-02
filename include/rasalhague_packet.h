#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

// ============================================================================
// RASALHAGUE TELEMETRY SYSTEM - STANDARD PACKET & DECODER
// ============================================================================
// Both the Rocket Transmitter and Ground Station Receiver use this exact header.
// All values are 1-byte packed in fixed, identical byte positions.
//
// swamp-gcs copy: trimmed from Alex's Code/Rasalhague_Receiver/src/telemetry_format.h
// (Arduino.h and the two print functions removed so it builds on a PC). The struct must
// stay byte-for-byte identical to Alex's; the static_assert below catches size changes.

enum FlightState : uint8_t {
    STATE_PAD = 0,
    STATE_BOOST,
    STATE_COAST,
    STATE_DESCENT,
    STATE_LANDED
};

#pragma pack(push, 1)
struct TelemetryPacket {
    // --- System & Status ---
    uint32_t timestamp_ms;         // Milliseconds since transmitter boot (4 bytes)
    uint8_t  flight_state;         // Current flight state enum (1 byte)
    float    batt_voltage_v;       // Rocket battery voltage in Volts (4 bytes)

    // --- Barometer (BMP581) ---
    float    baro_pressure_hpa;    // Atmospheric pressure in hPa (4 bytes)
    float    baro_alt_m;           // Barometric altitude in meters MSL (4 bytes)
    float    baro_temp_c;          // Rocket internal temperature in Celsius (4 bytes)

    // --- IMU: Accelerometer & Gyroscope (LSM6DSOTR) ---
    float    accel_x_g;            // Acceleration X-axis in Gs (4 bytes)
    float    accel_y_g;            // Acceleration Y-axis in Gs (4 bytes)
    float    accel_z_g;            // Acceleration Z-axis in Gs (4 bytes)
    float    gyro_x_dps;           // Angular rate X-axis in deg/sec (4 bytes)
    float    gyro_y_dps;           // Angular rate Y-axis in deg/sec (4 bytes)
    float    gyro_z_dps;           // Angular rate Z-axis in deg/sec (4 bytes)

    // --- Magnetometer (MMC5983MA) ---
    float    mag_x_gauss;          // Magnetic field X-axis in Gauss (4 bytes)
    float    mag_y_gauss;          // Magnetic field Y-axis in Gauss (4 bytes)
    float    mag_z_gauss;          // Magnetic field Z-axis in Gauss (4 bytes)

    // --- GNSS / GPS (u-blox SAM-M10Q-00B via UBX Binary) ---
    uint8_t  gps_fix_type;         // Fix type: 0=None, 2=2D, 3=3D, 4=GNSS+DR (1 byte)
    uint8_t  gps_num_sats;         // Satellites in view (1 byte)
    double   gps_latitude_deg;     // Latitude in decimal degrees (8 bytes)
    double   gps_longitude_deg;    // Longitude in decimal degrees (8 bytes)
    float    gps_alt_msl_m;        // GPS Altitude above mean sea level in meters (4 bytes)
    
    // --- GPS 3D Velocity & Motion (Doppler Navigation Solution) ---
    float    gps_ground_speed_ms;  // 2D Horizontal Ground Speed in m/s (4 bytes)
    float    gps_vel_north_ms;     // NED North Velocity component in m/s (4 bytes)
    float    gps_vel_east_ms;      // NED East Velocity component in m/s (4 bytes)
    float    gps_vel_down_ms;      // NED Down Velocity component in m/s (+ = down, - = climb) (4 bytes)
    float    gps_heading_deg;      // Heading / Track angle in degrees (4 bytes)

    // --- Identification ---
    char     callsign[8];          // Station Ham Call-Sign string, null-terminated (8 bytes)
};
#pragma pack(pop)

// Constant packet size
constexpr size_t TELEMETRY_PACKET_SIZE = sizeof(TelemetryPacket);
static_assert(TELEMETRY_PACKET_SIZE == 107, "TelemetryPacket must be 107 bytes, the same as Alex's firmware");

// ============================================================================
// DECODER & HELPER FUNCTIONS FOR TEAM MEMBERS
// ============================================================================

/**
 * @brief Convert FlightState enum to human-readable string
 */
inline const char* flightStateToString(uint8_t state) {
    switch (state) {
        case STATE_PAD:     return "PAD";
        case STATE_BOOST:   return "BOOST";
        case STATE_COAST:   return "COAST";
        case STATE_DESCENT: return "DESCENT";
        case STATE_LANDED:  return "LANDED";
        default:            return "UNKNOWN";
    }
}

/**
 * @brief Decodes a raw byte buffer received over LoRa into a TelemetryPacket struct.
 * @param buffer Raw buffer received from radio
 * @param len Length in bytes of received payload
 * @param out_pkt Output struct where decoded values will be populated
 * @return True if length matches expected size and decoding succeeded, false otherwise.
 */
inline bool decodeTelemetryPacket(const uint8_t* buffer, size_t len, TelemetryPacket& out_pkt) {
    if (buffer == nullptr || len != TELEMETRY_PACKET_SIZE) {
        return false;
    }
    memcpy(&out_pkt, buffer, TELEMETRY_PACKET_SIZE);
    
    // Ensure callsign is safely null-terminated
    out_pkt.callsign[sizeof(out_pkt.callsign) - 1] = '\0';
    return true;
}
