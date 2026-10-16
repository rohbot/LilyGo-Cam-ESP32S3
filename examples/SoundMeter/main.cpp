/** @license MIT
 * Standalone OLED sound meter for LilyGO T-Camera S3 v1.6/v1.7.
 */
#include <Arduino.h>
#include <ESP_I2S.h>
#include <Wire.h>
#include <U8g2lib.h>
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "config.h"
#include "audio_level.h"
#include "noise_detector.h"
#include "event_radio.h"

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t FRAME_SAMPLES = 512;  // 32 ms blocks.
constexpr float SMOOTHING_SECONDS = 0.125f;
constexpr uint32_t DISPLAY_INTERVAL_MS = 100;
constexpr uint32_t PEAK_HOLD_MS = 1000;

XPowersPMU power;
I2SClass microphone(I2S_NUM_0);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R2, U8X8_PIN_NONE);
NoiseDetector detector;
EventRadio radio;

#if LILYGO_BOARD_VERSION == 16
int32_t samples[FRAME_SAMPLES * 2];
#else
int16_t samples[FRAME_SAMPLES];
#endif

bool haveLevel = false;
float smoothedPower = 0.0f;
float currentDbfs = -120.0f;
float heldDbfs = -120.0f;
float maximumDbfs = -120.0f;
uint32_t lastPeakMs = 0;
uint32_t lastUpdateMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastClipMs = 0;
bool haveClipped = false;

void showMessage(const char *title, const char *detail)
{
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 20, title);
    display.drawStr(0, 38, detail);
    display.sendBuffer();
}

void halt(const char *message, const char *detail = "Check board revision")
{
    Serial.println(message);
    showMessage(message, detail);
    while (true) {
        delay(1000);
    }
}

float displayedDb(float dbfs)
{
    return dbfs + (SPL_CALIBRATED ? SPL_OFFSET_DB : 0.0f);
}

uint8_t barPosition(float dbfs)
{
    const float fraction = (dbfs - BAR_MIN_DBFS) / (BAR_MAX_DBFS - BAR_MIN_DBFS);
    return static_cast<uint8_t>(constrain(fraction, 0.0f, 1.0f) * 124.0f);
}

void drawMeter(uint32_t now)
{
    char text[32];
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    if (!detector.ready()) {
        display.drawStr(0, 9, "LEARNING FLOOR");
    } else {
        snprintf(text, sizeof(text), "%s N:%lu",
                 radio.lastQueueFailed() ? "TX ERR" : detector.active() ? "EVENT" : "LISTEN",
                 static_cast<unsigned long>(radio.eventCount()));
        display.drawStr(0, 9, text);
    }
    if (haveClipped && now - lastClipMs < 1000) {
        display.drawStr(104, 9, "CLIP");
    }

    snprintf(text, sizeof(text), "%.1f", displayedDb(currentDbfs));
    display.setFont(u8g2_font_logisoso22_tn);
    display.drawStr(0, 35, text);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(98, 26, SPL_CALIBRATED ? "SPL*" : "dBFS");

    display.drawFrame(0, 40, 128, 12);
    display.drawBox(2, 42, barPosition(currentDbfs), 8);
    const uint8_t marker = 2 + min<uint8_t>(barPosition(heldDbfs), 123);
    display.setDrawColor(2);  // Keep the marker visible over a filled bar.
    display.drawVLine(marker, 41, 10);
    display.setDrawColor(1);
    if (detector.ready()) {
        snprintf(text, sizeof(text), "NF %.0f %+.0fdB",
                 displayedDb(detector.floorDbfs()), currentDbfs - detector.floorDbfs());
    } else {
        snprintf(text, sizeof(text), "Keep background quiet");
    }
    display.drawStr(0, 63, text);
    display.sendBuffer();
}

void checkResetButton(uint32_t now)
{
    static bool previousRaw = HIGH;
    static bool stable = HIGH;
    static uint32_t changedMs = 0;
    static uint32_t pressedMs = 0;
    static bool longPress = false;
    const bool raw = digitalRead(RESET_BUTTON_PIN);
    if (raw != previousRaw) {
        previousRaw = raw;
        changedMs = now;
    }
    if (raw != stable && now - changedMs >= 40) {
        stable = raw;
        if (stable == LOW) {
            pressedMs = now;
            longPress = false;
            heldDbfs = maximumDbfs = currentDbfs;
            lastPeakMs = now;
            haveClipped = false;
        }
    }
    if (stable == LOW && !longPress && now - pressedMs >= 1500) {
        detector.reset();
        longPress = true;
        Serial.println("Relearning noise floor");
    }
}

