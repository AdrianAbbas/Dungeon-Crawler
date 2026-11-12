#!/usr/bin/env bash
# Runs the whole Linux test suite as concurrent shards, then the CPU-measuring group
# alone (report-only) so its timings are not disturbed. The editor and workflow groups
# hold wall-clock checks too (a full repaint fits a 30 fps frame; Eco costs less than
# Standard), so they also run alone, after the shards: with the seventh shard
# (instruments, Direction Update 3) they failed under contention on the 4-core runner.
# So does the navigation group (Session 16).
# Together the shards cover every test group and every location (9-recordings: Session 8;
# 10-mix: Session 11; 11-brass: Session 12; 12-realbrass: Session 13; 13-seat: Session 14;
# 14-dungeonsynth: Session 15, its control's checks in 1-basics; 15-realwinds: Session 17;
# 16-dsmodern: Session 18; release, 1.0.0, in 1-basics).
# Usage: scripts/run-tests-parallel.sh path/to/DungeonCrawlerTests [log directory]
set -u
tests="${1:?the DungeonCrawlerTests binary}"
logs="${2:-build/test-logs}"
mkdir -p "$logs"

"$tests" --only smoke,parameters,state,layouts,transparency,art,levels,instrumenteditor,window,search,dungeonsynthui,release > "$logs/1-basics.txt" 2>&1 &
"$tests" --only rates,extremes,realtime,place,journeys,atmosphere,instrument,effects > "$logs/2-robustness.txt" 2>&1 &
"$tests" --only locations,travel,modules,musicality > "$logs/3-locations.txt" 2>&1 &
"$tests" --only qa > "$logs/5-qa.txt" 2>&1 &
"$tests" --only strength,like > "$logs/6-strength.txt" 2>&1 &
"$tests" --only instruments > "$logs/7-instruments.txt" 2>&1 &
# Session 8: the instruments with the new recordings and the FX's voice places.
"$tests" --only recordings > "$logs/9-recordings.txt" 2>&1 &
# Session 11: the MIX section, the reverb helpers, the brass and Punch.
"$tests" --only mix > "$logs/10-mix.txt" 2>&1 &
# Session 12: the horns that hit, LIGHT SEEKER's Punch UI.
"$tests" --only brass > "$logs/11-brass.txt" 2>&1 &
wait
# Session 13: the recorded brass (VSCO 2 CE, CC0), after the shards: as an eleventh concurrent
# shard it slowed the others enough that the navigation group's journey notice (a 2.5 s
# wall-clock timer) had gone before its check read it.
"$tests" --only realbrass > "$logs/12-realbrass.txt" 2>&1
# Session 14: the brass and the War Drums seated (alone too, for the same reason).
"$tests" --only seat > "$logs/13-seat.txt" 2>&1
# Session 15: DUNGEON SYNTH (the null test at 0 %, what each Era does, levels, clicks; alone too).
"$tests" --only dungeonsynth > "$logs/14-dungeonsynth.txt" 2>&1
# Session 18: DUNGEON SYNTH's Modern rebuilt (Classic bit-exact with Session 17; alone too).
"$tests" --only dsmodern > "$logs/16-dsmodern.txt" 2>&1
# Session 17: the recorded woodwinds (VSCO 2 CE, CC0; alone too, like the recorded brass).
"$tests" --only realwinds > "$logs/15-realwinds.txt" 2>&1
# Session 16: the navigation group joins them (its journey notice is a 2.5 s wall-clock timer;
# with the dungeon synth presets at 85-100 % the concurrent shards render more, and on the CI
# runner the notice had gone before its check read it, three runs in a row).
"$tests" --only editor,workflow,navigation > "$logs/8-timed.txt" 2>&1
"$tests" --only cpu > "$logs/4-cpu.txt" 2>&1

passed=0
failed=0
for log in "$logs"/[0-9]*-*.txt; do
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
