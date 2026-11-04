#!/usr/bin/env python3
"""Prepares the recorded brass (Session 13) for the instrument's sampler.

The horn, trumpet, tenor trombone and tuba of VSCO 2 Community Edition (Versilian
Studios, CC0 1.0: docs/LICENSES/VSCO-2-CE.txt), sustains and staccatos, every dynamic the
library has. The library is fetched from its official source by
scripts/ci-fetch-brass.sh; nothing of it is committed to this repository. The output goes
into the same folder as the developer's prepared recordings (tools/sample-prep/prepare.py)
and is merged into its index.json, so the sampler loads both.

    python3 -I tools/sample-prep/brass.py --source <VSCO-2-CE checkout> --out build/samples

Per note (one WAV per note, dynamic and take in the library):
 1. Decode (44.1 kHz, 16-bit stereo), fold to mono, take off DC (a 20 Hz high-pass).
 2. Trim to the onset: the library's files start with up to 80 ms of room before the
    note; the note starts 2 ms before its level first reaches 40 dB under its peak (a
    1 ms fade-in over the room), so the attack is where the key is struck.
 3. Pitch: YIN within 3 semitones of the sounding note (the file names are an octave
    under it: "MOHorn_sus_C3" sounds C4), refined on the harmonics; the sampler is tuned
    to what was played. A sustain is measured over its loop; a staccato over its body
    (20-170 ms: its pitch falls as it dies). A take whose harmonics disagree about the
    pitch (a wobbling pedal note) is dropped.
 4. Sustains: the best-matching loop in the held part (crossfaded, levelled), and the
    recorded release: the hold after the loop is cut out (the loop, then a crossfade
    into the last 60 ms before the note's release and its tail).
 5. Every note ends 60 dB under its peak (a 20 ms fade).
 6. Levels: one gain per instrument for both articulations (its loudest peak at
    -1 dBFS), so the dynamics and the staccato-to-sustain balance stay as recorded. Each
    take's loudness (for the sampler's evening and velocity curve), K-weighted, is what it
    contributes as played: a sustain's held part, a staccato's hit.
 7. Velocity layers: each articulation keeps the dynamics most of its notes have; a note
    lacking one borrows the nearest dynamic it has (levelled to that layer's median,
    marked "borrowed" in the report). Equal velocity ranges.
 8. Key ranges: each recorded pitch covers up to halfway to its neighbours; the ends
    stretch 4 semitones (the instrument fades to its synthesis beyond).
 9. Output: <id>/<note>-<layer>-<take>.flac, <id>.json (the sampler's mapping, as
    prepare.py writes it), the ids appended to index.json, the licence copied to
    licenses/, brass-report.json.

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
from scipy import signal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prepare as P  # noqa: E402  (its pitch, loop and loudness helpers)

RATE = P.RATE
LICENCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "LICENSES", "VSCO-2-CE.txt")

# The sets: id, the library's folder, articulation, the dynamics kept (by the library's
# v-number, softest first), the take naming.
INSTRUMENTS = [
    ("horn", "Brass/F Horn", "French horn"),
    ("trumpet", "Brass/Trumpet", "Trumpet"),
    ("trombone", "Brass/Tenor Trombone", "Tenor trombone"),
    ("tuba", "Brass/Tuba", "Tuba"),
]
ARTICULATIONS = ("sus", "stac")
NAME = re.compile(r"_(sus|stac)_([A-G]#?)(-?\d)_v(\d)_(?:r+)?(\d+)", re.IGNORECASE)
LAYER_NAMES = ["p", "mp", "mf", "f", "ff"]


def loudness(audio, art):
    """How loud a take sounds as the instrument plays it (K-weighted, LUFS-like). A sustain:
    its loudest 400 ms in its first 0.9 s (a note as it is played: its attack and how it
    settles; rating by the held part alone stepped at layer changes). A staccato: its hit and
    body (its loudest 150 ms in the first 350 ms: it dies away under the sustain). The
    sampler levels every take onto its velocity curve by this."""
    y = P.k_weight(audio[:int(1.4 * RATE)])
    lufs = lambda p: float(-0.691 + 10 * np.log10(max(p, 0.0) + 1e-24))
    def loudest(seg, win):
        if len(seg) <= win:
            return lufs(np.mean(seg ** 2))
        total = np.cumsum(np.concatenate(([0.0], seg ** 2)))
        return lufs(((total[win:] - total[:-win]) / win).max())
    if art != "sus":
        return loudest(y[:int(0.35 * RATE)], int(0.15 * RATE))
    return loudest(y[:int(0.9 * RATE)], int(0.4 * RATE))


def parse(fn):
    m = NAME.search(fn)
    if not m:
        return None
    art, pc, octave, v, take = m.groups()
    # An octave under the sounding pitch (checked against the audio below).
    label = P.midi_of(f"{pc.upper()}{octave}") + 12
    return art.lower(), label, int(v), int(take)


def load(path):
    x, sr = sf.read(path, always_2d=True)
    if sr != RATE:
        raise SystemExit(f"{path}: {sr} Hz (expected {RATE})")
    y = x.mean(axis=1)
    return signal.sosfiltfilt(signal.butter(2, 20, "highpass", fs=RATE, output="sos"), y)


def trim_onset(y):
    """The note from 2 ms before its level first reaches 40 dB under its peak."""
    hop = 44
    n = len(y) // hop
    env = P.db(np.sqrt((y[:n * hop].reshape(n, hop) ** 2).mean(axis=1)))
    peak = env.max()
    first = int(np.argmax(env > peak - 40))
    start = max(0, first * hop - int(0.002 * RATE))
    out = y[start:].copy()
    fade = int(0.001 * RATE)
    out[:fade] *= np.linspace(0.0, 1.0, fade)
    return out, start


def trim_tail(y):
    """Ends 60 dB under the peak, with a 20 ms fade."""
    env = P.envelope(y)
    if len(env) == 0:
        return y
    above = np.nonzero(env > env.max() - 60)[0]
    end = min(len(y), (int(above[-1]) + 2) * P.ENV_HOP)
    out = y[:end].copy()
    fade = min(len(out) // 4, int(0.02 * RATE))
    if fade > 0:
        out[-fade:] *= np.linspace(1.0, 0.0, fade)
    return out


def periodic_pitch(seg, midi):
    """The pitch that best lines the stretch up with itself about k periods later (the
    normalised autocorrelation's peak near k periods, k so the lag is near 1000 samples,
    within half a period's worth of it, refined by a parabola): a staccato's pitch over its
    body, as the instrument's tests measure it."""
    period = RATE / (440 * 2 ** ((midi - 69) / 12))
    k = max(1, int(round(1000 / period)))
    expected, spread = period * k, min(0.03, 0.45 / k)
    lo, hi = int(math.floor(expected / (1 + spread))), int(math.ceil(expected * (1 + spread)))
    n = len(seg) - hi - 1
    if n < 2048 or lo < 2:
        return None
    a = seg[:n]
    taus = np.arange(lo - 1, hi + 2)
    vals = []
    for t in taus:
        b = seg[t:t + n]
        m = float(np.dot(a, a) + np.dot(b, b))
        vals.append(2 * float(np.dot(a, b)) / m if m > 0 else 0.0)
    vals = np.array(vals)
    i = 1 + int(np.argmax(vals[1:-1]))
    y0, y1, y2 = vals[i - 1], vals[i], vals[i + 1]
    shift = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2) if (y0 - 2 * y1 + y2) != 0 else 0.0
    f = RATE * k / (taus[i] + shift)
    return 69 + 12 * math.log2(f / 440.0)


