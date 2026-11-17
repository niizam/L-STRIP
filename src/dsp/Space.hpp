// SPDX-License-Identifier: MIT
// Space module: chorus (insert) -> parallel reverb + tempo-synced delay, each
// with its own voice-triggered ducking.  Zero latency.
#pragma once
#include "Common.hpp"
#include "Reverb.hpp"
#include "../common/Params.hpp"

namespace lstrip::dsp {

struct SpaceSettings {
    int type = 0;
    float reverbMix = 0.1f, delayMix = 0, reverbDuckDb = -6, delayDuckDb = -9;
    int delayIdx = 5;
    float chorusRate = 0.8f, chorusMix = 0;
    int voices = 2;
    double bpm = 120.0;
};

class Space {
public:
    void prepare(double sr) {
        sr_ = sr;
        reverb_.prepare(sr);
        for (auto& d : delay_) d.prepare(static_cast<size_t>(4.2 * sr));
        for (auto& c : chorus_) c.prepare(static_cast<size_t>(0.06 * sr));
        env_.setTimes(0.002f, 0.150f, sr);
        duckAtt_ = timeCoef(0.004f, sr); duckRel_ = timeCoef(0.180f, sr);
        fbLpC_ = static_cast<float>(1.0 - std::exp(-2.0 * kPi * 4500.0 / sr));
        fbHpC_ = static_cast<float>(1.0 - std::exp(-2.0 * kPi * 150.0 / sr));
        reset();
        set(SpaceSettings{});
    }
    void reset() {
        reverb_.reset();
        for (auto& d : delay_) d.clear();
        for (auto& c : chorus_) c.clear();
        env_.env = 0; rvDuck_ = dlDuck_ = 0; lpS_[0] = lpS_[1] = hpS_[0] = hpS_[1] = 0;
        lfoPhase_ = 0; delaySamples_ = delayTarget_; // snap: no echo-time sweep after a reset
    }
    void set(const SpaceSettings& s) {
        if (s.type != s_.type || first_) reverb_.setType(s.type);
        first_ = false;
        s_ = s;
        const double bpm = std::min(std::max(s.bpm, 40.0), 300.0);
        const int idx = std::min(std::max(s.delayIdx, 0), 9);
        delayTarget_ = static_cast<float>(60.0 / bpm * kDelayTimeBeats[idx] * sr_);
        delayTarget_ = std::min(delayTarget_, static_cast<float>(4.0 * sr_));
        if (!delayInit_) { delaySamples_ = delayTarget_; delayInit_ = true; }
    }
    float reverbSendDb() const { return rvSendDb_; }
    float delaySendDb() const { return dlSendDb_; }
    float reverbDuckGr() const { return rvGrDb_; }
    float delayDuckGr() const { return dlGrDb_; }

