#include <telemetry_packet.h>
#include <iostream>

int main(){
    std::cout << std::hex << std::uppercase << tlm_crc16(reinterpret_cast<const uint8_t*>("123446789"), 9);
    return 0;
}