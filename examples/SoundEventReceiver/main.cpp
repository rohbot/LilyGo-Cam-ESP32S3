/** @license MIT
 * Receive sound events on a second ESP32-S3; print one line per unique event.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <sound_event_protocol.h>
#include <sound_radio_config.h>

struct ReceivedEvent {
    uint8_t source[6];
    SoundEventPacket packet;
};
QueueHandle_t events;

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int length)
{
    ReceivedEvent event;
    if (!info || length < 0 || !decodeSoundEvent(data, static_cast<size_t>(length), event.packet)) {
        return;
    }
    memcpy(event.source, info->src_addr, 6);
    // Wi-Fi task callback: copy only; process/print in loop().
    xQueueSend(events, &event, 0);
}

bool duplicate(const ReceivedEvent &event)
{
    struct SeenSender {
        bool used = false;
        uint8_t mac[6] = {};
        uint32_t bootId = 0;
        uint32_t sequence = 0;
    };
    static SeenSender senders[8];
    static uint8_t replace = 0;
    SeenSender *entry = nullptr;
    for (auto &sender : senders) {
        if (sender.used && memcmp(sender.mac, event.source, 6) == 0) {
            entry = &sender;
            break;
        }
    }
    if (!entry) {
        entry = &senders[replace];
        replace = (replace + 1) % 8;
        entry->used = true;
        memcpy(entry->mac, event.source, 6);
    } else if (entry->bootId == event.packet.bootId &&
               static_cast<int32_t>(event.packet.sequence - entry->sequence) <= 0) {
        return true;
    }
    entry->bootId = event.packet.bootId;
    entry->sequence = event.packet.sequence;
    return false;
}

void setup()
{
    Serial.begin(115200);
    events = xQueueCreate(16, sizeof(ReceivedEvent));
    if (!events || !WiFi.mode(WIFI_STA) || !WiFi.setSleep(false)
        || esp_wifi_set_channel(SOUND_RADIO_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK
        || esp_now_init() != ESP_OK || esp_now_register_recv_cb(onReceive) != ESP_OK) {
        while (true) {
            Serial.println("Receiver initialization failed");
            delay(1000);
        }
    }
}

void loop()
{
    // Repeat the MAC so it remains visible if the serial monitor opens after boot.
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus >= 5000) {
        lastStatus = millis();
        Serial.printf("ESP-NOW receiver %s, channel %u\n",
                      WiFi.macAddress().c_str(), SOUND_RADIO_CHANNEL);
    }
    ReceivedEvent event;
    if (xQueueReceive(events, &event, pdMS_TO_TICKS(20)) == pdTRUE && !duplicate(event)) {
        const auto &p = event.packet;
        Serial.printf("SOUND EVENT from %02X:%02X:%02X:%02X:%02X:%02X #%lu "
                      "level=%.2f dBFS floor=%.2f dBFS rise=%.2f dB duration=%lu ms uptime=%lu ms\n",
                      event.source[0], event.source[1], event.source[2],
                      event.source[3], event.source[4], event.source[5],
                      static_cast<unsigned long>(p.sequence), p.levelCentiDbfs / 100.0f,
                      p.floorCentiDbfs / 100.0f, (p.levelCentiDbfs - p.floorCentiDbfs) / 100.0f,
                      static_cast<unsigned long>(p.aboveThresholdMs), static_cast<unsigned long>(p.uptimeMs));
        // Add the receiving ESP32's desired action here.
    }
}
