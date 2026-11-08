#!/usr/bin/env bash
# Reproduce what a user does, then validate like a host.
#   1. unzip the release package and mark every file as a browser download
#      (com.apple.quarantine), which is what broke VEIL in GarageBand
#   2. record whether macOS on this runner refuses the quarantined copy
#   3. install with install-mac.sh --ci over copies installed under the old company name
#      (bundle IDs audio.veil.dungeoncrawler(.instrument)), which the installer must
#      remove; the registry must then list each plug-in once, as "Windward Audio: ..."
#   4. auval and the AU host harnesses (--strict), arm64 and x86_64 (Rosetta), for both
#      plug-ins: DUNGEON CRAWLER (the instrument, aumu Dgci, DungeonCrawlerInstrumentAUHarness)
#      and DUNGEON CRAWLER FX (the effect, aufx Dgcr, DungeonCrawlerAUHarness)
#      The harnesses also load a session of each saved before the rename
#      (Tests/fixtures/old-session-*.dcstate), which must load unchanged.
#   5. the user's files in the old folders are copied to ~/Library/Windward Audio/DUNGEON
#      CRAWLER (FX) on each plug-in's first launch, and the originals kept
# Usage: scripts/ci-user-install-test.sh <WindwardAudio-DungeonCrawler-macOS-Universal.zip> <effect harness binary> <log dir>
# (the instrument's harness is the binary DungeonCrawlerInstrumentAUHarness next to the effect's)
set -euo pipefail
package="$1"
harness="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
instrument_harness="$(dirname "$harness")/DungeonCrawlerInstrumentAUHarness"
logs="$3"
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$logs"
work="$(mktemp -d)"
status=0

# Run a command with a time limit (a Gatekeeper prompt would otherwise block forever).
limit() { local seconds="$1"; shift; /usr/bin/perl -e 'alarm shift @ARGV; exec @ARGV or die "exec failed: $!\n"' "$seconds" "$@"; }

/usr/bin/ditto -x -k "$package" "$work"
folder="$work/WindwardAudio-DungeonCrawler-macOS-Universal"
flag="0081;$(printf '%x' "$(date +%s)");Chrome;$(/usr/bin/uuidgen)"
/usr/bin/find "$folder" -exec /usr/bin/xattr -w com.apple.quarantine "$flag" {} +
{
    echo "Gatekeeper: $(/usr/sbin/spctl --status 2>&1)"
    /usr/bin/xattr -l "$folder/DungeonCrawler.component/Contents/MacOS/DungeonCrawler"
    /usr/bin/xattr -l "$folder/DungeonCrawlerFX.component/Contents/MacOS/DungeonCrawlerFX"
} | tee "$logs/quarantine.txt"

# The user's files where versions before the rename kept them (before anything loads a
# plug-in: its first launch copies them).
old_data="$HOME/Library/Application Support/DungeonCrawler"
old_host="$HOME/Library/Audio/Presets/Veil Audio"
new_root="$HOME/Library/Windward Audio"
aupreset() {   # file, AU type as a number
    mkdir -p "$(dirname "$1")"
    printf '<?xml version="1.0" encoding="UTF-8"?>\n<plist version="1.0"><dict><key>manufacturer</key><integer>1449486700</integer><key>name</key><string>%s</string><key>type</key><integer>%s</integer></dict></plist>\n' "$(basename "$1" .aupreset)" "$2" > "$1"
}
mkdir -p "$old_data/Presets" "$old_data/scenes"
echo "<DUNGEON_CRAWLER presetName=\"Before the rename\"/>" > "$old_data/Presets/Before the rename.dcpreset"
echo "a note left beside the drop-in scenes" > "$old_data/scenes/notes.txt"
aupreset "$old_host/DUNGEON CRAWLER/Instrument before.aupreset" 1635085685      # aumu
aupreset "$old_host/DUNGEON CRAWLER FX/Effect before.aupreset" 1635083896       # aufx
aupreset "$old_host/DungeonCrawler/Effect oldest.aupreset" 1635083896           # aufx, under the effect's first name
rm -rf "$new_root/DUNGEON CRAWLER" "$new_root/DUNGEON CRAWLER FX" "$HOME/Library/Audio/Presets/Windward Audio"

# 2. Quarantined copy installed by hand, without the script (informational).
components="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$components"
rm -rf "$components/DungeonCrawler.component" "$components/DungeonCrawlerFX.component"
/usr/bin/ditto "$folder/DungeonCrawlerFX.component" "$components/DungeonCrawlerFX.component"
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 3
set +e
limit 120 /usr/bin/auval -v aufx Dgcr Veil > "$logs/auval-quarantined-without-script.txt" 2>&1
quarantined_exit=$?
set -e
echo "auval on the quarantined copy (no install script): exit $quarantined_exit" | tee -a "$logs/quarantine.txt"
rm -rf "$components/DungeonCrawlerFX.component"

