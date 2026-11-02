"""Chromatic synth sweeps (Milestone 15b): the developer's Korg Minilogue patches.

Each recording is one patch played key by key, lowest to highest, one note at a time:
37 keys, chromatic. Two recordings per patch: short hits and long (held) hits, which
become two articulations. The keyboard's octave switch and the patch's own oscillator
octaves are not known from the file names, so every note's pitch comes from the audio;
the order of the sweep only settles octaves the detector could get wrong.

Used by prepare.py for the manifest's "kind": "synth" sets:

    {"id": ..., "kind": "synth", "file": ..., "articulation": "short" | "long",
     "patch": <group: one gain for both articulations>, "keys": 37, "loop": bool,
     "maxSeconds": ..., "endStretch": 12}

What it does, per recording:
 1. Decode (24-bit WAV, dual mono: one channel's worth), 10 Hz high-pass (DC only: the
    patches go down to 16 Hz). No noise reduction: a line input, no room.
 2. Slice by the gaps between notes: a note starts where the level rises out of a
    quieter stretch by enough (relaxed step by step until all the keys are found;
    with too many, the smallest rises go first). Each note runs to the next one.
 3. Pitch: YIN down to 14 Hz, refined on the harmonics; the sweep's order gives each
    note its key (the median offset of all notes), and each note is then measured near
    that key. Notes more than 25 cents out, or wandering, are reported.
 4. Long hits: where the key was let go (the start of the final fall), a sustain loop
    in the hold (waveform and envelope matched, so a pulsing patch keeps its pulse
    across the jump; crossfaded and levelled), and the hold beyond the loop is cut out,
    so the note is its attack, its loop and its recorded release.
 5. One gain per patch (both articulations: same input gain), loudest peak -1 dBFS.
 6. Mapping: every note on its own key; only the lowest and highest notes stretch to
    fill the ends of the range (`endStretch` semitones).
"""

import math
import os

import numpy as np
import soundfile as sf
from scipy import signal

import prepare as P

RATE = P.RATE
HOP = P.ENV_HOP           # 5 ms envelope frames


def hz(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def load_wav(path):
    audio, rate = sf.read(path, dtype="float64", always_2d=True)
    if rate != RATE:
        raise SystemExit(f"{path}: {rate} Hz (expected {RATE})")
    left, right = audio[:, 0], audio[:, -1]
    live = np.abs(left) > 1e-4
    corr = float(np.corrcoef(left[live], right[live])[0, 1]) if live.sum() > 1000 and audio.shape[1] > 1 else 1.0
    x = audio.mean(axis=1)
    nz = np.nonzero(np.abs(x) > 0)[0]
    end = int(nz[-1]) + 1 if len(nz) else 0
    info = sf.info(path)
    x = signal.sosfiltfilt(signal.butter(2, 10, "highpass", fs=RATE, output="sos"), x[:end])
    return x, corr, (len(audio) - end) / RATE, f"{info.subtype}, {info.channels} ch"


# ---------------------------------------------------------------------------
# Slicing

def smoothed(env, frames=4):
    return np.convolve(env, np.ones(frames) / frames, mode="same")


def onsets(x, keys):
    """Note starts (envelope frames) and how far each rose out of the quiet before it."""
    env = P.envelope(x)
    sm = smoothed(env)
    top = np.percentile(sm, 99)
    found = []
    for need in (22.0, 18.0, 15.0, 12.0, 9.0):
        found = []
        i = 30
        while i < len(sm) - 1:
            before = sm[max(0, i - 30):i].min()           # the quietest of the last 150 ms
            ahead = sm[i:i + 60].max()                    # the loudest of the next 300 ms
            if sm[i] - before > 6 and ahead - before >= need and ahead > top - 40:
                start = i
                while start > 0 and sm[start - 1] > before + 3 and i - start < 30:
                    start -= 1
                peak = i + int(np.argmax(sm[i:i + 60]))
                found.append({"start": start, "peak": peak, "rise": float(ahead - before), "level": float(ahead)})
                i = peak + 40
            else:
                i += 1
        if len(found) >= keys:
            break
    extra = []
    while len(found) > keys:
        k = min(range(1, len(found)), key=lambda j: found[j]["rise"])
        extra.append(found.pop(k))
    return found, extra, env, need


def note_end(env, start, stop, floor):
    """The frame where a note has faded into the floor (+5 dB) for good, before `stop`."""
    sm = smoothed(env, 10)
    peak = start + int(np.argmax(sm[start:min(stop, start + 200)]))
    for i in range(peak, stop):
        if sm[i] < floor + 5 and sm[i:min(stop, i + 20)].max() < floor + 8:
            return min(stop, i + 20)
    return stop


# ---------------------------------------------------------------------------
# Pitch

def yin_track(seg, fmin, fmax):
    frame = 4096 if fmin < 40 else 2048
    return P.yin(seg, fmin=fmin, fmax=fmax, frame=frame, hop=1024)


def raw_pitch(seg):
    frames = yin_track(seg, 14.0, 2500.0)
    good = [f for f, ap in frames if ap < 0.25]
    if len(good) < 3:
        return None
    return 69 + 12 * math.log2(float(np.median(good)) / 440.0)


def pitch_near(seg, label, partials=8):
    """(MIDI pitch, wander in cents) within 3 semitones of `label` (the key the sweep's
    order gives it), refined on the harmonics. A strong third harmonic over a weak second
    (the static-y bass) or a weak fundamental (the pulsating synth) can pull a wider
    search to a fifth or an octave."""
    fmin, fmax = max(10.0, hz(label - 3)), hz(label + 3)
    seg = signal.sosfiltfilt(signal.butter(4, 0.8 * fmin, "highpass", fs=RATE, output="sos"), seg)
    frames = yin_track(seg, fmin, fmax)
    good = [f for f, ap in frames if ap < 0.3]
    if len(good) < 3:
        return None, None
    f0 = float(np.median(good))
    wander = float(np.std([1200 * math.log2(f / f0) for f in good]))
    return 69 + 12 * math.log2(weighted_pitch(seg[:int(1.0 * RATE)], f0) / 440.0), wander


def weighted_pitch(seg, f0, harmonics=6):
    """The pitch a patch's harmonics agree on: each of the first six at its power-weighted
    centroid within 50 cents of f0's multiple, the six weighted by their power. A synth's
    oscillators are not one harmonic series (the static-y bass's fundamental sits 37 cents
    under its in-tune third harmonic; detuned oscillators beat; a pulsing patch splits each
    harmonic into sidebands), so no single peak is the pitch. Centred twice."""
    n = 1 << 18
    power = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n)) ** 2
    freqs = np.fft.rfftfreq(n, 1.0 / RATE)
    for _ in range(2):
        total, weight = 0.0, 0.0
        for k in range(1, harmonics + 1):
            lo, hi = np.searchsorted(freqs, [k * f0 * 2 ** (-50 / 1200), k * f0 * 2 ** (50 / 1200)])
            if hi >= len(freqs) or hi <= lo:
                continue
            p = power[lo:hi]
            cents = 1200 * np.log2(freqs[lo:hi] / (k * f0))
            total += float(np.sum(p * cents))
            weight += float(np.sum(p))
        if weight <= 0:
            break
        f0 *= 2 ** (total / weight / 1200)
    return f0


