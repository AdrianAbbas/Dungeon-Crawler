#!/usr/bin/env python3
"""Prepares the recorded woodwinds (Session 17) for the instrument's sampler.

The flute, piccolo, oboe, clarinet and bassoon of VSCO 2 Community Edition (Versilian
Studios, CC0 1.0: docs/LICENSES/VSCO-2-CE.txt), the library the brass already uses
(tools/sample-prep/brass.py, Session 13): sustains without vibrato, sustains with vibrato
where the library has them, staccatos, every dynamic it has. Fetched from its official
source by scripts/ci-fetch-brass.sh (the same pin); nothing of it is committed to this
repository. The output goes into the same samples folder and is merged into its index.json.

    python3 -I tools/sample-prep/woodwinds.py --source <VSCO-2-CE checkout> --out build/samples

The steps are brass.py's (its helpers are used), with these differences:
 - The file names differ per instrument (LDFlute_susNV_C4_v1_1, piccolo_C5_sustain1,
   Oboe_Sus_A#2_v1_Main, DCClar_susLong_D2_v1_rr1_sum, PSBassoon_A#0_v1_1); the dynamic is
   the v-number (none on the piccolo: one dynamic), the take the rr- or trailing number.
 - The octave of a name is checked against the audio, per instrument (most are named an
   octave under where they sound, as the brass is): a broad pitch estimate of every take,
   the octave offset most takes agree on.
 - A sustain's loop is searched in its first 4 s of hold (the takes are 5-11 s long); a
   vibrato sustain the same way (its loop then holds several vibrato cycles).
 - A staccato is tuned by its body (20-130 ms: a woodwind staccato is shorter than a brass
   one) and keeps its natural end (60 dB under its peak).
 - Sets: ww-<instrument>-<sus|vib|stac>, one gain per instrument for all of them (its loudest
   peak at -1 dBFS), so the dynamics and the articulations' balance stay as recorded.

Needs numpy, scipy and soundfile.
"""

import argparse
import json
import math
import os
import re
import shutil
import sys

import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prepare as P  # noqa: E402
import brass as B    # noqa: E402

RATE = P.RATE
LICENCE = B.LICENCE

# id, the library's folder, name, and the articulations: (kind, sub-folder).
INSTRUMENTS = [
    ("flute", "Woodwinds/Flute", "Flute", [("sus", "susNV"), ("vib", "susvib"), ("stac", "stac")]),
    ("piccolo", "Woodwinds/Piccolo", "Piccolo", [("sus", "Sus"), ("stac", "Stac")]),
    ("oboe", "Woodwinds/Oboe", "Oboe", [("sus", "Sus"), ("vib", "Vib"), ("stac", "Stacc")]),
    ("clarinet", "Woodwinds/Clarinet", "Clarinet", [("sus", "susLong"), ("stac", "stac")]),
    ("bassoon", "Woodwinds/Bassoon", "Bassoon", [("sus", "sus"), ("vib", "vib"), ("stac", "stac")]),
]
NOTE = re.compile(r"_([A-G]#?)(-?\d)(?:_|$)")
DYN = re.compile(r"_v(\d)(?:_|$)")
TAKE = re.compile(r"(?:_rr(\d+)|_(\d+)$|(?:staccato|sustain)(\d+)$)")


def parse(fn):
    stem = os.path.splitext(fn)[0]
    m = NOTE.search(stem)
    if not m:
        return None
    pc, octave = m.groups()
    d = DYN.search(stem)
    t = TAKE.search(stem)
    take = int(next(g for g in t.groups() if g is not None)) if t else 1
    return P.midi_of(f"{pc.upper()}{octave}"), int(d.group(1)) if d else 1, take


def broad_pitch(y):
    """A rough pitch (MIDI) of a take's held part, without a guess of where it is."""
    seg = y[int(0.15 * RATE):int(1.2 * RATE)]
    if len(seg) < 8192:
        seg = y[:int(0.6 * RATE)]
    frames = P.yin(seg, fmin=40.0, fmax=3000.0, frame=2048, hop=1024)
    good = [f for f, ap in frames if ap < 0.25]
    if len(good) < 3:
        return None
    return 69 + 12 * math.log2(float(np.median(good)) / 440.0)


