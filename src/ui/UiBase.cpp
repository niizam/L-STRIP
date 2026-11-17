// SPDX-License-Identifier: MIT
#include "UiBase.hpp"
#include "FontData.hpp"


#include <algorithm>
#include <cstdlib>

namespace lstrip::ui {

UiBase::UiBase() : UI(static_cast<uint>(kW), static_cast<uint>(kH)) {
    for (uint32_t i = 0; i < kParamCount; ++i) val_[i] = kParams[i].def;
    const FontBlob reg = interRegular(), bold = interSemiBold();
    font_[Regular] = createFontFromMemory("inter", reg.data, reg.size, false);
    font_[Bold] = createFontFromMemory("inter-semibold", bold.data, bold.size, false);
    setGeometryConstraints(static_cast<uint>(kW * 0.6f), static_cast<uint>(kH * 0.6f), true);
    // In-process formats: read meters straight from the DSP (see common/Shared.hpp).
    shared = SharedRegistry::instance().find(getPluginInstancePointer());
    if (std::getenv("LSTRIP_DEBUG")) std::fprintf(stderr, "[L-STRIP ui] direct DSP access: %s\n", shared ? "yes" : "no (using host parameter updates)");
}

void UiBase::set(uint32_t id, float v) {
    if (id >= kNumInputParams) return;
    const ParamDef& d = kParams[id];
    v = std::min(std::max(v, d.min), d.max);
    if (d.kind == Kind::Int || d.kind == Kind::Enum || d.kind == Kind::Bool) v = std::floor(v + 0.5f);
    if (v == val_[id]) return;
    val_[id] = v;
    setParameterValue(id, v);
}

float UiBase::toNorm(uint32_t id, float v) const {
    const ParamDef& d = kParams[id];
    if (d.flags & fLog) return std::log(std::max(v, d.min) / d.min) / std::log(d.max / d.min);
    return (v - d.min) / (d.max - d.min);
}
float UiBase::fromNorm(uint32_t id, float n) const {
    const ParamDef& d = kParams[id];
    n = std::min(std::max(n, 0.f), 1.f);
    if (d.flags & fLog) return d.min * std::pow(d.max / d.min, n);
    return d.min + n * (d.max - d.min);
}

std::string UiBase::format(uint32_t id, float v, bool withUnit) const {
    const ParamDef& d = kParams[id];
    char b[48];
    switch (d.kind) {
    case Kind::Bool: return v > 0.5f ? "On" : "Off";
    case Kind::Enum: { const int i = std::min(std::max(static_cast<int>(v + 0.5f - d.min), 0), static_cast<int>(d.max - d.min)); return d.labels[i]; }
    case Kind::Int: std::snprintf(b, sizeof b, "%d", static_cast<int>(v + 0.5f)); return b;
    default: break;
    }
    if (id == kGateThresh && v <= d.min + 0.5f) return "Off";
    if (id == kEqLowCut && v <= d.min + 1.f) return "Off";
    switch (d.unit) {
    case Unit::dB: {
        const bool duck = id == kReverbDuck || id == kDelayDuck, rng = id == kDeessRange;
        const bool thr = id == kGateThresh || id == kCompThresh;
        const bool trim = id == kCompTone || id == kMbOutput || id == kMbLow || id == kMbMid || id == kMbHigh || id == kInGain
                          || id == kOutGain || id == kEqLow || id == kEqMid || id == kEqHigh || id == kEqAir;
        if (duck || thr) std::snprintf(b, sizeof b, "%.0f", v);
        else if (rng) std::snprintf(b, sizeof b, "%.0f", -v);
        else if (trim) { if (std::fabs(v) < 0.05f) std::snprintf(b, sizeof b, "0.0"); else std::snprintf(b, sizeof b, "%+.1f", v); }
        else std::snprintf(b, sizeof b, "%.1f", v);
        std::string r = b;
        if (withUnit) r += " dB";
        return r;
    }
    case Unit::Percent: std::snprintf(b, sizeof b, withUnit ? "%.0f%%" : "%.0f", v); return b;
    case Unit::Hz:
        if (id == kChorusRate) std::snprintf(b, sizeof b, withUnit ? "%.2f Hz" : "%.2f", v);
        else std::snprintf(b, sizeof b, withUnit ? "%.0f Hz" : "%.0f", v);
        return b;
    case Unit::kHz: std::snprintf(b, sizeof b, withUnit ? "%.1f kHz" : "%.1f", v / 1000.f); return b;
    case Unit::ms: std::snprintf(b, sizeof b, withUnit ? "%.0f ms" : "%.0f", v); return b;
    default: std::snprintf(b, sizeof b, "%.2f", v); return b;
    }
}

void UiBase::txt(float x, float y, const std::string& s, float size, const Color& c, int align, Font f, float spacing) {
    fontFaceId(font_[f]);
    fontSize(size);
    textLetterSpacing(spacing);
    textAlign(align);
    fillColor(c);
    text(x, y, s.c_str(), nullptr);
    textLetterSpacing(0.f);
}

float UiBase::txtWidth(const std::string& s, float size, Font f, float spacing) {
    fontFaceId(font_[f]);
    fontSize(size);
    textLetterSpacing(spacing);
    DGL_NAMESPACE::Rectangle<float> r;
    textBounds(0, 0, s.c_str(), nullptr, r);
    textLetterSpacing(0.f);
    return r.getWidth();
}

void UiBase::pollShared() {
    static const bool dbg0 = std::getenv("LSTRIP_DEBUG") != nullptr;
    if (dbg0) { static double lastNo = 0; const double t = now(); if (!shared && t - lastNo > 3.0) { lastNo = t; std::fprintf(stderr, "[L-STRIP ui] no shared state bound yet\n"); } }
    if (!shared) return;
    static const bool dbg = std::getenv("LSTRIP_DEBUG") != nullptr;
    if (dbg) { static double last = 0; const double t = now(); if (t - last > 2.0) { last = t; std::fprintf(stderr, "[L-STRIP ui] meter_in %.1f dB  meter_out %.1f dB\n", shared->get(kMeterIn), shared->get(kMeterOut)); } }
    for (uint32_t i = 0; i < kParamCount; ++i) {
        const float v = shared->get(i);
        if (i >= kNumInputParams) { val_[i] = v; continue; }
        if (active == nullptr && v != val_[i]) val_[i] = v; // host automation / program change
    }
}

void UiBase::stateChanged(const char* key, const char* value) {
    if (std::getenv("LSTRIP_DEBUG")) std::fprintf(stderr, "[L-STRIP ui] stateChanged(%s, %s)\n", key, value);
    if (std::strcmp(key, "preset") == 0) {
        presetName = value ? value : "";
        onPresetStateFromHost(presetName);
    }
}

} // namespace lstrip::ui
