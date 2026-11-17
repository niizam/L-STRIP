#!/bin/sh
# SPDX-License-Identifier: MIT
# Package a finished build into a release tarball.
#   tools/package-release.sh VERSION [BUILD_DIR] [OUT_DIR]
# The tarball unpacks to L-STRIP-VERSION-linux-x86_64/ and can be installed with
# its own tools/install-user.sh.
set -eu
ver=${1:?usage: package-release.sh VERSION [BUILD_DIR] [OUT_DIR]}
here=$(cd "$(dirname "$0")/.." && pwd)
build=${2:-$here/build}
out=${3:-$here/dist}
name=L-STRIP-$ver-linux-x86_64
stage=$(mktemp -d)/$name

mkdir -p "$stage/bin" "$stage/tools" "$stage/assets/fonts" "$out"
for f in L-STRIP-vst2.so L-STRIP-Mono-vst2.so L-STRIP.clap L-STRIP-Mono.clap L-STRIP; do
    cp "$build/bin/$f" "$stage/bin/"
done
for d in L-STRIP.vst3 L-STRIP-Mono.vst3 L-STRIP.lv2 L-STRIP-Mono.lv2; do
    cp -r "$build/bin/$d" "$stage/bin/"
done
cp "$here/tools/install-user.sh" "$stage/tools/"
cp "$here/COPYING" "$here/LICENSE" "$here/THIRD-PARTY.rst" "$here/README.rst" "$stage/"
cp "$here/assets/fonts/OFL-Inter.txt" "$stage/assets/fonts/"
cp -r "$here/Documentation" "$stage/Documentation"
strip --strip-unneeded "$stage"/bin/*.so "$stage"/bin/*.clap "$stage/bin/L-STRIP" 2>/dev/null || true
find "$stage/bin" -name "*.so" -exec strip --strip-unneeded {} + 2>/dev/null || true
tar -C "$(dirname "$stage")" -czf "$out/$name.tar.gz" "$name"
(cd "$out" && sha256sum "$name.tar.gz" > "$name.tar.gz.sha256")
echo "$out/$name.tar.gz"