def octave_offset(source, folder, arts, report, inst):
    """The octave between the names and the sound, most takes agreeing."""
    votes = {}
    for _, sub in arts:
        d = os.path.join(source, folder, sub)
        for fn in sorted(os.listdir(d))[::3]:
            parsed = parse(fn)
            if not parsed:
                continue
            y, _ = B.trim_onset(B.load(os.path.join(d, fn)))
            p = broad_pitch(y)
            if p is None:
                continue
            off = 12 * int(round((p - parsed[0]) / 12.0))
            votes[off] = votes.get(off, 0) + 1
    if not votes:
        raise SystemExit(f"{inst}: no take with a measurable pitch")
    off = max(votes, key=votes.get)
    report["octaves"][inst] = {"offset": off, "votes": votes}
    return off


def process(source, inst, folder, kind, sub, offset, report):
    d = os.path.join(source, folder, sub)
    files = sorted(f for f in os.listdir(d) if f.lower().endswith(".wav"))
    notes = []
    for fn in files:
        parsed = parse(fn)
        where = f"{inst}-{kind} {fn}"
        if parsed is None:
            report["problems"].append(f"{where}: name not understood (skipped)")
            continue
        named, v, take = parsed
        label = named + offset
        y, start = B.trim_onset(B.load(os.path.join(d, fn)))
        entry = {"file": f"{folder}/{sub}/{fn}", "label": P.name_of(label), "v": v, "take": take, "trimmedMs": round(1000 * start / RATE, 1)}
        loop, release = None, -1
        if kind in ("sus", "vib"):
            region = P.sustain_region(y)
            root = None
            if region:
                a, b = region
                found = P.find_loop(y, 440 * 2 ** ((label - 69) / 12), (a, min(b, a + int(4.0 * RATE))))
                if found:
                    s, e, cost, _ = found
                    # A vibrato take's centre over its first held seconds (0.4-2.0 s: what is heard
                    # first; its loop lies within), a plain sustain's over its loop.
                    root = B.pitch(y[int(0.4 * RATE):int(2.0 * RATE)] if kind == "vib" else y[s:e], label)
                    if cost > 0.1:
                        report["warnings"].append(f"{where}: the best loop matches poorly ({cost:.3f}); crossfaded")
                    y = P.crossfade_loop(y, s, e, int(0.15 * RATE))
                    loop = (s, e, cost)
                    # As brass.py: past the loop's end the note plays on as the loop would,
                    # then crosses into 60 ms before the recorded release and its tail.
                    rel, xf, run = int(b), int(0.03 * RATE), int(0.08 * RATE)
                    tail = y[max(rel - int(0.06 * RATE), e):]
                    cont = y[s:s + run + xf].copy()
                    if len(tail) > xf and len(cont) == run + xf:
                        w = np.linspace(0.0, 1.0, xf)
                        cont[-xf:] = cont[-xf:] * (1 - w) + tail[:xf] * w
                        y = np.concatenate([y[:e], cont, tail[xf:]])
                        release = e + run
                    else:
                        y = np.concatenate([y[:e], y[s:]])
            if loop is None:
                report["problems"].append(f"{where}: no sustain loop found (plays as recorded)")
                root = B.pitch(y[int(0.3 * RATE):int(1.5 * RATE)], label)
            entry["loop"] = f"{loop[0] / RATE:.2f}-{loop[1] / RATE:.2f}s (match {loop[2]:.3f})" if loop else "none"
            body = y[loop[0]:loop[1]] if loop else y[int(0.3 * RATE):int(1.5 * RATE)]
        else:
            # Tuned by what sounds of it as the tongued onset (20-130 ms; the tests' window).
            body = y[int(0.02 * RATE):int(0.13 * RATE)]
            root = B.pitch(y[int(0.02 * RATE):int(0.2 * RATE)], label)
            fine = B.periodic_pitch(body, root if root is not None else float(label))
            if fine is not None and abs(fine - (root if root is not None else float(label))) < 0.5:
                root = fine
        if root is None:
            report["warnings"].append(f"{where}: pitch not measurable; mapped at its label {P.name_of(label)}")
            root = float(label)
        cents = 100 * (root - label)
        entry["cents"] = round(cents, 1)
        if abs(cents) > 50:
            report["problems"].append(f"{where}: sounds {cents:+.0f} cents from {P.name_of(label)}; mapped where it sounds")
        elif abs(cents) > 25:
            report["warnings"].append(f"{where}: {cents:+.0f} cents out (the sampler plays it in tune)")
        # A vibrato take's harmonics move together; its spread is measured but not held to
        # the plain sustain's limit.
        entry["spread"] = round(B.harmonic_spread(body, root), 1)
        y = B.trim_tail(y)
        if release >= len(y) - 64:
            release = -1
        notes.append({"label": label, "root": float(root), "v": v, "take": take, "audio": y,
                      "loop": (loop[0], loop[1]) if loop else None, "release": release, "entry": entry})
        report["files"].append(entry)
    limit = {"sus": 15.0, "vib": 40.0, "stac": 25.0}[kind]
    kept = []
    for n in notes:
        if n["entry"]["spread"] > limit and len(notes) > 4:
            report["problems"].append(f"{inst}-{kind} {n['entry']['file']}: dropped, no single pitch (its harmonics spread {n['entry']['spread']:.0f} cents)")
            continue
        kept.append(n)
    return kept


