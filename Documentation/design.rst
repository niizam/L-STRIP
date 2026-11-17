=========================
Design and architecture
=========================

Goals
=====

* A single plugin that replaces a whole vocal chain: gate, pitch
  correction, compressor, de-esser, EQ, multiband, space (reverb, delay,
  chorus) and an output saturator plus limiter.
* Zero added latency, except while pitch correction is on.
* Runs in OBS Studio on Linux, which hosts VST 2.x plugins only and has an
  unusual editor protocol (see ``obs.rst``).
* Permissively licensed (MIT), with no proprietary SDK anywhere.

Framework choice
================

L-STRIP is built on DPF, the DISTRHO Plugin Framework, vendored as a git
submodule in ``vendor/dpf`` (see ``vendor.rst``).  One code base produces
VST2, VST3, LV2, CLAP and a standalone JACK application.

JUCE was rejected because it is GPLv3 or commercial, which does not mix with
the MIT licence of this project.  DPF is ISC licensed and ships clean-room
VST2 and VST3 headers, so no Steinberg SDK is involved.

Scope
=====

Not included yet: the simplified "ONE" variant of the reference product (a
single AMOUNT knob with a few faders).  It would be a second, smaller editor
over the same parameter table and DSP chain, so nothing here prevents it.

Source layout
=============

``src/common/``
    ``Params.hpp``
        The parameter table.  The single source of truth for the DSP, the
        editor, the presets and the host automation list.  Ids are
        permanent; new parameters are only appended.
    ``Presets.hpp``
        Factory presets, as overrides on top of the parameter defaults.
    ``PresetStore.hpp``, ``MiniJson.hpp``
        User presets and editor configuration on disk (std only).
    ``Shared.hpp``
        The DSP-to-editor side channel (below).

``src/dsp/``
    Header-only, framework-free DSP.  ``Chain.hpp`` assembles the stages.
    Nothing in here allocates, locks or logs after ``prepare()``.

``src/plugin/``, ``src/plugin-mono/``
    The DPF plugin class and the format descriptors for the stereo and mono
    variants (both compile the same ``LStripPlugin.cpp``).

``src/ui/``
    The editor.  ``UiBase`` (parameter cache, theme, text helpers),
    ``Controls.hpp`` (knobs, faders, ...), ``Overlays.hpp`` (orb, pitch
    panel, preset browser, prompt), ``LStripUI.cpp`` (layout and events),
    ``IdleDriver.hpp`` (see below).

``tests/``, ``tools/``
    Headless DSP tests, a VST2 host harness, and the parameter-doc generator.

Signal flow
===========

::

    in gain -> Gate -+-> Auto-tune -> Compressor -> De-esser -> EQ
                     |   -> Multiband -> Space --------------------+
                     |                                             |
                     +-> dry tap, delayed by the tune latency -> Intensity
                                                                   |
        raw input ---------- master off: crossfade ----------------+
                                                                   v
                                  out gain -> Out Sat -> Limiter -> out

The Intensity knob blends the processed signal with the *gated* dry tap, so
the gate keeps working at any Intensity.  The limiter is the last stage, so
nothing clips; the one exception is the master-off bypass, which is a raw
passthrough.  Every module can be switched off; switching is a 5 ms
crossfade, never a click.

Threading
=========

Three kinds of thread touch the plugin:

audio thread
    Calls ``run()``.  Reads parameters, runs the chain, publishes meters.

host / GUI thread
    Sets parameters, creates and destroys the editor, saves state.

editor worker thread
    Only exists in hosts that never idle the editor (OBS).  See below.

Parameter cells in ``Chain`` are relaxed atomics.  The set of modules that
need their coefficients recomputed is an atomic bit mask updated with
``fetch_or`` by writers and drained with ``exchange(0)`` by the audio thread,
so a change made between "read" and "clear" can never be lost.  The test
suite exercises this with a thread that rewrites every parameter while the
audio thread runs, and it is clean under ThreadSanitizer.

How the editor sees the DSP
===========================

