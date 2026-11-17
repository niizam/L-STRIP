// SPDX-License-Identifier: MIT
// Mono variant: 1 in / 1 out.  Meant for hosts that feed single-channel sources
// (OBS Studio microphones); see Documentation/obs.rst.
#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define LSTRIP_MONO 1
#define DISTRHO_PLUGIN_NAME      "L-STRIP Mono"
#define DISTRHO_PLUGIN_URI       "https://github.com/niizam/L-STRIP#mono"
#define DISTRHO_PLUGIN_CLAP_ID   "io.github.niizam.l-strip.mono"
#define DISTRHO_PLUGIN_UNIQUE_ID LStm
#define DISTRHO_PLUGIN_NUM_INPUTS  1
#define DISTRHO_PLUGIN_NUM_OUTPUTS 1

#include "../plugin/CommonInfo.h"

#endif
