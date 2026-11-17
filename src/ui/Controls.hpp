// SPDX-License-Identifier: MIT
// Retained-mode controls.  Every control lives in design space (940x640) and is
// bound to a parameter id; labels, ranges and help text come from kParams.
#pragma once

#include "UiBase.hpp"
#include "../dsp/Compressor.hpp"
#include "../dsp/Equalizer.hpp"

#include <functional>
#include <vector>

namespace lstrip::ui {

constexpr float kPiF = 3.14159265358979f;

inline std::string upper(const char* s) {
    std::string r = s;
    for (char& c : r) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    return r;
}

struct Control {
    float x = 0, y = 0, w = 0, h = 0;
    const char* tip = nullptr;
    bool visible = true;

    virtual ~Control() = default;
    virtual void draw(UiBase&) = 0;
    // Return true to capture the pointer (subsequent drags/release go to this control).
    virtual bool press(UiBase&, float, float, int /*button*/, bool /*ctrl*/, bool /*dbl*/) { return false; }
    virtual void drag(UiBase&, float, float, bool /*shift*/) {}
    virtual void release(UiBase&, float, float) {}
    virtual bool scroll(UiBase&, float, float, float) { return false; }
    virtual bool hit(float px, float py) const { return visible && px >= x && px < x + w && py >= y && py < y + h; }