DPF delivers output parameters (our meters) to a VST2 editor only from the
host's ``effEditIdle`` call.  OBS Studio never makes that call on Linux
(verified: after opening the editor it sends only the get-rect and open
opcodes), so a plain DPF editor would never repaint and never see a meter.

Two mechanisms fix this:

``SharedState`` / ``SharedRegistry`` (``src/common/Shared.hpp``)
    Each plugin instance owns a block of atomic floats holding every
    parameter value and meter.  DPF passes the editor a pointer to the DSP
    object (``DISTRHO_PLUGIN_WANT_DIRECT_ACCESS``); the plugin registers its
    state under that pointer and the editor looks it up.  No RTTI is used.
    If the lookup finds nothing (an editor running in another process), the
    editor falls back to DPF's normal ``parameterChanged`` updates.  For LV2
    this mode requires the editor to live inside the DSP shared object, so
    the LV2 target is built ``MONOLITHIC`` and the bundle needs a host that
    supports the ``instance-access`` feature (Ardour, Carla, jalv and others).

``IdleDriver`` (``src/ui/IdleDriver.hpp``)
    Pumps the DGL event loop from a worker thread when, and only when, the
    host is known not to idle the editor.  Xlib is not thread-safe, so the
    rules are conservative: the worker waits for DPF's single create-time
    idle; it starts immediately only when the host process is OBS (or
    ``LSTRIP_SELF_IDLE=1``); in any other host it waits three seconds and
    starts only if the host idled at most once; and it retires for good as
    soon as it sees the host idling.  A mutex serialises the worker with the
    host-thread callbacks.

The editor adopts a value from ``SharedState`` only when that value
*changed* since the last poll.  In VST3 and CLAP an edit made in the editor
reaches the DSP only after the host forwards it; adopting "any difference"
would flicker edits back.

Rendering: Cairo, not OpenGL
============================

The editor is software-rendered with Cairo (``src/ui/Gfx.hpp`` is a small
drawing layer with NanoVG-style calls on top of Cairo, plus FreeType for the
embedded Inter fonts).  The first version used DPF's OpenGL/NanoVG backend and
it looked identical, but it crashed OBS Studio: OBS renders through EGL, and
creating a GLX context for the editor inside the OBS process made OBS's render
thread fault (in the X11 screen-capture copy) on NVIDIA's proprietary driver.
The crash also happened with the editor worker thread disabled, and did not
happen with a Cairo editor.  A software-rendered editor shares no GL state
with the host, so it cannot interfere with the host's renderer.  The editor
repaints at about 30 frames per second.

Formats and latency
===================

All variants are available in a stereo (2 in, 2 out) and a mono (1 in,
1 out) build, except the JACK application, which is stereo only.

=========== ===========================================================
Format      Notes
=========== ===========================================================
VST2        Primary target (OBS).  One host program only (DPF limit):
            presets live in the editor.  Latency 0, or 10 ms reported
            while pitch correction is on.
VST3        Factory presets exposed as programs.  Latency reported
            dynamically by DPF.
LV2         Same as VST3.  Monolithic bundle; the editor needs a host
            that provides ``instance-access``.
CLAP        Tune latency is *not* reported: DPF's CLAP wrapper must ask
            the host to restart the plugin on every latency change, which
            would glitch a live session each time Tune is toggled.  The
            dry path stays aligned internally.
JACK        Standalone application, for testing.
=========== ===========================================================

The mono variants feed the input to both internal channels and fold the
result back to one.  They never read a second host buffer; this matters in
OBS (see ``obs.rst``).

Parameter model
===============

Parameters are plain floats in their natural units (dB, Hz, %).  Booleans,
integers and choices are floats with a ``kind``.  Frequencies and the
chorus rate are logarithmic.  Outputs (meters, pitch readout) have the
``fOutput`` flag, are hidden from hosts, and never saved.

Session state beyond parameters is one string, ``preset`` (the name shown in
the editor).  Theme and starred presets are per user, not per session, and
live in ``~/.config/L-STRIP/ui.json``.
