#include "../examples/SoundMeter/audio_level.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main()
{
    const int16_t silence[] = {0, 0, 0, 0};
    assert(calculateLevel(silence, 4, 1, 32768.0).power == 0.0f);
    assert(powerToDbfs(0.0f) == -120.0f);
    assert(calculateLevel(silence, 0, 1, 32768.0).power == 0.0f);

    const int16_t dc[] = {12345, 12345, 12345, 12345};
    assert(calculateLevel(dc, 4, 1, 32768.0).power == 0.0f);

    // Half-scale square wave: RMS=0.5, or -6.0206 dBFS, despite a DC offset.
    const int16_t square[] = {17384, -15384, 17384, -15384};
    const AudioLevel half = calculateLevel(square, 4, 1, 32768.0);
    assert(fabsf(half.power - 0.25f) < 1e-6f);
    assert(fabsf(powerToDbfs(half.power) + 6.0206f) < 0.001f);

    // Left-aligned 32-bit I2S extremes, with an inactive right slot.
    const int32_t stereo[] = {INT32_MIN, 0, INT32_MAX - 255, 0};
    const AudioLevel left = calculateLevel(stereo, 2, 2, 2147483648.0);
    const AudioLevel right = calculateLevel(stereo + 1, 2, 2, 2147483648.0);
    assert(fabsf(left.power - 1.0f) < 1e-6f);
    assert(left.peak == 1.0f);
    assert(right.power == 0.0f);

    int32_t sine[512];
    for (size_t i = 0; i < 512; ++i) {
        sine[i] = static_cast<int32_t>(2147483392.0 * sin(2.0 * 3.141592653589793 * i / 32.0));
    }
    const AudioLevel tone = calculateLevel(sine, 512, 1, 2147483648.0);
    assert(fabsf(powerToDbfs(tone.power) + 3.0103f) < 0.001f);
    puts("Sound meter PCM checks passed");
}
