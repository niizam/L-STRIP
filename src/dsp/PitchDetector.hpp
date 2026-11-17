// SPDX-License-Identifier: MIT
// Causal YIN pitch detector running on a decimated (~12 kHz) copy of the input.
// It only looks at past samples, so it adds no algorithmic latency of its own;
// the estimate simply describes the voice a few milliseconds ago.
#pragma once
#include "Common.hpp"

namespace lstrip::dsp {

struct PitchEstimate {
    float hz = 0.f;       // 0 when unvoiced
    float confidence = 0.f;
    bool voiced = false;
};

class PitchDetector {
public:
    static constexpr int kWindow = 200;        // decimated samples integrated by YIN
    static constexpr int kHop = 32;            // decimated samples between estimates
    static constexpr float kMinHz = 70.f, kMaxHz = 900.f;

    void prepare(double sr) {
        decim_ = std::max(1, static_cast<int>(std::lround(sr / 12000.0)));
        srDec_ = static_cast<float>(sr / decim_);
        tauMin_ = std::max(2, static_cast<int>(srDec_ / kMaxHz));
        tauMax_ = static_cast<int>(srDec_ / kMinHz);
        histSize_ = 1;
        while (histSize_ < kWindow + tauMax_ + 8) histSize_ <<= 1;
        hist_.assign(histSize_, 0.f);
        diff_.assign(tauMax_ + 2, 0.f);
        cmnd_.assign(tauMax_ + 2, 0.f);
        const BiquadCoef lp = BiquadCoef::lowpass(srDec_ * 0.28, 0.5412, sr); // 4th-order Butterworth
        const BiquadCoef lp2 = BiquadCoef::lowpass(srDec_ * 0.28, 1.3065, sr);
        aa_[0].set(lp); aa_[1].set(lp2);
        reset();
    }
    void reset() {
        std::fill(hist_.begin(), hist_.end(), 0.f);
        aa_[0].reset(); aa_[1].reset();
        wr_ = 0; phase_ = 0; sinceHop_ = 0; last_ = PitchEstimate{};
        for (auto& h : recent_) h = 0.f;
    }

    // Feed one full-rate mono sample.  Returns true when a new estimate is ready.
    inline bool push(float x, PitchEstimate& out) {
        const float y = aa_[1].process(aa_[0].process(x));
        if (++phase_ < decim_) return false;
        phase_ = 0;
        wr_ = (wr_ + 1) & (histSize_ - 1);
        hist_[wr_] = y;
        if (++sinceHop_ < kHop) return false;
        sinceHop_ = 0;
        last_ = analyse();
        out = last_;
        return true;
    }
    const PitchEstimate& last() const { return last_; }

private:
    inline float h(int back) const { return hist_[(wr_ - back) & (histSize_ - 1)]; }

    PitchEstimate analyse() {
        PitchEstimate est;
        // energy gate
        double e = 0.0;
        for (int j = 0; j < kWindow; ++j) { const float v = h(j); e += v * v; }
        const float rms = static_cast<float>(std::sqrt(e / kWindow));
        if (rms < 0.0015f) { push3(0.f); return est; } // ~ -56 dBFS

        // difference function d(tau) = sum_j (x[j] - x[j+tau])^2
        for (int tau = 1; tau <= tauMax_; ++tau) {
            double d = 0.0;
            for (int j = 0; j < kWindow; ++j) { const float a = h(j) - h(j + tau); d += static_cast<double>(a) * a; }
            diff_[tau] = static_cast<float>(d);
        }
        // cumulative mean normalised difference
        cmnd_[0] = 1.f;
        double run = 0.0;
        for (int tau = 1; tau <= tauMax_; ++tau) {
            run += diff_[tau];
            cmnd_[tau] = run > 1e-12 ? static_cast<float>(diff_[tau] * tau / run) : 1.f;
        }
        int best = -1;
        for (int tau = tauMin_; tau < tauMax_; ++tau) {
            if (cmnd_[tau] < 0.15f) {
                while (tau + 1 < tauMax_ && cmnd_[tau + 1] < cmnd_[tau]) ++tau;
                best = tau; break;
            }
        }
        float minv = 1.f;
        if (best < 0) { // fall back to global minimum
            for (int tau = tauMin_; tau < tauMax_; ++tau)
                if (cmnd_[tau] < minv) { minv = cmnd_[tau]; best = tau; }
            if (best < 0 || minv > 0.30f) { push3(0.f); return est; }
        } else minv = cmnd_[best];
        // parabolic interpolation
        float tauF = static_cast<float>(best);
        if (best > 1 && best < tauMax_) {
            const float a = cmnd_[best - 1], b = cmnd_[best], c = cmnd_[best + 1];
            const float den = a - 2.f * b + c;
            if (std::fabs(den) > 1e-9f) tauF += 0.5f * (a - c) / den;
        }
        est.hz = srDec_ / tauF;
        est.confidence = 1.f - minv;
        est.voiced = est.hz >= kMinHz && est.hz <= kMaxHz;
        // 3-point median on log-pitch kills isolated octave errors
        const float lg = std::log2(est.hz);
        push3(est.voiced ? lg : 0.f);
        if (est.voiced) {
            float a = recent_[0], b = recent_[1], c = recent_[2];
            if (a > 0.f && b > 0.f && c > 0.f) {
                const float med = std::max(std::min(a, b), std::min(std::max(a, b), c));
                est.hz = std::exp2(med);
            }
        }
        return est;
    }
    void push3(float v) { recent_[0] = recent_[1]; recent_[1] = recent_[2]; recent_[2] = v; }

    int decim_ = 4, tauMin_ = 8, tauMax_ = 170, histSize_ = 512, phase_ = 0, sinceHop_ = 0;
    size_t wr_ = 0;
    float srDec_ = 12000.f;
    std::vector<float> hist_, diff_, cmnd_;
    Biquad aa_[2];
    float recent_[3] = {0, 0, 0};
    PitchEstimate last_;
};

} // namespace lstrip::dsp
