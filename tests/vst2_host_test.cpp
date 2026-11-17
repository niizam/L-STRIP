// SPDX-License-Identifier: MIT
// Minimal VST2 host that loads the built L-STRIP-vst2.so and verifies the plugin
// glue (not just the DSP): entry point, parameter mapping, programs, latency
// reporting, state chunks and audio equality with the reference Chain.
//
// Usage: lstrip-vst2-host-test /path/to/plugin.so [mono]
#include "../src/dsp/Chain.hpp"
#include "../src/common/Presets.hpp"

#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace lstrip;

// --- the classic VST 2.4 binary layout (stable ABI, reproduced here for the test)
struct AEffect;
typedef intptr_t (*Dispatcher)(AEffect*, int32_t, int32_t, intptr_t, void*, float);
typedef void (*ProcessFn)(AEffect*, float**, float**, int32_t);
typedef void (*SetParamFn)(AEffect*, int32_t, float);
typedef float (*GetParamFn)(AEffect*, int32_t);
struct AEffect {
    int32_t magic;
    Dispatcher dispatcher;
    ProcessFn process;
    SetParamFn setParameter;
    GetParamFn getParameter;
    int32_t numPrograms, numParams, numInputs, numOutputs, flags;
    intptr_t resvd1, resvd2;
    int32_t initialDelay, realQualities, offQualities;
    float ioRatio;
    void* object; void* user;
    int32_t uniqueID, version;
    ProcessFn processReplacing;
    ProcessFn processDoubleReplacing;
    char future[56];
};
typedef AEffect* (*EntryFn)(intptr_t (*)(AEffect*, int32_t, int32_t, intptr_t, void*, float));

enum { effOpen = 0, effClose = 1, effSetProgram = 2, effGetProgram = 3, effGetProgramName = 5, effGetParamName = 8,
       effSetSampleRate = 10, effSetBlockSize = 11, effMainsChanged = 12, effGetChunk = 23, effSetChunk = 24,
       effGetEffectName = 45, effGetVendorString = 47 };

