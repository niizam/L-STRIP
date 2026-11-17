// SPDX-License-Identifier: MIT
// L-STRIP editor: layout of the five panels, header, overlays and event routing.
#include "Application.hpp"
#include "Window.hpp"

#include "Controls.hpp"
#include "IdleDriver.hpp"
#include "Overlays.hpp"
#include "UiBase.hpp"

#include "../common/PresetStore.hpp"
#include "../common/Presets.hpp"

#include <cstdlib>
#include <memory>

START_NAMESPACE_DISTRHO

using namespace lstrip;
using namespace lstrip::ui;

namespace {

constexpr float kPanelY = 72.f, kPanelH = 548.f, kPanelW = 160.f;
constexpr float kPanelX[4] = {24.f, 194.f, 586.f, 756.f}; // dynamics, space, multiband, eq
constexpr float kMasterX = 364.f, kMasterW = 212.f;

// Draws a panel card with its header row (power, title, reset).
struct PanelFrame : Control {
    std::string title; uint32_t onId;
    PanelFrame(float px, std::string t, uint32_t power) : title(std::move(t)), onId(power) { x = px; y = kPanelY; w = kPanelW; h = kPanelH; }
    bool hit(float, float) const override { return false; }
    void draw(UiBase& u) override {
        u.beginPath(); u.roundedRect(x - 2.f, y + 4.f, w + 4.f, h + 6.f, 24.f);
        u.fillPaint(u.boxGradient(x, y + 8.f, w, h, 22.f, 22.f, u.th.shadow.withAlpha(u.dark() ? 0.35f : 0.12f), u.th.shadow.withAlpha(0.f))); u.fill();
        u.fillRR(x, y, w, h, 22.f, u.th.panel);
        u.strokeRR(x, y, w, h, 22.f, u.th.panelEdge, 1.f);
        const bool on = u.get(onId) > 0.5f;
        u.cap(x + w * 0.5f, y + 25.f, title, on ? u.th.textDim : u.th.textFaint, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE, 10.f);
        if (!on) { u.beginPath(); u.roundedRect(x, y + 44.f, w, h - 44.f, 22.f); u.fillColor(u.th.panel.withAlpha(0.35f)); u.fill(); }
    }
};

// Wraps a control so it is drawn dimmed when its module is switched off.
struct PresetBox : Control {
    std::function<void()> onOpen; std::function<std::string()> name; std::function<std::string()> tag; std::function<bool()> modified;
    PresetBox(float px, float py, float pw, float ph) { x = px; y = py; w = pw; h = ph; tip = "Open the preset browser."; }
    void draw(UiBase& u) override {
        u.fillRR(x, y, w, h, h * 0.5f, isHover(u) ? u.th.fieldHi : u.th.field);
        const std::string n = name() + (modified() ? " *" : "");
        const std::string t = tag();
        const float nw = u.txtWidth(n, 13.f, UiBase::Bold), tw = t.empty() ? 0.f : u.txtWidth(t, 10.f) + 8.f;
        const float start = x + w * 0.5f - (nw + tw) * 0.5f - 6.f;
        u.txt(start, y + h * 0.5f + 0.5f, n, 13.f, u.th.text, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE, UiBase::Bold);
        u.txt(start + nw + 8.f, y + h * 0.5f + 1.5f, t, 10.f, u.th.accent, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
        iconChevron(u, x + w - 20.f, y + h * 0.5f + 1.f, 4.f, 2, u.th.textDim);
    }
    bool press(UiBase&, float, float, int b, bool, bool) override { if (b == 1 && onOpen) onOpen(); return false; }
};

struct ArrowButton : Control {
    int dir; std::function<void()> cb;
    ArrowButton(float cx, float cy, int d, std::function<void()> f) : dir(d), cb(std::move(f)) { x = cx - 13; y = cy - 13; w = h = 26; tip = d < 0 ? "Previous preset." : "Next preset."; }
    void draw(UiBase& u) override { iconChevron(u, x + 13.f, y + 13.f, 4.5f, dir, isHover(u) ? u.th.text : u.th.textDim); }
    bool press(UiBase&, float, float, int b, bool, bool) override { if (b == 1 && cb) cb(); return false; }
};

struct BigText : Control { // fixed text drawn with explicit style (logo, footer)
    std::string s; float size; bool bold; bool dim; float spacing; int align;
    BigText(float px, float py, std::string str, float sz, bool bld, bool dimmed, float sp, int al) : s(std::move(str)), size(sz), bold(bld), dim(dimmed), spacing(sp), align(al) { x = px; y = py; }
    bool hit(float, float) const override { return false; }
    void draw(UiBase& u) override { u.txt(x, y, s, size, dim ? u.th.textDim : u.th.text, align, bold ? UiBase::Bold : UiBase::Regular, spacing); }
};

struct Divider : Control {
    Divider(float px, float py, float pw) { x = px; y = py; w = pw; h = 1; }
    bool hit(float, float) const override { return false; }
    void draw(UiBase& u) override { u.fillRR(x, y, w, 1.f, 0.5f, u.th.panelEdge); }
};

} // namespace

class LStripUI : public UiBase {
public:
    LStripUI()
        : store_(), driver_([this] { getWindow().getApp().idle(); tick(); })
    {
        store_.loadConfig();
        store_.reload();
        setDark(store_.dark());
        for (uint32_t i = 0; i < kNumInputParams; ++i) baseline_[i] = kParams[i].def;
        buildMain();
        driver_.start();
    }
    ~LStripUI() override { driver_.stop(); }

protected:
    // ----------------------------------------------------------- DPF callbacks
    void uiIdle() override { driver_.noteHostIdle(); tick(); }

