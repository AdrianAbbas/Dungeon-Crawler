#!/usr/bin/env bash
# Runs the whole Linux test suite as concurrent shards, then the CPU-measuring group
# alone (report-only) so its timings are not disturbed. Together the shards cover
# every test group and every location.
# Usage: scripts/run-tests-parallel.sh path/to/DungeonCrawlerTests [log directory]
set -u
tests="${1:?the DungeonCrawlerTests binary}"
logs="${2:-build/test-logs}"
mkdir -p "$logs"

"$tests" --only smoke,parameters,state,layouts,transparency,editor,art,levels > "$logs/1-basics.txt" 2>&1 &
"$tests" --only rates,extremes,realtime,place,journeys,atmosphere > "$logs/2-robustness.txt" 2>&1 &
"$tests" --only locations,travel,modules,musicality > "$logs/3-locations.txt" 2>&1 &
wait
"$tests" --only cpu > "$logs/4-cpu.txt" 2>&1

passed=0
failed=0
for log in "$logs"/[1-4]-*.txt; do
    line=$(grep -E '^[0-9]+ passed, [0-9]+ failed' "$log" | tail -1)
    if [[ -z "$line" ]]; then
        echo "$log: did not finish"
        failed=$((failed + 1))
        continue
    fi
    passed=$((passed + $(echo "$line" | cut -d' ' -f1)))
    failed=$((failed + $(echo "$line" | cut -d' ' -f3)))
    grep '^FAIL' "$log"
done
grep -h '\[metric\]' "$logs"/4-cpu.txt || true
echo "$passed passed, $failed failed"
[[ "$failed" -eq 0 ]]
