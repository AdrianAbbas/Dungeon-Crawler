#!/usr/bin/env python3
"""Warmth pass measurements (Session: warmth pass).

Measures stereo WAV renders of the DUNGEON CRAWLER test phrase and, optionally, gates them.

  measure.py --after DIR [--before DIR] [--hiss DIR [--hiss-before DIR]] [--summary FILE] [--json FILE] [--gate]

Per render: L/R correlation, band energies relative to the total (20-80 Hz, 200-500 Hz,
8-12 kHz, 12-20 kHz), spectral flatness 2-8 kHz, integrated loudness (ITU-R BS.1770-4),
loudness range (EBU Tech 3342; over the music, and over the whole file) with the short-term
10th percentile, and the true peak
(4x oversampled). With --gate the script fails when an "after" render breaks a target:
correlation > 0.8, LRA > 9 LU, true peak > -0.5 dBTP, or 2-8 kHz flatness < 0.12 at Grit 100 %.
The 8-20 kHz share is reported only (hiss fix, build 3: it was gated at >= -40 dB, which the
old hiss bed filled).

The hiss gate (hiss fix, build 3; --hiss, the "after" directory of `--hiss-renders`): per
instrument and era, the silence test (a chord held 5 s from 0.3 s, released at 5.3 s, 6 s of
silence) at AIR 15 % ("-silence.wav") and AIR 0 % ("-silence-air0.wav"). Gated: the noise floor
above 6 kHz in the last 4 s < -90 dBFS, and the hiss under the sustained pad (the difference of
the two, 0.8-5.3 s, RMS) < -60 dBFS. --hiss-before: build 2's renders, the floor reported.

Render names: "<instrument>-<era>-grit<0|50|100>.wav" (after, this build) and the 1.0 build's
"--listen-dungeonsynth" names "<instrument>-0-off.wav", "-classic-50.wav", ... (before).
Only the audio is read; nothing is written next to it.
"""
import argparse
import json
import math
import os
import sys

import numpy as np
from scipy import signal
import soundfile as sf

INSTRUMENTS = ["royal-strings", "dark-choir-pad", "chamber-organ", "lute"]
ERAS = ["classic", "modern"]
GRITS = [0, 50, 100]


def read(path):
    x, sr = sf.read(path, dtype="float64", always_2d=True)
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    return x[:, 0], x[:, 1], sr


def correlation(l, r):
    d = math.sqrt(float(np.dot(l, l)) * float(np.dot(r, r)))
    return float(np.dot(l, r)) / d if d > 0 else 1.0


def psd(l, r, sr):
    f, pl = signal.welch(l, sr, window="hann", nperseg=8192, noverlap=4096, scaling="spectrum")
    _, pr = signal.welch(r, sr, window="hann", nperseg=8192, noverlap=4096, scaling="spectrum")
    return f, pl + pr


def band_db(f, p, lo, hi):
    total = float(np.sum(p[(f >= 10.0)]))
    band = float(np.sum(p[(f >= lo) & (f < hi)]))
    return 10.0 * math.log10(max(band, 1e-30) / max(total, 1e-30))


def flatness(f, p, lo=2000.0, hi=8000.0):
    b = p[(f >= lo) & (f <= hi)]
    b = np.maximum(b, 1e-30)
    return float(np.exp(np.mean(np.log(b))) / np.mean(b))


