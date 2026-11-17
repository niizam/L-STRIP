// SPDX-License-Identifier: MIT
// Zero-latency split-band de-esser.  Sibilance is detected as the ratio of the
// 'sss' band energy to the vocal body energy, then the band is attenuated
// dynamically.  LISTEN outputs the removed signal.
#pragma once
#include "Common.hpp"

namespace lstrip::dsp {

class DeEsser {
public:
    void prepare(double sr) {
        sr_ = sr;
        envBand_.setTimes(0.0004f, 0.015f, sr);
        envBody_.setTimes(0.002f, 0.040f, sr);
        gainAtt_ = timeCoef(0.0005f, sr);
        gainRel_ = timeCoef(0.040f, sr);
        reset();
        set(6500.f, 0.4f, 12.f, false);
    }
    void reset() {
        grDb_ = 0.f;
        for (auto& b : bp_) b.reset();
        for (auto& b : body_) b.reset();
        envBand_.env = envBody_.env = 0.f;
    }
    void set(float freq, float amount, float rangeDb, bool listen) {
        amount_ = amount; range_ = rangeDb; listen_ = listen;
        const BiquadCoef bp = BiquadCoef::bandpass(freq, 1.4, sr_);
        bp_[0].set(bp); bp_[1].set(bp);
        const BiquadCoef lp = BiquadCoef::lowpass(2500.0, 0.707, sr_);
        body_[0].set(lp); body_[1].set(lp);
        // sibilance threshold (band-vs-body, dB) moves with amount
        thr_ = lerpf(2.f, -14.f, amount);
    }

    float process(float* l, float* r, int n) {
        const bool active = amount_ > 0.001f && range_ > 0.01f;
        if (!active && !listen_) { grDb_ = 0.f; return 0.f; }
        float grAcc = 0.f;
        for (int i = 0; i < n; ++i) {
            const float b0 = bp_[0].process(l[i]), b1 = bp_[1].process(r[i]);
            if (!active) { l[i] = listen_ ? 0.f : l[i]; r[i] = listen_ ? 0.f : r[i]; continue; }
            const float lo = std::max(std::fabs(body_[0].process(l[i])), std::fabs(body_[1].process(r[i])));
            const float eb = envBand_.process(std::max(std::fabs(b0), std::fabs(b1)));
            const float eo = envBody_.process(lo);
            float tgt = 0.f;
            if (eb > 1e-4f) { // ignore sub -80 dB debris
                const float rel = linToDb(eb) - linToDb(eo + 1e-6f);
                tgt = clampf((rel - thr_) * 0.8f, 0.f, range_);
            }
            const float c = tgt > grDb_ ? gainAtt_ : gainRel_;
            grDb_ = tgt + c * (grDb_ - tgt);
            grAcc += grDb_;
            const float cut = 1.f - dbToLin(-grDb_); // fraction of band removed
            const float rm0 = b0 * cut, rm1 = b1 * cut;
            if (listen_) { l[i] = rm0; r[i] = rm1; }
            else { l[i] -= rm0; r[i] -= rm1; }
        }
        return n > 0 ? grAcc / static_cast<float>(n) : 0.f;
    }

private:
    double sr_ = 48000;
    Biquad bp_[2], body_[2];
    EnvFollower envBand_, envBody_;
    float gainAtt_ = 0, gainRel_ = 0, grDb_ = 0, amount_ = 0.4f, range_ = 12, thr_ = 0;
    bool listen_ = false;
};

} // namespace lstrip::dsp
