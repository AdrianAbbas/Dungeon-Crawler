#!/usr/bin/env bash
# CI gate for the built macOS bundles of both plug-ins: both architecture slices
# present, AU identity (the instrument aumu/Dgci, the effect aufx/Dgcr) and sandboxSafe
# in Info.plist, minimum macOS version reported, the painted art inside every bundle
# (Contents/Resources/Painted), the recorded samples inside the instrument's, the voice
# sets inside the effect's (Session 8).
# Usage: scripts/ci-check-bundles.sh <build dir> [config]
set -euo pipefail
build="$1"
config="${2:-Release}"
status=0
# name, AU type, subtype, factory, bundle ID (since the company rename), what hosts list
for entry in "DungeonCrawler aumu Dgci DungeonCrawlerInstAUFactory com.windwardaudio.dungeoncrawler DUNGEON CRAWLER" \
             "DungeonCrawlerFX aufx Dgcr DungeonCrawlerAUFactory com.windwardaudio.dungeoncrawlerfx DUNGEON CRAWLER FX"; do
    read -r name type subtype factory bundle_id shown <<< "$entry"
    artefacts="$build/${name}_artefacts/$config"
    for bundle in "$artefacts/AU/$name.component" "$artefacts/VST3/$name.vst3" "$artefacts/Standalone/$name.app"; do
        binary="$bundle/Contents/MacOS/$name"
        if [[ ! -f "$binary" ]]; then
            echo "::error::missing $binary"; status=1; continue
        fi
        archs=$(/usr/bin/lipo -archs "$binary")
        printf '%-70s %s\n' "$binary" "$archs"
        if [[ " $archs " != *" arm64 "* || " $archs " != *" x86_64 "* ]]; then
            echo "::error::$binary must contain arm64 and x86_64 slices, has: $archs"; status=1
        fi
        for arch in x86_64 arm64; do
            minos=$(/usr/bin/otool -arch "$arch" -l "$binary" | /usr/bin/awk '/LC_BUILD_VERSION/{f=1} f&&/minos/{print $2; exit}')
            printf '    %-7s minimum macOS %s\n' "$arch" "${minos:-unknown}"
        done
        painted="$bundle/Contents/Resources/Painted"
        if [[ -f "$painted/map/sky.jpg" ]]; then
            printf '    painted art: %s scenes, %s\n' "$(find "$painted" -name scene.json | wc -l | tr -d ' ')" "$(du -sh "$painted" | cut -f1)"
        else
            echo "::error::missing the painted art in $bundle (Contents/Resources/Painted)"; status=1
        fi
        if [[ $type == aumu ]]; then
            samples="$bundle/Contents/Resources/Samples"
            if [[ -f "$samples/index.json" ]]; then
                printf '    samples: %s instruments, %s\n' "$(find "$samples" -maxdepth 1 -name '*.json' ! -name index.json | wc -l | tr -d ' ')" "$(du -sh "$samples" | cut -f1)"
            elif [[ "${DC_REQUIRE_SAMPLES:-0}" == 1 ]]; then
                echo "::error::missing the recorded samples in $bundle (Contents/Resources/Samples)"; status=1
            else
                echo "    samples: none (no SAMPLES_TOKEN: the sampled instruments play their stand-ins)"
            fi
        else
            # Session 8: the effect ships the developer's voice for its places (whispers,
            # the choir stacks' imprint), and nothing else.
            samples="$bundle/Contents/Resources/Samples"
            if [[ -f "$samples/index.json" ]]; then
                printf '    voice samples: %s sets, %s\n' "$(find "$samples" -maxdepth 1 -name '*.json' ! -name index.json | wc -l | tr -d ' ')" "$(du -sh "$samples" | cut -f1)"
                for set in voice-whispers choir-ooh choir-aah; do
                    [[ -f "$samples/$set.json" ]] || { echo "::error::$bundle lacks the voice set $set"; status=1; }
                done
            elif [[ "${DC_REQUIRE_SAMPLES:-0}" == 1 ]]; then
                echo "::error::missing the voice samples in $bundle (Contents/Resources/Samples)"; status=1
            else
                echo "    voice samples: none (no SAMPLES_TOKEN: the voice places sound as before)"
            fi
        fi
    done

    plist="$artefacts/AU/$name.component/Contents/Info.plist"
    check() {
        local key="$1" expected="$2" actual
        actual=$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:0:$key" "$plist" 2>/dev/null || echo "(absent)")
        if [[ "$actual" == "$expected" ]]; then
            printf '%s Info.plist %-16s %s\n' "$name" "$key" "$actual"
        else
            echo "::error::$name Info.plist $key expected '$expected', got '$actual'"; status=1
        fi
    }
    check type "$type"
    check subtype "$subtype"
    check manufacturer Veil
    check factoryFunction "$factory"
    check sandboxSafe true
    # The company rename: hosts show "Windward Audio: <name>" (AU) and the vendor
    # Windward Audio (VST3); the bundle IDs are new; the codes above stay.
    check name "Windward Audio: $shown"
    for bundle in "$artefacts/AU/$name.component" "$artefacts/VST3/$name.vst3" "$artefacts/Standalone/$name.app"; do
        id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || echo "(absent)")
        if [[ "$id" == "$bundle_id" ]]; then
            printf '%-70s CFBundleIdentifier %s\n' "$bundle" "$id"
        else
            echo "::error::$bundle CFBundleIdentifier expected '$bundle_id', got '$id'"; status=1
        fi
        if /usr/bin/grep -qi 'veil audio\|elsewhere audio' "$bundle/Contents/Info.plist"; then
            echo "::error::$bundle/Contents/Info.plist still names the old company"; status=1
        fi
    done
    moduleinfo="$artefacts/VST3/$name.vst3/Contents/Resources/moduleinfo.json"
    if [[ -f "$moduleinfo" ]]; then
        vendors=$(/usr/bin/grep -E '"Vendor"' "$moduleinfo" || true)
        if [[ -n "$vendors" ]] && ! printf '%s\n' "$vendors" | /usr/bin/grep -vq 'Windward Audio' && ! /usr/bin/grep -qi 'veil audio' "$moduleinfo"; then
            echo "$name VST3 moduleinfo.json vendor: Windward Audio"
        else
            echo "::error::$name VST3 moduleinfo.json does not name the vendor Windward Audio"; /usr/bin/grep -i vendor "$moduleinfo" || true; status=1
        fi
    else
        echo "$name VST3 moduleinfo.json: not generated (the vendor comes from the plug-in's factory, checked by the tests)"
    fi
done
exit $status