    bool isHover(const UiBase& u) const { return u.hover == this; }
    bool isActive(const UiBase& u) const { return u.active == this; }
};

// ------------------------------------------------------------- drawing atoms
inline void iconPower(UiBase& u, float cx, float cy, float r, const Color& c, float lw = 1.8f) {
    u.beginPath(); u.arc(cx, cy, r, -kPiF * 0.5f + 0.62f, -kPiF * 0.5f + 2.f * kPiF - 0.62f, UiBase::CW);
    u.strokeColor(c); u.strokeWidth(lw); u.lineCap(UiBase::ROUND); u.stroke();
    u.line(cx, cy - r * 1.05f, cx, cy - r * 0.1f, c, lw);
}
inline void iconReset(UiBase& u, float cx, float cy, float r, const Color& c) {
    // three-quarter circle with an arrow head at the top (counter-clockwise "undo" arrow)
    u.beginPath(); u.arc(cx, cy, r, 0.f, 1.5f * kPiF, UiBase::CW);
    u.strokeColor(c); u.strokeWidth(1.6f); u.lineCap(UiBase::ROUND); u.stroke();
    const float tx = cx + 1.5f, ty = cy - r;
    u.line(tx, ty, tx - 3.6f, ty - 3.2f, c, 1.6f);
    u.line(tx, ty, tx - 3.6f, ty + 3.2f, c, 1.6f);
}
inline void iconChevron(UiBase& u, float cx, float cy, float s, int dir /* -1 left, 1 right, 2 down */, const Color& c) {
    u.beginPath();
    if (dir == 2) { u.moveTo(cx - s, cy - s * 0.5f); u.lineTo(cx, cy + s * 0.5f); u.lineTo(cx + s, cy - s * 0.5f); }
    else { u.moveTo(cx - s * 0.5f * dir, cy - s); u.lineTo(cx + s * 0.5f * dir, cy); u.lineTo(cx - s * 0.5f * dir, cy + s); }
    u.strokeColor(c); u.strokeWidth(1.6f); u.lineCap(UiBase::ROUND); u.lineJoin(UiBase::ROUND); u.stroke();
}
inline void iconHeadphones(UiBase& u, float cx, float cy, float s, const Color& c) {
    u.beginPath(); u.arc(cx, cy, s, kPiF, 2.f * kPiF, UiBase::CW);
    u.strokeColor(c); u.strokeWidth(1.5f); u.lineCap(UiBase::ROUND); u.stroke();
    u.fillRR(cx - s - 2.f, cy - 0.5f, 4.f, s * 0.9f, 1.5f, c);
    u.fillRR(cx + s - 2.f, cy - 0.5f, 4.f, s * 0.9f, 1.5f, c);
}
inline void iconStar(UiBase& u, float cx, float cy, float r, bool filled, const Color& c) {
    u.beginPath();
    for (int i = 0; i < 10; ++i) {
        const float a = -kPiF * 0.5f + i * kPiF / 5.f, rr = (i & 1) ? r * 0.45f : r;
        if (i == 0) u.moveTo(cx + std::cos(a) * rr, cy + std::sin(a) * rr); else u.lineTo(cx + std::cos(a) * rr, cy + std::sin(a) * rr);
    }
    u.closePath();
    if (filled) { u.fillColor(c); u.fill(); } else { u.strokeColor(c); u.strokeWidth(1.2f); u.lineJoin(UiBase::ROUND); u.stroke(); }
}

// ------------------------------------------------------------------- Knob
struct Knob : Control {
    uint32_t id; std::string label; bool bipolar = false; float diameter;
    float dragStartY = 0, dragStartN = 0;
    Knob(uint32_t pid, float cx, float cy, float dia, const char* lbl = nullptr, bool bi = false)
        : id(pid), label(upper(lbl ? lbl : kParams[pid].name)), bipolar(bi), diameter(dia) {
        x = cx - dia * 0.5f - 6; y = cy - dia * 0.5f - 4; w = dia + 12; h = dia + 8 + 30; tip = kParams[pid].tip;
    }
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + 4 + diameter * 0.5f; }
    bool hit(float px, float py) const override { return visible && px >= x && px < x + w && py >= y && py < y + h; }
    void draw(UiBase& u) override {
        const float r = diameter * 0.5f, ccx = cx(), ccy = cy();
        const float n = u.toNorm(id, u.get(id));
        const bool act = isActive(u), hov = isHover(u);
        const float a0 = 0.75f * kPiF, a1 = 2.25f * kPiF, av = a0 + n * (a1 - a0);
        // track
        u.beginPath(); u.arc(ccx, ccy, r - 2.f, a0, a1, UiBase::CW); u.strokeColor(u.th.track); u.strokeWidth(3.f); u.lineCap(UiBase::ROUND); u.stroke();
        // value arc
        const float mid = a0 + (a1 - a0) * 0.5f;
        u.beginPath();
        if (bipolar) u.arc(ccx, ccy, r - 2.f, std::min(mid, av), std::max(mid, av), UiBase::CW);
        else u.arc(ccx, ccy, r - 2.f, a0, std::max(av, a0 + 0.001f), UiBase::CW);
        u.strokeColor(hov || act ? u.th.accentHi : u.th.accent); u.strokeWidth(3.f); u.lineCap(UiBase::ROUND); u.stroke();
        // face
        const float fr = r - 8.f;
        if (act) {
            u.beginPath(); u.circle(ccx, ccy, fr);
            u.fillPaint(u.radialGradient(ccx - fr * 0.3f, ccy - fr * 0.3f, 1.f, fr * 1.3f, u.th.accentHi, u.th.accent)); u.fill();
            u.line(ccx + std::cos(av) * fr * 0.35f, ccy + std::sin(av) * fr * 0.35f, ccx + std::cos(av) * fr * 0.85f, ccy + std::sin(av) * fr * 0.85f, u.th.onAccent, 2.f);
        } else {
            u.dot(ccx, ccy, fr, u.dark() ? Color(14, 17, 23) : Color(240, 243, 248));
            u.strokeRR(ccx - fr, ccy - fr, fr * 2, fr * 2, fr, u.th.trackDim, 1.f);
            u.line(ccx + std::cos(av) * fr * 0.4f, ccy + std::sin(av) * fr * 0.4f, ccx + std::cos(av) * fr * 0.88f, ccy + std::sin(av) * fr * 0.88f, u.th.text, 2.f);
        }
        u.cap(ccx, y + 4 + diameter + 9.f, label, u.th.textDim);
        u.txt(ccx, y + 4 + diameter + 24.f, u.format(id, u.get(id)), 12.f, u.th.text);
    }
    bool press(UiBase& u, float, float py, int b, bool, bool dbl) override {
        if (b != 1) return false;
        if (dbl) { u.reset(id); return false; }
        u.begin(id); dragStartY = py; dragStartN = u.toNorm(id, u.get(id)); return true;
    }
    void drag(UiBase& u, float, float py, bool shift) override {
        const float range = shift ? 900.f : 180.f;
        u.set(id, u.fromNorm(id, dragStartN + (dragStartY - py) / range));
    }
    void release(UiBase& u, float, float) override { u.end(id); }
    bool scroll(UiBase& u, float, float, float dy) override {
        u.begin(id); u.set(id, u.fromNorm(id, u.toNorm(id, u.get(id)) + dy * 0.03f)); u.end(id); return true;
    }
};

