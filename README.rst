=======
L-STRIP
=======

Open-source VST3 + VST2 plugin for Linux.
Compatible with OBS Studio and any other VST host.

L-STRIP is a one-window vocal chain: noise gate, real-time pitch
correction, compressor, de-esser, equalizer, multiband (OTT-style)
compression, space (reverb, tempo-synced delay and chorus with ducking) and
an output saturator with a safety limiter.  One intensity knob sets how much
of the chain you hear.  Everything is zero latency, except pitch correction,
which adds about 10 ms while it is on.

It ships as VST2, VST3, LV2 and CLAP plugins (each in a stereo and a mono
variant) and as a standalone JACK application.

Quick Start
-----------

::

    git clone --recursive https://github.com/niizam/L-STRIP.git
    cd L-STRIP
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j"$(nproc)"
    ctest --test-dir build
    tools/install-user.sh

Then, in OBS Studio: open the *Filters* of your microphone, add *VST 2.x
Plug-in*, choose **L-STRIP Mono**, press *Open Plug-in Interface*, and pick
a preset.  Use the stereo **L-STRIP** for stereo sources.  See
``Documentation/obs.rst`` for the reasons and for Wayland notes.

Documentation
-------------

* Documentation/README.rst: index of all documents
* Documentation/building.rst: dependencies, build, install, update
* Documentation/obs.rst: using it in OBS Studio
* Documentation/presets.rst: presets and their file format
* Documentation/parameters.rst: every parameter
* Documentation/design.rst, dsp.rst, ui.rst: how it works
* Documentation/testing.rst: tests, sanitizers and validators
* Documentation/vendor.rst: bundled framework and licensing

The documentation is plain reStructuredText, readable as plain text.

License
-------

* License: See COPYING
* Third-party components: See THIRD-PARTY.rst
