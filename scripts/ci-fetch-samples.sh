#!/usr/bin/env bash
# (Copied from WORLDBUILDER.) Fetches the developer's recordings from the private AdrianAbbas/Samples repository and
# prepares them for the sampler (tools/sample-prep/prepare.py). The token comes from the
# repository secret SAMPLES_TOKEN (a fine-grained token with read access to that one
# repository); it is passed as an HTTP header, never in a URL or on disk.
# Usage: scripts/ci-fetch-samples.sh <checkout dir> <prepared output dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
checkout="${1:?checkout directory}"
out="${2:?output directory}"
repo="${SAMPLES_REPOSITORY:-AdrianAbbas/Samples}"

if [[ -z "${SAMPLES_TOKEN:-}" ]]; then
    echo "::error title=Samples token missing::The repository secret SAMPLES_TOKEN is not set, so the private recordings ($repo) cannot be fetched. Add it under Settings > Secrets and variables > Actions (see docs/DEVLOG.md, Session 7), then re-run this workflow."
    exit 1
fi

rm -rf "$checkout" "$out"
auth=$(printf 'x-access-token:%s' "$SAMPLES_TOKEN" | base64 | tr -d '\n')
echo "::add-mask::$auth"
if ! git -c http.extraheader="AUTHORIZATION: basic $auth" clone -q --depth 1 "https://github.com/$repo.git" "$checkout"; then
    echo "::error title=Samples fetch failed::Could not clone $repo with SAMPLES_TOKEN. Check that the token has not expired and has Contents: read access to $repo."
    exit 1
fi
echo "Samples: $(git -C "$checkout" log -1 --format='%h %s') ($(find "$checkout" -maxdepth 1 -iname '*.mp3' | wc -l | tr -d ' ') MP3s, $(find "$checkout" -maxdepth 1 -iname '*.wav' | wc -l | tr -d ' ') WAVs)"

python3 -m pip install --quiet -r "$here/tools/sample-prep/requirements.txt"
python3 "$here/tools/sample-prep/prepare.py" --source "$checkout" --out "$out" | tee "$out.log"
# The effect captures (LIMINAL's capture tools) into the same folder: captures/*.wav.
python3 -I "$here/tools/captures/prepare.py" "$checkout" "$out/captures" > "$out.captures.log" 2>&1 || { tail -30 "$out.captures.log"; echo "::warning title=Captures::the effect captures could not be prepared; the instruments play without them"; }
rm -rf "$out/captures/report"
ls -la "$out/captures" || true
# The recordings themselves are not needed after preparation.
rm -rf "$checkout"
test -f "$out/index.json"