// -------------------------------------------------------- Vertical fader
struct VFader : Control {
    uint32_t id; std::string label; bool bipolar; float trackH, trackTop;
    bool showUnit = true;
    VFader(uint32_t pid, float cx, float top, float height, const char* lbl = nullptr, bool bi = false, float width = 46)
        : id(pid), label(upper(lbl ? lbl : kParams[pid].name)), bipolar(bi), trackH(height), trackTop(top) {
        x = cx - width * 0.5f; y = top - 8; w = width; h = height + 16 + 40; tip = kParams[pid].tip;
    }
    float cx() const { return x + w * 0.5f; }
    float thumbY(float n) const { return trackTop + (1.f - n) * trackH; }
    bool hit(float px, float py) const override { return visible && px >= x && px < x + w && py >= y && py < y + h; }
    void draw(UiBase& u) override {
        const float n = u.toNorm(id, u.get(id)), ty = thumbY(n), c = cx();
        u.fillRR(c - 2.f, trackTop, 4.f, trackH, 2.f, u.th.track);
        const float baseY = bipolar ? thumbY(u.toNorm(id, 0.f)) : trackTop + trackH;
        u.fillRR(c - 2.f, std::min(ty, baseY), 4.f, std::abs(baseY - ty), 2.f, isHover(u) || isActive(u) ? u.th.accentHi : u.th.accent);
        if (bipolar) u.line(c - 7.f, baseY, c + 7.f, baseY, u.th.textFaint, 1.f, false);
        u.fillRR(c - 15.f, ty - 8.f, 30.f, 16.f, 8.f, u.th.fieldHi);
        u.strokeRR(c - 15.f, ty - 8.f, 30.f, 16.f, 8.f, isActive(u) ? u.th.accent : u.th.textFaint, 1.f);
        u.cap(c, trackTop + trackH + 22.f, label, u.th.textDim);
        u.txt(c, trackTop + trackH + 38.f, u.format(id, u.get(id), showUnit), 11.5f, u.th.text);
    }
    bool press(UiBase& u, float, float py, int b, bool, bool dbl) override {
        if (b != 1) return false;
        if (dbl) { u.reset(id); return false; }
        u.begin(id); drag(u, 0, py, false); return true;
    }
    void drag(UiBase& u, float, float py, bool) override { u.set(id, u.fromNorm(id, 1.f - (py - trackTop) / trackH)); }
    void release(UiBase& u, float, float) override { u.end(id); }
    bool scroll(UiBase& u, float, float, float dy) override {
        u.begin(id); u.set(id, u.fromNorm(id, u.toNorm(id, u.get(id)) + dy * 0.03f)); u.end(id); return true;
    }
};

// ------------------------------------------------------ Horizontal slider
struct HSlider : Control {
    uint32_t id; bool bipolar; float trackX, trackW, cy;
    HSlider(uint32_t pid, float tx, float tw, float cyy, bool bi = false) : id(pid), bipolar(bi), trackX(tx), trackW(tw), cy(cyy) {
        x = tx - 8; y = cyy - 10; w = tw + 16; h = 20; tip = kParams[pid].tip;
    }
    void draw(UiBase& u) override {
        const float n = u.toNorm(id, u.get(id)), tx = trackX + n * trackW;
        u.fillRR(trackX, cy - 1.5f, trackW, 3.f, 1.5f, u.th.track);
        const float bx = bipolar ? trackX + u.toNorm(id, 0.f) * trackW : trackX;
        u.fillRR(std::min(tx, bx), cy - 1.5f, std::abs(tx - bx), 3.f, 1.5f, isHover(u) || isActive(u) ? u.th.accentHi : u.th.accent);
        u.dot(tx, cy, 6.5f, u.th.fieldHi);
        u.beginPath(); u.circle(tx, cy, 6.5f); u.strokeColor(isActive(u) ? u.th.accent : u.th.textFaint); u.strokeWidth(1.3f); u.stroke();
    }
    bool press(UiBase& u, float px, float, int b, bool, bool dbl) override {
        if (b != 1) return false;
        if (dbl) { u.reset(id); return false; }
        u.begin(id); drag(u, px, 0, false); return true;
    }
    void drag(UiBase& u, float px, float, bool) override { u.set(id, u.fromNorm(id, (px - trackX) / trackW)); }
    void release(UiBase& u, float, float) override { u.end(id); }
    bool scroll(UiBase& u, float, float, float dy) override {
        u.begin(id); u.set(id, u.fromNorm(id, u.toNorm(id, u.get(id)) + dy * 0.03f)); u.end(id); return true;
    }
};

