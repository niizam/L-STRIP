// SPDX-License-Identifier: MIT
// Zero-latency noise gate: hysteresis + hold + smoothed gain.
#pragma once
#include "Common.hpp"

namespace lstrip::dsp {

class Gate {
public:
    void prepare(double sr) {
        sr_ = sr;
        det_.setTimes(0.0005f, 0.020f, sr);
        openCoef_ = timeCoef(0.0007f, sr);
        closeCoef_ = timeCoef(0.100f, sr);
        holdSamples_ = static_cast<int>(0.060 * sr);
        reset();
    }
    void reset() { gain_ = 0.f; open_ = false; hold_ = 0; det_.env = 0.f; }
    void setThreshold(float db) { off_ = db <= -79.5f; thr_ = db; }

    // Returns current reduction in dB (0 = fully open).
    float process(float* l, float* r, int n) {
        if (off_) { gain_ = 1.f; return 0.f; }
        const float openLin = dbToLin(thr_), closeLin = dbToLin(thr_ - 4.f);
        const float floorLin = dbToLin(-70.f);
        for (int i = 0; i < n; ++i) {
            const float e = det_.process(std::max(std::fabs(l[i]), std::fabs(r[i])));
            if (e >= openLin) { open_ = true; hold_ = holdSamples_; }
            else if (open_ && e < closeLin) { if (--hold_ <= 0) open_ = false; }
            const float tgt = open_ ? 1.f : floorLin;
            const float c = tgt > gain_ ? openCoef_ : closeCoef_;
            gain_ = tgt + c * (gain_ - tgt);
            l[i] *= gain_; r[i] *= gain_;
        }
        return -linToDb(gain_);
    }

private:
    double sr_ = 48000;
    EnvFollower det_;
    float openCoef_ = 0, closeCoef_ = 0, gain_ = 0, thr_ = -60;
    int holdSamples_ = 0, hold_ = 0;
    bool open_ = false, off_ = false;
};

} // namespace lstrip::dsp
