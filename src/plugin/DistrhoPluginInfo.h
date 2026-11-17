// SPDX-License-Identifier: MIT
#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND     "L-STRIP"
#define DISTRHO_PLUGIN_NAME      "L-STRIP"
#define DISTRHO_PLUGIN_URI       "https://github.com/niizam/L-STRIP"
#define DISTRHO_PLUGIN_CLAP_ID   "io.github.niizam.l-strip"
#define DISTRHO_PLUGIN_BRAND_ID  LStr
#define DISTRHO_PLUGIN_UNIQUE_ID LStp

#define DISTRHO_PLUGIN_CLAP_FEATURES "audio-effect", "compressor", "equalizer", "reverb", "delay", "chorus", "limiter"
#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Dynamics"
#define DISTRHO_PLUGIN_LV2_CATEGORY    "lv2:DynamicsPlugin"

#define DISTRHO_PLUGIN_HAS_UI            1
#define DISTRHO_PLUGIN_HAS_EMBED_UI      1
#define DISTRHO_PLUGIN_IS_RT_SAFE        1
#define DISTRHO_PLUGIN_NUM_INPUTS        2
#define DISTRHO_PLUGIN_NUM_OUTPUTS       2
#define DISTRHO_PLUGIN_WANT_LATENCY      1
#define DISTRHO_PLUGIN_WANT_PROGRAMS     1
#define DISTRHO_PLUGIN_WANT_STATE        1
#define DISTRHO_PLUGIN_WANT_TIMEPOS      1
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1

#define DISTRHO_UI_USE_NANOVG            1
#define DISTRHO_UI_USER_RESIZABLE        1
#define DISTRHO_UI_FILE_BROWSER          1
#define DISTRHO_UI_DEFAULT_WIDTH         940
#define DISTRHO_UI_DEFAULT_HEIGHT        640

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
