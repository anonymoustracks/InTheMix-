#!/usr/bin/env python3
"""Anchor bench: pair-aware IN/OUT anchors against today's, for blind A/B listening.

Prototype for docs/research/auto-anchors.md (GlideMix 1.3.8 plan, Chris 2026-11-16).
Plain Python 3, no packages; ffmpeg renders the audio. Reads a COPY of the GlideMix
library (never the live one) plus the analysis files next to it, read-only.

For every song pair A -> B it works out
  * TODAY: the app's current default anchors (MixHouseBeatGrids::sourceAnchors,
    ported: GlideMix's own phrases, 8-bar steps, the 15 % / 40 % loudness rules) and
    the default Volume Fade of 8 bars;
  * NEW: pair-aware anchors -- every phrase line of A's last part against every phrase
    line of B's first part, blend 32 / 16 / 8 bars, scored on DJ practice;
and renders both transitions the MixMeister way GlideMix plays them: B is silent before
its IN, both play together from the shared anchor (A's OUT = B's IN) for the blend,
A falls 0 -> -6 dB and is cut, B rises -6 -> 0 dB.

Song names are private: output names songs by library id only.

  anchorbench.py --library COPY.sqlite pairs --count 20 --seed 1 --out DIR
  anchorbench.py --library COPY.sqlite pairs --ids 123,456,789 --out DIR   (a chain)
  anchorbench.py ... --no-audio   (scores and checks only)
"""
import argparse
import json
import math
import os
import random
import sqlite3
import struct
import subprocess
import sys
import zlib

# ---------------------------------------------------------------- keys (Mixxx ids)
# 1..12 major C..B, 13..24 minor C..B; Open Key number -> (major, minor), from
# mixhousekeymatch.cpp.
OPEN_KEY = {1: (1, 22), 2: (8, 17), 3: (3, 24), 4: (10, 19), 5: (5, 14), 6: (12, 21),
            7: (7, 16), 8: (2, 23), 9: (9, 18), 10: (4, 13), 11: (11, 20), 12: (6, 15)}


def open_key_number(key):
    for n, (maj, mnr) in OPEN_KEY.items():
        if key in (maj, mnr):
            return n
    return 0


def camelot(key):
    n = open_key_number(key)
    if not n:
        return "?"
    major = key <= 12
    # Open Key n = Camelot ((n + 6) mod 12) + 1 with d -> B, m -> A
    c = (n + 6) % 12 + 1
    return f"{c}{'B' if major else 'A'}"


def keys_compatible(a, b):
    """Same key, relative major/minor, or one step on the wheel in the same mode."""
    if not a or not b:
        return None   # unknown
    na, nb = open_key_number(a), open_key_number(b)
    if a == b or na == nb:
        return True
    same_mode = (a <= 12) == (b <= 12)
    return same_mode and ((na - nb) % 12 in (1, 11))


# ---------------------------------------------------------------- data
def varint(buf, i):
    shift = 0
    v = 0
    while True:
        b = buf[i]
        i += 1
        v |= (b & 0x7F) << shift
        if not b & 0x80:
            return v, i
        shift += 7


def proto_fields(buf):
    """(field, wiretype, value-or-bytes) of one protobuf message."""
    i = 0
    out = []
    while i < len(buf):
        key, i = varint(buf, i)
        f, wt = key >> 3, key & 7
        if wt == 0:
            v, i = varint(buf, i)
            out.append((f, wt, v))
        elif wt == 2:
            n, i = varint(buf, i)
            out.append((f, wt, buf[i:i + n]))
            i += n
        elif wt == 1:
            i += 8
        elif wt == 5:
            i += 4
        else:
            break
    return out


def signal_values(buf):
    vals = []
    for f, wt, v in proto_fields(buf):
        if f == 1 and wt == 0:
            vals.append(v & 0xFF)
        elif f == 1 and wt == 2:
            j = 0
            while j < len(v):
                x, j = varint(v, j)
                vals.append(x & 0xFF)
    return vals


