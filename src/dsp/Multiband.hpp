// SPDX-License-Identifier: MIT
// OTT-style 3-band up/down compressor.  Linkwitz-Riley 4th-order crossovers
// (min-phase, zero latency), each band has simultaneous downward and upward
// compression in the dB domain.
#pragma once
#include "Common.hpp"

namespace lstrip::dsp {

struct MbSettings {
    float depth = 0.45f, time = 0.5f, upward = 0.35f, down = 0.6f, output = 0;
    float bandGain[3] = {0, 0, 0};
    int style = 0; // 0 smooth, 1 punch, 2 OTT
};

class Multiband {
public:
    static constexpr float kLowX = 150.f, kHighX = 3000.f;

    void prepare(double sr) { sr_ = sr; reset(); set(MbSettings{}); }
    void reset() {
        for (auto& c : ch_) {
            for (auto& b : c.lp1) b.reset();
            for (auto& b : c.hp1) b.reset();
            for (auto& b : c.lp2) b.reset();
            for (auto& b : c.hp2) b.reset();
            for (auto& b : c.ap) b.reset();
        }
        for (int b = 0; b < 3; ++b) { levelDb_[b] = -80.f; gainDb_[b] = 0.f; env_[b] = 0.f; }
    }
    void set(const MbSettings& s) {
        s_ = s;
        const BiquadCoef lp1 = BiquadCoef::lowpass(kLowX, 0.7071, sr_), hp1 = BiquadCoef::highpass(kLowX, 0.7071, sr_);
        const BiquadCoef lp2 = BiquadCoef::lowpass(kHighX, 0.7071, sr_), hp2 = BiquadCoef::highpass(kHighX, 0.7071, sr_);
        const BiquadCoef ap = BiquadCoef::allpass(kHighX, 0.7071, sr_);
        for (auto& c : ch_) {
            for (auto& b : c.lp1) b.set(lp1);
            for (auto& b : c.hp1) b.set(hp1);
            for (auto& b : c.lp2) b.set(lp2);
            for (auto& b : c.hp2) b.set(hp2);
            for (auto& b : c.ap) b.set(ap);
        }
        // Time: 0 -> fast, 1 -> slow.  Exponential mapping.
        const float att = 0.0005f * std::pow(40.f, s.time);   // 0.5 .. 20 ms
        const float rel = 0.020f * std::pow(20.f, s.time);    // 20 .. 400 ms
        attC_ = timeCoef(att, sr_);
        relC_ = timeCoef(rel, sr_);
        switch (s.style) {
        case 1: thrDown_ = -18; ratDown_ = 4; thrUp_ = -32; ratUp_ = 3; break;
        case 2: thrDown_ = -24; ratDown_ = 8; thrUp_ = -28; ratUp_ = 4; break;
        default: thrDown_ = -14; ratDown_ = 3; thrUp_ = -36; ratUp_ = 2; break;
        }
        outLin_ = dbToLin(s.output);
        for (int b = 0; b < 3; ++b) bandLin_[b] = dbToLin(s.bandGain[b]);
    }
    float bandLevelDb(int b) const { return levelDb_[b]; }

    void process(float* l, float* r, int n) {
        float* p[2] = {l, r};
        float peak[3] = {0, 0, 0};
        for (int i = 0; i < n; ++i) {
            float band[2][3];
            for (int ch = 0; ch < 2; ++ch) {
                Chan& c = ch_[ch];
                float x = p[ch][i];
                // LR4 = two cascaded Butterworth sections.
                float lo = c.lp1[1].process(c.lp1[0].process(x));
                float rest = c.hp1[1].process(c.hp1[0].process(x));
                float mid = c.lp2[1].process(c.lp2[0].process(rest));
                float hi = c.hp2[1].process(c.hp2[0].process(rest));
                lo = c.ap[1].process(c.ap[0].process(lo)); // keep phase coherent with mid/high split
                band[ch][0] = lo; band[ch][1] = mid; band[ch][2] = hi;
            }
            float out[2] = {0, 0};
            for (int b = 0; b < 3; ++b) {
                const float a = std::max(std::fabs(band[0][b]), std::fabs(band[1][b]));
                const float cc = a > env_[b] ? attC_ : relC_;
                env_[b] = a + cc * (env_[b] - a);
                const float lvl = linToDb(env_[b]);
                float g = 0.f;
                if (lvl > thrDown_) g -= (lvl - thrDown_) * (1.f - 1.f / ratDown_) * s_.down;
                if (lvl < thrUp_) {
                    // fade the boost out near the noise floor so silence is not lifted
                    const float fade = clampf((lvl + 75.f) / 25.f, 0.f, 1.f);
                    g += std::min((thrUp_ - lvl) * (1.f - 1.f / ratUp_) * s_.upward, 18.f) * fade;
                }
                g *= s_.depth;
                gainDb_[b] += (g - gainDb_[b]) * 0.02f; // slew to avoid zipper on level steps
                const float gl = dbToLin(gainDb_[b]) * bandLin_[b];
                out[0] += band[0][b] * gl; out[1] += band[1][b] * gl;
                peak[b] = std::max(peak[b], std::max(std::fabs(band[0][b]), std::fabs(band[1][b])) * gl);
            }
            p[0][i] = out[0] * outLin_; p[1][i] = out[1] * outLin_;
        }
        for (int b = 0; b < 3; ++b) levelDb_[b] = std::max(linToDb(peak[b]), -80.f);
    }

private:
    struct Chan { Biquad lp1[2], hp1[2], lp2[2], hp2[2], ap[2]; };
    double sr_ = 48000;
    MbSettings s_;
    Chan ch_[2];
    float attC_ = 0, relC_ = 0, thrDown_ = -14, ratDown_ = 3, thrUp_ = -36, ratUp_ = 2, outLin_ = 1;
    float bandLin_[3] = {1, 1, 1}, env_[3] = {0, 0, 0}, gainDb_[3] = {0, 0, 0}, levelDb_[3] = {-80, -80, -80};
};

} // namespace lstrip::dsp
