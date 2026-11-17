// SPDX-License-Identifier: MIT
// Zero-latency feed-forward soft-knee compressor with tone tilt and parallel mix.
#pragma once
#include "Common.hpp"

namespace lstrip::dsp {

struct CompSettings {
    float thresh = -18, makeup = 4.5f, tone = 0, mix = 1;
    int mode = 0;   // 0 fast, 1 slow, 2 auto
    int style = 0;  // 0 clean, 1 punch, 2 thick
};

// Style-dependent static curve (shared with the UI so the curve display is exact).
struct CompCurve { float ratio, kneeDb; };
inline CompCurve compCurveFor(int style) {
    switch (style) {
    case 1: return {4.0f, 3.0f};
    case 2: return {8.0f, 6.0f};
    default: return {2.5f, 8.0f};
    }
}
// Output level (dB) for an input level (dB), before makeup.
inline float compStaticCurve(float inDb, float thresh, float ratio, float knee) {
    const float over = inDb - thresh;
    if (2.f * over < -knee) return inDb;
    if (2.f * std::fabs(over) <= knee) {
        const float t = over + knee * 0.5f;
        return inDb + (1.f / ratio - 1.f) * t * t / (2.f * knee);
    }
    return thresh + over / ratio;
}

class Compressor {
public:
    void prepare(double sr) {
        sr_ = sr; reset(); set(CompSettings{});
    }
    void reset() { grDb_ = 0.f; rms_ = 0.f; peak_ = 0.f; for (auto& b : tone_) b.reset(); }
    void set(const CompSettings& s) {
        s_ = s;
        curve_ = compCurveFor(s.style);
        float att = 0.003f, rel = 0.08f;
        if (s.mode == 1) { att = 0.025f; rel = 0.30f; }
        else if (s.mode == 2) { att = 0.008f; rel = 0.25f; }
        if (s.style == 1) att *= 0.5f;       // punch: quicker
        attC_ = timeCoef(att, sr_);
        relC_ = timeCoef(rel, sr_);
        relFast_ = timeCoef(0.05f, sr_);
        relSlow_ = timeCoef(0.5f, sr_);
        rmsC_ = timeCoef(0.005f, sr_);
        const float t = s.tone * 0.5f;
        tone_[0].set(BiquadCoef::lowShelf(400, 0.7, -t, sr_));
        tone_[1].set(BiquadCoef::highShelf(3000, 0.7, t, sr_));
        tone_[2].set(BiquadCoef::lowShelf(400, 0.7, -t, sr_));
        tone_[3].set(BiquadCoef::highShelf(3000, 0.7, t, sr_));
        toneOn_ = std::fabs(s.tone) > 0.05f;
    }
    CompCurve curve() const { return curve_; }

    // Returns average gain reduction (dB, >= 0) over the block.
    float process(float* l, float* r, int n) {
        const float makeupLin = dbToLin(s_.makeup);
        float grAcc = 0.f;
        for (int i = 0; i < n; ++i) {
            const float dry0 = l[i], dry1 = r[i];
            const float a = std::max(std::fabs(dry0), std::fabs(dry1));
            // detector: clean = RMS-ish, punch/thick = peak-leaning
            rms_ = a * a + rmsC_ * (rms_ - a * a);
            const float rmsA = std::sqrt(rms_);
            const float det = s_.style == 0 ? rmsA : std::max(rmsA, a * 0.9f);
            const float inDb = linToDb(det);
            const float outDb = compStaticCurve(inDb, s_.thresh, curve_.ratio, curve_.kneeDb);
            const float tgt = std::max(0.f, inDb - outDb); // desired reduction (>=0)
            float rc = relC_;
            if (s_.mode == 2) { // auto: bigger reductions release faster
                const float k = clampf(grDb_ / 12.f, 0.f, 1.f);
                rc = lerpf(relSlow_, relFast_, k);
            }
            const float c = tgt > grDb_ ? attC_ : rc;
            grDb_ = tgt + c * (grDb_ - tgt);
            grAcc += grDb_;
            const float g = dbToLin(-grDb_) * makeupLin;
            float w0 = dry0 * g, w1 = dry1 * g;
            if (toneOn_) {
                w0 = tone_[1].process(tone_[0].process(w0));
                w1 = tone_[3].process(tone_[2].process(w1));
            }
            l[i] = dry0 + (w0 - dry0) * s_.mix;
            r[i] = dry1 + (w1 - dry1) * s_.mix;
        }
        return n > 0 ? grAcc / static_cast<float>(n) : 0.f;
    }

private:
    double sr_ = 48000;
    CompSettings s_;
    CompCurve curve_{2.5f, 8.f};
    float attC_ = 0, relC_ = 0, relFast_ = 0, relSlow_ = 0, rmsC_ = 0;
    float grDb_ = 0, rms_ = 0, peak_ = 0;
    Biquad tone_[4];
    bool toneOn_ = false;
};

} // namespace lstrip::dsp