# Copies as versions before the rename installed them: the old bundle IDs and names
# (under other file names, so only their bundle IDs identify them).
vst3="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$vst3"
old_copies=()
for entry in "DungeonCrawler audio.veil.dungeoncrawler.instrument DUNGEON CRAWLER" "DungeonCrawlerFX audio.veil.dungeoncrawler DUNGEON CRAWLER FX"; do
    read -r name old_id shown <<< "$entry"
    for ext in component vst3; do
        dir="$components"; [[ $ext == vst3 ]] && dir="$vst3"
        dest="$dir/$name-before-rename.$ext"
        rm -rf "$dest"
        /usr/bin/ditto "$folder/$name.$ext" "$dest"
        /usr/bin/xattr -dr com.apple.quarantine "$dest" 2>/dev/null || true
        /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $old_id" "$dest/Contents/Info.plist"
        [[ $ext == component ]] && /usr/libexec/PlistBuddy -c "Set :AudioComponents:0:name Veil Audio: $shown" "$dest/Contents/Info.plist"
        /usr/bin/codesign --force --deep --sign - "$dest" >/dev/null 2>&1
        old_copies+=("$dest")
    done
done
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 3
{
    echo "Registry before the install (the copies under the old name):"
    /usr/bin/auval -a 2>/dev/null | /usr/bin/grep -E 'Dgc[ri]' || echo "(not listed)"
} | tee "$logs/registry-before.txt"

# 3. The user's path: run the installer from the quarantined download.
set +e
limit 300 /bin/bash "$folder/install-mac.sh" --ci 2>&1 | tee "$logs/install-mac.txt"
install_exit=${PIPESTATUS[0]}
set -e
echo "install-mac.sh exit $install_exit"
if [[ $install_exit != 0 ]]; then echo "::error::install-mac.sh failed (exit $install_exit)"; status=1; fi
for name in DungeonCrawler DungeonCrawlerFX; do
    if /usr/bin/xattr -lr "$components/$name.component" | /usr/bin/grep -q com.apple.quarantine; then
        echo "::error::installed $name.component is still quarantined"; status=1
    fi
done
for old in "${old_copies[@]}"; do
    if [[ -e "$old" ]]; then echo "::error::install-mac.sh left the copy under the old name: $old"; status=1; fi
done
# The registry lists each plug-in once, under the new company name.
: > "$logs/registry.txt"
for entry in "aumu Dgci DUNGEON CRAWLER" "aufx Dgcr DUNGEON CRAWLER FX"; do
    read -r type subtype shown <<< "$entry"
    expected="Windward Audio: $shown"
    listing=$(/usr/bin/auval -a 2>/dev/null | /usr/bin/grep -E "^[[:space:]]*$type[[:space:]]+$subtype[[:space:]]" || true)
    printf '%s\n' "$listing" | tee -a "$logs/registry.txt"
    entries=$(printf '%s\n' "$listing" | /usr/bin/grep -c . || true)
    if [[ $entries != 1 ]]; then
        echo "::error::the registry lists $shown $entries times"; status=1
    elif ! printf '%s\n' "$listing" | /usr/bin/grep -qE "$type[[:space:]]+$subtype[[:space:]]+Veil[[:space:]]+-[[:space:]]+$expected[[:space:]]*\$"; then
        echo "::error::the registry does not list $shown as '$expected': $listing"; status=1
    else
        echo "registry: $shown listed once, as $expected"
    fi
done

