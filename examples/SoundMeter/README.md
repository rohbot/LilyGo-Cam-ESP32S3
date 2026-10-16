# OLED sound meter

Standalone microphone level meter for the LilyGO T-Camera ESP32-S3, using the
onboard SSD1306 128×64 OLED, with adaptive noise-floor detection and ESP-NOW alerts.
No camera, Wi-Fi access point, or serial monitor is needed to run. The Wi-Fi radio
is enabled in station mode for ESP-NOW, without joining a network.
The project selects this firmware by default through `platformio.ini`.

## Build and upload

Run from the repository root. Choose the board revision printed on your board:

| Board | Microphone | Environment |
| --- | --- | --- |
| v1.6 | Standard I2S | `v1_6` |
| v1.7 | PDM | `v1_7` |

```sh
~/.platformio/penv/bin/pio run -e v1_6
~/.platformio/penv/bin/pio run -e v1_6 -t upload
~/.platformio/penv/bin/pio device monitor -b 115200
```

Replace `v1_6` with `v1_7` for that revision. If needed, add
`--upload-port /dev/ttyACM0` to the upload command, using your board's actual port.
Use PlatformIO's current Python environment; this machine's `/usr/bin/pio` is an
older installation. Uploading replaces the firmware currently on the board.

Build outputs are `.pio/build/<environment>/firmware.bin` and
`.pio/build/<environment>/firmware.factory.bin` (the combined flash image).
To restore the factory application, select `src_dir = examples/factory` in
`platformio.ini` and rebuild/upload.

## Display and controls

- Large number: DC-removed RMS signal level with 125 ms energy smoothing.
- Bar: current level from −90 to 0 dBFS. The thin marker holds the highest
  32 ms block RMS for one second, then falls at 18 dB per second.
- Top: `LEARNING FLOOR`, `LISTEN`, or `EVENT`, with the detected event count `N`.
  `TX ERR` indicates that the last ESP-NOW send could not be queued locally;
  `N` counts detected events, not confirmed deliveries.
- Bottom: `NF` is the learned noise floor, followed by the current level above it.
  The bar's marker still shows the held block RMS level.
- `CLIP`: a sample approached the digital full-scale limit within the last second.
- Tap **BOOT** while running to reset the peak marker, internal maximum, and clipping indicator.
  Hold for **1.5 seconds** to relearn the background noise floor for five seconds.
  Holding BOOT during reset instead enters the board's firmware download mode.
- Serial output repeats the levels twice per second for calibration or logging.

## Noise floor and event detection

Allow five seconds of representative background sound at startup. Detection is
disabled during this period. The initial floor is the 20th percentile of the
32 ms block RMS levels, in 1 dB bins, so occasional louder sounds have limited
influence. The floor then tracks normal background changes slowly upward
(30 second time constant) and faster downward (3 seconds).

An event requires two consecutive 32 ms RMS blocks at least **10 dB above the
floor**, and at least **−85 dBFS**. It sends one alert when confirmed. The floor
is frozen while a candidate or event is loud, so a sustained sound cannot raise
its own detection threshold. Rearming requires 500 ms at or below the floor +
6 dB (with the corresponding absolute release limit), plus a two-second minimum
interval between alerts. A sustained noise sends one event until it becomes quiet.
If a new permanent background is above the trigger level, hold BOOT to relearn it.
Audio read failures restart learning to avoid triggering from stale audio.

Tune `NOISE_LEARN_MS`, `EVENT_ABOVE_FLOOR_DB`, `EVENT_MIN_DBFS`,
`EVENT_MIN_DURATION_MS`, `EVENT_QUIET_REARM_MS`, and the other constants in
`config.h`. For brief impulses, lower the confirmation duration to 32 ms.
Detection compares unsmoothed block RMS against the floor; the large OLED number
retains its 125 ms smoothing. Floor and alert comparisons work without SPL calibration.

## ESP-NOW sender and receiver

The sender initially broadcasts to `FF:FF:FF:FF:FF:FF` on **Wi-Fi channel 6**.
Flash the [receiver example](../SoundEventReceiver/README.md) onto the second
ESP32-S3 and open its serial monitor at 115200 baud. Both devices must use the
same `SOUND_RADIO_CHANNEL` from `../../include/sound_radio_config.h`.

Each alert is a versioned 32-byte packet defined in
`../../include/sound_event_protocol.h`, containing a random sender boot ID, event
sequence, uptime, level and noise floor in centi-dBFS, and confirmation duration.
Levels on the wire always use dBFS, even if the OLED has SPL calibration enabled.
The receiver derives the rise above the floor and prints one line per event.

Three identical copies are sent about 100 ms apart; the receiver deduplicates
them by source MAC, boot ID, and sequence, tracking up to eight senders.
Broadcast is unencrypted and has no receiver acknowledgement. A successful local
send queue result does not prove reception; repeated copies reduce packet-loss
risk but delivery is best effort. No audio samples are transmitted.

To target one board, copy its printed **station MAC** into `ESPNOW_DESTINATION`
in `config.h` and rebuild the sender. The receiver works with either broadcast or
unencrypted unicast. Leave both devices on the fixed channel; joining an access
point on a different channel would require changing this channel arrangement.

## Units and optional calibration

Default readings are **dBFS**, relative to the digital PCM full-scale amplitude.
Louder sound moves the value toward zero; digital silence is shown as −120.0.
A full-scale sine wave is approximately −3.0 dBFS with this RMS convention.
The bar stops at −90 dBFS even if the number is lower.

This is initially a relative sound meter. For an estimated absolute reading:

1. Place a reference sound level meter beside this microphone in a steady sound
   field. Note the reference dB SPL and this firmware's steady dBFS reading.
2. Set `SPL_OFFSET_DB` in `config.h` to **reference SPL minus measured dBFS**.
   Example: 65 dB SPL and −35 dBFS gives an offset of 100 dB.
3. Set `SPL_CALIBRATED = true`, rebuild, and upload. The OLED then labels readings
   `SPL*`; the displayed level and NF include the offset. The bar retains its digital
   full-scale range.

Calibration is specific to the board, enclosure, and microphone settings.
The 16 kHz capture and DC removal do not implement A/C weighting or standardized
sound-level-meter time/frequency responses. `SPL*` is an unweighted estimate,
not a certified dBA reading. For reference-meter comparisons, use matching
unweighted settings where available; a single offset cannot correct frequency
response differences.

## Audio calculation check

From the repository root, the PCM calculation can be tested without hardware:

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/sound_meter_level_test.cpp -o /tmp/sound_meter_level_test
/tmp/sound_meter_level_test
g++ -std=c++11 -Wall -Wextra -Werror tests/noise_detector_test.cpp -o /tmp/noise_detector_test
/tmp/noise_detector_test
```

The tests check PCM normalization, noise learning, threshold confirmation,
hysteresis, cooldown, floor adaptation, sustained-event suppression, and packet
validation. Firmware compilation checks both microphone code paths and the
receiver. Acoustic response and over-the-air delivery still need hardware checks.