def analysis(y, long_note):
    # Past the attack (a filter envelope's sweep), for up to a second.
    a = int((0.25 if long_note else 0.06) * RATE)
    return y[a:a + int((1.0 if long_note else 0.5) * RATE)]


# ---------------------------------------------------------------------------
# Long hits: the release, the loop

def release_point(y):
    """Where the key was let go: walking back from the end of the note, up its final fall
    (5 ms frames, smoothed over 50 ms) to where the level stops rising (the knee)."""
    env = P.envelope(y)
    sm = smoothed(env, 10)
    live = np.nonzero(sm > sm.max() - 45)[0]
    if len(live) < 40:
        return None

    def knee(t):
        while t > 1 and sm[t - 1] > sm[t] + 0.02:
            t -= 1
        return t

    t = knee(int(live[-1]))
    # A pulsing patch dips between pulses: while the level here is still well under the
    # hold's, this was a dip on the way down; keep climbing.
    hold = float(np.percentile(sm[:t], 75)) if t > 20 else float(sm[t])
    while t > 20 and sm[t] < hold - 6:
        t = knee(t - 1)
    return t * HOP


def find_loop(y, f0, a, b, min_len=0.6, max_len=2.2):
    """Loop start and end in y[a:b] (samples), early in the hold (what follows the loop is
    cut out): an upward zero crossing, its best match earlier (waveform within 23 ms,
    envelope within 150 ms each side), longer loops a little preferred."""
    win = 512
    env = smoothed(P.envelope(y), 2)
    lo_e, hi_e = a + int(min_len * RATE), min(b, a + int((max_len + 0.3) * RATE))
    ends = [i for i in range(max(lo_e, win), min(hi_e, len(y) - win)) if y[i - 1] < 0 <= y[i]]
    if not ends:
        return None
    best = None
    for e in ends[::max(1, len(ends) // 30)]:
        lo, hi = max(a, e - int(max_len * RATE)), e - int(min_len * RATE)
        if hi - lo < 64:
            continue
        ref = y[e - win:e + win]
        region = y[lo - win:hi + win]
        corr = signal.correlate(region, ref, mode="valid")
        energy = np.convolve(region ** 2, np.ones(2 * win), mode="valid")[:len(corr)]
        diff = (energy - 2 * corr + np.sum(ref ** 2)) / np.maximum(energy + np.sum(ref ** 2), 1e-20)
        # Only upward zero crossings start a loop.
        cand = np.nonzero((y[lo - 1:hi - 1] < 0) & (y[lo:hi] >= 0))[0]
        if len(cand) == 0:
            continue
        ef = e // HOP
        ctx = 30
        if ef + ctx >= len(env):
            continue
        around_e = env[ef - ctx:ef + ctx]
        scores = []
        for k in cand[np.argsort(diff[cand])[:60]]:
            s = lo + int(k)
            sf_ = s // HOP
            if sf_ - ctx < 0:
                continue
            env_cost = float(np.mean(np.abs(env[sf_ - ctx:sf_ + ctx] - around_e)))
            cost = float(diff[k]) + 0.03 * env_cost
            scores.append((cost - 0.02 * (e - s) / RATE, s, float(diff[k]), env_cost))
        if not scores:
            continue
        sc = min(scores)
        if best is None or sc[0] < best[0]:
            best = (sc[0], sc[1], e, sc[2], sc[3])
    if best is None:
        return None
    _, s, e, wave, env_cost = best
    return s, e, wave, env_cost


# ---------------------------------------------------------------------------

def process(spec, source, report):
    """The notes of one recording, labelled and trimmed (not yet levelled or written)."""
    sid, path = spec["id"], P.resolve(source, spec["file"]) or os.path.join(source, spec["file"])
    keys, long_note = spec.get("keys", 37), spec["articulation"] == "long"
    if not os.path.exists(path):
        report["problems"].append(f"{sid}: missing file '{spec['file']}'")
        return []
    x, corr, trailing, fmt = load_wav(path)
    found, extra, env, need = onsets(x, keys)
    floor = float(np.percentile(env[env > -200], 5))
    entry = {"file": spec["file"], "set": sid, "layer": spec["articulation"], "format": fmt,
             "stereo": "dual mono" if corr > 0.999 else f"stereo (L/R correlation {corr:.3f}, mixed to mono)",
             "trailingSilence": round(trailing, 2), "noiseFloorDb": round(floor, 1), "expected": keys, "found": len(found),
             "candidates": len(found) + len(extra), "riseNeededDb": need,
             "rejected": [f"{c['start'] * HOP / RATE:.2f}s (rose {c['rise']:.0f} dB: inside a note)" for c in extra], "notes": []}
    report["files"].append(entry)
    if len(found) != keys:
        report["problems"].append(f"{sid}: expected {keys} notes, found {len(found)}")

    # Slice, then a first pitch for every note; the sweep's order gives the keys.
    notes = []
    for k, c in enumerate(found):
        stop = found[k + 1]["start"] - 2 if k + 1 < len(found) else len(env)
        end = note_end(env, c["start"], stop, floor)
        start = max(0, c["start"] * HOP - int(0.005 * RATE))
        # The attack: 5 ms before the level first comes within 30 dB of the note's peak
        # (a patch's noise or the last note's echo can creep up for 100 ms before it).
        head = P.envelope(x[start:min(end * HOP, start + int(1.5 * RATE))])
        if len(head):
            start += max(0, int(np.argmax(head >= head.max() - 30.0)) * HOP - int(0.005 * RATE))
        y = x[start:end * HOP].copy()
        notes.append({"y": y, "at": start / RATE, "raw": raw_pitch(analysis(y, long_note))})
    offsets = [n["raw"] - k for k, n in enumerate(notes) if n["raw"] is not None]
    if not offsets:
        report["problems"].append(f"{sid}: no pitch found")
        return []
    base = int(round(float(np.median(offsets))))
    entry["range"] = f"{P.name_of(base)}-{P.name_of(base + len(notes) - 1)}"
    octave_off = 0
    heard = []
    zones = []
    for k, n in enumerate(notes):
        label = base + k
        where = f"{sid} {P.name_of(label)} at {n['at']:.2f}s"
        if n["raw"] is not None and abs(n["raw"] - label) > 0.5:
            if abs(abs(n["raw"] - label) - 12) < 0.5 or abs(abs(n["raw"] - label) - 24) < 0.5:
                octave_off += 1
            else:
                heard.append(f"{P.name_of(label)} (heard {P.name_of(n['raw'])})")
        midi, wander = pitch_near(analysis(n["y"], long_note), label)
        if midi is None:
            report["problems"].append(f"{where}: no pitch near the key; using the key")
            midi, wander = float(label), 0.0
        cents = 100 * (midi - label)
        if abs(cents) >= 50:
            report["problems"].append(f"{where}: {cents:+.0f} cents from the key")
        elif abs(cents) > 25:
            report["warnings"].append(f"{where}: {cents:+.0f} cents out (the sampler plays it in tune)")
        if wander > 15:
            report["warnings"].append(f"{where}: its pitch wanders {wander:.0f} cents (standard deviation)")
        note = {"label": P.name_of(label), "at": round(n["at"], 2), "detected": f"{cents:+.0f} cents", "wander": round(wander, 1)}
        y = n["y"]
        zone = {"label": P.name_of(label), "key": label, "root": round(midi, 3), "source": spec["file"], "at": round(n["at"], 3)}
        if long_note and spec.get("loop", True):
            rel = release_point(y)
            held = None
            if rel is not None:
                sm = smoothed(P.envelope(y[:rel]), 10)
                peak = int(np.argmax(sm[:min(len(sm), 120)]))
                a = (peak + 30) * HOP            # 150 ms past the attack's peak
                b = rel - int(0.03 * RATE)
                note["held"] = round(rel / RATE, 2)
                if b - a >= int(0.5 * RATE):
                    # A loop as long as the hold allows, up to 1.2 s (a noisy or pulsing
                    # patch repeats audibly in a short one).
                    hold = (b - a) / RATE
                    held = find_loop(y, hz(midi), a, b, min_len=min(0.6, 0.7 * hold), max_len=1.2)
            if held is None:
                report["problems"].append(f"{where}: no sustain loop (the hold is too short)")
            else:
                s, e, wave, env_cost = held
                # Its pitch where it loops (the attack's filter sweep can pull the start).
                m2, w2 = pitch_near(y[s:e], label)
                if m2 is not None and abs(m2 - midi) > 0.03:
                    note["detected"] += f", held {100 * (m2 - label):+.0f} cents"
                    midi = m2
                    zone["root"] = round(midi, 3)
                y = P.crossfade_loop(y, s, e, int(min(0.15, 0.25 * (e - s) / RATE) * RATE))
                # Attack + loop + the recorded release: the hold beyond the loop is not needed.
                pad = 64
                tail = y[rel:]
                y = np.concatenate([y[:e + pad], tail])
                zone["loop"] = [s, e]
                zone["release"] = e + pad
                note["loop"] = f"{s / RATE:.2f}-{e / RATE:.2f}s (waveform {wave:.3f}, envelope {env_cost:.1f} dB)"
                if wave > 0.15 or env_cost > 2.0:
                    report["warnings"].append(f"{where}: the best loop matches loosely (waveform {wave:.3f}, envelope {env_cost:.1f} dB); crossfaded")
        # Length: until the note has fallen 50 dB under its peak (a short hit's ring, a long
        # one's recorded release), at most maxSeconds for a short hit.
        y = y[:faded(y, 50.0)]
        limit = int(spec.get("maxSeconds", 3.0) * RATE)
        if len(y) > limit and "loop" not in zone:
            y = y[:limit]
        fin = int(0.002 * RATE)
        y[:fin] *= np.linspace(0, 1, fin)
        fout = int(np.clip(0.1 * len(y), 0.02 * RATE, 0.2 * RATE))
        y[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
        note["seconds"] = round(len(y) / RATE, 2)
        zone["audio"] = y
        entry["notes"].append(note)
        zones.append(zone)
    if heard:
        # The near measurement below settles them (a problem is reported if it does not).
        report["warnings"].append(f"{sid}: the first pass heard {', '.join(heard)} (a strong third harmonic); measured at the sweep's key")
    if octave_off:
        report["warnings"].append(f"{sid}: the detector heard {octave_off} note(s) an octave off; the sweep's order put them on their keys")
    named = spec.get("fileSays")
    if named:
        report["warnings"].append(f"{sid}: {named}")
    entry["sounding"] = round(float(np.median([sounding(z["audio"]) for z in zones])), 2)
    return zones


def faded(y, below):
    """Where a note has fallen `below` dB under its peak for good (samples, +20 ms)."""
    sm = smoothed(P.envelope(y), 10)
    live = np.nonzero(sm > sm.max() - below)[0]
    return min(len(y), (int(live[-1]) + 4) * HOP) if len(live) else len(y)


def sounding(y):
    """How long a note sounds within 20 dB of its peak (seconds)."""
    env = smoothed(P.envelope(y), 10)
    return float(np.sum(env > env.max() - 20) * HOP / RATE)


def write(spec, zones, gain, out_dir):
    """Levels, writes the notes and the mapping file."""
    sid = spec["id"]
    os.makedirs(os.path.join(out_dir, sid), exist_ok=True)
    keys = sorted(z["key"] for z in zones)
    stretch = spec.get("endStretch", 7)
    out = []
    for z in zones:
        a = z["audio"] * gain
        name = f"{z['label'].replace('#', 's')}-{spec['articulation']}-1.flac"
        sf.write(os.path.join(out_dir, sid, name), a.astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
        env = P.envelope(a[:int(1.5 * RATE)])
        top = int(np.argmax(np.convolve(env, np.ones(2) / 2, mode="same"))) * HOP
        head = a[top:top + int(0.3 * RATE)]
        m = {"file": f"{sid}/{name}", "label": z["label"], "root": z["root"],
             "loKey": z["key"] - (stretch if z["key"] == keys[0] else 0),
             "hiKey": z["key"] + (stretch if z["key"] == keys[-1] else 0),
             "loVel": 0.0, "hiVel": 1.0, "layer": 0, "rr": 0, "length": len(a),
             "rms": round(float(P.db(np.sqrt(np.mean(head ** 2)))), 2), "loudness": round(P.note_loudness(a), 2),
             "peak": round(float(P.db(np.abs(a).max())), 2), "source": z["source"], "at": z["at"]}
        if "loop" in z:
            m["loopStart"], m["loopEnd"] = z["loop"]
            m["release"] = z["release"]
        out.append(m)
    out.sort(key=lambda m: m["root"])
    mapping = {"id": sid, "name": spec["name"], "kind": "synth", "articulation": spec["articulation"], "rate": RATE,
               "channels": 1, "layers": [spec["articulation"]], "gainDb": round(20 * math.log10(gain), 2), "zones": out}
    with open(os.path.join(out_dir, f"{sid}.json"), "w") as f:
        import json
        json.dump(mapping, f, indent=1)
    return mapping


def run(specs, source, out_dir, report):
    """Every synth set: processed, levelled per patch, written. Returns the mappings."""
    mappings = run_chromatic([s for s in specs if s["kind"] == "chromatic"], source, out_dir, report)
    specs = [s for s in specs if s["kind"] != "chromatic"]
    done = {}
    for spec in specs:
        print(f"[{spec['id']}] ...", flush=True)
        done[spec["id"]] = (spec, process(spec, source, report))
    for patch in dict.fromkeys(s["patch"] for s in specs):
        members = [done[s["id"]] for s in specs if s["patch"] == patch and done[s["id"]][1]]
        if not members:
            continue
        peak = max(np.abs(z["audio"]).max() for _, zones in members for z in zones)
        gain = 10 ** (-1 / 20) / max(peak, 1e-9)
        # The articulations' lengths, as recorded: short hits should be the shorter.
        lengths = {spec["articulation"]: e["sounding"] for spec, _ in members for e in report["files"] if e["set"] == spec["id"]}
        if "short" in lengths and "long" in lengths and lengths["short"] >= lengths["long"]:
            report["problems"].append(f"{patch}: the short hits sound longer ({lengths['short']} s) than the long ones ({lengths['long']} s)")
        report["patches"][patch] = {"gainDb": round(20 * math.log10(gain), 2), "sounding": lengths}
        for spec, zones in members:
            mappings.append(write(spec, zones, gain, out_dir))
    return mappings


# ---------------------------------------------------------------------------
# Session 8: acoustic piano sweeps ("kind": "chromatic"). Every key, lowest to highest,
# one at a time; three short-hit dynamics and one held sweep, the same gain for all
# four. Unlike the synth sweeps, the order alone cannot give the keys: a double strike
# or a key too soft to find shifts every label after it, so the keys are assigned in
# order by how clearly each expected pitch sounds (prepare.pick, as for the strings).

def piano_pitch(seg, midi):
    """(f0 Hz, inharmonicity B) of a struck string near `midi`: its partials found where
    f_n = n f0 sqrt(1 + B n^2) puts them, then f0 and B fitted ((f_n / n)^2 is linear in
    n^2). The fundamental of a low note is weak; its partials still give f0."""
    n_fft = 1 << 18
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n_fft))
    binhz = RATE / n_fft
    f0, b = hz(midi), 4.0e-4 * 2 ** ((midi - 60) / 24)
    for _ in range(3):
        pts = []
        for n in range(1, 13):
            pred = n * f0 * math.sqrt(1 + b * n * n)
            if pred > min(6000.0, 0.45 * RATE):
                break
            lo, hi = int(pred * 2 ** (-35 / 1200) / binhz), int(pred * 2 ** (35 / 1200) / binhz) + 1
            if hi >= len(spec) - 1 or hi - lo < 3:
                break
            i = lo + int(np.argmax(spec[lo:hi]))
            around = np.median(spec[max(0, lo - (hi - lo)):hi + (hi - lo)])
            if spec[i] < 4 * around:
                continue
            a, c, d = np.log(spec[i - 1] + 1e-20), np.log(spec[i] + 1e-20), np.log(spec[i + 1] + 1e-20)
            shift = 0.5 * (a - d) / (a - 2 * c + d) if (a - 2 * c + d) != 0 else 0.0
            pts.append((n, (i + shift) * binhz, float(spec[i])))
        if not pts:
            return None, None
        if len(pts) >= 3:
            nn = np.array([p[0] for p in pts], float)
            yy = np.array([(p[1] / p[0]) ** 2 for p in pts])
            w = np.array([p[2] for p in pts])
            A = np.vstack([np.ones_like(nn), nn * nn]).T * w[:, None]
            sol, *_ = np.linalg.lstsq(A, yy * w, rcond=None)
            if sol[0] > 0:
                f0 = math.sqrt(sol[0])
                b = float(np.clip(sol[1] / sol[0], 0.0, 0.01))
        else:
            n, fn, _ = pts[0]
            f0 = fn / (n * math.sqrt(1 + b * n * n))
    return f0, b


def chromatic_notes(spec, source, report):
    """The notes of one piano sweep: labelled, sliced, the previous key's ringing notched
    out where it overlaps. Returns zones with their measured pitches (not yet tuned)."""
    sid = spec["id"]
    path = P.resolve(source, spec["file"])
    keys = spec.get("keys", 88)
    if path is None:
        report["problems"].append(f"{sid}: missing file '{spec['file']}'")
        return []
    x, corr, trailing, fmt = load_wav(path)
    # Candidates: every rise out of the quiet (relaxed: a missed key cannot be recovered).
    loose, _, env, need = onsets(x, 10 ** 6)
    ordered, _, _, _ = onsets(x, keys)
    # The first key: the median offset of the first pass's pitches over the ordered onsets.
    raw = []
    for k, c in enumerate(ordered):
        s = c["start"] * HOP
        e = ordered[k + 1]["start"] * HOP if k + 1 < len(ordered) else len(x)
        r = raw_pitch(x[s + int(0.04 * RATE):min(e, s + int(0.6 * RATE))])
        if r is not None:
            raw.append(r - k)
    base = int(round(float(np.median(raw)))) if raw else spec.get("lowest", 21)
    if "lowest" in spec and abs(base - P.midi_of(spec["lowest"])) > 0:
        report["warnings"].append(f"{sid}: the sweep's order points at {P.name_of(base)} as the lowest key; the manifest says {spec['lowest']}, kept the manifest")
        base = P.midi_of(spec["lowest"])
    sequence = list(range(base, base + keys))
    cands = [(c["start"], c["peak"], c["level"]) for c in loose]
    chosen, rejected = P.pick(cands, x, False, 3, sequence)
    floor = float(np.percentile(env[env > -200], 5))
    entry = {"file": spec["file"], "set": sid, "layer": spec.get("articulation", "short"), "format": fmt,
             "stereo": "dual mono" if corr > 0.999 else f"stereo (L/R correlation {corr:.3f}, mixed to mono)",
             "trailingSilence": round(trailing, 2), "noiseFloorDb": round(floor, 1), "expected": keys, "found": len(chosen),
             "candidates": len(cands), "riseNeededDb": 9.0, "range": f"{P.name_of(base)}-{P.name_of(base + keys - 1)}",
             "rejected": [f"{c['start'] * HOP / RATE:.2f}s ({c['level']:.0f} dB)" for c in rejected], "notes": []}
    report["files"].append(entry)
    if len(chosen) != keys:
        report["problems"].append(f"{sid}: expected {keys} keys, found {len(chosen)}")
        return []
    zones, prev = [], None
    for k, c in enumerate(chosen):
        label = sequence[k]
        where = f"{sid} {P.name_of(label)} at {c['start'] * HOP / RATE:.2f}s"
        stop = chosen[k + 1]["start"] - 2 if k + 1 < len(chosen) else len(env)
        stop = min(stop, c["start"] + int(spec.get("maxSeconds", 6.0) * RATE / HOP))
        start = max(0, c["start"] * HOP - int(0.004 * RATE))
        end = note_end(env, c["start"], stop, floor)
        # The hammer: 5 ms before the level first comes within 30 dB of the note's peak (a
        # candidate can start on the damper or a key's noise before it).
        head = P.envelope(x[start:min(end * HOP, start + int(1.5 * RATE))])
        if len(head):
            start += max(0, int(np.argmax(head >= head.max() - 30.0)) * HOP - int(0.005 * RATE))
        y = x[start:end * HOP].copy()
        seg = y[int(0.06 * RATE):int(0.06 * RATE) + int(0.7 * RATE)]
        f0, b = piano_pitch(seg, label) if len(seg) > 4096 else (None, None)
        note = {"label": P.name_of(label), "at": round(start / RATE, 2)}
        if f0 is None:
            report["problems"].append(f"{where}: no partials found; using the key")
            f0, b = hz(label), 0.0
        midi = 69 + 12 * math.log2(f0 / 440.0)
        if abs(midi - label) > 0.6:
            report["problems"].append(f"{where}: sounds {P.name_of(midi)} ({100 * (midi - label):+.0f} cents from its key)")
        # The previous key still ringing into this one (a low string's long decay): its
        # partials notched out (zero-phase, narrow), except where they sit on this note's.
        if prev is not None:
            pf, pb, plevel = prev
            head = P.envelope(y[:int(0.4 * RATE)])
            if len(head) and plevel > head.max() - 30.0:
                mine = [n * f0 * math.sqrt(1 + b * n * n) for n in range(1, 40)]
                notched = []
                for n in range(1, 24):
                    f = n * pf * math.sqrt(1 + pb * n * n)
                    if f > 5000:
                        break
                    if min(abs(f / m - 1) for m in mine) < 0.025:
                        continue
                    bq, aq = signal.iirnotch(f, 25, RATE)
                    y = signal.filtfilt(bq, aq, y)
                    notched.append(f)
                if notched:
                    note["bleed"] = f"previous key notched at {len(notched)} partials ({plevel - head.max():+.0f} dB under)"
        # The level this note still has where the next one starts (for the next notch).
        tail_level = float(np.max(P.envelope(x[max(start, stop * HOP - int(0.1 * RATE)):stop * HOP]) if stop * HOP - start > int(0.1 * RATE) else [-120]))
        prev = (f0, b, tail_level)
        y = y[:faded(y, spec.get("rangeDb", 60.0))]
        fin = int(0.001 * RATE)
        y[:fin] *= np.linspace(0, 1, fin)
        fout = int(np.clip(0.12 * len(y), 0.02 * RATE, 0.25 * RATE))
        y[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
        note["seconds"] = round(len(y) / RATE, 2)
        note["detected"] = f"{100 * (midi - label):+.0f} cents, B {b:.1e}" + (f"; {note['bleed']}" if "bleed" in note else "")
        zones.append({"label": P.name_of(label), "key": label, "measured": midi, "audio": y, "source": spec["file"], "at": round(start / RATE, 3)})
        entry["notes"].append(note)
    return zones


def stretch_curve(keys, devs):
    """A smooth curve through the notes' deviations from equal temperament (cents): the
    piano's stretch (low notes flat, high notes sharp) and its overall offset. A running
    median over 9 keys, then a running mean over 7 (single strings out of tune do not
    bend it)."""
    order = np.argsort(keys)
    k, d = np.asarray(keys)[order], np.asarray(devs)[order]
    med = np.array([np.median(d[max(0, i - 4):i + 5]) for i in range(len(d))])
    smooth = np.array([np.mean(med[max(0, i - 3):i + 4]) for i in range(len(med))])
    return dict(zip(k.tolist(), smooth.tolist()))


def run_chromatic(specs, source, out_dir, report):
    """The piano: every sweep sliced and measured; the overall tuning corrected with the
    stretch kept (each note sounds at its key plus the smooth stretch curve, relative to
    A4); one gain for all four files; the short sweeps one instrument with three
    velocity layers, the held sweep another."""
    done = []
    for spec in specs:
        print(f"[{spec['id']}] ...", flush=True)
        done.append((spec, chromatic_notes(spec, source, report)))
    mappings = []
    for group in dict.fromkeys(s.get("tuningGroup", s["id"]) for s in specs):
        members = [(s, z) for s, z in done if s.get("tuningGroup", s["id"]) == group and z]
        if not members:
            continue
        keys = [z["key"] for _, zs in members for z in zs]
        devs = [100 * (z["measured"] - z["key"]) for _, zs in members for z in zs]
        curve = stretch_curve(keys, devs)
        ref = curve.get(69, float(np.median(devs)))
        out_of_tune = []
        for s, zs in members:
            for z in zs:
                dev = 100 * (z["measured"] - z["key"])
                stretch = curve[z["key"]] - ref
                # Played at its key, the note sounds at key + stretch (cents).
                z["root"] = round(z["key"] + (dev - stretch) / 100.0, 3)
                if abs(dev - curve[z["key"]]) > 15:
                    out_of_tune.append(f"{s['id']} {z['label']} {dev - curve[z['key']]:+.0f}")
        report["tuning"] = report.get("tuning", {})
        report["tuning"][group] = {"overallCents": round(ref, 1), "stretch": {P.name_of(k): round(v - ref, 1) for k, v in curve.items() if k % 12 == 9},
                                   "stringsOffTheCurve": out_of_tune}
        report["warnings"].append(f"{group}: overall tuning {ref:+.1f} cents at A4, corrected; stretch kept "
                                  f"({', '.join(f'{P.name_of(k)} {v - ref:+.0f}' for k, v in curve.items() if k % 24 == 9)} cents)")
        if out_of_tune:
            report["warnings"].append(f"{group}: {len(out_of_tune)} strings more than 15 cents off the stretch curve (tuned onto it): {', '.join(out_of_tune)}")
    for patch in dict.fromkeys(s.get("patch", s["id"]) for s in specs):
        members = [(s, z) for s, z in done if s.get("patch", s["id"]) == patch and z]
        if not members:
            continue
        built = []
        for inst in dict.fromkeys(s.get("instrument", s["id"]) for s, _ in members):
            parts = sorted([(s, zs) for s, zs in members if s.get("instrument", s["id"]) == inst], key=lambda p: p[0].get("layerIndex", 0))
            steps = parts[0][0].get("layerStepsDb")
            if steps and len(steps) == len(parts):
                # The hand was not even from key to key: each key's layers are set `steps`
                # apart around the key's own level (each keeps its tone). A take that would
                # need more than `maxStepDb` is dropped: that key's layer then plays the
                # neighbouring key's take, a semitone away (reported: re-record it).
                limit = parts[0][0].get("maxStepDb", 12.0)
                dropped = []
                for key in sorted({z["key"] for _, zs in parts for z in zs}):
                    mine = [(li, z) for li, (_, zs) in enumerate(parts) for z in zs if z["key"] == key]
                    lv = {li: P.note_loudness(z["audio"]) for li, z in mine}
                    centre = float(np.median([lv[li] - steps[li] for li in lv]))
                    for li, z in mine:
                        trim = centre + steps[li] - lv[li]
                        if abs(trim) > limit:
                            parts[li][1].remove(z)
                            dropped.append(f"{parts[li][0].get('layerName', li)} {z['label']} ({trim:+.0f} dB)")
                        else:
                            z["audio"] = z["audio"] * 10 ** (trim / 20)
                if dropped:
                    report["problems"].append(f"{inst}: takes far off their key's other dynamics, dropped (the neighbouring key's take plays; re-record): {', '.join(dropped)}")
            built.append((inst, parts))
        # One gain for the patch (the same input gain for every file), after the steps.
        peak = max(np.abs(z["audio"]).max() for _, parts in built for _, zs in parts for z in zs)
        gain = 10 ** (-1 / 20) / max(peak, 1e-9)
        for inst, parts in built:
            mappings.append(write_chromatic(inst, parts, gain, out_dir))
            # Velocity layers: louder with each layer, note by note.
            if len(parts) > 1:
                for key in sorted({z["key"] for _, zs in parts for z in zs}):
                    lv = [next((P.note_loudness(z["audio"]) for z in zs if z["key"] == key), None) for _, zs in parts]
                    if None not in lv and any(lv[i] >= lv[i + 1] for i in range(len(lv) - 1)):
                        report["warnings"].append(f"{inst} {P.name_of(key)}: velocity layers not in loudness order ({', '.join(f'{v:.1f}' for v in lv)} dB)")
    return mappings


def write_chromatic(inst, parts, gain, out_dir):
    os.makedirs(os.path.join(out_dir, inst), exist_ok=True)
    out, names = [], []
    for li, (spec, zones) in enumerate(parts):
        names.append(spec.get("layerName", spec.get("articulation", "short")))
        keys = sorted(z["key"] for z in zones)
        for z in zones:
            a = z["audio"] * gain
            name = f"{z['label'].replace('#', 's')}-{names[-1]}-1.flac"
            sf.write(os.path.join(out_dir, inst, name), a.astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
            env = P.envelope(a[:int(1.5 * RATE)])
            top = int(np.argmax(np.convolve(env, np.ones(2) / 2, mode="same"))) * HOP
            head = a[top:top + int(0.3 * RATE)]
            vel = spec.get("velocity", [0.0, 1.0])
            out.append({"file": f"{inst}/{name}", "label": z["label"], "root": z["root"],
                        "loKey": z["key"] - (spec.get("endStretch", 5) if z["key"] == keys[0] else 0),
                        "hiKey": z["key"] + (spec.get("endStretch", 5) if z["key"] == keys[-1] else 0),
                        "loVel": vel[0], "hiVel": vel[1], "layer": li, "rr": 0, "length": len(a),
                        "rms": round(float(P.db(np.sqrt(np.mean(head ** 2) + 1e-24))), 2), "loudness": round(P.note_loudness(a), 2),
                        "peak": round(float(P.db(np.abs(a).max())), 2), "source": z["source"], "at": z["at"]})
    out.sort(key=lambda m: (m["root"], m["layer"]))
    mapping = {"id": inst, "name": parts[0][0]["name"], "kind": "chromatic", "rate": RATE, "channels": 1, "layers": names,
               "gainDb": round(20 * math.log10(gain), 2), "zones": out}
    import json
    with open(os.path.join(out_dir, f"{inst}.json"), "w") as f:
        json.dump(mapping, f, indent=1)
    return mapping
