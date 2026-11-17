// SPDX-License-Identifier: MIT
// L-STRIP parameter table: the single source of truth for DSP, UI, presets
// and host automation.  RULE: ids are permanent.  Never reorder or remove;
// append new entries at the end only (saved host sessions index by id).
#pragma once

#include <cstdint>
#include <cstring>

namespace lstrip {

enum ParamId : uint32_t {
    // --- Master ---
    kMasterOn = 0, kIntensity, kInGain, kOutGain, kOutSat, kGateThresh,
    // --- Dynamics (compressor + de-esser) ---
    kDynOn, kDynStyle, kCompThresh, kCompMakeup, kCompTone, kCompMix, kCompMode,
    kDeessFreq, kDeessAmount, kDeessRange, kDeessListen,
    // --- Space ---
    kSpaceOn, kSpaceType, kReverbMix, kDelayMix, kReverbDuck, kDelayDuck, kDelayTime,
    kChorusRate, kChorusMix, kChorusVoices,
    // --- Multiband ---
    kMbOn, kMbStyle, kMbDepth, kMbTime, kMbUpward, kMbDown, kMbOutput,
    kMbLow, kMbMid, kMbHigh,
    // --- Equalizer ---
    kEqOn, kEqStyle, kEqLow, kEqMid, kEqHigh, kEqAir, kEqLowCut,
    // --- Auto-tune ---
    kTuneOn, kTuneKey, kTuneScale,
    kTuneNote0, kTuneNote1, kTuneNote2, kTuneNote3, kTuneNote4, kTuneNote5,
    kTuneNote6, kTuneNote7, kTuneNote8, kTuneNote9, kTuneNote10, kTuneNote11,
    kTuneSpeed, kTuneHumanize, kTuneMix,
    // --- Outputs (read-only, DSP -> UI) ---
    kMeterIn, kMeterOut, kOrbLevel, kGateGR, kCompGR, kDeessGR,
    kBandLow, kBandMid, kBandHigh,
    kReverbSend, kDelaySend, kReverbDuckGR, kDelayDuckGR,
    kTuneInMidi, kTuneOutMidi,
    kParamCount
};

constexpr uint32_t kNumInputParams = kMeterIn;

enum class Kind : uint8_t { Float, Bool, Int, Enum };
enum class Unit : uint8_t { None, dB, Percent, Hz, kHz, ms, Midi };

enum : uint8_t { fOutput = 1, fLog = 2 };

struct ParamDef {
    const char* symbol;   // stable text id used in preset files (never change)
    const char* name;
    const char* shortName;
    Kind kind;
    Unit unit;
    float min, max, def;
    uint8_t flags;
    const char* tip;
    const char* const* labels; // for Enum
};

inline constexpr const char* kCompModeLabels[]   = {"Fast", "Slow", "Auto"};
inline constexpr const char* kDynStyleLabels[]   = {"Clean", "Punch", "Thick"};
inline constexpr const char* kSpaceTypeLabels[]  = {"Room", "Hall", "Plate"};
inline constexpr const char* kMbStyleLabels[]    = {"Smooth", "Punch", "OTT"};
inline constexpr const char* kEqStyleLabels[]    = {"Analog", "Modern", "Broadcast"};
inline constexpr const char* kKeyLabels[]        = {"C", "C#", "D", "D#", "E", "F",
                                                    "F#", "G", "G#", "A", "A#", "B"};
inline constexpr const char* kScaleLabels[]      = {"Major", "Minor", "Chromatic", "Pent. Major",
                                                    "Pent. Minor", "Blues", "Dorian", "Custom"};
inline constexpr const char* kDelayTimeLabels[]  = {"1/16", "1/16T", "1/8", "1/8T", "1/8.",
                                                    "1/4", "1/4T", "1/4.", "1/2T", "1/2"};
inline constexpr float kDelayTimeBeats[] = {0.25f, 1.f / 6.f, 0.5f, 1.f / 3.f, 0.75f,
                                            1.0f, 2.f / 3.f, 1.5f, 4.f / 3.f, 2.0f};

constexpr uint32_t kScaleCustom = 7;

#define LS_NOTE(n, sym) \
    {sym, "Note " #n, "N" #n, Kind::Bool, Unit::None, 0, 1, 1, 0, \
     "Click a note to allow or forbid it as a correction target.", nullptr}

inline constexpr ParamDef kParams[kParamCount] = {
    // Master
    {"master_on", "Master", "Master", Kind::Bool, Unit::None, 0, 1, 1, 0,
     "Master power. Off bypasses the whole chain.", nullptr},
    {"intensity", "Intensity", "Level", Kind::Float, Unit::Percent, 0, 100, 100, 0,
     "How much of the whole chain you hear (blend with your untouched voice).", nullptr},
    {"in_gain", "Input", "In", Kind::Float, Unit::dB, -24, 24, 0, 0,
     "Input level into the chain.", nullptr},
    {"out_gain", "Output", "Out", Kind::Float, Unit::dB, -24, 24, 0, 0,
     "Output level after the limiter-protected chain.", nullptr},
    {"out_sat", "Out Sat", "Sat", Kind::Float, Unit::Percent, 0, 100, 0, 0,
     "Warm saturation for a thicker, upfront voice.", nullptr},
    {"gate_thresh", "Gate", "Gate", Kind::Float, Unit::dB, -80, -20, -60, 0,
     "Removes fan, AC and keyboard noise between words. Fully down = Off.", nullptr},
    // Dynamics
    {"dyn_on", "Dynamics", "Dyn", Kind::Bool, Unit::None, 0, 1, 1, 0,
     "Dynamics power (compressor + de-esser).", nullptr},
    {"dyn_style", "Dynamics Style", "Style", Kind::Enum, Unit::None, 0, 2, 0, 0,
     "Character of the compressor: Clean, Punch or Thick.", kDynStyleLabels},
    {"comp_thresh", "Threshold", "Thresh", Kind::Float, Unit::dB, -40, 0, -18, 0,
     "Level above which the compressor starts turning the voice down.", nullptr},
    {"comp_makeup", "Makeup", "Makeup", Kind::Float, Unit::dB, 0, 18, 4.5f, 0,
     "Brings the level back up after compression.", nullptr},
    {"comp_tone", "Tone", "Tone", Kind::Float, Unit::dB, -6, 6, 0, 0,
     "Warmer (negative) or brighter (positive).", nullptr},
    {"comp_mix", "Comp Mix", "Mix", Kind::Float, Unit::Percent, 0, 100, 100, 0,
     "Blend of compressed and dry voice (parallel compression).", nullptr},
    {"comp_mode", "Comp Mode", "Mode", Kind::Enum, Unit::None, 0, 2, 0, 0,
     "Reaction speed: Fast for rap, Slow for singing, Auto adapts to the voice.", kCompModeLabels},
    {"deess_freq", "De-ess Freq", "Freq", Kind::Float, Unit::kHz, 3000, 12000, 6500, fLog,
     "Centre frequency of the harsh 'sss' band.", nullptr},
    {"deess_amount", "De-ess Amount", "Amount", Kind::Float, Unit::Percent, 0, 100, 40, 0,
     "How strongly sibilance is tamed.", nullptr},
    {"deess_range", "De-ess Range", "Range", Kind::Float, Unit::dB, 0, 30, 12, 0,
     "Maximum reduction applied to the 'sss' band.", nullptr},
    {"deess_listen", "De-ess Listen", "Listen", Kind::Bool, Unit::None, 0, 1, 0, 0,
     "Hear exactly what the de-esser removes. Use it to find the right frequency.", nullptr},
    // Space
    {"space_on", "Space", "Space", Kind::Bool, Unit::None, 0, 1, 1, 0,
     "Space power (reverb, delay, chorus).", nullptr},
    {"space_type", "Reverb Type", "Type", Kind::Enum, Unit::None, 0, 2, 0, 0,
     "Room, Hall or Plate reverb.", kSpaceTypeLabels},
    {"reverb_mix", "Reverb", "Reverb", Kind::Float, Unit::Percent, 0, 100, 10, 0,
     "Room or hall sound around your voice.", nullptr},
    {"delay_mix", "Delay", "Delay", Kind::Float, Unit::Percent, 0, 100, 0, 0,
     "Tempo-synced echo level.", nullptr},
    {"reverb_duck", "Reverb Ducking", "Duck", Kind::Float, Unit::dB, -24, 0, -6, 0,
     "How far the reverb dips while you talk, so words stay clear.", nullptr},
    {"delay_duck", "Delay Ducking", "Duck", Kind::Float, Unit::dB, -24, 0, -9, 0,
     "How far the echo dips while you talk, so words stay clear.", nullptr},
    {"delay_time", "Delay Time", "Time", Kind::Enum, Unit::None, 0, 9, 5, 0,
     "Echo time as a note length, synced to the host tempo (120 BPM if none).", kDelayTimeLabels},
    {"chorus_rate", "Chorus Rate", "Rate", Kind::Float, Unit::Hz, 0.1f, 5.0f, 0.8f, fLog,
     "Speed of the chorus movement.", nullptr},
    {"chorus_mix", "Chorus Mix", "Mix", Kind::Float, Unit::Percent, 0, 100, 0, 0,
     "Amount of chorus for a wider sound.", nullptr},
    {"chorus_voices", "Chorus Voices", "Voices", Kind::Int, Unit::None, 1, 4, 2, 0,
     "Number of chorus voices.", nullptr},
    // Multiband
    {"mb_on", "Multiband", "MB", Kind::Bool, Unit::None, 0, 1, 1, 0,
     "Multiband power.", nullptr},
    {"mb_style", "Multiband Style", "Style", Kind::Enum, Unit::None, 0, 2, 0, 0,
     "Smooth, Punch or OTT (hard) multiband character.", kMbStyleLabels},
    {"mb_depth", "Depth", "Depth", Kind::Float, Unit::Percent, 0, 100, 45, 0,
     "How strong the effect is.", nullptr},
    {"mb_time", "Time", "Time", Kind::Float, Unit::Percent, 0, 100, 50, 0,
     "How fast the effect works (higher = slower and smoother).", nullptr},
    {"mb_upward", "Upward", "Upward", Kind::Float, Unit::Percent, 0, 100, 35, 0,
     "Lifts breaths and soft word endings.", nullptr},
    {"mb_down", "Down", "Down", Kind::Float, Unit::Percent, 0, 100, 60, 0,
     "Pushes down parts that jump out too loud.", nullptr},
    {"mb_output", "MB Output", "Output", Kind::Float, Unit::dB, -12, 12, 0, 0,
     "Output level of the multiband stage.", nullptr},
    {"mb_low", "Low", "Low", Kind::Float, Unit::dB, -12, 12, 0, 0,
     "Level of the low band after processing.", nullptr},
    {"mb_mid", "Mid", "Mid", Kind::Float, Unit::dB, -12, 12, 0, 0,
     "Level of the mid band after processing.", nullptr},
    {"mb_high", "High", "High", Kind::Float, Unit::dB, -12, 12, 0, 0,
     "Level of the high band after processing.", nullptr},
    // EQ
    {"eq_on", "Equalizer", "EQ", Kind::Bool, Unit::None, 0, 1, 1, 0,
     "Equalizer power.", nullptr},
    {"eq_style", "EQ Style", "Style", Kind::Enum, Unit::None, 0, 2, 0, 0,
     "Analog, Modern or Broadcast voicing of the bands.", kEqStyleLabels},
    {"eq_low", "Low", "Low", Kind::Float, Unit::dB, -12, 12, 2.0f, 0,
     "Body. Fuller voice or less boom.", nullptr},
    {"eq_mid", "Mid", "Mid", Kind::Float, Unit::dB, -12, 12, -1.5f, 0,
     "Remove boxiness.", nullptr},
    {"eq_high", "High", "High", Kind::Float, Unit::dB, -12, 12, 1.0f, 0,
     "Add clarity and presence.", nullptr},
    {"eq_air", "Air", "Air", Kind::Float, Unit::dB, -12, 12, 3.0f, 0,
     "Sparkle for a polished, pro sound.", nullptr},
    {"eq_lowcut", "Low-Cut", "LowCut", Kind::Float, Unit::Hz, 20, 300, 90, fLog,
     "Cuts desk, AC and footstep rumble. 20 Hz = Off.", nullptr},
    // Auto-tune
    {"tune_on", "Pitch Correction", "Tune", Kind::Bool, Unit::None, 0, 1, 0, 0,
     "Real-time pitch correction. Adds about 3-10 ms of latency while on.", nullptr},
    {"tune_key", "Key", "Key", Kind::Enum, Unit::None, 0, 11, 0, 0,
     "Root note of the scale.", kKeyLabels},
    {"tune_scale", "Scale", "Scale", Kind::Enum, Unit::None, 0, 7, 0, 0,
     "Scale that decides which notes are allowed.", kScaleLabels},
    LS_NOTE(0, "tune_note_c"), LS_NOTE(1, "tune_note_cs"), LS_NOTE(2, "tune_note_d"),
    LS_NOTE(3, "tune_note_ds"), LS_NOTE(4, "tune_note_e"), LS_NOTE(5, "tune_note_f"),
    LS_NOTE(6, "tune_note_fs"), LS_NOTE(7, "tune_note_g"), LS_NOTE(8, "tune_note_gs"),
    LS_NOTE(9, "tune_note_a"), LS_NOTE(10, "tune_note_as"), LS_NOTE(11, "tune_note_b"),
    {"tune_speed", "Speed", "Speed", Kind::Float, Unit::ms, 0, 200, 80, 0,
     "Low speed = classic robotic effect; high speed = natural correction.", nullptr},
    {"tune_humanize", "Humanize", "Human", Kind::Float, Unit::Percent, 0, 100, 30, 0,
     "Keeps the natural vibrato and feel of your voice.", nullptr},
    {"tune_mix", "Tune Mix", "Mix", Kind::Float, Unit::Percent, 0, 100, 100, 0,
     "Blend of corrected and original voice.", nullptr},
    // Outputs
    {"meter_in", "Input Meter", "In", Kind::Float, Unit::dB, -80, 6, -80, fOutput, "", nullptr},
    {"meter_out", "Output Meter", "Out", Kind::Float, Unit::dB, -80, 6, -80, fOutput, "", nullptr},
    {"orb_level", "Orb Level", "Orb", Kind::Float, Unit::None, 0, 1, 0, fOutput, "", nullptr},
    {"gate_gr", "Gate Reduction", "GateGR", Kind::Float, Unit::dB, 0, 80, 0, fOutput, "", nullptr},
    {"comp_gr", "Comp Reduction", "CompGR", Kind::Float, Unit::dB, 0, 30, 0, fOutput, "", nullptr},
    {"deess_gr", "De-ess Reduction", "DeessGR", Kind::Float, Unit::dB, 0, 30, 0, fOutput, "", nullptr},
    {"band_low", "Low Band Level", "BLow", Kind::Float, Unit::dB, -80, 6, -80, fOutput, "", nullptr},
    {"band_mid", "Mid Band Level", "BMid", Kind::Float, Unit::dB, -80, 6, -80, fOutput, "", nullptr},
    {"band_high", "High Band Level", "BHigh", Kind::Float, Unit::dB, -80, 6, -80, fOutput, "", nullptr},
    {"reverb_send", "Reverb Send Level", "RvSend", Kind::Float, Unit::dB, -80, 6, -80, fOutput, "", nullptr},
    {"delay_send", "Delay Send Level", "DlSend", Kind::Float, Unit::dB, -80, 6, -80, fOutput, "", nullptr},
    {"reverb_duck_gr", "Reverb Duck Reduction", "RvDuck", Kind::Float, Unit::dB, 0, 24, 0, fOutput, "", nullptr},
    {"delay_duck_gr", "Delay Duck Reduction", "DlDuck", Kind::Float, Unit::dB, 0, 24, 0, fOutput, "", nullptr},
    {"tune_in_midi", "Detected Pitch", "InMidi", Kind::Float, Unit::Midi, 0, 127, 0, fOutput, "", nullptr},
    {"tune_out_midi", "Corrected Pitch", "OutMidi", Kind::Float, Unit::Midi, 0, 127, 0, fOutput, "", nullptr},
};

#undef LS_NOTE

inline constexpr bool isOutput(uint32_t id) { return (kParams[id].flags & fOutput) != 0; }

inline int findParamBySymbol(const char* sym) {
    for (uint32_t i = 0; i < kParamCount; ++i)
        if (std::strcmp(kParams[i].symbol, sym) == 0) return static_cast<int>(i);
    return -1;
}

// ---------------------------------------------------------------- scales
constexpr uint16_t maskOf(const int (&iv)[12]) {
    uint16_t m = 0;
    for (int i = 0; i < 12; ++i)
        if (iv[i] >= 0) m = static_cast<uint16_t>(m | (1u << iv[i]));
    return m;
}
// Semitone intervals above the root (-1 = unused slot).
inline constexpr uint16_t kScaleMasks[7] = {
    maskOf({0, 2, 4, 5, 7, 9, 11, -1, -1, -1, -1, -1}),  // Major
    maskOf({0, 2, 3, 5, 7, 8, 10, -1, -1, -1, -1, -1}),  // Minor (natural)
    maskOf({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}),      // Chromatic
    maskOf({0, 2, 4, 7, 9, -1, -1, -1, -1, -1, -1, -1}), // Pentatonic major
    maskOf({0, 3, 5, 7, 10, -1, -1, -1, -1, -1, -1, -1}),// Pentatonic minor
    maskOf({0, 3, 5, 6, 7, 10, -1, -1, -1, -1, -1, -1}), // Blues
    maskOf({0, 2, 3, 5, 7, 9, 10, -1, -1, -1, -1, -1}),  // Dorian
};

// Mask of allowed pitch classes (bit 0 = C) for key + scale (+ custom flags).
inline uint16_t scaleMask(int key, int scale, const float* noteFlags /*12 floats or null*/) {
    if (scale >= 0 && scale < 7) {
        const uint16_t m = kScaleMasks[scale];
        const int k = ((key % 12) + 12) % 12;
        return static_cast<uint16_t>(((m << k) | (m >> (12 - k))) & 0xFFF);
    }
    uint16_t out = 0;
    if (noteFlags)
        for (int i = 0; i < 12; ++i)
            if (noteFlags[i] > 0.5f) out |= static_cast<uint16_t>(1u << i);
    return out;
}

} // namespace lstrip
