// SPDX-License-Identifier: MIT
// The complete L-STRIP signal chain (framework independent).
//
//   in gain -> Gate -+-> Plosive tamer -> Auto-tune -> Auto level -> Compressor -> De-esser -> EQ -> Multiband
//                    |   -> Space ---------------------------------+
//                    +-> (dry, delayed to match tune latency) -> Intensity blend
//   -> out gain -> Out Sat -> Limiter            (master off = raw bypass)
//
// The Intensity dry path is tapped *after* the gate, so the gate keeps working at
// any Intensity, and the limiter is the last stage, so nothing clips except the
// raw master-off bypass.  Everything is zero-latency except Auto-tune, which adds
// PitchCorrector's fixed latency while enabled (the dry path is delayed to match).
#pragma once
#include "Common.hpp"
#include "Compressor.hpp"
#include "DeEsser.hpp"
#include "Equalizer.hpp"
#include "Gate.hpp"
#include "Leveler.hpp"
#include "Multiband.hpp"
#include "OutputStage.hpp"
#include "PitchCorrector.hpp"
#include "Plosive.hpp"
#include "Space.hpp"
#include "../common/Params.hpp"

#include <atomic>

namespace lstrip::dsp {

// Parameter cell written by GUI/host threads and read by the audio thread.
struct ParamCell {
    std::atomic<float> v{0.f};
    operator float() const { return v.load(std::memory_order_relaxed); }
    ParamCell& operator=(float x) { v.store(x, std::memory_order_relaxed); return *this; }
};

class Chain {
public:
    static constexpr int kChunk = 512;

    Chain() { for (uint32_t i = 0; i < kParamCount; ++i) p_[i] = kParams[i].def; }

    void prepare(double sr) {
        sr_ = sr;
        gate_.prepare(sr); plosive_.prepare(sr); level_.prepare(sr); tune_.prepare(sr); comp_.prepare(sr); deess_.prepare(sr);
        eq_.prepare(sr); mb_.prepare(sr); space_.prepare(sr); sat_.prepare(sr); lim_.prepare(sr);
        for (auto& d : dryDelay_) d.prepare(static_cast<size_t>(0.05 * sr));
        rampN_ = std::max(1, static_cast<int>(0.005 * sr));
        for (auto& f : fade_) f.reset(0.f);
        inG_.reset(1.f); outG_.reset(1.f); mix_.reset(1.f); bypass_.reset(0.f);
        dryDelaySm_ = 0.f;
        dirtyAll();
        applyDirty();
    }
    void reset() {
        gate_.reset(); plosive_.reset(); level_.reset(); tune_.reset(); comp_.reset(); deess_.reset(); eq_.reset(); mb_.reset();
        space_.reset(); sat_.reset(); lim_.reset();
        for (auto& d : dryDelay_) d.clear();
    }

    void setParam(uint32_t id, float v) {
        if (id >= kParamCount || isOutput(id)) return;
        const ParamDef& d = kParams[id];
        v = clampf(v, d.min, d.max);
        if (p_[id] == v) return;
        p_[id] = v;
        markDirty(id);   // release: the audio thread's exchange() below sees the new value
    }
    float param(uint32_t id) const { return p_[id]; }
    void setTempo(double bpm) { if (bpm > 1.0 && bpm != bpm_) { bpm_ = bpm; dirty_.fetch_or(kDirSpace, std::memory_order_release); } }

    // Latency in samples currently introduced by the chain.
    int latency() const { return p_[kTuneOn] > 0.5f && p_[kMasterOn] > 0.5f ? tune_.latencySamples() : 0; }
    int tuneLatency() const { return tune_.latencySamples(); }
    const TuneReadout& tuneReadout() const { return tune_.readout(); }

    // In-place stereo processing; any block size.
    void process(float* l, float* r, int n) {
        ScopedNoDenormals nd;
        applyDirty();
        while (n > 0) {
            const int c = std::min(n, kChunk);
            processChunk(l, r, c);
            l += c; r += c; n -= c;
        }
    }

    // Meter / readout values (valid after process()).
    float output(uint32_t id) const { return out_[id - kFirstOutput]; }

