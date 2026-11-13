#!/usr/bin/env python3
"""M8 DN-3 (../DECLARATION-r1.md): copied from ../../2026-11-m8-dn0/tools/roster.py; ONE change: the lane binary
that must --list every problem is argv[2] (the adoption lane binary), not HARNESS_ROOT/build/.

M8 DN-0 roster: HS TRAIN (22) + cutest-small-dense v1 TRAIN (97), both hven lanes. TRAIN ONLY.

Every name is re-checked against the harness's own split policy (schema/split.py, split-v1)
and, for CUTEst, against schema/holdout_guard.py; any HOLDOUT name aborts (exit 3) before a
cell runs. The lane binary's --list must name every problem.

Usage: roster.py HARNESS_ROOT  -> prints "population<TAB>lane<TAB>problem" lines, sorted.
"""
import json
import os
import subprocess
import sys

LANES = ("hven-ipm", "hven-sqp")


def main():
    hb = sys.argv[1]
    sys.path.insert(0, os.path.join(hb, "schema"))
    import split as split_policy  # noqa: E402

    hs = json.load(open(os.path.join(hb, "sets", "hs-27.json")))
    hs_train = sorted(p["problem"] for p in hs["problems"] if p["split"] == "TRAIN")
    # HS071 (set h1-bootstrap) is HS TRAIN by the same policy (docs/sets.md, "The split").
    hs_train.append("hs071")
    sd = json.load(open(os.path.join(hb, "sets", "cutest-small-dense.json")))
    sd_train = sorted(p["problem"] for p in sd["problems"] if p["split"] == "TRAIN")
    if len(hs_train) != 22 or len(sd_train) != 97:
        sys.exit(f"roster: counts {len(hs_train)}/{len(sd_train)} != 22/97")
    for name in hs_train:
        if split_policy.assign("hs", name) != "TRAIN":
            print(f"roster: REFUSED hs/{name} is not TRAIN", file=sys.stderr)
            sys.exit(3)
    for name in sd_train:
        if split_policy.assign("cutest", name) != "TRAIN":
            print(f"roster: REFUSED cutest/{name} is not TRAIN", file=sys.stderr)
            sys.exit(3)
    guard = subprocess.run([sys.executable, os.path.join(hb, "schema", "holdout_guard.py"),
                            *sd_train], capture_output=True, text=True)
    if guard.returncode != 0:
        print(guard.stdout + guard.stderr, file=sys.stderr)
        sys.exit(3)
    listing = subprocess.run([sys.argv[2],
                              "--list"], capture_output=True, text=True, check=True).stdout
    listed = {f.split("=", 1)[1] for line in listing.splitlines() for f in line.split("|")
              if f.startswith("problem=")}
    missing = [n for n in hs_train + sd_train if n not in listed]
    if missing:
        sys.exit(f"roster: lane binary does not list {missing}")
    for pop, names in (("hs", hs_train), ("sd", sd_train)):
        for lane in LANES:
            for name in names:
                print(f"{pop}\t{lane}\t{name}")


if __name__ == "__main__":
    main()
