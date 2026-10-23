"""Renders a short rock groove from a simple SFZ kit (as the app plays it).

usage: demo_beat.py <kit.sfz> <out.wav> [note_map like 50:45,47:47,45:41]
"""
import os
import random
import re
import subprocess
import sys

import numpy as np

sfz, out = sys.argv[1], sys.argv[2]
remap = dict(tuple(map(int, p.split(":"))) for p in sys.argv[3].split(",")) if len(sys.argv) > 3 and sys.argv[3] else {}
RATE = 48000
base = os.path.dirname(sfz)

regions, group = [], {}
for raw in open(sfz, encoding="utf-8", errors="replace").read().splitlines():
    line = raw.split("//")[0].strip()
    if not line:
        continue
    for header, body in re.findall(r"<(\w+)>([^<]*)", line) or [("", line)]:
        ops = dict(re.findall(r"(\w+)=(\S+)", body))
        if "sample" in ops:  # sample paths in these kits contain no spaces
            pass
        if header == "group":
            group = ops
        elif header == "region":
            r = dict(group)
            r.update(ops)
            regions.append(r)

cache = {}


def load(path):
    if path not in cache:
        raw = subprocess.run(["ffmpeg", "-v", "error", "-i", os.path.join(base, path), "-f", "f32le", "-ac", "2", "-ar", str(RATE), "-"],
                             capture_output=True, check=True).stdout
        cache[path] = np.frombuffer(raw, dtype=np.float32).reshape(-1, 2)
    return cache[path]


def pick(note, vel):
    note = remap.get(note, note)
    pr = random.random()
    match = [r for r in regions if int(r.get("key", -1)) == note and int(r.get("lovel", 1)) <= vel <= int(r.get("hivel", 127))]
    for r in match:
        if float(r.get("lorand", 0)) <= pr < float(r.get("hirand", 1)):
            return r
    return match[0] if match else None


bpm, bars = 100, 4
step = 60 / bpm / 4
length = int((bars * 16 * step + 4) * RATE)
mix = np.zeros((length, 2), dtype=np.float32)
hits = []
for bar in range(bars):
    t0 = bar * 16
    fill = bar == bars - 1
    for s in range(16):
        if not (fill and s >= 8):
            hits.append((t0 + s, 42 if s % 2 == 0 else 42, 100 if s % 4 == 0 else (75 if s % 2 == 0 else 55)))
    for s, v in [(0, 118), (6, 90), (8, 112), (10, 96)] if not fill else [(0, 118), (6, 90)]:
        hits.append((t0 + s, 36, v))
    hits += [(t0 + 4, 38, 115), (t0 + 12, 38, 118)] if not fill else [(t0 + 4, 38, 115)]
    if bar == 0:
        hits.append((t0, 49, 120))
    if fill:
        for s, n, v in [(8, 50, 105), (9, 50, 95), (10, 47, 108), (11, 47, 98), (12, 45, 112), (13, 45, 100), (14, 38, 120), (15, 38, 124)]:
            hits.append((t0 + s, n, v))
hits.append((bars * 16, 36, 120))
hits.append((bars * 16, 49, 125))

active_open = []
for when, note, vel in sorted(hits):
    r = pick(note, vel)
    if r is None:
        continue
    data = load(r["sample"])
    start = int(when * step * RATE)
    end = min(length, start + len(data))
    mix[start:end] += data[: end - start] * 0.3

peak = np.max(np.abs(mix))
print(f"peak {peak:.3f}")
subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "f32le", "-ar", str(RATE), "-ac", "2", "-i", "-", "-c:a", "pcm_s16le", out],
               input=(mix * (0.9 / max(peak, 1e-6))).astype(np.float32).tobytes(), check=True)
