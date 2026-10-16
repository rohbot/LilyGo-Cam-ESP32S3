#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "config.h"

class NoiseDetector {
public:
    void reset()
    {
        memset(histogram_, 0, sizeof(histogram_));
        learnedMs_ = observations_ = candidateMs_ = quietMs_ = 0;
        ready_ = active_ = sent_ = false;
        floor_ = -120.0f;
    }

    // Duration is valid captured audio time, not time spent drawing or sending.
    bool update(float dbfs, uint32_t durationMs)
    {
        elapsedMs_ += durationMs;
        if (!ready_) {
            const int bin = static_cast<int>(fminf(0.0f, fmaxf(-120.0f, dbfs)) + 120.0f);
            ++histogram_[bin];
            ++observations_;
            learnedMs_ += durationMs;
            if (learnedMs_ >= NOISE_LEARN_MS) {
                // Lower fifth: brief sounds during learning don't set the floor.
                const uint32_t target = (observations_ + 4) / 5;
                uint32_t cumulative = 0;
                for (int i = 0; i <= 120; ++i) {
                    cumulative += histogram_[i];
                    if (cumulative >= target) {
                        floor_ = static_cast<float>(i - 120);
                        break;
                    }
                }
                ready_ = true;
            }
            return false;
        }

        const float trigger = fmaxf(floor_ + EVENT_ABOVE_FLOOR_DB, EVENT_MIN_DBFS);
        const float release = fmaxf(floor_ + EVENT_RELEASE_ABOVE_FLOOR_DB,
                                   EVENT_MIN_DBFS - (EVENT_ABOVE_FLOOR_DB - EVENT_RELEASE_ABOVE_FLOOR_DB));
        if (active_) {
            // Freeze the floor throughout an event so sustained noise isn't learned.
            quietMs_ = dbfs <= release ? quietMs_ + durationMs : 0;
            if (quietMs_ >= EVENT_QUIET_REARM_MS) {
                active_ = false;
                quietMs_ = 0;
            }
            return false;
        }

        if (dbfs >= trigger) {
            candidateMs_ += durationMs;
            if (candidateMs_ >= EVENT_MIN_DURATION_MS &&
                (!sent_ || elapsedMs_ - lastEventMs_ >= EVENT_COOLDOWN_MS)) {
                active_ = sent_ = true;
                lastEventMs_ = elapsedMs_;
                lastDurationMs_ = candidateMs_;
                candidateMs_ = 0;
                return true;
            }
            return false;  // Freeze while checking a candidate, including cooldown.
        }
        candidateMs_ = 0;
        const float seconds = dbfs < floor_ ? NOISE_FALL_SECONDS : NOISE_RISE_SECONDS;
        const float alpha = 1.0f - expf(-static_cast<float>(durationMs) / (1000.0f * seconds));
        floor_ += alpha * (dbfs - floor_);
        return false;
    }

    bool ready() const { return ready_; }
    bool active() const { return active_; }
    float floorDbfs() const { return floor_; }
    uint32_t eventDurationMs() const { return lastDurationMs_; }

private:
    uint32_t histogram_[121] = {};
    uint32_t learnedMs_ = 0, observations_ = 0, elapsedMs_ = 0, lastEventMs_ = 0;
    uint32_t candidateMs_ = 0, quietMs_ = 0, lastDurationMs_ = 0;
    float floor_ = -120.0f;
    bool ready_ = false, active_ = false, sent_ = false;
};
