// SPDX-License-Identifier: MIT
#include "DistrhoPlugin.hpp"

#include "../common/Presets.hpp"
#include "../dsp/Chain.hpp"

START_NAMESPACE_DISTRHO

enum StateIndex { kStatePreset = 0, kStateCount };

class LStripPlugin : public Plugin {
public:
    LStripPlugin()
        : Plugin(lstrip::kParamCount, lstrip::kNumFactoryPresets, kStateCount)
    {
        chain_.prepare(getSampleRate());
        for (uint32_t i = 0; i < lstrip::kParamCount; ++i) values_[i] = lstrip::kParams[i].def;
    }

protected:
    const char* getLabel() const override { return "L-STRIP"; }
    const char* getDescription() const override {
        return "One-stop vocal chain: gate, pitch correction, compressor, de-esser, EQ, "
               "multiband, space and a safety limiter in a single window.";
    }
    const char* getMaker() const override { return "L-STRIP contributors"; }
    const char* getHomePage() const override { return "https://github.com/niizam/L-STRIP"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }
    int64_t getUniqueId() const override { return d_cconst('L', 'S', 't', 'p'); }

    // ------------------------------------------------------------- parameters
    void initParameter(uint32_t index, Parameter& p) override {
        const lstrip::ParamDef& d = lstrip::kParams[index];
        p.name = d.name;
        p.shortName = d.shortName;
        p.symbol = d.symbol;
        p.ranges.min = d.min; p.ranges.max = d.max; p.ranges.def = d.def;
        p.hints = 0;
        if (d.flags & lstrip::fOutput) p.hints |= kParameterIsOutput;
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
        default: p.unit = ""; break;
        }
        p.description = d.tip;
    }

    float getParameterValue(uint32_t index) const override { return values_[index]; }

    void setParameterValue(uint32_t index, float value) override {
        if (index >= lstrip::kNumInputParams) return;
        values_[index] = value;
        chain_.setParam(index, value);
        if (index == lstrip::kTuneOn || index == lstrip::kMasterOn) setLatency(static_cast<uint32_t>(chain_.latency()));
    }

    // --------------------------------------------------------------- programs
    void initProgramName(uint32_t index, String& name) override { name = lstrip::kFactoryPresets[index].name; }

    void loadProgram(uint32_t index) override {
        if (index >= lstrip::kNumFactoryPresets) return;
        float v[lstrip::kNumInputParams];
        lstrip::presetToValues(lstrip::kFactoryPresets[index], v);
        for (uint32_t i = 0; i < lstrip::kNumInputParams; ++i) setParameterValue(i, v[i]);
        presetName_ = lstrip::kFactoryPresets[index].name;
    }

    // ------------------------------------------------------------------ state
    void initState(uint32_t index, State& s) override {
        if (index == kStatePreset) { s.key = "preset"; s.label = "Preset name"; s.defaultValue = ""; s.hints = kStateIsHostReadable; }
    }
    String getState(const char* key) const override { return std::strcmp(key, "preset") == 0 ? presetName_ : String(); }
    void setState(const char* key, const char* value) override { if (std::strcmp(key, "preset") == 0) presetName_ = value; }

    // ------------------------------------------------------------------- audio
    void activate() override { chain_.reset(); setLatency(static_cast<uint32_t>(chain_.latency())); }
    void sampleRateChanged(double sr) override {
        chain_.prepare(sr);
        for (uint32_t i = 0; i < lstrip::kNumInputParams; ++i) chain_.setParam(i, values_[i]);
        setLatency(static_cast<uint32_t>(chain_.latency()));
    }

    void run(const float** in, float** out, uint32_t frames) override {
        const TimePosition& tp = getTimePosition();
        if (tp.bbt.valid) chain_.setTempo(tp.bbt.beatsPerMinute);
        if (out[0] != in[0]) std::memcpy(out[0], in[0], sizeof(float) * frames);
        if (out[1] != in[1]) std::memcpy(out[1], in[1], sizeof(float) * frames);
        chain_.process(out[0], out[1], static_cast<int>(frames));
        for (uint32_t i = lstrip::kNumInputParams; i < lstrip::kParamCount; ++i) values_[i] = chain_.output(i);
    }

private:
    lstrip::dsp::Chain chain_;
    float values_[lstrip::kParamCount];
    String presetName_;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LStripPlugin)
};

Plugin* createPlugin() { return new LStripPlugin(); }

END_NAMESPACE_DISTRHO
