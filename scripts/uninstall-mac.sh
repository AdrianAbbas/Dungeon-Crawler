#!/usr/bin/env bash
# Remove DUNGEON CRAWLER (the instrument) and DUNGEON CRAWLER FX for the current user.
#   bash uninstall-mac.sh           remove the plug-ins, keep your saved presets
#   bash uninstall-mac.sh --purge   also remove your presets and drop-in scenes
#                                   (~/Library/Windward Audio/DUNGEON CRAWLER and
#                                   DUNGEON CRAWLER FX, and the copies from before 1.0)
set -euo pipefail
purge=0
[[ "${1:-}" == --purge ]] && purge=1

removed=0
for bundle in "$HOME/Library/Audio/Plug-Ins/Components/DungeonCrawler.component" "$HOME/Library/Audio/Plug-Ins/VST3/DungeonCrawler.vst3" \
              "$HOME/Library/Audio/Plug-Ins/Components/DungeonCrawlerFX.component" "$HOME/Library/Audio/Plug-Ins/VST3/DungeonCrawlerFX.vst3"; do
    if [[ -e "$bundle" ]]; then
        rm -rf "$bundle"
        echo "Removed $bundle"
        removed=1
    fi
done
[[ $removed == 1 ]] || echo "No DUNGEON CRAWLER plug-ins found in ~/Library/Audio/Plug-Ins."

for data in "$HOME/Library/Windward Audio/DUNGEON CRAWLER" "$HOME/Library/Windward Audio/DUNGEON CRAWLER FX" \
            "$HOME/Library/Audio/Presets/Windward Audio/DUNGEON CRAWLER" "$HOME/Library/Audio/Presets/Windward Audio/DUNGEON CRAWLER FX" \
            "$HOME/Library/Application Support/DungeonCrawler"; do
    if [[ $purge == 1 && -d "$data" ]]; then
        rm -rf "$data"
        echo "Removed $data"
    elif [[ -d "$data" ]]; then
        echo "Kept your presets and scenes in $data (use --purge to remove them)."
    fi
done

for system_copy in /Library/Audio/Plug-Ins/Components/DungeonCrawler*.component /Library/Audio/Plug-Ins/VST3/DungeonCrawler*.vst3; do
    [[ -e "$system_copy" ]] && echo "A system-wide copy remains: sudo rm -rf \"$system_copy\""
done

/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Audio Unit registry reset. Restart GarageBand or Logic."
