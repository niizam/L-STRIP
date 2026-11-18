// SPDX-License-Identifier: MIT
// 5-section zero-latency EQ: low-cut + low shelf + mid bell + high bell + air shelf,
// followed by an optional 24 dB/oct high-cut (off while at 20 kHz).
#pragma once
#include "Common.hpp"

namespace lstrip::dsp {

struct EqSettings {
    float low = 2, mid = -1.5f, high = 1, air = 3, lowCut = 90;
    int style = 0; // 0 analog, 1 modern, 2 broadcast
    float highCut = 20000;   // Hz; >= kHighCutOff means off
};

inline constexpr float kHighCutOff = 19900.f;

struct EqVoicing { double lowF, midF, midQ, highF, highQ, airF; };
inline EqVoicing eqVoicingFor(int style) {
    switch (style) {
    case 1: return {80, 600, 1.0, 4000, 1.0, 12000};
    case 2: return {120, 350, 1.2, 5000, 0.9, 14000};
    default: return {100, 400, 0.7, 3000, 0.7, 10000};
    }
}

// Section coefficients (index 0 = low-cut HP Q=0.707; 1..4 = bands). Shared with the UI.
inline void eqDesign(const EqSettings& s, double sr, BiquadCoef out[5]) {
    const EqVoicing v = eqVoicingFor(s.style);
    out[0] = BiquadCoef::highpass(std::max(1.0f, s.lowCut), 0.707, sr);
    out[1] = BiquadCoef::lowShelf(v.lowF, 0.7, s.low, sr);
    out[2] = BiquadCoef::peaking(v.midF, v.midQ, s.mid, sr);
    out[3] = BiquadCoef::peaking(v.highF, v.highQ, s.high, sr);
    out[4] = BiquadCoef::highShelf(v.airF, 0.7, s.air, sr);
}

// The two sections of the high-cut (4th-order Butterworth).  Shared with the UI curve.
inline void eqHighCutDesign(const EqSettings& s, double sr, BiquadCoef out[2]) {
    out[0] = BiquadCoef::lowpass(s.highCut, 0.5412, sr);
    out[1] = BiquadCoef::lowpass(s.highCut, 1.3066, sr);
}

class Equalizer {
public:
    void prepare(double sr) { sr_ = sr; reset(); set(EqSettings{}); }
    void reset() { for (auto& c : ch_) for (auto& b : c) b.reset(); for (auto& c : hc_) for (auto& b : c) b.reset(); }
    void set(const EqSettings& s) {
        BiquadCoef c[5];
        eqDesign(s, sr_, c);
        lowCutOn_ = s.lowCut > 21.f;
        for (auto& chan : ch_) for (int i = 0; i < 5; ++i) chan[i].set(c[i]);
        const bool hc = s.highCut < kHighCutOff;
        if (hc) {
            BiquadCoef h[2];
            eqHighCutDesign(s, sr_, h);
            for (auto& chan : hc_) for (int i = 0; i < 2; ++i) chan[i].set(h[i]);
            if (!highCutOn_) for (auto& chan : hc_) for (auto& b : chan) b.reset();
        }
        highCutOn_ = hc;
    }
    void process(float* l, float* r, int n) {
        float* p[2] = {l, r};
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i) {
                float x = p[ch][i];
                if (lowCutOn_) x = ch_[ch][0].process(x);
                for (int s = 1; s < 5; ++s) x = ch_[ch][s].process(x);
                if (highCutOn_) x = hc_[ch][1].process(hc_[ch][0].process(x));
                p[ch][i] = x;
            }
    }
private:
    double sr_ = 48000;
    Biquad ch_[2][5], hc_[2][2];
    bool lowCutOn_ = true, highCutOn_ = false;
};

} // namespace lstrip::dsp
