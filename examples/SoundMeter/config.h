#pragma once
#include <stdint.h>

// PlatformIO selects the microphone type through the v1_6 / v1_7 environment.
#ifndef LILYGO_BOARD_VERSION
#define LILYGO_BOARD_VERSION 16
#endif
#if LILYGO_BOARD_VERSION != 16 && LILYGO_BOARD_VERSION != 17
#error "Select board revision 16 (I2S) or 17 (PDM)"
#endif

// Leave false until compared against a reference sound level meter.
// Offset = reference dB SPL - this meter's dBFS at the same steady sound.
// This is a broadband estimate; there is no A/C frequency weighting.
constexpr bool SPL_CALIBRATED = false;
constexpr float SPL_OFFSET_DB = 0.0f;

constexpr int I2C_SDA_PIN = 7;
constexpr int I2C_SCL_PIN = 6;
constexpr int MIC_DATA_PIN = 41;
constexpr int MIC_CLOCK_PIN = 40;
constexpr int MIC_WS_OR_LR_PIN = 42;
constexpr int RESET_BUTTON_PIN = 0;  // BOOT, press during normal operation.
constexpr float BAR_MIN_DBFS = -90.0f;
constexpr float BAR_MAX_DBFS = 0.0f;

// Broadcast initially. Replace with the receiver's STA MAC for unicast.
constexpr uint8_t ESPNOW_DESTINATION[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
constexpr uint8_t ESPNOW_EVENT_COPIES = 3;
constexpr uint32_t ESPNOW_REPEAT_MS = 100;

constexpr uint32_t NOISE_LEARN_MS = 5000;
constexpr float EVENT_ABOVE_FLOOR_DB = 10.0f;
constexpr float EVENT_RELEASE_ABOVE_FLOOR_DB = 6.0f;
constexpr float EVENT_MIN_DBFS = -85.0f;  // Ignore tiny changes near digital silence.
constexpr uint32_t EVENT_MIN_DURATION_MS = 64;
constexpr uint32_t EVENT_QUIET_REARM_MS = 500;
constexpr uint32_t EVENT_COOLDOWN_MS = 2000;
constexpr float NOISE_RISE_SECONDS = 30.0f;
constexpr float NOISE_FALL_SECONDS = 3.0f;
static_assert(EVENT_ABOVE_FLOOR_DB > EVENT_RELEASE_ABOVE_FLOOR_DB, "Event hysteresis required");
static_assert(NOISE_LEARN_MS > 0 && EVENT_MIN_DURATION_MS > 0 && EVENT_QUIET_REARM_MS > 0,
              "Detector durations must be positive");
static_assert(NOISE_RISE_SECONDS > 0 && NOISE_FALL_SECONDS > 0, "Invalid floor time constants");
static_assert(ESPNOW_EVENT_COPIES > 0 &&
              EVENT_COOLDOWN_MS > ESPNOW_EVENT_COPIES * ESPNOW_REPEAT_MS,
              "Finish retransmissions before another event");