def rank_dynamics(notes, where, report):
    """The dynamics by rank: the library numbers them per note (a flute note recorded p and
    mf is v1 and v2, its neighbour p and f v1 and v3). The layer count most notes have is
    kept, and each note's dynamics, softest first, take those layers' numbers (a note with
    fewer borrows the nearest when the set is written)."""
    per = {}
    for n in notes:
        per.setdefault(n["label"], set()).add(n["v"])
    counts = {}
    for vs in per.values():
        counts[len(vs)] = counts.get(len(vs), 0) + 1
    layers = max(counts, key=lambda c: (counts[c], c))
    plan = sorted({v for vs in per.values() if len(vs) == layers for v in vs})
    if len(plan) != layers:
        plan = list(range(1, layers + 1))
    for label, vs in per.items():
        order = sorted(vs)
        if len(order) == layers:
            remap = dict(zip(order, plan))
        else:
            # Fewer (or more) dynamics: each to the plan's dynamic nearest by its number (a
            # note recorded only loud stays loud).
            remap = {v: min(plan, key=lambda q: (abs(q - v), q)) for v in order}
        for n in notes:
            if n["label"] == label and remap.get(n["v"], n["v"]) != n["v"]:
                report["warnings"].append(f"{where} {P.name_of(label)}: its v{n['v']} plays as v{remap[n['v']]} (dynamics by rank)")
                n["entry"]["layerFrom"] = n["v"]
                n["v"] = remap[n["v"]]
    return notes


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--source", required=True, help="the library's checkout (its Woodwinds folder)")
    ap.add_argument("--out", required=True, help="the prepared samples folder (merged into its index.json)")
    ap.add_argument("--commit", default="", help="the library's commit (for the report)")
    ap.add_argument("--only", default="", help="comma-separated instrument ids (development)")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    report = {"source": "VSCO 2 Community Edition (Versilian Studios), CC0 1.0", "commit": args.commit,
              "files": [], "problems": [], "warnings": [], "sets": {}, "octaves": {}}
    ids = []
    only = [s for s in args.only.split(",") if s]
    for inst, folder, label, arts in INSTRUMENTS:
        if only and inst not in only:
            continue
        offset = octave_offset(args.source, folder, arts, report, inst)
        per = {kind: process(args.source, inst, folder, kind, sub, offset, report) for kind, sub in arts}
        peak = max(float(np.abs(n["audio"]).max()) for ns in per.values() for n in ns)
        gain = 10 ** (-1 / 20) / max(peak, 1e-9)
        for kind, notes in per.items():
            if not notes:
                report["problems"].append(f"{inst}-{kind}: no notes")
                continue
            sid = f"ww-{inst}-{kind}"
            notes = rank_dynamics(notes, f"{inst}-{kind}", report)
            keep = B.layer_plan(notes)
            # brass.py rates a take by its articulation ("stac" or a sustain).
            title = f"{label} ({ {'sus': 'sustain', 'vib': 'vibrato', 'stac': 'staccato'}[kind] })"
            # A note lacking a dynamic borrows its nearest, raised to that layer's loudness: if
            # that would clip, the set is written a little lower (the sampler levels every take
            # by its loudness, so the set's own gain does not change what is heard).
            set_gain = gain
            for _ in range(6):
                try:
                    mapping = B.write_set(sid, title, [dict(n) for n in notes], keep, set_gain, args.out, report)
                    break
                except SystemExit as e:
                    over = re.search(r"\(([-\d.]+) dBFS\)", str(e))
                    set_gain *= 10 ** (-(float(over.group(1)) + 1.5 if over else 6.0) / 20)
                    report["warnings"].append(f"{sid}: written {20 * math.log10(set_gain / gain):+.1f} dB lower (a borrowed take would clip)")
            else:
                raise SystemExit(f"{sid}: borrowed takes clip")
            mapping["kind"] = "woodwind"
            mapping["octaveOffset"] = offset
            with open(os.path.join(args.out, f"{sid}.json"), "w") as f:
                json.dump(mapping, f, indent=1)
            ids.append(sid)
            z = mapping["zones"]
            report["sets"][sid] = {"zones": len(z), "layers": mapping["layers"], "keys": f"{min(q['loKey'] for q in z)}-{max(q['hiKey'] for q in z)}",
                                   "roots": f"{min(q['root'] for q in z):.1f}-{max(q['root'] for q in z):.1f}",
                                   "loops": sum(1 for q in z if "loopStart" in q), "seconds": round(sum(q["length"] for q in z) / RATE, 1),
                                   "gainDb": mapping["gainDb"]}
    index_path = os.path.join(args.out, "index.json")
    index = {"version": 1, "instruments": []}
    if os.path.exists(index_path):
        with open(index_path) as f:
            index = json.load(f)
    index["instruments"] = [i for i in index.get("instruments", []) if not str(i).startswith("ww-")] + ids
    with open(index_path, "w") as f:
        json.dump(index, f, indent=1)
    os.makedirs(os.path.join(args.out, "licenses"), exist_ok=True)
    shutil.copyfile(LICENCE, os.path.join(args.out, "licenses", "woodwind-samples-CC0.txt"))
    total = sum(os.path.getsize(os.path.join(args.out, sid, fn)) for sid in ids for fn in os.listdir(os.path.join(args.out, sid)))
    report["flacBytes"] = total
    with open(os.path.join(args.out, "woodwinds-report.json"), "w") as f:
        json.dump(report, f, indent=1)
    for e in report["files"]:
        print(f"{e['file']}: {e['label']} v{e['v']} take {e['take']}, {e['cents']:+.1f} cents, spread {e['spread']}, onset trimmed {e['trimmedMs']} ms" + (f", loop {e['loop']}" if "loop" in e else ""))
    print()
    for inst, o in report["octaves"].items():
        print(f"Octave: {inst} sounds {o['offset']:+d} semitones from its names (votes {o['votes']})")
    for sid, s in report["sets"].items():
        print(f"Prepared: {s['zones']} notes  {sid}  (layers {'/'.join(s['layers'])}, keys {s['keys']}, roots {s['roots']}, {s['loops']} loops, {s['seconds']} s, gain {s['gainDb']:+.1f} dB)")
    print(f"FLAC total: {total / 1e6:.2f} MB")
    print(f"\nProblems ({len(report['problems'])}):" if report["problems"] else "\nNo problems.")
    for p in report["problems"]:
        print(f" - {p}")
    if report["warnings"]:
        print(f"\nWarnings ({len(report['warnings'])}):")
        for w in report["warnings"]:
            print(f" - {w}")


if __name__ == "__main__":
    main()