def pitch(seg, label):
    if len(seg) < 4096:
        return None
    lo, hi = 440 * 2 ** ((label - 3 - 69) / 12), 440 * 2 ** ((label + 3 - 69) / 12)
    frames = P.yin(seg, fmin=lo, fmax=hi, frame=2048 if label > 40 else 4096)
    good = [f for f, ap in frames if ap < 0.3]
    if len(good) < 3:
        return None
    f0 = P.refine(seg[:int(0.5 * RATE)], float(np.median(good)), 6)
    return 69 + 12 * math.log2(f0 / 440.0)


def harmonic_spread(seg, root):
    """How far the first eight harmonics disagree about the pitch (cents, power-weighted
    standard deviation): a steady note's agree within a few cents; a pedal note that
    wobbles, or a staccato still settling, does not have one pitch to tune to."""
    if seg is None or len(seg) < 4096:
        return 0.0
    n = 1 << 19
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n))
    binhz = RATE / n
    f0 = 440 * 2 ** ((root - 69) / 12)
    cents, weights = [], []
    for k in range(1, 9):
        lo, hi = int(f0 * k * 0.97 / binhz), int(f0 * k * 1.03 / binhz) + 1
        if hi >= len(spec):
            break
        i = lo + int(np.argmax(spec[lo:hi]))
        cents.append(1200 * math.log2(max(i * binhz, 1e-9) / (f0 * k)))
        weights.append(spec[i])
    if len(cents) < 3:
        return 0.0
    w = np.array(weights)
    c = np.array(cents)
    mean = float(np.average(c, weights=w))
    return float(np.sqrt(np.average((c - mean) ** 2, weights=w)))


