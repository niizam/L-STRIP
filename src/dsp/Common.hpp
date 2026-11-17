// SPDX-License-Identifier: MIT
// Shared DSP helpers.  Everything here is real-time safe: no allocation,
// locks or I/O after prepare().
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(__SSE__)
#include <xmmintrin.h>   // must stay outside the namespace
#endif

namespace lstrip::dsp {

constexpr double kPi = 3.14159265358979323846;

inline float dbToLin(float db) { return std::pow(10.0f, db * 0.05f); }
inline float linToDb(float lin) { return 20.0f * std::log10(std::max(lin, 1e-9f)); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// One-pole coefficient for a time constant (seconds) at sample rate sr.
inline float timeCoef(float seconds, double sr) {
    if (seconds <= 0.f) return 0.f;
    return std::exp(-1.0f / (seconds * static_cast<float>(sr)));
}

// Flush denormals for the lifetime of the object (x86 only; no-op elsewhere).
struct ScopedNoDenormals {
#if defined(__SSE__)
    unsigned int old;
    ScopedNoDenormals() : old(_mm_getcsr()) { _mm_setcsr(old | 0x8040); }
    ~ScopedNoDenormals() { _mm_setcsr(old); }
#endif
};

// Linear ramp smoother.
struct Smoother {
    float value = 0.f, target = 0.f, step = 0.f;
    int remaining = 0;
    void reset(float v) { value = target = v; remaining = 0; }
    void setTarget(float t, int rampSamples) {
        if (t == target) return;
        target = t;
        if (rampSamples <= 0) { value = t; remaining = 0; return; }
        remaining = rampSamples;
        step = (t - value) / static_cast<float>(rampSamples);
    }
    inline float next() {
        if (remaining > 0) {
            value += step;
            if (--remaining == 0) value = target;
        }
        return value;
    }
    bool settled() const { return remaining == 0; }
};

// Direct-form-II-transposed biquad with double precision state.
struct BiquadCoef {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    // RBJ cookbook designs.
    static BiquadCoef lowpass(double f, double q, double sr) { return make(0, f, q, 0, sr); }
    static BiquadCoef highpass(double f, double q, double sr) { return make(1, f, q, 0, sr); }
    static BiquadCoef bandpass(double f, double q, double sr) { return make(2, f, q, 0, sr); }
    static BiquadCoef allpass(double f, double q, double sr) { return make(3, f, q, 0, sr); }
    static BiquadCoef peaking(double f, double q, double dB, double sr) { return make(4, f, q, dB, sr); }
    static BiquadCoef lowShelf(double f, double q, double dB, double sr) { return make(5, f, q, dB, sr); }
    static BiquadCoef highShelf(double f, double q, double dB, double sr) { return make(6, f, q, dB, sr); }

    // Magnitude response (linear) at frequency f.
    double magnitude(double f, double sr) const {
        const double w = 2.0 * kPi * f / sr;
        const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const double dr = 1 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return std::sqrt((nr * nr + ni * ni) / std::max(dr * dr + di * di, 1e-30));
    }

private:
    static BiquadCoef make(int type, double f, double q, double dB, double sr) {
        f = std::min(std::max(f, 1.0), sr * 0.49);
        const double A = std::pow(10.0, dB / 40.0);
        const double w0 = 2.0 * kPi * f / sr;
        const double cw = std::cos(w0), sw = std::sin(w0);
        const double alpha = sw / (2.0 * q);
        double b0, b1, b2, a0, a1, a2;
        switch (type) {
        case 0: // LP
            b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case 1: // HP
            b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case 2: // BP (constant 0 dB peak gain)
            b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case 3: // AP
            b0 = 1 - alpha; b1 = -2 * cw; b2 = 1 + alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
        case 4: // peaking
            b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
        case 5: { // low shelf
            const double sA = 2 * std::sqrt(A) * alpha;
            b0 = A * ((A + 1) - (A - 1) * cw + sA); b1 = 2 * A * ((A - 1) - (A + 1) * cw);
            b2 = A * ((A + 1) - (A - 1) * cw - sA); a0 = (A + 1) + (A - 1) * cw + sA;
            a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - sA; break;
        }
        default: { // high shelf
            const double sA = 2 * std::sqrt(A) * alpha;
            b0 = A * ((A + 1) + (A - 1) * cw + sA); b1 = -2 * A * ((A - 1) + (A + 1) * cw);
            b2 = A * ((A + 1) + (A - 1) * cw - sA); a0 = (A + 1) - (A - 1) * cw + sA;
            a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - sA; break;
        }
        }
        BiquadCoef c;
        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
        return c;
    }
};

struct Biquad {
    BiquadCoef c;
    double z1 = 0, z2 = 0;
    void set(const BiquadCoef& n) { c = n; }
    void reset() { z1 = z2 = 0; }
    inline float process(float xf) {
        const double x = xf;
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return static_cast<float>(y);
    }
};

// Circular delay line with Hermite fractional read.  Size is fixed by prepare().
struct DelayLine {
    std::vector<float> buf;
    size_t mask = 0, pos = 0;
    void prepare(size_t maxSamples) {
        size_t n = 8;
        while (n < maxSamples + 8) n <<= 1;
        buf.assign(n, 0.f);
        mask = n - 1;
        pos = 0;
    }
    void clear() { std::fill(buf.begin(), buf.end(), 0.f); }
    inline void push(float x) { pos = (pos + 1) & mask; buf[pos] = x; }
    // delay in samples >= 0 relative to the most recent push (0 = newest).
    inline float read(float delay) const {
        const float d = std::max(delay, 1.0f);
        const size_t id = static_cast<size_t>(d);
        const float f = d - static_cast<float>(id);
        const float xm1 = buf[(pos - id + 1) & mask];
        const float x0 = buf[(pos - id) & mask];
        const float x1 = buf[(pos - id - 1) & mask];
        const float x2 = buf[(pos - id - 2) & mask];
        const float c0 = x0, c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
    inline float readInt(size_t d) const { return buf[(pos - d) & mask]; }
};

// Peak-to-RMS-ish level follower with separate attack/release (linear domain).
struct EnvFollower {
    float env = 0.f, att = 0.f, rel = 0.f;
    void setTimes(float attackSec, float releaseSec, double sr) {
        att = timeCoef(attackSec, sr);
        rel = timeCoef(releaseSec, sr);
    }
    inline float process(float x) {
        const float a = std::fabs(x);
        const float c = a > env ? att : rel;
        env = a + c * (env - a);
        return env;
    }
};

} // namespace lstrip::dsp
