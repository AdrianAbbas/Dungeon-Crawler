#!/usr/bin/env bash
# Install "DUNGEON CRAWLER.pkg" on the runner the way a user does, then validate the installed copies.
#   1. with --notarized: mark the package as a browser download (com.apple.quarantine) and check
#      that Gatekeeper accepts it ("accepted", "source=Notarized Developer ID")
#   2. plant what the package must remove silently: copies installed before 1.0 (the old bundle
#      IDs audio.veil.dungeoncrawler*, under their own and other names), system-wide and in
#      ~/Library, and the per-user copies install-mac.sh left (scripts/ci-user-install-test.sh
#      runs first); and plant presets in the folders used before 1.0, for the migration check
#   3. sudo installer -pkg; check the receipts, the four installed bundles (root-owned, signed,
#      com.windwardaudio.* IDs, version 1.0.0, with their sounds), that the planted copies are
#      gone, and the registry: each plug-in listed once, as "Windward Audio: ..."
#   4. auval and the AU host harnesses (--strict, with a session saved before 1.0), arm64 and
#      x86_64 (Rosetta), for both plug-ins, on the installed copies
#   5. the presets were copied to ~/Library/Windward Audio/... (the old folders kept)
# Usage: scripts/ci-pkg-install-test.sh [--notarized] <DUNGEON CRAWLER.pkg> <effect harness binary> <log dir>
# (the instrument's harness is the binary DungeonCrawlerInstrumentAUHarness next to the effect's)
set -euo pipefail
notarized=0
if [[ "${1:-}" == --notarized ]]; then notarized=1; shift; fi
pkg="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
harness="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
instrument_harness="$(dirname "$harness")/DungeonCrawlerInstrumentAUHarness"
logs="$3"
mkdir -p "$logs"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
status=0
limit() { local seconds="$1"; shift; /usr/bin/perl -e 'alarm shift @ARGV; exec @ARGV or die "exec failed: $!\n"' "$seconds" "$@"; }
sys_components="/Library/Audio/Plug-Ins/Components"
sys_vst3="/Library/Audio/Plug-Ins/VST3"
user_components="$HOME/Library/Audio/Plug-Ins/Components"
user_vst3="$HOME/Library/Audio/Plug-Ins/VST3"

# 1. Gatekeeper on the downloaded package.
if [[ $notarized == 1 ]]; then
    /usr/bin/xattr -w com.apple.quarantine "0081;$(printf '%x' "$(date +%s)");Chrome;$(/usr/bin/uuidgen)" "$pkg"
    out=$(/usr/sbin/spctl --assess --type install -vvv "$pkg" 2>&1 || true)
    echo "$out" | tee "$logs/pkg-gatekeeper.txt"
    if ! echo "$out" | /usr/bin/grep -q ": accepted" || ! echo "$out" | /usr/bin/grep -q "^source=Notarized Developer ID"; then
        echo "::error::Gatekeeper does not accept the downloaded DUNGEON CRAWLER.pkg as notarised Developer ID"; status=1
    fi
else
    echo "::notice::DUNGEON CRAWLER.pkg is unsigned in this run (ad-hoc build): Gatekeeper checks skipped" | tee "$logs/pkg-gatekeeper.txt"
fi

# 2. Plant old copies. Their sources: the bundles inside the package itself.
expanded="$(mktemp -d)/expanded"
/usr/sbin/pkgutil --expand-full "$pkg" "$expanded"
src_au="$expanded/au.pkg/Payload"
src_vst3="$expanded/vst3.pkg/Payload"
ls "$src_au" "$src_vst3"
# The installer standard (docs/INSTALLER_STANDARD.md): title, pages, licence and backgrounds.
dist_xml="$expanded/Distribution"
/usr/bin/grep -q "<title>DUNGEON CRAWLER by Windward Audio</title>" "$dist_xml" || { echo "::error::the installer's window title is not 'DUNGEON CRAWLER by Windward Audio'"; status=1; }
for res in welcome.html readme.html EULA.rtf conclusion.html background.png background-dark.png; do
    /usr/bin/grep -q "\"$res\"" "$dist_xml" || { echo "::error::the distribution does not use $res"; status=1; }
    [[ -f "$expanded/Resources/$res" ]] || { echo "::error::the installer has no $res"; status=1; }
