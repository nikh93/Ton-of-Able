"""Builds a compact SM Drums kit for Ton of Able.

usage: convert_smdrums.py <SMDrums git checkout> <output dir> <AVL kit dir>

For every drum machine lane it reads the SM Drums SFZ mapping, keeps evenly
spaced velocity layers and the first round robins, trims the decay below
-70 dBFS, writes 16-bit FLAC and a plain SFZ (key, lovel/hivel, lorand/hirand,
group/off_by) that the app's SampleKit reads.
"""
import os
import re
import subprocess
import sys

import numpy as np

src, out, avl = sys.argv[1], sys.argv[2], sys.argv[3]

# lane note -> (mapping, layers, round robins, choke group, off_by)
LANES = [
    (36, "kick", 16, 4, 0, 0, "Kick"),
    (38, "snare65reg", 16, 4, 0, 0, "Snare"),
    (37, "sidestick", 8, 3, 0, 0, "Sidestick"),
    (42, "closedhat", 12, 4, 1, 0, "HatClosed"),
    (44, "foothat", 8, 3, 1, 0, "HatFoot"),
    (46, "openhat", 8, 4, 2, 1, "HatOpen"),
    (45, "tom1", 12, 3, 0, 0, "TomFloor"),
    (47, "tom3", 12, 3, 0, 0, "TomMid"),
    (50, "tom4", 12, 3, 0, 0, "TomHigh"),
    (49, "crash16", 8, 3, 0, 0, "Crash16"),
    (51, "ride20tip", 8, 3, 0, 0, "Ride20"),
]


def parse_mapping(name):
    text = subprocess.run(["git", "-C", src, "show", f"HEAD:Programs/mappings/smdrums_sfz_{name}.sfz"],
                          capture_output=True, text=True, check=True).stdout
    groups, current, region = [], None, None
    for raw in text.splitlines():
        line = raw.split("//")[0].strip()
        if not line:
            continue
        if line.startswith("<group>"):
            current = {"lorand": 0.0, "regions": []}
            groups.append(current)
            region = None
            continue
        if line.startswith("<region>"):
            region = {"lovel": 1, "hivel": 127}
            current["regions"].append(region)
            continue
        key, _, value = line.partition("=")
        target = region if region is not None else current
        if key in ("lovel", "hivel"):
            target[key] = int(value)
        elif key == "lorand" and region is None:
            current["lorand"] = float(value)
        elif key == "sample":
            target["sample"] = value.replace("\\", "/").replace("../", "", 1)
    groups.sort(key=lambda g: g["lorand"])
    return [g for g in groups if g["regions"]]


def pick_layers(regions, count):
    regions = sorted(regions, key=lambda r: r["lovel"])
    if len(regions) <= count:
        return regions
    picks = sorted({round(i * (len(regions) - 1) / (count - 1)) for i in range(count)})
    return [regions[i] for i in picks]


def decode(path):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-ac", "2", "-"],
                         capture_output=True, check=True).stdout
    rate = int(subprocess.run(["ffprobe", "-v", "error", "-select_streams", "a:0", "-show_entries",
                               "stream=sample_rate", "-of", "csv=p=0", path],
                              capture_output=True, text=True, check=True).stdout.strip())
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, 2), rate


def trim(audio, rate):
    level = np.max(np.abs(audio), axis=1)
    above = np.nonzero(level > 10 ** (-70 / 20))[0]
    end = int(above[-1]) if len(above) else 0
    end = min(len(audio), end + int(0.02 * rate))
    audio = audio[:end].copy()
    fade = min(len(audio), int(0.02 * rate))
    if fade > 0:
        audio[-fade:] *= np.linspace(1.0, 0.0, fade, dtype=np.float32)[:, None]
    return audio


def write_flac(audio, rate, path):
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "f32le", "-ar", str(rate), "-ac", "2", "-i", "-",
                    "-c:a", "flac", "-sample_fmt", "s16", "-compression_level", "8", path],
                   input=audio.astype(np.float32).tobytes(), check=True)


os.makedirs(os.path.join(out, "Samples"), exist_ok=True)
plan = []
for note, mapping, layers, robins, group, off_by, label in LANES:
    groups = parse_mapping(mapping)[:robins]
    chosen = [pick_layers(g["regions"], layers) for g in groups]
    plan.append((note, label, group, off_by, chosen))

needed = sorted({r["sample"] for _, _, _, _, chosen in plan for rr in chosen for r in rr})
with open(os.path.join(out, "..", "needed.txt"), "w") as f:
    f.write("\n".join(needed) + "\n")
subprocess.run(["git", "-C", src, "checkout", "HEAD", "--pathspec-from-file=" + os.path.abspath(os.path.join(out, "..", "needed.txt"))],
               check=True)

lines = ["// SM Drums (Scott McLean, Tod Stillwell, Suleiman Ali), condensed for Ton of Able:",
         "// evenly spaced velocity layers and round robins, decays trimmed below -70 dBFS.", "",
         "// Level matched to the other bundled kits.", "<global> volume=4", ""]
total = 0
for note, label, group, off_by, chosen in plan:
    robins = len(chosen)
    header = f"<group> key={note}"
    if group:
        header += f" group={group}"
    if off_by:
        header += f" off_by={off_by}"
    lines += [f"// {label}", header]
    for index, layers in enumerate(chosen):
        lo_rand, hi_rand = index / robins, (index + 1) / robins
        # Velocity ranges: split halfway between the original layers' centres.
        centres = [(r["lovel"] + r["hivel"]) / 2 for r in layers]
        bounds = [1] + [int((centres[i] + centres[i + 1]) / 2) + 1 for i in range(len(centres) - 1)] + [128]
        for layer_index, region in enumerate(layers):
            audio, rate = decode(os.path.join(src, region["sample"]))
            audio = trim(audio, rate)
            name = f"{label}_v{layer_index + 1:02d}_rr{index + 1}.flac"
            write_flac(audio, rate, os.path.join(out, "Samples", name))
            total += os.path.getsize(os.path.join(out, "Samples", name))
            lines.append(f"<region> sample=Samples/{name} lovel={bounds[layer_index]} hivel={bounds[layer_index + 1] - 1}"
                         f" lorand={lo_rand:.4f} hirand={hi_rand:.4f}")
    lines.append("")

# SM Drums has no hand clap: borrow the one from the AVL kit.
lines += ["// HandClap (AVL Drumkits)", "<group> key=39"]
for layer, (lo, hi) in enumerate([(1, 26), (27, 52), (53, 77), (78, 102), (103, 127)]):
    lines.append(f"<region> sample=../AVL-Drumkits/Samples/39-HandClap-{layer + 1}.flac lovel={lo} hivel={hi}")
lines.append("")

with open(os.path.join(out, "SM_Drums.sfz"), "w") as f:
    f.write("\n".join(lines))
print(f"{sum(len(rr) for _, _, _, _, c in plan for rr in c)} samples, {total / 1e6:.1f} MB")
