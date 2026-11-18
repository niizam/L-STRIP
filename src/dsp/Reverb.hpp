// SPDX-License-Identifier: MIT
// 8-line feedback delay network reverb with Hadamard mixing, per-line damping,
// input diffusion and stereo decorrelated taps.
#pragma once
#include "Common.hpp"

namespace lstrip::dsp {

class Reverb {
public:
    void prepare(double sr) {
        sr_ = sr;
        const double scale = sr / 48000.0;
        static const int base[8] = {1117, 1327, 1559, 1733, 1913, 2111, 2297, 2477};
        for (int i = 0; i < 8; ++i) {
            baseLen_[i] = base[i] * scale;
            line_[i].prepare(static_cast<size_t>(baseLen_[i] * 2.2) + 16);
        }
        static const int apLen[3] = {142, 107, 379};
        for (int i = 0; i < 3; ++i) { ap_[i].prepare(static_cast<size_t>(apLen[i] * scale) + 8); apD_[i] = apLen[i] * scale; }
        pre_.prepare(static_cast<size_t>(0.14 * sr));   // type pre-delay (<= 18 ms) + extra (<= 100 ms)
        reset();
        setType(0);
    }
    void reset() {
        for (auto& l : line_) l.clear();
        for (auto& a : ap_) a.clear();
        pre_.clear();
        for (int i = 0; i < 8; ++i) lp_[i] = 0.f;
    }
    // 0 room, 1 hall, 2 plate
    void setType(int t) {
        float rt60, size, damp, pre;
        switch (t) {
        case 1: rt60 = 1.8f; size = 1.35f; damp = 0.35f; pre = 0.018f; break;
        case 2: rt60 = 1.3f; size = 0.95f; damp = 0.12f; pre = 0.004f; break;
        default: rt60 = 0.55f; size = 0.7f; damp = 0.30f; pre = 0.008f; break;
        }
        for (int i = 0; i < 8; ++i) {
            len_[i] = static_cast<float>(baseLen_[i] * size);
            gain_[i] = std::pow(10.f, -3.f * len_[i] / (rt60 * static_cast<float>(sr_)));
        }
        damp_ = damp;
        basePre_ = pre * static_cast<float>(sr_);
        preSamples_ = basePre_ + extraPre_;
    }
    // Extra pre-delay in milliseconds, added to the type's own.  0 changes nothing.
    void setExtraPreDelayMs(float ms) {
        extraPre_ = ms * 0.001f * static_cast<float>(sr_);
        preSamples_ = basePre_ + extraPre_;
    }

    // Mono in -> stereo out (wet only).
    inline void process(float in, float& outL, float& outR) {
        pre_.push(in);
        float x = pre_.read(preSamples_);
        for (int i = 0; i < 3; ++i) { // Schroeder allpass diffusion
            const float d = ap_[i].read(apD_[i]);
            const float v = x + 0.6f * d;
            ap_[i].push(v);
            x = d - 0.6f * v;
        }
        float v[8];
        for (int i = 0; i < 8; ++i) v[i] = line_[i].read(len_[i]);
        // Hadamard 8x8 (in-place fast transform), scaled by 1/sqrt(8)
        for (int h = 1; h < 8; h <<= 1)
            for (int i = 0; i < 8; i += h << 1)
                for (int j = i; j < i + h; ++j) { const float a = v[j], b = v[j + h]; v[j] = a + b; v[j + h] = a - b; }
        const float k = 0.35355339f;
        float l = 0.f, r = 0.f;
        for (int i = 0; i < 8; ++i) {
            float y = v[i] * k * gain_[i];
            lp_[i] += (1.f - damp_) * (y - lp_[i]);   // high-frequency damping
            y = lp_[i];
            line_[i].push(y + x * 0.5f);
            if (i & 1) r += y; else l += y;
        }
        outL = l * 0.5f; outR = r * 0.5f;
    }

private:
    double sr_ = 48000;
    DelayLine line_[8], ap_[3], pre_;
    double baseLen_[8] = {};
    float len_[8] = {}, gain_[8] = {}, lp_[8] = {}, apD_[3] = {}, damp_ = 0.3f, preSamples_ = 0, basePre_ = 0, extraPre_ = 0;
};

} // namespace lstrip::dsp
