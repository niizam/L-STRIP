// SPDX-License-Identifier: MIT
// Plosive tamer: softens the low-frequency thump of "p" and "b" sounds.
//
// A plosive is a burst of energy below ~100 Hz with little else going on above
// it.  A sung or spoken low note also has energy down there, but its upper
// harmonics are strong too.  So the detector compares the level of the low
// band with the level of the "body" of the voice (above 300 Hz): only when the
// low band clearly dominates is the signal blended towards a steep 150 Hz
// high-pass, and only for as long as that lasts.
//
//     out = x + act * (highpass(x) - x)        act in 0..1
//
// With act == 0 the output is the input bit for bit, and with the amount at 0
// the module does not run at all.  Zero latency.
#pragma once
#include "Common.hpp"

namespace lstrip::dsp {

class PlosiveTamer {
public:
    void prepare(double sr) {
        sr_ = sr;
        lowEnv_.setTimes(0.001f, 0.025f, sr);
        bodyEnv_.setTimes(0.004f, 0.060f, sr);
        actAtt_ = timeCoef(0.0015f, sr);
        actRel_ = timeCoef(0.025f, sr);
        lowDet_.set(BiquadCoef::lowpass(90.0, 0.707, sr));
        bodyDet_.set(BiquadCoef::highpass(300.0, 0.707, sr));
        for (auto& c : hp_) {
            c[0].set(BiquadCoef::highpass(150.0, 0.5412, sr));   // 4th-order Butterworth, as two sections
            c[1].set(BiquadCoef::highpass(150.0, 1.3066, sr));
        }
        reset();
    }
    void reset() {
        lowDet_.reset(); bodyDet_.reset();
        lowEnv_.env = bodyEnv_.env = 0.f;
        for (auto& c : hp_) for (auto& b : c) b.reset();
        act_ = 0.f; active_ = false;
    }
    // amount01: 0 = off, 1 = strongest.
    void set(float amount01) { amount_ = clampf(amount01, 0.f, 1.f); }
    // Most recent blend amount (0..1), for tests and meters.
    float activity() const { return act_; }

    void process(float* l, float* r, int n) {
        if (amount_ <= 0.f && act_ < 1e-4f) { act_ = 0.f; active_ = false; return; }
        if (!active_) { // starting from cold: do not judge on stale detector state
            lowDet_.reset(); bodyDet_.reset(); lowEnv_.env = bodyEnv_.env = 0.f;
            for (auto& c : hp_) for (auto& b : c) b.reset();
            active_ = true;
        }
        // More amount = more sensitive and a deeper blend.  Sung notes sit at a low/body ratio of about 1-2,
        // pops well above it; the threshold stays clear of the first at the highest amount.
        const float thr = lerpf(5.0f, 2.5f, amount_);
        const float cap = 0.5f + 0.5f * amount_;
        const float floorLin = dbToLin(-48.f);
        for (int i = 0; i < n; ++i) {
            const float m = 0.5f * (l[i] + r[i]);
            const float low = lowEnv_.process(lowDet_.process(m));
            const float body = bodyEnv_.process(bodyDet_.process(m));
            float tgt = 0.f;
            if (amount_ > 0.f && low > floorLin) tgt = clampf((low / (body + 1e-6f) - thr) / (0.6f * thr), 0.f, 1.f) * cap;
            act_ = tgt + (tgt > act_ ? actAtt_ : actRel_) * (act_ - tgt);
            float* p[2] = {l, r};
            for (int c = 0; c < 2; ++c) {
                const float x = p[c][i];
                const float hp = hp_[c][1].process(hp_[c][0].process(x));
                p[c][i] = x + act_ * (hp - x);
            }
        }
    }

private:
    double sr_ = 48000;
    float amount_ = 0.f, act_ = 0.f, actAtt_ = 0.f, actRel_ = 0.f;
    bool active_ = false;
    Biquad lowDet_, bodyDet_, hp_[2][2];
    EnvFollower lowEnv_, bodyEnv_;
};

} // namespace lstrip::dsp