// ------------------------------------------------- Stepper  ( - value + )
struct Stepper : Control {
    uint32_t id; float step; bool showValue; std::string fixedText;
    float btn = 11.f;
    Stepper(uint32_t pid, float cx, float cy, float width, float stepv = 1.f)
        : id(pid), step(stepv), showValue(true) { x = cx - width * 0.5f; y = cy - 12; w = width; h = 24; tip = kParams[pid].tip; }
    bool onMinus(float px) const { return px < x + 24; }
    bool onPlus(float px) const { return px > x + w - 24; }
    void draw(UiBase& u) override {
        const Color c = isHover(u) ? u.th.text : u.th.textDim;
        const float cy = y + h * 0.5f;
        for (int side = 0; side < 2; ++side) {
            const float bx = side == 0 ? x + btn + 1.f : x + w - btn - 1.f;
            const bool over = isHover(u) && ((side == 0 && onMinus(u.mx)) || (side == 1 && onPlus(u.mx)));
            u.dot(bx, cy, btn, over ? u.th.fieldHi : u.th.field);
            u.line(bx - 4.f, cy, bx + 4.f, cy, c, 1.5f);
            if (side == 1) u.line(bx, cy - 4.f, bx, cy + 4.f, c, 1.5f);
        }
        u.txt(x + w * 0.5f, cy, showValue ? u.format(id, u.get(id)) : fixedText, 12.f, kParams[id].kind == Kind::Enum ? u.th.accentHi : u.th.text);
    }
    bool press(UiBase& u, float px, float, int b, bool, bool) override {
        if (b != 1) return false;
        float d = 0.f;
        if (onMinus(px)) d = -step; else if (onPlus(px)) d = step;
        if (d != 0.f) { u.begin(id); u.set(id, u.get(id) + d); u.end(id); }
        return false;
    }
    bool scroll(UiBase& u, float, float, float dy) override {
        u.begin(id); u.set(id, u.get(id) + (dy > 0 ? step : -step)); u.end(id); return true;
    }
};

// ------------------------------------------------ Carousel  ( <  Name  > )
struct Carousel : Control {
    uint32_t id;
    Carousel(uint32_t pid, float px, float py, float pw) : id(pid) { x = px; y = py; w = pw; h = 38; tip = kParams[pid].tip; }
    void draw(UiBase& u) override {
        u.fillRR(x, y, w, h, 12.f, u.th.field);
        const int n = static_cast<int>(kParams[id].max - kParams[id].min) + 1, cur = static_cast<int>(u.get(id) - kParams[id].min + 0.5f);
        const bool lh = isHover(u) && u.mx < x + 30, rh = isHover(u) && u.mx > x + w - 30;
        iconChevron(u, x + 15.f, y + h * 0.5f, 4.f, -1, lh ? u.th.text : u.th.textDim);
        iconChevron(u, x + w - 15.f, y + h * 0.5f, 4.f, 1, rh ? u.th.text : u.th.textDim);
        u.txt(x + w * 0.5f, y + 14.f, u.format(id, u.get(id)), 13.5f, u.th.text, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE, UiBase::Bold);
        for (int i = 0; i < n; ++i) u.dot(x + w * 0.5f + (i - (n - 1) * 0.5f) * 8.f, y + 29.f, 2.2f, i == cur ? u.th.accent : u.th.textFaint);
    }
    bool press(UiBase& u, float px, float, int b, bool, bool) override {
        if (b != 1) return false;
        const int n = static_cast<int>(kParams[id].max - kParams[id].min) + 1;
        int cur = static_cast<int>(u.get(id) - kParams[id].min + 0.5f);
        if (px < x + 30) cur = (cur + n - 1) % n; else cur = (cur + 1) % n; // wraps
        u.begin(id); u.set(id, kParams[id].min + cur); u.end(id);
        return false;
    }
    bool scroll(UiBase& u, float, float, float dy) override {
        const int n = static_cast<int>(kParams[id].max - kParams[id].min) + 1;
        int cur = static_cast<int>(u.get(id) - kParams[id].min + 0.5f);
        cur = (cur + (dy > 0 ? 1 : n - 1)) % n;
        u.begin(id); u.set(id, kParams[id].min + cur); u.end(id); return true;
    }
};

// -------------------------------------------------- Power + reset buttons
struct PowerButton : Control {
    uint32_t id; bool big;
    PowerButton(uint32_t pid, float cx, float cy, bool bigStyle = false) : id(pid), big(bigStyle) {
        const float s = big ? 44.f : 26.f; x = cx - s * 0.5f; y = cy - s * 0.5f; w = h = s; tip = kParams[pid].tip;
    }
    void draw(UiBase& u) override {
        const bool on = u.get(id) > 0.5f; const float cx = x + w * 0.5f, cy = y + h * 0.5f;
        if (big) {
            u.dot(cx, cy, w * 0.5f, on ? u.th.accent : u.th.field);
            iconPower(u, cx, cy + 1.f, 7.f, on ? u.th.onAccent : u.th.textDim, 2.f);
        } else {
            if (isHover(u)) u.dot(cx, cy, 13.f, u.th.field);
            iconPower(u, cx, cy + 0.5f, 6.f, on ? u.th.accent : u.th.textFaint);
        }
    }
    bool press(UiBase& u, float, float, int b, bool, bool) override {
        if (b == 1) { u.begin(id); u.set(id, u.get(id) > 0.5f ? 0.f : 1.f); u.end(id); }
        return false;
    }
};

