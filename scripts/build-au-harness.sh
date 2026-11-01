#!/usr/bin/env bash
# Build the AudioToolbox host harnesses (tools/au-harness) as universal binaries: the
# effect's (DungeonCrawlerAUHarness, aufx/Dgcr) and the instrument's
# (DungeonCrawlerInstrumentAUHarness, aumu/Dgci, next to it).
# Usage: scripts/build-au-harness.sh [output-path of the effect's harness]
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="${1:-$here/build/DungeonCrawlerAUHarness}"
mkdir -p "$(dirname "$out")"
for name in DungeonCrawlerAUHarness DungeonCrawlerInstrumentAUHarness; do
    target="$(dirname "$out")/$name"
    [[ $name == DungeonCrawlerAUHarness ]] && target="$out"
    /usr/bin/xcrun clang++ -std=c++20 -fobjc-arc -O1 -g -Wall -Wextra -Wno-unused-parameter \
        -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 \
        "$here/tools/au-harness/$name.mm" -o "$target" \
        -framework AppKit -framework AudioToolbox -framework AudioUnit -framework AVFoundation \
        -framework CoreAudio -framework CoreAudioKit -framework Foundation
    /usr/bin/codesign --force --sign - "$target"
    printf 'Built %s (%s)\n' "$target" "$(/usr/bin/lipo -archs "$target")"
done
