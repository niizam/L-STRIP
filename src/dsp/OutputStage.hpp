// SPDX-License-Identifier: MIT
// Output saturation (first-order antiderivative anti-aliased tanh, zero latency)
// followed by a zero-latency peak limiter.
#pragma once
#include "Common.hpp"

namespace lstrip::dsp {

class Saturator {
public:
    void prepare(double sr) { dcC_ = static_cast<float>(1.0 - std::exp(-2.0 * kPi * 8.0 / sr)); reset(); }
    void reset() { for (int c = 0; c < 2; ++c) { xPrev_[c] = 0; fPrev_[c] = 0; dc_[c] = 0; } }
    void set(float amount) { amt_ = amount; }
    void process(float* l, float* r, int n) {
        if (amt_ < 0.001f) { reset(); return; }
        const float drive = 1.f + 4.f * amt_;
        const float bias = 0.12f * amt_;
        const float blend = std::min(1.f, amt_ * 3.f);
        const float norm = 1.f / (drive * (1.f - std::tanh(bias) * std::tanh(bias)));
        float* p[2] = {l, r};
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i) {
                const float x = p[c][i];
                const float u = x * drive + bias;
                const float F = logcosh(u);
                const float du = u - xPrev_[c];
                float y;
                if (std::fabs(du) > 1e-4f) y = (F - fPrev_[c]) / du;
                else y = std::tanh(0.5f * (u + xPrev_[c]));
                xPrev_[c] = u; fPrev_[c] = F;
                y = (y - std::tanh(bias)) * norm;
                dc_[c] += dcC_ * (y - dc_[c]);
                y -= dc_[c];
                p[c][i] = x + (y - x) * blend;
            }
    }
private:
    static inline float logcosh(float x) {
        const float a = std::fabs(x);
        return a + std::log1p(std::exp(-2.f * a)) - 0.69314718f;
    }
    float amt_ = 0, dcC_ = 0, xPrev_[2] = {}, fPrev_[2] = {}, dc_[2] = {};
};

class Limiter {
public:
    void prepare(double sr) { relC_ = timeCoef(0.060f, sr); reset(); setCeiling(-1.f); }
    void reset() { gain_ = 1.f; }
    void setCeiling(float db) { ceil_ = dbToLin(db); }
    // Returns max gain reduction in dB for the block.
    float process(float* l, float* r, int n) {
        float minG = 1.f;
        for (int i = 0; i < n; ++i) {
            const float pk = std::max(std::fabs(l[i]), std::fabs(r[i]));
            const float need = pk > ceil_ ? ceil_ / pk : 1.f;
            // instant attack (no lookahead => 0 ms latency), smooth release
            gain_ = need < gain_ ? need : need + relC_ * (gain_ - need);
            l[i] = clampf(l[i] * gain_, -ceil_, ceil_);
            r[i] = clampf(r[i] * gain_, -ceil_, ceil_);
            minG = std::min(minG, gain_);
        }
        return -linToDb(minG);
    }
private:
    float relC_ = 0, gain_ = 1, ceil_ = 0.89f;
};

} // namespace lstrip::dsp
