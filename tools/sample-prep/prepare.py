#!/usr/bin/env python3
"""Prepares the developer's recordings for WORLDBUILDER's sampler (Milestone 15).

Reads the MP3s of the private Samples repository and writes, per instrument, mono
16-bit FLAC notes and a mapping file (key ranges, velocity layers, round-robins, loop
points, release tails) that the plugin's sampler loads. Nothing it writes belongs in
the public repository: the output goes to a build folder.

    python3 tools/sample-prep/prepare.py --source ../samples --out build/samples

Steps, for every recording:
 1. Decode, and take off the MP3 encoder's padding: GarageBand's MP3s carry Apple's
    iTunSMPB tag (encoder delay, padding, sample count); the LAME header's delay and
    padding are used instead when there is one. The decoder's own 529-sample delay is
    taken off with the encoder's.
 2. Gentle noise reduction: the room tone's noise spectrum, fitted per third of an
    octave to the recording's own quiet gaps, is taken off with a soft spectral gate
    (at most 9 dB, smoothed over time so it does not warble).
 3. Slice: onsets from the level's rise, the expected number picked by strength and
    tone (handling noises and finger clicks fall out), each note running until it fades
    into the floor or the next one starts.
 4. Detect each note's pitch (YIN, refined on its harmonics) and check it against the
    label: the sampler is tuned to what was played, the label only says where it goes.
 5. Trim (keeping the attack and the natural tail), take off DC, fade the ends, and
    normalise each instrument with one gain (so the velocity layers keep their dynamics).
 6. Sustained notes: a sustain loop (searched for the best match, crossfaded) and the
    point where the bow lifts (the recorded release tail).
 7. Write the FLACs, the mapping files and a report of everything it could not match.

Needs numpy, scipy and soundfile (libsndfile 1.1+ decodes MP3).

The Korg Minilogue sweeps (WAV, one patch per recording, "kind": "synth") are prepared by
sweeps.py: sliced by key, each note at its own detected pitch, loops for the long hits.
"""

import argparse
import json
import math
import os
import re
import struct
import sys

import numpy as np
import soundfile as sf
from scipy import signal

RATE = 44100
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
DECODER_DELAY = 529


def midi_of(name):
    m = re.fullmatch(r"([A-G])([#b]?)(-?\d)", name)
    if not m:
        raise ValueError(f"bad note name {name}")
    pc = NOTE_NAMES.index(m.group(1)) + (1 if m.group(2) == "#" else (-1 if m.group(2) == "b" else 0))
    return 12 * (int(m.group(3)) + 1) + pc


def name_of(midi):
    m = int(round(midi))
    return f"{NOTE_NAMES[m % 12]}{m // 12 - 1}"


def db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), 1e-12))


