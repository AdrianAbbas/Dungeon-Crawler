#!/usr/bin/env bash
# Install DUNGEON CRAWLER (the instrument) and DUNGEON CRAWLER FX (the effect) for the
# current user and make sure macOS will load them.
#
# Run it from the unzipped WindwardAudio-DungeonCrawler-macOS-Universal folder:
#     bash install-mac.sh
# (Tip: type "bash " in Terminal, then drag install-mac.sh into the window.)
#
# What it does:
#   0. removes copies installed under the earlier company name (found by their
#      earlier bundle IDs), so hosts do not list either plug-in twice
#   1. copies DungeonCrawler.component and DungeonCrawlerFX.component to
#      ~/Library/Audio/Plug-Ins/Components, the .vst3 bundles to ~/Library/Audio/Plug-Ins/VST3
#   2. removes the com.apple.quarantine flag your browser added. Without this,
#      macOS refuses to load the plug-ins and GarageBand shows a silent slot
#   3. re-signs every bundle (ad hoc) and verifies the signatures
#   4. restarts the Audio Unit registrar and clears the hosts' Audio Unit cache so
#      they see the new copies under their new company name, once each
#   5. runs Apple's Audio Unit validator (auval) on both and prints PASS or FAIL
#   6. prints the next steps for GarageBand and Logic
# (Before this version DungeonCrawler.component was the effect. It is now the
# instrument; the effect is DungeonCrawlerFX.component with the same Audio Unit codes,
# so projects that used the effect find it again.)
#
# Options:
#   --ci              non-interactive: no prompts, no check for running hosts
#   --out-of-process  remove the sandboxSafe flag from the installed copies, so
#                     GarageBand hosts them in a separate process (fallback only)
# (No `set -u`: macOS bash 3.2 treats an empty "$@" as unbound.)
set -eo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ci=0
out_of_process=0
for arg in "$@"; do
    case "$arg" in
        --ci) ci=1 ;;
        --out-of-process) out_of_process=1 ;;
        -h|--help) /usr/bin/sed -n '2,26p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 64 ;;
    esac
done

bold() { printf '\033[1m%s\033[0m\n' "$*"; }
ok()   { printf '  \033[32mOK\033[0m   %s\n' "$*"; }
warn() { printf '  \033[33mNOTE\033[0m %s\n' "$*"; }
fail() { printf '  \033[31mFAIL\033[0m %s\n' "$*"; }

if [[ "$(uname -s)" != Darwin ]]; then
    echo "install-mac.sh runs on macOS only." >&2
    exit 1
fi

# name, Audio Unit type, subtype
plugins=("DungeonCrawler aumu Dgci" "DungeonCrawlerFX aufx Dgcr")
for entry in "${plugins[@]}"; do
    read -r name _ _ <<< "$entry"
    if [[ ! -d "$here/$name.component" ]]; then
        echo "$name.component was not found next to this script ($here)." >&2
        echo "Unzip WindwardAudio-DungeonCrawler-macOS-Universal.zip first and run the script from that folder." >&2
        exit 1
    fi
done

components="$HOME/Library/Audio/Plug-Ins/Components"
vst3="$HOME/Library/Audio/Plug-Ins/VST3"
logs="$HOME/Library/Logs/DungeonCrawler"
mkdir -p "$components" "$vst3" "$logs"

bold "DUNGEON CRAWLER installer"

# Hosts keep their own view of plug-ins while running.
if [[ $ci == 0 ]]; then
    running=$(/usr/bin/pgrep -l -x GarageBand 2>/dev/null; /usr/bin/pgrep -l -f 'Logic Pro' 2>/dev/null || true)
    if [[ -n "$running" ]]; then
        warn "GarageBand or Logic is running. Quit it (Cmd-Q), then press Return to continue."
        read -r _
    fi
fi

# A second, system-wide copy would compete with this one.
for system_copy in /Library/Audio/Plug-Ins/Components/DungeonCrawler*.component /Library/Audio/Plug-Ins/VST3/DungeonCrawler*.vst3; do
    if [[ -e "$system_copy" ]]; then
        warn "An older system-wide copy exists: $system_copy"
        warn "Remove it so hosts load the new one:  sudo rm -rf \"$system_copy\""
    fi
done

