#!/usr/bin/env bash
# Install a DUNGEON CRAWLER component into ~/Library/Audio/Plug-Ins/Components, restart
# the AU registrar and wait until it is listed. Used by CI experiments.
# Usage: scripts/register-au.sh <path/to/DungeonCrawler.component | DungeonCrawlerFX.component>
set -euo pipefail
source_bundle="$1"
name="$(basename "$source_bundle")"
case "$name" in
    DungeonCrawlerFX.component) code="aufx[[:space:]]+Dgcr" ;;
    DungeonCrawler.component) code="aumu[[:space:]]+Dgci" ;;
    *) echo "unknown bundle $name" >&2; exit 64 ;;
esac
components="$HOME/Library/Audio/Plug-Ins/Components"
installed="$components/$name"
mkdir -p "$components"
rm -rf "$installed"
/usr/bin/ditto "$source_bundle" "$installed"
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
for attempt in $(seq 1 30); do
    if /usr/bin/auval -a 2>/dev/null | /usr/bin/grep -Eq "^[[:space:]]*$code[[:space:]]+Veil"; then
        echo "registered after $attempt attempt(s)"
        exit 0
    fi
    sleep 1
done
echo "$name not registered after 30 s" >&2
exit 2
