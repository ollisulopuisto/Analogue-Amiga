#!/usr/bin/env python3
"""Generate openFPGA instance JSONs for the Amiga core.

usage: build_instances.py <common_dir> <out_dir>
  common_dir  = .../Assets/amiga/common   (holds Classics/<Genre>/*.adf, Improved/*.adf, Save Disks/*.adf, Workbench/*.adf)
  out_dir     = .../Assets/amiga/Mazamars312.Amiga   (instance JSONs are written here, one folder per genre)
Slot 320 = DF0, 321 = DF1 (neither is core-specific, so filenames are relative to common_dir).
"""
import json, os, re, sys, unicodedata

common, out = sys.argv[1:3]
DISK = re.compile(r"^(?P<name>.*?)(?: \(Disk (?P<n>\d+) of (?P<m>\d+)\))?(?P<rest>(?: \([^)]*\))*)\.adf$")

def norm(s):
    s = unicodedata.normalize("NFKD", s).lower()
    s = re.sub(r"\(.*?\)|\[.*?\]", " ", s)
    return re.sub(r"[^a-z0-9]+", "", s)

def listdir(p):
    return sorted(f for f in os.listdir(p) if not f.startswith(".")) if os.path.isdir(p) else []

# save disks keyed by normalised game name ("Civilization Save.adf" -> civilization)
saves = {}
for f in listdir(os.path.join(common, "Save Disks")):
    base = re.sub(r" Save\.adf$", "", f)
    if f.endswith(" Save.adf") and not base.startswith("Blank"):
        saves[norm(base)] = f"Save Disks/{f}"

def find_save(game):
    g = norm(game)
    if g in saves: return saves[g]
    for k, v in saves.items():
        if k and (g.startswith(k) or k.startswith(g)) and min(len(k), len(g)) >= 6: return v
    return None

def instance(slots):
    return {"instance": {"magic": "APF_VER_1", "data_path": "",
                         "data_slots": [{"id": i, "filename": fn} for i, fn in slots]}}

def write(rel, slots):
    p = os.path.join(out, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    json.dump(instance(slots), open(p, "w"), indent=1)
    open(p, "a").write("\n")

n = 0
def games(folder, prefix):
    """yield (title, [relative disk paths in order]) for one folder of ADFs"""
    groups = {}
    for f in listdir(folder):
        if not f.lower().endswith(".adf"): continue
        m = DISK.match(f)
        if not m: continue
        key = m.group("name") + (m.group("rest") or "")
        groups.setdefault(key, []).append((int(m.group("n") or 1), f))
    for key, disks in sorted(groups.items()):
        yield key, [f"{prefix}/{f}" for _, f in sorted(disks)]

stats = {"games": 0, "with_save": 0, "two_disk": 0}
for kind, sub in (("Classics", None), ("Improved", "")):
    base = os.path.join(common, kind)
    folders = [(g, os.path.join(base, g), f"{kind}/{g}") for g in listdir(base) if os.path.isdir(os.path.join(base, g))] if kind == "Classics" else [("Improved", base, "Improved")]
    for genre, folder, prefix in folders:
        for title, disks in games(folder, prefix):
            slots = [(320, disks[0])]
            save = find_save(title)
            if save: slots.append((321, save)); stats["with_save"] += 1
            elif len(disks) > 1: slots.append((321, disks[1])); stats["two_disk"] += 1
            safe = re.sub(r'[\\/:*?"<>|]', "-", title)[:90]
            write(f"{genre}/{safe}.json", slots)
            stats["games"] += 1

# fixed entries
write("00 Boot without disk.json", [])
wb = "Workbench/Workbench 3.1 (Disk 2 of 6) Workbench.adf"
if os.path.exists(os.path.join(common, wb)): write("01 Workbench 3.1.json", [(320, wb)])
if os.path.exists(os.path.join(common, "Improved-Editions.hdf")):
    write("02 Improved Editions (hard disk).json", [(310, "Improved-Editions.hdf")])
print(stats)
