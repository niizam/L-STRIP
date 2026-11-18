// SPDX-License-Identifier: MIT
// Headless DSP tests for L-STRIP.  No framework needed; returns non-zero on failure.
#include "../src/dsp/Chain.hpp"
#include "../src/common/PresetStore.hpp"
#include <atomic>
#include <filesystem>
#include <thread>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

using namespace lstrip;
using namespace lstrip::dsp;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { ++g_pass; } else { ++g_fail; std::printf("  FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

constexpr double SR = 48000.0;

// All modules off: pure passthrough configuration.
static void setAllOff(Chain& c) {
    for (uint32_t i : {kDynOn, kSpaceOn, kMbOn, kEqOn, kTuneOn}) c.setParam(i, 0);
    c.setParam(kGateThresh, -80); c.setParam(kOutSat, 0); c.setParam(kIntensity, 100);
    c.setParam(kInGain, 0); c.setParam(kOutGain, 0); c.setParam(kMasterOn, 1);
}
static void runChain(Chain& c, std::vector<float>& l, std::vector<float>& r) {
    c.process(l.data(), r.data(), static_cast<int>(l.size()));
}
static float rmsDb(const std::vector<float>& x, size_t a, size_t b) {
    double e = 0; for (size_t i = a; i < b; ++i) e += x[i] * x[i];
    return linToDb(static_cast<float>(std::sqrt(e / (b - a))));
}
// Synthetic "voice": harmonic stack with 1/n rolloff.
static std::vector<float> voice(double f0, size_t n, float amp = 0.3f, double vibCents = 0, double vibHz = 5.5) {
    std::vector<float> x(n); double ph = 0;
    for (size_t i = 0; i < n; ++i) {
        const double f = f0 * std::pow(2.0, vibCents * std::sin(2 * kPi * vibHz * i / SR) / 1200.0);
        ph += 2 * kPi * f / SR; double v = 0;
        for (int h = 1; h <= 14; ++h) v += std::sin(h * ph) / h;
        x[i] = static_cast<float>(v) * amp;
    }
    return x;
}
// Autocorrelation pitch measurement on a stable segment (Hz).
static double measureHz(const std::vector<float>& x, size_t a, size_t b, double lo, double hi) {
    const int minLag = static_cast<int>(SR / hi), maxLag = static_cast<int>(SR / lo);
    std::vector<double> ac(maxLag + 2, 0.0);
    for (int lag = minLag - 1; lag <= maxLag + 1; ++lag) {
        double s = 0, e1 = 0, e2 = 0;
        for (size_t i = a; i + lag < b; ++i) { s += x[i] * x[i + lag]; e1 += x[i] * x[i]; e2 += x[i + lag] * x[i + lag]; }
        ac[lag] = s / std::sqrt(e1 * e2 + 1e-30);
    }
    int best = minLag;
    for (int lag = minLag; lag <= maxLag; ++lag) if (ac[lag] > ac[best]) best = lag;
    const double y0 = ac[best - 1], y1 = ac[best], y2 = ac[best + 1];
    const double off = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2);
    return SR / (best + off);
}
static double cents(double a, double b) { return 1200.0 * std::log2(a / b); }

static void testParamTable() {
    std::printf("param table\n");
    for (uint32_t i = 0; i < kParamCount; ++i) {
        const ParamDef& d = kParams[i];
        CHECK(d.symbol && d.symbol[0], "param %u has symbol", i);
        CHECK(d.def >= d.min && d.def <= d.max, "param %s default in range", d.symbol);
        CHECK(d.min < d.max, "param %s range", d.symbol);
        for (uint32_t j = i + 1; j < kParamCount; ++j) CHECK(std::strcmp(d.symbol, kParams[j].symbol) != 0, "dup symbol %s", d.symbol);
        CHECK(isOutput(i) == (i >= kFirstOutput && i <= kLastOutput), "outputs are one contiguous block: %s", d.symbol);
        CHECK(hostToId(idToHost(i)) == i && idToHost(hostToId(i)) == i, "host index mapping round-trips: %s", d.symbol);
        CHECK(isOutput(i) == (idToHost(i) >= kNumInputs), "host order puts every output last: %s", d.symbol);
    }
    // C major = C D E F G A B -> bits 0,2,4,5,7,9,11
    CHECK(scaleMask(0, 0, nullptr) == ((1u<<0)|(1u<<2)|(1u<<4)|(1u<<5)|(1u<<7)|(1u<<9)|(1u<<11)), "C major mask");
    // D major root rotates: D E F# G A B C#
    CHECK(scaleMask(2, 0, nullptr) == ((1u<<2)|(1u<<4)|(1u<<6)|(1u<<7)|(1u<<9)|(1u<<11)|(1u<<1)), "D major mask");
}

static void testPassthrough() {
    std::printf("passthrough + latency\n");
    Chain c; c.prepare(SR); setAllOff(c);
    std::vector<float> l = voice(200, 24000), r = l, ref = l;
    runChain(c, l, r); // fades settle within 5 ms
    double maxErr = 0; for (size_t i = 2000; i < l.size(); ++i) maxErr = std::max(maxErr, (double)std::fabs(l[i] - ref[i]));
    CHECK(maxErr < 1e-5, "unity passthrough, max error %g", maxErr);
    CHECK(c.latency() == 0, "latency 0 when tune off");
    c.setParam(kTuneOn, 1);
    CHECK(c.latency() == static_cast<int>(std::lround(0.010 * SR)), "tune latency %d", c.latency());
    c.setParam(kMasterOn, 0);
    CHECK(c.latency() == 0, "latency 0 when master off");
}

static void testBlockSizes() {
    std::printf("block-size independence\n");
    auto src = voice(180, 30000);
    auto run = [&](int bs) {
        Chain c; c.prepare(SR);
        std::vector<float> l = src, r = src;
        for (size_t i = 0; i < l.size(); i += bs) c.process(l.data() + i, r.data() + i, (int)std::min<size_t>(bs, l.size() - i));
        return l;
    };
    auto a = run(512), b = run(37), d = run(4096);
    double e1 = 0, e2 = 0;
    for (size_t i = 0; i < a.size(); ++i) { e1 = std::max(e1, (double)std::fabs(a[i] - b[i])); e2 = std::max(e2, (double)std::fabs(a[i] - d[i])); }
    CHECK(e1 < 1e-3 && e2 < 1e-3, "block sizes agree (%g, %g)", e1, e2);
}

static void testGate() {
    std::printf("gate\n");
    Chain c; c.prepare(SR); setAllOff(c); c.setParam(kGateThresh, -40);
    std::vector<float> l(48000), r;
    std::mt19937 g(1); std::uniform_real_distribution<float> u(-1, 1);
    for (size_t i = 0; i < l.size(); ++i) l[i] = u(g) * 0.002f; // -54 dBFS noise
    r = l; runChain(c, l, r);
    CHECK(rmsDb(l, 24000, 48000) < -90.f, "noise gated: %.1f dB", rmsDb(l, 24000, 48000));
    auto v = voice(200, 24000, 0.3f); r = v; runChain(c, v, r);
    CHECK(rmsDb(v, 4000, 24000) > -20.f, "voice passes: %.1f dB", rmsDb(v, 4000, 24000));
}

static void testCompressor() {
    std::printf("compressor\n");
    Chain c; c.prepare(SR); setAllOff(c);
    c.setParam(kDynOn, 1); c.setParam(kCompThresh, -30); c.setParam(kCompMakeup, 0);
    c.setParam(kDeessAmount, 0);
    std::vector<float> l(48000); for (size_t i = 0; i < l.size(); ++i) l[i] = 0.7f * std::sin(2 * kPi * 440 * i / SR);
    auto r = l; runChain(c, l, r);
    CHECK(c.output(kCompGR) > 6.f, "gain reduction %.1f dB", c.output(kCompGR));
    CHECK(rmsDb(l, 30000, 48000) < rmsDb(r, 0, 100) + 100.f, "finite");
    float peakIn = 0.7f, peakOut = 0; for (size_t i = 30000; i < l.size(); ++i) peakOut = std::max(peakOut, std::fabs(l[i]));
    CHECK(peakOut < peakIn * 0.6f, "compressed peak %.3f", peakOut);
    CHECK(c.latency() == 0, "no latency");
}

static void testDeEsser() {
    std::printf("de-esser\n");
    auto make = [](float amount, bool listen, std::vector<float>& out) {
        Chain c; c.prepare(SR); setAllOff(c);
        c.setParam(kDynOn, 1); c.setParam(kCompMix, 0);
        c.setParam(kDeessFreq, 7000); c.setParam(kDeessAmount, amount); c.setParam(kDeessRange, 20); c.setParam(kDeessListen, listen);
        // vowel, then an unvoiced 7 kHz fricative burst (as in a real 'sss'), then vowel
        std::vector<float> l = voice(150, 48000, 0.15f);
        for (size_t i = 24000; i < 40000; ++i) l[i] = 0.12f * std::sin(2 * kPi * 7000 * i / SR) + 0.05f * std::sin(2 * kPi * 8300 * i / SR);
        auto r = l; runChain(c, l, r); out = l;
    };
    std::vector<float> off, on, listen;
    make(0, false, off); make(100, false, on); make(100, true, listen);
    const float sOff = rmsDb(off, 28000, 38000), sOn = rmsDb(on, 28000, 38000);
    CHECK(sOn < sOff - 3.f, "sibilance reduced %.1f -> %.1f dB", sOff, sOn);
    const float vOff = rmsDb(off, 4000, 20000), vOn = rmsDb(on, 4000, 20000);
    CHECK(std::fabs(vOn - vOff) < 1.5f, "vowel mostly untouched %.1f vs %.1f", vOff, vOn);
    CHECK(rmsDb(listen, 28000, 38000) > -40.f, "listen outputs removed band");
}

static void testLimiter() {
    std::printf("limiter + saturation\n");
    Chain c; c.prepare(SR); setAllOff(c); c.setParam(kInGain, 24); c.setParam(kOutSat, 60);
    std::vector<float> l = voice(200, 24000, 0.9f), r = l; runChain(c, l, r);
    float pk = 0; bool finite = true;
    for (float v : l) { pk = std::max(pk, std::fabs(v)); finite &= std::isfinite(v); }
    CHECK(finite, "finite");
    CHECK(pk <= dbToLin(-1.f) + 1e-4f, "limited peak %.4f", pk);
}

static void testGateAtIntensity() {
    std::printf("gate stays effective at Intensity < 100%%\n");
    Chain c; c.prepare(SR); setAllOff(c); c.setParam(kGateThresh, -40); c.setParam(kIntensity, 70);
    std::vector<float> l(48000); std::mt19937 g(7); std::uniform_real_distribution<float> u(-1, 1);
    for (auto& v : l) v = u(g) * 0.002f; // -54 dBFS room noise
    auto r = l; runChain(c, l, r);
    CHECK(rmsDb(l, 24000, 48000) < -85.f, "noise stays gated at 70%% intensity: %.1f dB", rmsDb(l, 24000, 48000));
}

static void testTuneAlignment() {
    std::printf("tune dry/wet time alignment\n");
    // A 110 Hz voice exactly on A2: no correction needed, so Intensity 70%% with tune on
    // must equal Intensity 70%% with tune off, delayed by the reported latency.
    auto run = [&](bool tuneOn, std::vector<float>& out, int& lat) {
        Chain c; c.prepare(SR); setAllOff(c); c.setParam(kIntensity, 70); c.setParam(kTuneOn, tuneOn);
        c.setParam(kTuneScale, 2); c.setParam(kTuneSpeed, 0); c.setParam(kTuneHumanize, 0);
        c.setParam(kDynOn, 1); c.setParam(kCompMix, 0); c.setParam(kDeessAmount, 0); // trivial processing so wet != dry
        c.setParam(kEqOn, 1); c.setParam(kEqLow, 0); c.setParam(kEqMid, 0); c.setParam(kEqHigh, 0); c.setParam(kEqAir, 6);
        std::vector<float> l = voice(110.0, 48000, 0.3f), r = l; runChain(c, l, r); out = l; lat = c.latency();
    };
    std::vector<float> on, off; int latOn = 0, latOff = 0;
    run(true, on, latOn); run(false, off, latOff);
    double e = 0, ref = 0;
    for (size_t i = 30000; i < 46000; ++i) { const double d = on[i] - off[i - latOn]; e += d * d; ref += off[i - latOn] * (double)off[i - latOn]; }
    const double errDb = 10 * std::log10(e / ref + 1e-20);
    CHECK(errDb < -14.0, "aligned dry/wet at 110 Hz: error %.1f dB (latency %d)", errDb, latOn);
}

static void testEq() {
    std::printf("equalizer response\n");
    EqSettings s; s.low = 6; s.mid = 0; s.high = 0; s.air = 0; s.lowCut = 100; s.style = 0;
    BiquadCoef co[5]; eqDesign(s, SR, co);
    auto mag = [&](double f) { double m = 1; for (auto& b : co) m *= b.magnitude(f, SR); return 20 * std::log10(m); };
    CHECK(mag(20) < -12.0, "low-cut attenuates 20 Hz: %.1f dB", mag(20));
    CHECK(std::fabs(mag(1000)) < 0.6, "flat mid: %.2f dB", mag(1000));
    EqSettings a; a.low = 0; a.mid = 0; a.high = 0; a.air = 6; a.lowCut = 20; BiquadCoef ca[5]; eqDesign(a, SR, ca);
    double m = 1; for (auto& b : ca) m *= b.magnitude(18000, SR);
    CHECK(std::fabs(20 * std::log10(m) - 6.0) < 1.0, "air +6 dB at 18 kHz: %.2f", 20 * std::log10(m));
}

static void testMultiband() {
    std::printf("multiband\n");
    Chain c; c.prepare(SR); setAllOff(c); c.setParam(kMbOn, 1); c.setParam(kMbDepth, 0);
    c.setParam(kMbLow, 0); c.setParam(kMbMid, 0); c.setParam(kMbHigh, 0);
    std::vector<float> l = voice(150, 24000, 0.2f), r = l, ref = l; runChain(c, l, r);
    // depth 0 => crossover only: must sum flat in magnitude (allpass-like)
    CHECK(std::fabs(rmsDb(l, 8000, 24000) - rmsDb(ref, 8000, 24000)) < 0.5f, "crossover sums flat: %.2f vs %.2f", rmsDb(l, 8000, 24000), rmsDb(ref, 8000, 24000));
    CHECK(c.latency() == 0, "zero latency");
    // upward compression lifts quiet signals
    Chain d; d.prepare(SR); setAllOff(d); d.setParam(kMbOn, 1); d.setParam(kMbDepth, 100); d.setParam(kMbUpward, 100); d.setParam(kMbDown, 0); d.setParam(kMbStyle, 2);
    std::vector<float> q = voice(150, 48000, 0.01f), q2 = q, qref = q; runChain(d, q, q2);
    CHECK(rmsDb(q, 30000, 48000) > rmsDb(qref, 30000, 48000) + 3.f, "upward lifts quiet signal: %.1f vs %.1f", rmsDb(q, 30000, 48000), rmsDb(qref, 30000, 48000));
}

static void testSpace() {
    std::printf("space\n");
    Chain c; c.prepare(SR); setAllOff(c); c.setParam(kSpaceOn, 1); c.setParam(kReverbMix, 80); c.setParam(kDelayMix, 60);
    c.setParam(kChorusMix, 50); c.setParam(kChorusVoices, 3);
    std::vector<float> l(96000, 0.f); for (int i = 0; i < 2000; ++i) l[i] = 0.5f * std::sin(2 * kPi * 300 * i / SR);
    auto r = l; runChain(c, l, r);
    CHECK(rmsDb(l, 30000, 90000) > -80.f, "reverb/delay tail present: %.1f dB", rmsDb(l, 30000, 90000));
    bool fin = true; for (float v : l) fin &= std::isfinite(v); CHECK(fin, "finite");
    CHECK(rmsDb(l, 90000, 96000) < rmsDb(l, 4000, 12000), "tail decays");
}

static void testPitch() {
    std::printf("pitch correction\n");
    struct Case { double inHz; uint32_t scale; int key; double expectHz; const char* name; };
    const double A3 = 220.0;
    Case cases[] = {
        {A3 * std::pow(2.0, 30.0 / 1200), 2, 0, A3, "chromatic 30c sharp -> A3"},
        {A3 * std::pow(2.0, -35.0 / 1200), 2, 0, A3, "chromatic 35c flat -> A3"},
        {A3 * std::pow(2.0, 140.0 / 1200), 0, 0, 246.9417, "C major: 1.4 semitones above A -> B"}, // A# not in C major -> nearest B
        {146.0, 2, 0, 146.8324, "male range D3"},
        {330.0 * std::pow(2.0, 20.0 / 1200), 2, 0, 329.6276, "E4"},
    };
    for (auto& k : cases) {
        Chain c; c.prepare(SR); setAllOff(c); c.setParam(kTuneOn, 1);
        c.setParam(kTuneScale, (float)k.scale); c.setParam(kTuneKey, (float)k.key);
        c.setParam(kTuneSpeed, 0); c.setParam(kTuneHumanize, 0); c.setParam(kTuneMix, 100);
        std::vector<float> l = voice(k.inHz, 48000, 0.3f), r = l; runChain(c, l, r);
        const double got = measureHz(l, 30000, 46000, k.expectHz * 0.9, k.expectHz * 1.1);
        const double err = cents(got, k.expectHz);
        CHECK(std::fabs(err) < 8.0, "%s: got %.2f Hz (%.1f cents off)", k.name, got, err);
        CHECK(c.latency() == static_cast<int>(std::lround(0.010 * SR)), "latency reported");
    }
    // Detection readout
    {
        Chain c; c.prepare(SR); setAllOff(c); c.setParam(kTuneOn, 1);
        std::vector<float> l = voice(261.63, 24000), r = l; runChain(c, l, r);
        CHECK(std::fabs(c.output(kTuneInMidi) - 60.f) < 0.3f, "detected midi %.2f (expect 60)", c.output(kTuneInMidi));
    }
    // Unvoiced input (noise) must not blow up or shift anything
    {
        Chain c; c.prepare(SR); setAllOff(c); c.setParam(kTuneOn, 1);
        std::mt19937 g(3); std::uniform_real_distribution<float> u(-1, 1);
        std::vector<float> l(48000); for (auto& v : l) v = 0.2f * u(g);
        auto r = l; runChain(c, l, r);
        bool fin = true; float pk = 0; for (float v : l) { fin &= std::isfinite(v); pk = std::max(pk, std::fabs(v)); }
        CHECK(fin && pk < 1.0f, "noise stays sane (peak %.2f)", pk);
        CHECK(c.output(kTuneInMidi) == 0.f, "noise reads as unvoiced");
    }
    // Vibrato: humanize 100% keeps vibrato; humanize 0 flattens it.
    {
        auto spread = [&](float hum) {
            Chain c; c.prepare(SR); setAllOff(c); c.setParam(kTuneOn, 1); c.setParam(kTuneScale, 2);
            c.setParam(kTuneSpeed, 0); c.setParam(kTuneHumanize, hum);
            std::vector<float> l = voice(220, 96000, 0.3f, 40.0, 5.0), r = l; runChain(c, l, r);
            double lo = 1e9, hi = -1e9;
            for (size_t a = 48000; a + 4800 <= l.size(); a += 2400) {
                const double hz = measureHz(l, a, a + 4800, 190, 260); lo = std::min(lo, hz); hi = std::max(hi, hz);
            }
            return cents(hi, lo);
        };
        const double flat = spread(0), kept = spread(100);
        CHECK(flat < kept, "humanize preserves vibrato: spread %.1f (0%%) < %.1f (100%%)", flat, kept);
    }
}

static void testConcurrentParams() {
    std::printf("concurrent parameter writes (GUI thread vs audio thread)\n");
    Chain c; c.prepare(SR);
    std::atomic<bool> stop{false};
    std::thread gui([&] {
        std::mt19937 g(5); std::uniform_real_distribution<float> u(0, 1);
        while (!stop.load()) {
            for (uint32_t i = 0; i < kParamCount; ++i) {   // like a preset apply: every parameter, back to back
                if (i == kMasterOn || isOutput(i)) continue;
                c.setParam(i, kParams[i].min + u(g) * (kParams[i].max - kParams[i].min));
            }
        }
    });
    auto src = voice(200, 2048, 0.3f);
    bool fin = true;
    for (int it = 0; it < 400; ++it) {
        std::vector<float> l = src, r = src;
        c.process(l.data(), r.data(), static_cast<int>(l.size()));
        for (float v : l) fin &= std::isfinite(v);
    }
    stop.store(true); gui.join();
    CHECK(fin, "audio stays finite while parameters change from another thread");
    // After the writer stops, the *latest* edit must be what the DSP uses: no update is lost.
    setAllOff(c); c.setParam(kEqOn, 1);
    for (uint32_t id : {kEqLow, kEqMid, kEqHigh}) c.setParam(id, 0);
    c.setParam(kEqLowCut, 20); c.setParam(kEqStyle, 0);   // the writer randomised the style too
    for (uint32_t id : {kEqHighCut, kLevelAmount, kPlosive}) c.setParam(id, kParams[id].def);   // ...and the 1.1 controls that colour the level
    auto sine = [](size_t n) { std::vector<float> x(n); for (size_t i = 0; i < n; ++i) x[i] = 0.2f * std::sin(2 * kPi * 12000.0 * i / SR); return x; };
    auto level = [&](float airDb) {
        c.setParam(kEqAir, airDb);                        // edit made right before the block is processed
        std::vector<float> l = sine(8192), r = l; c.process(l.data(), r.data(), 8192);
        return rmsDb(l, 4096, 8192);
    };
    level(0.f);
    const float lo = level(-6.f), hi = level(6.f), lo2 = level(-6.f);
    CHECK(hi > lo + 8.f && std::fabs(lo2 - lo) < 0.5f, "each edit takes effect (air -6/+6/-6 -> %.1f / %.1f / %.1f dB)", lo, hi, lo2);
}

static void testGarbageInput() {
    std::printf("garbage on one channel (uninitialised host buffer)\n");
    for (int mode = 0; mode < 3; ++mode) {
        Chain c; c.prepare(SR);
        const float bad[3] = {NAN, INFINITY, 1e30f};
        std::vector<float> l = voice(200, 4096, 0.3f), r(4096, 0.f);
        for (size_t i = 100; i < 300; ++i) r[i] = bad[mode];   // garbage in R only
        c.process(l.data(), r.data(), 4096);
        bool fin = true; for (size_t i = 0; i < 4096; ++i) fin &= std::isfinite(l[i]) && std::isfinite(r[i]);
        CHECK(fin, "output stays finite after %s on the right channel", mode == 0 ? "NaN" : (mode == 1 ? "Inf" : "1e30"));
        // and the chain must recover: later clean blocks behave like a fresh chain
        Chain fresh; fresh.prepare(SR);
        std::vector<float> a = voice(200, 16384, 0.3f), b = a, fa = a, fb = a;
        for (int k = 0; k < 3; ++k) {
            std::vector<float> g = voice(200, 4096, 0.3f), z(4096, 0.f); z[50] = bad[mode];
            c.process(g.data(), z.data(), 4096);
        }
        c.process(a.data(), b.data(), 16384); fresh.process(fa.data(), fb.data(), 16384);
        double e = 0; for (size_t i = 8000; i < 16384; ++i) e = std::max(e, (double)std::fabs(a[i] - fa[i]));
        CHECK(e < 5e-2, "chain recovers after garbage (%s): max deviation %.4f", mode == 0 ? "NaN" : (mode == 1 ? "Inf" : "1e30"), e);
    }
}

static void testFuzz() {
    std::printf("stability fuzz\n");
    Chain c; c.prepare(SR);
    std::mt19937 g(42); std::uniform_real_distribution<float> u(0, 1);
    auto src = voice(180, 4800, 0.5f);
    bool fin = true; float pk = 0;
    for (int iter = 0; iter < 300; ++iter) {
        for (uint32_t i = 0; i < kParamCount; ++i) {
            if (i == kMasterOn || isOutput(i)) continue;
            if (u(g) < 0.15f) c.setParam(i, kParams[i].min + u(g) * (kParams[i].max - kParams[i].min));
        }
        std::vector<float> l = src, r = src;
        for (auto& v : l) v *= (u(g) < 0.1f ? 4.f : 1.f);
        // (peak is checked per block after the +24 dB input gain worst case)
        c.process(l.data(), r.data(), static_cast<int>(l.size()));
        for (size_t i = 0; i < l.size(); ++i) { fin &= std::isfinite(l[i]) && std::isfinite(r[i]); pk = std::max(pk, std::max(std::fabs(l[i]), std::fabs(r[i]))); }
    }
    CHECK(fin, "no NaN/Inf under random automation");
    CHECK(pk <= dbToLin(-1.f) + 1e-3f || c.param(kMasterOn) < 0.5f, "limiter is last: peak %.3f", pk);
}

// ======================================================================= features added in 1.1

// Level of the band below 90 Hz in [a, b), in dB.
static float lowBandDb(const std::vector<float>& x, size_t a, size_t b) {
    Biquad f; f.set(BiquadCoef::lowpass(90.0, 0.707, SR));
    double e = 0;
    for (size_t i = 0; i < b; ++i) { const float y = f.process(x[i]); if (i >= a) e += y * y; }
    return linToDb(static_cast<float>(std::sqrt(e / (b - a))));
}
template <class Fn>
static void runInBlocks(Fn&& fn, std::vector<float>& l, std::vector<float>& r) {
    for (size_t pos = 0; pos < l.size(); pos += 480) fn(l.data() + pos, r.data() + pos, static_cast<int>(std::min<size_t>(480, l.size() - pos)));
}
static bool sameBits(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

static void testNewParamsNeutral() {
    std::printf("1.1 parameters default to no effect\n");
    CHECK(kParams[kTuneTranspose].def == 0 && kParams[kTuneAmount].def == 100 && kParams[kPlosive].def == 0 && kParams[kLevelAmount].def == 0,
          "tune/plosive/level defaults");
    CHECK(kParams[kEqHighCut].def == kParams[kEqHighCut].max && kParams[kReverbHighCut].def == kParams[kReverbHighCut].max
          && kParams[kReverbLowCut].def == kParams[kReverbLowCut].min, "cut filters default to off");
    CHECK(kParams[kDelayFeedback].def == 40 && kParams[kDelayTone].def == 4500 && kParams[kReverbPreDelay].def == 0, "space extras default to the 1.0 sound");
    for (uint32_t i = kTuneTranspose; i < kParamCount; ++i) CHECK(i > kLastOutput && !isOutput(i), "%s is an input after the outputs", kParams[i].symbol);
    // The off positions are exact bypasses: not a single sample changes.
    auto x = voice(120, 20000, 0.3f), a = x, b = x;
    PlosiveTamer p; p.prepare(SR); p.set(0.f); runInBlocks([&](float* l, float* r, int n) { p.process(l, r, n); }, a, b);
    CHECK(sameBits(a, x) && sameBits(b, x), "plosive tamer at 0 %% is bit-exact");
    Leveler lv; lv.prepare(SR); lv.set(0.f); a = x; b = x; runInBlocks([&](float* l, float* r, int n) { lv.process(l, r, n); }, a, b);
    CHECK(sameBits(a, x) && sameBits(b, x), "auto level at 0 %% is bit-exact");
    // A 1.0 preset (no 1.1 values) must load with the 1.1 parameters at their defaults.
    float v[kParamCount]; presetToValues(kFactoryPresets[0], v);
    CHECK(v[kPlosive] == 0 && v[kLevelAmount] == 0 && v[kTuneTranspose] == 0 && v[kTuneAmount] == 100 && v[kEqHighCut] == 20000, "old preset leaves new controls neutral");
}

static void testTranspose() {
    std::printf("tune: transpose and correction amount\n");
    auto run = [&](double f0, int st, float amount, std::vector<float>& out) {
        Chain c; c.prepare(SR); setAllOff(c);
        c.setParam(kTuneOn, 1); c.setParam(kTuneScale, 2); c.setParam(kTuneAmount, amount); c.setParam(kTuneTranspose, static_cast<float>(st));
        c.setParam(kTuneSpeed, 10); c.setParam(kTuneHumanize, 0);
        out = voice(f0, 96000, 0.3f); auto r = out; runChain(c, out, r);
    };
    std::vector<float> o;
    for (double f0 : {130.0, 200.0})
        for (int st : {-12, -5, 0, 7, 12}) {
            run(f0, st, 0.f, o);
            const double want = f0 * std::pow(2.0, st / 12.0);
            const double got = measureHz(o, 60000, 90000, want * 0.8, want * 1.25);
            CHECK(std::fabs(cents(got, want)) < 10.0, "%.0f Hz %+d st -> %.1f Hz (want %.1f, %.1f ct off)", f0, st, got, want, cents(got, want));
            CHECK(std::fabs(rmsDb(o, 60000, 90000) - rmsDb(voice(f0, 96000, 0.3f), 60000, 90000)) < 1.5f, "level preserved at %+d st", st);
        }
    // Correction amount: a 5 % flat 200 Hz note is pulled to the nearest semitone at 100 %, left alone at 0 %.
    const double flat = 196.0 * std::pow(2.0, -35.0 / 1200.0);   // G3, 35 cents flat
    run(flat, 0, 100.f, o); const double full = measureHz(o, 60000, 90000, 150, 260);
    run(flat, 0, 0.f, o);   const double none = measureHz(o, 60000, 90000, 150, 260);
    CHECK(std::fabs(cents(full, 196.0)) < 10.0 && std::fabs(cents(none, 196.0)) > 25.0, "amount 100%% corrects (%.1f Hz), 0%% does not (%.1f Hz)", full, none);
    CHECK(std::fabs(cents(none, flat)) < 5.0, "amount 0%% keeps the original pitch (%.1f Hz vs %.1f)", none, flat);
    // Breaths and consonants move with the voice instead of snapping back to the original pitch.
    std::mt19937 g(3); std::uniform_real_distribution<float> u(-1, 1);
    std::vector<float> noise(48000); for (auto& v : noise) v = u(g) * 0.1f;
    auto rel = [&](int st) {
        Chain c; c.prepare(SR); setAllOff(c); c.setParam(kTuneOn, 1); c.setParam(kTuneAmount, 0); c.setParam(kTuneTranspose, static_cast<float>(st));
        auto l = noise, r = noise; runChain(c, l, r);
        double e = 0, ref = 0; const int lat = c.latency();
        for (size_t i = 20000; i < 46000; ++i) { const double d = l[i] - noise[i - lat]; e += d * d; ref += static_cast<double>(noise[i - lat]) * noise[i - lat]; }
        return 10 * std::log10(e / ref + 1e-20);
    };
    CHECK(rel(0) < -60.0, "unvoiced sound passes straight through when not transposing: %.1f dB", rel(0));
    CHECK(rel(5) > -6.0 && rel(-5) > -6.0, "unvoiced sound is shifted too when transposing: %.1f / %.1f dB", rel(5), rel(-5));
    // Latency stays 10 ms however far it is shifted.
    { Chain c; c.prepare(SR); setAllOff(c); c.setParam(kTuneOn, 1); c.setParam(kTuneTranspose, 12);
      CHECK(c.latency() == static_cast<int>(std::lround(0.010 * SR)), "latency unchanged by transpose: %d", c.latency()); }
}

static void testPlosive() {
    std::printf("plosive tamer\n");
    // A pop: a 60 ms burst at 50 Hz under a quiet voice.
    auto withPop = [&](float voiceAmp) {
        auto x = voice(180, 48000, voiceAmp); const size_t s = 20000, n = static_cast<size_t>(0.060 * SR);
        for (size_t i = 0; i < n; ++i) x[s + i] += 0.5f * std::sin(static_cast<float>(kPi) * i / n) * std::sin(2.f * static_cast<float>(kPi) * 50.f * i / static_cast<float>(SR));
        return x;
    };
    for (float amount : {0.6f, 1.0f}) {
        auto x = withPop(0.05f), a = x, b = x;
        PlosiveTamer p; p.prepare(SR); p.set(amount); runInBlocks([&](float* l, float* r, int n) { p.process(l, r, n); }, a, b);
        const float before = lowBandDb(x, 20000, 22880), after = lowBandDb(a, 20000, 22880);
        CHECK(after < before - (amount > 0.9f ? 8.f : 5.f), "amount %.0f%%: pop lows reduced %.1f dB", amount * 100, before - after);
        CHECK(std::fabs(rmsDb(a, 30000, 48000) - rmsDb(x, 30000, 48000)) < 0.2f, "voice after the pop untouched (%.2f dB)", rmsDb(a, 30000, 48000) - rmsDb(x, 30000, 48000));
    }
    // Low sung notes are left alone even at 100 %.
    for (double f0 : {70.0, 85.0, 110.0, 160.0}) {
        auto x = voice(f0, 48000, 0.3f), a = x, b = x;
        PlosiveTamer p; p.prepare(SR); p.set(1.f); runInBlocks([&](float* l, float* r, int n) { p.process(l, r, n); }, a, b);
        CHECK(std::fabs(rmsDb(a, 12000, 48000) - rmsDb(x, 12000, 48000)) < 0.5f, "%.0f Hz sung note changes by %.2f dB", f0, rmsDb(a, 12000, 48000) - rmsDb(x, 12000, 48000));
    }
    // Switching it off glides: no click.
    { PlosiveTamer p; p.prepare(SR); p.set(1.f); auto x = withPop(0.05f), a = x, b = x; runInBlocks([&](float* l, float* r, int n) { p.process(l, r, n); }, a, b);
      p.set(0.f); const auto y0 = voice(180, 9600, 0.2f); auto y = y0, z = y0;
      p.process(y.data(), z.data(), static_cast<int>(y.size()));
      float jump = 0; for (size_t i = 1; i < y.size(); ++i) jump = std::max(jump, std::fabs((y[i] - y[i - 1]) - (y0[i] - y0[i - 1])));
      CHECK(jump < 0.02f, "no click when switched off (largest step difference %.4f)", jump); }
}

static void testLeveler() {
    std::printf("auto level\n");
    // 3 s quiet then 3 s loud.
    std::vector<float> x = voice(180, 144000, 0.02f); { auto loud = voice(180, 144000, 0.35f); x.insert(x.end(), loud.begin(), loud.end()); }
    const float inGap = rmsDb(x, 240000, 288000) - rmsDb(x, 96000, 144000);
    for (float amount : {0.5f, 1.0f}) {
        auto a = x, b = x; Leveler lv; lv.prepare(SR); lv.set(amount); runInBlocks([&](float* l, float* r, int n) { lv.process(l, r, n); }, a, b);
        const float gap = rmsDb(a, 240000, 288000) - rmsDb(a, 96000, 144000);
        CHECK(gap < inGap * (1.f - 0.4f * amount) , "amount %.0f%%: level gap %.1f dB -> %.1f dB", amount * 100, inGap, gap);
        CHECK(std::fabs(lv.gainDb()) <= Leveler::kMaxRideDb * amount + 0.01f, "ride limited to %.0f dB (now %.2f)", Leveler::kMaxRideDb * amount, lv.gainDb());
    }
    // No swell when the voice arrives: after silence (or after the amount is raised from 0) a voice that is
    // already at the target must leave the gain near 0 dB, and a louder one must never be lifted first.
    for (float levelDb : {-35.f, -20.f, -10.f}) {
        auto v = voice(180, 96000, 1.f); const float cur = rmsDb(v, 0, v.size());
        for (auto& t : v) t *= dbToLin(levelDb - cur);
        std::vector<float> x(48000, 0.f); x.insert(x.end(), v.begin(), v.end());
        auto b = x; Leveler lv; lv.prepare(SR); lv.set(1.f);
        float lo = 99.f, hi = -99.f;
        for (size_t pos = 0; pos + 16 <= x.size(); pos += 16) { lv.process(x.data() + pos, b.data() + pos, 16); if (pos >= 48000) { lo = std::min(lo, lv.gainDb()); hi = std::max(hi, lv.gainDb()); } }
        const float ideal = std::max(-Leveler::kMaxRideDb, std::min(Leveler::kMaxRideDb, Leveler::kTargetDb - levelDb));
        if (levelDb == -20.f) CHECK(lo > -0.5f && hi < 0.5f, "voice on target after silence: gain stays within +/-0.5 dB (%.2f .. %.2f)", lo, hi);
        else if (levelDb > -20.f) CHECK(hi < 0.2f && lo > ideal - 0.5f, "loud voice after silence is only ever eased (%.2f .. %.2f, ideal %.1f)", lo, hi, ideal);
        else CHECK(lo > -0.2f && hi <= ideal + 0.2f, "quiet voice after silence is lifted without overshoot (%.2f .. %.2f, ideal %.1f)", lo, hi, ideal);
    }
    // It holds still in pauses: loud phrase, then 3 s of room noise at -65 dBFS: gain must not creep up.
    { auto a = voice(180, 96000, 0.35f); std::mt19937 g(9); std::uniform_real_distribution<float> u(-1, 1);
      for (int i = 0; i < 144000; ++i) a.push_back(u(g) * 0.0006f);
      auto b = a; Leveler lv; lv.prepare(SR); lv.set(1.f);
      for (size_t pos = 0; pos < 96000; pos += 480) lv.process(a.data() + pos, b.data() + pos, 480);
      const float atPause = lv.gainDb();
      for (size_t pos = 96000; pos < a.size(); pos += 480) lv.process(a.data() + pos, b.data() + pos, 480);
      CHECK(atPause < -3.f, "loud phrase is eased (%.2f dB)", atPause);
      CHECK(std::fabs(lv.gainDb() - atPause) < 0.5f, "gain held through a pause: %.2f dB -> %.2f dB", atPause, lv.gainDb()); }
    // Switching it off while it is applying gain glides back to unity.
    { auto a = voice(180, 96000, 0.02f), b = a; Leveler lv; lv.prepare(SR); lv.set(1.f); runInBlocks([&](float* l, float* r, int n) { lv.process(l, r, n); }, a, b);
      const float was = lv.gainDb(); lv.set(0.f); auto y = voice(180, 48000, 0.02f), z = y;
      runInBlocks([&](float* l, float* r, int n) { lv.process(l, r, n); }, y, z);
      CHECK(was > 3.f && lv.gainDb() == 0.f, "boost %.1f dB then off -> %.3f dB", was, lv.gainDb());
      // Glide, not a jump: sample the gain every 16 samples while it returns to unity.
      Leveler lv2; lv2.prepare(SR); lv2.set(1.f); { auto q = voice(180, 96000, 0.02f), w = q; runInBlocks([&](float* l, float* r, int n) { lv2.process(l, r, n); }, q, w); }
      lv2.set(0.f); float prevG = lv2.gainDb(), worst = 0; auto q = voice(180, 48000, 0.02f), w = q;
      for (size_t pos = 0; pos + 16 <= q.size(); pos += 16) { lv2.process(q.data() + pos, w.data() + pos, 16); worst = std::max(worst, std::fabs(lv2.gainDb() - prevG)); prevG = lv2.gainDb(); }
      CHECK(worst < 0.1f, "gain glides back to unity (largest step %.3f dB per 16 samples)", worst); }
}

static void testSpaceExtras() {
    std::printf("space extras: delay feedback/tone, reverb pre-delay and cuts, EQ high-cut\n");
    const int N = 4 * 48000;
    auto burst = [&] { std::vector<float> l(N, 0.f); for (int i = 0; i < 480; ++i) l[i] = 0.5f * std::sin(2 * kPi * 500 * i / SR); return l; };
    auto energy = [&](const std::vector<float>& x, double t0, double t1) { double e = 0; for (size_t i = static_cast<size_t>(t0 * SR); i < static_cast<size_t>(t1 * SR) && i < x.size(); ++i) e += x[i] * x[i]; return 10 * std::log10(e + 1e-20); };
    // Delay: 1/4 note at 120 bpm = 0.5 s.  Feedback 0 leaves one echo, 85 keeps repeating.
    auto delayRun = [&](float fb, float tone) {
        Chain c; c.prepare(SR); setAllOff(c); c.setTempo(120); c.setParam(kSpaceOn, 1); c.setParam(kReverbMix, 0); c.setParam(kDelayMix, 100);
        c.setParam(kDelayTime, 5); c.setParam(kDelayDuck, 0); c.setParam(kDelayFeedback, fb); c.setParam(kDelayTone, tone);
        auto l = burst(), r = l; runChain(c, l, r); return l;
    };
    { const auto none = delayRun(0.f, 4500.f), lots = delayRun(85.f, 4500.f);
      CHECK(energy(none, 0.45, 0.65) > -45.0, "first echo present: %.1f dB", energy(none, 0.45, 0.65));
      CHECK(energy(none, 1.45, 3.9) < energy(none, 0.45, 0.65) - 30.0, "feedback 0 gives no repeats");
      CHECK(energy(lots, 1.45, 3.9) > energy(none, 1.45, 3.9) + 20.0, "feedback 85%% keeps repeating: %.1f vs %.1f dB", energy(lots, 1.45, 3.9), energy(none, 1.45, 3.9));
      // Tone darkens the repeats of broadband material: use a click-rich burst
      auto click = [&](float tone) { Chain c; c.prepare(SR); setAllOff(c); c.setTempo(120); c.setParam(kSpaceOn, 1); c.setParam(kReverbMix, 0); c.setParam(kDelayMix, 100);
          c.setParam(kDelayTime, 5); c.setParam(kDelayDuck, 0); c.setParam(kDelayFeedback, 80); c.setParam(kDelayTone, tone);
          std::vector<float> l(N, 0.f); l[10] = 0.8f; auto r = l; runChain(c, l, r); return l; };
      const auto dark = click(1000.f), bright = click(12000.f);
      auto hf = [&](const std::vector<float>& x) { Biquad h; h.set(BiquadCoef::highpass(4000, 0.707, SR)); double e = 0; for (size_t i = 0; i < x.size(); ++i) { const float y = h.process(x[i]); if (i > 72000) e += y * y; } return 10 * std::log10(e + 1e-20); };
      CHECK(hf(dark) < hf(bright) - 6.0, "lower delay tone darkens the repeats: %.1f vs %.1f dB", hf(dark), hf(bright)); }
    // Reverb pre-delay: nothing before it.
    auto rvRun = [&](float pre, float lo, float hi) {
        Chain c; c.prepare(SR); setAllOff(c); c.setParam(kSpaceOn, 1); c.setParam(kDelayMix, 0); c.setParam(kReverbMix, 100); c.setParam(kReverbDuck, 0);
        c.setParam(kSpaceType, 1); c.setParam(kReverbPreDelay, pre); c.setParam(kReverbLowCut, lo); c.setParam(kReverbHighCut, hi);
        std::vector<float> l(N, 0.f); l[10] = 0.8f; auto r = l; runChain(c, l, r); return l; };
    { const auto p0 = rvRun(0.f, 20.f, 20000.f), p80 = rvRun(80.f, 20.f, 20000.f);
      CHECK(energy(p80, 0.025, 0.080) < energy(p0, 0.025, 0.080) - 25.0, "pre-delay 80 ms empties 25-80 ms: %.1f vs %.1f dB", energy(p80, 0.025, 0.08), energy(p0, 0.025, 0.08));
      CHECK(energy(p80, 0.1, 0.5) > -40.0, "reverb still arrives after the pre-delay");
      auto band = [&](const std::vector<float>& x, bool high) { Biquad f; f.set(high ? BiquadCoef::highpass(4000, 0.707, SR) : BiquadCoef::lowpass(200, 0.707, SR)); double e = 0; for (size_t i = 0; i < x.size(); ++i) { const float y = f.process(x[i]); if (i > 4800) e += y * y; } return 10 * std::log10(e + 1e-20); };   // skip the dry impulse
      const auto base = rvRun(0.f, 20.f, 20000.f), dark = rvRun(0.f, 20.f, 2000.f), thin = rvRun(0.f, 400.f, 20000.f);
      CHECK(band(dark, true) < band(base, true) - 10.0, "reverb high-cut removes the top: %.1f vs %.1f dB", band(dark, true), band(base, true));
      CHECK(band(thin, false) < band(base, false) - 10.0, "reverb low-cut removes the bottom: %.1f vs %.1f dB", band(thin, false), band(base, false)); }
    // EQ high-cut: 24 dB/oct, flat below.
    { EqSettings s; s.low = 0; s.mid = 0; s.high = 0; s.air = 0; s.lowCut = 20; s.highCut = 3000;
      BiquadCoef h[2]; eqHighCutDesign(s, SR, h);
      auto mag = [&](double f) { return 20 * std::log10(h[0].magnitude(f, SR) * h[1].magnitude(f, SR)); };
      CHECK(std::fabs(mag(500)) < 0.3, "high-cut flat at 500 Hz: %.2f dB", mag(500));
      CHECK(std::fabs(mag(3000) + 3.0) < 0.7, "high-cut is -3 dB at its corner: %.2f dB", mag(3000));
      CHECK(mag(12000) < -45.0, "high-cut at 12 kHz: %.1f dB (4th order)", mag(12000));
      Chain c; c.prepare(SR); setAllOff(c); c.setParam(kEqOn, 1); c.setParam(kEqLow, 0); c.setParam(kEqMid, 0); c.setParam(kEqHigh, 0); c.setParam(kEqAir, 0); c.setParam(kEqLowCut, 20); c.setParam(kEqHighCut, 3000);
      std::vector<float> l(48000); for (size_t i = 0; i < l.size(); ++i) l[i] = 0.2f * std::sin(2 * kPi * 9000 * i / SR); auto r = l; const auto ref = l; runChain(c, l, r);
      CHECK(rmsDb(l, 24000, 48000) < rmsDb(ref, 24000, 48000) - 30.f, "9 kHz sine through the high-cut: %.1f dB", rmsDb(l, 24000, 48000) - rmsDb(ref, 24000, 48000)); }
}

// Every factory preset, including the 1.1 ones: finite, under the limiter, and neither silent nor huge.
static void testFactoryPresetsRender() {
    std::printf("factory presets render sanely\n");
    for (uint32_t k = 0; k < kNumFactoryPresets; ++k) {
        Chain c; c.prepare(SR);
        float v[kParamCount]; presetToValues(kFactoryPresets[k], v);
        for (uint32_t i = 0; i < kParamCount; ++i) c.setParam(i, v[i]);
        auto l = voice(150, 96000, 0.12f, 14), r = l;
        for (size_t i = 40000; i < 52000; ++i) { l[i] = 0.f; r[i] = 0.f; }   // a pause, so gates and riders get exercised
        runChain(c, l, r);
        bool fin = true; float pk = 0;
        for (size_t i = 0; i < l.size(); ++i) { fin &= std::isfinite(l[i]) && std::isfinite(r[i]); pk = std::max(pk, std::max(std::fabs(l[i]), std::fabs(r[i]))); }
        const float lvl = rmsDb(l, 60000, 96000);
        CHECK(fin, "%s finite", kFactoryPresets[k].name);
        CHECK(pk <= dbToLin(-1.f) + 1e-3f, "%s peak %.3f under the limiter", kFactoryPresets[k].name, pk);
        CHECK(lvl > -40.f && lvl < -6.f, "%s output level %.1f dB is sane", kFactoryPresets[k].name, lvl);
    }
}

static void testPresetStore() {
    std::printf("preset store + json\n");
    namespace fs = std::filesystem;
    const std::string base = "lstrip_test_store";
    fs::remove_all(base);
    PresetStore st(base + "/presets", base + "/config");
    float v[kParamCount]; for (uint32_t i = 0; i < kParamCount; ++i) v[i] = kParams[i].def;
    v[kIntensity] = 42; v[kEqAir] = -3.25f; v[kTuneOn] = 1;
    std::string err;
    CHECK(st.save("My \"Vocal\"/Chain", v, &err), "save: %s", err.c_str());
    CHECK(!st.save("Init", v, &err), "factory names are protected");
    CHECK(!st.save("   ", v, &err), "empty name rejected");
    PresetStore st2(base + "/presets", base + "/config"); st2.reload();
    const UserPreset* p = st2.find("My \"Vocal\"/Chain");
    CHECK(p && std::fabs(p->values[kIntensity] - 42) < 1e-4 && std::fabs(p->values[kEqAir] + 3.25f) < 1e-4 && p->values[kTuneOn] == 1, "roundtrip");
    CHECK(st2.rename("My \"Vocal\"/Chain", "Renamed", &err) && st2.find("Renamed") && !st2.find("My \"Vocal\"/Chain"), "rename");
    std::string imported; CHECK(st2.importFile(base + "/presets/Renamed.lstrip.json", &imported, &err) && imported != "Renamed", "import avoids name clash: %s", imported.c_str());
    // hostile files
    { std::ofstream f(base + "/presets/bad.lstrip.json"); f << "{\"format\":\"lstrip-preset\",\"name\":\"X\",\"params\":{\"intensity\":1e99,\"nonsense\":5,\"eq_air\":\"str\"}}"; }
    { std::ofstream f(base + "/presets/junk.lstrip.json"); f << "{{{{ not json"; }
    { std::ofstream f(base + "/presets/deep.lstrip.json"); f << std::string(5000, '['); }
    st2.reload();
    const UserPreset* x = st2.find("X");
    CHECK(x && x->values[kIntensity] == 100.f && x->values[kEqAir] == kParams[kEqAir].def, "values clamped / bad types defaulted");
    CHECK(st2.remove("Renamed") && !st2.find("Renamed"), "delete");
    st2.toggleStar("Podcast Clean"); st2.setDark(false);
    PresetStore st3(base + "/presets", base + "/config"); st3.loadConfig();
    CHECK(st3.starred("Podcast Clean") && !st3.dark(), "config roundtrip");
    Json j; CHECK(Json::parse("{\"a\":[1,2,{\"b\":\"\\u00e9\\n\"}],\"c\":true}", j) && j.get("a")->a.size() == 3 && j.get("c")->b, "json parse");
    CHECK(!Json::parse("[1,2", j) && !Json::parse("{\"a\":}", j) && !Json::parse("nan", j), "json rejects garbage");
    // export folder follows xdg user-dirs
    { fs::create_directories(base + "/cfg"); std::ofstream f(base + "/cfg/user-dirs.dirs"); f << "XDG_DOCUMENTS_DIR=\"$HOME/Docs\"\n"; }
    setenv("XDG_CONFIG_HOME", (std::filesystem::current_path() / (base + "/cfg")).c_str(), 1); setenv("HOME", "/home/tester", 1);
    CHECK(PresetStore::exportDir() == "/home/tester/Docs/L-STRIP", "export dir: %s", PresetStore::exportDir().c_str());
    CHECK(st3.exportTo("Out Name", v, base + "/exp", nullptr) && fs::exists(base + "/exp/Out Name.lstrip.json"), "export writes a file");
    fs::remove_all(base);
}

int main() {
    testPresetStore();
    testNewParamsNeutral(); testTranspose(); testPlosive(); testLeveler(); testSpaceExtras(); testFactoryPresetsRender();
    testParamTable(); testPassthrough(); testBlockSizes(); testGate(); testCompressor(); testDeEsser();
    testLimiter(); testGateAtIntensity(); testTuneAlignment(); testEq(); testMultiband(); testSpace(); testPitch(); testConcurrentParams(); testGarbageInput(); testFuzz();
    std::printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
