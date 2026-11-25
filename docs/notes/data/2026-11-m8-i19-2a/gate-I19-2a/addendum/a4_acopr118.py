"""A4: every row any store holds for NLP-46 ACOPR118/sqp-qn (R, H0, P, C1; both tiers), straight from the leg directories."""
from common import *
import os, csv
for label, store, arms in (("store-R", STORE_R, ["H0"]), ("store-Hbase", HB + "/.scratch/i19-2a/store-Hbase", ["H0"]), ("store-H", STORE_H, ["H0", "P", "C1"])):
    for arm in arms:
        base = os.path.join(store, "NLP-46", arm)
        for dirpath, _, files in sorted(os.walk(base)):
            if "results.csv" not in files:
                continue
            rows, _t = i19_run.read_leg_rows(dirpath)
            for r, side in rows:
                if r["problem"] == "ACOPR118" and r["options_label"].startswith("sqp-qn"):
                    print("%-12s %-3s %-60s status=%-12s outcome=%-10s wall_s=%-8s iters=%-4s fact=%-6s side=%s" % (
                        label, arm, os.path.relpath(dirpath, store), r["status"] or "(none)", r.get("outcome"), r.get("wall_s"), r.get("iterations"), r.get("factorizations"), "present" if side else "absent"))
print("\nwall cap (NLP-46):", spec["corpora"]["NLP-46"]["wall_cap_s"], "s")
print("\nTier assignment rows for ACOPR118/sqp-qn:")
for ln in open(STORE_H + "/NLP-46/tier-assignment.csv"):
    if "ACOPR118,sqp-qn" in ln: print(" ", ln.strip())

print("\nRaw results.csv rows including the superseded co-run legs (superseded.txt names the cell the exclusive leg replaced):")
import glob
for arm in ("H0", "P", "C1"):
    for f in sorted(glob.glob("%s/NLP-46/%s/sqp-qn/nlp/*/results.csv" % (STORE_H, arm))):
        sup = os.path.join(os.path.dirname(f), "superseded.txt")
        for r in csv.DictReader(l for l in open(f) if not l.startswith("#")):
            if r["problem"] == "ACOPR118":
                print("  %-3s %-24s status=%s outcome=%s wall_s=%.1f superseded=%s" % (arm, os.path.basename(os.path.dirname(f)), r["status"] or "(none)", r["outcome"], float(r["wall_s"]), os.path.exists(sup) and "ACOPR118" in open(sup).read()))

print("\nNLP-46 dnf_wall (no status) rows per arm (non-superseded rows):")
for arm in ("H0", "P", "C1"):
    d = load(STORE_H, arm, ["NLP-46"])
    L = sorted(c[2] + "/" + c[3] for c, r in d.items() if not r["ended"])
    print("  %-3s %d: %s" % (arm, len(L), ", ".join(L)))
