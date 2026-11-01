#!/usr/bin/env bash
# Sign the built bundles ad hoc (whole bundle, --deep), verify them, and stage
# dist/DungeonCrawler-macOS-Universal.zip with both plug-ins (DUNGEON CRAWLER, the
# instrument, and DUNGEON CRAWLER FX, the effect: AU and VST3 each) and the install scripts.
# Usage: scripts/package-mac.sh <build dir with DungeonCrawler_artefacts and DungeonCrawlerFX_artefacts> <dist dir> [config]
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$1"
dist="$2"
config="${3:-Release}"
stage="$dist/DungeonCrawler-macOS-Universal"
rm -rf "$stage" "$dist/DungeonCrawler-macOS-Universal.zip"
mkdir -p "$stage"

for name in DungeonCrawler DungeonCrawlerFX; do
    artefacts="$build/${name}_artefacts/$config"
    for bundle in "$artefacts/AU/$name.component" "$artefacts/VST3/$name.vst3" "$artefacts/Standalone/$name.app"; do
        # Developer ID signing (optional CI path) runs before this script; keep it.
        if ! /usr/bin/codesign -dv --verbose=2 "$bundle" 2>&1 | /usr/bin/grep -q "Authority=Developer ID Application"; then
            /usr/bin/codesign --force --deep --sign - --timestamp=none "$bundle"
        fi
        /usr/bin/codesign --verify --strict --verbose=4 "$bundle"
        /usr/bin/codesign -dv --verbose=2 "$bundle" 2>&1 | /usr/bin/grep -E "^(Identifier|Format|Signature|Authority|Sealed Resources)"
    done
    /usr/bin/ditto "$artefacts/AU/$name.component" "$stage/$name.component"
    /usr/bin/ditto "$artefacts/VST3/$name.vst3" "$stage/$name.vst3"
done
install -m 755 "$here/scripts/install-mac.sh" "$here/scripts/uninstall-mac.sh" "$here/scripts/diagnose-mac.sh" "$stage/"
install -m 644 "$here/packaging/INSTALL.txt" "$stage/"
# ditto keeps permissions and bundle metadata, unlike a plain zip of the artifact.
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$stage" "$dist/DungeonCrawler-macOS-Universal.zip"
echo "Packaged $dist/DungeonCrawler-macOS-Universal.zip"
/usr/bin/unzip -l "$dist/DungeonCrawler-macOS-Universal.zip" | /usr/bin/grep -E "install|INSTALL|MacOS/DungeonCrawler(FX)?$"
