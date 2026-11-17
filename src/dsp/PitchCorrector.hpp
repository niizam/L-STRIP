// SPDX-License-Identifier: MIT
// Real-time pitch corrector.
//
//  detector : causal YIN on the mono sum (PitchDetector.hpp), no lookahead
//  control  : nearest allowed note (with hysteresis), humanize, speed smoothing
//  shifter  : two-tap delay-line shifter whose cycle length is 2 pitch periods,
//             so both taps (one period apart) are phase-coherent and the
//             cross-fade jump is a whole number of periods.  Small corrections
//             make the taps sweep slowly, so jumps are rare.
//
// Latency: a fixed 6 ms (reported to the host while the module is enabled); the
// real tap delay averages 0.5 ms + one pitch period (5.5 ms at 200 Hz, 10.5 ms
// at 100 Hz).  The un-shifted path is delayed by the same fixed 6 ms.
#pragma once
#include "Common.hpp"
#include "PitchDetector.hpp"

namespace lstrip::dsp {

struct TuneSettings {
    uint16_t mask = 0xFFF;  // allowed pitch classes, bit 0 = C
    float speedMs = 80.f;   // 0 = instant/robotic
    float humanize = 0.3f;  // 0..1
    float mix = 1.f;        // 0..1
};

struct TuneReadout {
    float inMidi = 0.f;   // detected pitch (fractional MIDI note), 0 = unvoiced
    float outMidi = 0.f;  // corrected pitch
};

class PitchCorrector {
public:
    static constexpr float kLatencySec = 0.006f;
    static constexpr float kMinTapSec = 0.0005f;

    void prepare(double sr) {
        sr_ = sr;
        const size_t n = static_cast<size_t>(0.1 * sr);
        for (auto& b : buf_) b.prepare(n);
        det_.prepare(sr);
        latency_ = static_cast<int>(std::lround(kLatencySec * sr));
        hopSec_ = static_cast<float>(PitchDetector::kHop * std::max(1, static_cast<int>(std::lround(sr / 12000.0))) / sr);
        reset();
    }
    void reset() {
        for (auto& b : buf_) b.clear();
        det_.reset();
        u_ = 0.f; ratio_ = 1.f; corrCents_ = 0.f; corrTarget_ = 0.f; wet_ = 0.f;
        slow_ = 0.f; haveNote_ = false; curNote_ = 0; unvoicedHops_ = 1000; wasVoiced_ = false;
        L_ = 2.f * static_cast<float>(sr_) / 150.f; T0_ = static_cast<float>(sr_) / 150.f;
        ro_ = TuneReadout{};
    }
    void set(const TuneSettings& s) { s_ = s; }
    int latencySamples() const { return latency_; }
    const TuneReadout& readout() const { return ro_; }

    void process(float* l, float* r, int n) {
        const float fsr = static_cast<float>(sr_);
        const float dmin = kMinTapSec * fsr;
        const float fixedDelay = static_cast<float>(latency_);
        const float ratioC = 1.f - std::exp(-1.f / (0.002f * fsr));
        const float lenC = 1.f - std::exp(-1.f / (0.060f * fsr));
        const float wetC = 1.f - std::exp(-1.f / (0.008f * fsr));
        const float maxL = 0.045f * fsr;
        for (int i = 0; i < n; ++i) {
            buf_[0].push(l[i]); buf_[1].push(r[i]);
            PitchEstimate est;
            if (det_.push(0.5f * (l[i] + r[i]), est)) onEstimate(est);

            ratio_ += (std::exp2(corrCents_ * (1.f / 1200.f)) - ratio_) * ratioC;
            L_ += (2.f * T0_ - L_) * lenC;
            const float Lc = clampf(L_, 2.f * 0.0011f * fsr, maxL);
            wet_ += (wetTarget_ - wet_) * wetC;

            u_ += (1.f - ratio_) / Lc;
            u_ -= std::floor(u_);
            const float uB = u_ + 0.5f >= 1.f ? u_ - 0.5f : u_ + 0.5f;
            const float dA = dmin + u_ * Lc, dB = dmin + uB * Lc;
            const float wA = 0.5f - 0.5f * std::cos(2.f * static_cast<float>(kPi) * u_);
            const float wB = 1.f - wA;

            const float w = wet_ * s_.mix;
            float* p[2] = {l, r};
            for (int c = 0; c < 2; ++c) {
                const float dry = buf_[c].read(fixedDelay);
                if (w < 1e-4f) { p[c][i] = dry; continue; }
                const float sh = wA * buf_[c].read(dA) + wB * buf_[c].read(dB);
                p[c][i] = dry + (sh - dry) * w;
            }
        }
    }

private:
    void onEstimate(const PitchEstimate& e) {
        if (!e.voiced || s_.mask == 0) {
            if (++unvoicedHops_ > 10) { wetTarget_ = 0.f; haveNote_ = false; ro_.inMidi = ro_.outMidi = 0.f; wasVoiced_ = false; }
            return;
        }
        unvoicedHops_ = 0;
        wetTarget_ = 1.f;
        T0_ = clampf(static_cast<float>(sr_) / e.hz, static_cast<float>(sr_) / 900.f, static_cast<float>(sr_) / 70.f);
        const float midi = 69.f + 12.f * std::log2(e.hz / 440.f);

        // nearest allowed note with 25-cent hysteresis towards the current one
        int best = -1; float bestD = 1e9f;
        const int base = static_cast<int>(std::floor(midi));
        for (int k = base - 7; k <= base + 8; ++k) {
            if (!(s_.mask & (1u << (((k % 12) + 12) % 12)))) continue;
            const float d = std::fabs(midi - k);
            if (d < bestD) { bestD = d; best = k; }
        }
        if (best < 0) { wetTarget_ = 0.f; return; }
        if (haveNote_ && curNote_ != best && s_.mask & (1u << (((curNote_ % 12) + 12) % 12))
            && std::fabs(midi - curNote_) <= bestD + 0.25f)
            best = curNote_;
        if (!haveNote_ || best != curNote_) { curNote_ = best; haveNote_ = true; }

        const float dc = (midi - static_cast<float>(curNote_)) * 100.f; // cents off target
        if (!wasVoiced_) { slow_ = dc; wasVoiced_ = true; }
        slow_ += (dc - slow_) * (1.f - std::exp(-hopSec_ / 0.20f));
        const float fast = dc - slow_;
        corrTarget_ = clampf(-(dc - s_.humanize * fast), -400.f, 400.f);
        const float tau = std::max(s_.speedMs * 0.001f, 0.0005f);
        corrCents_ += (corrTarget_ - corrCents_) * (1.f - std::exp(-hopSec_ / tau));

        ro_.inMidi = midi;
        ro_.outMidi = midi + corrCents_ * 0.01f;
    }

    double sr_ = 48000;
    TuneSettings s_;
    DelayLine buf_[2];
    PitchDetector det_;
    int latency_ = 288, curNote_ = 0, unvoicedHops_ = 1000;
    bool haveNote_ = false, wasVoiced_ = false;
    float hopSec_ = 0.00267f, u_ = 0, ratio_ = 1, corrCents_ = 0, corrTarget_ = 0, wet_ = 0, wetTarget_ = 0;
    float slow_ = 0, L_ = 640, T0_ = 320;
    TuneReadout ro_;
};

} // namespace lstrip::dsp
