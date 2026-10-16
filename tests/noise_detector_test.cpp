#include "../examples/SoundMeter/noise_detector.h"
#include "../include/sound_event_protocol.h"
#include <assert.h>
#include <stdio.h>

void learn(NoiseDetector &detector, float background)
{
    detector.reset();
    for (unsigned i = 0; i < (NOISE_LEARN_MS + 31) / 32; ++i) {
        // Intermittent sounds must not contaminate the starting floor.
        assert(!detector.update(i % 10 == 0 ? -20.0f : background, 32));
    }
    assert(detector.ready());
    assert(fabsf(detector.floorDbfs() - background) < 0.01f);
}

int main()
{
    NoiseDetector detector;
    learn(detector, -60.0f);
    assert(!detector.update(-40.0f, 32));  // A single short impulse is rejected.
    assert(!detector.update(-60.0f, 32));
    assert(!detector.update(-40.0f, 32));
    assert(detector.update(-40.0f, 32));
    assert(detector.eventDurationMs() == 64);
    assert(detector.active());
    for (int i = 0; i < 500; ++i) {
        assert(!detector.update(-40.0f, 32));  // Sustained sound: one event only.
    }
    assert(fabsf(detector.floorDbfs() + 60.0f) < 0.01f);
    for (int i = 0; i < 20; ++i) {
        assert(!detector.update(-52.0f, 32));  // Hysteresis: 8 dB above won't rearm.
    }
    assert(detector.active());
    for (int i = 0; i < 16; ++i) {
        assert(!detector.update(-60.0f, 32));
    }
    assert(!detector.active());
    assert(!detector.update(-40.0f, 32));
    assert(detector.update(-40.0f, 32));

    learn(detector, -60.0f);
    assert(!detector.update(-40.0f, 32));
    assert(detector.update(-40.0f, 32));
    for (int i = 0; i < 16; ++i) {
        assert(!detector.update(-60.0f, 32));
    }
    for (int i = 0; i < 10; ++i) {
        assert(!detector.update(-40.0f, 32));  // Cooldown suppresses a fast repeat.
    }

    learn(detector, -60.0f);
    for (int i = 0; i < 938; ++i) {
        assert(!detector.update(-55.0f, 32));
    }
    assert(detector.floorDbfs() > -57.0f && detector.floorDbfs() < -56.0f);
    for (int i = 0; i < 94; ++i) {
        assert(!detector.update(-70.0f, 32));
    }
    assert(detector.floorDbfs() < -64.0f);  // Follows a quieter background faster.

    learn(detector, -120.0f);
    for (int i = 0; i < 20; ++i) {
        assert(!detector.update(-100.0f, 32));  // Ignore tiny signals near silence.
    }
    assert(!detector.update(-70.0f, 32));
    assert(detector.update(-70.0f, 32));
    detector.reset();
    assert(!detector.ready() && !detector.active());
    assert(!detector.update(-20.0f, 32));  // No alert during relearning.

    SoundEventPacket original;
    original.sequence = 1;
    original.levelCentiDbfs = -4000;
    original.floorCentiDbfs = -6000;
    SoundEventPacket decoded;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&original);
    assert(decodeSoundEvent(bytes, sizeof(original), decoded));
    assert(decoded.levelCentiDbfs - decoded.floorCentiDbfs == 2000);
    assert(!decodeSoundEvent(bytes, sizeof(original) - 1, decoded));
    original.version = 2;
    assert(!decodeSoundEvent(bytes, sizeof(original), decoded));
    original.version = 1;
    original.levelCentiDbfs = 100;
    assert(!decodeSoundEvent(bytes, sizeof(original), decoded));
    puts("Noise detector and event packet checks passed");
}
