================================
Vendored code and licensing
================================

What is vendored
================

``vendor/dpf`` (git submodule)
    DPF, the DISTRHO Plugin Framework, from
    https://github.com/DISTRHO/DPF.  It carries its own submodule, pugl
    (windowing).  The pinned revision is recorded by the superproject:
    ``git submodule status``.

``assets/fonts``
    Inter Regular and Inter SemiBold (SIL Open Font License 1.1), with the
    licence text.  The fonts are embedded in the plugin binaries at build
    time; they are not loaded from disk.

Nothing else is bundled.  The DSP, the editor widgets, the JSON reader and
the preset store are written for this project.

Why a submodule
===============

DPF is the only part of the build that changes on its own schedule, so it is
pinned and updated deliberately.  A submodule keeps the tree small, keeps the
upstream history and licence files intact, and makes an audit simple: every
file under ``vendor/dpf`` is exactly what upstream published at the pinned
commit.  Never edit files there; if DPF needs a change, send it upstream.

After updating DPF
==================

1. Rebuild everything and run ``ctest`` (the VST2 harness and the DSP suite).
2. Run ``clap-validator`` and compare with the findings in ``testing.rst``.
3. Open the editor in OBS (see ``obs.rst``): confirm it still renders, that
   ``LSTRIP_DEBUG=1`` reports ``direct DSP access: yes`` and that meters move.
4. Check ``vendor/dpf/LICENSING.md`` for changes and update
   ``THIRD-PARTY.rst`` at the repository root.

Licensing of the plugin formats
===============================

DPF does not use the Steinberg VST2 or VST3 SDKs.

VST2
    DPF uses a clean-room, liberally licensed description of the VST2
    binary interface (``distrho/src/xaymar-vst2``, BSD-style).  No Steinberg
    material and no GPL "vestige" header is involved.
VST3
    DPF implements the interface itself (``distrho/src/travesty``, ISC); the
    official VST3 SDK is not used.
LV2, CLAP
    Their headers are ISC and MIT licensed respectively.

Therefore the whole project, source and binaries, can be distributed under
the MIT licence in ``LICENSE``, provided the attributions in
``THIRD-PARTY.rst`` are kept.  The VST trademark belongs to Steinberg; the
documentation says "VST2" and "VST3" only to name the formats.

DPF's own rule is that DPF itself must be mentioned in attribution
(``vendor/dpf/LICENSING.md``).  That is done in ``THIRD-PARTY.rst`` and in
the plugin's "About" metadata.