    // Helpers for the UI (spectrum of the EQ section etc. live in the headers).
    double sampleRate() const { return sr_; }

private:
    enum : uint32_t { kDirComp = 1, kDirDeess = 2, kDirEq = 4, kDirMb = 8, kDirSpace = 16,
                      kDirTune = 32, kDirGate = 64, kDirSat = 128, kDirPlosive = 256, kDirLevel = 512, kDirAll = 0xFFFF };
    void dirtyAll() { dirty_.store(kDirAll, std::memory_order_release); }
    void mark(uint32_t bit) { dirty_.fetch_or(bit, std::memory_order_release); }
    void markDirty(uint32_t id) {
        switch (id) {
        case kGateThresh: mark(kDirGate); break;
        case kOutSat: mark(kDirSat); break;
        case kPlosive: mark(kDirPlosive); break;
        case kLevelAmount: mark(kDirLevel); break;
        case kTuneTranspose: case kTuneAmount: mark(kDirTune); break;
        case kEqHighCut: mark(kDirEq); break;
        case kDelayFeedback: case kDelayTone: case kReverbPreDelay: case kReverbLowCut: case kReverbHighCut:
            mark(kDirSpace); break;
        case kDynStyle: case kCompThresh: case kCompMakeup: case kCompTone: case kCompMix: case kCompMode:
            mark(kDirComp); break;
        case kDeessFreq: case kDeessAmount: case kDeessRange: case kDeessListen: mark(kDirDeess); break;
        case kSpaceType: case kReverbMix: case kDelayMix: case kReverbDuck: case kDelayDuck:
        case kDelayTime: case kChorusRate: case kChorusMix: case kChorusVoices: mark(kDirSpace); break;
        case kMbStyle: case kMbDepth: case kMbTime: case kMbUpward: case kMbDown: case kMbOutput:
        case kMbLow: case kMbMid: case kMbHigh: mark(kDirMb); break;
        case kEqStyle: case kEqLow: case kEqMid: case kEqHigh: case kEqAir: case kEqLowCut: mark(kDirEq); break;
        default:
            if ((id >= kTuneKey && id <= kTuneMix)) mark(kDirTune);
            break;
        }
    }
    void applyDirty() {
        // exchange(0): a bit set by another thread after this point is kept for the next block, never lost.
        const uint32_t dirty = dirty_.exchange(0, std::memory_order_acquire);
        if (!dirty) return;
        if (dirty & kDirGate) gate_.setThreshold(p_[kGateThresh]);
        if (dirty & kDirSat) sat_.set(p_[kOutSat] * 0.01f);
        if (dirty & kDirPlosive) plosive_.set(p_[kPlosive] * 0.01f);
        if (dirty & kDirLevel) level_.set(p_[kLevelAmount] * 0.01f);
        if (dirty & kDirComp) {
            CompSettings s;
            s.thresh = p_[kCompThresh]; s.makeup = p_[kCompMakeup]; s.tone = p_[kCompTone];
            s.mix = p_[kCompMix] * 0.01f; s.mode = static_cast<int>(p_[kCompMode] + 0.5f);
            s.style = static_cast<int>(p_[kDynStyle] + 0.5f);
            comp_.set(s);
        }
        if (dirty & kDirDeess)
            deess_.set(p_[kDeessFreq], p_[kDeessAmount] * 0.01f, p_[kDeessRange], p_[kDeessListen] > 0.5f);
        if (dirty & kDirEq) {
            EqSettings s;
            s.low = p_[kEqLow]; s.mid = p_[kEqMid]; s.high = p_[kEqHigh]; s.air = p_[kEqAir];
            s.lowCut = p_[kEqLowCut]; s.style = static_cast<int>(p_[kEqStyle] + 0.5f); s.highCut = p_[kEqHighCut];
            eq_.set(s);
        }
        if (dirty & kDirMb) {
            MbSettings s;
            s.depth = p_[kMbDepth] * 0.01f; s.time = p_[kMbTime] * 0.01f; s.upward = p_[kMbUpward] * 0.01f;
            s.down = p_[kMbDown] * 0.01f; s.output = p_[kMbOutput];
            s.bandGain[0] = p_[kMbLow]; s.bandGain[1] = p_[kMbMid]; s.bandGain[2] = p_[kMbHigh];
            s.style = static_cast<int>(p_[kMbStyle] + 0.5f);
            mb_.set(s);
        }
        if (dirty & kDirSpace) {
            SpaceSettings s;
            s.type = static_cast<int>(p_[kSpaceType] + 0.5f);
            s.reverbMix = p_[kReverbMix] * 0.01f; s.delayMix = p_[kDelayMix] * 0.01f;
            s.reverbDuckDb = p_[kReverbDuck]; s.delayDuckDb = p_[kDelayDuck];
            s.delayIdx = static_cast<int>(p_[kDelayTime] + 0.5f);
            s.chorusRate = p_[kChorusRate]; s.chorusMix = p_[kChorusMix] * 0.01f;
            s.voices = static_cast<int>(p_[kChorusVoices] + 0.5f); s.bpm = bpm_;
            s.delayFeedback = p_[kDelayFeedback] / 100.f; s.delayToneHz = p_[kDelayTone];
            s.reverbPreDelayMs = p_[kReverbPreDelay]; s.reverbLowCutHz = p_[kReverbLowCut]; s.reverbHighCutHz = p_[kReverbHighCut];
            space_.set(s);
        }
        if (dirty & kDirTune) {
            float flags[12];
            for (int i = 0; i < 12; ++i) flags[i] = p_[kTuneNote0 + i];
            TuneSettings s;
            s.mask = scaleMask(static_cast<int>(p_[kTuneKey] + 0.5f), static_cast<int>(p_[kTuneScale] + 0.5f), flags);
            s.speedMs = p_[kTuneSpeed]; s.humanize = p_[kTuneHumanize] * 0.01f; s.mix = p_[kTuneMix] * 0.01f;
            s.transpose = static_cast<int>(std::lround(p_[kTuneTranspose])); s.amount = p_[kTuneAmount] / 100.f;
            tune_.set(s);
        }
    }