def k_weight(x, rate=RATE):
    """ITU-R BS.1770's K-weighting (a high shelf, then a high-pass) at any rate."""
    k = math.tan(math.pi * 1681.974450955533 / rate)
    q = 0.7071752369554196
    vh = 10 ** (3.999843853973347 / 20)
    vb = vh ** 0.4996667741545416
    a0 = 1 + k / q + k * k
    shelf_b = [(vh + vb * k / q + k * k) / a0, 2 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0]
    shelf_a = [1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
    k = math.tan(math.pi * 38.13547087602444 / rate)
    q = 0.5003270373238773
    a0 = 1 + k / q + k * k
    hp_a = [1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
    return signal.lfilter([1, -2, 1], hp_a, signal.lfilter(shelf_b, shelf_a, x))


def note_loudness(audio):
    """How loud a note sounds (LUFS of one channel): its loudest 400 ms, K-weighted,
    within the first 1.5 s (a bow takes a while to speak)."""
    y = k_weight(audio[:int(1.9 * RATE)])
    win = int(0.4 * RATE)
    if len(y) <= win:
        return float(-0.691 + 10 * np.log10(np.mean(y ** 2) + 1e-24))
    # A running mean by cumulative sums (a 17640-tap convolution crawls on denormals in
    # the faded tails).
    total = np.cumsum(np.concatenate(([0.0], y ** 2)))
    power = ((total[win:] - total[:-win]) / win)[:int(1.5 * RATE)]
    return float(-0.691 + 10 * np.log10(max(power.max(), 0.0) + 1e-24))


# ---------------------------------------------------------------------------
# 0. Finding the recordings: in the root or in any subfolder (01-voice/, 02-piano/...),
# as MP3 or WAV (Session 8). A manifest name is a path relative to the source, or just
# a file name found anywhere below it (case and the extension's case ignored).

AUDIO_EXT = (".mp3", ".wav", ".aif", ".aiff", ".flac")
_tree = {}


def recordings(source):
    """Every audio file below `source`: relative paths, sorted (hidden folders skipped)."""
    if source not in _tree:
        found = []
        for dp, dns, fns in os.walk(source):
            dns[:] = sorted(d for d in dns if not d.startswith("."))
            for fn in fns:
                if fn.lower().endswith(AUDIO_EXT) and not fn.startswith("."):
                    found.append(os.path.relpath(os.path.join(dp, fn), source))
        _tree[source] = sorted(found)
    return _tree[source]


def resolve(source, name):
    """The path of a manifest's recording, wherever it is below `source`, or None."""
    direct = os.path.join(source, name)
    if os.path.exists(direct):
        return direct
    base = os.path.basename(name).lower()
    stem = os.path.splitext(base)[0]
    exact = [r for r in recordings(source) if os.path.basename(r).lower() == base]
    loose = exact or [r for r in recordings(source) if os.path.splitext(os.path.basename(r).lower())[0] == stem]
    return os.path.join(source, sorted(loose, key=lambda r: (r.count(os.sep), r))[0]) if loose else None


# ---------------------------------------------------------------------------
# 1. Decoding and the MP3 padding

def mp3_gapless(path):
    """(skip, count) from the iTunSMPB tag or the LAME header, or None."""
    with open(path, "rb") as f:
        head = f.read(1 << 18)
    # iTunSMPB in an ID3v2 COMM frame: " 00000000 DELAY PADDING COUNT ..."
    m = re.search(rb"iTunSMPB\x00+ ?([0-9A-Fa-f]{8}) ([0-9A-Fa-f]{8}) ([0-9A-Fa-f]{8}) ([0-9A-Fa-f]{16})", head)
    if m:
        delay, padding, count = int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16)
        return delay + DECODER_DELAY, count, f"iTunSMPB (encoder delay {delay}, padding {padding})"
    # A LAME/Xing header in the first frame.
    offset = 0
    if head[:3] == b"ID3":
        offset = 10 + ((head[6] << 21) | (head[7] << 14) | (head[8] << 7) | head[9])
    lame = head.find(b"LAME", offset, offset + 400)
    xing = max(head.find(b"Xing", offset, offset + 200), head.find(b"Info", offset, offset + 200))
    if lame >= 0 and xing >= 0:
        d = head[lame + 21:lame + 24]
        delay, padding = (d[0] << 4) | (d[1] >> 4), ((d[1] & 15) << 8) | d[2]
        frames = struct.unpack(">I", head[xing + 8:xing + 12])[0] if head[xing + 7] & 1 else 0
        if frames:
            return delay + DECODER_DELAY, frames * 1152 - delay - padding, f"LAME header (delay {delay}, padding {padding})"
    return None


def load(path):
    audio, rate = sf.read(path, dtype="float64", always_2d=True)
    if rate != RATE:
        raise SystemExit(f"{path}: {rate} Hz (expected {RATE})")
    gap = mp3_gapless(path) if path.lower().endswith(".mp3") else None
    note = "no gapless information (nothing trimmed)"
    if gap:
        skip, count, note = gap
        audio = audio[skip:skip + count]
    # The recordings are dual mono; check, then keep one channel's worth.
    left, right = audio[:, 0], audio[:, -1]
    live = np.abs(left) > 1e-4
    corr = float(np.corrcoef(left[live], right[live])[0, 1]) if live.sum() > 1000 and audio.shape[1] > 1 else 1.0
    mono = audio.mean(axis=1)
    # Trailing digital silence (GarageBand exported past the end of the take).
    nz = np.nonzero(np.abs(mono) > 0)[0]
    end = int(nz[-1]) + 1 if len(nz) else 0
    return mono[:end], note, corr, len(mono) - end


# ---------------------------------------------------------------------------
# 2. Noise reduction

NFFT, HOP = 2048, 512


def stft(x):
    _, _, z = signal.stft(x, RATE, window="hann", nperseg=NFFT, noverlap=NFFT - HOP, boundary="even", padded=True)
    return z


def istft(z, length):
    _, x = signal.istft(z, RATE, window="hann", nperseg=NFFT, noverlap=NFFT - HOP, boundary=True)
    return x[:length]


def band_edges():
    # Thirds of an octave from 20 Hz, as FFT bins.
    freqs = np.fft.rfftfreq(NFFT, 1.0 / RATE)
    edges, f = [], 20.0
    while f < RATE / 2:
        edges.append(f)
        f *= 2 ** (1 / 3)
    return freqs, np.array(edges + [RATE / 2])


def room_profile(room):
    p = np.abs(stft(room)) ** 2
    live = p.sum(axis=0) > 1e-12
    return np.median(p[:, live], axis=1)


def fitted_profile(z, room):
    """The room tone's spectrum, scaled per third-octave to this file's quiet frames."""
    p = np.abs(z) ** 2
    energy = p.sum(axis=0)
    live = energy > 1e-12
    quiet = live & (energy <= np.percentile(energy[live], 15))
    if quiet.sum() < 20:
        return room * np.median(np.median(p[:, live], axis=1) / np.maximum(room, 1e-20)), 0
    gaps = np.median(p[:, quiet], axis=1)
    freqs, edges = band_edges()
    ratio = np.ones_like(room)
    for lo, hi in zip(edges[:-1], edges[1:]):
        sel = (freqs >= lo) & (freqs < hi)
        if sel.any():
            ratio[sel] = np.median(gaps[sel] / np.maximum(room[sel], 1e-20))
    # Below the first band: the first band's ratio.
    ratio[freqs < edges[0]] = ratio[(freqs >= edges[0])][0]
    return room * ratio, int(quiet.sum())


def denoise(x, room, max_db=9.0, over=1.6):
    z = stft(x)
    noise, quiet_frames = fitted_profile(z, room)
    p = np.abs(z) ** 2
    floor = 10 ** (-max_db / 20)
    gain = np.sqrt(np.maximum(1.0 - over * noise[:, None] / np.maximum(p, 1e-20), 0.0))
    gain = np.maximum(gain, floor)
    # Smooth across frequency (3 bins) and time (fast attack, ~60 ms release) so the gate
    # does not leave musical noise.
    gain = signal.convolve2d(gain, np.ones((3, 1)) / 3, mode="same", boundary="symm")
    release = math.exp(-HOP / (0.06 * RATE))
    out = gain.copy()
    for t in range(1, out.shape[1]):
        out[:, t] = np.maximum(gain[:, t], release * out[:, t - 1] + (1 - release) * gain[:, t])
    y = istft(z * out, len(x))
    return y, quiet_floor(x), quiet_floor(y), quiet_frames


def quiet_floor(x):
    """The level of the quiet gaps (the 10th percentile of 5 ms frames), dBFS."""
    env = envelope(x)
    return float(np.percentile(env[env > -200], 10))


# ---------------------------------------------------------------------------
# 3. Onsets and slicing

ENV_HOP = 220   # 5 ms


def envelope(x):
    n = len(x) // ENV_HOP
    frames = x[:n * ENV_HOP].reshape(n, ENV_HOP)
    return db(np.sqrt((frames ** 2).mean(axis=1)))


def onsets(env, bowed):
    """Candidate onsets: the level rising sharply out of a quieter stretch."""
    look = 60 if bowed else 20          # how far back the quieter stretch is (frames)
    rise = 14.0 if bowed else 12.0
    floor = np.percentile(env, 10)
    cands = []
    i = look
    while i < len(env) - 1:
        before = env[max(0, i - look):i].min()
        if env[i] - before >= rise and env[i] > floor + 18:
            # The peak within the next 300 ms (bowed: 1 s).
            j = min(len(env), i + (200 if bowed else 60))
            peak = int(i + np.argmax(env[i:j]))
            # Walk back to where it left the quieter level (+3 dB).
            start = i
            while start > 0 and env[start - 1] > before + 3 and i - start < look:
                start -= 1
            cands.append((start, peak, float(env[peak])))
            i = peak + (40 if bowed else 20)
        else:
            i += 1
    return cands, floor


def yin(x, fmin=55.0, fmax=2000.0, frame=2048, hop=512, threshold=0.15):
    """f0 and aperiodicity per frame."""
    tmax, tmin = int(RATE / fmin), max(2, int(RATE / fmax))
    out = []
    for s in range(0, len(x) - frame - tmax, hop):
        w = x[s:s + frame + tmax]
        if np.sqrt(np.mean(w ** 2)) < 1e-5:
            continue
        # Difference function by FFT.
        # r(tau) = sum over the frame of x[j] * x[j + tau]
        n = 1 << int(math.ceil(math.log2(len(w) * 2)))
        acf = np.fft.irfft(np.fft.rfft(w, n) * np.conj(np.fft.rfft(w[:frame], n)), n)[:tmax + 1]
        energy = np.cumsum(np.concatenate(([0.0], w ** 2)))
        e0 = energy[frame] - energy[0]
        et = energy[np.arange(tmax + 1) + frame] - energy[np.arange(tmax + 1)]
        d = np.maximum(e0 + et - 2 * acf, 0.0)
        d[0] = 0
        cmnd = d[1:] * np.arange(1, tmax + 1) / np.maximum(np.cumsum(d[1:]), 1e-20)
        cmnd = np.concatenate(([1.0], cmnd))
        tau = None
        for t in range(tmin, tmax):
            if cmnd[t] < threshold:
                while t + 1 < tmax and cmnd[t + 1] < cmnd[t]:
                    t += 1
                tau = t
                break
        if tau is None:
            tau = int(np.argmin(cmnd[tmin:tmax]) + tmin)
        a, b, c = cmnd[tau - 1], cmnd[tau], cmnd[min(tau + 1, tmax)]
        shift = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0.0
        out.append((RATE / (tau + shift), float(b)))
    return out


def refine(x, f0, partials):
    """f0 from the harmonics' peaks in a long, zero-padded spectrum."""
    n = 1 << 18
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)), n))
    binhz = RATE / n
    estimates, weights = [], []
    for k in range(1, partials + 1):
        target = f0 * k
        lo, hi = int(target * 0.97 / binhz), int(target * 1.03 / binhz) + 1
        if hi >= len(spec) - 1:
            break
        i = lo + int(np.argmax(spec[lo:hi]))
        a, b, c = np.log(spec[i - 1] + 1e-20), np.log(spec[i] + 1e-20), np.log(spec[i + 1] + 1e-20)
        shift = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0.0
        estimates.append((i + shift) * binhz / k)
        weights.append(spec[i] / k)
    if not estimates:
        return f0
    return float(np.average(estimates, weights=weights))


def analysis_segment(x, onset, bowed):
    start = onset + int((0.35 if bowed else 0.07) * RATE)
    return x[start:start + int((1.2 if bowed else 0.6) * RATE)]


def pitch_of(x, onset, bowed, partials, near=None):
    """(MIDI pitch, aperiodicity). With `near` (a MIDI note) the search stays within 8
    semitones of it: a string ringing in sympathy (the guitar's open A, a violin's open
    strings) can be stronger than a high note's own fundamental."""
    seg = analysis_segment(x, onset, bowed)
    if len(seg) < 4096:
        return None, 1.0
    if near is None:
        frames = yin(seg)
    else:
        fmin = 440 * 2 ** ((near - 8 - 69) / 12)
        # Strings ringing far below the search range would still win YIN's dips.
        seg = signal.sosfiltfilt(signal.butter(4, 0.8 * fmin, "highpass", fs=RATE, output="sos"), seg)
        frames = yin(seg, fmin=fmin, fmax=440 * 2 ** ((near + 8 - 69) / 12))
    good = [f for f, ap in frames if ap < 0.3]
    if len(good) < 3:
        return None, (min(ap for _, ap in frames) if frames else 1.0)
    f0 = float(np.median(good))
    f0 = refine(seg[:int(0.5 * RATE)], f0, partials)
    aper = float(np.median([ap for _, ap in frames]))
    return 69 + 12 * math.log2(f0 / 440.0), aper


def harmonic_levels(seg, f0, count=4):
    n = 1 << 17
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n))
    binhz = RATE / n
    out = []
    for k in range(1, count + 1):
        lo, hi = int(f0 * k * 0.98 / binhz), int(f0 * k * 1.02 / binhz) + 1
        out.append(float(db(spec[lo:hi].max())) if hi < len(spec) else -240.0)
    return out