struct ResetButton : Control {
    std::vector<uint32_t> ids;
    ResetButton(float cx, float cy, std::vector<uint32_t> idlist) : ids(std::move(idlist)) {
        x = cx - 13; y = cy - 13; w = h = 26; tip = "Reset this section to its defaults.";
    }
    void draw(UiBase& u) override {
        const float cx = x + w * 0.5f, cy = y + h * 0.5f;
        if (isHover(u)) u.dot(cx, cy, 13.f, u.th.field);
        iconReset(u, cx, cy, 5.5f, isHover(u) ? u.th.text : u.th.textFaint);
    }
    bool press(UiBase& u, float, float, int b, bool, bool) override {
        if (b == 1) for (uint32_t i : ids) if (i != kDynOn && i != kSpaceOn && i != kMbOn && i != kEqOn) u.reset(i);
        return false;
    }
};

// ------------------------------------------------------------- Segmented
struct Segmented : Control {
    uint32_t id;
    Segmented(uint32_t pid, float px, float py, float pw) : id(pid) { x = px; y = py; w = pw; h = 28; tip = kParams[pid].tip; }
    void draw(UiBase& u) override {
        u.fillRR(x, y, w, h, 10.f, u.th.field);
        const int n = static_cast<int>(kParams[id].max - kParams[id].min) + 1, cur = static_cast<int>(u.get(id) - kParams[id].min + 0.5f);
        const float sw = (w - 6.f) / n;
        for (int i = 0; i < n; ++i) {
            if (i == cur) u.fillRR(x + 3.f + i * sw, y + 3.f, sw, h - 6.f, 8.f, u.th.fieldHi);
            u.txt(x + 3.f + (i + 0.5f) * sw, y + h * 0.5f, kParams[id].labels[i], 12.f, i == cur ? u.th.accentHi : u.th.textDim);
        }
    }
    bool press(UiBase& u, float px, float, int b, bool, bool) override {
        if (b != 1) return false;
        const int n = static_cast<int>(kParams[id].max - kParams[id].min) + 1;
        const int i = std::min(std::max(static_cast<int>((px - x - 3.f) / ((w - 6.f) / n)), 0), n - 1);
        u.begin(id); u.set(id, kParams[id].min + i); u.end(id);
        return false;
    }
};

// ---------------------------------------------------------- Pill buttons
struct PillButton : Control {
    std::string label; std::function<void()> onClick; std::function<bool(UiBase&)> isOn;
    bool filled = false; int icon = 0; // 0 none, 1 headphones, 2 tune fork, 3 moon, 4 sun, 5 help
    float fontSize = 11.5f;
    PillButton(float px, float py, float pw, float ph, std::string lbl, std::function<void()> cb)
        : label(std::move(lbl)), onClick(std::move(cb)) { x = px; y = py; w = pw; h = ph; }
    void draw(UiBase& u) override {
        const bool on = isOn && isOn(u);
        Color bg = filled ? u.th.accent : (on ? u.th.accentSoft : u.th.field);
        if (isHover(u)) bg = filled ? u.th.accentHi : (on ? u.th.accentSoft : u.th.fieldHi);
        u.fillRR(x, y, w, h, h * 0.5f, bg);
        if (on && !filled) u.strokeRR(x, y, w, h, h * 0.5f, u.th.accent, 1.f);
        const Color fg = filled ? u.th.onAccent : (on ? u.th.accentHi : u.th.text);
        float tx = x + w * 0.5f;
        if (icon) {
            const float iw = 14.f, total = iw + 5.f + u.txtWidth(label, fontSize, UiBase::Bold);
            const float ix = x + (w - total) * 0.5f + iw * 0.5f, iy = y + h * 0.5f;
            if (icon == 1) iconHeadphones(u, ix, iy + 1.f, 5.f, fg);
            else if (icon == 2) { // tuning fork
                u.line(ix - 3.f, iy - 6.f, ix - 3.f, iy + 1.f, fg, 1.5f); u.line(ix + 3.f, iy - 6.f, ix + 3.f, iy + 1.f, fg, 1.5f);
                u.beginPath(); u.arc(ix, iy + 1.f, 3.f, 0.f, kPiF, UiBase::CW); u.strokeColor(fg); u.strokeWidth(1.5f); u.stroke(); u.line(ix, iy + 4.f, ix, iy + 7.f, fg, 1.5f);
            } else if (icon == 3) { u.beginPath(); u.arc(ix, iy, 5.f, 0.6f, 5.7f, UiBase::CW); u.strokeColor(fg); u.strokeWidth(1.5f); u.stroke(); }
            else if (icon == 4) { u.dot(ix, iy, 3.2f, fg); for (int k = 0; k < 8; ++k) { const float a = k * kPiF / 4.f; u.line(ix + std::cos(a) * 5.f, iy + std::sin(a) * 5.f, ix + std::cos(a) * 6.8f, iy + std::sin(a) * 6.8f, fg, 1.2f); } }
            else if (icon == 5) { u.beginPath(); u.circle(ix, iy, 6.f); u.strokeColor(fg); u.strokeWidth(1.2f); u.stroke(); u.txt(ix, iy + 0.5f, "?", 9.f, fg, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE, UiBase::Bold); }
            tx = ix + iw * 0.5f + 5.f + (u.txtWidth(label, fontSize, UiBase::Bold)) * 0.5f;
        }
        u.txt(tx, y + h * 0.5f + 0.5f, label, fontSize, fg, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE, UiBase::Bold);
    }
    bool press(UiBase&, float, float, int b, bool, bool) override { if (b == 1 && onClick) onClick(); return false; }
};