    // Run `fn` on (l, r) with a crossfaded bypass controlled by `on`.
    template <class Fn>
    void module(int idx, bool on, float* l, float* r, int n, Fn&& fn) {
        Smoother& f = fade_[idx];
        f.setTarget(on ? 1.f : 0.f, rampN_);
        if (f.settled() && f.value <= 0.f) return;
        if (f.settled() && f.value >= 1.f) { fn(l, r, n); return; }
        std::copy(l, l + n, tl_); std::copy(r, r + n, tr_);
        fn(tl_, tr_, n);
        for (int i = 0; i < n; ++i) {
            const float w = f.next();
            l[i] += (tl_[i] - l[i]) * w; r[i] += (tr_[i] - r[i]) * w;
        }
    }

    // Hosts may hand over garbage (OBS gives plugins an uninitialised buffer for a missing channel).
    // One NaN would poison every stereo-linked detector and the reverb/delay lines until reset, and
    // clampf() would pass it straight through, so non-finite and absurd samples are cleaned up first.
    static inline void sanitize(float* x, int n) {
        for (int i = 0; i < n; ++i) {
            const float v = x[i];
            x[i] = (v == v && v - v == 0.f) ? clampf(v, -16.f, 16.f) : 0.f;   // v==v: not NaN, v-v==0: not Inf
        }
    }