def sustain_note(y, label, report, where):
    """Loop and release; the hold after the loop cut out. Returns (audio, root, loop, release)."""
    region = P.sustain_region(y)
    root = None
    loop = None
    if region:
        a, b = region
        # The loop is searched in the first 3 s of the hold (the rest is cut out).
        b2 = min(b, a + int(3.0 * RATE))
        found = P.find_loop(y, 440 * 2 ** ((label - 69) / 12), (a, b2))
        if found:
            s, e, cost, _ = found
            root = pitch(y[s:e], label)
            if cost > 0.1:
                report["warnings"].append(f"{where}: the best loop matches poorly ({cost:.3f}); crossfaded")
            y = P.crossfade_loop(y, s, e, int(0.15 * RATE))
            loop = (s, e, cost)
            # Past the loop's end the note plays on as the loop would (its start again: the
            # crossfade made the loop's end lead into it), for 80 ms, then crosses (30 ms) into
            # 60 ms before the recorded release and its tail: the hold between them is cut
            # out. A key let go mid-loop reads on past the loop's end into the tail without a
            # jump (the audio right after the loop's end otherwise still continues the
            # original take, which the loop's crossfade no longer matches).
            rel = int(b)
            xf = int(0.03 * RATE)
            run = int(0.08 * RATE)
            tail = y[max(rel - int(0.06 * RATE), e):]
            cont = y[s:s + run + xf].copy()
            if len(tail) > xf and len(cont) == run + xf:
                w = np.linspace(0.0, 1.0, xf)
                cont[-xf:] = cont[-xf:] * (1 - w) + tail[:xf] * w
                y = np.concatenate([y[:e], cont, tail[xf:]])
                release = e + run   # the crossfade's start: the voice crosses from here
            else:
                y = np.concatenate([y[:e], y[s:]])
                release = -1
            return y, root, loop, release
    report["problems"].append(f"{where}: no sustain loop found (plays as recorded, no loop)")
    seg = y[int(0.3 * RATE):int(1.5 * RATE)]
    return y, pitch(seg, label), None, -1


def process(source, inst, folder, art, report):
    files = sorted(f for f in os.listdir(os.path.join(source, folder, art)) if f.lower().endswith(".wav"))
    notes = []
    for fn in files:
        parsed = parse(fn)
        where = f"{inst}-{art} {fn}"
        if parsed is None or parsed[0] != art:
            report["problems"].append(f"{where}: name not understood (skipped)")
            continue
        _, label, v, take = parsed
        y, start = trim_onset(load(os.path.join(source, folder, art, fn)))
        entry = {"file": f"{folder}/{art}/{fn}", "label": P.name_of(label), "v": v, "take": take, "trimmedMs": round(1000 * start / RATE, 1)}
        if art == "sus":
            y, root, loop, release = sustain_note(y, label, report, where)
            if loop:
                entry["loop"] = f"{loop[0] / RATE:.2f}-{loop[1] / RATE:.2f}s (match {loop[2]:.3f})"
        else:
            # A staccato is tuned by its body (20-170 ms): its pitch falls as it dies, and in
            # the instrument it sounds only as the hit (it decays under the sustain).
            body = y[int(0.02 * RATE):int(0.17 * RATE)]
            root = pitch(body, label)
            # Refined (or, where YIN found nothing steady, measured) by the periodic estimate.
            fine = periodic_pitch(body, root if root is not None else float(label))
            if fine is not None and abs(fine - (root if root is not None else float(label))) < 0.5:
                root = fine
            loop, release = None, -1
        if root is None:
            report["warnings"].append(f"{where}: pitch not measurable; mapped at its label {P.name_of(label)}")
            root = float(label)
        cents = 100 * (root - label)
        entry["cents"] = round(cents, 1)
        if abs(cents) > 50:
            report["problems"].append(f"{where}: sounds {cents:+.0f} cents from {P.name_of(label)}; mapped where it sounds")
        elif abs(cents) > 25:
            report["warnings"].append(f"{where}: {cents:+.0f} cents out (the sampler plays it in tune)")
        # A take with no single pitch (its harmonics disagree) is dropped when the note has
        # other takes or neighbours (checked below, per set).
        body = y[loop[0]:loop[1]] if loop else y[int(0.02 * RATE):int(0.17 * RATE)]
        entry["spread"] = round(harmonic_spread(body, root), 1)
        y = trim_tail(y) if loop is None else y
        if loop is not None:
            y = trim_tail(y)
            if release >= len(y) - 64:
                release = -1
        notes.append({"label": label, "root": float(root), "v": v, "take": take, "audio": y,
                      "loop": (loop[0], loop[1]) if loop else None, "release": release, "entry": entry})
        report["files"].append(entry)
    # Unpitched takes out: a sustain whose harmonics spread more than 15 cents, a staccato
    # more than 25 (its body still settling); the note is then played from its neighbours or
    # its other takes.
    limit = 15.0 if art == "sus" else 25.0
    kept = []
    for n in notes:
        if n["entry"]["spread"] > limit:
            report["problems"].append(f"{inst}-{art} {n['entry']['file']}: dropped, no single pitch (its harmonics spread {n['entry']['spread']:.0f} cents)")
            continue
        kept.append(n)
    return kept


