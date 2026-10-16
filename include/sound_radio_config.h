#pragma once
#include <stdint.h>

// Sender and receiver must use the same channel. No access point is required.
constexpr uint8_t SOUND_RADIO_CHANNEL = 6;
static_assert(SOUND_RADIO_CHANNEL >= 1 && SOUND_RADIO_CHANNEL <= 11, "Invalid radio channel");