# 4. Validators on the installed copy.
run_check() {
    local name="$1"; shift
    set +e
    limit 600 "$@" > "$logs/$name.txt" 2>&1
    local code=$?
    set -e
    echo "$name: exit $code"
    if [[ $code != 0 ]]; then echo "::error::$name failed (exit $code)"; status=1; fi
}
strict=""  # becomes -strict when this auval supports it
/usr/bin/auval -h 2>&1 | /usr/bin/grep -qE -- '(^|[[:space:]])-strict([[:space:]]|$)' && strict="-strict"
# One process per hosting route, so a crash in one cannot hide the others. Every
# route is a gate: AUv2 API out-of-process and in-process, and the AUAudioUnit API
# out-of-process (the effect's rendered through AVAudioEngine since Milestone 1, see docs/DEVLOG.md).
suites="v2-oop v2-inproc auaudiounit"
rosetta=0
/usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null && rosetta=1
[[ $rosetta == 1 ]] || { echo "::error::Rosetta unavailable: the x86_64 slices were not validated"; status=1; }
samples_flag=""
[[ "${DC_REQUIRE_SAMPLES:-0}" == 1 ]] && samples_flag="--require-samples"
for entry in "fx aufx Dgcr $harness" "instrument aumu Dgci $instrument_harness"; do
    read -r tag type subtype binary <<< "$entry"
    extra=""
    [[ $tag == instrument ]] && extra="$samples_flag"
    shown="DUNGEON CRAWLER"; [[ $tag == fx ]] && shown="DUNGEON CRAWLER FX"
    # shellcheck disable=SC2086  # $strict is empty or one flag
    run_check "auval-$tag-arm64" /usr/bin/arch -arm64 /usr/bin/auval $strict -v "$type" "$subtype" Veil
    # shellcheck disable=SC2086  # $extra is empty or one flag
    for suite in $suites; do run_check "harness-$tag-arm64-$suite" /usr/bin/arch -arm64 "$binary" --strict --only "$suite" $extra --expect-name "Windward Audio: $shown" --old-session "$repo/Tests/fixtures/old-session-$tag.dcstate"; done
    if [[ $rosetta == 1 ]]; then
        # shellcheck disable=SC2086
        run_check "auval-$tag-x86_64" /usr/bin/arch -x86_64 /usr/bin/auval $strict -v "$type" "$subtype" Veil
        # shellcheck disable=SC2086
        for suite in $suites; do run_check "harness-$tag-x86_64-$suite" /usr/bin/arch -x86_64 "$binary" --strict --only "$suite" $extra --expect-name "Windward Audio: $shown" --old-session "$repo/Tests/fixtures/old-session-$tag.dcstate"; done
    fi
done
# Control (also a gate): Apple's own AULowpass through the same AUAudioUnit route, so a
# failure there tells a harness problem from a DUNGEON CRAWLER problem.
run_check control-apple-lowpass-auaudiounit "$harness" --strict --only auaudiounit --no-window --component "aufx:lpas:appl"
grep -h -E "^(FAIL|SUMMARY)" "$logs"/harness-*.txt || true

# 5. Each plug-in's first launch copied the user's files to its new folder and kept the
# originals; each host preset went to the plug-in its type names.
{
    echo "Migrated files:"
    /usr/bin/find "$new_root" "$HOME/Library/Audio/Presets/Windward Audio" -type f 2>/dev/null || true
} | tee "$logs/migration.txt"
expect_file() { if ! /usr/bin/cmp -s "$1" "$2"; then echo "::error::not migrated: $1"; status=1; fi; }
expect_none() { if [[ -e "$1" ]]; then echo "::error::copied to the wrong plug-in: $1"; status=1; fi; }
for product in "DUNGEON CRAWLER" "DUNGEON CRAWLER FX"; do
    expect_file "$new_root/$product/Presets/Before the rename.dcpreset" "$old_data/Presets/Before the rename.dcpreset"
    expect_file "$new_root/$product/scenes/notes.txt" "$old_data/scenes/notes.txt"
    [[ -f "$new_root/$product/.migrated" ]] && echo "migration ($product): $(cat "$new_root/$product/.migrated")"
done
expect_file "$new_root/DUNGEON CRAWLER/Host Presets/Instrument before.aupreset" "$old_host/DUNGEON CRAWLER/Instrument before.aupreset"
expect_file "$HOME/Library/Audio/Presets/Windward Audio/DUNGEON CRAWLER/Instrument before.aupreset" "$old_host/DUNGEON CRAWLER/Instrument before.aupreset"
expect_file "$new_root/DUNGEON CRAWLER FX/Host Presets/Effect before.aupreset" "$old_host/DUNGEON CRAWLER FX/Effect before.aupreset"
expect_file "$HOME/Library/Audio/Presets/Windward Audio/DUNGEON CRAWLER FX/Effect before.aupreset" "$old_host/DUNGEON CRAWLER FX/Effect before.aupreset"
expect_file "$new_root/DUNGEON CRAWLER FX/Host Presets/Effect oldest.aupreset" "$old_host/DungeonCrawler/Effect oldest.aupreset"
expect_none "$new_root/DUNGEON CRAWLER FX/Host Presets/Instrument before.aupreset"
expect_none "$new_root/DUNGEON CRAWLER/Host Presets/Effect before.aupreset"
for original in "$old_data/Presets/Before the rename.dcpreset" "$old_data/scenes/notes.txt" "$old_host/DUNGEON CRAWLER/Instrument before.aupreset" \
                "$old_host/DUNGEON CRAWLER FX/Effect before.aupreset" "$old_host/DungeonCrawler/Effect oldest.aupreset"; do
    if [[ ! -f "$original" ]]; then echo "::error::the original was not kept: $original"; status=1; fi
done
python3 "$(dirname "${BASH_SOURCE[0]}")/ci-crash-summary.py" 60 DungeonCrawlerAUHarness DungeonCrawlerInstrumentAUHarness AUHostingService auval DungeonCrawler DungeonCrawlerFX > "$logs/crashes.txt" 2>&1 || true
cat "$logs/crashes.txt"
exit $status
