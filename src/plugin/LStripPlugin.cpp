// SPDX-License-Identifier: MIT
#include "DistrhoPlugin.hpp"
#include "DistrhoPluginUtils.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "../common/Presets.hpp"
#include "../common/Shared.hpp"
#include "../dsp/Chain.hpp"

START_NAMESPACE_DISTRHO

enum StateIndex { kStatePreset = 0, kStateCount };

class LStripPlugin : public Plugin {
public:
    LStripPlugin()
        : Plugin(lstrip::kParamCount, lstrip::kNumFactoryPresets, kStateCount)
    {
        // CLAP hosts must restart the plugin to apply a latency change, and DPF's CLAP wrapper requests that
        // restart whenever the value changes while active.  Toggling Tune would glitch the whole session, so in
        // CLAP the Tune latency is not reported (the dry path stays internally aligned).  See Documentation/dsp.rst.
        reportsLatency_ = std::strcmp(getPluginFormatName(), "CLAP") != 0;
        chain_.prepare(getSampleRate());
        shared_ = std::make_shared<lstrip::SharedState>();
        lstrip::SharedRegistry::instance().add(static_cast<Plugin*>(this), shared_);
        for (uint32_t i = 0; i < lstrip::kParamCount; ++i) values_[i] = lstrip::kParams[i].def;
    }

    ~LStripPlugin() override { lstrip::SharedRegistry::instance().remove(static_cast<Plugin*>(this)); }

protected:
    const char* getLabel() const override { return "L-STRIP"; }
    const char* getDescription() const override {
        return "One-stop vocal chain: gate, pitch correction, compressor, de-esser, EQ, "
               "multiband, space and a safety limiter in a single window. "
               "Built with the DISTRHO Plugin Framework (DPF, ISC licence).";
    }
    const char* getMaker() const override { return "L-STRIP contributors"; }
    const char* getHomePage() const override { return "https://github.com/niizam/L-STRIP"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(1, 1, 0); }
    int64_t getUniqueId() const override { return d_cconst('L', 'S', 't', 'p'); }

    // ------------------------------------------------------------- parameters
    // DPF's parameter index (`index` below) is the host-facing order: inputs first, outputs last.  It differs from
    // the permanent L-STRIP id (see idToHost/hostToId in Params.hpp); everything internal uses the id.
    void initParameter(uint32_t index, Parameter& p) override {
        const lstrip::ParamDef& d = lstrip::kParams[lstrip::hostToId(index)];
        p.name = d.name;
        p.shortName = d.shortName;
        p.symbol = d.symbol;
        p.ranges.min = d.min; p.ranges.max = d.max; p.ranges.def = d.def;
        p.hints = 0;
        if (d.flags & lstrip::fOutput) p.hints |= kParameterIsOutput | kParameterIsHidden; // meters: not for hosts' parameter lists
        else p.hints |= kParameterIsAutomatable;
        if (d.flags & lstrip::fLog) p.hints |= kParameterIsLogarithmic;
        switch (d.kind) {
        case lstrip::Kind::Bool: p.hints |= kParameterIsBoolean | kParameterIsInteger; break;
        case lstrip::Kind::Int: p.hints |= kParameterIsInteger; break;
        case lstrip::Kind::Enum: {
            p.hints |= kParameterIsInteger;
            const uint8_t n = static_cast<uint8_t>(d.max - d.min + 1);
            p.enumValues.count = n;
            p.enumValues.restrictedMode = true;
            p.enumValues.values = new ParameterEnumerationValue[n];
            p.enumValues.deleteLater = true;
            for (uint8_t i = 0; i < n; ++i) { p.enumValues.values[i].value = d.min + i; p.enumValues.values[i].label = d.labels[i]; }
            break;
        }
        default: break;
        }
        switch (d.unit) {
        case lstrip::Unit::dB: p.unit = "dB"; break;
        case lstrip::Unit::Percent: p.unit = "%"; break;
        case lstrip::Unit::Hz: case lstrip::Unit::kHz: p.unit = "Hz"; break;
        case lstrip::Unit::ms: p.unit = "ms"; break;
        case lstrip::Unit::St: p.unit = "st"; break;
        default: p.unit = ""; break;
        }
        p.description = d.tip;
    }

    float getParameterValue(uint32_t index) const override { return values_[lstrip::hostToId(index)]; }