# What hosts list for each bundle name.
display_name() { if [[ "$1" == DungeonCrawlerFX ]]; then echo "DUNGEON CRAWLER FX"; else echo "DUNGEON CRAWLER"; fi; }

# The bundle IDs the plug-ins had before the company rename. A copy with one of these
# (under any file name) is removed; its Audio Unit codes are the same as the new one's,
# so leaving it would list the plug-in twice.
old_ids="audio.veil.dungeoncrawler audio.veil.dungeoncrawler.instrument"
remove_old_copies() {
    local dir bundle id
    for dir in "$components" "$vst3"; do
        for bundle in "$dir"/*.component "$dir"/*.vst3; do
            [[ -d "$bundle" ]] || continue
            id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || true)
            for old in $old_ids; do
                if [[ "$id" == "$old" ]]; then
                    rm -rf "$bundle"
                    ok "removed the copy under the earlier company name: $bundle"
                fi
            done
        done
    done
    for dir in /Library/Audio/Plug-Ins/Components /Library/Audio/Plug-Ins/VST3; do
        for bundle in "$dir"/*.component "$dir"/*.vst3; do
            [[ -d "$bundle" ]] || continue
            id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || true)
            for old in $old_ids; do
                if [[ "$id" == "$old" ]]; then
                    warn "A system-wide copy under the earlier company name exists: $bundle"
                    warn "Remove it so hosts list the plug-in once:  sudo rm -rf \"$bundle\""
                fi
            done
        done
    done
}

install_bundle() {
    local src="$1" dest="$2" binary
    rm -rf "$dest"
    /usr/bin/ditto "$src" "$dest"
    binary="$dest/Contents/MacOS/$(basename "${dest%.*}")"
    chmod +x "$binary"
    /usr/bin/xattr -dr com.apple.quarantine "$dest" 2>/dev/null || true
    if /usr/bin/xattr -lr "$dest" 2>/dev/null | /usr/bin/grep -q com.apple.quarantine; then
        fail "could not remove the quarantine flag from $dest"
        return 1
    fi
    if [[ "$dest" == *.component && $out_of_process == 1 ]]; then
        /usr/libexec/PlistBuddy -c "Delete :AudioComponents:0:sandboxSafe" "$dest/Contents/Info.plist" 2>/dev/null || true
        warn "sandboxSafe removed: GarageBand will host $(basename "$dest") out-of-process"
    fi
    # A Developer ID signed and notarised release keeps its signature; anything else
    # is re-signed ad hoc for this Mac.
    if /usr/bin/codesign -dv --verbose=2 "$dest" 2>&1 | /usr/bin/grep -q "Authority=Developer ID Application" \
        && [[ $out_of_process == 0 ]]; then
        :
    elif ! /usr/bin/codesign --force --deep --sign - "$dest" > "$logs/codesign-sign-$(basename "$dest").txt" 2>&1; then
        fail "signing failed for $dest (see $logs/codesign-sign-$(basename "$dest").txt)"
        return 1
    fi
    if ! /usr/bin/codesign --verify --strict --verbose=2 "$dest" > "$logs/codesign-$(basename "$dest").txt" 2>&1; then
        fail "signature check failed for $dest (see $logs/codesign-$(basename "$dest").txt)"
        return 1
    fi
    ok "$(basename "$dest") installed, quarantine removed, signature valid ($(/usr/bin/file -b "$binary" | /usr/bin/sed 's/Mach-O universal binary with //'))"
}

bold "0. Removing copies under the earlier company name"
remove_old_copies

bold "1-3. Copying, removing quarantine, signing"
for entry in "${plugins[@]}"; do
    read -r name _ _ <<< "$entry"
    install_bundle "$here/$name.component" "$components/$name.component"
    if [[ -d "$here/$name.vst3" ]]; then
        install_bundle "$here/$name.vst3" "$vst3/$name.vst3"
    else
        warn "$name.vst3 not in this folder; skipped"
    fi
done

bold "4. Resetting the Audio Unit registry"
# GarageBand and Logic keep a cache of the Audio Units they scanned (with their names);
# it is rebuilt at their next launch, so they show the new company name.
rm -f "$HOME/Library/Caches/AudioUnitCache/com.apple.audiounits.cache" 2>/dev/null || true
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
registry_failed=0
: > "$logs/install-registry.txt"
for entry in "${plugins[@]}"; do
    read -r name type subtype <<< "$entry"
    shown="Windward Audio: $(display_name "$name")"
    registered=0
    listing=""
    for _ in $(seq 1 30); do
        listing=$(/usr/bin/auval -a 2>/dev/null | /usr/bin/grep -E "^[[:space:]]*$type[[:space:]]+$subtype[[:space:]]+Veil" || true)
        if [[ -n "$listing" ]]; then
            registered=1
            break
        fi
        sleep 1
    done
    printf '%s\n' "$listing" >> "$logs/install-registry.txt"
    entries=$(printf '%s\n' "$listing" | /usr/bin/grep -c . || true)
    if [[ $registered == 0 ]]; then
        fail "$(display_name "$name") is not listed by the Audio Unit registry after 30 s"
    elif [[ $entries != 1 ]]; then
        fail "the Audio Unit registry lists $(display_name "$name") $entries times (see $logs/install-registry.txt)"
        registry_failed=1
    elif ! printf '%s\n' "$listing" | /usr/bin/grep -Eq "$shown[[:space:]]*\$"; then
        fail "the Audio Unit registry lists $(display_name "$name") under another name: $listing"
        registry_failed=1
    else
        ok "macOS lists $(display_name "$name") once, as $shown"
    fi
done

bold "5. Validating with Apple's auval"
result=$registry_failed
for entry in "${plugins[@]}"; do
    read -r name type subtype <<< "$entry"
    auval_log="$logs/install-auval-$name.txt"
    set +e
    /usr/bin/auval -v "$type" "$subtype" Veil > "$auval_log" 2>&1
    auval_exit=$?
    set -e
    if [[ $auval_exit == 0 ]] && /usr/bin/grep -q "AU VALIDATION SUCCEEDED" "$auval_log"; then
        ok "auval $name: PASS ($(uname -m))"
    else
        fail "auval $name: FAIL ($(uname -m), exit $auval_exit). Full log: $auval_log"
        /usr/bin/grep -E "FATAL|ERROR|Cannot open" "$auval_log" | /usr/bin/head -5 | /usr/bin/sed 's/^/       /'
        result=1
    fi
    if [[ "$(uname -m)" == arm64 ]] && /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then
        set +e
        /usr/bin/arch -x86_64 /usr/bin/auval -v "$type" "$subtype" Veil > "$logs/install-auval-$name-x86_64.txt" 2>&1
        x86_exit=$?
        set -e
        if [[ $x86_exit == 0 ]]; then ok "auval $name: PASS (x86_64 under Rosetta)"; else warn "auval $name under Rosetta exited $x86_exit (only matters for Intel-mode hosts)"; fi
    fi
done

bold "6. Next steps"
cat <<'NEXT'
  GarageBand
    1. Open GarageBand > Settings > Audio/MIDI and make sure "Audio Units" is ticked.
    2. Quit and reopen GarageBand.
    3. The instrument: make a Software Instrument track, open the Library (Y) or the
       track's instrument slot in Smart Controls (B) > Plug-ins > Instrument:
       Audio Units > Windward Audio > DUNGEON CRAWLER. Play your keyboard or the piano roll.
    4. The effect: on any track, Smart Controls (B) > Plug-ins > an empty Audio Effect
       slot: Audio Units > Windward Audio > DUNGEON CRAWLER FX.
    5. If the window shows sliders instead of the painted interface, use the view menu
       at the top of the plug-in window to switch from "Controls" to "Editor".
  Logic Pro
    1. Logic Pro > Settings > Plug-in Manager, find Windward Audio > DUNGEON CRAWLER and
       DUNGEON CRAWLER FX. If one shows as failed, select it and click
       "Reset & Rescan Selection".
    2. The instrument: a Software Instrument track's Instrument slot:
       AU Instruments > Windward Audio > DUNGEON CRAWLER.
    3. The effect: any track's Audio FX slot: Audio Units > Windward Audio > DUNGEON CRAWLER FX.
NEXT
if [[ $result == 0 ]]; then
    bold "DUNGEON CRAWLER and DUNGEON CRAWLER FX are installed and validated."
else
    bold "DUNGEON CRAWLER is installed, but validation failed. Send the auval logs in $logs when reporting this."
fi
exit $result
