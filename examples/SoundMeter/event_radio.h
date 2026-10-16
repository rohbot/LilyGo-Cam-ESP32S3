#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_random.h>
#include <sound_radio_config.h>
#include <sound_event_protocol.h>
#include "config.h"

class EventRadio {
public:
    bool begin()
    {
        if (!WiFi.mode(WIFI_STA) || !WiFi.setSleep(false)
            || esp_wifi_set_channel(SOUND_RADIO_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK
            || esp_now_init() != ESP_OK) {
            return false;
        }
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, ESPNOW_DESTINATION, 6);
        peer.channel = SOUND_RADIO_CHANNEL;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        if (esp_now_add_peer(&peer) != ESP_OK) {
            return false;
        }
        bootId_ = esp_random();
        Serial.printf("ESP-NOW sender %s, channel %u\n", WiFi.macAddress().c_str(), SOUND_RADIO_CHANNEL);
        return true;
    }

    void sendEvent(float levelDbfs, float floorDbfs, uint32_t durationMs, uint32_t now)
    {
        packet_.bootId = bootId_;
        packet_.sequence = ++sequence_;
        packet_.uptimeMs = now;
        packet_.levelCentiDbfs = lroundf(levelDbfs * 100.0f);
        packet_.floorCentiDbfs = lroundf(floorDbfs * 100.0f);
        packet_.aboveThresholdMs = durationMs;
        remaining_ = ESPNOW_EVENT_COPIES;
        sendCopy(now);
    }

    void poll(uint32_t now)
    {
        if (remaining_ && now - lastSendMs_ >= ESPNOW_REPEAT_MS) {
            sendCopy(now);
        }
    }

    uint32_t eventCount() const { return sequence_; }
    bool lastQueueFailed() const { return queueFailed_; }

private:
    void sendCopy(uint32_t now)
    {
        const esp_err_t result = esp_now_send(ESPNOW_DESTINATION,
            reinterpret_cast<const uint8_t *>(&packet_), sizeof(packet_));
        queueFailed_ = result != ESP_OK;
        if (queueFailed_) {
            Serial.printf("ESP-NOW queue error: %s\n", esp_err_to_name(result));
        }
        --remaining_;
        lastSendMs_ = now;
    }
    SoundEventPacket packet_;
    uint32_t bootId_ = 0, sequence_ = 0, lastSendMs_ = 0;
    uint8_t remaining_ = 0;
    bool queueFailed_ = false;
};
