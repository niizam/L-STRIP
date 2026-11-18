==========
Presets
==========

Factory presets
===============

Factory presets are compiled in (``src/common/Presets.hpp``).  Each one is a
list of overrides applied on top of the parameter defaults, so adding a
parameter later never breaks an existing preset.

=============== ========= ==============================================
Name            Category  Character
=============== ========= ==============================================
Podcast Clean   Podcast   Natural, level and tidy; gate, gentle EQ
Warm Intimate   Podcast   Slower compression, warmer tone, slight sat
Dry & Tight     Podcast   No space effects, firm gate and compression
Stream Bright   Stream    Punchy compression, bright EQ, short plate
Radio Voice     Stream    Thick compression, broadcast EQ, OTT multiband
Singer Polish   Music     Slow compression, hall, light delay and chorus
Big Hall Vocal  Music     Large hall with ducked reverb and delay
Natural Tune    Tune      Pitch correction in C major, natural speed
Robot Voice     Tune      Hard pitch correction with delay and chorus
De-ess Only     Utility   Only the de-esser is active
Init            Utility   All parameter defaults
Chat Voice      Stream    Chat and gaming: auto level, plosive tamer, dry
Whisper         Podcast   Gate off, strong auto level, upward multiband
Telephone       Stream    Band-limited 300 Hz - 3.4 kHz, dense and gritty
Megaphone       Stream    Narrow and hard, OTT multiband, tight echo
Small Room      Music     Short reverb, darkened and without boom
Cavern          Music     Hall, long dark echoes with high feedback
Helium          Tune      Pitch up eight semitones, no correction
Giant           Tune      Pitch down eight semitones, thick low end
=============== ========= ==============================================

The last eight were added in 1.1.  They are appended after the earlier
presets, so program numbers that existed before did not move.  Helium and
Giant use Transpose with Correction at 0 %; the shifter moves the
formants with the pitch, which is what makes them sound like a different
(smaller or larger) voice.

Hosts that expose programs (VST3, LV2) list these.  DPF's VST2 wrapper
exposes only one program, so in VST2 hosts such as OBS the presets are
reached through the editor.

User presets
============

User presets are JSON files::

    $XDG_DATA_HOME/L-STRIP/presets/<name>.lstrip.json

``XDG_DATA_HOME`` defaults to ``~/.local/share``.  Example::

    {
      "format": "lstrip-preset",
      "version": 1,
      "name": "My Stream Voice",
      "category": "User",
      "params": {
        "master_on": 1,
        "intensity": 85,
        "comp_thresh": -20,
        "eq_air": 4.5
      }
    }

Values are stored by parameter *symbol* (see ``parameters.rst``).  When a
file is read:

* unknown symbols are ignored, so presets from newer versions still load;
* missing symbols take their default value, which is how presets saved by
  version 1.0 load in later versions (every parameter added since defaults
  to "no effect");
* every value is clamped to the parameter's range, and wrong types fall
  back to the default;
* files that are not valid JSON, are nested deeper than 32 levels, or are
  larger than 4 MiB are rejected.

Names are trimmed, limited to 48 characters, and must not equal a factory
preset name.  File names are derived from the preset name with unsafe
characters replaced.

Import and export
=================

*Import* opens a file chooser and copies a ``.lstrip.json`` file into the
user preset folder.  Name clashes are resolved by renaming the imported
preset.  *Export* does not ask for a path: it writes the current settings to
``<Documents>/L-STRIP/<name>.lstrip.json`` (the folder named by
``XDG_DOCUMENTS_DIR``, default ``~/Documents``) and shows the location in a
message.  Because the files are plain text they can be shared by mail or
kept in version control.

The import chooser is DPF's: on desktops that provide the
``xdg-desktop-portal`` service it is the native file chooser, otherwise a
small built-in X11 dialog.  DPF's built-in dialog cannot *save* files, which
is why export is not dialog based.  Importing never *requires* the dialog:
a preset file copied into ``$XDG_DATA_HOME/L-STRIP/presets`` shows up the
next time the preset browser is opened.

Editor configuration
====================

Theme and starred presets are saved in::

    $XDG_CONFIG_HOME/L-STRIP/ui.json     (default ~/.config/L-STRIP/ui.json)

Session state
=============

Host sessions store all parameters plus the name of the current preset.
Closing and reopening a project restores both.
