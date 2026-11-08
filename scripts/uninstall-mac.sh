#!/usr/bin/env bash
# Remove DUNGEON CRAWLER (the instrument) and DUNGEON CRAWLER FX for the current user.
#   bash uninstall-mac.sh           remove the plug-ins, keep your saved presets
#   bash uninstall-mac.sh --purge   also remove your presets and drop-in scenes
#                                   (~/Library/Windward Audio/DUNGEON CRAWLER and
#                                   DUNGEON CRAWLER FX, the host's presets, and the
#                                   folders of the versions before the company rename)
set -euo pipefail
purge=0
[[ "${1:-}" == --purge ]] && purge=1

components="$HOME/Library/Audio/Plug-Ins/Components"
vst3="$HOME/Library/Audio/Plug-Ins/VST3"
# The bundle IDs: now, and before the company rename (a copy of any is removed).
ids="com.windwardaudio.dungeoncrawler com.windwardaudio.dungeoncrawlerfx audio.veil.dungeoncrawler audio.veil.dungeoncrawler.instrument"

removed=0
for bundle in "$components/DungeonCrawler.component" "$vst3/DungeonCrawler.vst3" \
              "$components/DungeonCrawlerFX.component" "$vst3/DungeonCrawlerFX.vst3"; do
    if [[ -e "$bundle" ]]; then
        rm -rf "$bundle"
        echo "Removed $bundle"
        removed=1
    fi
done
for bundle in "$components"/*.component "$vst3"/*.vst3; do
    [[ -d "$bundle" ]] || continue
    id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || true)
    for known in $ids; do
        if [[ "$id" == "$known" ]]; then
            rm -rf "$bundle"
            echo "Removed $bundle"
            removed=1
        fi
    done
done
[[ $removed == 1 ]] || echo "No DUNGEON CRAWLER plug-ins found in ~/Library/Audio/Plug-Ins."

for data in "$HOME/Library/Windward Audio/DUNGEON CRAWLER" "$HOME/Library/Windward Audio/DUNGEON CRAWLER FX" \
            "$HOME/Library/Audio/Presets/Windward Audio/DUNGEON CRAWLER" "$HOME/Library/Audio/Presets/Windward Audio/DUNGEON CRAWLER FX"; do
    if [[ $purge == 1 && -d "$data" ]]; then
        rm -rf "$data"
        echo "Removed $data"
    elif [[ -d "$data" ]]; then
        echo "Kept your presets and scenes in $data (use --purge to remove them)."
    fi
done
# The folders of the versions before the company rename (copied to the folders above on
# the first launch after it; the originals were kept).
if [[ $purge == 1 ]]; then
    for data in "$HOME/Library/Application Support/DungeonCrawler" "$HOME/Library/Audio/Presets/Veil Audio/DUNGEON CRAWLER" \
                "$HOME/Library/Audio/Presets/Veil Audio/DUNGEON CRAWLER FX" "$HOME/Library/Audio/Presets/Veil Audio/DungeonCrawler" \
                "$HOME/Library/Audio/Presets/Veil Audio/DungeonCrawlerFX"; do
        if [[ -d "$data" ]]; then
            rm -rf "$data"
            removed_old=1
        fi
    done
    if [[ ${removed_old:-0} == 1 ]]; then echo "Removed the presets and scenes kept from the versions before the company rename."; fi
fi

for system_copy in /Library/Audio/Plug-Ins/Components/DungeonCrawler*.component /Library/Audio/Plug-Ins/VST3/DungeonCrawler*.vst3; do
    [[ -e "$system_copy" ]] && echo "A system-wide copy remains: sudo rm -rf \"$system_copy\""
done

/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Audio Unit registry reset. Restart GarageBand or Logic."
