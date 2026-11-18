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
    ``tests/dsp_tests.cpp``: about 4400 checks that need no plugin
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

* The parameter table: unique symbols, defaults in range, the outputs
  forming one block flagged ``fOutput``, the host-index mapping round
  trip, and the scale masks.
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
* The controls added in 1.1, one by one.  Each is an exact bypass at its
  default (plosive tamer and auto level bit-exact at 0 %).  Transpose lands
  within 10 cents of the target from -12 to +12 semitones on two voices and
  keeps the level; unvoiced noise follows the shift and, when not
  transposing, still passes straight through; the latency stays 10 ms.
  Correction at 100 % pulls a note 35 cents flat to the target and at 0 %
  leaves it alone.  The plosive tamer must remove at least 5-8 dB from a
  pop's low band, leave the voice after it untouched, leave sung notes from
  70 to 160 Hz within 0.5 dB and not click when switched off.  Auto level
  must narrow a 25 dB loudness gap, hold its gain through a pause (the
  case that makes naive riders pump) and glide, not jump, when switched
  off.  Delay feedback and tone, reverb pre-delay and cuts, and the EQ
  high-cut are measured against their intended response.
* Every factory preset (the 1.1 ones included) renders finite, under the
  limiter ceiling and at a sane level.

What the host harness covers
============================

Entry point and magic number; input and output counts (and, for the mono
build, that a null second channel is never touched); that every input
parameter (and no meter) is visible to the host, with the v1.0 parameters
at the index they always had; round-trip of every parameter through the
host's normalised range; every factory preset rendered through the plugin
and compared with the reference ``Chain``; reported latency following Tune;
a state chunk saved from one instance and restored into another; a
hand-built chunk in the format a v1.0 plugin wrote (none of the 1.1
parameters in it), which must restore the old values and leave the new
controls at their defaults; independence of two instances.

Backward compatibility with an earlier release
==============================================

Parameter ids are permanent, and every parameter added later defaults to "no
effect".  ``tools/compat-check.sh`` proves the second half::

    tools/compat-check.sh            # against the newest v* tag
    tools/compat-check.sh v1.0       # or any revision

It compiles ``tools/compat_render.cpp`` twice, against the old revision's
sources (taken with ``git archive``, so the repository is not touched) and
against the working tree, renders a fixed synthetic voice through every
factory preset and through 24 seeded random settings of the original
parameters (at 48 and 44.1 kHz, with parameter changes while audio runs),
and requires every render the old build made to be reproduced bit for bit.
It is not part of ``ctest`` (it needs git history and a second compiler
run), but CMake builds the renderer so that it cannot stop compiling.  Run it
before every release.  Checked against v1.0 for 1.1: 92 of 92 identical.

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

Regression: OBS on NVIDIA
-------------------------

A user report showed OBS crashing in its render thread when the editor was
opened (NVIDIA RTX 3080 Ti, proprietary driver 615, X11).  It was reproduced
with an isolated OBS profile that has a screen-capture source plus the plugin
with ``open_when_active_vst_settings``: OBS segfaulted in ``xshm_video_tick``.
Controls: the same scene without the plugin ran cleanly; with the editor worker
thread disabled it still crashed; a DPF example plugin with a Cairo editor ran
cleanly.  After the editor moved to Cairo the scene ran for the full test
duration with live meters and no core dump.

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
