#!/usr/bin/env bash
# Session 13: the recorded brass. Fetches the horn, trumpet, tenor trombone and tuba of
# VSCO 2 Community Edition (Versilian Studios) from its official source at a pinned commit,
# checks that its licence is still CC0 1.0 (docs/LICENSES/VSCO-2-CE.txt), and prepares the
# notes into the samples folder (tools/sample-prep/brass.py), merged with the developer's
# recordings when those were fetched first (scripts/ci-fetch-samples.sh). Nothing of the
# library is committed to this repository. The checkout is kept between runs (CI caches
# the directory; a complete checkout is not fetched again).
# Usage: scripts/ci-fetch-brass.sh <checkout dir> <prepared samples dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
checkout="${1:?checkout directory}"
out="${2:?prepared samples directory}"
source_url="https://github.com/sgossner/VSCO-2-CE.git"
commit="440300901dfe9275fd84e0b7763af1f8443ae62e"   # master, 2020-08-04 (library version 1.1.0)
licence_sha256="36ffd9dc085d529a7e60e1276d73ae5a030b020313e6c5408593a6ae2af39673"
paths=("/LICENSE" "/Readme.txt" "/Brass/F Horn/sus/" "/Brass/F Horn/stac/" "/Brass/Trumpet/sus/" "/Brass/Trumpet/stac/"
       "/Brass/Tenor Trombone/sus/" "/Brass/Tenor Trombone/stac/" "/Brass/Tuba/sus/" "/Brass/Tuba/stac/")

if [[ -f "$checkout/.complete-$commit" ]]; then
    echo "Brass library: cached checkout of $commit"
else
    rm -rf "$checkout"
    git init -q "$checkout"
    git -C "$checkout" remote add origin "$source_url"
    git -C "$checkout" config core.sparseCheckout true
    printf '%s\n' "${paths[@]}" > "$checkout/.git/info/sparse-checkout"
    fetched=0
    for attempt in 1 2 3 4; do
        if git -C "$checkout" fetch -q --depth 1 --filter=blob:none origin "$commit"; then fetched=1; break; fi
        echo "fetch attempt $attempt failed; retrying"
        sleep $((2 ** attempt))
    done
    if [[ "$fetched" != 1 ]]; then
        echo "::error title=Brass library fetch failed::Could not fetch $source_url at $commit"
        exit 1
    fi
    git -C "$checkout" checkout -q FETCH_HEAD
    touch "$checkout/.complete-$commit"
fi

# The licence must still be the CC0 1.0 text recorded in docs/LICENSES/VSCO-2-CE.txt.
python3 -I - "$checkout/LICENSE" "$licence_sha256" <<'PY'
import hashlib, sys
data = open(sys.argv[1], "rb").read()
digest = hashlib.sha256(data).hexdigest()
if digest != sys.argv[2] or not data.startswith(b"CC0 1.0 Universal"):
    print(f"::error title=Brass library licence changed::LICENSE is not the recorded CC0 1.0 text (sha256 {digest})")
    sys.exit(1)
print("Brass library licence: CC0 1.0 Universal (sha256 matches docs/LICENSES/VSCO-2-CE.txt)")
PY
echo "Brass library: $(find "$checkout/Brass" -iname '*.wav' | wc -l | tr -d ' ') WAVs at $commit"

python3 -m pip install --quiet -r "$here/tools/sample-prep/requirements.txt"
mkdir -p "$out"
python3 -I "$here/tools/sample-prep/brass.py" --source "$checkout" --out "$out" --commit "$commit" > "$out.brass.log"
grep -E '^Prepared: |^FLAC total' "$out.brass.log"
{ sed -n '/^Problems/,/^Warnings/p' "$out.brass.log" || true; } | head -20
test -f "$out/index.json"
test -f "$out/licenses/brass-samples-CC0.txt"
# Every later check requires the recorded brass.
[[ -n "${GITHUB_ENV:-}" ]] && echo "DC_REQUIRE_BRASS=1" >> "$GITHUB_ENV"
exit 0
