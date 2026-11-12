#!/usr/bin/env bash
# Sign the built bundles ad hoc (whole bundle, --deep), verify them, and stage
# dist/DungeonCrawler-macOS-Universal.zip with both plug-ins (DUNGEON CRAWLER, the
# instrument, and DUNGEON CRAWLER FX, the effect: AU and VST3 each) and the install scripts.
# That zip is the developer package (CI artifact DUNGEON-CRAWLER-dev-macOS), for testing only.
# Usage: scripts/package-mac.sh <build dir with DungeonCrawler_artefacts and DungeonCrawlerFX_artefacts> <dist dir> [config]
#
# Customer package: scripts/package-mac.sh --customer <signed, notarised "DUNGEON CRAWLER.pkg"> <dist dir>
# stages <dist dir>/DUNGEON-CRAWLER-1.0-macOS with only "DUNGEON CRAWLER.pkg" and
# "DUNGEON CRAWLER - Read Me.pdf", and zips it as <dist dir>/DUNGEON-CRAWLER-1.0-macOS.zip (the
# release asset). It refuses an unsigned package.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "${1:-}" == --customer ]]; then
    pkg="${2:?the signed DUNGEON CRAWLER.pkg}"
    dist="${3:?the dist directory}"
    if ! /usr/sbin/pkgutil --check-signature "$pkg" | /usr/bin/grep -q "Developer ID Installer"; then
        echo "error: $pkg is not signed with Developer ID Installer; no customer package" >&2
        exit 1
    fi
    customer="$dist/DUNGEON-CRAWLER-1.0-macOS"
    rm -rf "$customer" "$customer.zip"
    mkdir -p "$customer"
    /usr/bin/ditto "$pkg" "$customer/DUNGEON CRAWLER.pkg"
    install -m 644 "$here/packaging/DUNGEON CRAWLER - Read Me.pdf" "$customer/"
    # No extended attributes or resource forks (no __MACOSX/ entries in the zip). Stapled
    # tickets live inside the package file itself, so nothing is lost.
    /usr/bin/xattr -cr "$customer"
    # Exactly these two files, nothing else.
    listing=$(cd "$customer" && LC_ALL=C /bin/ls -A)
    expected=$(printf '%s\n%s' "DUNGEON CRAWLER - Read Me.pdf" "DUNGEON CRAWLER.pkg")
    [[ "$listing" == "$expected" ]] || { echo "error: unexpected files in the customer package: $listing" >&2; exit 1; }
    /usr/bin/ditto -c -k --norsrc --noextattr --noacl --keepParent "$customer" "$customer.zip"
    echo "Packaged $customer.zip"
    /usr/bin/unzip -l "$customer.zip"
    if /usr/bin/unzip -Z1 "$customer.zip" | /usr/bin/grep -vE '^DUNGEON-CRAWLER-1\.0-macOS/((DUNGEON CRAWLER\.pkg)|(DUNGEON CRAWLER - Read Me\.pdf))?$'; then
        echo "error: the customer zip holds more than the package and the Read Me" >&2; exit 1
    fi
    /usr/bin/xcrun stapler validate "$customer/DUNGEON CRAWLER.pkg"
    exit 0
fi
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