    void setParameterValue(uint32_t index, float value) override {
        const uint32_t id = lstrip::hostToId(index);
        if (id >= lstrip::kParamCount || lstrip::isOutput(id)) return;
        values_[id] = value;
        shared_->set(id, value);
        chain_.setParam(id, value);
    }

    // --------------------------------------------------------------- programs
    void initProgramName(uint32_t index, String& name) override { name = lstrip::kFactoryPresets[index].name; }

    void loadProgram(uint32_t index) override {
        if (index >= lstrip::kNumFactoryPresets) return;
        float v[lstrip::kParamCount];
        lstrip::presetToValues(lstrip::kFactoryPresets[index], v);
        for (uint32_t id = 0; id < lstrip::kParamCount; ++id)
            if (!lstrip::isOutput(id)) setParameterValue(lstrip::idToHost(id), v[id]);
        presetName_ = lstrip::kFactoryPresets[index].name;
    }

    // ------------------------------------------------------------------ state
    void initState(uint32_t index, State& s) override {
        if (index == kStatePreset) { s.key = "preset"; s.label = "Preset name"; s.defaultValue = ""; s.hints = kStateIsHostReadable; }
    }
    String getState(const char* key) const override { return std::strcmp(key, "preset") == 0 ? presetName_ : String(); }
    void setState(const char* key, const char* value) override { if (std::strcmp(key, "preset") == 0) presetName_ = value; }

    // ------------------------------------------------------------------- audio
    void activate() override { chain_.reset(); reportLatency(); }
    void sampleRateChanged(double sr) override {
        chain_.prepare(sr);
        for (uint32_t id = 0; id < lstrip::kParamCount; ++id) chain_.setParam(id, values_[id]);   // outputs are ignored
        reportLatency();
    }

    void run(const float** in, float** out, uint32_t frames) override {
        const TimePosition& tp = getTimePosition();
        if (tp.bbt.valid) chain_.setTempo(tp.bbt.beatsPerMinute);
        reportLatency(); // setLatency() is only legal from the constructor, activate() and run()
       #if DISTRHO_PLUGIN_NUM_INPUTS == 1
        // Mono variant: the chain is stereo internally.  Feed the mono input to both sides and fold the
        // (reverb/chorus-widened) result back to one channel.  Never touches a second host buffer, so it is
        // safe in hosts that hand out uninitialised memory for missing channels (see Documentation/obs.rst).
        for (uint32_t pos = 0; pos < frames;) {
            const uint32_t n = std::min<uint32_t>(frames - pos, kMonoChunk);
            std::memcpy(monoL_, in[0] + pos, sizeof(float) * n);
            std::memcpy(monoR_, in[0] + pos, sizeof(float) * n);
            chain_.process(monoL_, monoR_, static_cast<int>(n));
            for (uint32_t i = 0; i < n; ++i) out[0][pos + i] = 0.5f * (monoL_[i] + monoR_[i]);
            pos += n;
        }
       #else
        if (out[0] != in[0]) std::memcpy(out[0], in[0], sizeof(float) * frames);
        if (out[1] != in[1]) std::memcpy(out[1], in[1], sizeof(float) * frames);
        chain_.process(out[0], out[1], static_cast<int>(frames));
       #endif
        for (uint32_t id = lstrip::kFirstOutput; id <= lstrip::kLastOutput; ++id) { values_[id] = chain_.output(id); shared_->set(id, values_[id]); }
    }

private:
   #if DISTRHO_PLUGIN_NUM_INPUTS == 1
    static constexpr uint32_t kMonoChunk = 512;
    float monoL_[kMonoChunk], monoR_[kMonoChunk];
   #endif
    void reportLatency() {
        const int lat = reportsLatency_ ? chain_.latency() : 0;
        if (lat != reportedLatency_) { reportedLatency_ = lat; setLatency(static_cast<uint32_t>(lat)); }
    }
    int reportedLatency_ = -1;
    bool reportsLatency_ = true;
    std::shared_ptr<lstrip::SharedState> shared_;
    lstrip::dsp::Chain chain_;
    float values_[lstrip::kParamCount];
    String presetName_;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LStripPlugin)
};

Plugin* createPlugin() { return new LStripPlugin(); }

END_NAMESPACE_DISTRHO
