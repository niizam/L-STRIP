=========================
User interface (UI/UX)
=========================

Principles
==========

* One window, no menus.  Every module is a card with the same anatomy: power
  button, title, reset button, style carousel, then its controls.
* Direct manipulation.  Knobs, faders, curves and the intensity orb are all
  draggable; nothing needs a dialog.
* Always legible.  Values are printed in plain units; hover tips explain
  every control in one sentence.
* Never in the way of the voice.  The interface repaints at about 30 Hz but
  does no work on the audio thread.  Drawing is software (Cairo) so it never
  touches the host's graphics driver; see ``design.rst``.

The visual language (rounded dark cards, a single blue accent, spaced
capitals for labels, the glowing particle orb) and the arrangement of the
five modules follow the published screenshots of the Windows vocal-chain
plugin this project re-implements for Linux.  No artwork, fonts or code from
that plugin are included: every control is drawn by this project's own
code, and the Inter typeface (SIL OFL) is embedded in the binary.

Layout
======

The design space is 940 x 640 and is scaled uniformly (aspect-locked) to the
window; the minimum size is 60 percent.  From left to right:

Dynamics
    Style carousel, a live compression curve, Threshold / Makeup / Tone /
    Mix knobs, a Fast / Slow / Auto switch, then the De-esser: frequency
    knob, amount slider, range stepper and the LISTEN button.
Space
    Style carousel (Room / Hall / Plate), Reverb and Delay knobs, SEND and
    DUCK meters, ducking steppers, delay time, then the Chorus block.
Master
    The intensity orb (the ring is the control), the large percentage
    read-out, and four faders: Gate, In, Out, Out Sat, with level meters.
Multiband
    Style carousel, three live band meters, Depth and Time knobs, Upward /
    Down / Output knobs, and Low / Mid / High trim sliders.
Equalizer
    Style carousel, the curve with four draggable points, four faders and
    the Low-cut slider.

The header holds the TUNE button, the preset name with previous / next
arrows, the light/dark switch, Save and Presets.  The pitch-correction
panel opens from TUNE.  Below the master faders sit TIPS and MORE; MORE
opens the panel of extra controls described below and is lit while any of
them is away from its default.

Interaction model
=================

================ ==========================================================
Gesture          Effect
================ ==========================================================
Drag knob        Vertical drag; hold Shift for fine control
Mouse wheel      Moves the value; also scrolls the preset list
Double click     Resets that control to its default
Click +/-        Steps a value; carousel arrows wrap around
Drag a point     On the compressor curve: threshold; on the EQ curve: gain
Drag the orb     Anywhere on the ring or the orb sets the intensity
Esc              Closes an overlay
================ ==========================================================

Every drag is wrapped in the host's begin/end edit gestures so that DAW
automation recording and undo behave correctly.

Hover tips
==========

The TIPS button toggles short explanations that appear after the pointer
rests on a control for about half a second.  The text lives in the parameter
table (``src/common/Params.hpp``), so the editor, the host's parameter list
and ``parameters.rst`` always agree.

Pitch-correction panel
======================

* A power switch and the latency note.
* The current note and octave, the detected input pitch and the correction
  in cents.
* A scrolling pitch trace: the original voice (dotted) against the
  corrected voice (solid), with lines at the allowed notes.
* Twelve note buttons.  Click a note to allow or forbid it; the scale
  switches to *Custom*.  Changing Key while on a Custom scale transposes
  the notes with it.
* Key and Scale steppers, and the Speed, Humanize and Mix knobs.

The "More" panel
================

The controls added in 1.1 live in one panel, so the main window keeps the
layout it had:

* Voice: Auto Level and Plosive Tamer (both start at Off).
* Pitch shift: the PITCH ON switch, a Transpose stepper (-12 to +12
  semitones) and the Correction slider.  A hint appears when a shift is set
  while pitch correction is off.
* Tone: the EQ high-cut (the equalizer curve in the main window shows it).
* Delay: Feedback and Tone.
* Reverb: Pre-delay, Low-cut and High-cut.

Hover tips work inside the panel.  Esc, the close button or a click outside
the card closes it.

Presets in the editor
=====================

The preset name in the header shows an asterisk when anything has changed
since the preset was loaded.  The browser (Presets) has category chips,
star toggles, rename and delete for user presets, and Save current,
Import and Export buttons.  Saving opens a small prompt with a suggested
name.  Typing is optional: if a host swallows keyboard input the suggested
name can simply be accepted.  Export writes to ``<Documents>/L-STRIP``
without a dialog.  File formats and locations are in ``presets.rst``.

Theming
=======

Colours are tokens in ``src/ui/Theme.hpp``; light and dark mode swap the
whole set.  The choice is saved per user in ``~/.config/L-STRIP/ui.json``.

Known limitations
=================

* No keyboard focus navigation yet; the editor is pointer driven.
* The editor needs an X11 window.  On a Wayland session host applications
  must run through XWayland (see ``obs.rst``).
* The editor repaints at about 30 Hz; the pitch trace and meters are
  sampled at about 60 Hz.