static intptr_t hostCallback(AEffect*, int32_t opcode, int32_t, intptr_t, void*, float) {
    if (opcode == 1) return 2400;   // audioMasterVersion
    return 0;
}

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) ++g_pass; else { ++g_fail; std::printf("  FAIL line %d: %s  ", __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static float norm(uint32_t id, float v) { return (v - kParams[id].min) / (kParams[id].max - kParams[id].min); } // linear (DPF maps by range)

static void applyValues(AEffect* fx, const float* v) {
    for (uint32_t i = 0; i < kNumInputParams; ++i) fx->setParameter(fx, static_cast<int>(i), norm(i, v[i]));
}

struct Plugin {
    void* lib = nullptr; AEffect* fx = nullptr;
    bool load(const char* path) {
        lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!lib) { std::printf("dlopen failed: %s\n", dlerror()); return false; }
        EntryFn entry = reinterpret_cast<EntryFn>(dlsym(lib, "VSTPluginMain"));
        if (!entry) entry = reinterpret_cast<EntryFn>(dlsym(lib, "main"));
        if (!entry) { std::printf("no VST entry point\n"); return false; }
        fx = entry(hostCallback);
        return fx != nullptr;
    }
    void start(double sr, int block) {
        fx->dispatcher(fx, effOpen, 0, 0, nullptr, 0);
        fx->dispatcher(fx, effSetSampleRate, 0, 0, nullptr, static_cast<float>(sr));
        fx->dispatcher(fx, effSetBlockSize, 0, block, nullptr, 0);
        fx->dispatcher(fx, effMainsChanged, 0, 1, nullptr, 0);
    }
    void stop() { if (fx) { fx->dispatcher(fx, effMainsChanged, 0, 0, nullptr, 0); fx->dispatcher(fx, effClose, 0, 0, nullptr, 0); fx = nullptr; } }
};

static std::vector<float> voice(double f0, size_t n, double sr) {
    std::vector<float> x(n); double ph = 0;
    for (size_t i = 0; i < n; ++i) { ph += 2 * M_PI * f0 / sr; double v = 0; for (int h = 1; h <= 10; ++h) v += std::sin(h * ph) / h; x[i] = static_cast<float>(v) * 0.3f; }
    return x;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: %s plugin.so\n", argv[0]); return 2; }
    const double SR = 48000.0; const int BLOCK = 512;
    const bool mono = argc > 2 && std::strcmp(argv[2], "mono") == 0;
    Plugin p;
    if (!p.load(argv[1])) return 1;
    AEffect* fx = p.fx;

    std::printf("metadata\n");
    CHECK(fx->magic == 0x56737450, "magic 'VstP' (got 0x%x)", fx->magic);
    CHECK(fx->numParams == static_cast<int>(kNumInputParams), "VST2 exposes only input params (%d vs %u); outputs stay internal", fx->numParams, kNumInputParams);
    CHECK(fx->numInputs == (mono ? 1 : 2) && fx->numOutputs == (mono ? 1 : 2), "%s in/out (%d/%d)", mono ? "mono" : "stereo", fx->numInputs, fx->numOutputs);
        CHECK((fx->flags & 1) != 0, "has editor");
    CHECK((fx->flags & 16) != 0, "supports processReplacing");
    p.start(SR, BLOCK);
    char buf[256] = {};
    fx->dispatcher(fx, effGetEffectName, 0, 0, buf, 0);
    CHECK(std::strcmp(buf, mono ? "L-STRIP Mono" : "L-STRIP") == 0, "effect name '%s'", buf);

    std::printf("parameter mapping\n");
    // All inputs precede all outputs (VST2 requirement) and round-trip through normalised values.
    for (uint32_t i = 0; i < kNumInputParams; ++i) {
        const float tgt = kParams[i].kind == Kind::Float ? 0.37f : norm(i, kParams[i].min + std::floor((kParams[i].max - kParams[i].min) * 0.5f));
        fx->setParameter(fx, static_cast<int>(i), tgt);
        const float back = fx->getParameter(fx, static_cast<int>(i));
        CHECK(std::fabs(back - tgt) < 1e-5f, "%s round trip %.4f -> %.4f", kParams[i].symbol, tgt, back);
    }
    for (uint32_t i = 0; i < kParamCount; ++i) {
        char name[64] = {};
        fx->dispatcher(fx, effGetParamName, static_cast<int32_t>(i), 0, name, 0);
        CHECK(name[0] != 0, "param %u has a name", i);
    }

    std::printf("programs\n");
    // DPF's VST2 wrapper exposes a single program (presets live in the editor there);
    // LV2/VST3/CLAP expose the factory programs.  Only verify programs when exposed.
    if (fx->numPrograms > 1) {
        for (uint32_t k = 0; k < kNumFactoryPresets; ++k) {
            fx->dispatcher(fx, effSetProgram, 0, static_cast<intptr_t>(k), nullptr, 0);
            float v[kNumInputParams]; presetToValues(kFactoryPresets[k], v);
            for (uint32_t i = 0; i < kNumInputParams; ++i) CHECK(std::fabs(fx->getParameter(fx, static_cast<int>(i)) - norm(i, v[i])) < 1e-4f, "program %u param %s", k, kParams[i].symbol);
        }
    } else std::printf("  (single program exposed by the VST2 wrapper: skipped)\n");

    std::printf("audio equals the reference chain (all presets, odd block sizes, in-place)\n");
    for (uint32_t k = 0; k < kNumFactoryPresets; ++k) {
        float v[kNumInputParams]; presetToValues(kFactoryPresets[k], v);
        applyValues(fx, v);
        dsp::Chain ref; ref.prepare(SR);
        for (uint32_t i = 0; i < kNumInputParams; ++i) ref.setParam(i, v[i]);
        const std::vector<float> src = voice(180.0 + 7.0 * k, 24000, SR);
        std::vector<float> rl = src, rr = src, ol(src.size()), orr(src.size());
        ref.process(rl.data(), rr.data(), static_cast<int>(rl.size()));
        if (mono) for (size_t i = 0; i < rl.size(); ++i) rl[i] = 0.5f * (rl[i] + rr[i]);   // the mono build folds L/R
        // fresh plugin state per preset: reset via suspend/resume
        fx->dispatcher(fx, effMainsChanged, 0, 0, nullptr, 0);
        fx->dispatcher(fx, effMainsChanged, 0, 1, nullptr, 0);
        const int blocks[3] = {512, 77, 1024};
        size_t pos = 0; int bi = 0; double maxErr = 0;
        while (pos < src.size()) {
            const int n = static_cast<int>(std::min<size_t>(blocks[bi++ % 3], src.size() - pos));
            // The mono build must never touch a second channel: give it a null pointer (obs-vst hands out
            // uninitialised memory there).  Out-of-place for mono, in-place for stereo is covered below.
            float* in[2] = {const_cast<float*>(src.data()) + pos, mono ? nullptr : const_cast<float*>(src.data()) + pos};
            float* out[2] = {ol.data() + pos, mono ? nullptr : orr.data() + pos};
            fx->processReplacing(fx, in, out, n);
            pos += n;
        }
        // the plugin's initial program load happens before audio; the reference chain started from defaults,
        // so allow the initial parameter fade-in (5 ms) to settle: compare the second half
        for (size_t i = src.size() / 2; i < src.size(); ++i) maxErr = std::max(maxErr, static_cast<double>(std::fabs(ol[i] - rl[i])));
        CHECK(maxErr < 2e-2, "preset '%s': plugin vs reference max error %.5f", kFactoryPresets[k].name, maxErr);
    }

    std::printf("latency reporting\n");
    { float v[kNumInputParams]; presetToValues(kFactoryPresets[0], v); applyValues(fx, v); }  // Podcast Clean: tune off
    float in0[2][512] = {}, out0[2][512] = {};
    float* ip[2] = {in0[0], in0[1]}; float* op[2] = {out0[0], out0[1]};
    fx->processReplacing(fx, ip, op, 512);
    CHECK(fx->initialDelay == 0, "no latency with tune off (got %d)", fx->initialDelay);
    fx->setParameter(fx, static_cast<int>(kTuneOn), 1.f);
    fx->processReplacing(fx, ip, op, 512);
    CHECK(fx->initialDelay == static_cast<int>(std::lround(0.010 * SR)), "10 ms latency with tune on (got %d)", fx->initialDelay);
    fx->setParameter(fx, static_cast<int>(kTuneOn), 0.f);
    fx->processReplacing(fx, ip, op, 512);
    CHECK(fx->initialDelay == 0, "latency returns to 0 (got %d)", fx->initialDelay);

    std::printf("state chunk round trip\n");
    { float v[kNumInputParams]; presetToValues(kFactoryPresets[3], v); applyValues(fx, v); }  // Stream Bright
    fx->setParameter(fx, static_cast<int>(kEqAir), 0.123f);
    void* chunk = nullptr;
    const intptr_t clen = fx->dispatcher(fx, effGetChunk, 0, 0, &chunk, 0);
    CHECK(clen > 0 && chunk != nullptr, "chunk produced (%ld bytes)", static_cast<long>(clen));
    std::vector<unsigned char> saved(static_cast<unsigned char*>(chunk), static_cast<unsigned char*>(chunk) + (clen > 0 ? clen : 0));
    Plugin q;
    if (q.load(argv[1])) {
        q.start(SR, BLOCK);
        q.fx->dispatcher(q.fx, effSetChunk, 0, static_cast<intptr_t>(saved.size()), saved.data(), 0);
        bool same = true;
        for (uint32_t i = 0; i < kNumInputParams; ++i) if (std::fabs(q.fx->getParameter(q.fx, static_cast<int>(i)) - fx->getParameter(fx, static_cast<int>(i))) > 1e-5f) { same = false; std::printf("    mismatch %s\n", kParams[i].symbol); break; }
        CHECK(same, "chunk restores every parameter into a second instance");
        q.stop();
    }

    std::printf("multiple instances are independent\n");
    {
        Plugin a, b; a.load(argv[1]); b.load(argv[1]); a.start(SR, BLOCK); b.start(SR, BLOCK);
        a.fx->setParameter(a.fx, static_cast<int>(kEqAir), 1.f);
        b.fx->setParameter(b.fx, static_cast<int>(kEqAir), 0.f);
        CHECK(a.fx->getParameter(a.fx, static_cast<int>(kEqAir)) == 1.f && b.fx->getParameter(b.fx, static_cast<int>(kEqAir)) == 0.f, "instances do not share parameters");
        a.stop(); b.stop();
    }

    p.stop();
    std::printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
