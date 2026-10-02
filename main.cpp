// swamp-gcs tests. Build and run from the swamp-gcs folder:
//   g++ -Wall -Wextra -I include .\main.cpp -o .\main.exe
//   .\main.exe
// Each check prints PASS or FAIL; the program returns the number of failures.
#include <line_reader.h>
#include <rx_line.h>
#include <rasalhague_packet.h>
#include <cstdio>
#include <cstring>
#include <string>

static int failures = 0;

static void expect(bool ok, const char* what){
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if(!ok) failures++;
}

// Alex's 107-byte test packet, as the ground receiver will print it.
// Values in field order: 412345, 0, 4.012, 1007.25, 50.5, 22.15, -0.01, 0.02, 1.0,
// 0.35, -0.7, 0.07, 0.21, -0.05, 0.43, 3, 11, 29.6516, -82.3248, 54.0, 0.12, 0.05,
// -0.11, 0.02, 293.5, "N0CALL"
static const std::string TEST_LINE =
    R"({"type":"rx","rssi":-87.5,"snr":9.5,"hex":")"
    "b94a0600004e62804000d07b4400004a423333b1410ad723bc0ad7a33c0000803f3333b3"
    "3e333333bf295c8f3d3d0a573ecdcc4cbdf628dc3e030bd712f241cfa63d40696ff085c9"
    "9454c0000058428fc2f53dcdcc4c3dae47e1bd0ad7a33c00c092434e3043414c4c0000"
    R"("})";

static void test_line_reader(){
    std::printf("-- LineReader\n");
    std::string line;

    LineReader a;
    a.feed("abc\r\n", 5);
    expect(a.next_line(line) && line == "abc", "a CRLF line comes out without \\r\\n");

    LineReader b;
    b.feed("ab", 2);
    bool early = b.next_line(line);
    b.feed("c\n", 2);
    expect(!early && b.next_line(line) && line == "abc", "a line split across two feeds is joined");

    LineReader c;
    c.feed("\n\r\nxyz\n", 7);
    expect(c.next_line(line) && line == "xyz", "empty lines are skipped");

    LineReader d;
    d.feed("one\ntwo\n", 8);
    bool first = d.next_line(line) && line == "one";
    bool second = d.next_line(line) && line == "two";
    expect(first && second && !d.next_line(line), "two lines in one feed come out in order");

    LineReader e;
    std::string text(600, 'a');
    text += "\nok\n";
    e.feed(text.data(), text.size());
    expect(e.next_line(line) && line == "ok" && e.dropped_Count() == 1, "an over-long line is dropped and counted");
}

static void test_parse_rx_line(){
    std::printf("-- parse_rx_line\n");
    RxLine rx;
    expect(parse_rx_line(TEST_LINE, rx), "the test line parses");
    expect(rx.rssi == -87.5f, "rssi is -87.5 dBm");
    expect(rx.snr == 9.5f, "snr is 9.5 dB");
    expect(rx.hex.size() == 214, "hex is 214 characters (107 bytes)");

    RxLine junk;
    expect(!parse_rx_line(R"({"type":"status","msg":"E22 ready"})", junk), "a status line is not an rx line");
    expect(!parse_rx_line("[TRACKER] Azimuth: 12.00 deg | Elevation: 5.00 deg", junk), "receiver debug text is ignored");
    expect(!parse_rx_line(R"({"type":"rx","rssi":-87,"snr":9.5,"hex":""})", junk), "empty hex is rejected");
    expect(!parse_rx_line(R"({"type":"rx","rssi":-87,"snr":9.5,"hex":"abc"})", junk), "odd-length hex is rejected");
    expect(!parse_rx_line(R"({"type":"rx","rssi":-87,"snr":9.5,"hex":"zz"})", junk), "non-hex characters are rejected");
    expect(!parse_rx_line(R"({"type":"rx","rssi":-87,"hex":"abcd"})", junk), "a line without snr is rejected");
}

