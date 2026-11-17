#!/bin/sh
# SPDX-License-Identifier: MIT
# Install (or --uninstall) L-STRIP for the current user, without root.
#   tools/install-user.sh [--uninstall] [BUILD_DIR]
# VST2 plugins are installed as L-STRIP.so and L-STRIP-Mono.so because OBS Studio
# lists VST2 plugins by file name.
set -eu

uninstall=0
if [ "${1:-}" = "--uninstall" ]; then uninstall=1; shift; fi
here=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-$here/build}
[ -d "$build/bin" ] || build=$here      # inside an unpacked release tarball
bin=$build/bin

vst2=${HOME}/.vst
vst3=${HOME}/.vst3
lv2=${HOME}/.lv2
clap=${HOME}/.clap
app=${HOME}/.local/bin
doc=${XDG_DATA_HOME:-$HOME/.local/share}/doc/l-strip

if [ "$uninstall" -eq 1 ]; then
    rm -fv "$vst2/L-STRIP.so" "$vst2/L-STRIP-Mono.so" "$clap/L-STRIP.clap" "$clap/L-STRIP-Mono.clap" "$app/L-STRIP"
    rm -rfv "$vst3/L-STRIP.vst3" "$vst3/L-STRIP-Mono.vst3" "$lv2/L-STRIP.lv2" "$lv2/L-STRIP-Mono.lv2" "$doc"
    exit 0
fi

for f in L-STRIP-vst2.so L-STRIP-Mono-vst2.so L-STRIP.clap; do
    [ -e "$bin/$f" ] || { echo "error: $bin/$f not found; build first (see Documentation/building.rst)" >&2; exit 1; }
done

mkdir -p "$vst2" "$vst3" "$lv2" "$clap" "$app" "$doc"
install -m 755 "$bin/L-STRIP-vst2.so"      "$vst2/L-STRIP.so"
install -m 755 "$bin/L-STRIP-Mono-vst2.so" "$vst2/L-STRIP-Mono.so"
install -m 755 "$bin/L-STRIP.clap"         "$clap/L-STRIP.clap"
install -m 755 "$bin/L-STRIP-Mono.clap"    "$clap/L-STRIP-Mono.clap"
for b in L-STRIP.vst3 L-STRIP-Mono.vst3; do rm -rf "$vst3/$b"; cp -r "$bin/$b" "$vst3/$b"; done
for b in L-STRIP.lv2 L-STRIP-Mono.lv2;   do rm -rf "$lv2/$b";  cp -r "$bin/$b" "$lv2/$b";  done
[ -e "$bin/L-STRIP" ] && install -m 755 "$bin/L-STRIP" "$app/L-STRIP"
# the embedded font's licence (SIL OFL) and the notices travel with the binaries
install -m 644 "$here/COPYING" "$here/LICENSE" "$here/THIRD-PARTY.rst" "$here/assets/fonts/OFL-Inter.txt" "$doc/"

echo "Installed:"
echo "  VST2  $vst2/L-STRIP.so, L-STRIP-Mono.so"
echo "  VST3  $vst3/L-STRIP.vst3, L-STRIP-Mono.vst3"
echo "  LV2   $lv2/L-STRIP.lv2, L-STRIP-Mono.lv2"
echo "  CLAP  $clap/L-STRIP.clap, L-STRIP-Mono.clap"
echo "  app   $app/L-STRIP"
