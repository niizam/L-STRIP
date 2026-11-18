// SPDX-License-Identifier: MIT
// Factory presets.  A preset is a list of overrides applied on top of the
// parameter defaults (so adding parameters later never breaks old presets).
#pragma once
#include "Params.hpp"

namespace lstrip {

struct PresetValue { ParamId id; float value; };

struct FactoryPreset {
    const char* name;
    const char* category;
    const PresetValue* values;
    uint32_t count;
};


namespace presets {

inline constexpr PresetValue init[] = {{kIntensity, 100}};

inline constexpr PresetValue podcastClean[] = {
    {kIntensity, 70}, {kGateThresh, -56}, {kOutSat, 15},
};
inline constexpr PresetValue streamBright[] = {
    {kIntensity, 85}, {kGateThresh, -52}, {kOutSat, 20},
    {kDynStyle, 1}, {kCompThresh, -20}, {kCompMakeup, 5.5f}, {kCompTone, 1.5f}, {kCompMode, 0},
    {kDeessAmount, 55}, {kDeessFreq, 7000},
    {kEqLow, 1.0f}, {kEqMid, -2.0f}, {kEqHigh, 2.0f}, {kEqAir, 4.5f}, {kEqStyle, 1},
    {kMbStyle, 1}, {kMbDepth, 55}, {kMbUpward, 40}, {kMbDown, 60},
    {kReverbMix, 8}, {kSpaceType, 2}, {kReverbDuck, -9},
};
inline constexpr PresetValue radioVoice[] = {
    {kIntensity, 100}, {kGateThresh, -48}, {kOutSat, 25},
    {kDynStyle, 2}, {kCompThresh, -24}, {kCompMakeup, 7.0f}, {kCompTone, -1.0f}, {kCompMode, 2},
    {kDeessAmount, 50},
    {kEqLow, 3.0f}, {kEqMid, -1.0f}, {kEqHigh, 2.5f}, {kEqAir, 1.5f}, {kEqLowCut, 110}, {kEqStyle, 2},
    {kMbStyle, 2}, {kMbDepth, 35}, {kMbUpward, 30}, {kMbDown, 70},
    {kReverbMix, 0},
};
inline constexpr PresetValue singerPolish[] = {
    {kIntensity, 90}, {kGateThresh, -62}, {kOutSat, 10},
    {kDynStyle, 0}, {kCompThresh, -20}, {kCompMakeup, 4.0f}, {kCompMode, 1},
    {kDeessAmount, 45},
    {kEqLow, 1.5f}, {kEqMid, -1.0f}, {kEqHigh, 1.0f}, {kEqAir, 4.0f}, {kEqLowCut, 80},
    {kMbStyle, 0}, {kMbDepth, 30},
    {kSpaceType, 1}, {kReverbMix, 24}, {kDelayMix, 12}, {kDelayTime, 5}, {kChorusMix, 18}, {kChorusVoices, 3},
};
inline constexpr PresetValue naturalTune[] = {
    {kIntensity, 85}, {kGateThresh, -56}, {kOutSat, 12},
    {kTuneOn, 1}, {kTuneKey, 0}, {kTuneScale, 0}, {kTuneSpeed, 90}, {kTuneHumanize, 40}, {kTuneMix, 100},
};
inline constexpr PresetValue robotVoice[] = {
    {kIntensity, 100}, {kGateThresh, -52}, {kOutSat, 30},
    {kTuneOn, 1}, {kTuneKey, 0}, {kTuneScale, 2}, {kTuneSpeed, 0}, {kTuneHumanize, 0}, {kTuneMix, 100},
    {kMbStyle, 1}, {kMbDepth, 40},
    {kDelayMix, 20}, {kDelayTime, 2}, {kChorusMix, 35}, {kChorusVoices, 2}, {kReverbMix, 12},
};
inline constexpr PresetValue warmIntimate[] = {
    {kIntensity, 80}, {kGateThresh, -58}, {kOutSat, 20},
    {kCompThresh, -22}, {kCompMakeup, 5.0f}, {kCompTone, -2.0f}, {kCompMode, 1},
    {kDeessAmount, 35},
    {kEqLow, 3.0f}, {kEqMid, -1.0f}, {kEqHigh, 0.0f}, {kEqAir, 1.5f}, {kEqLowCut, 80},
    {kMbDepth, 35}, {kReverbMix, 6},
};
inline constexpr PresetValue dryTight[] = {
    {kIntensity, 100}, {kGateThresh, -50}, {kOutSat, 5},
    {kCompThresh, -22}, {kCompMakeup, 5.0f}, {kCompMode, 0},
    {kSpaceOn, 0}, {kMbDepth, 30}, {kEqAir, 2.0f},
};
inline constexpr PresetValue bigHall[] = {
    {kIntensity, 90}, {kGateThresh, -60}, {kOutSat, 10},
    {kSpaceType, 1}, {kReverbMix, 38}, {kReverbDuck, -8}, {kDelayMix, 18}, {kDelayTime, 4}, {kDelayDuck, -12},
    {kChorusMix, 12}, {kEqAir, 4.0f}, {kMbDepth, 35},
};
inline constexpr PresetValue deessOnly[] = {
    {kIntensity, 100}, {kGateThresh, -80}, {kOutSat, 0},
    {kCompMix, 0}, {kDeessAmount, 60}, {kDeessRange, 15},
    {kEqOn, 0}, {kMbOn, 0}, {kSpaceOn, 0},
};

// ---- Added in 1.1.  They use the controls added in 1.1 and are appended after "Init" so existing program
// indices stay where they were.
inline constexpr PresetValue chatVoice[] = {
    {kIntensity, 80}, {kGateThresh, -54}, {kOutSat, 8}, {kLevelAmount, 45}, {kPlosive, 45},
    {kCompThresh, -20}, {kCompMakeup, 4.0f}, {kDeessAmount, 45},
    {kEqLow, 1.5f}, {kEqMid, -1.5f}, {kEqHigh, 1.5f}, {kEqAir, 2.0f}, {kEqLowCut, 100},
    {kMbDepth, 30}, {kReverbMix, 0},
};
inline constexpr PresetValue whisper[] = {
    {kIntensity, 100}, {kGateThresh, -80}, {kLevelAmount, 65}, {kPlosive, 70},
    {kCompThresh, -32}, {kCompMakeup, 6.0f}, {kCompMode, 1}, {kDeessAmount, 30}, {kDeessRange, 8},
    {kEqLow, 0.0f}, {kEqMid, -1.0f}, {kEqHigh, 1.5f}, {kEqAir, 4.0f}, {kEqLowCut, 70},
    {kMbDepth, 35}, {kMbUpward, 50}, {kReverbMix, 0},
};
inline constexpr PresetValue telephone[] = {
    {kIntensity, 100}, {kGateThresh, -52}, {kOutSat, 45},
    {kDynStyle, 2}, {kCompThresh, -26}, {kCompMakeup, 7.0f},
    {kEqStyle, 1}, {kEqLow, -12.0f}, {kEqMid, 5.0f}, {kEqHigh, 0.0f}, {kEqAir, 0.0f},
    {kEqLowCut, 300}, {kEqHighCut, 3400},
    {kMbDepth, 25}, {kSpaceOn, 0},
};
inline constexpr PresetValue megaphone[] = {
    {kIntensity, 100}, {kGateThresh, -50}, {kOutSat, 65},
    {kDynStyle, 2}, {kCompThresh, -28}, {kCompMakeup, 9.0f},
    {kEqStyle, 1}, {kEqLow, -6.0f}, {kEqMid, 6.0f}, {kEqHigh, 3.0f}, {kEqAir, -3.0f},
    {kEqLowCut, 280}, {kEqHighCut, 5200},
    {kMbStyle, 2}, {kMbDepth, 50},
    {kSpaceType, 0}, {kReverbMix, 12}, {kReverbLowCut, 250}, {kReverbHighCut, 4500},
    {kDelayMix, 10}, {kDelayTime, 2}, {kDelayFeedback, 30}, {kDelayTone, 3000},
};
inline constexpr PresetValue smallRoom[] = {
    {kIntensity, 100}, {kGateThresh, -58},
    {kSpaceType, 0}, {kReverbMix, 42}, {kReverbDuck, -2}, {kReverbLowCut, 160}, {kReverbHighCut, 11000},
};
inline constexpr PresetValue cavern[] = {
    {kIntensity, 100}, {kGateThresh, -58},
    {kSpaceType, 1}, {kReverbMix, 40}, {kReverbDuck, -4}, {kReverbPreDelay, 35}, {kReverbLowCut, 130}, {kReverbHighCut, 3800},
    {kDelayMix, 38}, {kDelayTime, 5}, {kDelayFeedback, 68}, {kDelayTone, 2200}, {kDelayDuck, -4},
    {kEqLowCut, 130}, {kEqAir, -1.0f},
};
inline constexpr PresetValue helium[] = {
    {kIntensity, 100}, {kGateThresh, -56}, {kOutSat, 5},
    {kTuneOn, 1}, {kTuneAmount, 0}, {kTuneTranspose, 8}, {kTuneMix, 100},
    {kEqLowCut, 120}, {kEqAir, 4.0f}, {kReverbMix, 8},
};
inline constexpr PresetValue giant[] = {
    {kIntensity, 100}, {kGateThresh, -56}, {kOutSat, 35},
    {kTuneOn, 1}, {kTuneAmount, 0}, {kTuneTranspose, -8}, {kTuneMix, 100},
    {kEqLow, 4.0f}, {kEqHigh, -1.0f}, {kEqAir, -3.0f},
    {kMbStyle, 1}, {kMbDepth, 40}, {kSpaceType, 1}, {kReverbMix, 20},
};

#define LS_PRESET(n, cat, arr) {n, cat, arr, static_cast<uint32_t>(sizeof(arr) / sizeof(arr[0]))}
} // namespace presets

inline constexpr FactoryPreset kFactoryPresets[] = {
    LS_PRESET("Podcast Clean", "Podcast", presets::podcastClean),
    LS_PRESET("Warm Intimate", "Podcast", presets::warmIntimate),
    LS_PRESET("Dry & Tight", "Podcast", presets::dryTight),
    LS_PRESET("Stream Bright", "Stream", presets::streamBright),
    LS_PRESET("Radio Voice", "Stream", presets::radioVoice),
    LS_PRESET("Singer Polish", "Music", presets::singerPolish),
    LS_PRESET("Big Hall Vocal", "Music", presets::bigHall),
    LS_PRESET("Natural Tune", "Tune", presets::naturalTune),
    LS_PRESET("Robot Voice", "Tune", presets::robotVoice),
    LS_PRESET("De-ess Only", "Utility", presets::deessOnly),
    LS_PRESET("Init", "Utility", presets::init),
    // Added in 1.1
    LS_PRESET("Chat Voice", "Stream", presets::chatVoice),
    LS_PRESET("Whisper", "Podcast", presets::whisper),
    LS_PRESET("Telephone", "Stream", presets::telephone),
    LS_PRESET("Megaphone", "Stream", presets::megaphone),
    LS_PRESET("Small Room", "Music", presets::smallRoom),
    LS_PRESET("Cavern", "Music", presets::cavern),
    LS_PRESET("Helium", "Tune", presets::helium),
    LS_PRESET("Giant", "Tune", presets::giant),
};
inline constexpr uint32_t kNumFactoryPresets = sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]);

// Fill `values` (kParamCount floats, indexed by id) with defaults + the preset's overrides.
inline void presetToValues(const FactoryPreset& p, float* values) {
    for (uint32_t i = 0; i < kParamCount; ++i) values[i] = kParams[i].def;
    for (uint32_t i = 0; i < p.count; ++i) values[p.values[i].id] = p.values[i].value;
}

} // namespace lstrip