    void parameterChanged(uint32_t index, float value) override { UiBase::parameterChanged(index, value); }

    void onPresetStateFromHost(const std::string& n) override { if (!n.empty()) { currentName_ = n; identified_ = false; tickCount_ = 0; } }

    void uiFileBrowserSelected(const char* filename) override {
        if (!filename) return;
        if (browse_ == Browse::Import) {
            std::string name, err;
            if (store_.importFile(filename, &name, &err)) { toast("Imported \"" + name + "\""); if (presets_) presets_->rebuild(); }
            else toast("Import failed: " + err);
        } else if (browse_ == Browse::Export) {
            std::string path = filename;
            if (path.size() < 12 || path.compare(path.size() - 12, 12, ".lstrip.json") != 0) path += ".lstrip.json";
            float v[kNumInputParams]; for (uint32_t i = 0; i < kNumInputParams; ++i) v[i] = get(i);
            std::string out; const std::string dir = path.substr(0, path.find_last_of('/') == std::string::npos ? 0 : path.find_last_of('/'));
            std::string base = path.substr(path.find_last_of('/') == std::string::npos ? 0 : path.find_last_of('/') + 1);
            base = base.substr(0, base.size() - 12);
            if (store_.exportTo(base.empty() ? currentName_ : base, v, dir.empty() ? "." : dir, &out)) toast("Exported to " + out);
            else toast("Export failed");
        }
        browse_ = Browse::None;
    }

    // ------------------------------------------------------------- drawing
    void onNanoDisplay() override {
        const float W = static_cast<float>(getWidth()), H = static_cast<float>(getHeight());
        const float s = std::min(W / kW, H / kH);
        ox_ = (W - kW * s) * 0.5f; oy_ = (H - kH * s) * 0.5f; scale_ = s;

        // window backdrop
        beginPath(); rect(0, 0, W, H); fillColor(th.window); fill();
        beginPath(); rect(0, 0, W, H);
        fillPaint(radialGradient(W * 0.5f, H * 0.18f, 10.f, std::max(W, H) * 0.75f, th.windowGlow.withAlpha(dark() ? 0.55f : 0.7f), th.windowGlow.withAlpha(0.f))); fill();

        save();
        translate(ox_, oy_); scale(s, s);
        // plate
        fillRR(0, 0, kW, kH, 26.f, th.plate); strokeRR(0, 0, kW, kH, 26.f, th.panelEdge.withAlpha(0.8f), 1.f);

        for (auto& c : main_) if (c->visible) c->draw(*this);

        if (overlay_) overlay_->draw(*this);

        drawTooltip();
        drawToast();
        restore();
    }

    // ---------------------------------------------------------------- input
    bool toDesign(double px, double py, float& dx, float& dy) const {
        dx = static_cast<float>((px - ox_) / scale_); dy = static_cast<float>((py - oy_) / scale_);
        return true;
    }