done
if /usr/bin/grep -il -E 'veil|install-mac' "$expanded/Distribution" "$expanded"/Resources/*.html "$expanded/Resources/EULA.rtf"; then
    echo "::error::the installer's pages mention an old name or a script"; status=1
fi
plant() {   # plant <source bundle> <destination> <bundle ID> [sudo]
    ${4:+/usr/bin/sudo} mkdir -p "$(dirname "$2")"
    ${4:+/usr/bin/sudo} rm -rf "$2"
    ${4:+/usr/bin/sudo} /usr/bin/ditto "$1" "$2"
    ${4:+/usr/bin/sudo} /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $3" "$2/Contents/Info.plist"
    echo "planted $2 ($3)"
}
# The per-user copies the zip install (install-mac.sh, run before this) left must go too.
planted=()
for copy in "$user_components/DungeonCrawler.component" "$user_components/DungeonCrawlerFX.component" "$user_vst3/DungeonCrawler.vst3" "$user_vst3/DungeonCrawlerFX.vst3"; do
    if [[ -d "$copy" ]]; then planted+=("$copy"); echo "per-user copy from the zip install: $copy"; fi
done
# Pre-1.0 copies (old bundle IDs) at the paths the package installs to: replaced by the new ones.
plant "$src_au/DungeonCrawler.component" "$sys_components/DungeonCrawler.component" audio.veil.dungeoncrawler.instrument sudo
plant "$src_vst3/DungeonCrawlerFX.vst3" "$sys_vst3/DungeonCrawlerFX.vst3" audio.veil.dungeoncrawler sudo
# Pre-1.0 copies under other names and in ~/Library: removed.
plant "$src_au/DungeonCrawlerFX.component" "$sys_components/DUNGEON CRAWLER old.component" audio.veil.dungeoncrawler sudo
plant "$src_vst3/DungeonCrawler.vst3" "$sys_vst3/DungeonCrawler 0.1.vst3" audio.veil.dungeoncrawler.instrument sudo
plant "$src_au/DungeonCrawlerFX.component" "$user_components/DungeonCrawlerFX.component" audio.veil.dungeoncrawler
plant "$src_vst3/DungeonCrawler.vst3" "$user_vst3/DungeonCrawler.vst3" audio.veil.dungeoncrawler.instrument
planted+=("$sys_components/DUNGEON CRAWLER old.component" "$sys_vst3/DungeonCrawler 0.1.vst3")
# A plug-in that only shares the old company code is someone else's: it must stay.
plant "$src_vst3/DungeonCrawlerFX.vst3" "$user_vst3/Unrelated.vst3" com.example.unrelated
# Presets in the folders used before 1.0 (for step 5): the plug-ins' own and a host's.
old_data="$HOME/Library/Application Support/DungeonCrawler"
old_host="$HOME/Library/Audio/Presets/Veil Audio/DUNGEON CRAWLER FX"
new_data="$HOME/Library/Windward Audio"
new_host="$HOME/Library/Audio/Presets/Windward Audio"
rm -rf "$new_data/DUNGEON CRAWLER" "$new_data/DUNGEON CRAWLER FX" "$new_host/DUNGEON CRAWLER" "$new_host/DUNGEON CRAWLER FX"
mkdir -p "$old_data/Presets" "$old_host"
cp "$here/Tests/fixtures/session-pre-1.0-fx.xml" "$old_data/Presets/Saved Before 1.0.dcpreset"
printf '<?xml version="1.0" encoding="UTF-8"?>\n<plist version="1.0"><dict><key>manufacturer</key><integer>1449485676</integer><key>name</key><string>Old Echo</string><key>subtype</key><integer>1147626354</integer><key>type</key><integer>1635083896</integer><key>version</key><integer>0</integer></dict></plist>\n' > "$old_host/Old Echo.aupreset"

# 3. Install.
set +e
limit 600 /usr/bin/sudo /usr/sbin/installer -pkg "$pkg" -target / -verboseR 2>&1 | tee "$logs/pkg-installer.txt"
install_exit=${PIPESTATUS[0]}
set -e
/usr/bin/grep "DUNGEON CRAWLER installer:" /var/log/install.log | /usr/bin/tail -40 | tee "$logs/pkg-scripts.txt" || true
echo "installer exit $install_exit"
if [[ $install_exit != 0 ]]; then echo "::error::installer -pkg DUNGEON CRAWLER.pkg failed (exit $install_exit)"; status=1; fi
/usr/sbin/pkgutil --pkgs | /usr/bin/grep com.windwardaudio | tee "$logs/pkg-receipts.txt" || true
for receipt in com.windwardaudio.pkg.dungeoncrawler.au com.windwardaudio.pkg.dungeoncrawler.vst3; do
    /usr/bin/grep -qx "$receipt" "$logs/pkg-receipts.txt" || { echo "::error::no installer receipt $receipt"; status=1; }
done
for entry in "$sys_components/DungeonCrawler.component com.windwardaudio.dungeoncrawler" "$sys_components/DungeonCrawlerFX.component com.windwardaudio.dungeoncrawlerfx" \
             "$sys_vst3/DungeonCrawler.vst3 com.windwardaudio.dungeoncrawler" "$sys_vst3/DungeonCrawlerFX.vst3 com.windwardaudio.dungeoncrawlerfx"; do
    bundle="${entry% *}"; want_id="${entry##* }"
    if [[ ! -d "$bundle" ]]; then echo "::error::DUNGEON CRAWLER.pkg did not install $bundle"; status=1; continue; fi
    owner=$(/usr/bin/stat -f %Su:%Sg "$bundle")
    [[ "$owner" == root:wheel ]] || { echo "::error::$bundle is owned by $owner (expected root:wheel)"; status=1; }
    id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist")
    version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$bundle/Contents/Info.plist")
    [[ "$id" == "$want_id" ]] || { echo "::error::$bundle has bundle ID $id (expected $want_id)"; status=1; }
    [[ "$version" == 1.0.0 ]] || { echo "::error::$bundle is version $version (expected 1.0.0)"; status=1; }
    if /usr/bin/codesign --verify --deep --strict --verbose=2 "$bundle" >> "$logs/pkg-codesign.txt" 2>&1; then
        echo "installed and validly signed: $bundle ($id $version)"
    else
        echo "::error::the installed $bundle fails codesign --verify --deep --strict"; status=1
    fi
    if [[ $notarized == 1 ]]; then
        out=$(/usr/sbin/spctl --assess --type open --context context:primary-signature -vvv "$bundle" 2>&1 || true)
        echo "$out" >> "$logs/pkg-gatekeeper.txt"
        echo "$out" | /usr/bin/grep -q "^source=Notarized Developer ID" || { echo "::error::Gatekeeper does not accept the installed $bundle as notarised"; status=1; }
    fi
done
# The sounds travel inside the bundles when this build has them.
if [[ -d "$src_au/DungeonCrawler.component/Contents/Resources/Samples" ]]; then
    for bundle in "$sys_components/DungeonCrawler.component" "$sys_vst3/DungeonCrawler.vst3"; do
        [[ -f "$bundle/Contents/Resources/Samples/index.json" ]] || { echo "::error::$bundle was installed without its sounds"; status=1; }
    done
fi
for old in "${planted[@]}"; do
    if [[ -e "$old" ]]; then echo "::error::DUNGEON CRAWLER.pkg left $old in place"; status=1; else echo "removed by the package: $old"; fi
done
if [[ -d "$user_vst3/Unrelated.vst3" ]]; then echo "kept, not ours: $user_vst3/Unrelated.vst3"; rm -rf "$user_vst3/Unrelated.vst3"
else echo "::error::the package removed an unrelated plug-in"; status=1; fi

/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
for _ in $(seq 1 30); do
    listing=$(/usr/bin/auval -a 2>/dev/null || true)
    if echo "$listing" | /usr/bin/grep -Eq '^[[:space:]]*aumu[[:space:]]+Dgci[[:space:]]+Veil' \
        && echo "$listing" | /usr/bin/grep -Eq '^[[:space:]]*aufx[[:space:]]+Dgcr[[:space:]]+Veil'; then break; fi
    sleep 1
done
/usr/bin/auval -a > "$logs/pkg-registry-all.txt" 2>&1 || true
/usr/bin/grep -E 'Dgci|Dgcr' "$logs/pkg-registry-all.txt" | tee "$logs/pkg-registry.txt" || true
for entry in "aumu Dgci Veil|Windward Audio: DUNGEON CRAWLER$" "aufx Dgcr Veil|Windward Audio: DUNGEON CRAWLER FX$"; do
    codes="${entry%%|*}"; name="${entry#*|}"
    pattern="^[[:space:]]*${codes// /[[:space:]]+}"
    n=$(/usr/bin/grep -c -E "$pattern" "$logs/pkg-registry.txt" || true)
    [[ "$n" == 1 ]] || { echo "::error::after the package install the AU registry lists $codes $n times (expected once)"; status=1; }
    /usr/bin/grep -E "$pattern" "$logs/pkg-registry.txt" | /usr/bin/sed 's/[[:space:]]*$//' | /usr/bin/grep -Eq "[[:space:]]-[[:space:]]+$name" \
        || { echo "::error::the AU registry does not name $codes \"${name%\$}\""; status=1; }
done
if /usr/bin/grep -iE 'veil audio|elsewhere' "$logs/pkg-registry-all.txt"; then echo "::error::the AU registry still shows an old company name"; status=1; fi
windward=$(/usr/bin/grep -c 'Windward Audio: DUNGEON CRAWLER' "$logs/pkg-registry.txt" || true)
[[ "$windward" == 2 ]] || { echo "::error::the AU registry lists $windward Windward Audio DUNGEON CRAWLER plug-ins (expected 2)"; status=1; }

# 4. Validators on the installed copies.
run_check() {
    local name="$1"; shift
    set +e
    limit 600 "$@" > "$logs/$name.txt" 2>&1
    local code=$?
    set -e
    echo "$name: exit $code"
    if [[ $code != 0 ]]; then echo "::error::$name failed (exit $code)"; status=1; fi
}
strict=""
/usr/bin/auval -h 2>&1 | /usr/bin/grep -qE -- '(^|[[:space:]])-strict([[:space:]]|$)' && strict="-strict"
samples_flag=""
[[ "${DC_REQUIRE_SAMPLES:-0}" == 1 ]] && samples_flag="--require-samples"
archs="arm64"
if /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then archs="arm64 x86_64"; else echo "::error::Rosetta unavailable: the x86_64 slice was not validated"; status=1; fi
for arch in $archs; do
    for entry in "fx aufx Dgcr $harness session-pre-1.0-fx.xml" "instrument aumu Dgci $instrument_harness session-pre-1.0-instrument.xml"; do
        read -r tag type subtype binary fixture <<< "$entry"
        extra=""
        [[ $tag == instrument ]] && extra="$samples_flag"
        # shellcheck disable=SC2086
        run_check "pkg-auval-$tag-$arch" /usr/bin/arch -$arch /usr/bin/auval $strict -v "$type" "$subtype" Veil
        for suite in v2-oop v2-inproc auaudiounit; do
            # shellcheck disable=SC2086
            run_check "pkg-harness-$tag-$arch-$suite" /usr/bin/arch -$arch "$binary" --strict --only "$suite" --old-session "$here/Tests/fixtures/$fixture" $extra
        done
    done
done
grep -h -E "^(FAIL|SUMMARY)" "$logs"/pkg-harness-*.txt || true
for f in "$logs"/pkg-auval-*.txt; do echo "$(basename "$f"): $(/usr/bin/grep -E 'AU VALIDATION (SUCCEEDED|FAILED)' "$f" || echo 'no verdict')"; done

# 5. The plug-ins (loaded above) copied the presets from the folders used before 1.0, and kept them.
{
    for p in "DUNGEON CRAWLER" "DUNGEON CRAWLER FX"; do
        f="$new_data/$p/Presets/Saved Before 1.0.dcpreset"
        if [[ -f "$f" ]] && /usr/bin/cmp -s "$f" "$old_data/Presets/Saved Before 1.0.dcpreset"; then echo "OK   migrated: $f"
        else echo "::error::the preset saved before 1.0 was not copied to $f"; status=1; fi
    done
    if [[ -f "$new_host/DUNGEON CRAWLER FX/Old Echo.aupreset" ]]; then echo "OK   migrated: $new_host/DUNGEON CRAWLER FX/Old Echo.aupreset"
    else echo "::error::the host preset saved before 1.0 was not copied to $new_host/DUNGEON CRAWLER FX"; status=1; fi
    if [[ -f "$old_data/Presets/Saved Before 1.0.dcpreset" && -f "$old_host/Old Echo.aupreset" ]]; then echo "OK   the old folders were kept"
    else echo "::error::the migration removed the old presets (it must copy)"; status=1; fi
    find "$new_data" "$new_host" -print 2>/dev/null | sed "s|$HOME|~|"
} 2>&1 | tee "$logs/pkg-migration.txt"
/usr/bin/grep -q "::error::" "$logs/pkg-migration.txt" && status=1
python3 "$here/scripts/ci-crash-summary.py" 60 DungeonCrawlerAUHarness DungeonCrawlerInstrumentAUHarness AUHostingService auval DungeonCrawler DungeonCrawlerFX > "$logs/pkg-crashes.txt" 2>&1 || true
cat "$logs/pkg-crashes.txt"
exit $status
