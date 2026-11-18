#!/bin/sh
# SPDX-License-Identifier: MIT
# Backward-compatibility check: does the working tree still sound exactly like
# an earlier release?
#
#   tools/compat-check.sh [git-revision]        (default: the newest v* tag)
#
# It renders a fixed signal through every factory preset and through seeded
# random settings of the original parameters, once with the old revision's
# sources and once with the working tree's, and requires every render the old
# build made to be reproduced bit for bit.  New parameters stay at their
# defaults, so this proves the defaults really are neutral.  See
# Documentation/testing.rst.
set -eu
cd "$(dirname "$0")/.."
rev=${1:-$(git describe --tags --abbrev=0 --match 'v*' 2>/dev/null || true)}
[ -n "$rev" ] || { echo "no v* tag found: pass a revision" >&2; exit 2; }
cxx=${CXX:-g++}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/old"
git archive "$rev" src | tar -x -C "$tmp/old"
"$cxx" -std=c++17 -O2 -I"$tmp/old/src" tools/compat_render.cpp -o "$tmp/render-old"
"$cxx" -std=c++17 -O2 -Isrc tools/compat_render.cpp -o "$tmp/render-new"
"$tmp/render-old" | sort > "$tmp/old.txt"
"$tmp/render-new" | sort > "$tmp/new.txt"
total=$(wc -l < "$tmp/old.txt")
missing=$(comm -23 "$tmp/old.txt" "$tmp/new.txt" | wc -l)
if [ "$missing" -ne 0 ]; then
    echo "FAIL: $missing of $total renders differ from $rev:" >&2
    comm -23 "$tmp/old.txt" "$tmp/new.txt" | head -10 >&2
    exit 1
fi
echo "OK: all $total renders are bit-identical to $rev"
