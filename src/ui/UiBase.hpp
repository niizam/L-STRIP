// SPDX-License-Identifier: MIT
// UiBase: everything the controls need from the editor (parameter access,
// theme, text/geometry helpers, interaction state).  LStripUI derives from it.
#pragma once

#include "DistrhoUI.hpp"
#include "Color.hpp"

#include "../common/Params.hpp"
#include "../common/Shared.hpp"
#include "Theme.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

namespace lstrip::ui {

constexpr float kW = 940.f, kH = 640.f;

struct Control;

class UiBase : public DISTRHO_NAMESPACE::UI {
public:
    UiBase();

    // ------------------------------------------------------------ parameters
    float get(uint32_t id) const { return val_[id]; }
    // User edit: clamps, caches and tells the host/DSP.
    void set(uint32_t id, float v);
    void begin(uint32_t id) { editParameter(id, true); }
    void end(uint32_t id) { editParameter(id, false); }
    void reset(uint32_t id) { begin(id); set(id, kParams[id].def); end(id); }

    // Normalised 0..1 <-> value (logarithmic where the table says so).
    float toNorm(uint32_t id, float v) const;
    float fromNorm(uint32_t id, float n) const;

    std::string format(uint32_t id, float v, bool withUnit = true) const;

    // ----------------------------------------------------------------- theme
    bool dark() const { return dark_; }
    void setDark(bool d) { dark_ = d; th = d ? darkTheme() : lightTheme(); }
    Theme th = darkTheme();

    // ------------------------------------------------------------------ time
    double now() const {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }

    // ------------------------------------------------------------------ text
    enum Font { Regular = 0, Bold = 1 };
    void txt(float x, float y, const std::string& s, float size, const Color& c, int align = ALIGN_CENTER | ALIGN_MIDDLE,
             Font f = Regular, float spacing = 0.f);
    float txtWidth(const std::string& s, float size, Font f = Regular, float spacing = 0.f);
    // small spaced capitals used for panel and control labels
    void cap(float x, float y, const std::string& s, const Color& c, int align = ALIGN_CENTER | ALIGN_MIDDLE, float size = 9.5f) {
        txt(x, y, s, size, c, align, Regular, 1.4f);
    }

    // -------------------------------------------------------------- geometry
    void fillRR(float x, float y, float w, float h, float r, const Color& c) {
        beginPath(); roundedRect(x, y, w, h, r); fillColor(c); fill();
    }
    void strokeRR(float x, float y, float w, float h, float r, const Color& c, float lw = 1.f) {
        beginPath(); roundedRect(x + lw * .5f, y + lw * .5f, w - lw, h - lw, r); strokeColor(c); strokeWidth(lw); stroke();
    }
    void line(float x0, float y0, float x1, float y1, const Color& c, float lw = 1.f, bool round = true) {
        beginPath(); moveTo(x0, y0); lineTo(x1, y1); strokeColor(c); strokeWidth(lw); lineCap(round ? ROUND : BUTT); stroke();
    }
    void dot(float x, float y, float r, const Color& c) { beginPath(); circle(x, y, r); fillColor(c); fill(); }

    // ----------------------------------------------------------- interaction
    Control* hover = nullptr;
    Control* active = nullptr;
    float mx = -1, my = -1;       // pointer in design space
    bool tipsOn = true;
    double hoverSince = 0;

    // Toast message shown at the bottom for a couple of seconds.
    void toast(const std::string& s) { toast_ = s; toastUntil_ = now() + 2.6; }

    // Data shared with the DSP.
    std::shared_ptr<SharedState> shared;
    void pollShared();
    void bindShared(std::shared_ptr<SharedState> s);

    // DPF callbacks that are common to all subclasses.
    void parameterChanged(uint32_t index, float value) override { if (index < kParamCount) val_[index] = value; }
    void stateChanged(const char* key, const char* value) override;

    // current preset name as stored in the session
    std::string presetName;
    virtual void onPresetStateFromHost(const std::string&) {}

protected:
    float val_[kParamCount];
    float lastShared_[kParamCount];   // last value seen in `shared`; edits are only overwritten when it changes
    bool dark_ = true;
    FontId font_[2] = {-1, -1};
    std::string toast_;
    double toastUntil_ = 0;
};

} // namespace lstrip::ui
