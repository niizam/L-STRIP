// SPDX-License-Identifier: MIT
// Headless DSP tests for L-STRIP.  No framework needed; returns non-zero on failure.
#include "../src/dsp/Chain.hpp"

#include <cstdio>
#include <cstdlib>
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
        CHECK(isOutput(i) == (i >= kNumInputParams), "outputs are last: %s", d.symbol);
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
    CHECK(c.latency() == static_cast<int>(std::lround(0.006 * SR)), "tune latency %d", c.latency());
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
        CHECK(c.latency() == static_cast<int>(std::lround(0.006 * SR)), "latency reported");
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

static void testFuzz() {
    std::printf("stability fuzz\n");
    Chain c; c.prepare(SR);
    std::mt19937 g(42); std::uniform_real_distribution<float> u(0, 1);
    auto src = voice(180, 4800, 0.5f);
    bool fin = true; float pk = 0;
    for (int iter = 0; iter < 300; ++iter) {
        for (uint32_t i = 0; i < kNumInputParams; ++i) {
            if (u(g) < 0.15f) c.setParam(i, kParams[i].min + u(g) * (kParams[i].max - kParams[i].min));
        }
        std::vector<float> l = src, r = src;
        for (auto& v : l) v *= (u(g) < 0.1f ? 4.f : 1.f);
        c.process(l.data(), r.data(), static_cast<int>(l.size()));
        for (size_t i = 0; i < l.size(); ++i) { fin &= std::isfinite(l[i]) && std::isfinite(r[i]); pk = std::max(pk, std::max(std::fabs(l[i]), std::fabs(r[i]))); }
    }
    CHECK(fin, "no NaN/Inf under random automation");
    CHECK(pk < 200.f, "bounded output (peak %.2f; +24 dB out gain sits after the limiter)", pk);
}

int main() {
    testParamTable(); testPassthrough(); testBlockSizes(); testGate(); testCompressor(); testDeEsser();
    testLimiter(); testEq(); testMultiband(); testSpace(); testPitch(); testFuzz();
    std::printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
