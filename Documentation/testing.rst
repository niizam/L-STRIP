=========
Testing
=========

Run everything
==============

::

    cmake -S . -B build && cmake --build build -j"$(nproc)"
    ctest --test-dir build --output-on-failure

``ctest`` runs four suites:

``dsp``
    ``tests/dsp_tests.cpp``: about 3300 checks that need no plugin
    framework (listed below).
``vst2-host``, ``vst2-host-mono``
    ``tests/vst2_host_test.cpp``: a minimal VST2 host that ``dlopen``\ s the
    built ``.so`` and drives it the way a host does.
``params-doc``
    Fails when ``Documentation/parameters.rst`` no longer matches the
    parameter table.  Regenerate with ``cmake --build build --target
    params-doc``.

What the DSP suite covers
=========================

* The parameter table: unique symbols, defaults in range, outputs last, and
  the scale masks.
* Bit-exact passthrough with everything off; latency 0 and 10 ms.
* Independence from the host's block size (37, 512, 4096 samples).
* Gate, compressor, de-esser (including LISTEN), limiter and saturation,
  the gate staying effective below 100 % Intensity, EQ response against the
  biquad maths, multiband flatness and upward lift, reverb / delay / chorus
  tails.
* Pitch correction accuracy: a synthetic voice 30 cents sharp, 35 cents
  flat, 1.4 semitones off in a scale, and across the male and female
  range, must land within 8 cents of the target note; noise must stay
  unvoiced; Humanize must preserve vibrato.
* Dry / wet time alignment with Tune on at Intensity below 100 %.
* 300 rounds of randomised parameter automation, including +24 dB input,
  must stay finite and below the limiter ceiling.
* A thread that rewrites every parameter while audio runs, and a check that
  the latest edit always wins.
* NaN, infinity and 1e30 on one channel only: the output stays finite and the
  chain recovers to match a fresh instance.
* The preset store and JSON reader, including hostile files (huge values,
  wrong types, deep nesting, garbage).

What the host harness covers
============================

Entry point and magic number; input and output counts (and, for the mono
build, that a null second channel is never touched); round-trip of every
parameter through the host's normalised range; every factory preset
rendered through the plugin and compared with the reference ``Chain``;
reported latency following Tune; a state chunk saved from one instance and
restored into another; independence of two instances.

Sanitizers
==========

::

    g++ -std=c++17 -O1 -g -fsanitize=address,undefined \
        tests/dsp_tests.cpp -o /tmp/dsp_asan -pthread && /tmp/dsp_asan
    g++ -std=c++17 -O1 -g -fsanitize=thread \
        tests/dsp_tests.cpp -o /tmp/dsp_tsan -pthread && /tmp/dsp_tsan

Both are clean.

Plugin validators
=================

``clap-validator``::

    clap-validator validate build/bin/L-STRIP.clap

For both the stereo and the mono build, 29 tests pass, 11 are skipped by the
validator (they test features L-STRIP does not have, such as note ports) and
three report findings that come from DPF's CLAP wrapper rather than from the
plugin:

* ``param-set-events`` and ``param-set-no-cookies`` compare *all* parameter
  values after a flush and after a process call.  Meters legitimately differ
  between the two (they only move when audio is processed).
* ``param-set-wrong-namespace``: the wrapper accepts parameter events with a
  foreign namespace id.

One warning (``state-invalid-random``) says the wrapper accepted random bytes
as a state; it does not crash.  Latency changes were a fourth finding in
earlier builds; it is gone because the CLAP build no longer reports the
Tune latency (see ``design.rst``).

``lv2lint``
-----------

Run it against an *installed* tree so the plugin is found next to the system
LV2 ontologies::

    DESTDIR=/tmp/stage cmake --install build --prefix /usr
    LV2_PATH=/tmp/stage/usr/lib/lv2:/usr/lib/lv2 lv2lint \
        https://github.com/niizam/L-STRIP

The plugin is typed ``lv2:DynamicsPlugin`` and its ports, presets and UI
descriptor load.  The remaining findings come from how DPF builds a
monolithic LV2 bundle and are not about L-STRIP itself: the bundle exports
the build-time symbol ``lv2_generate_ttl``; DPF's KXStudio *programs*
extension is unknown to the linter; ``instance-access``, ``data-access`` and
"DSP and UI in one binary" are flagged as discouraged (they are what the
direct DSP-to-editor channel needs, see ``design.rst``); and there is no
author e-mail in the metadata.

Manual tests in a real host
===========================

The editor and the audio path were also verified in OBS Studio:

1. Seed an OBS scene whose global audio source is a media source playing a
   generated vocal recording, with the VST filter pointing at the plugin and
   ``open_when_active_vst_settings`` set.
2. Run OBS under ``Xvfb`` with ``LSTRIP_DEBUG=1``; the log shows the editor
   bound to the engine and the meters moving.
3. Raise the editor window and take screenshots; two frames a second apart
   differ.

What is not covered
===================

* Hosts other than OBS and the test harness: Carla, REAPER, Ardour, Bitwig
  and so on were not run (Carla's own bridge aborted with an X error in the
  virtual X server used here, before reaching the plugin).
* Audio quality judged by ears.  The numeric tests prove accuracy and
  stability, not taste.
* The Import file dialog is DPF's; it was not observed in the headless
  X server used for testing (the call succeeds, but no window was captured).
  Export and the preset folder were verified.
* VST3 and LV2 editors were not opened in a host.  They share the editor
  code that was exercised in OBS (VST2) and in the standalone application.