    void processChunk(float* l, float* r, int n) {
        sanitize(l, n); sanitize(r, n);
        // raw dry copy for Intensity / bypass
        std::copy(l, l + n, dl_); std::copy(r, r + n, dr_);

        // metering + input gain
        inG_.setTarget(dbToLin(p_[kInGain]), rampN_);
        float inPk = 0.f;
        for (int i = 0; i < n; ++i) {
            const float g = inG_.next();
            l[i] *= g; r[i] *= g;
            inPk = std::max(inPk, std::max(std::fabs(l[i]), std::fabs(r[i])));
        }
        out_[kMeterIn - kFirstOutput] = std::max(linToDb(inPk), -80.f);

        const float gateGr = gate_.process(l, r, n);
        out_[kGateGR - kFirstOutput] = gateGr;
        std::copy(l, l + n, gl_); std::copy(r, r + n, gr_); // gated dry tap

        plosive_.process(l, r, n);   // no-op at 0 %; before Tune so thumps do not disturb the pitch detector
        module(0, p_[kTuneOn] > 0.5f, l, r, n, [&](float* a, float* b, int m) { tune_.process(a, b, m); });
        out_[kTuneInMidi - kFirstOutput] = tune_.readout().inMidi;
        out_[kTuneOutMidi - kFirstOutput] = tune_.readout().outMidi;

        level_.process(l, r, n);     // no-op at 0 %
        float compGr = 0.f, deessGr = 0.f;
        module(1, p_[kDynOn] > 0.5f, l, r, n, [&](float* a, float* b, int m) {
            compGr = comp_.process(a, b, m);
            deessGr = deess_.process(a, b, m);
        });
        out_[kCompGR - kFirstOutput] = compGr;
        out_[kDeessGR - kFirstOutput] = deessGr;

        module(2, p_[kEqOn] > 0.5f, l, r, n, [&](float* a, float* b, int m) { eq_.process(a, b, m); });
        module(3, p_[kMbOn] > 0.5f, l, r, n, [&](float* a, float* b, int m) { mb_.process(a, b, m); });
        out_[kBandLow - kFirstOutput] = mb_.bandLevelDb(0);
        out_[kBandMid - kFirstOutput] = mb_.bandLevelDb(1);
        out_[kBandHigh - kFirstOutput] = mb_.bandLevelDb(2);

        module(4, p_[kSpaceOn] > 0.5f, l, r, n, [&](float* a, float* b, int m) { space_.process(a, b, m); });
        out_[kReverbSend - kFirstOutput] = space_.reverbSendDb();
        out_[kDelaySend - kFirstOutput] = space_.delaySendDb();
        out_[kReverbDuckGR - kFirstOutput] = space_.reverbDuckGr();
        out_[kDelayDuckGR - kFirstOutput] = space_.delayDuckGr();

        // Intensity blend with latency-aligned dry, then output gain and master bypass.
        const float delayTarget = static_cast<float>(latency());
        mix_.setTarget(p_[kIntensity] * 0.01f, rampN_);
        outG_.setTarget(dbToLin(p_[kOutGain]), rampN_);
        bypass_.setTarget(p_[kMasterOn] > 0.5f ? 0.f : 1.f, rampN_);
        float outPk = 0.f, acc = 0.f;
        for (int i = 0; i < n; ++i) {
            dryDelaySm_ += clampf(delayTarget - dryDelaySm_, -0.2f, 0.2f);
            dryDelay_[0].push(gl_[i]); dryDelay_[1].push(gr_[i]);
            float d0, d1;
            if (dryDelaySm_ < 1.f) {
                d0 = gl_[i] + (dryDelay_[0].read(1.f) - gl_[i]) * dryDelaySm_;
                d1 = gr_[i] + (dryDelay_[1].read(1.f) - gr_[i]) * dryDelaySm_;
            } else { d0 = dryDelay_[0].read(dryDelaySm_); d1 = dryDelay_[1].read(dryDelaySm_); }
            const float m = mix_.next(), g = outG_.next();
            l[i] = (d0 + (l[i] - d0) * m) * g;
            r[i] = (d1 + (r[i] - d1) * m) * g;
        }
        sat_.process(l, r, n);
        lim_.process(l, r, n);
        for (int i = 0; i < n; ++i) {
            const float by = bypass_.next();           // master off: crossfade to the raw input
            l[i] += (dl_[i] - l[i]) * by; r[i] += (dr_[i] - r[i]) * by;
            if (!(l[i] == l[i]) || !(r[i] == r[i])) { l[i] = r[i] = 0.f; }   // never hand NaN to the host
            const float pk = std::max(std::fabs(l[i]), std::fabs(r[i]));
            outPk = std::max(outPk, pk); acc += pk * pk;
        }
        out_[kMeterOut - kFirstOutput] = std::max(linToDb(outPk), -80.f);
        const float rmsDb = linToDb(std::sqrt(acc / static_cast<float>(std::max(n, 1))));
        const float target = clampf((rmsDb + 50.f) / 40.f, 0.f, 1.f);
        orb_ += (target - orb_) * 0.25f;
        out_[kOrbLevel - kFirstOutput] = orb_;
    }

    double sr_ = 48000, bpm_ = 120.0;
    ParamCell p_[kParamCount];
    float out_[kParamCount - kFirstOutput] = {};
    std::atomic<uint32_t> dirty_{kDirAll};

    Gate gate_; PlosiveTamer plosive_; Leveler level_; PitchCorrector tune_; Compressor comp_; DeEsser deess_; Equalizer eq_;
    Multiband mb_; Space space_; Saturator sat_; Limiter lim_;
    Smoother fade_[5], inG_, outG_, mix_, bypass_;
    DelayLine dryDelay_[2];
    float dryDelaySm_ = 0.f, orb_ = 0.f;
    int rampN_ = 240;
    float dl_[kChunk], dr_[kChunk], gl_[kChunk], gr_[kChunk], tl_[kChunk], tr_[kChunk];
};

} // namespace lstrip::dsp