static void test_hex(){
    std::printf("-- hex_value and hex_to_bytes\n");
    expect(hex_value('0') == 0 && hex_value('9') == 9 && hex_value('a') == 10 && hex_value('F') == 15,
           "hex digits map to 0-15");
    expect(hex_value('g') == -1 && hex_value(':') == -1, "other characters give -1");
    uint8_t two[2] = {0, 0};
    expect(hex_to_bytes("01ff", two, 2) && two[0] == 0x01 && two[1] == 0xFF, "\"01ff\" becomes 0x01 0xFF");
    expect(!hex_to_bytes("01f", two, 2), "the wrong number of characters is rejected");
    expect(!hex_to_bytes("0g", two, 1), "a bad character is rejected");
}

static void test_rasalhague_packet(){
    std::printf("-- Rasalhague packet\n");
    RxLine rx;
    uint8_t buf[255] = {0};
    TelemetryPacket pkt{};
    bool parsed = parse_rx_line(TEST_LINE, rx);
    size_t n = rx.hex.size() / 2;
    bool bytes_ok = parsed && n <= sizeof buf && hex_to_bytes(rx.hex, buf, n);
    expect(bytes_ok && decodeTelemetryPacket(buf, n, pkt), "line -> bytes -> 107-byte packet");

    // Compare with float literals (4.012f, not 4.012): the float the board sent is
    // never equal to the double 4.012, but it is exactly equal to 4.012f.
    expect(pkt.timestamp_ms == 412345 && pkt.flight_state == STATE_PAD && pkt.batt_voltage_v == 4.012f,
           "time, flight state, battery");
    expect(pkt.baro_pressure_hpa == 1007.25f && pkt.baro_alt_m == 50.5f && pkt.baro_temp_c == 22.15f,
           "barometer: pressure, altitude, temperature");
    expect(pkt.accel_x_g == -0.01f && pkt.accel_y_g == 0.02f && pkt.accel_z_g == 1.0f, "accelerometer");
    expect(pkt.gyro_x_dps == 0.35f && pkt.gyro_y_dps == -0.7f && pkt.gyro_z_dps == 0.07f, "gyroscope");
    expect(pkt.mag_x_gauss == 0.21f && pkt.mag_y_gauss == -0.05f && pkt.mag_z_gauss == 0.43f, "magnetometer");
    expect(pkt.gps_fix_type == 3 && pkt.gps_num_sats == 11 && pkt.gps_latitude_deg == 29.6516
           && pkt.gps_longitude_deg == -82.3248 && pkt.gps_alt_msl_m == 54.0f,
           "GPS fix and position");
    expect(pkt.gps_ground_speed_ms == 0.12f && pkt.gps_vel_north_ms == 0.05f && pkt.gps_vel_east_ms == -0.11f
           && pkt.gps_vel_down_ms == 0.02f && pkt.gps_heading_deg == 293.5f,
           "GPS velocity and heading");
    expect(std::strcmp(pkt.callsign, "N0CALL") == 0, "callsign");

    expect(!decodeTelemetryPacket(buf, 52, pkt), "a 52-byte packet is rejected");
    expect(std::strcmp(flightStateToString(STATE_DESCENT), "DESCENT") == 0
           && std::strcmp(flightStateToString(9), "UNKNOWN") == 0,
           "state names, and UNKNOWN for anything else");

    uint8_t full[107];
    std::memset(full, 'A', sizeof full);   // the last 8 bytes (callsign) have no terminator
    expect(decodeTelemetryPacket(full, sizeof full, pkt) && pkt.callsign[7] == '\0',
           "callsign is null-terminated even if the board sends 8 letters");
}

int main(){
    test_line_reader();
    test_parse_rx_line();
    test_hex();
    test_rasalhague_packet();
    if(failures == 0) std::printf("\nALL PASS\n");
    else std::printf("\n%d FAILED\n", failures);
    return failures;
}
