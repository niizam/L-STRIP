==========================
L-STRIP documentation
==========================

L-STRIP is an open-source vocal chain plugin for Linux.  These documents are
plain reStructuredText that also reads well as plain text, so they can be
quoted in mailing-list mail.  Check them with::

    rst2html --halt=warning Documentation/<file>.rst > /dev/null

For people who want to use it
-----------------------------

``building.rst``
    Build, install and update.  Dependencies per distribution.

``obs.rst``
    Using L-STRIP in OBS Studio on Linux: which variant to pick, how to
    open the editor, and why some things work the way they do.

``presets.rst``
    Factory presets, user presets, the file format, import and export.

``parameters.rst``
    Every parameter: id, symbol, range, default and help text.  Generated.

For people who want to hack on it
---------------------------------

``design.rst``
    Architecture: formats, threading, how the editor talks to the DSP.

``dsp.rst``
    Every processing stage, the algorithms and numbers, the latency table,
    and the known limits.

``ui.rst``
    Editor layout, interaction model, theming, tips and overlays.

``testing.rst``
    What the tests cover, how to run the sanitizers and plugin validators,
    and the known validator findings.

``vendor.rst``
    The bundled DPF submodule, its licensing, and the third-party notices.