// Toggle bound to a bool parameter, drawn as a pill (LISTEN etc).
struct ParamToggle : PillButton {
    uint32_t id;
    ParamToggle(uint32_t pid, float px, float py, float pw, float ph, const char* lbl) : PillButton(px, py, pw, ph, lbl, nullptr), id(pid) {
        tip = kParams[pid].tip;
        isOn = [pid](UiBase& u) { return u.get(pid) > 0.5f; };
    }
    bool press(UiBase& u, float, float, int b, bool, bool) override {
        if (b == 1) { u.begin(id); u.set(id, u.get(id) > 0.5f ? 0.f : 1.f); u.end(id); }
        return false;
    }
};

// ------------------------------------------------------------- Text label
struct Label : Control {
    std::function<std::string(UiBase&)> text; float size; int align; bool bold; bool dim; float spacing;
    Label(float px, float py, std::string s, float sz = 9.5f, bool dimmed = true, int al = UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE, bool bld = false, float sp = 1.4f)
        : size(sz), align(al), bold(bld), dim(dimmed), spacing(sp) {
        x = px; y = py; w = h = 0; text = [s](UiBase&) { return s; };
    }
    bool hit(float, float) const override { return false; }
    void draw(UiBase& u) override { u.txt(x, y, text(u), size, dim ? u.th.textDim : u.th.text, align, bold ? UiBase::Bold : UiBase::Regular, spacing); }
};

// ---------------------------------------------------------- Value readout
struct ValueText : Control {
    uint32_t id; float size; int align;
    ValueText(uint32_t pid, float px, float py, float sz = 12.f, int al = UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE) : id(pid), size(sz), align(al) { x = px; y = py; }
    bool hit(float, float) const override { return false; }
    void draw(UiBase& u) override { u.txt(x, y, u.format(id, u.get(id)), size, u.th.text, align); }
};

// ----------------------------------------------------- Thin meter strip
struct MeterBar : Control {
    uint32_t id; float minV, maxV; bool fromRight = false; std::string label; bool accentOnly = true;
    float smooth = 0.f;
    MeterBar(uint32_t pid, float px, float py, float pw, float lo, float hi, const char* lbl, bool right = false)
        : id(pid), minV(lo), maxV(hi), fromRight(right), label(lbl) { x = px; y = py; w = pw; h = 8; }
    bool hit(float, float) const override { return false; }
    void draw(UiBase& u) override {
        const float tgt = std::min(std::max((u.get(id) - minV) / (maxV - minV), 0.f), 1.f);
        smooth += (tgt - smooth) * (tgt > smooth ? 0.5f : 0.12f);
        u.fillRR(x, y + 3.f, w, 3.f, 1.5f, u.th.trackDim);
        const float fw = smooth * w;
        if (fw > 0.5f) u.fillRR(fromRight ? x + w - fw : x, y + 3.f, fw, 3.f, 1.5f, u.th.accent);
    }
};

