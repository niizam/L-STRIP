=====================================
L-STRIP third-party notices
=====================================

L-STRIP itself is MIT licensed (see ``COPYING`` and ``LICENSE``).  The
binaries include or link the following components, all under permissive
licences.  Their full licence texts are in the named files of the DPF
submodule (``vendor/dpf``) or in ``assets/fonts``.

========================== ========= ====================================
Component                  Licence   Copyright and where
========================== ========= ====================================
DPF (DISTRHO Plugin        ISC       2012-2025 Filipe Coelho and
Framework), including DGL            contributors; ``vendor/dpf/LICENSE``
pugl (windowing)           ISC       2011-2022 David Robillard;
                                     ``vendor/dpf/dgl/src/pugl-upstream``
libSOFD (file dialog)      MIT       2014 Robin Gareus;
                                     ``vendor/dpf/distrho/extra/sofd``
VST2 interface description MIT/BSD   2020 Michael Fabian Dirks;
                                     ``vendor/dpf/distrho/src/xaymar-vst2``
VST3 interface (travesty)  ISC       DPF authors;
                                     ``vendor/dpf/distrho/src/travesty``
CLAP headers               MIT       2014-2022 Alexandre Bique;
                                     ``vendor/dpf/distrho/src/clap``
LV2 headers                ISC       2006-2012 Steve Harris, David
                                     Robillard, based on LADSPA
                                     (2000-2002 Richard Furse and
                                     others); ``vendor/dpf/distrho/src/lv2``
RtAudio (standalone        MIT       2001-2021 Gary P. Scavone;
application only)                    ``vendor/dpf/distrho/src/jackbridge/
                                     rtaudio``
Inter typeface (Regular    SIL OFL   2016 The Inter Project Authors;
and SemiBold), embedded    1.1       ``assets/fonts/OFL-Inter.txt``
========================== ========= ====================================

The editor is drawn with two system libraries that are *linked dynamically*
and not bundled: Cairo (LGPL 2.1 or MPL 1.1) and FreeType (FreeType License
or GPLv2).  Their licences apply to those libraries; this project's code is
not derived from them.
Installed packages include this file, ``COPYING`` and the font licence under
``share/doc/l-strip``.

"VST" is a trademark of Steinberg Media Technologies GmbH.  Other names are
used only to identify file formats.