def octave_check(x, onset, bowed, midi):
    """+12 when the note has no odd harmonics of `midi` (it sounds an octave higher)."""
    seg = analysis_segment(x, onset, bowed)
    h = harmonic_levels(seg, 440 * 2 ** ((midi - 69) / 12))
    odd, even = max(h[0], h[2]), max(h[1], h[3])
    return 12 if odd < even - 30 else 0


def sympathetic(y, f0):
    """Notches steady tones below the note's fundamental: other strings ringing in
    sympathy (they are not this note, and would drone in every key it is played in)."""
    removed = []
    if f0 > 250:
        # Above the guitar's G string, nothing of the note lies below half its pitch:
        # a gentle high-pass takes the ringing open strings down first.
        y = signal.sosfiltfilt(signal.butter(2, 0.5 * f0, "highpass", fs=RATE, output="sos"), y)
        removed.append((0.5 * f0, None))
    seg = y[int(0.3 * RATE):int(1.5 * RATE)]
    if len(seg) < RATE // 2 or f0 < 120:
        return y, removed
    n = 1 << 16
    spec = db(np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n)))
    binhz = RATE / n
    top = spec[int(f0 * 0.97 / binhz):int(f0 * 1.03 / binhz)].max()
    lo, hi = int(60 / binhz), int(f0 * 0.85 / binhz)
    region = spec[lo:hi]
    if len(region) < 20:
        return y, []
    base = signal.medfilt(region, 41)
    peaks, _ = signal.find_peaks(region, prominence=12, distance=int(15 / binhz))
    for k in peaks:
        level = region[k]
        if level - base[k] < 15 or level < top - 30:
            continue
        hz = (lo + k) * binhz
        b, a = signal.iirnotch(hz, 12, RATE)
        y = signal.filtfilt(b, a, y)
        removed.append((hz, level - top))
    return y, removed


# ---------------------------------------------------------------------------
# 6. Loops and release tails

def sustain_region(x):
    env = envelope(x)
    if len(env) < 100:
        return None
    sm = np.convolve(env, np.ones(10) / 10, mode="same")
    level = np.percentile(sm, 80)
    above = np.nonzero(sm >= level - 4)[0]
    if len(above) < 40:
        return None
    attack_end = max(int(above[0]) + 20, 30)          # 100 ms past reaching the sustain
    release = int(above[-1])
    return attack_end * ENV_HOP, release * ENV_HOP


def find_loop(x, f0_hz, region):
    """Loop start and end (end exclusive) with the best-matching waveforms."""
    a, b = region
    if b - a < int(0.6 * RATE):
        return None
    period = RATE / f0_hz
    win = 512
    end_lo, end_hi = b - int(0.35 * RATE), b - int(0.05 * RATE)
    start_lo, start_hi = a, a + int(0.4 * (b - a))
    best = None
    # Ends on upward zero crossings near the end; starts anywhere in the window, scored
    # by the normalised difference of the waveforms around them.
    ends = [i for i in range(max(end_lo, win), min(end_hi, len(x) - win)) if x[i - 1] < 0 <= x[i]]
    step = max(1, len(ends) // 40)
    for e in ends[::step]:
        ref = x[e - win:e + win]
        region_s = x[start_lo - win:start_hi + win]
        if len(region_s) < 2 * win + 1:
            continue
        corr = signal.correlate(region_s, ref, mode="valid")
        energy = np.convolve(region_s ** 2, np.ones(2 * win), mode="valid")[:len(corr)]
        diff = energy - 2 * corr + np.sum(ref ** 2)
        norm = diff / np.maximum(energy + np.sum(ref ** 2), 1e-20)
        k = int(np.argmin(norm))
        s = start_lo + k
        length = e - s
        if length < int(0.5 * RATE):
            continue
        cost = float(norm[k])
        # Prefer longer loops a little.
        score = cost - 0.02 * length / RATE
        if best is None or score < best[0]:
            best = (score, s, e, cost)
    if best is None:
        return None
    _, s, e, cost = best
    return s, e, cost, period


def crossfade_loop(x, s, e, xf):
    """Blends the end of the loop into what precedes its start, so the jump is seamless.
    Two stretches of a bowed note are only partly alike, so a plain crossfade dips in
    level where they cancel: the blend is levelled (per 10 ms) to the level the two
    stretches have on their own."""
    xf = min(xf, s, (e - s) // 3)
    if xf <= 0:
        return x
    w = np.linspace(0.0, 1.0, xf)
    a, b = x[e - xf:e], x[s - xf:s]
    y = x.copy()
    blend = a * (1 - w) + b * w
    win = int(0.01 * RATE)
    rms = lambda v: np.sqrt(np.convolve(v ** 2, np.ones(win) / win, mode="same") + 1e-18)
    target = rms(a) * (1 - w) + rms(b) * w
    gain = np.clip(target / rms(blend), 0.5, 2.5)
    gain = np.convolve(gain, np.hanning(2 * win) / np.hanning(2 * win).sum(), mode="same")
    # The ends stay untouched (gain 1) so the blend joins the waveform on both sides.
    edge = np.minimum(1.0, np.minimum(np.arange(xf), np.arange(xf)[::-1]) / win)
    y[e - xf:e] = blend * (1 + (gain - 1) * edge)
    return y


# ---------------------------------------------------------------------------
# The sets

def evidence(x, onset, bowed, midis):
    """How clearly each expected pitch sounds in the note starting at `onset`: the
    prominence (dB, 0..30) of its fundamental or second harmonic over the spectrum
    around it."""
    seg = analysis_segment(x, onset, bowed)
    if len(seg) < 4096:
        return [0.0] * len(midis)
    n = 1 << 16
    spec = db(np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n)))
    binhz = RATE / n
    out = []
    for m in midis:
        f0 = 440 * 2 ** ((m - 69) / 12)
        best = 0.0
        for k in (1, 2):
            lo, hi = int(f0 * k * 2 ** (-0.4 / 12) / binhz), int(f0 * k * 2 ** (0.4 / 12) / binhz) + 1
            wlo, whi = int(f0 * k * 2 ** (-3 / 12) / binhz), int(f0 * k * 2 ** (3 / 12) / binhz) + 1
            if whi >= len(spec):
                continue
            best = max(best, spec[lo:hi].max() - np.median(spec[wlo:whi]))
        out.append(float(np.clip(best, 0.0, 30.0)))
    return out


def pick(cands, x, bowed, partials, sequence):
    """Chooses one onset per expected note, in order (`sequence`: the expected MIDI notes,
    each repeated per take). Each onset is scored by how clearly the expected pitch
    sounds in it and by its level, and the best in-order assignment wins, so a handling
    noise or a thump never shifts the labels of the notes after it."""
    scored = []
    for start, peak, level in cands:
        onset = start * ENV_HOP
        midi, aper = pitch_of(x, onset, bowed, partials)
        scored.append({"start": start, "peak": peak, "level": level, "midi": midi, "aper": aper})
    if not sequence:
        # A varying number of takes: every tonal onset.
        pool = [c for c in scored if c["midi"] is not None and c["aper"] < 0.35]
        return pool, [c for c in scored if c not in pool]
    labels = sorted(set(sequence))
    top = max(c["level"] for c in scored) if scored else 0.0
    for c in scored:
        ev = dict(zip(labels, evidence(x, c["start"] * ENV_HOP, bowed, labels)))
        c["score"] = [ev[m] + 0.25 * max(0.0, 40.0 + c["level"] - top) for m in sequence]
    n, m = len(scored), len(sequence)
    if n < m:
        return scored, []
    # best[i][j]: the best total with the first j notes assigned among the first i onsets.
    neg = -1.0e18
    best = np.full((n + 1, m + 1), neg)
    best[:, 0] = 0.0
    take = np.zeros((n + 1, m + 1), dtype=bool)
    for i in range(1, n + 1):
        for j in range(1, min(i, m) + 1):
            skip = best[i - 1][j]
            use = best[i - 1][j - 1] + scored[i - 1]["score"][j - 1]
            if use >= skip:
                best[i][j], take[i][j] = use, True
            else:
                best[i][j] = skip
    chosen, i, j = [], n, m
    while j > 0:
        if take[i][j]:
            chosen.append(scored[i - 1])
            j -= 1
        i -= 1
    chosen.reverse()
    return chosen, [c for c in scored if c not in chosen]