def layer_plan(notes):
    """The dynamics kept: those that at least half the notes have, softest first."""
    per = {}
    for n in notes:
        per.setdefault(n["label"], set()).add(n["v"])
    counts = {}
    for vs in per.values():
        for v in vs:
            counts[v] = counts.get(v, 0) + 1
    keep = sorted(v for v, c in counts.items() if c >= max(1, len(per) // 2))
    return keep


def write_set(sid, name, notes, keep, gain, out_dir, report):
    os.makedirs(os.path.join(out_dir, sid), exist_ok=True)
    layers = len(keep)
    names = LAYER_NAMES[-layers:] if layers <= 2 else LAYER_NAMES[:layers]
    if layers == 2:
        names = ["p", "f"]
    zones = []
    dropped = [n for n in notes if n["v"] not in keep]
    for n in dropped:
        report["warnings"].append(f"{sid} {P.name_of(n['label'])}: dynamic v{n['v']} not kept (only this note has it)")
    kept = [n for n in notes if n["v"] in keep]
    for n in kept:
        n["layer"] = keep.index(n["v"])
        n["audio"] = n["audio"] * gain
        n["loudness"] = loudness(n["audio"], "stac" if sid.endswith("-stac") else "sus")
    medians = {}
    for li in range(layers):
        lv = [n["loudness"] for n in kept if n["layer"] == li]
        medians[li] = float(np.median(lv)) if lv else -30.0
    # Notes lacking a dynamic borrow the nearest one they have, levelled to that layer.
    labels = sorted({n["label"] for n in kept})
    borrowed = []
    for label in labels:
        have = {n["layer"] for n in kept if n["label"] == label}
        for li in range(layers):
            if li in have:
                continue
            src = min(have, key=lambda h: (abs(h - li), -h))
            for n in [q for q in kept if q["label"] == label and q["layer"] == src]:
                shift = float(np.clip(medians[li] - medians[src], -12.0, 12.0))
                b = dict(n)
                b["layer"] = li
                b["audio"] = n["audio"] * 10 ** (shift / 20)
                b["loudness"] = n["loudness"] + shift
                b["borrowedFrom"] = names[src]
                borrowed.append(b)
                report["warnings"].append(f"{sid} {P.name_of(label)}: no {names[li]} take; borrows its {names[src]} ({shift:+.1f} dB)")
    allz = kept + borrowed
    # Takes numbered per (note, layer).
    for key in {(z["label"], z["layer"]) for z in allz}:
        for i, z in enumerate(sorted([z for z in allz if (z["label"], z["layer"]) == key], key=lambda z: z["take"])):
            z["rr"] = i
    peak_after = max(float(np.abs(z["audio"]).max()) for z in allz)
    if peak_after > 0.99:
        raise SystemExit(f"{sid}: a borrowed take would clip ({P.db(peak_after):.1f} dBFS)")
    for z in allz:
        roots = sorted({round(q["root"], 1) for q in allz if q["layer"] == z["layer"]})
        r = round(z["root"], 1)
        lower = [q for q in roots if q < r - 0.5]
        upper = [q for q in roots if q > r + 0.5]
        lo = math.floor((max(lower) + r) / 2) + 1 if lower else int(round(r)) - 4
        hi = math.floor((min(upper) + r) / 2) if upper else int(round(r)) + 4
        fn = f"{P.name_of(z['label']).replace('#', 's')}-{names[z['layer']]}-{z['rr'] + 1}.flac"
        sf.write(os.path.join(out_dir, sid, fn), z["audio"].astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
        a = z["audio"]
        env_z = P.envelope(a[:int(1.5 * RATE)])
        top = int(np.argmax(env_z)) * P.ENV_HOP if len(env_z) else 0
        head = a[top:top + int(0.3 * RATE)]
        mz = {"file": f"{sid}/{fn}", "label": P.name_of(z["label"]), "root": round(z["root"], 3), "loKey": lo, "hiKey": hi,
              "loVel": round(z["layer"] / layers, 4), "hiVel": round((z["layer"] + 1) / layers, 4) if z["layer"] < layers - 1 else 1.0,
              "layer": z["layer"], "rr": z["rr"], "length": len(a),
              "rms": round(float(P.db(np.sqrt(np.mean(head ** 2) + 1e-24))), 2), "loudness": round(z["loudness"], 2),
              "peak": round(float(P.db(np.abs(a).max())), 2), "source": z["entry"]["file"], "at": 0.0}
        if "borrowedFrom" in z:
            mz["borrowed"] = z["borrowedFrom"]
        if z["loop"] is not None:
            mz["loopStart"], mz["loopEnd"] = int(z["loop"][0]), int(z["loop"][1])
            if z["release"] > 0:
                mz["release"] = int(z["release"])
        zones.append(mz)
    zones.sort(key=lambda m: (m["root"], m["layer"], m["rr"]))
    mapping = {"id": sid, "name": name, "kind": "brass", "rate": RATE, "channels": 1, "layers": names,
               "gainDb": round(20 * math.log10(gain), 2), "zones": zones}
    with open(os.path.join(out_dir, f"{sid}.json"), "w") as f:
        json.dump(mapping, f, indent=1)
    return mapping


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--source", required=True, help="the library's checkout (its Brass folder)")
    ap.add_argument("--out", required=True, help="the prepared samples folder (merged into its index.json)")
    ap.add_argument("--commit", default="", help="the library's commit (for the report)")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    report = {"source": "VSCO 2 Community Edition (Versilian Studios), CC0 1.0", "commit": args.commit,
              "files": [], "problems": [], "warnings": [], "sets": {}}
    ids = []
    for inst, folder, label in INSTRUMENTS:
        per_art = {art: process(args.source, inst, folder, art, report) for art in ARTICULATIONS}
        # One gain for both articulations: the loudest peak at -1 dBFS.
        peak = max(float(np.abs(n["audio"]).max()) for ns in per_art.values() for n in ns)
        gain = 10 ** (-1 / 20) / max(peak, 1e-9)
        for art, notes in per_art.items():
            sid = f"brass-{inst}-{art}"
            keep = layer_plan(notes)
            mapping = write_set(sid, f"{label} ({'sustain' if art == 'sus' else 'staccato'})", notes, keep, gain, args.out, report)
            ids.append(sid)
            z = mapping["zones"]
            report["sets"][sid] = {"zones": len(z), "layers": mapping["layers"], "keys": f"{min(q['loKey'] for q in z)}-{max(q['hiKey'] for q in z)}",
                                   "loops": sum(1 for q in z if "loopStart" in q), "seconds": round(sum(q["length"] for q in z) / RATE, 1),
                                   "gainDb": mapping["gainDb"]}
    # Merged into the samples folder's index (the developer's recordings may already be there).
    index_path = os.path.join(args.out, "index.json")
    index = {"version": 1, "instruments": []}
    if os.path.exists(index_path):
        with open(index_path) as f:
            index = json.load(f)
    index["instruments"] = [i for i in index.get("instruments", []) if not str(i).startswith("brass-")] + ids
    with open(index_path, "w") as f:
        json.dump(index, f, indent=1)
    os.makedirs(os.path.join(args.out, "licenses"), exist_ok=True)
    shutil.copyfile(LICENCE, os.path.join(args.out, "licenses", "brass-samples-CC0.txt"))
    total = sum(os.path.getsize(os.path.join(args.out, sid, fn)) for sid in ids for fn in os.listdir(os.path.join(args.out, sid)))
    report["flacBytes"] = total
    with open(os.path.join(args.out, "brass-report.json"), "w") as f:
        json.dump(report, f, indent=1)
    for e in report["files"]:
        print(f"{e['file']}: {e['label']} v{e['v']} take {e['take']}, {e['cents']:+.1f} cents, onset trimmed {e['trimmedMs']} ms" + (f", loop {e['loop']}" if "loop" in e else ""))
    print()
    for sid, s in report["sets"].items():
        print(f"Prepared: {s['zones']} notes  {sid}  (layers {'/'.join(s['layers'])}, keys {s['keys']}, {s['loops']} loops, {s['seconds']} s, gain {s['gainDb']:+.1f} dB)")
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