def k_weight(x, sr):
    # ITU-R BS.1770-4: the high shelf and the RLB high-pass (coefficients derived for any rate).
    f0, g, q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    k = math.tan(math.pi * f0 / sr)
    vh, vb = 10 ** (g / 20), 10 ** (g / 20) ** 0.4996667741545416
    a0 = 1 + k / q + k * k
    b = [(vh + vb * k / q + k * k) / a0, 2 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0]
    a = [1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
    y = signal.lfilter(b, a, x)
    f0, q = 38.13547087602444, 0.5003270373238773
    k = math.tan(math.pi * f0 / sr)
    a0 = 1 + k / q + k * k
    b = [1, -2, 1]
    a = [1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
    return signal.lfilter(b, a, y)


def block_loudness(l, r, sr, seconds, hop):
    wl, wr = k_weight(l, sr), k_weight(r, sr)
    n, h = int(round(seconds * sr)), int(round(hop * sr))
    out = []
    for start in range(0, max(1, len(wl) - n + 1), h):
        z = (np.mean(wl[start:start + n] ** 2) + np.mean(wr[start:start + n] ** 2))
        out.append(-0.691 + 10 * math.log10(max(z, 1e-30)))
    return np.array(out)


def integrated(l, r, sr):
    blocks = block_loudness(l, r, sr, 0.4, 0.1)
    gated = blocks[blocks > -70.0]
    if len(gated) == 0:
        return -99.0
    mean = 10 * math.log10(np.mean(10 ** (gated / 10)))
    gated = gated[gated > mean - 10.0]
    return float(10 * math.log10(np.mean(10 ** (gated / 10)))) if len(gated) else -99.0


def loudness_range(l, r, sr):
    st = block_loudness(l, r, sr, 3.0, 0.1)
    gated = st[st > -70.0]
    if len(gated) == 0:
        return 0.0, -99.0
    mean = 10 * math.log10(np.mean(10 ** (gated / 10)))
    gated = gated[gated > mean - 20.0]
    p10, p95 = np.percentile(gated, 10), np.percentile(gated, 95)
    return float(p95 - p10), float(p10)


def true_peak(l, r):
    peak = 0.0
    for x in (l, r):
        y = signal.resample_poly(x, 4, 1)
        peak = max(peak, float(np.max(np.abs(y))))
    return 20 * math.log10(max(peak, 1e-12))


# The test phrase ends at 9.7 s (its last notes, 0.1 s in, 9.6 s long); the loudness range is read
# over the music, the phrase and 1.5 s of its ring-out (the same window before and after). With
# 3 s windows the renders' last 3.5 s of decay would otherwise set the 10th percentile alone.
MUSIC_SECONDS = 11.2


def measure(path):
    l, r, sr = read(path)
    f, p = psd(l, r, sr)
    n = min(len(l), int(MUSIC_SECONDS * sr))
    lra, p10 = loudness_range(l[:n], r[:n], sr)
    lra_full, _ = loudness_range(l, r, sr)
    return {
        "corr": correlation(l, r),
        "b20_80": band_db(f, p, 20, 80),
        "b200_500": band_db(f, p, 200, 500),
        "b8_12k": band_db(f, p, 8000, 12000),
        "b12_20k": band_db(f, p, 12000, 20000),
        "b8_20k": band_db(f, p, 8000, 20000),
        "flat2_8k": flatness(f, p),
        "lufs": integrated(l, r, sr),
        "lra": lra,
        "lra_full": lra_full,
        "st_p10": p10,
        "tp": true_peak(l, r),
    }


HOLD_START, RELEASE_AT, SILENCE_SECONDS = 0.3, 5.3, 6.0


def rms_db(x, sr, start, end):
    a, b = int(start * sr), int(end * sr)
    seg = x[a:b]
    return 10 * math.log10(max(float(np.mean(seg ** 2)) if len(seg) else 0.0, 1e-30))


def above_6k(l, r, sr):
    sos = signal.butter(8, 6000.0, "highpass", fs=sr, output="sos")
    return signal.sosfilt(sos, l), signal.sosfilt(sos, r)


def hiss_measure(path, air0_path=None):
    l, r, sr = read(path)
    hl, hr = above_6k(l, r, sr)
    end = RELEASE_AT + SILENCE_SECONDS
    floor = 10 * math.log10(max(0.5 * (10 ** (rms_db(hl, sr, end - 4.0, end) / 10) + 10 ** (rms_db(hr, sr, end - 4.0, end) / 10)), 1e-30))
    out = {"floor6k": floor, "b8_20k": band_db(*psd(l, r, sr), 8000, 20000)}
    pad = 0.5 * (10 ** (rms_db(hl, sr, HOLD_START + 0.5, RELEASE_AT) / 10) + 10 ** (rms_db(hr, sr, HOLD_START + 0.5, RELEASE_AT) / 10))
    out["above6k_pad"] = 10 * math.log10(max(pad, 1e-30))
    if air0_path and os.path.exists(air0_path):
        ql, qr, _ = read(air0_path)
        n = min(len(l), len(ql))
        dl, dr = l[:n] - ql[:n], r[:n] - qr[:n]
        h = 0.5 * (10 ** (rms_db(dl, sr, HOLD_START + 0.5, RELEASE_AT) / 10) + 10 ** (rms_db(dr, sr, HOLD_START + 0.5, RELEASE_AT) / 10))
        out["hiss_pad"] = 10 * math.log10(max(h, 1e-30))
        nz = np.nonzero((dl != 0) | (dr != 0))[0]
        out["hiss_last"] = float(nz[-1]) / sr if len(nz) else -1.0
    return out


def hiss_section(after_dir, before_dir, bad, results):
    lines = ["", "## Hiss gate (build 3): a chord held 5 s, released at 5.3 s, then 6 s of silence (Grit 0 %, AIR 15 %)", "",
             "Floor: above 6 kHz in the last 4 s (gate < -90 dBFS). Hiss: AIR 15 % minus AIR 0 % under the held pad, 0.8-5.3 s RMS (gate < -60 dBFS). "
             "Each cell: before (build 2) -> after (this build); 8-20 kHz share reported only.", "",
             "| instrument | era | floor > 6 kHz (last 4 s) | above 6 kHz under the pad | hiss under the pad | hiss ends | 8-20 kHz share (silence test) | result |",
             "|---|---|---|---|---|---|---|---|"]
    for inst in INSTRUMENTS:
        for era in ERAS:
            base = f"{inst}-{era}"
            path = os.path.join(after_dir, base + "-silence.wav")
            if not os.path.exists(path):
                bad.append(f"{base}: silence test render missing ({path})")
                continue
            m = hiss_measure(path, os.path.join(after_dir, base + "-silence-air0.wav"))
            b = None
            if before_dir and os.path.exists(os.path.join(before_dir, base + "-silence.wav")):
                b = hiss_measure(os.path.join(before_dir, base + "-silence.wav"))
            results[f"{inst} {era} silence"] = {"after": m, "before": b}
            f = []
            if m["floor6k"] >= -90.0:
                f.append(f"floor above 6 kHz {m['floor6k']:.1f} dBFS >= -90")
            if "hiss_pad" not in m:
                f.append("AIR 0 % render missing (hiss not measured)")
            elif m["hiss_pad"] >= -60.0:
                f.append(f"hiss under the pad {m['hiss_pad']:.1f} dBFS >= -60")
            bad.extend(f"{base}: {x}" for x in f)
            cell = lambda k, fmt="{:.1f}": ((fmt.format(b[k]) + " -> ") if b and k in b else "") + (fmt.format(m[k]) if k in m else "n/a")
            ends = f"{m['hiss_last']:.2f} s" if m.get("hiss_last", -1) >= 0 else "none"
            lines.append(f"| {inst} | {era} | {cell('floor6k')} | {cell('above6k_pad')} | {cell('hiss_pad')} | {ends} | {cell('b8_20k')} | "
                         + ("pass" if not f else "FAIL: " + "; ".join(f)) + " |")
    return lines


def before_name(inst, era, grit):
    return f"{inst}-0-off.wav" if grit == 0 else f"{inst}-{era}-{grit}.wav"


def after_name(inst, era, grit):
    return f"{inst}-{era}-grit{grit}.wav"


def failures(m, grit):
    out = []
    if m["corr"] > 0.8:
        out.append(f"correlation {m['corr']:.2f} > 0.8")
    if m["lra"] > 9.0:
        out.append(f"LRA {m['lra']:.1f} > 9")
    if m["tp"] > -0.5:
        out.append(f"true peak {m['tp']:.2f} dBTP > -0.5")
    if grit == 100 and m["flat2_8k"] < 0.12:
        out.append(f"2-8 kHz flatness {m['flat2_8k']:.3f} < 0.12 at Grit 100 %")
    return out


COLUMNS = [("corr", "L/R corr", "{:.2f}"), ("b20_80", "20-80 Hz", "{:.1f}"), ("b200_500", "200-500 Hz", "{:.1f}"),
           ("b8_12k", "8-12 kHz", "{:.1f}"), ("b12_20k", "12-20 kHz", "{:.1f}"), ("flat2_8k", "flat 2-8k", "{:.3f}"),
           ("lufs", "LUFS", "{:.1f}"), ("lra", "LRA", "{:.1f}"), ("lra_full", "LRA (file)", "{:.1f}"), ("st_p10", "ST p10", "{:.1f}"), ("tp", "TP dBTP", "{:.2f}")]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--after", required=True)
    ap.add_argument("--before")
    ap.add_argument("--summary")
    ap.add_argument("--json")
    ap.add_argument("--gate", action="store_true")
    ap.add_argument("--hiss", help="the after directory of --hiss-renders (the hiss gate)")
    ap.add_argument("--hiss-before", help="the before directory of --hiss-renders (build 2's air bed)")
    ap.add_argument("--cpu-before", help="a file holding the 1.0 build's CPU seconds for the listening renders")
    ap.add_argument("--cpu-after", help="the same for this build (warm)")
    a = ap.parse_args()
    rows, bad, results = [], [], {}
    for inst in INSTRUMENTS:
        for era in ERAS:
            for grit in GRITS:
                key = f"{inst} {era} grit {grit}"
                path = os.path.join(a.after, after_name(inst, era, grit))
                if not os.path.exists(path):
                    bad.append(f"{key}: render missing ({path})")
                    continue
                m = measure(path)
                b = None
                if a.before:
                    bp = os.path.join(a.before, before_name(inst, era, grit))
                    if os.path.exists(bp):
                        b = measure(bp)
                results[key] = {"after": m, "before": b}
                f = failures(m, grit)
                bad.extend(f"{key}: {x}" for x in f)
                rows.append((inst, era, grit, b, m, f))
    lines = ["## Warmth pass: the test phrase, 4 instruments x 2 eras x Grit 0 / 50 / 100 % (48 kHz stereo)", "",
             "Bands are dB relative to the whole spectrum. Each cell: before (1.0 build) -> after (this build). "
             "LRA: over the music (the phrase and 1.5 s of its ring-out); LRA (file): the whole render, its 3.5 s ring-out included. "
             "Gates (after): correlation <= 0.8, LRA <= 9 LU, true peak <= -0.5 dBTP, flatness >= 0.12 at Grit 100 %. 8-20 kHz: reported only.", "",
             "| instrument | era | grit | " + " | ".join(c[1] for c in COLUMNS) + " | result |",
             "|---|---|---|" + "---|" * len(COLUMNS) + "---|"]
    for inst, era, grit, b, m, f in rows:
        cells = []
        for k, _, fmt in COLUMNS:
            cells.append((fmt.format(b[k]) + " -> " if b else "") + fmt.format(m[k]))
        lines.append(f"| {inst} | {era} | {grit} | " + " | ".join(cells) + " | " + ("pass" if not f else "FAIL: " + "; ".join(f)) + " |")
    if a.hiss:
        lines += hiss_section(a.hiss, a.hiss_before, bad, results)
    if a.cpu_before and a.cpu_after:
        try:
            before_s = float(open(a.cpu_before).read().split()[-1])
            after_s = float(open(a.cpu_after).read().split()[-1])
            ratio = after_s / before_s if before_s > 0 else 0.0
            ok = ratio <= 1.5
            lines += ["", f"CPU, the same listening renders (5 instruments x 5 settings, the FX in two places): 1.0 build {before_s:.1f} s, "
                          f"this build {after_s:.1f} s, **{ratio:.2f}x** ({'within' if ok else 'over'} 1.5x)"]
            if not ok:
                bad.append(f"CPU {ratio:.2f}x the 1.0 build's (over 1.5x)")
        except (OSError, ValueError, IndexError) as error:
            bad.append(f"CPU timing unreadable ({error})")
    lines += ["", f"**{len(rows)} renders, {len(bad)} failures**", ""]
    text = "\n".join(lines)
    print(text)
    if a.summary:
        with open(a.summary, "a") as out:
            out.write(text + "\n")
    if a.json:
        with open(a.json, "w") as out:
            json.dump(results, out, indent=1)
    if bad:
        print("\n".join("FAIL " + x for x in bad), file=sys.stderr)
    return 1 if (a.gate and bad) else 0


if __name__ == "__main__":
    sys.exit(main())
