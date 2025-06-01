#ifndef BEAMFORMING_H
#define BEAMFORMING_H

#include <cstdint>

struct BeamformingInfo {
    double sender_latitude;
    double sender_longitude;
    float beam_center_angle;  // degrees, 0-359.9
    float beam_width_angle;   // degrees, 360.0 = omnidirectional
    float max_range;          // meters
    
    BeamformingInfo() : sender_latitude(0.0), sender_longitude(0.0),
                        beam_center_angle(0.0f), beam_width_angle(360.0f),
                        max_range(1000.0f) {}
} __attribute__((packed));

#endif // BEAMFORMING_H 