def read_overview(path):
    """GlideMix's waveform summary: all + low/mid/high, 2 interleaved entries per column."""
    data = open(path, "rb").read()
    raw = zlib.decompress(data[4:])   # qCompress: 4-byte big-endian length, then zlib
    all_, low, mid, high = [], [], [], []
    for f, wt, v in proto_fields(raw):
        if f == 3 and wt == 2:
            all_ = signal_values(v)
        elif f == 4 and wt == 2:
            for g, wt2, s in proto_fields(v):
                if wt2 != 2:
                    continue
                if g == 1:
                    low = signal_values(s)
                elif g == 2:
                    mid = signal_values(s)
                elif g == 3:
                    high = signal_values(s)

    def cols(x):
        return [max(x[2 * k], x[2 * k + 1]) for k in range(len(x) // 2)]

    return cols(all_), cols(low), cols(mid), cols(high)


class Song:
    """One analysed song, on GlideMix's constant grid (bar 1 + BPM)."""

    def __init__(self, row, analysis_dir):
        (self.id, self.location, self.duration, self.bpm, self.key, phrase_bpm, downbeat, phrases_json,
         analysis_id) = row
        self.bpm = phrase_bpm or self.bpm
        self.bar = 240.0 / self.bpm
        self.bar1 = max(0.0, downbeat or 0.0)
        self.phrases = json.loads(phrases_json or "[]")
        self.saved_anchors = None
        cols = read_overview(os.path.join(analysis_dir, str(analysis_id)))
        self.nbars = int((self.duration - self.bar1) / self.bar)
        self.level, self.low, self.mid, self.high = (self._per_bar(c) for c in cols)
        top = max(self.level) if self.level else 0.0
        self.loudest = top
        low_top = max(self.low) if self.low else 0.0
        # 0..1 per bar, against the song's own loudest bar
        self.lv = [x / top if top else 0.0 for x in self.level]
        self.bass = [x / low_top if low_top else 0.0 for x in self.low]

    def _per_bar(self, cols):
        if not cols:
            return [0.0] * self.nbars
        per_sec = len(cols) / self.duration
        out = []
        for b in range(self.nbars):
            t = self.bar1 + b * self.bar
            i0 = min(len(cols), int(max(0.0, t * per_sec)))
            i1 = min(len(cols), max(i0 + 1, int((t + self.bar) * per_sec)))
            out.append(sum(cols[i0:i1]) / (i1 - i0) if i1 > i0 else 0.0)
        return out

    def t(self, bar):
        return self.bar1 + bar * self.bar

    def label_at(self, bar):
        t = self.t(bar) + 0.01
        for p in self.phrases:
            if p["s"] <= t < p["e"]:
                return p["l"]
        return ""

    def kind(self, bar):
        """Phrase label without its number: Intro, Verse, Chorus, Bridge, Up, Down, Outro."""
        return self.label_at(bar).split(" ")[0]


VOCAL = {"Verse": 1.0, "Chorus": 1.0, "Bridge": 0.5, "Up": 0.5, "Down": 0.4, "Intro": 0.0, "Outro": 0.0, "": 0.3}


def load_songs(db, analysis_dir, ids):
    q = ("SELECT l.id, tl.location, l.duration, l.bpm, l.key_id, p.bpm, p.downbeat_sec, p.phrases, "
         "(SELECT a.id FROM track_analysis a WHERE a.track_id = l.id AND a.type = 2 ORDER BY a.id DESC LIMIT 1) "
         "FROM library l JOIN track_locations tl ON tl.id = l.location "
         "JOIN mixhouse_model_phrases p ON p.track_id = l.id WHERE l.id = ?")
    songs = {}
    for i in ids:
        row = db.execute(q, (i,)).fetchone()
        if not row or row[8] is None or not row[2] or not row[5]:
            continue
        try:
            s = Song(row, analysis_dir)
        except (OSError, zlib.error, IndexError):
            continue
        a = db.execute("SELECT intro_sec, outro_sec FROM mixhouse_anchors WHERE location = ?", (s.location,)).fetchone()
        if a:
            s.saved_anchors = a
        if s.nbars >= 48:
            songs[i] = s
    return songs


# ---------------------------------------------------------------- today's rule
def today_anchors(s):
    """MixHouseBeatGrids::sourceAnchors' defaults (GlideMix phrases), in bars from bar 1.

    Returns (in_bar, out_bar, why). Saved (moved) library anchors win, as in the app."""
    if s.saved_anchors:
        i, o = s.saved_anchors
        return round((i - s.bar1) / s.bar), round((o - s.bar1) / s.bar), "your saved anchors"
    loudest = 1.0

    def loud_enough(frm, bars):
        tot = sum(s.lv[b] if 0 <= b < s.nbars else 0.0 for b in range(frm, frm + bars))
        return tot / bars >= 0.15 * loudest

    label_outro = -1
    for p in s.phrases:
        if p["l"].startswith("Outro") and p["s"] > s.t(8):
            label_outro = round((p["s"] - s.bar1) / s.bar)
            break
    label_intro = 0
    for p in s.phrases:
        if not p["l"].startswith("Intro"):
            if p["s"] > s.bar1 + s.bar / 4:
                label_intro = round((p["s"] - s.bar1) / s.bar)
            break
    end_bars = (s.duration - s.bar1) / s.bar
    intro, outro = 0, 1
    for bars in (8, 4, 2, 1):
        out = label_outro
        if out >= 0:
            while out - bars > label_intro and not loud_enough(out, bars):
                out -= bars
        else:
            b1 = int(math.floor(end_bars / bars)) * bars
            while b1 - 2 * bars > 0 and not loud_enough(b1 - bars, bars):
                b1 -= bars
            out = b1 - bars
        inn = label_intro
        while inn + bars < out and not loud_enough(inn, bars):
            inn += bars
        for _ in range(16):
            if not (inn + bars < out) or not (0 <= inn < s.nbars) or s.lv[inn] >= 0.4:
                break
            inn += 1
        intro, outro = inn, out
        if inn + bars <= out:
            break
    if intro >= outro:
        intro = 0
    return intro, outro, "today's rule"


# ---------------------------------------------------------------- the new scorer
STRUCT_A = {"Outro": 1.0, "Down": 0.9, "Bridge": 0.6, "Up": 0.5, "Intro": 0.5, "Verse": 0.3, "Chorus": 0.0, "": 0.4}
STRUCT_B = {"Intro": 1.0, "Up": 0.7, "Down": 0.6, "Bridge": 0.6, "Verse": 0.3, "Chorus": 0.1, "Outro": 0.0, "": 0.4}
ARRIVE = {"Chorus": 1.0, "Up": 0.8, "Verse": 0.6, "Bridge": 0.4, "Down": 0.2, "Intro": 0.0, "Outro": 0.0, "": 0.3}
WEIGHTS = {"structure": 3.0, "arrival": 2.0, "vocal": 3.0, "bass": 1.5, "hole": 8.0, "energy": 1.0,
           "length": 1.5, "cutoff": 4.0, "arrived": 4.0, "cut": 3.0}


def mean(xs):
    return sum(xs) / len(xs) if xs else 0.0


def blend_metrics(a, out_bar, b, in_bar, length):
    """Bar by bar over the blend: A from its OUT, B from its IN."""
    va = [VOCAL.get(a.kind(out_bar + k), 0.3) for k in range(length)]
    vb = [VOCAL.get(b.kind(in_bar + k), 0.3) for k in range(length)]
    vocal = mean([x * y for x, y in zip(va, vb)])
    bass = mean([min(a.bass[out_bar + k], b.bass[in_bar + k]) for k in range(length)])
    # Volume Fade gains: A 0 -> -6 dB, B -6 -> 0 dB, as linear amplitudes.
    floor = []
    for k in range(length):
        ga = 10 ** (-6.0 * k / length / 20)
        gb = 10 ** (-6.0 * (1 - k / length) / 20)
        floor.append(max(a.lv[out_bar + k] * ga, b.lv[in_bar + k] * gb))
    hole_bars = sum(1 for x in floor if x < 0.35)
    vocal_bars = sum(1 for x, y in zip(va, vb) if x >= 0.9 and y >= 0.9)
    bass_bars = sum(1 for k in range(length) if a.bass[out_bar + k] > 0.6 and b.bass[in_bar + k] > 0.6)
    return {"vocal": vocal, "bass": bass, "hole_bars": hole_bars, "vocal_bars": vocal_bars, "bass_bars": bass_bars,
            "floor": min(floor) if floor else 0.0}


def score(a, out_bar, b, in_bar, length, target):
    m = blend_metrics(a, out_bar, b, in_bar, length)
    struct = mean([STRUCT_A.get(a.kind(out_bar + k), 0.4) for k in range(length)]) * 0.5 \
        + mean([STRUCT_B.get(b.kind(in_bar + k), 0.4) for k in range(length)]) * 0.5
    arr_bar = in_bar + length
    after = b.lv[arr_bar:arr_bar + 8]
    during = b.lv[in_bar:arr_bar]
    lift = max(0.0, min(1.0, (mean(after) - mean(during)) / 0.3)) if after else 0.0
    bass_in = max(0.0, min(1.0, (mean(b.bass[arr_bar:arr_bar + 8]) - mean(b.bass[in_bar:arr_bar])) / 0.3))
    arrival = 0.4 * ARRIVE.get(b.kind(arr_bar), 0.3) + 0.4 * lift + 0.2 * bass_in
    energy = abs(mean(a.lv[max(0, out_bar - 8):out_bar]) - mean(after)) if after else 0.5
    length_fit = 1.0 - abs(math.log2(length / target)) / 2.0
    skipped = (a.nbars - (out_bar + length)) / a.nbars + in_bar / b.nbars
    cutoff = max(0.0, skipped - 0.2)   # keep most of both songs (a DJ mix playing 2 minutes is a style)
    # Run 2 (2026-11-16): the blend ends where B has ARRIVED -- A is cut, so B must be near full
    # level then, not still in a quiet intro (run 1 lost there: -9..-20 dB at the cut).
    arrived = mean(b.lv[arr_bar:arr_bar + 4])
    before = 0.5 * a.lv[out_bar + length - 1] + b.lv[arr_bar - 1]
    cut_db = 20 * math.log10(max(mean(b.lv[arr_bar:arr_bar + 2]), 1e-3) / max(before, 1e-3))
    not_arrived = max(0.0, 0.6 - arrived) / 0.6
    cut_drop = min(1.0, max(0.0, -cut_db - 3.0) / 6.0)
    total = (WEIGHTS["structure"] * struct + WEIGHTS["arrival"] * arrival - WEIGHTS["vocal"] * m["vocal"]
             - WEIGHTS["bass"] * m["bass"] - WEIGHTS["hole"] * m["hole_bars"] / length - WEIGHTS["energy"] * energy
             + WEIGHTS["length"] * length_fit - WEIGHTS["cutoff"] * cutoff
             - WEIGHTS["arrived"] * not_arrived - WEIGHTS["cut"] * cut_drop)
    return total, m, {"structure": struct, "arrival": arrival, "energy": energy, "arrived": arrived,
                      "cut_db": cut_db}


def pair_aware(a, b, target=32, match_keys=False):
    """Best (out_bar, in_bar, length) for A -> B, with a short reason."""
    compat = keys_compatible(a.key, b.key)
    lengths = [32, 16, 8]
    key_note = ""
    if compat is False and not match_keys:
        lengths = [8]   # clashing keys: short blend only
        key_note = "keys clash: 8 bars"
    elif compat is False:
        key_note = "keys matched"
    best = None
    # First only blends where B has arrived when A is cut (near full level); if a pair has none, the best of the rest.
    for need_arrival in (0.45, 0.0):
        for length in lengths:
            if length > target:
                continue
            # OUT: phrase lines (8 bars from bar 1) in A's last 35 %, with A still playing for the blend.
            for out_bar in range(8 * math.ceil(a.nbars * 0.65 / 8), a.nbars - length + 1, 8):
                if mean(a.lv[out_bar:out_bar + length]) < 0.25:
                    continue
                # IN: phrase lines in B's first 30 %, with a phrase to arrive into after the blend.
                for in_bar in range(0, min(int(b.nbars * 0.30), b.nbars - length - 8) + 1, 8):
                    total, m, parts = score(a, out_bar, b, in_bar, length, target)
                    if parts["arrived"] < need_arrival:
                        continue
                    if best is None or total > best[0]:
                        best = (total, out_bar, in_bar, length, m, parts)
        if best is not None:
            break
    if best is None:
        return None
    total, out_bar, in_bar, length, m, parts = best
    why = [f"{a.kind(out_bar) or '?'} under {b.kind(in_bar) or '?'}", f"{length} bars",
           f"B arrives on {b.kind(in_bar + length) or '?'}"]
    if m["vocal_bars"] == 0:
        why.append("no vocals together")
    else:
        why.append(f"{m['vocal_bars']} bars of vocals together")
    if key_note:
        why.append(key_note)
    return {"out_bar": out_bar, "in_bar": in_bar, "length": length, "score": round(total, 3), "why": ", ".join(why),
            "checks": m, "parts": {k: round(v, 3) for k, v in parts.items()}}


# ---------------------------------------------------------------- audio
def render(a, out_bar, b, in_bar, length, path, window_bars, pre_bars=8, part="mix"):
    """The transition as GlideMix plays it (B tempo-matched to A, Volume Fade of `length` bars).

    Every render of a pair spans the same `window_bars` after A's OUT, so the length
    gives nothing away: a shorter blend just plays more of B afterwards."""
    post_bars = window_bars - length
    out_t = a.t(out_bar)
    a_start = max(0.0, out_t - pre_bars * a.bar)
    pre = out_t - a_start
    blend = length * a.bar
    total = pre + blend + post_bars * a.bar
    ratio = a.bpm / b.bpm   # atempo: >1 = faster
    in_t = b.t(in_bar)
    b_len = (blend + post_bars * a.bar) * ratio
    # A: full until OUT, then 0 -> -6 dB over the blend, then cut.
    ga = f"if(lt(t,{pre:.4f}),1,if(lt(t,{pre + blend:.4f}),pow(10,-6*(t-{pre:.4f})/{blend:.4f}/20),0))"
    # B (after the delay its t counts from the render's start): -6 -> 0 dB over the blend.
    gb = f"if(lt(t,{pre + blend:.4f}),pow(10,-6*(1-(t-{pre:.4f})/{blend:.4f})/20),1)"
    filt = (f"[0:a]aresample=44100,aformat=channel_layouts=stereo,volume='{ga}':eval=frame[a];"
            f"[1:a]aresample=44100,aformat=channel_layouts=stereo,atempo={ratio:.6f},"
            f"adelay={int(pre * 1000)}|{int(pre * 1000)},volume='{gb}':eval=frame[b];"
            f"[a][b]amix=inputs=2:normalize=0:duration=longest,volume=-3dB,alimiter=limit=0.89:level=false,"
            f"atrim=0:{total:.3f},afade=t=in:d=1,afade=t=out:st={total - 2:.3f}:d=2[o]")
    if part != "mix":
        # One song's part alone, for measuring, no limiter. B gets its lead-in as
        # real silence (concat, sample exact): on its own, adelay's delay was lost
        # in the file (B started at 0); in the mix amix lines it up by timestamp.
        if part == "a":
            filt = (f"[0:a]aresample=44100,aformat=channel_layouts=stereo,volume='{ga}':eval=frame,"
                    f"apad,atrim=0:{total:.3f}[o]")
        else:
            filt = (f"anullsrc=r=44100:cl=stereo,atrim=0:{pre:.4f}[s];"
                    f"[1:a]aresample=44100,aformat=channel_layouts=stereo,atempo={ratio:.6f}[bx];"
                    f"[s][bx]concat=n=2:v=0:a=1,volume='{gb}':eval=frame,apad,atrim=0:{total:.3f}[o]")
    cmd = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
           "-ss", f"{a_start:.4f}", "-t", f"{total + 1:.4f}", "-i", a.location,
           "-ss", f"{in_t:.4f}", "-t", f"{b_len + 1:.4f}", "-i", b.location,
           "-filter_complex", filt, "-map", "[o]", "-c:a", "pcm_s16le", path]
    subprocess.run(cmd, check=True)
    return {"pre": pre, "bar": a.bar, "blend": blend, "length": length, "total": total}


# ---------------------------------------------------------------- measuring (rate)
BANDS = {"full": "highpass=f=30", "low": "lowpass=f=150,lowpass=f=150", "mid": "highpass=f=300,highpass=f=300,lowpass=f=3000",
         "kick": "highpass=f=40,lowpass=f=120"}
ENV_RATE = 1000


def envelope(path, band):
    """Rectified band envelope at 1 kHz (ffmpeg does the filtering and smoothing)."""
    import array
    cmd = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", path, "-af",
           f"{BANDS[band]},aformat=sample_fmts=flt:channel_layouts=mono,aeval='abs(val(0))',aresample={ENV_RATE}",
           "-f", "f32le", "-ac", "1", "pipe:1"]
    data = subprocess.run(cmd, check=True, capture_output=True).stdout
    env = array.array("f")
    env.frombytes(data[:len(data) // 4 * 4])
    return env


def per_bar(env, t0, bar, bars):
    out = []
    for k in range(bars):
        i0, i1 = int((t0 + k * bar) * ENV_RATE), int((t0 + (k + 1) * bar) * ENV_RATE)
        seg = env[i0:i1]
        out.append(sum(seg) / len(seg) if len(seg) else 0.0)
    return out


def median(xs):
    s = sorted(xs)
    return s[len(s) // 2] if s else 0.0


def onset(env):
    """Rises of the kick envelope (10 ms), the moments the beats hit."""
    return [max(0.0, env[i] - env[i - 10]) if i >= 10 else 0.0 for i in range(len(env))]


def beat_lag(env_a, env_b, t0, t1):
    """Lag (ms, + = B late) where A's and B's kick onsets line up best inside [t0, t1], and how well."""
    oa, ob = onset(env_a), onset(env_b)
    i0, i1 = int(t0 * ENV_RATE), min(len(oa), len(ob), int(t1 * ENV_RATE))
    if i1 - i0 < 2000:
        return 0, 0.0
    na = math.sqrt(sum(x * x for x in oa[i0:i1])) or 1.0
    nb = math.sqrt(sum(x * x for x in ob[i0:i1])) or 1.0
    best = (0.0, 0)
    for lag in range(-60, 61, 2):
        s = 0.0
        for i in range(i0 + 60, i1 - 60, 2):
            s += oa[i] * ob[i + lag]
        if s > best[0]:
            best = (s, lag)
    corr = 2 * best[0] / (na * nb)
    return best[1], corr


def measure(a, b, v, window, tmp):
    """Rule checks on the rendered audio of one version (A part, B part, measured separately)."""
    pa, pb = os.path.join(tmp, "a.wav"), os.path.join(tmp, "b.wav")
    info = render(a, v["out_bar"], b, v["in_bar"], v["length"], pa, window, part="a")
    render(a, v["out_bar"], b, v["in_bar"], v["length"], pb, window, part="b")
    pre, bar, length = info["pre"], info["bar"], v["length"]
    pre_bars = int(round(pre / bar))
    post = window - length
    env = {(p, band): envelope(path, band) for p, path in (("a", pa), ("b", pb)) for band in ("full", "low", "mid", "kick")}
    bars = {}
    for (p, band), e in env.items():
        bars[(p, band)] = per_bar(e, 0.0, bar, pre_bars + window)
    blend = range(pre_bars, pre_bars + length)
    # References: A at full before its OUT, B at full after the blend.
    ref = {band: (median(bars[("a", band)][:pre_bars]) or 1e-9, median(bars[("b", band)][pre_bars + length:]) or 1e-9)
           for band in ("full", "low", "mid")}
    bass_both = sum(1 for k in blend if bars[("a", "low")][k] > 0.35 * ref["low"][0] and bars[("b", "low")][k] > 0.35 * ref["low"][1])
    mid_both = sum(1 for k in blend if bars[("a", "mid")][k] > 0.5 * ref["mid"][0] and bars[("b", "mid")][k] > 0.5 * ref["mid"][1])
    mix = [bars[("a", "full")][k] + bars[("b", "full")][k] for k in range(pre_bars + window)]
    mix_ref = median(mix[:pre_bars]) or 1e-9
    holes = sum(1 for k in range(pre_bars, pre_bars + window) if mix[k] < 0.5 * mix_ref)
    cut = pre_bars + length
    before, after = mix[cut - 1], mix[cut] if cut < len(mix) else mix[-1]
    cut_jump = 20 * math.log10(max(after, 1e-9) / max(before, 1e-9))
    lag, corr = beat_lag(env[("a", "kick")], env[("b", "kick")], pre, pre + length * bar)
    return {"lag_ms": lag, "beat_corr": round(corr, 2), "bass_both": bass_both, "mid_both": mid_both, "holes": holes,
            "cut_jump_db": round(cut_jump, 1), "length": length, "phrase_ok": v["out_bar"] % 8 == 0 and v["in_bar"] % 8 == 0,
            "post_bars": post}


def rating(m):
    """1-5 from DJ practice (docs/research/auto-anchors.md section 2), on what the audio can show.

    Not scored, only noted: the beat offset (it measures the grid this renderer uses, not the
    anchors -- the app lines songs up on its own grid and warp) and "both full in the mids"
    (a mix can't tell vocals from other mids without stems; in a Volume Fade both songs are
    full during the blend, so it would only punish longer blends)."""
    r = 5.0
    notes = []
    if m["bass_both"]:
        r -= min(1.5, 0.5 * math.ceil(m["bass_both"] / 4))
        notes.append(f"{m['bass_both']} bars two basslines")
    if m["holes"]:
        r -= min(2.0, 1.0 * m["holes"])
        notes.append(f"{m['holes']} quiet bars")
    if abs(m["cut_jump_db"]) > 4:
        r -= 1 if abs(m["cut_jump_db"]) > 8 else 0.5
        notes.append(f"level jump {m['cut_jump_db']} dB at the cut")
    if not m["phrase_ok"]:
        r -= 0.5
        notes.append("not on a phrase line")
    if m["length"] >= 16 and not m["bass_both"] and not m["holes"]:
        r += 0.5
        notes.append(f"clean {m['length']}-bar blend")
    info = []
    if m["beat_corr"] >= 0.15 and abs(m["lag_ms"]) > 20:
        info.append(f"(render: beats {m['lag_ms']:+d} ms apart -- listen for a flam)")
    return max(1.0, min(5.0, r)), notes + info



# ---------------------------------------------------------------- pairs
def choose_pairs(db, analysis_dir, count, seed, folder):
    rows = db.execute(
        "SELECT l.id, l.bpm FROM library l JOIN track_locations tl ON tl.id = l.location "
        "JOIN mixhouse_model_phrases p ON p.track_id = l.id "
        "WHERE l.mixxx_deleted = 0 AND l.bpm > 0 AND l.key_id > 0 AND tl.location LIKE ?", (folder + "%",)).fetchall()
    rng = random.Random(seed)
    rng.shuffle(rows)
    songs = load_songs(db, analysis_dir, [r[0] for r in rows[:count * 12]])
    ids = list(songs)
    pairs = []
    used = set()
    for i in ids:
        if i in used:
            continue
        for j in ids:
            if j == i or j in used:
                continue
            # DJs keep tempo changes small (95 % under 10 %, Kim et al.) and pick songs that go
            # together in key: test the normal case, the one long blends are for.
            if abs(math.log(songs[i].bpm / songs[j].bpm)) < math.log(1.06) and keys_compatible(songs[i].key, songs[j].key):
                pairs.append((i, j))
                used.update((i, j))
                break
        if len(pairs) >= count:
            break
    return songs, pairs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--library", required=True, help="a COPY of library.sqlite")
    ap.add_argument("--analysis", default=os.path.expanduser("~/Library/Application Support/Glidemix/Library/analysis"))
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("pairs")
    p.add_argument("--count", type=int, default=20)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--folder", default=os.path.expanduser("~/Desktop/"))
    p.add_argument("--ids", help="a chain of library ids, comma separated (instead of random pairs)")
    p.add_argument("--target", type=int, default=32, choices=(8, 16, 32))
    p.add_argument("--match-keys", action="store_true")
    p.add_argument("--out", required=True)
    p.add_argument("--no-audio", action="store_true")
    r = sub.add_parser("rate", help="measure the rules on the rendered audio of a pairs run and rate 1-5")
    r.add_argument("--dir", required=True, help="the folder a pairs run wrote (report.json, .answers.json)")
    args = ap.parse_args()

    db = sqlite3.connect(f"file:{args.library}?mode=ro", uri=True)
    if args.cmd == "rate":
        return rate(db, args)
    if args.ids:
        ids = [int(x) for x in args.ids.split(",")]
        songs = load_songs(db, args.analysis, ids)
        pairs = [(x, y) for x, y in zip(ids, ids[1:]) if x in songs and y in songs]
    else:
        songs, pairs = choose_pairs(db, args.analysis, args.count, args.seed, args.folder)
    os.makedirs(args.out, exist_ok=True)
    rng = random.Random(args.seed * 7919)
    report, answers = [], {}
    for n, (ia, ib) in enumerate(pairs, 1):
        a, b = songs[ia], songs[ib]
        _, a_out, _ = today_anchors(a)
        b_in, _, why_old = today_anchors(b)
        old_len = 8
        a_out = min(a_out, a.nbars - old_len)
        b_in = min(b_in, b.nbars - old_len)
        old = {"out_bar": a_out, "in_bar": b_in, "length": old_len, "why": why_old,
               "checks": blend_metrics(a, a_out, b, b_in, old_len)}
        new = pair_aware(a, b, args.target, args.match_keys)
        if not new:
            continue
        row = {"pair": n, "a": ia, "b": ib, "bpm": [a.bpm, b.bpm], "keys": [camelot(a.key), camelot(b.key)],
               "keys_ok": keys_compatible(a.key, b.key), "today": old, "new": new}
        report.append(row)
        if not args.no_audio:
            x_is_new = rng.random() < 0.5
            versions = {"X": new if x_is_new else old, "Y": old if x_is_new else new}
            window = max(old["length"], new["length"]) + 8
            for tag, v in versions.items():
                render(a, v["out_bar"], b, v["in_bar"], v["length"], os.path.join(args.out, f"pair{n:02d}-{tag}.wav"),
                       window)
            answers[f"pair{n:02d}"] = {"new": "X" if x_is_new else "Y"}
        print(f"pair {n:2d}: today OUT bar {old['out_bar']} / IN bar {old['in_bar']} / 8 bars "
              f"(vocal {old['checks']['vocal_bars']}, bass {old['checks']['bass_bars']}, hole {old['checks']['hole_bars']})"
              f" | new OUT {new['out_bar']} / IN {new['in_bar']} / {new['length']} bars "
              f"(vocal {new['checks']['vocal_bars']}, bass {new['checks']['bass_bars']}, hole {new['checks']['hole_bars']})"
              f" -- {new['why']}")
    with open(os.path.join(args.out, "report.json"), "w") as f:
        json.dump(report, f, indent=1)
    if answers:
        # Kept apart so the listening stays blind: open it only after rating.
        with open(os.path.join(args.out, ".answers.json"), "w") as f:
            json.dump(answers, f, indent=1)
        with open(os.path.join(args.out, "ratings.csv"), "w") as f:
            f.write("pair,X (1-5),Y (1-5),notes\n")
            for k in sorted(answers):
                f.write(f"{k},,,\n")
    summary(report)


def rate(db, args):
    import tempfile
    report = json.load(open(os.path.join(args.dir, "report.json")))
    answers = json.load(open(os.path.join(args.dir, ".answers.json")))
    songs = load_songs(db, args.analysis, sorted({r["a"] for r in report} | {r["b"] for r in report}))
    rows, sums, wins = [], {"today": 0.0, "new": 0.0}, {"new": 0, "today": 0, "tie": 0}
    with tempfile.TemporaryDirectory() as tmp:
        for row in report:
            a, b = songs[row["a"]], songs[row["b"]]
            window = max(row["today"]["length"], row["new"]["length"]) + 8
            res = {}
            for which in ("today", "new"):
                m = measure(a, b, row[which], window, tmp)
                score_, notes = rating(m)
                res[which] = {"rating": score_, "notes": notes, "measured": m}
                sums[which] += score_
            d = res["new"]["rating"] - res["today"]["rating"]
            wins["new" if d > 0 else "today" if d < 0 else "tie"] += 1
            key = f"pair{row['pair']:02d}"
            new_tag = answers[key]["new"]
            rows.append({"pair": key, "new_is": new_tag, **res})
            print(f"{key}: today {res['today']['rating']:.1f} ({'; '.join(res['today']['notes']) or 'clean'})"
                  f" | new {res['new']['rating']:.1f} ({'; '.join(res['new']['notes']) or 'clean'})")
    with open(os.path.join(args.dir, "machine-ratings.json"), "w") as f:
        json.dump(rows, f, indent=1)
    with open(os.path.join(args.dir, "machine-ratings.csv"), "w") as f:
        f.write("pair,X,Y,new is,today notes,new notes\n")
        for r in rows:
            x = r["new"] if r["new_is"] == "X" else r["today"]
            y = r["today"] if r["new_is"] == "X" else r["new"]
            f.write(f"{r['pair']},{x['rating']:.1f},{y['rating']:.1f},{r['new_is']},"
                    f"\"{'; '.join(r['today']['notes'])}\",\"{'; '.join(r['new']['notes'])}\"\n")
    n = len(rows)
    print(f"\nAverage rating: today {sums['today'] / n:.2f}, new {sums['new'] / n:.2f}. "
          f"New better in {wins['new']}, today better in {wins['today']}, equal in {wins['tie']} of {n}.")


def summary(report):
    if not report:
        print("no pairs")
        return

    def tot(which, key):
        return sum(r[which]["checks"][key] for r in report)

    print(f"\n{len(report)} transitions. Bars of vocals together: today {tot('today', 'vocal_bars')}, "
          f"new {tot('new', 'vocal_bars')}; both basslines: today {tot('today', 'bass_bars')}, new {tot('new', 'bass_bars')}; "
          f"quiet bars (holes): today {tot('today', 'hole_bars')}, new {tot('new', 'hole_bars')}.")
    lengths = {}
    for r in report:
        lengths[r["new"]["length"]] = lengths.get(r["new"]["length"], 0) + 1
    print("New blend lengths: " + ", ".join(f"{k} bars x{v}" for k, v in sorted(lengths.items(), reverse=True)))


if __name__ == "__main__":
    sys.exit(main())
