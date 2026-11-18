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
// Stop-gap until the flight computer sends its own orientation: this only guesses from data meant
// for other things, so it shows tilt but not spin about the rocket's axis or heading.
//  - Gravity: when the accelerometer reads ~1 g the rocket is resting or hanging still, and "down"
//    gives its tilt. Used in every stage (it used to be PAD only, so the view froze on the bench
//    after a launch trigger, under the chute and after landing).
//  - Direction of travel: in BOOST/COAST when moving faster than 5 m/s, now also on the way down
//    after apogee (it used to need a climb, so the view froze at apogee).
//  - Otherwise hold the last orientation.
inline void OrientationTracker::on_packet(const TelemetryPacket& p, const FlightStats& s, long long pc_ms){
    Quaternion target = to;                       // default: hold the last target

    Vector3 a = { p.accel_z_g, p.accel_y_g, -p.accel_x_g };
    float g = Vector3Length(a);
    bool at_rest = (g > 0.8f && g < 1.2f);
    Quaternion from_gravity = at_rest ? QuaternionFromVector3ToVector3(Vector3Normalize(a), Vector3{0,0,1})
                                      : to;

    if (p.flight_state == STATE_BOOST || p.flight_state == STATE_COAST) {
        float vn = 0, ve = 0, up = 0;
        if (gps_fix_usable(p.gps_fix_type, p.gps_num_sats)) {   // 4-5 satellite fixes can be wildly off
            up = -p.gps_vel_down_ms;
            vn = p.gps_vel_north_ms;
            ve = p.gps_vel_east_ms;
        }
        else up = s.vspeed_ms;

        Vector3 v = { vn, -ve, up };   // north, west (= -east), up
        float speed = Vector3Length(v);
        //5 m/s is not fixed, lower = more noise
        if (speed >= 5.0f)       target = QuaternionFromVector3ToVector3(Vector3{0,0,1}, Vector3Normalize(v));
        else if (at_rest)        target = from_gravity;   // e.g. on the bench after a launch trigger
    }
    else if (at_rest) {
        target = from_gravity;   // PAD, DESCENT (hanging under the chute), LANDED (lying on the ground)
    }
    from = at(pc_ms);
    to = target;
    t_packet_ms = pc_ms;
}
inline Quaternion OrientationTracker::at(long long now_ms) const{
    float t = Clamp((now_ms-t_packet_ms)/200.0,0,1);
    return QuaternionSlerp(from,to,t);
}