    void process(float* l, float* r, int n) {
        const int nv = std::min(std::max(s_.voices, 1), 4);
        const float vGain = 1.f / std::sqrt(static_cast<float>(nv));
        const float lfoInc = static_cast<float>(s_.chorusRate / sr_);
        const float cmix = s_.chorusMix;
        const float rvMax = s_.reverbMix, dlMax = s_.delayMix;
        const float rvDuckMax = -s_.reverbDuckDb, dlDuckMax = -s_.delayDuckDb;
        float rvPk = 0.f, dlPk = 0.f, rvGr = 0.f, dlGr = 0.f;
        for (int i = 0; i < n; ++i) {
            const float dry0 = l[i], dry1 = r[i];
            // ---- chorus insert
            chorus_[0].push(dry0); chorus_[1].push(dry1);
            float c0 = 0.f, c1 = 0.f;
            if (cmix > 0.001f) {
                for (int v = 0; v < nv; ++v) {
                    const float ph = lfoPhase_ + static_cast<float>(v) / static_cast<float>(nv);
                    const float lfoA = std::sin(2.f * static_cast<float>(kPi) * ph);
                    const float lfoB = std::sin(2.f * static_cast<float>(kPi) * (ph + 0.25f));
                    const float base = (0.012f + 0.0022f * v) * static_cast<float>(sr_);
                    const float dep = 0.0035f * static_cast<float>(sr_);
                    const float a = chorus_[0].read(base + dep * lfoA);
                    const float b = chorus_[1].read(base + dep * lfoB);
                    const float pan = nv == 1 ? 0.5f : static_cast<float>(v) / static_cast<float>(nv - 1);
                    c0 += a * (1.f - pan * 0.6f) * vGain; c1 += b * (0.4f + pan * 0.6f) * vGain;
                }
            }
            lfoPhase_ += lfoInc; if (lfoPhase_ >= 1.f) lfoPhase_ -= 1.f;
            const float m0 = dry0 * (1.f - 0.5f * cmix) + c0 * cmix;
            const float m1 = dry1 * (1.f - 0.5f * cmix) + c1 * cmix;

            // ---- ducking detector on the voice
            const float e = env_.process(std::max(std::fabs(dry0), std::fabs(dry1)));
            const float act = clampf((linToDb(e) + 45.f) / 20.f, 0.f, 1.f);
            const float rvTgt = act * rvDuckMax, dlTgt = act * dlDuckMax;
            rvDuck_ = rvTgt + (rvTgt > rvDuck_ ? duckAtt_ : duckRel_) * (rvDuck_ - rvTgt);
            dlDuck_ = dlTgt + (dlTgt > dlDuck_ ? duckAtt_ : duckRel_) * (dlDuck_ - dlTgt);
            rvGr = std::max(rvGr, rvDuck_); dlGr = std::max(dlGr, dlDuck_);

            // ---- reverb send
            float o0 = 0.f, o1 = 0.f;
            if (rvMax > 0.001f) {
                float wl, wr;
                reverb_.process((m0 + m1) * 0.5f, wl, wr);
                const float g = rvMax * dbToLin(-rvDuck_);
                o0 += wl * g; o1 += wr * g;
                rvPk = std::max(rvPk, std::max(std::fabs(wl), std::fabs(wr)) * g);
            }
            // ---- delay send (ping-pong)
            delaySamples_ += (delayTarget_ - delaySamples_) * 0.0005f;
            const float dA = delay_[0].read(delaySamples_), dB = delay_[1].read(delaySamples_);
            const float in = (m0 + m1) * 0.5f;
            const float fb0 = filterFb(0, dB * 0.4f), fb1 = filterFb(1, dA * 0.4f);
            delay_[0].push(in + fb0); delay_[1].push(fb1);
            if (dlMax > 0.001f) {
                const float g = dlMax * dbToLin(-dlDuck_);
                o0 += dA * g; o1 += dB * g;
                dlPk = std::max(dlPk, std::max(std::fabs(dA), std::fabs(dB)) * g);
            }
            l[i] = m0 + o0; r[i] = m1 + o1;
        }
        rvSendDb_ = std::max(linToDb(rvPk), -80.f); dlSendDb_ = std::max(linToDb(dlPk), -80.f);
        rvGrDb_ = rvGr; dlGrDb_ = dlGr;
    }

private:
    inline float filterFb(int ch, float x) { // band-limit the feedback path
        lpS_[ch] += fbLpC_ * (x - lpS_[ch]);
        hpS_[ch] += fbHpC_ * (lpS_[ch] - hpS_[ch]);
        return lpS_[ch] - hpS_[ch];
    }
    double sr_ = 48000;
    SpaceSettings s_;
    bool first_ = true;
    Reverb reverb_;
    DelayLine delay_[2], chorus_[2];
    EnvFollower env_;
    float duckAtt_ = 0, duckRel_ = 0, rvDuck_ = 0, dlDuck_ = 0;
    float fbLpC_ = 0, fbHpC_ = 0, lpS_[2] = {}, hpS_[2] = {};
    float lfoPhase_ = 0, delaySamples_ = 0, delayTarget_ = 0;
    bool delayInit_ = false;
    float rvSendDb_ = -80, dlSendDb_ = -80, rvGrDb_ = 0, dlGrDb_ = 0;
};

} // namespace lstrip::dsp
