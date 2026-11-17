// SPDX-License-Identifier: MIT
// A small immediate-mode vector drawing layer on top of Cairo, with the call
// names and semantics of the NanoVG subset the editor uses.
//
// Why Cairo and not OpenGL: OBS Studio renders through EGL, and an OpenGL
// (GLX) editor window created inside the OBS process crashed OBS's render
// thread on NVIDIA's proprietary driver.  A software-rendered editor shares
// no GL state with the host, so it cannot interfere with it.
#pragma once

#include "Color.hpp"
#include "Geometry.hpp"

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace lstrip::ui {

using DGL_NAMESPACE::Color;

class Gfx {
public:
    enum Align {
        ALIGN_LEFT = 1 << 0, ALIGN_CENTER = 1 << 1, ALIGN_RIGHT = 1 << 2,
        ALIGN_TOP = 1 << 3, ALIGN_MIDDLE = 1 << 4, ALIGN_BOTTOM = 1 << 5, ALIGN_BASELINE = 1 << 6
    };
    enum LineCap { BUTT, ROUND, SQUARE, BEVEL, MITER };
    enum Winding { CCW = 1, CW = 2 };
    typedef int FontId;

    struct Paint {
        enum Kind { None, Radial, Linear, Box } kind = None;
        Color inner, outer;
        float a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;   // meaning depends on kind
    };

    Gfx() = default;
    virtual ~Gfx() {
        for (auto* f : faces_) if (f) cairo_font_face_destroy(f);
        for (auto* f : ftFaces_) if (f) FT_Done_Face(f);
        if (ft_) FT_Done_FreeType(ft_);
    }

    // ------------------------------------------------------------ frame setup
    void gfxBegin(cairo_t* cr) {
        cr_ = cr;
        cap_ = BUTT; join_ = MITER; lineW_ = 1.0f; paintActive_ = false;
        cairo_set_antialias(cr_, CAIRO_ANTIALIAS_DEFAULT);
        cairo_set_operator(cr_, CAIRO_OPERATOR_OVER);
    }

    // ------------------------------------------------------------------ state
    void save() { cairo_save(cr_); }
    void restore() { cairo_restore(cr_); }
    void translate(float x, float y) { cairo_translate(cr_, x, y); }
    void scale(float x, float y) { cairo_scale(cr_, x, y); }
    void scissor(float x, float y, float w, float h) { cairo_new_path(cr_); cairo_rectangle(cr_, x, y, w, h); cairo_clip(cr_); }

    void fillColor(const Color& c) { fillC_ = c; paintActive_ = false; }
    void fillColor(int r, int g, int b, int a = 255) { fillColor(Color(r, g, b, a / 255.f)); }
    void strokeColor(const Color& c) { strokeC_ = c; }
    void strokeColor(int r, int g, int b, int a = 255) { strokeColor(Color(r, g, b, a / 255.f)); }
    void strokeWidth(float w) { lineW_ = w; }
    void lineCap(LineCap c = BUTT) { cap_ = c; }
    void lineJoin(LineCap j = MITER) { join_ = j; }
    void fillPaint(const Paint& p) { paint_ = p; paintActive_ = true; }

    // ---------------------------------------------------------------- gradients
    Paint radialGradient(float cx, float cy, float inr, float outr, const Color& ic, const Color& oc) {
        Paint p; p.kind = Paint::Radial; p.inner = ic; p.outer = oc; p.a = cx; p.b = cy; p.c = inr; p.d = outr; return p;
    }
    Paint linearGradient(float sx, float sy, float ex, float ey, const Color& ic, const Color& oc) {
        Paint p; p.kind = Paint::Linear; p.inner = ic; p.outer = oc; p.a = sx; p.b = sy; p.c = ex; p.d = ey; return p;
    }
    Paint boxGradient(float x, float y, float w, float h, float r, float feather, const Color& ic, const Color& oc) {
        Paint p; p.kind = Paint::Box; p.inner = ic; p.outer = oc; p.a = x; p.b = y; p.c = w; p.d = h; p.e = r; p.f = feather; return p;
    }

    // -------------------------------------------------------------------- paths
    void beginPath() { cairo_new_path(cr_); }
    void closePath() { cairo_close_path(cr_); }
    void moveTo(float x, float y) { cairo_move_to(cr_, x, y); }
    void lineTo(float x, float y) { cairo_line_to(cr_, x, y); }
    void arc(float cx, float cy, float r, float a0, float a1, Winding dir) {
        if (dir == CW) cairo_arc(cr_, cx, cy, r, a0, a1); else cairo_arc_negative(cr_, cx, cy, r, a0, a1);
    }
    void rect(float x, float y, float w, float h) { cairo_rectangle(cr_, x, y, w, h); }
    void circle(float cx, float cy, float r) { cairo_new_sub_path(cr_); cairo_arc(cr_, cx, cy, r, 0, 2 * M_PI); }
    void roundedRect(float x, float y, float w, float h, float r) { rrPath(x, y, w, h, r); }

    void fill() {
        if (paintActive_ && paint_.kind != Paint::None) { fillWithPaint(); return; }
        setSource(fillC_);
        cairo_fill_preserve(cr_);
    }
    void stroke() {
        setSource(strokeC_);
        cairo_set_line_width(cr_, lineW_);
        cairo_set_line_cap(cr_, cap_ == ROUND ? CAIRO_LINE_CAP_ROUND : (cap_ == SQUARE ? CAIRO_LINE_CAP_SQUARE : CAIRO_LINE_CAP_BUTT));
        cairo_set_line_join(cr_, join_ == ROUND ? CAIRO_LINE_JOIN_ROUND : (join_ == BEVEL ? CAIRO_LINE_JOIN_BEVEL : CAIRO_LINE_JOIN_MITER));
        cairo_stroke_preserve(cr_);
    }

    // --------------------------------------------------------------------- text
    // The font bytes must stay alive for the lifetime of the object (the editor embeds them in .rodata).
    FontId createFontFromMemory(const char*, const unsigned char* data, unsigned size, bool) {
        if (!ft_ && FT_Init_FreeType(&ft_) != 0) return -1;
        FT_Face face = nullptr;
        if (FT_New_Memory_Face(ft_, data, static_cast<FT_Long>(size), 0, &face) != 0) return -1;
        ftFaces_.push_back(face);
        faces_.push_back(cairo_ft_font_face_create_for_ft_face(face, 0));
        return static_cast<FontId>(faces_.size() - 1);
    }
    void fontFaceId(FontId f) { font_ = f; }
    void fontSize(float s) { fontSize_ = s; }
    void textLetterSpacing(float s) { spacing_ = s; }
    void textAlign(int a) { align_ = a; }

    float text(float x, float y, const char* s, const char*) {
        if (!s || !*s || font_ < 0 || font_ >= static_cast<int>(faces_.size())) return 0.f;
        useFont();
        cairo_font_extents_t fe; cairo_font_extents(cr_, &fe);
        const float w = measure(s);
        float px = x;
        if (align_ & ALIGN_CENTER) px = x - w * 0.5f; else if (align_ & ALIGN_RIGHT) px = x - w;
        float py = y;
        if (align_ & ALIGN_MIDDLE) py = y + static_cast<float>(fe.ascent - fe.descent) * 0.5f;
        else if (align_ & ALIGN_TOP) py = y + static_cast<float>(fe.ascent);
        else if (align_ & ALIGN_BOTTOM) py = y - static_cast<float>(fe.descent);
        setSource(fillC_);
        if (spacing_ == 0.f) { cairo_move_to(cr_, px, py); cairo_show_text(cr_, s); }
        else {
            float cx = px;
            for (size_t i = 0; s[i];) {
                const size_t n = utf8Len(static_cast<unsigned char>(s[i]));
                const std::string ch(s + i, n);
                cairo_move_to(cr_, cx, py); cairo_show_text(cr_, ch.c_str());
                cairo_text_extents_t te; cairo_text_extents(cr_, ch.c_str(), &te);
                cx += static_cast<float>(te.x_advance) + spacing_;
                i += n;
            }
        }
        return w;
    }
    float textBounds(float x, float y, const char* s, const char*, DGL_NAMESPACE::Rectangle<float>& r) {
        useFont();
        const float w = (s && *s && font_ >= 0) ? measure(s) : 0.f;
        r = DGL_NAMESPACE::Rectangle<float>(x, y, w, fontSize_);
        return w;
    }

protected:
    cairo_t* cr_ = nullptr;

private:
    static size_t utf8Len(unsigned char c) { return c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4; }
    void useFont() {
        cairo_set_font_face(cr_, faces_[static_cast<size_t>(font_)]);
        // NanoVG's font size is the line height (ascent + descent); Cairo's is the em size.
        cairo_set_font_size(cr_, 100.0);   // probe large: small sizes get their metrics rounded to whole pixels
        cairo_font_extents_t fe; cairo_font_extents(cr_, &fe);
        const double lineH = (fe.ascent + fe.descent) / 100.0;
        cairo_set_font_size(cr_, lineH > 0.0 ? fontSize_ / lineH : fontSize_);
    }
    float measure(const char* s) {
        cairo_text_extents_t te;
        if (spacing_ == 0.f) { cairo_text_extents(cr_, s, &te); return static_cast<float>(te.x_advance); }
        float w = 0; int n = 0;
        for (size_t i = 0; s[i]; ++n) {
            const size_t l = utf8Len(static_cast<unsigned char>(s[i]));
            const std::string ch(s + i, l);
            cairo_text_extents(cr_, ch.c_str(), &te);
            w += static_cast<float>(te.x_advance) + spacing_;
            i += l;
        }
        return w - (n > 0 ? spacing_ : 0.f);
    }
    void setSource(const Color& c) { cairo_set_source_rgba(cr_, c.red, c.green, c.blue, c.alpha); }
    void rrPath(float x, float y, float w, float h, float r) {
        r = std::max(0.f, std::min(r, std::min(w, h) * 0.5f));
        cairo_new_sub_path(cr_);
        cairo_arc(cr_, x + w - r, y + r, r, -M_PI / 2, 0);
        cairo_arc(cr_, x + w - r, y + h - r, r, 0, M_PI / 2);
        cairo_arc(cr_, x + r, y + h - r, r, M_PI / 2, M_PI);
        cairo_arc(cr_, x + r, y + r, r, M_PI, 3 * M_PI / 2);
        cairo_close_path(cr_);
    }
    void fillWithPaint() {
        if (paint_.kind == Paint::Radial || paint_.kind == Paint::Linear) {
            cairo_pattern_t* pat = paint_.kind == Paint::Radial
                ? cairo_pattern_create_radial(paint_.a, paint_.b, paint_.c, paint_.a, paint_.b, paint_.d)
                : cairo_pattern_create_linear(paint_.a, paint_.b, paint_.c, paint_.d);
            // Interpolate in premultiplied space like NanoVG, via a few stops (avoids dark fringes with alpha 0 ends).
            for (int i = 0; i <= 8; ++i) {
                const float t = i / 8.f;
                const float al = paint_.inner.alpha + (paint_.outer.alpha - paint_.inner.alpha) * t;
                const float r = paint_.inner.red + (paint_.outer.red - paint_.inner.red) * t;
                const float g = paint_.inner.green + (paint_.outer.green - paint_.inner.green) * t;
                const float b = paint_.inner.blue + (paint_.outer.blue - paint_.inner.blue) * t;
                cairo_pattern_add_color_stop_rgba(pat, t, r, g, b, al);
            }
            cairo_set_source(cr_, pat);
            cairo_fill_preserve(cr_);
            cairo_pattern_destroy(pat);
        } else { // Box: a feathered rounded box (used for soft shadows); built from stacked translucent layers.
            cairo_save(cr_);
            cairo_clip_preserve(cr_);
            cairo_new_path(cr_);
            const int steps = 10;
            float prev = 0.f;
            for (int i = 0; i <= steps; ++i) {
                const float t = i / static_cast<float>(steps);                  // 0 = outside, 1 = inside
                const float grow = paint_.f * (0.5f - t);                         // grows the box outwards for small t
                const float s = t * t * (3 - 2 * t);
                const float al = paint_.outer.alpha + (paint_.inner.alpha - paint_.outer.alpha) * s;
                const float layer = (al - prev) / std::max(1e-3f, 1.f - prev);
                if (layer > 0.f) {
                    rrPath(paint_.a - grow, paint_.b - grow, paint_.c + 2 * grow, paint_.d + 2 * grow, paint_.e + grow);
                    cairo_set_source_rgba(cr_, paint_.inner.red, paint_.inner.green, paint_.inner.blue, layer);
                    cairo_fill(cr_);
                    prev = al;
                }
            }
            cairo_restore(cr_);
        }
    }

    Color fillC_{255, 255, 255, 1.f}, strokeC_{255, 255, 255, 1.f};
    Paint paint_; bool paintActive_ = false;
    LineCap cap_ = BUTT, join_ = MITER;
    float lineW_ = 1.f, fontSize_ = 12.f, spacing_ = 0.f;
    int align_ = ALIGN_LEFT | ALIGN_BASELINE; FontId font_ = -1;
    FT_Library ft_ = nullptr;
    std::vector<FT_Face> ftFaces_;
    std::vector<cairo_font_face_t*> faces_;
};

} // namespace lstrip::ui
