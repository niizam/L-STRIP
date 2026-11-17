=============================
Building and installing
=============================

Get the source
==============

DPF is a git submodule, and DPF has a submodule of its own (pugl), so clone
recursively::

    git clone --recursive https://github.com/niizam/L-STRIP.git
    cd L-STRIP

If you already cloned without ``--recursive``::

    git submodule update --init --recursive

Dependencies
============

A C++17 compiler, CMake 3.15 or newer, pkg-config, and the X11 and OpenGL
development files.  The standalone application additionally uses whichever
of ALSA, PulseAudio and JACK development files are installed.

Arch Linux::

    pacman -S --needed base-devel cmake pkgconf libx11 libxext libxrandr \
        libxcursor mesa alsa-lib libpulse jack2

Debian / Ubuntu::

    apt install build-essential cmake pkg-config libx11-dev libxext-dev \
        libxrandr-dev libxcursor-dev libgl-dev libasound2-dev \
        libpulse-dev libjack-jackd2-dev

Fedora::

    dnf install gcc-c++ cmake pkgconf-pkg-config libX11-devel \
        libXext-devel libXrandr-devel libXcursor-devel mesa-libGL-devel \
        alsa-lib-devel pulseaudio-libs-devel jack-audio-connection-kit-devel

Build
=====

::

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j"$(nproc)"
    ctest --test-dir build --output-on-failure

The plugins end up in ``build/bin``:

============================== =========================================
``L-STRIP-vst2.so``            VST2, stereo
``L-STRIP-Mono-vst2.so``       VST2, mono (for OBS microphones)
``L-STRIP.vst3``               VST3 bundle, stereo
``L-STRIP-Mono.vst3``          VST3 bundle, mono
``L-STRIP.lv2``                LV2 bundle, stereo
``L-STRIP-Mono.lv2``           LV2 bundle, mono
``L-STRIP.clap``               CLAP, stereo
``L-STRIP-Mono.clap``          CLAP, mono
``L-STRIP``                    Standalone JACK application
============================== =========================================

Useful options:

``-DLSTRIP_BUILD_PLUGINS=OFF``
    Build only the headless DSP tests (no DPF, no X11 needed).
``-DLSTRIP_BUILD_TESTS=OFF``
    Skip the tests.

Install
=======

For one user, without root::

    tools/install-user.sh

This copies the plugins to ``~/.vst`` (named ``L-STRIP.so`` and
``L-STRIP-Mono.so``, which is how OBS lists them), ``~/.vst3``, ``~/.lv2``
and ``~/.clap``, and the application to ``~/.local/bin``.  Remove them with
``tools/install-user.sh --uninstall``.

For packagers::

    cmake --install build --prefix /usr

installs to ``lib/vst``, ``lib/vst3``, ``lib/lv2``, ``lib/clap`` and
``bin``, plus the licence and third-party notices under
``share/doc/l-strip``.  The embedded font's licence (SIL OFL) is installed
there too, as its terms require.

Updating the vendored framework
===============================

::

    git -C vendor/dpf fetch
    git -C vendor/dpf checkout <tag-or-commit>
    git add vendor/dpf
    ctest --test-dir build

See ``vendor.rst`` for what to re-check after an update.

Troubleshooting
===============

``vendor/dpf is missing``
    You cloned without ``--recursive``; see above.
Editor does not open
    The editor needs an X11 window; see ``obs.rst`` for Wayland.
Plugin not found by the host
    Check the search paths of your host; ``tools/install-user.sh`` uses the
    standard per-user locations.