// ------------------------------------------------ Compressor transfer curve
struct CompCurveView : Control {
    CompCurveView(float px, float py, float pw, float ph) { x = px; y = py; w = pw; h = ph; tip = "Compressor curve. Drag the point to set the threshold."; }
    static constexpr float kLo = -48.f, kHi = 0.f;
    float sx(float db) const { return x + 6.f + (db - kLo) / (kHi - kLo) * (w - 12.f); }
    float sy(float db) const { return y + h - 6.f - (db - kLo) / (kHi - kLo) * (h - 12.f); }
    void draw(UiBase& u) override {
        u.fillRR(x, y, w, h, 10.f, u.dark() ? Color(14, 17, 23) : Color(236, 240, 246));
        u.strokeRR(x, y, w, h, 10.f, u.th.panelEdge, 1.f);
        const float thr = u.get(kCompThresh), mk = u.get(kCompMakeup);
        const dsp::CompCurve cc = dsp::compCurveFor(static_cast<int>(u.get(kDynStyle) + 0.5f));
        u.save(); u.scissor(x + 1.f, y + 1.f, w - 2.f, h - 2.f);
        // region above threshold
        u.beginPath(); u.rect(sx(thr), y, x + w - sx(thr), h); u.fillColor(u.th.accentSoft); u.fill();
        // unity guide
        u.beginPath(); u.moveTo(sx(kLo), sy(kLo)); u.lineTo(sx(kHi), sy(kHi)); u.strokeColor(u.th.textFaint.withAlpha(0.5f)); u.strokeWidth(1.f); u.stroke();
        // curve
        u.beginPath();
        for (int i = 0; i <= 60; ++i) {
            const float in = kLo + (kHi - kLo) * i / 60.f;
            const float out = dsp::compStaticCurve(in, thr, cc.ratio, cc.kneeDb) + (u.get(kCompMix) * 0.01f) * mk;
            if (i == 0) u.moveTo(sx(in), sy(std::min(std::max(out, kLo), kHi + 6.f))); else u.lineTo(sx(in), sy(std::min(std::max(out, kLo), kHi + 6.f)));
        }
        u.strokeColor(u.th.accent); u.strokeWidth(2.f); u.lineJoin(UiBase::ROUND); u.stroke();
        // live level marker
        const float lvl = std::min(std::max(u.get(kMeterIn), kLo), kHi);
        if (u.get(kMeterIn) > kLo) u.dot(sx(lvl), sy(dsp::compStaticCurve(lvl, thr, cc.ratio, cc.kneeDb) + (u.get(kCompMix) * 0.01f) * mk), 2.4f, u.th.accentHi.withAlpha(0.9f));
        u.restore();
        // threshold handle
        const float hx = sx(std::min(std::max(thr, kLo), kHi)), hy = sy(std::min(std::max(thr + (u.get(kCompMix) * 0.01f) * mk, kLo), kHi));
        u.dot(hx, hy, 5.f, u.th.panel);
        u.beginPath(); u.circle(hx, hy, 5.f); u.strokeColor(isActive(u) || isHover(u) ? u.th.accentHi : u.th.accent); u.strokeWidth(2.f); u.stroke();
    }
    bool press(UiBase& u, float px, float py, int b, bool, bool dbl) override {
        if (b != 1) return false;
        if (dbl) { u.reset(kCompThresh); return false; }
        u.begin(kCompThresh); drag(u, px, py, false); return true;
    }
    void drag(UiBase& u, float px, float, bool) override { u.set(kCompThresh, kLo + (px - x - 6.f) / (w - 12.f) * (kHi - kLo)); }
    void release(UiBase& u, float, float) override { u.end(kCompThresh); }
};

