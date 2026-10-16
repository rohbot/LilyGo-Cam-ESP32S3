# ESP-NOW sound event receiver

Flash this separate firmware onto the second ESP32-S3. It receives the
[LilyGO sound meter's](../SoundMeter/README.md) event messages and prints a line
per unique event at 115200 baud. It needs no microphone or OLED.

From the repository root:

```sh
~/.platformio/penv/bin/pio run -d examples/SoundEventReceiver
~/.platformio/penv/bin/pio run -d examples/SoundEventReceiver -t upload --upload-port /dev/ttyACM1
~/.platformio/penv/bin/pio device monitor -p /dev/ttyACM1 -b 115200
```

Replace the port with the receiving board's actual serial port. The separate
`platformio.ini` uses the `esp32-s3-devkitc-1` preset with a **4 MB flash** override
and the Arduino `default.csv` partition layout to match the connected receiver.
Adjust its board, flash, and USB settings if your receiving S3 board differs. This does not change the
LilyGO sender's build configuration. Uploading replaces the receiver's old firmware.

Both devices default to **channel 6**, configured in the shared
`../../include/sound_radio_config.h`. No router or Wi-Fi credentials are used.
The receiver prints its station MAC every five seconds; you may use this MAC as
the sender's `ESPNOW_DESTINATION` for unicast. Broadcast works without entering a MAC.

Keep the background quiet during the sender's five-second learning period. Then
make a loud sound lasting at least 64 ms (for example, speak near the microphone).
Expected output resembles:

```text
SOUND EVENT from AA:BB:CC:DD:EE:FF #1 level=-40.00 dBFS floor=-60.00 dBFS rise=20.00 dB duration=64 ms uptime=8000 ms
```

Keep quiet for at least 500 ms and allow two seconds between event onsets to
trigger another event. Continuous loud sound produces a single event. Adjust
the sender's thresholds if your microphone or room needs different sensitivity.

The Wi-Fi callback validates and copies packets into a bounded queue; printing
and duplicate suppression happen in `loop()`. Add your desired receiving-board
action at the indicated point after a unique event is accepted. This example
accepts compatible unencrypted packets from any sender in range on its channel.
Retransmitted packets are suppressed for up to eight tracked sender MACs.
Queues or radio packets may be dropped; there is no application-level delivery
acknowledgement or persistent event storage.
