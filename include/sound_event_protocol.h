#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Wire format v1, little-endian ESP32 integers, levels always in centi-dBFS.
struct SoundEventPacket {
    uint32_t magic = 0x534E4431;  // SND1
    uint16_t version = 1;
    uint16_t size = 32;
    uint32_t bootId = 0;
    uint32_t sequence = 0;
    uint32_t uptimeMs = 0;
    int32_t levelCentiDbfs = 0;
    int32_t floorCentiDbfs = 0;
    uint32_t aboveThresholdMs = 0;
};
static_assert(sizeof(SoundEventPacket) == 32, "Sound event wire layout changed");

inline bool decodeSoundEvent(const uint8_t *data, size_t length, SoundEventPacket &packet)
{
    if (!data || length != sizeof(packet)) {
        return false;
    }
    memcpy(&packet, data, sizeof(packet));
    return packet.magic == 0x534E4431 && packet.version == 1 && packet.size == sizeof(packet)
        && packet.sequence != 0 && packet.levelCentiDbfs >= -12000 && packet.levelCentiDbfs <= 0
        && packet.floorCentiDbfs >= -12000 && packet.floorCentiDbfs <= 0;
}