void setup()
{
    Serial.begin(115200);  // Start with or without a connected serial monitor.
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    display.setBusClock(400000);
    display.begin();
    showMessage("SOUND METER", "Starting microphone");

    if (!power.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA_PIN, I2C_SCL_PIN)) {
        halt("Power init failed");
    }
    power.setBLDO1Voltage(3300);
    power.enableBLDO1();
    power.disableTSPinMeasure();
    power.setChargingLedMode(XPOWERS_CHG_LED_OFF);
    delay(100);

    microphone.setTimeout(200);
#if LILYGO_BOARD_VERSION == 16
    microphone.setPins(MIC_CLOCK_PIN, MIC_WS_OR_LR_PIN, -1, MIC_DATA_PIN);
    const bool ready = microphone.begin(
        I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
        I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH);
#else
    pinMode(MIC_WS_OR_LR_PIN, OUTPUT);
    digitalWrite(MIC_WS_OR_LR_PIN, LOW);
    microphone.setPinsPdmRx(MIC_CLOCK_PIN, MIC_DATA_PIN);
    const bool ready = microphone.begin(
        I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT);
#endif
    if (!ready) {
        halt("Mic init failed");
    }
    // Discard startup samples while the microphone settles.
    for (int i = 0; i < 8; ++i) {
        microphone.readBytes(reinterpret_cast<char *>(samples), sizeof(samples));
    }
    if (!radio.begin()) {
        halt("ESP-NOW init failed", "Check radio/channel");
    }
    Serial.printf("Sound meter ready: board v1.%d, %s\n",
                  LILYGO_BOARD_VERSION % 10, SPL_CALIBRATED ? "estimated SPL" : "dBFS");
}

void loop()
{
    const size_t bytesRead = microphone.readBytes(
        reinterpret_cast<char *>(samples), sizeof(samples));
    const uint32_t now = millis();
    radio.poll(now);
    checkResetButton(now);
    if (bytesRead != sizeof(samples)) {
        Serial.printf("Microphone read failed: %d\n", microphone.lastError());
        showMessage("No audio data", "Check mic / revision");
        // Recover without blending old readings into a new stream.
        haveLevel = false;
        detector.reset();
        delay(100);
        return;
    }

#if LILYGO_BOARD_VERSION == 16
    const AudioLevel left = calculateLevel(samples, FRAME_SAMPLES, 2, 2147483648.0);
    const AudioLevel right = calculateLevel(samples + 1, FRAME_SAMPLES, 2, 2147483648.0);
    // The single microphone occupies one slot; accommodate either L/R wiring.
    const AudioLevel level = left.power >= right.power ? left : right;
#else
    const AudioLevel level = calculateLevel(samples, FRAME_SAMPLES, 1, 32768.0);
#endif
    if (level.peak >= 0.99f) {
        lastClipMs = now;
        haveClipped = true;
    }
    const float blockDbfs = powerToDbfs(level.power);
    if (detector.update(blockDbfs, FRAME_SAMPLES * 1000 / SAMPLE_RATE)) {
        radio.sendEvent(blockDbfs, detector.floorDbfs(), detector.eventDurationMs(), now);
        Serial.printf("EVENT #%lu: %.1f dBFS, floor %.1f, rise %.1f dB\n",
                      static_cast<unsigned long>(radio.eventCount()), blockDbfs,
                      detector.floorDbfs(), blockDbfs - detector.floorDbfs());
    }
    if (!haveLevel) {
        smoothedPower = level.power;
        heldDbfs = blockDbfs;
        haveLevel = true;
        lastPeakMs = now;
    } else {
        // Smooth energy, rather than averaging decibel readings.
        const float alpha = 1.0f - expf(-static_cast<float>(FRAME_SAMPLES) /
                                      (SAMPLE_RATE * SMOOTHING_SECONDS));
        smoothedPower += alpha * (level.power - smoothedPower);
        if (blockDbfs >= heldDbfs) {
            heldDbfs = blockDbfs;
            lastPeakMs = now;
        } else if (now - lastPeakMs >= PEAK_HOLD_MS) {
            heldDbfs = fmaxf(blockDbfs, heldDbfs - 18.0f * (now - lastUpdateMs) / 1000.0f);
        }
    }
    lastUpdateMs = now;
    currentDbfs = powerToDbfs(smoothedPower);
    maximumDbfs = fmaxf(maximumDbfs, blockDbfs);

    if (now - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
        lastDisplayMs = now;
        drawMeter(now);
    }
    static uint32_t lastSerialMs = 0;
    if (now - lastSerialMs >= 500) {
        lastSerialMs = now;
        Serial.printf("Level %.1f %s, peak %.1f, max %.1f\n",
                      displayedDb(currentDbfs), SPL_CALIBRATED ? "dB SPL*" : "dBFS",
                      displayedDb(heldDbfs), displayedDb(maximumDbfs));
        Serial.printf("Floor %.1f dBFS, state %s, events %lu\n", detector.floorDbfs(),
                      !detector.ready() ? "learning" : detector.active() ? "event" : "listening",
                      static_cast<unsigned long>(radio.eventCount()));
    }
}