    Control* pick(float px, float py) {
        if (overlay_) return overlay_.get();
        for (auto it = main_.rbegin(); it != main_.rend(); ++it) if ((*it)->visible && (*it)->hit(px, py)) return it->get();
        return nullptr;
    }

    bool onMouse(const MouseEvent& ev) override {
        float px, py; toDesign(ev.pos.getX(), ev.pos.getY(), px, py);
        mx = px; my = py;
        const bool ctrl = (ev.mod & kModifierControl) != 0;
        if (ev.press) {
            const double t = now();
            const bool dbl = t - lastClickT_ < 0.35 && std::hypot(px - lastClickX_, py - lastClickY_) < 6.f;
            lastClickT_ = dbl ? 0.0 : t; lastClickX_ = px; lastClickY_ = py;
            Control* c = pick(px, py);
            if (!c) return false;
            hover = c;
            if (c->press(*this, px, py, static_cast<int>(ev.button), ctrl, dbl)) active = c;
            else if (overlay_) { /* overlay handled it */ }
            return true;
        }
        if (active) {
            Control* a = active; active = nullptr;
            a->release(*this, px, py);
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override {
        float px, py; toDesign(ev.pos.getX(), ev.pos.getY(), px, py);
        mx = px; my = py;
        if (active) { active->drag(*this, px, py, (ev.mod & kModifierShift) != 0); return true; }
        Control* c = pick(px, py);
        if (auto* t = dynamic_cast<TuneOverlay*>(overlay_.get())) t->motion(*this);
        else if (auto* p = dynamic_cast<PresetsOverlay*>(overlay_.get())) p->motion(*this);
        else { if (c != hover) hoverSince = now(); hover = c; }
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override {
        float px, py; toDesign(ev.pos.getX(), ev.pos.getY(), px, py);
        Control* c = pick(px, py);
        if (!c) return false;
        if (overlay_) return overlay_->scroll(*this, px, py, static_cast<float>(ev.delta.getY()));
        return c->scroll(*this, px, py, static_cast<float>(ev.delta.getY()));
    }

    bool onKeyboard(const KeyboardEvent& ev) override {
        if (overlay_) { overlay_->key(*this, ev.key, ev.press); return true; }
        return false;
    }
    bool onCharacterInput(const CharacterInputEvent& ev) override {
        if (overlay_ && overlay_->wantsText()) { overlay_->chars(*this, ev.string); return true; }
        return false;
    }

private:
    enum class Browse { None, Import, Export };

    // ------------------------------------------------------------- building
    template <class T, class... A> T* add(A&&... a) { auto* p = new T(std::forward<A>(a)...); main_.emplace_back(p); return p; }

    void panelHeader(float px, const char* title, uint32_t power, std::vector<uint32_t> resetIds) {
        add<PanelFrame>(px, title, power);
        add<PowerButton>(power, px + 25.f, kPanelY + 25.f);
        add<ResetButton>(px + kPanelW - 25.f, kPanelY + 25.f, std::move(resetIds));
    }

    void buildMain() {
        // ------------------------------------------------------------- header
        auto* dotLogo = add<BigText>(46.f, 38.f, "L-STRIP", 21.f, true, false, 3.2f, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
        (void)dotLogo;
        add<BigText>(137.f, 40.f, "vocal chain", 9.5f, false, true, 0.4f, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
        add<StatusDot>(30.f, 38.f);
        auto* tune = add<PillButton>(222.f, 24.f, 66.f, 28.f, "TUNE", [this] { openTune(); });
        tune->icon = 2; tune->isOn = [](UiBase& u) { return u.get(kTuneOn) > 0.5f; }; tune->tip = "Real-time pitch correction: key, scale and speed.";
        auto* box = add<PresetBox>(352.f, 22.f, 236.f, 32.f);
        box->onOpen = [this] { openPresets(); };
        box->name = [this] { return currentName_; };
        box->tag = [this] { return currentTag(); };
        box->modified = [this] { return isModified(); };
        add<ArrowButton>(334.f, 38.f, -1, [this] { stepPreset(-1); });
        add<ArrowButton>(606.f, 38.f, 1, [this] { stepPreset(1); });
        auto* theme = add<PillButton>(734.f, 24.f, 66.f, 28.f, "Light", [this] { setDark(!dark()); store_.setDark(dark()); });
        theme->icon = 3; theme->tip = "Switch between dark and light mode.";
        theme->label = "Light";
        themeBtn_ = theme;
        auto* save = add<PillButton>(808.f, 24.f, 48.f, 28.f, "Save", [this] { openSave(); });
        save->tip = "Save the current settings as a preset.";
        auto* pres = add<PillButton>(862.f, 24.f, 56.f, 28.f, "Presets", [this] { openPresets(); });
        pres->filled = true; pres->tip = "Browse, star, rename, delete, import and export presets.";

        // ------------------------------------------------------------ dynamics
        {
            const float px = kPanelX[0], py = kPanelY;
            panelHeader(px, "DYNAMICS", kDynOn, {kDynStyle, kCompThresh, kCompMakeup, kCompTone, kCompMix, kCompMode, kDeessFreq, kDeessAmount, kDeessRange, kDeessListen, kDynOn});
            add<Carousel>(kDynStyle, px + 10.f, py + 48.f, 140.f);
            add<CompCurveView>(px + 10.f, py + 96.f, 140.f, 62.f);
            add<Label>(px + 14.f, py + 176.f, "COMPRESSOR", 9.5f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            add<Knob>(kCompThresh, px + 46.f, py + 214.f, 42.f, "THRESH");
            add<Knob>(kCompMakeup, px + 114.f, py + 214.f, 42.f, "MAKEUP");
            add<Knob>(kCompTone, px + 46.f, py + 302.f, 42.f, "TONE", true);
            add<Knob>(kCompMix, px + 114.f, py + 302.f, 42.f, "MIX");
            add<Segmented>(kCompMode, px + 10.f, py + 372.f, 140.f);
            add<Label>(px + 14.f, py + 424.f, "DE-ESSER", 9.5f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            auto* listen = add<ParamToggle>(kDeessListen, px + 84.f, py + 411.f, 66.f, 26.f, "LISTEN"); listen->icon = 1; listen->fontSize = 10.5f;
            add<Knob>(kDeessFreq, px + 36.f, py + 468.f, 34.f, "FREQ");
            add<Label>(px + 72.f, py + 453.f, "AMOUNT", 9.f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            add<ValueText>(kDeessAmount, px + 150.f, py + 453.f, 11.f, UiBase::ALIGN_RIGHT | UiBase::ALIGN_MIDDLE);
            add<HSlider>(kDeessAmount, px + 76.f, 70.f, py + 472.f);
            add<Label>(px + 72.f, py + 499.f, "RANGE", 9.f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            add<Stepper>(kDeessRange, px + 116.f, py + 520.f, 80.f, -1.f); // display is the negated reduction, so + = more
        }

        // --------------------------------------------------------------- space
        {
            const float px = kPanelX[1], py = kPanelY;
            panelHeader(px, "SPACE", kSpaceOn, {kSpaceType, kReverbMix, kDelayMix, kReverbDuck, kDelayDuck, kDelayTime, kChorusRate, kChorusMix, kChorusVoices});
            add<Carousel>(kSpaceType, px + 10.f, py + 48.f, 140.f);
            add<Knob>(kReverbMix, px + 44.f, py + 124.f, 50.f, "REVERB");
            add<Knob>(kDelayMix, px + 116.f, py + 124.f, 50.f, "DELAY");
            for (int c = 0; c < 2; ++c) {
                const float bx = px + 12.f + c * 70.f;
                add<Label>(bx, py + 226.f, "SEND", 8.5f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
                add<MeterBar>(c == 0 ? kReverbSend : kDelaySend, bx + 30.f, py + 222.f, 32.f, -60.f, 0.f, "");
                add<Label>(bx, py + 244.f, "DUCK", 8.5f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
                add<MeterBar>(c == 0 ? kReverbDuckGR : kDelayDuckGR, bx + 30.f, py + 240.f, 32.f, 0.f, 12.f, "");
                add<Label>(px + 44.f + c * 72.f, py + 272.f, "DUCKING", 9.f, true);
                add<Stepper>(c == 0 ? kReverbDuck : kDelayDuck, px + 44.f + c * 72.f, py + 294.f, 70.f, 1.f);
            }
            add<Label>(px + 14.f, py + 336.f, "DELAY TIME", 9.f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            add<Stepper>(kDelayTime, px + 112.f, py + 336.f, 84.f, 1.f);
            add<Divider>(px + 14.f, py + 366.f, 132.f);
            add<Label>(px + 14.f, py + 390.f, "CHORUS", 9.5f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            add<Knob>(kChorusRate, px + 36.f, py + 440.f, 34.f, "RATE");
            add<Label>(px + 76.f, py + 420.f, "MIX", 9.f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            add<ValueText>(kChorusMix, px + 150.f, py + 420.f, 11.f, UiBase::ALIGN_RIGHT | UiBase::ALIGN_MIDDLE);
            add<HSlider>(kChorusMix, px + 80.f, 66.f, py + 438.f);
            add<Label>(px + 76.f, py + 468.f, "VOICES", 9.f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            add<Stepper>(kChorusVoices, px + 118.f, py + 490.f, 76.f, 1.f);
        }

        // ----------------------------------------------------------- multiband
        {
            const float px = kPanelX[2], py = kPanelY;
            panelHeader(px, "MULTIBAND", kMbOn, {kMbStyle, kMbDepth, kMbTime, kMbUpward, kMbDown, kMbOutput, kMbLow, kMbMid, kMbHigh});
            add<Carousel>(kMbStyle, px + 10.f, py + 48.f, 140.f);
            add<BandMeters>(px + 10.f, py + 96.f, 140.f, 70.f);
            add<Knob>(kMbDepth, px + 44.f, py + 210.f, 50.f, "DEPTH");
            add<Knob>(kMbTime, px + 116.f, py + 210.f, 50.f, "TIME");
            add<Knob>(kMbUpward, px + 28.f, py + 322.f, 34.f, "UPWARD");
            add<Knob>(kMbDown, px + 80.f, py + 322.f, 34.f, "DOWN");
            add<Knob>(kMbOutput, px + 132.f, py + 322.f, 34.f, "OUTPUT", true);
            const char* names[3] = {"LOW", "MID", "HIGH"}; const uint32_t ids[3] = {kMbLow, kMbMid, kMbHigh};
            for (int i = 0; i < 3; ++i) {
                const float yy = py + 424.f + i * 36.f;
                add<Label>(px + 14.f, yy, names[i], 9.f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
                add<HSlider>(ids[i], px + 50.f, 62.f, yy, true);
                add<ValueText>(ids[i], px + 150.f, yy, 11.f, UiBase::ALIGN_RIGHT | UiBase::ALIGN_MIDDLE);
            }
        }

        // ------------------------------------------------------------------ eq
        {
            const float px = kPanelX[3], py = kPanelY;
            panelHeader(px, "EQUALIZER", kEqOn, {kEqStyle, kEqLow, kEqMid, kEqHigh, kEqAir, kEqLowCut});
            add<Carousel>(kEqStyle, px + 10.f, py + 48.f, 140.f);
            add<EqCurveView>(px + 10.f, py + 96.f, 140.f, 78.f);
            const uint32_t ids[4] = {kEqLow, kEqMid, kEqHigh, kEqAir};
            const char* names[4] = {"LOW", "MID", "HIGH", "AIR"};
            for (int i = 0; i < 4; ++i) {
                auto* f = add<VFader>(ids[i], px + 24.f + i * 37.f, py + 212.f, 150.f, names[i], true, 36.f);
                f->showUnit = false;
            }
                        add<Label>(px + 14.f, py + 448.f, "LOW-CUT", 9.5f, true, UiBase::ALIGN_LEFT | UiBase::ALIGN_MIDDLE);
            add<LowCutValue>(px + 148.f, py + 448.f);
            add<HSlider>(kEqLowCut, px + 18.f, 124.f, py + 480.f);
        }

        // -------------------------------------------------------------- master
        {
            const float cx = kMasterX + kMasterW * 0.5f, py = kPanelY;
            add<Label>(cx + 14.f, py + 25.f, "MASTER", 10.f, true, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE);
            add<PowerButton>(kMasterOn, cx - 34.f, py + 25.f, true);
            add<Orb>(cx, py + 124.f, 82.f);
            const float top = py + 336.f, hgt = 124.f;
            const float fx[4] = {cx - 78.f, cx - 26.f, cx + 26.f, cx + 78.f};
            add<VMeter>(kGateGR, fx[0] - 14.f, top, hgt, 0.f, 40.f, true);
            add<VFader>(kGateThresh, fx[0], top, hgt, "GATE", false, 48.f);
            add<VMeter>(kMeterIn, fx[1] - 14.f, top, hgt, -60.f, 6.f);
            add<VFader>(kInGain, fx[1], top, hgt, "IN", false, 48.f);
            add<VMeter>(kMeterOut, fx[2] - 14.f, top, hgt, -60.f, 6.f);
            add<VFader>(kOutGain, fx[2], top, hgt, "OUT", false, 48.f);
            add<VFader>(kOutSat, fx[3], top, hgt, "OUT SAT", false, 48.f);
            auto* tips = add<PillButton>(cx - 32.f, py + 514.f, 64.f, 26.f, "TIPS", [this] { tipsOn = !tipsOn; toast(tipsOn ? "Hover tips on" : "Hover tips off"); });
            tips->icon = 5; tips->isOn = [](UiBase& u) { return u.tipsOn; }; tips->fontSize = 10.5f; tips->tip = "Show or hide a short explanation when you hover over a control.";
            add<BigText>(cx, py + 556.f, "OPEN SOURCE \xC2\xB7 MIT", 8.5f, false, true, 1.6f, UiBase::ALIGN_CENTER | UiBase::ALIGN_MIDDLE);
        }
    }

    struct StatusDot : Control {
        StatusDot(float cx, float cy) { x = cx - 8; y = cy - 8; w = h = 16; }
        bool hit(float, float) const override { return false; }
        void draw(UiBase& u) override {
            const bool on = u.get(kMasterOn) > 0.5f; const float cx = x + 8.f, cy = y + 8.f;
            if (on) { u.beginPath(); u.circle(cx, cy, 9.f); u.fillPaint(u.radialGradient(cx, cy, 1.f, 9.f, u.th.accent.withAlpha(0.45f), u.th.accent.withAlpha(0.f))); u.fill(); }
            u.dot(cx, cy, 4.2f, on ? u.th.accent : u.th.textFaint);
        }
    };
    struct LowCutValue : Control {
        LowCutValue(float rx, float cy) { x = rx; y = cy; }
        bool hit(float, float) const override { return false; }
        void draw(UiBase& u) override {
            const float v = u.get(kEqLowCut);
            if (v <= kParams[kEqLowCut].min + 1.f) { u.txt(x, y, "Off", 20.f, u.th.text, UiBase::ALIGN_RIGHT | UiBase::ALIGN_MIDDLE); return; }
            char b[16]; std::snprintf(b, sizeof b, "%.0f", v);
            const float uw = u.txtWidth("Hz", 10.f);
            u.txt(x, y + 3.f, "Hz", 10.f, u.th.textDim, UiBase::ALIGN_RIGHT | UiBase::ALIGN_MIDDLE);
            u.txt(x - uw - 3.f, y, b, 22.f, u.th.text, UiBase::ALIGN_RIGHT | UiBase::ALIGN_MIDDLE);
        }
    };

    // --------------------------------------------------------------- overlays
    // Overlays are never destroyed from inside their own callbacks: they are parked in
    // dead_ and released on the next tick.
    void setOverlay(std::unique_ptr<Overlay> o, PresetsOverlay* presets = nullptr) {
        if (overlay_) dead_.push_back(std::move(overlay_));
        overlay_ = std::move(o); presets_ = presets; active = nullptr; hover = nullptr;
    }
    void closeOverlay() { setOverlay(nullptr); }

    void openTune() {
        auto o = std::make_unique<TuneOverlay>(&trace_);
        o->onClose = [this] { closeOverlay(); };
        setOverlay(std::move(o));
    }
    void openPresets() {
        store_.reload();
        auto o = std::make_unique<PresetsOverlay>(&store_, currentName_);
        PresetsOverlay* self = o.get();
        o->onClose = [this] { closeOverlay(); };
        o->onLoad = [this](const std::string& n, bool factory) { loadPreset(n, factory); };
        o->onSave = [this] { openSave(); };
        o->onImport = [this] { browse_ = Browse::Import; FileBrowserOptions fo; fo.title = "Import L-STRIP preset"; fo.startDir = store_.dataDir().c_str(); openFileBrowser(fo); };
        o->onExport = [this] { browse_ = Browse::Export; FileBrowserOptions fo; fo.saving = true; fo.title = "Export preset"; exportName_ = PresetStore::fileNameFor(currentName_); fo.defaultName = exportName_.c_str(); openFileBrowser(fo); };
        o->onRename = [this](const std::string& n) { openRename(n); };
        o->onDelete = [this](const std::string& n) { if (store_.remove(n)) { toast("Deleted \"" + n + "\""); if (n == currentName_) { currentName_ = "Init"; } if (presets_) presets_->rebuild(); } };
        setOverlay(std::move(o), self);
    }
    void openSave() {
        const bool user = store_.exists(currentName_);
        auto o = std::make_unique<PromptOverlay>("Save preset", user ? currentName_ : store_.nextName(), [this](const std::string& n) {
            float v[kNumInputParams]; for (uint32_t i = 0; i < kNumInputParams; ++i) v[i] = get(i);
            std::string err;
            if (store_.save(n, v, &err)) { currentName_ = PresetStore::cleanName(n); setState("preset", currentName_.c_str()); captureBaseline(); toast("Saved \"" + currentName_ + "\""); closeOverlay(); }
            else toast(err);
        });
        o->onClose = [this] { closeOverlay(); };
        setOverlay(std::move(o));
    }
    void openRename(const std::string& from) {
        auto o = std::make_unique<PromptOverlay>("Rename preset", from, [this, from](const std::string& n) {
            std::string err;
            if (store_.rename(from, n, &err)) { if (currentName_ == from) { currentName_ = PresetStore::cleanName(n); setState("preset", currentName_.c_str()); } toast("Renamed"); openPresets(); }
            else toast(err);
        });
        o->onClose = [this] { openPresets(); };
        setOverlay(std::move(o));
    }

    // ---------------------------------------------------------------- presets
    void applyValues(const float* v) {
        for (uint32_t i = 0; i < kNumInputParams; ++i) { begin(i); set(i, v[i]); end(i); }
    }
    void loadPreset(const std::string& name, bool factory) {
        float v[kNumInputParams];
        if (factory) {
            for (uint32_t i = 0; i < kNumFactoryPresets; ++i) if (name == kFactoryPresets[i].name) { presetToValues(kFactoryPresets[i], v); break; }
        } else if (const UserPreset* p = store_.find(name)) std::copy(p->values, p->values + kNumInputParams, v);
        else { toast("Preset not found"); return; }
        applyValues(v);
        currentName_ = name; setState("preset", name.c_str()); captureBaseline();
    }
    // Match the live parameter values against the known presets (host loads program 0 itself).
    void identifyCurrent() {
        identified_ = true;
        float cur[kNumInputParams]; for (uint32_t i = 0; i < kNumInputParams; ++i) cur[i] = get(i);
        auto same = [&](const float* v) { for (uint32_t i = 0; i < kNumInputParams; ++i) if (std::fabs(v[i] - cur[i]) > 1e-3f) return false; return true; };
        float v[kNumInputParams];
        for (uint32_t k = 0; k < kNumFactoryPresets; ++k) { presetToValues(kFactoryPresets[k], v); if (same(v)) { currentName_ = kFactoryPresets[k].name; captureBaseline(); return; } }
        for (const auto& p : store_.user()) if (same(p.values)) { currentName_ = p.name; captureBaseline(); return; }
        if (!presetName.empty()) currentName_ = presetName; else currentName_ = "Untitled";
        captureBaseline();
    }
    void captureBaseline() { for (uint32_t i = 0; i < kNumInputParams; ++i) baseline_[i] = get(i); }
    bool isModified() const { for (uint32_t i = 0; i < kNumInputParams; ++i) if (std::fabs(baseline_[i] - get(i)) > 1e-4f) return true; return false; }
    std::string currentTag() const {
        for (uint32_t i = 0; i < kNumFactoryPresets; ++i) if (currentName_ == kFactoryPresets[i].name) return kFactoryPresets[i].category;
        return store_.exists(currentName_) ? "User" : "";
    }
    void stepPreset(int dir) {
        const auto list = PresetsOverlay::all(store_);
        int cur = -1;
        for (size_t i = 0; i < list.size(); ++i) if (list[i].name == currentName_) cur = static_cast<int>(i);
        const int n = static_cast<int>(list.size());
        const int next = cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
        loadPreset(list[next].name, list[next].factory);
    }

    // -------------------------------------------------------------------- tick
    void tick() {
        dead_.clear();
        pollShared();
        if (!identified_ && ++tickCount_ > 3) identifyCurrent();
        const double t = now();
        trace_.sample(t, get(kTuneInMidi), get(kTuneOutMidi));
        if (themeBtn_) { themeBtn_->label = dark() ? "Light" : "Dark"; themeBtn_->icon = dark() ? 4 : 3; }
        repaint();
    }

    void drawTooltip() {
        if (!tipsOn || overlay_ || !hover || !hover->tip || !*hover->tip || active) return;
        if (now() - hoverSince < 0.45) return;
        const std::string s = hover->tip;
        // wrap to ~230 px
        std::vector<std::string> lines; std::string cur;
        size_t pos = 0;
        while (pos < s.size()) {
            size_t sp = s.find(' ', pos); if (sp == std::string::npos) sp = s.size();
            const std::string word = s.substr(pos, sp - pos);
            const std::string tryLine = cur.empty() ? word : cur + " " + word;
            if (txtWidth(tryLine, 11.f) > 220.f && !cur.empty()) { lines.push_back(cur); cur = word; } else cur = tryLine;
            pos = sp + 1;
        }
        if (!cur.empty()) lines.push_back(cur);
        float tw = 0; for (auto& l : lines) tw = std::max(tw, txtWidth(l, 11.f));
        const float bw = tw + 20.f, bh = lines.size() * 15.f + 14.f;
        float bx = std::min(std::max(mx + 14.f, 6.f), kW - bw - 6.f), by = my + 20.f;
        if (by + bh > kH - 6.f) by = my - bh - 12.f;
        beginPath(); roundedRect(bx - 1.f, by + 3.f, bw + 2.f, bh + 2.f, 10.f); fillColor(th.shadow.withAlpha(0.35f)); fill();
        fillRR(bx, by, bw, bh, 9.f, dark() ? Color(34, 39, 50) : Color(32, 38, 52));
        strokeRR(bx, by, bw, bh, 9.f, th.accent.withAlpha(0.6f), 1.f);
        for (size_t i = 0; i < lines.size(); ++i) txt(bx + 10.f, by + 14.f + i * 15.f, lines[i], 11.f, Color(236, 240, 248), ALIGN_LEFT | ALIGN_MIDDLE);
    }
    void drawToast() {
        if (now() > toastUntil_ || toast_.empty()) return;
        const float a = static_cast<float>(std::min(1.0, (toastUntil_ - now()) * 3.0));
        const float tw = txtWidth(toast_, 12.f, Bold) + 36.f;
        const float bx = (kW - tw) * 0.5f, by = kH - 44.f;
        fillRR(bx, by, tw, 30.f, 15.f, th.accent.withAlpha(0.95f * a));
        txt(kW * 0.5f, by + 15.5f, toast_, 12.f, th.onAccent.withAlpha(a), ALIGN_CENTER | ALIGN_MIDDLE, Bold);
    }

    PresetStore store_;
    IdleDriver driver_;
    std::vector<std::unique_ptr<Control>> main_;
    std::unique_ptr<Overlay> overlay_;
    std::vector<std::unique_ptr<Overlay>> dead_;
    PresetsOverlay* presets_ = nullptr;
    PillButton* themeBtn_ = nullptr;
    PitchTrace trace_;
    std::string currentName_ = "Podcast Clean", exportName_;
    float baseline_[kNumInputParams];
    Browse browse_ = Browse::None;
    float ox_ = 0, oy_ = 0, scale_ = 1;
    double lastClickT_ = 0; float lastClickX_ = 0, lastClickY_ = 0;
    unsigned tickCount_ = 0;
    bool identified_ = false;
};

UI* createUI() { return new LStripUI(); }

END_NAMESPACE_DISTRHO
