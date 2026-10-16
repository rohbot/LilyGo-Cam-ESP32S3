#pragma once

#include <stddef.h>
#include <math.h>

struct AudioLevel {
    float power;  // Mean square, normalized to the PCM full-scale amplitude.
    float peak;   // Absolute raw sample peak, before DC removal (for clipping).
};

// Normalize before subtracting the mean so signed PCM extremes cannot overflow.
// I2S 24-bit samples are left aligned in 32-bit slots; use fullScale=2^31.
template <typename Sample>
AudioLevel calculateLevel(const Sample *data, size_t count, size_t stride,
                          double fullScale)
{
    if (count == 0) {
        return {0.0f, 0.0f};
    }
    double sum = 0.0;
    double peak = 0.0;
    for (size_t i = 0; i < count; ++i) {
        const double sample = data[i * stride] / fullScale;
        sum += sample;
        peak = fmax(peak, fabs(sample));
    }
    const double mean = sum / count;
    double squareSum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        const double centered = data[i * stride] / fullScale - mean;
        squareSum += centered * centered;
    }
    return {static_cast<float>(squareSum / count), static_cast<float>(peak)};
}

inline float powerToDbfs(float power)
{
    // Finite floor for digital silence. A full-scale sine reads about -3 dBFS.
    return 10.0f * log10f(fmaxf(power, 1.0e-12f));
}
