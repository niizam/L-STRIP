// SPDX-License-Identifier: MIT
// Backward-compatibility render.  Renders a fixed synthetic voice through every
// factory preset and through seeded random settings of the *original* v1.0
// parameters (ids below kMeterIn, which are permanent), then prints one FNV-1a
// hash of the raw float output per configuration.
//
// tools/compat-check.sh compiles this one file against an old revision's
// sources and against the working tree and requires every line the old build
// printed to appear, unchanged, in the new build's output: i.e. that sessions
// and presets made with the old version sound bit for bit the same.  Parameters
// added after that revision stay at their defaults, which is the point.
//
// It must keep compiling against old revisions: use only Chain, Presets.hpp
// and Params.hpp, and only through the API they had in v1.0.
#include "dsp/Chain.hpp"
#include "common/Presets.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace lstrip;
using namespace lstrip::dsp;

namespace {

uint64_t fnv(uint64_t h, const float* x, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        uint32_t b; std::memcpy(&b, &x[i], 4);
        for (int k = 0; k < 4; ++k) { h ^= (b >> (8 * k)) & 0xFF; h *= 1099511628211ull; }
    }
    return h;
}

struct Lcg {
    uint64_t s;
    float next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<float>((s >> 40) & 0xFFFFFF) / 16777216.f; }
};

// A gliding, vibrato-laden harmonic voice with phrase gaps, a sibilant burst and a little room noise.
void makeSignal(double sr, std::vector<float>& l, std::vector<float>& r) {
    const size_t n = static_cast<size_t>(sr * 6.0);
    l.assign(n, 0.f); r.assign(n, 0.f);
    Lcg rng{12345};
    double ph = 0;
    for (size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / sr;
        const double f0 = 140.0 * std::pow(2.0, (std::sin(2 * kPi * 0.35 * t) * 700 + std::sin(2 * kPi * 5.5 * t) * 40) / 1200.0);
        ph += 2 * kPi * f0 / sr;
        double v = 0;
        for (int h = 1; h <= 16; ++h) v += std::sin(h * ph) / h;
        const double pos = std::fmod(t, 1.5);
        const double env = pos < 1.0 ? 0.5 - 0.5 * std::cos(2 * kPi * pos) : 0.0;
        const double sib = (pos > 1.0 && pos < 1.12) ? 0.12 : 0.0;
        const float noise = rng.next() - 0.5f, room = (rng.next() - 0.5f) * 0.002f;
        l[i] = static_cast<float>(v * 0.22 * env + sib * noise + room);
        r[i] = static_cast<float>(v * 0.22 * env * 0.97 + sib * noise * 0.9 + room * 0.8);
    }
}

uint64_t renderOne(double sr, const float* values, int mode) {
    std::vector<float> l, r; makeSignal(sr, l, r);
    Chain c;
    c.prepare(sr);
    c.setTempo(97.0);
    for (uint32_t i = 0; i < kMeterIn; ++i) c.setParam(i, values[i]);
    size_t pos = 0; int step = 0;
    static const int blocks[] = {64, 480, 100, 1024, 333, 512, 17, 700};
    while (pos < l.size()) {
        const size_t nb = std::min<size_t>(static_cast<size_t>(blocks[step++ & 7]), l.size() - pos);
        c.process(l.data() + pos, r.data() + pos, static_cast<int>(nb));
        if (mode == 1 && (step % 40) == 0) {   // parameter changes while audio runs
            c.setParam(kIntensity, 50.f + 40.f * static_cast<float>((step / 40) & 1));
            c.setParam(kTuneOn, static_cast<float>((step / 40) & 1));
        }
        pos += nb;
    }
    uint64_t h = 1469598103934665603ull;
    h = fnv(h, l.data(), l.size());
    return fnv(h, r.data(), r.size());
}

} // namespace

int main() {
    float v[kParamCount];
    for (double sr : {48000.0, 44100.0}) {
        for (uint32_t k = 0; k < kNumFactoryPresets; ++k) {
            presetToValues(kFactoryPresets[k], v);
            std::printf("preset %-16s sr=%.0f  %016llx\n", kFactoryPresets[k].name, sr, static_cast<unsigned long long>(renderOne(sr, v, 0)));
            v[kTuneOn] = 1;   // same preset with Tune forced on and parameters moving mid-stream
            std::printf("preset %-16s sr=%.0f tune+nudge %016llx\n", kFactoryPresets[k].name, sr, static_cast<unsigned long long>(renderOne(sr, v, 1)));
        }
        Lcg rng{777};
        for (int n = 0; n < 24; ++n) {
            for (uint32_t i = 0; i < kMeterIn; ++i) {
                const ParamDef& d = kParams[i];
                float x = d.min + rng.next() * (d.max - d.min);
                if (d.kind == Kind::Bool) x = rng.next() > 0.35f ? 1.f : 0.f;
                if (d.kind == Kind::Enum || d.kind == Kind::Int) x = std::floor(x + 0.5f);
                v[i] = x;
            }
            v[kMasterOn] = 1;
            std::printf("random %02d sr=%.0f  %016llx\n", n, sr, static_cast<unsigned long long>(renderOne(sr, v, n & 1)));
        }
    }
    return 0;
}
