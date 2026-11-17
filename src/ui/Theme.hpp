// SPDX-License-Identifier: MIT
// Colour tokens.  Light/Dark is a token swap; nothing else in the UI hard-codes a colour.
#pragma once
#include "Color.hpp"

namespace lstrip::ui {

using DGL_NAMESPACE::Color;

struct Theme {
    Color window, windowGlow, plate, panel, panelEdge, field, fieldHi, track, trackDim;
    Color text, textDim, textFaint, accent, accentHi, accentSoft, onAccent, shadow;
    Color good, warn;
};

inline Theme darkTheme() {
    Theme t;
    t.window     = Color::fromHTML("#070A10");
    t.windowGlow = Color::fromHTML("#12234A");
    t.plate      = Color::fromHTML("#0F1218");
    t.panel      = Color::fromHTML("#171A21");
    t.panelEdge  = Color::fromHTML("#232833");
    t.field      = Color::fromHTML("#1F232C");
    t.fieldHi    = Color::fromHTML("#2A2F3A");
    t.track      = Color::fromHTML("#2B303B");
    t.trackDim   = Color::fromHTML("#1E222B");
    t.text       = Color::fromHTML("#F1F4FA");
    t.textDim    = Color::fromHTML("#8E97AA");
    t.textFaint  = Color::fromHTML("#5C6577");
    t.accent     = Color::fromHTML("#3B82F6");
    t.accentHi   = Color::fromHTML("#6AA3FF");
    t.accentSoft = Color::fromHTML("#16233F");
    t.onAccent   = Color::fromHTML("#FFFFFF");
    t.shadow     = Color::fromHTML("#000000");
    t.good       = Color::fromHTML("#34D399");
    t.warn       = Color::fromHTML("#F59E0B");
    return t;
}

inline Theme lightTheme() {
    Theme t;
    t.window     = Color::fromHTML("#DDE3EE");
    t.windowGlow = Color::fromHTML("#C9D8F5");
    t.plate      = Color::fromHTML("#F3F5F9");
    t.panel      = Color::fromHTML("#FFFFFF");
    t.panelEdge  = Color::fromHTML("#DCE1EA");
    t.field      = Color::fromHTML("#EDF0F5");
    t.fieldHi    = Color::fromHTML("#E1E6EE");
    t.track      = Color::fromHTML("#D3D9E3");
    t.trackDim   = Color::fromHTML("#E4E8EF");
    t.text       = Color::fromHTML("#141A26");
    t.textDim    = Color::fromHTML("#5A6579");
    t.textFaint  = Color::fromHTML("#8D97AA");
    t.accent     = Color::fromHTML("#2563EB");
    t.accentHi   = Color::fromHTML("#3B82F6");
    t.accentSoft = Color::fromHTML("#DCE8FF");
    t.onAccent   = Color::fromHTML("#FFFFFF");
    t.shadow     = Color::fromHTML("#44506A");
    t.good       = Color::fromHTML("#059669");
    t.warn       = Color::fromHTML("#D97706");
    return t;
}

} // namespace lstrip::ui
