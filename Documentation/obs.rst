=====================================
Using L-STRIP in OBS Studio (Linux)
=====================================

Which variant to use
====================

OBS runs VST 2.x plugins through its "VST 2.x Plug-in" audio filter.  Install
the plugins with ``tools/install-user.sh``; OBS scans ``~/.vst`` and lists
the files by name (set ``VST_PATH`` to search somewhere else):

``L-STRIP Mono``
    Use this for a **microphone**.  Microphones are single-channel sources.
``L-STRIP``
    Stereo.  Use it for stereo sources (a stereo interface channel, a
    music player).

Why the difference matters.  OBS decides how many channels to hand a plugin
from the plugin's own input and output count.  For a source that has fewer
channels, it passes a freshly allocated buffer that is **never initialised**
as the missing channel.  A stereo plugin that links its channels (as any
compressor, gate or reverb does) would be driven by that garbage when it is
put on a mono microphone.  The mono variant never reads a second buffer.

If the stereo build is used on a mono microphone by mistake it will not
crash and will not output NaN: every input sample is checked and non-finite
values are replaced by silence.  But finite garbage on the second channel can
still sway the linked detectors, so use *L-STRIP Mono* for microphones.

Adding the filter
=================

1. In the Audio Mixer, open the gear (or the source's menu) next to your
   microphone and choose *Filters*.
2. Press ``+`` and add *VST 2.x Plug-in*.
3. Choose *L-STRIP Mono* in the list, then press *Open Plug-in Interface*.
   Tick *Open interface when active* to have the editor open automatically.
4. Pick a preset (Presets button), for example *Podcast Clean* or *Stream
   Bright*, and adjust.  The orb is the master intensity.

The plugin's settings are saved with the OBS scene collection.

Things worth knowing
====================

Latency
    Everything is zero latency.  Switching on pitch correction adds about
    10 ms.  OBS ignores the latency a plugin reports, so nothing is
    compensated; 10 ms is not audible on a live voice, but do not use Tune
    on a source that has to stay frame-aligned with something else.
Channel layout
    OBS processes the first two channels of a source.  Surround sources are
    not supported.
Wayland
    The editor is an X11 window embedded by OBS.  If OBS runs as a native
    Wayland client, VST editors may not work at all.  Start OBS through
    XWayland::

        QT_QPA_PLATFORM=xcb obs

Flatpak
    A sandboxed OBS only sees plugins in its own extension directory.  This
    configuration has not been tested.
Tempo-synced delay
    OBS reports a fixed tempo of 120 BPM, so delay times are fixed to that.

Why the editor behaves the way it does
======================================

OBS never gives plugin editors an idle callback on Linux (the VST protocol
expects the host to do so about 30 times a second).  Most plugin
frameworks therefore show a frozen or blank window in OBS.  L-STRIP detects
OBS and drives its own editor from a small worker thread, and reads its
meters straight from the audio engine instead of waiting for the host.  The
details are in ``design.rst``.  Environment variables for debugging:

``LSTRIP_SELF_IDLE=1``
    Force the worker thread on in any host.
``LSTRIP_NO_SELF_IDLE=1``
    Never start the worker thread.
``LSTRIP_DEBUG=1``
    Print a one-line status and the input / output meter every two seconds
    on stderr.

How this was tested
===================

The plugin was loaded in OBS Studio 33 under a virtual X server with a
generated vocal recording playing through a media source: the editor opened,
rendered and animated, and the meters, band meters, ducking meters and the
orb followed the audio.  Test procedure details are in ``testing.rst``.