def slice_note(x, onset_frame, next_frame, env, floor, max_seconds, bowed, lead_db=40.0):
    start = max(0, onset_frame * ENV_HOP - int((0.03 if bowed else 0.006) * RATE))
    stop_limit = len(x) if next_frame is None else next_frame * ENV_HOP - int(0.02 * RATE)
    stop_limit = min(stop_limit, start + int(max_seconds * RATE))
    # The end: the smoothed level fallen to the floor (+5 dB) and staying there.
    sm = np.convolve(env, np.ones(10) / 10, mode="same")
    peak = onset_frame + int(np.argmax(sm[onset_frame:onset_frame + 200]))
    end = stop_limit
    for i in range(peak, min(len(sm), stop_limit // ENV_HOP)):
        if sm[i] < floor + 5:
            end = min(stop_limit, (i + 20) * ENV_HOP)
            break
    # Leading near-silence (a slow bow's onset walked back too far, a finger settling on a
    # string before the pluck): start 10 ms before the level first comes within `lead_db`
    # (40 dB; a plucked set can ask for less) of the note's peak.
    env_note = envelope(x[start:end])
    if len(env_note):
        first = int(np.argmax(env_note >= env_note.max() - lead_db))
        start += max(0, first * ENV_HOP - int(0.01 * RATE))
    y = x[start:end].copy()
    # Fades: a short one in (the attack stays), a longer cosine one out.
    fin = int((0.012 if bowed else 0.002) * RATE)
    y[:fin] *= np.linspace(0, 1, fin)
    fout = int(np.clip(0.15 * len(y), 0.05 * RATE, 0.4 * RATE))
    y[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
    return y, start


def process_set(spec, source, room, out_dir, report):
    sid = spec["id"]
    bowed = spec["kind"] == "bowed"
    partials = spec.get("partials", 3)
    os.makedirs(os.path.join(out_dir, sid), exist_ok=True)
    zones, floors = [], {}
    # Several files can make one velocity layer ("index": the layer they join; the voice's
    # falsetto notes join both the soft and the full layer). Names per layer index.
    names = {}
    for fi, layer in enumerate(spec["layers"]):
        names.setdefault(layer.get("index", fi), layer["name"])
    nlayers = len(names)
    tolerance = spec.get("labelTolerance", 0.5)
    # Layers that are one articulation recorded in two files (the violin's open and
    # stopped pizzicato) become one key map; shared notes are extra round-robins.
    merge = spec.get("mergeLayers", False)
    merged_rr = {}
    for fi, layer in enumerate(spec["layers"]):
        li = layer.get("index", fi)
        path = resolve(source, layer["file"]) or os.path.join(source, layer["file"])
        notes = layer.get("notes", spec.get("notes"))
        expected = [midi_of(n) for n in notes]
        takes = spec.get("takes", 1)
        count = len(expected) * takes if takes > 0 else 0
        if not os.path.exists(path):
            report["problems"].append(f"{sid}: missing file '{layer['file']}'")
            continue
        raw, gap_note, corr, trailing = load(path)
        x, before, after, quiet = denoise(raw, room)
        floors.setdefault(li, []).append(before)
        x = signal.sosfiltfilt(signal.butter(2, 25, "highpass", fs=RATE, output="sos"), x)
        env = envelope(x)
        cands, floor = onsets(env, bowed)
        if spec.get("segmentNotes"):
            # Sung notes (Session 8): long tones between clear gaps. A breath before a note
            # rises like an onset; a sustained stretch of at least `minNote` seconds does not.
            seg = gaps_segments(x, spec.get("noteGap", 0.25), spec.get("noteThresholdDb", -32.0), spec.get("minNote", 1.0))
            cands = []
            for a_, b_ in seg:
                fa, fb = a_ // ENV_HOP, max(a_ // ENV_HOP + 1, b_ // ENV_HOP)
                pk = fa + int(np.argmax(env[fa:fb]))
                # The note's start: where it first comes within 30 dB of its own peak.
                st = fa + int(np.argmax(env[fa:fb] >= env[pk] - 30.0))
                cands.append((st, pk, float(env[pk])))
        sequence = [m for m in expected for _ in range(takes)] if takes > 0 else []
        chosen, rejected = pick(cands, x, bowed, partials, sequence)
        entry = {
            "file": layer["file"], "set": sid, "layer": layer["name"], "padding": gap_note,
            "stereo": "dual mono" if corr > 0.999 else f"stereo (L/R correlation {corr:.3f}, mixed to mono)",
            "trailingSilence": round(trailing / RATE, 2), "noiseFloorDb": round(before, 1),
            "noiseAfterDb": round(after, 1), "quietFrames": quiet,
            "expected": count if count else "varies", "found": len(chosen), "candidates": len(cands),
            "rejected": [f"{c['start'] * ENV_HOP / RATE:.2f}s ({c['level']:.0f} dB)" for c in rejected],
            "notes": [],
        }
        if count and len(chosen) != count:
            report["problems"].append(f"{sid}/{layer['name']}: expected {count} notes, found {len(chosen)}")
        # Assign labels: in order when the count matches; otherwise (or with a varying
        # number of takes) by the nearest expected pitch.
        in_order = takes > 0 and len(chosen) == count
        rr_count = {}
        for k, c in enumerate(chosen):
            nxt = chosen[k + 1]["start"] if k + 1 < len(chosen) else None
            if in_order:
                label = expected[k // takes]
            elif c["midi"] is not None:
                label = min(expected, key=lambda m: min(abs(m + o - c["midi"]) for o in (-12, 0, 12)))
                if min(abs(label + o - c["midi"]) for o in (-12, 0, 12)) > tolerance:
                    entry["rejected"].append(f"{c['start'] * ENV_HOP / RATE:.2f}s (sounds like {name_of(c['midi'])}, not a string)")
                    continue
            else:
                report["problems"].append(f"{sid}/{layer['name']}: note at {c['start'] * ENV_HOP / RATE:.2f}s has no pitch; skipped")
                continue
            onset = c["start"] * ENV_HOP
            y, start = slice_note(x, c["start"], nxt, env, floor, spec.get("maxSeconds", 6.0), bowed, spec.get("leadDb", 40.0))
            midi, _ = pitch_of(x, onset, bowed, partials, near=label)
            note = {"label": name_of(label), "at": round(start / RATE, 2), "seconds": round(len(y) / RATE, 2)}
            where = f"{sid}/{layer['name']} {name_of(label)} at {start / RATE:.2f}s"
            if midi is None:
                found, _ = pitch_of(x, onset, bowed, partials)
                midi = float(label)
                note["detected"] = None
                report["problems"].append(f"{where}: no pitch within 8 semitones of the label"
                                          + (f" (sounds like {name_of(found)})" if found else "") + "; using the label")
            else:
                cents = 100 * (midi - label)
                if abs(cents) >= 50 and abs(abs(cents) - 1200) >= 50 and not spec.get("trustPitch"):
                    # Another pitch won: keep the label when its harmonics are as clear
                    # (a weak fundamental under louder ringing strings).
                    ev_label, ev_found = evidence(x, onset, bowed, [label, int(round(midi))])
                    if ev_label >= ev_found - 3 and ev_label >= 12:
                        report["warnings"].append(f"{where}: the pitch detector heard {name_of(midi)} (other strings ringing louder), "
                                                  f"but {name_of(label)}'s harmonics are as clear; kept the label")
                        midi = float(label)
                        cents = 0.0
                if octave_check(x, onset, bowed, midi):
                    midi += 12
                    report["problems"].append(f"{where}: no odd harmonics of {name_of(label)}; it sounds an octave higher ({name_of(midi)}), mapped there")
                    cents = 100 * (midi - label)
                note["detected"] = f"{name_of(midi)} {cents:+.0f} cents from the label" if abs(cents) >= 50 else f"{cents:+.0f} cents"
                if 50 <= abs(cents) < 1150:
                    report["problems"].append(f"{where}: played {name_of(midi)} ({cents:+.0f} cents from the label); mapped where it sounds")
                elif abs(cents) > 25 and abs(cents) < 50:
                    report["warnings"].append(f"{where}: {cents:+.0f} cents out (the sampler plays it in tune)")
            y, removed = sympathetic(y, 440 * 2 ** ((midi - 69) / 12))
            if removed:
                note["sympathetic"] = ", ".join(f"high-pass {hz:.0f} Hz" if lv is None else f"{hz:.0f} Hz ({name_of(69 + 12 * math.log2(hz / 440))}, {lv:+.0f} dB)" for hz, lv in removed)
            key = (0 if merge else li, label)
            rr = merged_rr.get(key, 0)
            merged_rr[key] = rr + 1
            rr_count[label] = rr_count.get(label, 0) + 1
            zone = {"label": name_of(label), "root": round(midi, 3), "layer": 0 if merge else li, "velocity": layer["velocity"], "rr": rr,
                    "audio": y, "source": layer["file"], "at": round(start / RATE, 3)}
            if bowed and spec.get("loop"):
                region = sustain_region(y)
                loop = find_loop(y, 440 * 2 ** ((midi - 69) / 12), region) if region else None
                if loop:
                    s, e, cost, _ = loop
                    # A held note plays its loop: its pitch there is the note's pitch (the
                    # bow's onset and other strings ringing pull the first second off).
                    frames = yin(y[s:e], fmin=440 * 2 ** ((label - 8 - 69) / 12), fmax=440 * 2 ** ((label + 8 - 69) / 12))
                    steady = [f for f, ap in frames if ap < 0.3]
                    if len(steady) >= 5:
                        spread = float(np.std([1200 * math.log2(f / np.median(steady)) for f in steady]))
                        zone["wander"] = spread
                        note["loop wander"] = f"{spread:.0f} cents"
                        held_midi = 69 + 12 * math.log2(float(np.median(steady)) / 440.0)
                        if abs(held_midi - midi) > 0.05:
                            note["detected"] += f", held {100 * (held_midi - label):+.0f} cents"
                        midi = held_midi
                        zone["root"] = round(midi, 3)
                    zone["audio"] = crossfade_loop(y, s, e, int(0.15 * RATE))
                    zone["loop"] = [s, e]
                    zone["release"] = int(region[1])
                    note["loop"] = f"{s / RATE:.2f}-{e / RATE:.2f}s (match {cost:.3f})"
                    if cost > 0.1:
                        report["warnings"].append(f"{sid} {name_of(label)} take {rr + 1}: the best loop matches poorly ({cost:.3f}); crossfaded")
                else:
                    report["problems"].append(f"{sid} {name_of(label)} take {rr + 1}: no sustain loop found (too short)")
            entry["notes"].append(note)
            zones.append(zone)
        entry["found"] = len(entry["notes"])
        report["files"].append(entry)
        if takes == 0:
            entry["takesPerNote"] = {name_of(k): v for k, v in sorted(rr_count.items())}
        for m in expected:
            if m not in rr_count:
                report["problems"].append(f"{sid}/{layer['name']}: no note matched {name_of(m)}")
    if not zones:
        return None
    # A held take whose pitch wanders (more than 12 cents, standard deviation, across its
    # loop) would wobble every time the loop turns: dropped when the note has other takes.
    for z in [z for z in zones if z.get("wander", 0) > spec.get("maxWander", 12)]:
        others = [o for o in zones if o is not z and o["label"] == z["label"] and o["layer"] == z["layer"] and o.get("wander", 0) <= 12]
        if others:
            zones.remove(z)
            report["warnings"].append(f"{sid} {z['label']} take {z['rr'] + 1}: dropped, its pitch wanders while held ({z['wander']:.0f} cents); "
                                      f"{len(others)} steady take{'s' if len(others) > 1 else ''} left")
    for key in {(z["layer"], z["label"]) for z in zones}:
        for i, z in enumerate(sorted([z for z in zones if (z["layer"], z["label"]) == key], key=lambda z: z["rr"])):
            z["rr"] = i

    # The layers of one instrument were recorded at different input levels (the soft
    # takes turned up). The room's noise is the same in all of them, so each layer is
    # levelled by its noise floor: the dynamics between the layers come back.
    if len(floors) > 1 and not merge and not spec.get("noFloorLevelling"):
        per = {li: float(np.mean(f)) for li, f in floors.items()}
        mean = float(np.mean(list(per.values())))
        trims = {li: mean - f for li, f in per.items()}
        for z in zones:
            z["audio"] = z["audio"] * 10 ** (trims[z["layer"]] / 20)
        report["levelling"][sid] = {names[i]: {"floorDb": round(per[i], 1), "trimDb": round(trims[i], 1)} for i in sorted(per)}
    # The noise floors bring the layers near each other, but not into a usable order: the
    # player's touch changed across the register too (the medium guitar takes match the
    # hard ones on the low strings and fall 12-20 dB under them above G4). With
    # "layerStepsDb", each note's layers are set that far apart (K-weighted loudness),
    # around the note's own level (the median over its layers), so a velocity never jumps
    # to a much quieter or louder take; each take keeps its own tone.
    steps = spec.get("layerStepsDb")
    if steps and nlayers == len(steps):
        changes = []
        for label in sorted({z["label"] for z in zones}, key=midi_of):
            mine = [z for z in zones if z["label"] == label]
            level = {li: float(np.mean([note_loudness(z["audio"]) for z in mine if z["layer"] == li]))
                     for li in {z["layer"] for z in mine}}
            centre = float(np.median([level[li] - steps[li] for li in level]))
            for z in mine:
                trim = float(np.clip(centre + steps[z["layer"]] - note_loudness(z["audio"]), -24.0, 18.0))
                z["audio"] = z["audio"] * 10 ** (trim / 20)
                changes.append(trim)
            for li in level:
                report["layerSteps"].setdefault(sid, {}).setdefault(label, {})[names[li]] = round(float(np.clip(
                    centre + steps[li] - level[li], -24.0, 18.0)), 1)
        report["warnings"].extend(f"{sid}: a layer step needed more than the limit ({c:+.1f} dB)" for c in changes if c in (-24.0, 18.0))
    # 5. One gain for the whole instrument: its loudest peak at -1 dBFS.
    peak = max(np.abs(z["audio"]).max() for z in zones)
    gain = 10 ** (-1 / 20) / max(peak, 1e-9)
    # Loudness per note (the first 300 ms), and the layers' order.
    for z in zones:
        z["audio"] = z["audio"] * gain
        # From the loudest 10 ms in the first 1.5 s (a bow takes a while to speak).
        env_z = envelope(z["audio"][:int(1.5 * RATE)])
        top = int(np.argmax(np.convolve(env_z, np.ones(2) / 2, mode="same"))) * ENV_HOP
        head = z["audio"][top:top + int(0.3 * RATE)]
        z["rms"] = round(float(db(np.sqrt(np.mean(head ** 2)))), 2)
        z["peak"] = round(float(db(np.abs(z["audio"]).max())), 2)
        z["loudness"] = round(note_loudness(z["audio"]), 2)
    layers = nlayers
    if layers > 1 and not merge:
        for label in sorted({z["label"] for z in zones}, key=midi_of):
            levels = [next((z["loudness"] for z in zones if z["label"] == label and z["layer"] == li), None) for li in range(layers)]
            if None not in levels and any(levels[i] >= levels[i + 1] for i in range(layers - 1)):
                report["warnings"].append(f"{sid} {label}: the velocity layers are not in loudness order "
                                          f"({', '.join(f'{v:.1f}' for v in levels)} dB); kept as labelled")

    # Key ranges: each recorded pitch covers up to halfway to its neighbours; the ends
    # stretch a fifth (the sampler folds anything further by octaves).
    mapping_zones = []
    for z in zones:
        # Within the zone's own layer (a layer may lack a note another one has).
        roots = sorted({round(q["root"], 1) for q in zones if q["layer"] == z["layer"]})
        r = round(z["root"], 1)
        lower = [q for q in roots if q < round(r, 1) - 0.5]
        upper = [q for q in roots if q > round(r, 1) + 0.5]
        lo = math.floor((max(lower) + r) / 2) + 1 if lower else int(round(r)) - 7
        hi = math.floor((min(upper) + r) / 2) if upper else int(round(r)) + 7
        name = f"{z['label'].replace('#', 's')}-{names[z['layer']] if not merge else spec['id'].split('-')[-1]}-{z['rr'] + 1}.flac"
        sf.write(os.path.join(out_dir, sid, name), z["audio"].astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
        mz = {"file": f"{sid}/{name}", "label": z["label"], "root": z["root"], "loKey": lo, "hiKey": hi,
              "loVel": z["velocity"][0], "hiVel": z["velocity"][1], "layer": z["layer"], "rr": z["rr"],
              "length": len(z["audio"]), "rms": z["rms"], "loudness": z["loudness"], "peak": z["peak"], "source": z["source"], "at": z["at"]}
        if "loop" in z:
            mz["loopStart"], mz["loopEnd"] = z["loop"]
            mz["release"] = z["release"]
        mapping_zones.append(mz)
    mapping_zones.sort(key=lambda m: (m["root"], m["layer"], m["rr"]))
    mapping = {"id": sid, "name": spec["name"], "kind": spec["kind"], "rate": RATE, "channels": 1,
               "layers": [names[i] for i in sorted(names)] if not merge else [spec["id"].split("-")[-1]], "gainDb": round(20 * math.log10(gain), 2), "zones": mapping_zones}
    if spec.get("envelope"):
        mapping["envelopeHz"], mapping["envelopeDb"] = spectral_envelope(zones)
    with open(os.path.join(out_dir, f"{sid}.json"), "w") as f:
        json.dump(mapping, f, indent=1)
    return mapping



# ---------------------------------------------------------------------------
# Session 8: hits (drums, the banjo's open fifth string) and textures (whispers,
# breaths, a hummed melody). Both are processed first and written once every set of
# their gain group is known, so the strokes of one drum keep their levels between them.

def hit_onsets(env, count, top_db=45.0):
    """Starts of `count` hits (5 ms frames): a rise out of the quiet before it, relaxed
    until enough are found; with too many, the smallest rises go first (a hand settling
    on the skin, a breath, a chair)."""
    sm = np.convolve(env, np.ones(3) / 3, mode="same")
    top = float(np.percentile(sm, 99.8))
    found = []
    for need in (24.0, 20.0, 16.0, 12.0, 9.0):
        found, i = [], 30
        while i < len(sm) - 1:
            before = sm[max(0, i - 30):i].min()
            ahead = sm[i:i + 20].max()
            if sm[i] - before > 6 and ahead - before >= need and ahead > top - top_db:
                start = i
                while start > 0 and sm[start - 1] > before + 3 and i - start < 10:
                    start -= 1
                peak = i + int(np.argmax(sm[i:i + 60]))
                level = float(sm[peak])
                # How long it sounds within 20 dB of its peak: a hit rings, a click does not.
                ring = int(np.sum(sm[peak:peak + 200] > level - 20.0))
                if i < len(sm) - 30:       # not the file's last 150 ms (an edit's click)
                    found.append({"start": start, "peak": peak, "rise": float(ahead - before), "level": level,
                                  "score": level + 10.0 * math.log10(max(ring, 1))})
                i = peak + 40          # 200 ms: a hit's own bounce is not a new hit
            else:
                i += 1
        if len(found) >= count:
            break
    extra = []
    while len(found) > count:
        k = min(range(len(found)), key=lambda j: found[j]["score"])
        extra.append(found.pop(k))
    return found, extra


def process_hits(spec, source, room, report):
    """One recording of `takes` hits per dynamic, soft to hard: velocity layers and
    round-robins. Returns zones (audio not yet levelled)."""
    sid = spec["id"]
    path = resolve(source, spec["file"])
    layers, takes = spec.get("layers", ["soft", "medium", "hard"]), spec.get("takes", 3)
    count = len(layers) * takes
    if path is None:
        report["problems"].append(f"{sid}: missing file '{spec['file']}'")
        return []
    raw, gap_note, corr, trailing = load(path)
    x, before, after, quiet = denoise(raw, room, max_db=spec.get("denoiseDb", 9.0))
    x = signal.sosfiltfilt(signal.butter(2, spec.get("highPass", 25), "highpass", fs=RATE, output="sos"), x)
    env = envelope(x)
    found, extra = hit_onsets(env, count)
    floor = float(np.percentile(env[env > -200], 10))
    entry = {"file": spec["file"], "set": sid, "layer": "/".join(layers), "padding": gap_note,
             "stereo": "dual mono" if corr > 0.999 else f"stereo (L/R correlation {corr:.3f}, mixed to mono)",
             "trailingSilence": round(trailing / RATE, 2), "noiseFloorDb": round(before, 1), "noiseAfterDb": round(after, 1),
             "quietFrames": quiet, "expected": count, "found": len(found), "candidates": len(found) + len(extra),
             "rejected": [f"{c['start'] * ENV_HOP / RATE:.2f}s ({c['level']:.0f} dB, rose {c['rise']:.0f} dB)" for c in extra], "notes": []}
    report["files"].append(entry)
    if len(found) != count:
        report["problems"].append(f"{sid}: expected {count} hits, found {len(found)}")
    zones = []
    sm = np.convolve(env, np.ones(10) / 10, mode="same")
    for k, c in enumerate(found):
        stop = found[k + 1]["start"] - 4 if k + 1 < len(found) else len(env)
        stop = min(stop, c["start"] + int(spec.get("maxSeconds", 3.0) * RATE / ENV_HOP))
        peak = c["peak"]
        end = stop
        for i in range(peak, stop):
            if sm[i] < max(floor + 4, sm[peak] - spec.get("rangeDb", 60.0)):
                end = min(stop, i + 10)
                break
        start = max(0, c["start"] * ENV_HOP - int(0.003 * RATE))
        y = x[start:end * ENV_HOP].copy()
        head = envelope(y[:int(0.3 * RATE)])
        if len(head):
            start_shift = max(0, int(np.argmax(head >= head.max() - 36.0)) * ENV_HOP - int(0.003 * RATE))
            y = y[start_shift:]
            start += start_shift
        fin = int(0.001 * RATE)
        y[:fin] *= np.linspace(0, 1, fin)
        fout = int(np.clip(0.2 * len(y), 0.02 * RATE, 0.25 * RATE))
        y[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
        layer, rr = min(k // takes, len(layers) - 1), k % takes
        # Pitch: a pitched hit (the banjo's string, a drum's bass tone) is tuned to what it
        # sounds; an unpitched one keeps the nominal root.
        root, detected = float(spec.get("root", 60)), "unpitched"
        frames = yin(y[int(0.03 * RATE):int(0.5 * RATE)], fmin=spec.get("fmin", 40.0), fmax=spec.get("fmax", 1500.0))
        good = [f for f, ap in frames if ap < 0.2]
        if len(good) >= 3:
            heard = 69 + 12 * math.log2(float(np.median(good)) / 440.0)
            if "label" in spec:
                label = midi_of(spec["label"])
                if abs(heard - label) < 1.0:
                    root = refine_midi(y, heard)
                    detected = f"{name_of(root)} {100 * (root - label):+.0f} cents"
                else:
                    detected = f"heard {name_of(heard)}, not {spec['label']}; nominal"
                    report["warnings"].append(f"{sid} hit {k + 1}: {detected}")
            else:
                detected = f"{name_of(heard)} {100 * (heard - round(heard)):+.0f} cents (a drum's tone; nominal root kept)"
        note = {"label": f"{layers[layer]} {rr + 1}", "at": round(start / RATE, 2), "seconds": round(len(y) / RATE, 2), "detected": detected}
        entry["notes"].append(note)
        zones.append({"label": spec.get("label", name_of(root)), "root": round(root, 3), "layer": layer, "rr": rr, "audio": y,
                      "velocity": spec.get("velocities", [[0.0, 0.42], [0.42, 0.75], [0.75, 1.0]])[layer],
                      "source": spec["file"], "at": round(start / RATE, 3), "loKey": 0, "hiKey": 127})
    # Velocity layers by how loud each hit is (Session 8): the recording's order says what
    # was meant, but a "medium" hit louder than the hard ones would make a key jump down in
    # level as the velocity rises. Reordered only when the order differs; reported.
    if spec.get("orderByLoudness", True) and len(zones) == count:
        recorded = [(z["layer"], z["rr"]) for z in zones]
        ranked = sorted(range(len(zones)), key=lambda i: note_loudness(zones[i]["audio"]))
        vels = spec.get("velocities", [[0.0, 0.42], [0.42, 0.75], [0.75, 1.0]])
        for rank, i in enumerate(ranked):
            zones[i]["layer"], zones[i]["rr"] = rank // takes, rank % takes
            zones[i]["velocity"] = vels[rank // takes]
        moved = [f"{layers[recorded[i][0]]} {recorded[i][1] + 1} -> {layers[zones[i]['layer']]}" for i in range(len(zones)) if recorded[i][0] != zones[i]["layer"]]
        if moved:
            report["warnings"].append(f"{sid}: velocity layers assigned by loudness, not as recorded: {', '.join(moved)}")
            entry["reordered"] = moved
    # "layerStepsDb": the dynamics set that far apart (mean loudness per layer, around the
    # hardest), each hit keeping its own tone. The djembes' soft hits were played 15-20 dB
    # under the medium ones: 6 dB steps let the velocity rise evenly across the layers.
    steps = spec.get("layerStepsDb")
    if steps and len(steps) == len(layers) and len(zones) == count:
        mean = {li: float(np.mean([note_loudness(z["audio"]) for z in zones if z["layer"] == li])) for li in range(len(layers))}
        top = mean[len(layers) - 1]
        trims = {li: float(np.clip(top + steps[li] - mean[li], -18.0, 18.0)) for li in mean}
        for z in zones:
            z["audio"] = z["audio"] * 10 ** (trims[z["layer"]] / 20)
        entry["layerTrimsDb"] = {layers[li]: round(t, 1) for li, t in trims.items()}
    # The dynamics should rise: soft < medium < hard (mean loudness per layer).
    means = [float(np.mean([note_loudness(z["audio"]) for z in zones if z["layer"] == li] or [-120])) for li in range(len(layers))]
    entry["layerLoudness"] = [round(m, 1) for m in means]
    if any(means[i] >= means[i + 1] for i in range(len(means) - 1)):
        report["warnings"].append(f"{sid}: the dynamics do not rise ({', '.join(f'{m:.1f}' for m in means)} dB)")
    for li in range(len(layers)):
        lv = [note_loudness(z["audio"]) for z in zones if z["layer"] == li]
        if len(lv) > 1 and max(lv) - min(lv) > 8:
            report["warnings"].append(f"{sid}/{layers[li]}: its {len(lv)} hits differ by {max(lv) - min(lv):.0f} dB")
    return zones


def refine_midi(y, midi):
    seg = y[int(0.05 * RATE):int(0.6 * RATE)]
    if len(seg) < 4096:
        return midi
    return 69 + 12 * math.log2(refine(seg, 440 * 2 ** ((midi - 69) / 12), 3) / 440.0)


def gaps_segments(x, gap_s, thr_db, min_s):
    """Phrases separated by at least `gap_s` of quiet (thr_db under the peak)."""
    env = envelope(x)
    sm = np.convolve(env, np.ones(8) / 8, mode="same")
    on = sm > sm.max() + thr_db
    gap = max(1, int(gap_s * RATE / ENV_HOP))
    out, i, n = [], 0, len(on)
    while i < n:
        if not on[i]:
            i += 1
            continue
        j = i
        while j < n and on[j:j + gap].any():
            j += 1
        if (j - i) * ENV_HOP >= min_s * RATE:
            out.append((max(0, i * ENV_HOP - int(0.05 * RATE)), min(len(x), j * ENV_HOP + int(0.15 * RATE))))
        i = j
    return out


def process_texture(spec, source, room, report):
    """Unpitched or freely pitched recordings (whispers, breaths, a hummed melody, a
    ghostly glide): denoised, each file whole or cut into its phrases, every piece a
    round-robin the instrument picks from."""
    sid = spec["id"]
    zones = []
    for fi, fname in enumerate(spec["files"]):
        path = resolve(source, fname)
        if path is None:
            report["problems"].append(f"{sid}: missing file '{fname}'")
            continue
        raw, gap_note, corr, trailing = load(path)
        x, before, after, quiet = denoise(raw, room, max_db=spec.get("denoiseDb", 9.0))
        x = signal.sosfiltfilt(signal.butter(2, spec.get("highPass", 60), "highpass", fs=RATE, output="sos"), x)
        pieces = gaps_segments(x, spec.get("gap", 0.6), spec.get("thresholdDb", -40.0), spec.get("minSeconds", 0.4)) if spec.get("segment") else [(0, len(x))]
        if not spec.get("segment"):
            live = np.nonzero(envelope(x) > envelope(x).max() - 45)[0]
            if len(live):
                pieces = [(max(0, int(live[0]) * ENV_HOP - int(0.05 * RATE)), min(len(x), int(live[-1]) * ENV_HOP + int(0.3 * RATE)))]
        entry = {"file": fname, "set": sid, "layer": "texture", "padding": gap_note,
                 "stereo": "dual mono" if corr > 0.999 else f"stereo (L/R correlation {corr:.3f}, mixed to mono)",
                 "trailingSilence": round(trailing / RATE, 2), "noiseFloorDb": round(before, 1), "noiseAfterDb": round(after, 1),
                 "quietFrames": quiet, "expected": "varies", "found": len(pieces), "candidates": len(pieces), "rejected": [], "notes": []}
        report["files"].append(entry)
        for a, b in pieces:
            y = x[a:b].copy()
            if spec.get("maxSeconds") and len(y) > spec["maxSeconds"] * RATE:
                y = y[:int(spec["maxSeconds"] * RATE)]
            fin, fout = int(0.02 * RATE), int(min(0.3, 0.2 * len(y) / RATE) * RATE)
            y[:fin] *= np.linspace(0, 1, fin)
            y[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
            root, detected = float(midi_of(spec["label"])) if "label" in spec else 60.0, "unpitched"
            if spec.get("pitched"):
                frames = yin(y, fmin=60, fmax=900, frame=2048, hop=1024)
                good = [f for f, ap in frames if ap < 0.25]
                heard = 69 + 12 * math.log2(float(np.median(good)) / 440.0) if len(good) >= 5 else None
                if heard is not None and ("label" not in spec or abs(heard - root) < spec.get("labelTolerance", 1.5)):
                    root = heard
                    detected = f"median {name_of(root)} {100 * (root - round(root)):+.0f} cents ({len(good)} steady frames)"
                elif "label" in spec:
                    detected = f"no steady pitch near {spec['label']} (rough); nominal {spec['label']}"
            entry["notes"].append({"label": f"piece {len(zones) + 1}", "at": round(a / RATE, 2), "seconds": round(len(y) / RATE, 2), "detected": detected})
            zones.append({"label": name_of(root), "root": round(root, 3), "layer": 0, "rr": len(zones), "audio": y, "velocity": [0.0, 1.0],
                          "source": fname, "at": round(a / RATE, 3), "loKey": 0, "hiKey": 127})
        entry["found"] = len(entry["notes"])
    # Pieces of one texture at one loudness (a whisper picked at random is never a jump).
    if spec.get("evenLoudness", True) and zones:
        target = float(np.median([note_loudness(z["audio"]) for z in zones]))
        for z in zones:
            z["audio"] = z["audio"] * 10 ** (float(np.clip(target - note_loudness(z["audio"]), -12, 12)) / 20)
    return zones


ENVELOPE_HZ = [125.0 * 2 ** (k / 3) for k in range(19)]     # 125 Hz .. 8 kHz, thirds of an octave


def spectral_envelope(zones):
    """The voice's spectral envelope (its formants and breath), dB per third of an octave,
    loudest band 0 dB: the log power spectrum of every take's held part, averaged over
    all takes and pitches (the harmonics of different notes fall in different places, so
    what is left is the shape of the voice). The FX's choir sings through it."""
    acc, count = None, 0
    for z in zones:
        a = z["audio"]
        s0, e0 = z.get("loop", (int(0.3 * RATE), min(len(a), int(1.8 * RATE))))
        seg = a[s0:e0]
        if len(seg) < 4096:
            continue
        f, pxx = signal.welch(seg, RATE, nperseg=4096)
        logp = 10 * np.log10(pxx + 1e-20)
        acc = logp if acc is None else acc + logp
        count += 1
    if acc is None:
        return ENVELOPE_HZ, [0.0] * len(ENVELOPE_HZ)
    acc /= count
    bands = []
    for c in ENVELOPE_HZ:
        sel = (f >= c / 2 ** (1 / 6)) & (f < c * 2 ** (1 / 6))
        bands.append(float(np.mean(acc[sel])) if sel.any() else -60.0)
    top = max(bands)
    return [round(c, 1) for c in ENVELOPE_HZ], [round(b - top, 2) for b in bands]


def write_zones(spec, zones, gain, out_dir, layer_names):
    """Writes levelled zones (hits, textures) and their mapping."""
    sid = spec["id"]
    os.makedirs(os.path.join(out_dir, sid), exist_ok=True)
    out = []
    for z in zones:
        a = z["audio"] * gain
        name = f"{z['label'].replace('#', 's')}-{layer_names[z['layer']]}-{z['rr'] + 1}.flac"
        sf.write(os.path.join(out_dir, sid, name), a.astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
        env_z = envelope(a[:int(1.5 * RATE)])
        top = int(np.argmax(np.convolve(env_z, np.ones(2) / 2, mode="same"))) * ENV_HOP if len(env_z) else 0
        head = a[top:top + int(0.3 * RATE)]
        out.append({"file": f"{sid}/{name}", "label": z["label"], "root": z["root"], "loKey": z["loKey"], "hiKey": z["hiKey"],
                    "loVel": z["velocity"][0], "hiVel": z["velocity"][1], "layer": z["layer"], "rr": z["rr"], "length": len(a),
                    "rms": round(float(db(np.sqrt(np.mean(head ** 2) + 1e-24))), 2), "loudness": round(note_loudness(a), 2),
                    "peak": round(float(db(np.abs(a).max())), 2), "source": z["source"], "at": z["at"]})
    out.sort(key=lambda m: (m["layer"], m["rr"]))
    mapping = {"id": sid, "name": spec["name"], "kind": spec["kind"], "rate": RATE, "channels": 1, "layers": layer_names,
               "gainDb": round(20 * math.log10(gain), 2), "zones": out}
    with open(os.path.join(out_dir, f"{sid}.json"), "w") as f:
        json.dump(mapping, f, indent=1)
    return mapping


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--source", required=True, help="folder with the recordings (the Samples repository)")
    ap.add_argument("--out", required=True, help="output folder (not in the repository)")
    ap.add_argument("--manifest", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "manifest.json"))
    ap.add_argument("--only", help="comma-separated set ids")
    args = ap.parse_args()

    with open(args.manifest) as f:
        manifest = json.load(f)
    os.makedirs(args.out, exist_ok=True)
    room_path = resolve(args.source, manifest["roomTone"])
    if room_path is None:
        raise SystemExit(f"room tone '{manifest['roomTone']}' not found below {args.source}")
    room_raw, room_note, _, _ = load(room_path)
    room = room_profile(room_raw)
    report = {"roomTone": {"file": manifest["roomTone"], "seconds": round(len(room_raw) / RATE, 1), "padding": room_note,
                           "levelDb": round(float(db(np.sqrt(np.mean(room_raw ** 2)))), 1)},
              "files": [], "problems": [], "warnings": [], "sets": {}, "levelling": {}, "layerSteps": {}}
    report["patches"] = {}
    known = {manifest["roomTone"]} | {u["file"] for u in manifest.get("unused", [])}
    only = set(args.only.split(",")) if args.only else None
    index = []
    synths = []
    grouped = []       # hits and textures: written once their gain group is complete
    for spec in manifest["sets"]:
        if spec["kind"] in ("synth", "chromatic"):
            known.add(spec["file"])
            if not only or spec["id"] in only:
                synths.append(spec)
            continue
        if spec["kind"] in ("hits", "texture"):
            known |= {spec["file"]} if spec["kind"] == "hits" else set(spec["files"])
            if not only or spec["id"] in only:
                print(f"[{spec['id']}] ...", flush=True)
                zones = process_hits(spec, args.source, room, report) if spec["kind"] == "hits" else process_texture(spec, args.source, room, report)
                if zones:
                    grouped.append((spec, zones))
            continue
        known |= {l["file"] for l in spec["layers"]}
        if only and spec["id"] not in only:
            continue
        print(f"[{spec['id']}] ...", flush=True)
        mapping = process_set(spec, args.source, room, args.out, report)
        if mapping:
            index.append(spec["id"])
            seconds = sum(z["length"] for z in mapping["zones"]) / RATE
            report["sets"][spec["id"]] = {"zones": len(mapping["zones"]), "seconds": round(seconds, 1),
                                          "loops": sum(1 for z in mapping["zones"] if "loopStart" in z)}
    for group in dict.fromkeys(sp.get("gainGroup", sp["id"]) for sp, _ in grouped):
        members = [(sp, zs) for sp, zs in grouped if sp.get("gainGroup", sp["id"]) == group]
        peak = max(np.abs(z["audio"]).max() for _, zs in members for z in zs)
        gain = 10 ** (members[0][0].get("peakDb", -1.0) / 20) / max(peak, 1e-9)
        for sp, zs in members:
            names = sp.get("layers", ["soft", "medium", "hard"]) if sp["kind"] == "hits" else ["texture"]
            mapping = write_zones(sp, zs, gain, args.out, names)
            index.append(sp["id"])
            seconds = sum(z["length"] for z in mapping["zones"]) / RATE
            report["sets"][sp["id"]] = {"zones": len(mapping["zones"]), "seconds": round(seconds, 1), "loops": 0}
    # The synth sweeps (sweeps.py uses this module's helpers, so it is imported here).
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import sweeps
    for mapping in sweeps.run(synths, args.source, args.out, report):
        index.append(mapping["id"])
        seconds = sum(z["length"] for z in mapping["zones"]) / RATE
        report["sets"][mapping["id"]] = {"zones": len(mapping["zones"]), "seconds": round(seconds, 1),
                                         "loops": sum(1 for z in mapping["zones"] if "loopStart" in z)}
    known_paths = {resolve(args.source, k) for k in known} - {None}
    for rel in recordings(args.source):
        if os.path.join(args.source, rel) not in known_paths:
            report["problems"].append(f"'{rel}' is not in the manifest (not used)")
    with open(os.path.join(args.out, "index.json"), "w") as f:
        json.dump({"version": 1, "instruments": index}, f, indent=1)
    # The FX's own small set (Session 8: the voice in its places), shipped in its bundle.
    fx = [sp["id"] for sp in manifest["sets"] if sp.get("fx") and sp["id"] in index]
    with open(os.path.join(args.out, "index-fx.json"), "w") as f:
        json.dump({"version": 1, "instruments": fx}, f, indent=1)
    total = sum(os.path.getsize(os.path.join(dp, fn)) for dp, _, fns in os.walk(args.out) for fn in fns if fn.endswith(".flac"))
    report["flacBytes"] = total
    with open(os.path.join(args.out, "report.json"), "w") as f:
        json.dump(report, f, indent=1)

    # The report, readable.
    print(f"\nRoom tone: {report['roomTone']['seconds']} s at {report['roomTone']['levelDb']} dBFS RMS ({room_note})")
    for e in report["files"]:
        if "padding" in e:
            print(f"\n{e['file']} -> {e['set']}/{e['layer']}: {e['found']} notes (expected {e['expected']}, {e['candidates']} candidates), "
                  f"{e['stereo']}, padding: {e['padding']}, noise floor {e['noiseFloorDb']} -> {e['noiseAfterDb']} dBFS")
        else:
            print(f"\n{e['file']} -> {e['set']}: {e['found']} notes (expected {e['expected']}, {e['candidates']} candidates, rise {e['riseNeededDb']:.0f} dB), "
                  f"{e['format']}, {e['stereo']}, {e.get('range', '?')}, floor {e['noiseFloorDb']} dBFS, sounding {e.get('sounding', '?')} s (median)")
        for n in e["notes"]:
            print(f"   {n['label']:4} at {n['at']:7.2f}s  {n['seconds']:5.2f}s  pitch {n['detected']}" + (f"  held {n['held']}s" if "held" in n else "")
                  + (f"  loop {n['loop']}" if "loop" in n else "")
                  + (f"  notched: {n['sympathetic']}" if "sympathetic" in n else ""))
        if e["rejected"]:
            print(f"   not notes: {', '.join(e['rejected'])}")
        if "takesPerNote" in e:
            print(f"   takes per note: {e['takesPerNote']}")
    print()
    for sid, s in report["sets"].items():
        # One line per sample set, also for the CI log ("Prepared: N notes").
        print(f"Prepared: {s['zones']} notes  {sid}  ({s['seconds']} s of audio, {s['loops']} loops)")
    for pid, pt in report["patches"].items():
        print(f"{pid}: gain {pt['gainDb']:+.1f} dB (both articulations), sounding " + ", ".join(f"{k} {v} s" for k, v in pt["sounding"].items()))
    for sid, lv in report["levelling"].items():
        print(f"{sid}: layers levelled by their noise floors: " + ", ".join(f"{k} {v['floorDb']} dB -> {v['trimDb']:+.1f} dB" for k, v in lv.items()))
    for sid, notes in report["layerSteps"].items():
        print(f"{sid}: layer steps per note (dB): " + "; ".join(f"{label} " + " ".join(f"{k} {v:+.1f}" for k, v in t.items()) for label, t in notes.items()))
    print(f"\nFLAC total: {total / 1e6:.2f} MB")
    print(f"\nCould not match ({len(report['problems'])}):" if report["problems"] else "\nEverything matched.")
    for p in report["problems"]:
        print(f" - {p}")
    if report["warnings"]:
        print(f"\nWarnings ({len(report['warnings'])}):")
        for w in report["warnings"]:
            print(f" - {w}")


if __name__ == "__main__":
    main()
