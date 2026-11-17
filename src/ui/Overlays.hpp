// SPDX-License-Identifier: MIT
// Modal overlays: Orb (intensity macro), Pitch-correction panel, Preset browser,
// name prompt.  Overlays own child controls and route pointer events to them.
#pragma once

#include "Controls.hpp"
#include "../common/PresetStore.hpp"

#include <array>
#include <deque>
#include <random>

namespace lstrip::ui {

// ================================================================= Orb
struct Orb : Control {
    struct P { float r, a, sp, sz, ph; };
    std::array<P, 170> ps;
    float cx, cy, R;
    float glow = 0.f;
    double last = 0.0;
    Orb(float ccx, float ccy, float radius) : cx(ccx), cy(ccy), R(radius) {
        x = cx - R - 14; y = cy - R - 14; w = h = (R + 14) * 2; tip = kParams[kIntensity].tip;
        std::mt19937 g(7); std::uniform_real_distribution<float> u(0.f, 1.f);
        for (auto& p : ps) { p.r = std::sqrt(u(g)) * 0.62f; p.a = u(g) * 6.2832f; p.sp = (u(g) - 0.5f) * 0.5f; p.sz = 0.7f + u(g) * 1.9f; p.ph = u(g) * 6.2832f; }
    }
    float angleToNorm(float px, float py) const {
        float a = std::atan2(py - cy, px - cx); if (a < 0) a += 2.f * kPiF;
        float rel = a - 0.75f * kPiF; if (rel < 0) rel += 2.f * kPiF;
        if (rel > 1.5f * kPiF) return rel > 1.75f * kPiF ? 0.f : 1.f;
        return rel / (1.5f * kPiF);
    }
    void draw(UiBase& u) override {
        const double t = u.now(); const float dt = last > 0 ? static_cast<float>(std::min(t - last, 0.1)) : 0.016f; last = t;
        const float n = u.get(kIntensity) / 100.f, lvl = u.get(kOrbLevel), on = u.get(kMasterOn) > 0.5f ? 1.f : 0.35f;
        glow += (lvl - glow) * 0.2f;
        const float a0 = 0.75f * kPiF, a1 = 2.25f * kPiF, av = a0 + n * (a1 - a0);
        // ring
        u.beginPath(); u.arc(cx, cy, R, a0, a1, UiBase::CW); u.strokeColor(u.th.track); u.strokeWidth(3.f); u.lineCap(UiBase::ROUND); u.stroke();
        u.beginPath(); u.arc(cx, cy, R, a0, std::max(av, a0 + 0.002f), UiBase::CW); u.strokeColor(u.th.accent.withAlpha(on)); u.strokeWidth(3.f); u.stroke();
        const float hx = cx + std::cos(av) * R, hy = cy + std::sin(av) * R;
        u.dot(hx, hy, 6.5f, u.th.plate); u.beginPath(); u.circle(hx, hy, 6.5f); u.strokeColor(isActive(u) || isHover(u) ? u.th.accentHi : u.th.accent); u.strokeWidth(2.2f); u.stroke();
        // glow
        const float gr = R * (0.55f + 0.25f * glow + 0.15f * n);
        u.beginPath(); u.circle(cx, cy, gr * 1.6f);
        u.fillPaint(u.radialGradient(cx, cy, 2.f, gr * 1.6f, u.th.accent.withAlpha((0.28f + 0.35f * glow) * on * (0.4f + n * 0.6f)), u.th.accent.withAlpha(0.f))); u.fill();
        // particles
        for (auto& p : ps) {
            p.a += p.sp * dt * (0.4f + glow * 3.f + n);
            const float breathe = 1.f + 0.05f * std::sin(static_cast<float>(t) * 1.7f + p.ph) + glow * 0.12f * std::sin(static_cast<float>(t) * 9.f + p.ph);
            const float rr = p.r * R * breathe * (0.55f + 0.55f * n);
            const float al = (0.25f + 0.55f * (1.f - p.r)) * (0.45f + 0.55f * n) * on;
            u.dot(cx + std::cos(p.a) * rr, cy + std::sin(p.a) * rr, p.sz * (0.8f + glow * 0.5f), u.th.accent.withAlpha(std::min(al, 1.f)));
        }
        u.dot(cx, cy, 4.2f + glow * 3.f, u.th.accentHi.withAlpha(on));
        // level read-out
        char b[16]; std::snprintf(b, sizeof b, "%.0f%%", u.get(kIntensity));
        u.txt(cx, cy + R + 58.f, b, 46.f, u.th.text, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE);
        u.cap(cx, cy + R + 92.f, "LEVEL", u.th.textDim);
    }
    bool hit(float px, float py) const override { return visible && std::hypot(px - cx, py - cy) <= R + 16.f; }
    bool press(UiBase& u, float px, float py, int b, bool, bool dbl) override {
        if (b != 1) return false;
        if (dbl) { u.reset(kIntensity); return false; }
        u.begin(kIntensity); drag(u, px, py, false); return true;
    }
    void drag(UiBase& u, float px, float py, bool) override {
        const float n = angleToNorm(px, py);
        u.set(kIntensity, std::floor(n * 100.f + 0.5f));
    }
    void release(UiBase& u, float, float) override { u.end(kIntensity); }
    bool scroll(UiBase& u, float, float, float dy) override { u.begin(kIntensity); u.set(kIntensity, u.get(kIntensity) + dy * 2.f); u.end(kIntensity); return true; }
};

// ================================================================= Vertical meter strip
struct VMeter : Control {
    uint32_t id; float lo, hi, smooth = 0; bool grStyle;
    VMeter(uint32_t pid, float px, float top, float height, float mn, float mx, bool gr = false) : id(pid), lo(mn), hi(mx), grStyle(gr) { x = px - 2; y = top; w = 4; h = height; }
    bool hit(float, float) const override { return false; }
    void draw(UiBase& u) override {
        const float v = u.get(id);
        const float tgt = std::min(std::max((grStyle ? v : v - lo) / (hi - lo), 0.f), 1.f);
        smooth += (tgt - smooth) * (tgt > smooth ? 0.6f : 0.08f);
        u.fillRR(x, y, w, h, 2.f, u.th.trackDim);
        const float fh = smooth * h;
        if (fh > 0.5f) u.fillRR(x, grStyle ? y : y + h - fh, w, fh, 2.f, grStyle ? u.th.warn : u.th.accent);
    }
};

// ================================================================= Overlay base
struct Overlay : Control {
    std::function<void()> onClose;
    std::vector<std::unique_ptr<Control>> kids;
    Control* captured = nullptr;
    virtual bool wantsText() const { return false; }
    virtual void key(UiBase&, unsigned /*key*/, bool /*press*/) {}
    virtual void chars(UiBase&, const char* /*utf8*/) {}
    bool hit(float, float) const override { return true; } // modal: swallows everything
    Control* kidAt(float px, float py) {
        for (auto it = kids.rbegin(); it != kids.rend(); ++it) if ((*it)->hit(px, py)) return it->get();
        return nullptr;
    }
    void drag(UiBase& u, float px, float py, bool s) override { if (captured) captured->drag(u, px, py, s); }
    void release(UiBase& u, float px, float py) override { if (captured) { captured->release(u, px, py); captured = nullptr; } }
    void dim(UiBase& u) { u.beginPath(); u.rect(0, 0, kW, kH); u.fillColor(Color(0, 0, 0, u.dark() ? 0.62f : 0.38f)); u.fill(); }
    void card(UiBase& u, float cx, float cy, float cw, float ch) {
        u.beginPath(); u.roundedRect(cx - 4.f, cy + 6.f, cw + 8.f, ch + 8.f, 26.f);
        u.fillPaint(u.boxGradient(cx, cy + 10.f, cw, ch, 22.f, 26.f, u.th.shadow.withAlpha(0.55f), u.th.shadow.withAlpha(0.f))); u.fill();
        u.fillRR(cx, cy, cw, ch, 22.f, u.th.panel); u.strokeRR(cx, cy, cw, ch, 22.f, u.th.panelEdge, 1.f);
    }
    bool scroll(UiBase& u, float px, float py, float dy) override { if (Control* k = kidAt(px, py)) k->scroll(u, px, py, dy); return true; }
};

// ================================================================= Pitch-correction overlay
struct PitchTrace {
    static constexpr int kN = 200;
    std::array<float, kN> in{}, out{};
    int head = 0; double last = 0.0;
    PitchTrace() { in.fill(NAN); out.fill(NAN); }
    void sample(double now, float inMidi, float outMidi) {
        if (now - last < 0.0165) return;
        last = now;
        head = (head + 1) % kN;
        in[head] = inMidi > 1.f ? inMidi : NAN;
        out[head] = outMidi > 1.f ? outMidi : NAN;
    }
    float at(const std::array<float, kN>& a, int i) const { return a[(head + 1 + i) % kN]; } // i = 0 oldest
};

inline std::string noteName(int midi, bool withOctave = true) {
    static const char* n[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int pc = ((midi % 12) + 12) % 12;
    return withOctave ? std::string(n[pc]) + std::to_string(midi / 12 - 1) : n[pc];
}

struct TuneOverlay : Overlay {
    PitchTrace* trace; float cx0 = 160, cy0 = 70, cw = 620, ch = 500;
    float heldMidi = 0.f, heldOut = 0.f; double heldAt = 0.0;
    int hoverKey = -1;

    TuneOverlay(PitchTrace* t) : trace(t) {
        x = 0; y = 0; w = kW; h = kH;
        kids.emplace_back(new Knob(kTuneSpeed, cx0 + 100.f, cy0 + 428.f, 44.f));
        kids.emplace_back(new Knob(kTuneHumanize, cx0 + cw * 0.5f, cy0 + 428.f, 44.f));
        kids.emplace_back(new Knob(kTuneMix, cx0 + cw - 100.f, cy0 + 428.f, 44.f));
        kids.emplace_back(new Stepper(kTuneKey, cx0 + 110.f, cy0 + 376.f, 140.f));
        kids.emplace_back(new Stepper(kTuneScale, cx0 + cw - 120.f, cy0 + 376.f, 170.f));
    }
    uint16_t mask(UiBase& u) const {
        float fl[12]; for (int i = 0; i < 12; ++i) fl[i] = u.get(kTuneNote0 + i);
        return scaleMask(static_cast<int>(u.get(kTuneKey) + 0.5f), static_cast<int>(u.get(kTuneScale) + 0.5f), fl);
    }
    float keyX(int i) const { const float kw = (cw - 48.f - 11.f * 6.f) / 12.f; return cx0 + 24.f + i * (kw + 6.f); }
    float keyW() const { return (cw - 48.f - 11.f * 6.f) / 12.f; }

    void toggleNote(UiBase& u, int pc) {
        uint16_t m = mask(u); m ^= static_cast<uint16_t>(1u << pc);
        for (int i = 0; i < 12; ++i) { u.begin(kTuneNote0 + i); u.set(kTuneNote0 + i, (m >> i) & 1 ? 1.f : 0.f); u.end(kTuneNote0 + i); }
        u.begin(kTuneScale); u.set(kTuneScale, static_cast<float>(kScaleCustom)); u.end(kTuneScale);
    }

    void draw(UiBase& u) override {
        dim(u); card(u, cx0, cy0, cw, ch);
        const bool on = u.get(kTuneOn) > 0.5f;
        u.txt(cx0 + 28.f, cy0 + 36.f, "Pitch Correction", 17.f, u.th.text, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE, UiBase::Bold);
        // switch
        const float sx = cx0 + 190.f, sy = cy0 + 36.f;
        u.fillRR(sx, sy - 11.f, 44.f, 22.f, 11.f, on ? u.th.accent : u.th.track);
        u.dot(sx + (on ? 33.f : 11.f), sy, 8.f, u.th.onAccent);
        u.txt(sx + 56.f, sy, on ? "On \xC2\xB7 adds ~10 ms latency" : "Off", 11.5f, u.th.textDim, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
        // close
        const float qx = cx0 + cw - 34.f, qy = cy0 + 36.f;
        u.dot(qx, qy, 16.f, u.th.field);
        u.line(qx - 4.5f, qy - 4.5f, qx + 4.5f, qy + 4.5f, u.th.textDim, 1.6f); u.line(qx + 4.5f, qy - 4.5f, qx - 4.5f, qy + 4.5f, u.th.textDim, 1.6f);

        // note read-out
        const double t = u.now();
        const float im = u.get(kTuneInMidi), om = u.get(kTuneOutMidi);
        if (im > 1.f) { heldMidi = im; heldOut = om; heldAt = t; }
        const bool live = t - heldAt < 0.35 && on;
        const int nn = static_cast<int>(std::floor(heldOut + 0.5f));
        if (heldOut > 1.f) {
            const std::string pc = noteName(nn, false);
            const float tw = u.txtWidth(pc, 54.f, UiBase::Regular);
            u.txt(cx0 + 30.f, cy0 + 100.f, pc, 54.f, live ? u.th.accent : u.th.textFaint, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            u.txt(cx0 + 34.f + tw, cy0 + 114.f, std::to_string(nn / 12 - 1), 17.f, live ? u.th.accent : u.th.textFaint, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            char b[96];
            const int inNote = static_cast<int>(std::floor(heldMidi + 0.5f));
            const int inCt = static_cast<int>(std::lround((heldMidi - inNote) * 100.f));
            std::snprintf(b, sizeof b, "input  %s %+d ct", noteName(inNote).c_str(), inCt);
            u.txt(cx0 + cw - 30.f, cy0 + 88.f, b, 12.f, live ? u.th.text : u.th.textFaint, UiBase::ALIGN_RIGHT | UiBase::ALIGN_MIDDLE);
            std::snprintf(b, sizeof b, "correction  %+d ct", static_cast<int>(std::lround((heldOut - heldMidi) * 100.f)));
            u.txt(cx0 + cw - 30.f, cy0 + 108.f, b, 12.f, live ? u.th.accentHi : u.th.textFaint, UiBase::ALIGN_RIGHT | UiBase::ALIGN_MIDDLE);
        } else {
            u.txt(cx0 + 30.f, cy0 + 100.f, "\xE2\x80\x94", 46.f, u.th.textFaint, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            u.txt(cx0 + cw - 30.f, cy0 + 98.f, on ? "sing or speak to see the pitch" : "turn Pitch Correction on", 12.f, u.th.textFaint, UiBase::ALIGN_RIGHT | UiBase::ALIGN_MIDDLE);
        }

        // trace
        const float tx = cx0 + 24.f, ty = cy0 + 134.f, tw = cw - 48.f, th = 136.f;
        u.fillRR(tx, ty, tw, th, 12.f, u.dark() ? Color(14, 17, 23) : Color(236, 240, 246));
        u.strokeRR(tx, ty, tw, th, 12.f, u.th.panelEdge, 1.f);
        const float centre = heldOut > 1.f ? heldOut : 60.f, span = 3.2f;
        auto yOf = [&](float m) { return ty + th * 0.5f - (m - centre) / span * (th * 0.5f - 8.f); };
        const uint16_t mk = mask(u);
        u.save(); u.scissor(tx + 1.f, ty + 1.f, tw - 2.f, th - 2.f);
        for (int m = static_cast<int>(std::floor(centre - span - 1)); m <= static_cast<int>(std::ceil(centre + span + 1)); ++m) {
            const bool allowed = (mk >> (((m % 12) + 12) % 12)) & 1;
            const float yy = yOf(static_cast<float>(m));
            u.line(tx + 4.f, yy, tx + tw - 4.f, yy, allowed ? u.th.accent.withAlpha(0.32f) : u.th.textFaint.withAlpha(0.14f), allowed ? 1.4f : 1.f, false);
            if (allowed) u.txt(tx + 8.f, yy - 7.f, noteName(m, false), 8.5f, u.th.accent.withAlpha(0.7f), UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
        }
        auto curve = [&](const std::array<float, PitchTrace::kN>& a, const Color& c, float lw, bool dashed) {
            bool pen = false; int n = 0;
            u.beginPath();
            for (int i = 0; i < PitchTrace::kN; ++i) {
                const float v = trace->at(a, i);
                const float px = tx + 6.f + (tw - 12.f) * i / (PitchTrace::kN - 1.f);
                if (std::isnan(v)) { pen = false; continue; }
                if (dashed && ((++n / 3) & 1)) { pen = false; continue; }
                if (!pen) { u.moveTo(px, yOf(v)); pen = true; } else u.lineTo(px, yOf(v));
            }
            u.strokeColor(c); u.strokeWidth(lw); u.lineJoin(UiBase::ROUND); u.lineCap(UiBase::ROUND); u.stroke();
        };
        curve(trace->in, u.th.textDim.withAlpha(0.8f), 1.3f, true);
        curve(trace->out, u.th.accent, 2.2f, false);
        u.restore();
        u.txt(tx + 2.f, ty + th + 14.f, "--  original voice", 10.5f, u.th.textDim, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
        u.txt(tx + 112.f, ty + th + 14.f, "\xE2\x80\x94  corrected", 10.5f, u.th.accentHi, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
        u.txt(tx + tw, ty + th + 14.f, "Click the notes to allow or forbid them", 10.5f, u.th.textFaint, UiBase::ALIGN_RIGHT | UiBase::ALIGN_MIDDLE);

        // note keys
        for (int i = 0; i < 12; ++i) {
            const bool allowed = (mk >> i) & 1;
            const float kx = keyX(i), ky = cy0 + 306.f, kw = keyW();
            Color bg = allowed ? u.th.accent : u.th.field;
            if (hoverKey == i) bg = allowed ? u.th.accentHi : u.th.fieldHi;
            u.fillRR(kx, ky, kw, 34.f, 9.f, bg);
            u.txt(kx + kw * 0.5f, ky + 17.5f, noteName(i, false), 12.f, allowed ? u.th.onAccent : u.th.textDim, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE, UiBase::Bold);
        }
        u.cap(cx0 + 110.f, cy0 + 354.f, "KEY", u.th.textDim);
        u.cap(cx0 + cw - 120.f, cy0 + 354.f, "SCALE", u.th.textDim);
        for (auto& k : kids) k->draw(u);
    }

    bool press(UiBase& u, float px, float py, int b, bool ctrl, bool dbl) override {
        if (b != 1) return true;
        // close button / outside
        if (std::hypot(px - (cx0 + cw - 34.f), py - (cy0 + 36.f)) < 16.f) { if (onClose) onClose(); return false; }
        if (px < cx0 || px > cx0 + cw || py < cy0 || py > cy0 + ch) { if (onClose) onClose(); return false; }
        // power switch
        if (px > cx0 + 186.f && px < cx0 + 240.f && std::fabs(py - (cy0 + 36.f)) < 14.f) { u.begin(kTuneOn); u.set(kTuneOn, u.get(kTuneOn) > 0.5f ? 0.f : 1.f); u.end(kTuneOn); return false; }
        for (int i = 0; i < 12; ++i)
            if (px >= keyX(i) && px < keyX(i) + keyW() && py >= cy0 + 306.f && py < cy0 + 340.f) { toggleNote(u, i); return false; }
        if (Control* k = kidAt(px, py)) {
            // changing key while on a custom scale transposes the custom notes with it
            const bool keyStep = k == kids[3].get() && u.get(kTuneScale) + 0.5f >= kScaleCustom;
            const float oldKey = u.get(kTuneKey); float old[12]; for (int i = 0; i < 12; ++i) old[i] = u.get(kTuneNote0 + i);
            if (k->press(u, px, py, b, ctrl, dbl)) { captured = k; return true; }
            if (keyStep) {
                const int d = static_cast<int>(u.get(kTuneKey) - oldKey);
                if (d != 0) for (int i = 0; i < 12; ++i) { u.begin(kTuneNote0 + i); u.set(kTuneNote0 + (((i + d) % 12) + 12) % 12, old[i]); u.end(kTuneNote0 + i); }
            }
        }
        return captured != nullptr;
    }
    void motion(UiBase& u) {
        hoverKey = -1;
        for (int i = 0; i < 12; ++i)
            if (u.mx >= keyX(i) && u.mx < keyX(i) + keyW() && u.my >= cy0 + 306.f && u.my < cy0 + 340.f) hoverKey = i;
        u.hover = kidAt(u.mx, u.my) ? kidAt(u.mx, u.my) : this;
    }
    void key(UiBase&, unsigned k, bool press) override { if (press && k == 27 && onClose) onClose(); }
};

// ================================================================= Text prompt overlay
struct PromptOverlay : Overlay {
    std::string title, value; std::function<void(const std::string&)> onOk; bool selectAll = true;
    double opened = 0;
    PromptOverlay(std::string t, std::string v, std::function<void(const std::string&)> ok) : title(std::move(t)), value(std::move(v)), onOk(std::move(ok)) { x = 0; y = 0; w = kW; h = kH; }
    bool wantsText() const override { return true; }
    float bx() const { return (kW - 380.f) * 0.5f; }
    float by() const { return 215.f; }
    void draw(UiBase& u) override {
        if (opened == 0) opened = u.now();
        dim(u); card(u, bx(), by(), 380.f, 190.f);
        u.txt(bx() + 24.f, by() + 32.f, title, 15.f, u.th.text, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE, UiBase::Bold);
        u.fillRR(bx() + 24.f, by() + 58.f, 332.f, 38.f, 10.f, u.th.field);
        u.strokeRR(bx() + 24.f, by() + 58.f, 332.f, 38.f, 10.f, u.th.accent, 1.2f);
        const float tw = u.txtWidth(value, 14.f);
        if (selectAll && !value.empty()) u.fillRR(bx() + 34.f, by() + 66.f, tw + 2.f, 22.f, 4.f, u.th.accent.withAlpha(0.35f));
        u.txt(bx() + 36.f, by() + 77.f, value, 14.f, u.th.text, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
        if (static_cast<int>((u.now() - opened) * 2.0) % 2 == 0 && !selectAll) u.line(bx() + 37.f + tw, by() + 68.f, bx() + 37.f + tw, by() + 86.f, u.th.text, 1.4f);
        u.txt(bx() + 24.f, by() + 112.f, "Type a name. If typing does not work in your host, just click Save.", 10.f, u.th.textFaint, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
        for (int i = 0; i < 2; ++i) {
            const float x0 = bx() + 380.f - 24.f - 100.f - i * 108.f, y0 = by() + 134.f;
            const bool hov = u.mx >= x0 && u.mx < x0 + 100.f && u.my >= y0 && u.my < y0 + 34.f;
            u.fillRR(x0, y0, 100.f, 34.f, 17.f, i == 0 ? (hov ? u.th.accentHi : u.th.accent) : (hov ? u.th.fieldHi : u.th.field));
            u.txt(x0 + 50.f, y0 + 17.5f, i == 0 ? "Save" : "Cancel", 12.f, i == 0 ? u.th.onAccent : u.th.text, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE, UiBase::Bold);
        }
    }
    bool press(UiBase&, float px, float py, int b, bool, bool) override {
        if (b != 1) return false;
        for (int i = 0; i < 2; ++i) {
            const float x0 = bx() + 380.f - 24.f - 100.f - i * 108.f, y0 = by() + 134.f;
            if (px >= x0 && px < x0 + 100.f && py >= y0 && py < y0 + 34.f) { if (i == 0) { if (onOk) onOk(value); } else if (onClose) onClose(); return false; }
        }
        if (px < bx() || px > bx() + 380.f || py < by() || py > by() + 190.f) { if (onClose) onClose(); }
        selectAll = false;
        return false;
    }
    void key(UiBase&, unsigned k, bool press) override {
        if (!press) return;
        if (k == 27 && onClose) onClose();
        else if (k == 13) { if (onOk) onOk(value); }
        else if (k == 8 || k == 127) { if (selectAll) value.clear(); else if (!value.empty()) { do value.pop_back(); while (!value.empty() && (static_cast<unsigned char>(value.back()) & 0xC0) == 0x80); } selectAll = false; }
    }
    void chars(UiBase&, const char* s) override {
        if (!s || !*s || static_cast<unsigned char>(*s) < 32 || *s == 127) return;
        if (selectAll) { value.clear(); selectAll = false; }
        if (value.size() < 40) value += s;
    }
};

// ================================================================= Preset browser
struct PresetEntry { std::string name, category; bool factory; };

struct PresetsOverlay : Overlay {
    PresetStore* store; std::string current;
    std::function<void(const std::string& name, bool factory)> onLoad;
    std::function<void(const std::string&)> onRename, onDelete;
    std::function<void()> onSave, onImport, onExport;
    float cx0 = 250, cy0 = 66, cw = 440, ch = 508;
    std::string filter = "All"; float scrollY = 0.f; int hoverRow = -1;
    std::vector<PresetEntry> entries;

    PresetsOverlay(PresetStore* s, std::string cur) : store(s), current(std::move(cur)) { x = 0; y = 0; w = kW; h = kH; rebuild(); }

    static std::vector<PresetEntry> all(const PresetStore& st) {
        std::vector<PresetEntry> v;
        for (uint32_t i = 0; i < kNumFactoryPresets; ++i) v.push_back({kFactoryPresets[i].name, kFactoryPresets[i].category, true});
        for (const auto& p : st.user()) v.push_back({p.name, "User", false});
        return v;
    }
    void rebuild() {
        entries.clear();
        for (auto& e : all(*store)) {
            if (filter == "All" || (filter == "Starred" && store->starred(e.name)) || filter == e.category) entries.push_back(e);
        }
    }
    static const std::vector<std::string>& chips() { static const std::vector<std::string> c = {"All", "Starred", "Podcast", "Stream", "Music", "Tune", "Utility", "User"}; return c; }
    void chipRect(UiBase& u, size_t i, float& rx, float& rw) {
        float cursor = cx0 + 24.f;
        for (size_t k = 0; k <= i; ++k) { const float tw = u.txtWidth(chips()[k], 10.5f, UiBase::Bold) + 20.f; if (k == i) { rx = cursor; rw = tw; return; } cursor += tw + 6.f; }
    }
    float listTop() const { return cy0 + 112.f; }
    float listH() const { return 312.f; }
    float rowH() const { return 34.f; }
    float maxScroll() const { return std::max(0.f, entries.size() * rowH() - listH()); }

    void draw(UiBase& u) override {
        dim(u); card(u, cx0, cy0, cw, ch);
        u.txt(cx0 + 28.f, cy0 + 36.f, "Presets", 17.f, u.th.text, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE, UiBase::Bold);
        const float qx = cx0 + cw - 34.f, qy = cy0 + 36.f;
        u.dot(qx, qy, 16.f, u.th.field);
        u.line(qx - 4.5f, qy - 4.5f, qx + 4.5f, qy + 4.5f, u.th.textDim, 1.6f); u.line(qx + 4.5f, qy - 4.5f, qx - 4.5f, qy + 4.5f, u.th.textDim, 1.6f);
        for (size_t i = 0; i < chips().size(); ++i) {
            float rx, rw; chipRect(u, i, rx, rw);
            const bool sel = chips()[i] == filter;
            u.fillRR(rx, cy0 + 66.f, rw, 26.f, 13.f, sel ? u.th.accent : u.th.field);
            u.txt(rx + rw * 0.5f, cy0 + 79.5f, chips()[i], 10.5f, sel ? u.th.onAccent : u.th.textDim, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE, UiBase::Bold);
        }
        scrollY = std::min(std::max(scrollY, 0.f), maxScroll());
        u.save(); u.scissor(cx0 + 12.f, listTop(), cw - 24.f, listH());
        if (entries.empty()) u.txt(cx0 + cw * 0.5f, listTop() + 60.f, filter == "Starred" ? "No starred presets yet" : "Nothing here yet", 12.f, u.th.textFaint);
        for (size_t i = 0; i < entries.size(); ++i) {
            const float ry = listTop() + i * rowH() - scrollY;
            if (ry + rowH() < listTop() || ry > listTop() + listH()) continue;
            const PresetEntry& e = entries[i];
            const bool cur = e.name == current, hov = static_cast<int>(i) == hoverRow;
            if (cur || hov) u.fillRR(cx0 + 14.f, ry + 1.f, cw - 28.f, rowH() - 2.f, 9.f, cur ? u.th.accentSoft : u.th.field);
            iconStar(u, cx0 + 36.f, ry + rowH() * 0.5f, 6.5f, store->starred(e.name), store->starred(e.name) ? u.th.warn : u.th.textFaint);
            u.txt(cx0 + 56.f, ry + rowH() * 0.5f + 0.5f, e.name, 13.f, cur ? u.th.accentHi : u.th.text, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE, cur ? UiBase::Bold : UiBase::Regular);
            const float tagRight = e.factory ? cx0 + cw - 30.f : cx0 + cw - 84.f;
            u.txt(tagRight, ry + rowH() * 0.5f + 0.5f, e.category, 10.f, u.th.textFaint, UiBase::ALIGN_RIGHT | UiBase::ALIGN_MIDDLE, UiBase::Regular, 0.6f);
            if (!e.factory && (hov || cur)) { // rename / delete
                const float py = ry + rowH() * 0.5f;
                u.line(cx0 + cw - 66.f, py + 4.f, cx0 + cw - 56.f, py - 5.f, u.th.textDim, 1.6f); u.line(cx0 + cw - 66.f, py + 4.f, cx0 + cw - 67.f, py + 6.f, u.th.textDim, 1.6f);
                u.beginPath(); u.rect(cx0 + cw - 43.f, py - 4.f, 9.f, 10.f); u.strokeColor(u.th.textDim); u.strokeWidth(1.4f); u.stroke(); u.line(cx0 + cw - 45.f, py - 5.f, cx0 + cw - 32.f, py - 5.f, u.th.textDim, 1.4f);
            }
        }
        u.restore();
        // footer buttons
        const char* names[3] = {"Save current\xE2\x80\xA6", "Import\xE2\x80\xA6", "Export\xE2\x80\xA6"};
        for (int i = 0; i < 3; ++i) {
            const float bx = cx0 + 24.f + i * ((cw - 48.f - 16.f) / 3.f + 8.f), bw = (cw - 48.f - 16.f) / 3.f, by = cy0 + ch - 62.f;
            const bool hov = u.mx >= bx && u.mx < bx + bw && u.my >= by && u.my < by + 36.f;
            u.fillRR(bx, by, bw, 36.f, 18.f, i == 0 ? (hov ? u.th.accentHi : u.th.accent) : (hov ? u.th.fieldHi : u.th.field));
            u.txt(bx + bw * 0.5f, by + 18.5f, names[i], 12.f, i == 0 ? u.th.onAccent : u.th.text, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE, UiBase::Bold);
        }
        u.txt(cx0 + cw * 0.5f, cy0 + ch - 76.f + 4.f, "User presets are saved in ~/.local/share/L-STRIP/presets", 9.5f, u.th.textFaint, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE);
    }
    int rowAt(float px, float py) const {
        if (px < cx0 + 12.f || px > cx0 + cw - 12.f || py < listTop() || py > listTop() + listH()) return -1;
        const int r = static_cast<int>((py - listTop() + scrollY) / rowH());
        return r >= 0 && r < static_cast<int>(entries.size()) ? r : -1;
    }
    void motion(UiBase& u) { hoverRow = rowAt(u.mx, u.my); u.hover = this; }
    bool press(UiBase& u, float px, float py, int b, bool, bool) override {
        if (b != 1) return false;
        if (std::hypot(px - (cx0 + cw - 34.f), py - (cy0 + 36.f)) < 16.f || px < cx0 || px > cx0 + cw || py < cy0 || py > cy0 + ch) { if (onClose) onClose(); return false; }
        for (size_t i = 0; i < chips().size(); ++i) { float rx, rw; chipRect(u, i, rx, rw); if (px >= rx && px < rx + rw && py >= cy0 + 66.f && py < cy0 + 92.f) { filter = chips()[i]; scrollY = 0; rebuild(); return false; } }
        const int r = rowAt(px, py);
        if (r >= 0) {
            const PresetEntry e = entries[r];
            if (px < cx0 + 50.f) { store->toggleStar(e.name); rebuild(); return false; }
            if (!e.factory && px > cx0 + cw - 74.f && px < cx0 + cw - 50.f) { if (onRename) onRename(e.name); return false; }
            if (!e.factory && px >= cx0 + cw - 50.f && px < cx0 + cw - 24.f) { if (onDelete) onDelete(e.name); return false; }
            current = e.name;
            if (onLoad) onLoad(e.name, e.factory);
            return false;
        }
        for (int i = 0; i < 3; ++i) {
            const float bx = cx0 + 24.f + i * ((cw - 48.f - 16.f) / 3.f + 8.f), bw = (cw - 48.f - 16.f) / 3.f, by = cy0 + ch - 62.f;
            if (px >= bx && px < bx + bw && py >= by && py < by + 36.f) { auto& f = i == 0 ? onSave : (i == 1 ? onImport : onExport); if (f) f(); return false; }
        }
        return false;
    }
    bool scroll(UiBase&, float, float, float dy) override { scrollY = std::min(std::max(scrollY - dy * rowH() * 1.5f, 0.f), maxScroll()); return true; }
    void key(UiBase&, unsigned k, bool press) override { if (press && k == 27 && onClose) onClose(); }
};

} // namespace lstrip::ui