// ---------------------------------------------------------------- EQ curve
struct EqCurveView : Control {
    int dragging = -1;
    EqCurveView(float px, float py, float pw, float ph) { x = px; y = py; w = pw; h = ph; tip = "EQ curve. Drag the points up and down."; }
    static constexpr float kF0 = 20.f, kF1 = 20000.f, kRange = 15.f;
    float fx(float f) const { return x + 4.f + std::log(f / kF0) / std::log(kF1 / kF0) * (w - 8.f); }
    float dy(float db) const { return y + h * 0.5f - db / kRange * (h * 0.5f - 6.f); }
    dsp::EqSettings settings(UiBase& u) const {
        dsp::EqSettings s; s.low = u.get(kEqLow); s.mid = u.get(kEqMid); s.high = u.get(kEqHigh); s.air = u.get(kEqAir);
        s.lowCut = u.get(kEqLowCut); s.style = static_cast<int>(u.get(kEqStyle) + 0.5f); return s;
    }
    void draw(UiBase& u) override {
        u.fillRR(x, y, w, h, 10.f, u.dark() ? Color(14, 17, 23) : Color(236, 240, 246));
        u.strokeRR(x, y, w, h, 10.f, u.th.panelEdge, 1.f);
        const dsp::EqSettings s = settings(u);
        dsp::BiquadCoef co[5]; dsp::eqDesign(s, 48000.0, co);
        const bool on = u.get(kEqOn) > 0.5f;
        u.save(); u.scissor(x + 1.f, y + 1.f, w - 2.f, h - 2.f);
        if (s.lowCut > 21.f) { u.beginPath(); u.rect(x, y, fx(s.lowCut) - x, h); u.fillColor(u.th.accentSoft.withAlpha(0.8f)); u.fill(); }
        u.line(x + 2.f, dy(0), x + w - 2.f, dy(0), u.th.textFaint.withAlpha(0.35f), 1.f, false);
        u.beginPath();
        for (int i = 0; i <= 90; ++i) {
            const float f = kF0 * std::pow(kF1 / kF0, i / 90.f);
            double m = 1.0; for (int k = 0; k < 5; ++k) m *= co[k].magnitude(f, 48000.0);
            const float db = std::min(std::max(static_cast<float>(20.0 * std::log10(m + 1e-9)), -kRange), kRange);
            if (i == 0) u.moveTo(fx(f), dy(db)); else u.lineTo(fx(f), dy(db));
        }
        u.strokeColor(on ? u.th.accent : u.th.textFaint); u.strokeWidth(2.f); u.lineJoin(UiBase::ROUND); u.stroke();
        u.restore();
        const dsp::EqVoicing vo = dsp::eqVoicingFor(s.style);
        const double freqs[4] = {vo.lowF, vo.midF, vo.highF, vo.airF};
        const uint32_t ids[4] = {kEqLow, kEqMid, kEqHigh, kEqAir};
        for (int i = 0; i < 4; ++i) {
            const float px = fx(static_cast<float>(freqs[i])), py = dy(u.get(ids[i]));
            u.dot(px, py, 4.6f, u.th.panel);
            u.beginPath(); u.circle(px, py, 4.6f); u.strokeColor(dragging == i || (isHover(u) && nearest(u.mx, u.my, u) == i) ? u.th.accentHi : (on ? u.th.accent : u.th.textFaint)); u.strokeWidth(2.f); u.stroke();
        }
    }
    int nearest(float px, float, UiBase& u) const {
        const dsp::EqVoicing vo = dsp::eqVoicingFor(static_cast<int>(u.get(kEqStyle) + 0.5f));
        const double freqs[4] = {vo.lowF, vo.midF, vo.highF, vo.airF};
        int best = 0; float bd = 1e9f;
        for (int i = 0; i < 4; ++i) { const float d = std::fabs(px - fx(static_cast<float>(freqs[i]))); if (d < bd) { bd = d; best = i; } }
        return best;
    }
    bool press(UiBase& u, float px, float py, int b, bool, bool dbl) override {
        static const uint32_t ids[4] = {kEqLow, kEqMid, kEqHigh, kEqAir};
        if (b != 1) return false;
        dragging = nearest(px, py, u);
        if (dbl) { u.reset(ids[dragging]); dragging = -1; return false; }
        u.begin(ids[dragging]); drag(u, px, py, false); return true;
    }
    void drag(UiBase& u, float, float py, bool) override {
        static const uint32_t ids[4] = {kEqLow, kEqMid, kEqHigh, kEqAir};
        if (dragging >= 0) u.set(ids[dragging], (y + h * 0.5f - py) / (h * 0.5f - 6.f) * kRange);
    }
    void release(UiBase& u, float, float) override {
        static const uint32_t ids[4] = {kEqLow, kEqMid, kEqHigh, kEqAir};
        if (dragging >= 0) u.end(ids[dragging]);
        dragging = -1;
    }
};

// ----------------------------------------------------------- Band meters
struct BandMeters : Control {
    float smooth[3] = {0, 0, 0};
    BandMeters(float px, float py, float pw, float ph) { x = px; y = py; w = pw; h = ph; }
    bool hit(float, float) const override { return false; }
    void draw(UiBase& u) override {
        u.strokeRR(x, y, w, h, 10.f, u.th.panelEdge, 1.f);
        const char* names[3] = {"LOW", "MID", "HIGH"};
        const uint32_t ids[3] = {kBandLow, kBandMid, kBandHigh};
        const float bw = (w - 16.f) / 3.f - 6.f;
        for (int i = 0; i < 3; ++i) {
            const float bx = x + 8.f + i * (bw + 9.f);
            u.fillRR(bx, y + 8.f, bw, h - 38.f, 6.f, u.dark() ? Color(25, 28, 36) : Color(228, 233, 241));
            const float tgt = std::min(std::max((u.get(ids[i]) + 60.f) / 60.f, 0.f), 1.f);
            smooth[i] += (tgt - smooth[i]) * (tgt > smooth[i] ? 0.5f : 0.1f);
            const float fh = std::max(3.f, smooth[i] * (h - 38.f));
            u.fillRR(bx, y + 8.f + (h - 38.f) - fh, bw, fh, 3.f, u.th.accent);
            u.cap(bx + bw * 0.5f, y + h - 12.f, names[i], u.th.textDim);
        }
    }
};

} // namespace lstrip::ui
