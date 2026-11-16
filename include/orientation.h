#pragma once
#include "rasalhague_packet.h"
#include "flight_stats.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "raymath.h"
#pragma GCC diagnostic pop

struct OrientationTracker {
    Quaternion from = QuaternionIdentity();   // where the smoothing started
    Quaternion to   = QuaternionIdentity();   // latest target
    long long t_packet_ms = 0;

    void on_packet(const TelemetryPacket& p, const FlightStats& s, long long pc_ms);  // once per NEW packet
    Quaternion at(long long now_ms) const;                                           // once per frame
}; 
inline void OrientationTracker::on_packet(const TelemetryPacket& p, const FlightStats& s, long long pc_ms){
    Quaternion target = to;                       // default: hold the last target

    if (p.flight_state == STATE_PAD) {
        Vector3 a = { p.accel_z_g, p.accel_y_g, -p.accel_x_g };
        float g = Vector3Length(a);
        if (g > 0.8f && g < 1.2f)
            target = QuaternionFromVector3ToVector3(Vector3Normalize(a), Vector3{0,0,1});
    }
    else if(p.flight_state == STATE_BOOST || p.flight_state == STATE_COAST ){
        float vn = 0, ve = 0, up = 0;
        if(p.gps_fix_type >= 3){
            up = -p.gps_vel_down_ms;
            vn = p.gps_vel_north_ms;
            ve = p.gps_vel_east_ms;
        } 
        else up = s.vspeed_ms;

        Vector3 v = { vn, -ve, up };   // north, west (= -east), up
        float speed = Vector3Length(v);
        //5 m/s is not fixed, lower = more noise
        if (speed >= 5.0f && v.z > 0.0f) {
            target = QuaternionFromVector3ToVector3(Vector3{0,0,1}, Vector3Normalize(v));
        }     
    }
    from = at(pc_ms);
    to = target;
    t_packet_ms = pc_ms;
}
inline Quaternion OrientationTracker::at(long long now_ms) const{
    float t = Clamp((now_ms-t_packet_ms)/200.0,0,1);
    return QuaternionSlerp(from,to,t);
}
