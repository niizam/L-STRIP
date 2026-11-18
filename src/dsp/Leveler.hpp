// SPDX-License-Identifier: MIT
// Auto Level: a slow "vocal rider" that sits in front of the compressor.
//
// It measures the loudness of the voice over about 400 ms and moves a gain
// towards the amount needed to bring that loudness to a fixed target, so quiet
// passages are lifted and loud ones eased before the compressor sees them.
//
//   * A fast detector decides whether the voice is present.  While it is not
//     (a pause, room noise) the loudness measurement and the gain are both
//     frozen, so the rider never pumps room noise up between phrases and does
//     not "see" the end of a phrase as a quiet passage.
//   * When the voice first arrives (after a reset, or after the amount was
//     raised from 0) the rider stays at unity for the first 100 ms of
//     continuous speech and starts its loudness measurement from the
//     average level of that window.  Without that, the measurement would
//     start from the first few quiet-looking samples and the gain would swell
//     before settling.
//   * Amount sets how far it may ride (up to +/-12 dB at 100 %).
//   * It moves slowly (about a second to lift, a quarter of a second to ease),
//     so it works on phrases, not on syllables.  Fast level control is the
//     compressor's job.
//
// With the amount at 0 the module is bypassed.  Switching it off while it is
// applying gain glides back to unity first, so there is no click.  Zero latency.
#pragma once
#include "Common.hpp"

namespace lstrip::dsp {

class Leveler {
public:
    static constexpr float kTargetDb = -20.f;     // desired loudness (RMS, dBFS)
    static constexpr float kFloorDb = -50.f;      // below this the voice counts as absent
    static constexpr float kMaxRideDb = 12.f;

    void prepare(double sr) {
        meanC_ = timeCoef(0.400f, sr);
        fastC_ = timeCoef(0.030f, sr);
        primeN_ = static_cast<int>(0.100 * sr);
        upC_ = timeCoef(1.000f, sr);
        downC_ = timeCoef(0.250f, sr);
        exitC_ = timeCoef(0.100f, sr);
        reset();
    }
    void reset() { ms_ = 0.f; fast_ = 0.f; primed_ = false; acc_ = 0.0; run_ = 0; gainDb_ = 0.f; }
    void set(float amount01) { amount_ = clampf(amount01, 0.f, 1.f); }
    // Gain currently applied, in dB (for tests and meters).
    float gainDb() const { return gainDb_; }

    void process(float* l, float* r, int n) {
        if (amount_ <= 0.f && gainDb_ == 0.f) { ms_ = 0.f; fast_ = 0.f; primed_ = false; acc_ = 0.0; run_ = 0; return; }
        const float range = kMaxRideDb * amount_;
        const float floorMs = dbToLin(kFloorDb) * dbToLin(kFloorDb);
        for (int i = 0; i < n; ++i) {
            const float p = 0.5f * (l[i] * l[i] + r[i] * r[i]);
            fast_ = p + fastC_ * (fast_ - p);
            const bool present = fast_ > floorMs;                     // is the voice there right now?
            if (present) {
                if (primed_) ms_ = p + meanC_ * (ms_ - p);
                else {                                                // first words: average 100 ms before trusting it
                    acc_ += static_cast<double>(p);
                    if (++run_ >= primeN_) { ms_ = static_cast<float>(acc_ / run_); primed_ = true; }
                }
            } else if (!primed_) { acc_ = 0.0; run_ = 0; }            // presence must be continuous
            float desired = gainDb_;                                  // hold by default
            if (amount_ <= 0.f) desired = 0.f;
            else if (primed_ && present && ms_ > floorMs) desired = clampf(kTargetDb - 10.f * std::log10(ms_), -range, range);
            const float c = amount_ <= 0.f ? exitC_ : (desired > gainDb_ ? upC_ : downC_);
            gainDb_ = desired + c * (gainDb_ - desired);
            if (amount_ <= 0.f && std::fabs(gainDb_) < 0.005f) gainDb_ = 0.f;
            const float g = std::exp(gainDb_ * 0.11512925f);          // dB -> linear
            l[i] *= g; r[i] *= g;
        }
    }

private:
    float amount_ = 0.f, ms_ = 0.f, fast_ = 0.f, gainDb_ = 0.f;
    bool primed_ = false;
    double acc_ = 0.0;
    int run_ = 0, primeN_ = 4800;
    float meanC_ = 0.f, fastC_ = 0.f, upC_ = 0.f, downC_ = 0.f, exitC_ = 0.f;
};

} // namespace lstrip::dsp
