#!/usr/bin/env python3
"""PF-2d: classify TRAIN movers of arm NEW vs arm REF. usage: train_movers.py NEW_DIR DN3_DIR REF_DIR
Prints, per mover row, the keys that differ; counts status movers (the `status` key) and counter movers."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import train_check as tc

ndir, d3, ref = sys.argv[1:4]
roster = [l.rstrip("\n").split("\t") for l in open(os.path.join(d3, "roster.tsv")) if l.strip()]
status_movers, other, nrows = [], [], 0
keycount = {}
for pop, lane, prob in roster:
    tag = f"{pop}-{lane}-{prob}"
    for arm in ("D", "K"):
        eq, new, old, diffs, ig = tc.row_equal(os.path.join(ndir, "cells", f"{tag}.{arm}"), os.path.join(ref, "cells", f"{tag}.{arm}"))
        nrows += 1
        if eq is None or eq:
            if eq is None:
                print(f"MISSING {tag}.{arm}")
            continue
        keys = [d.split(":")[0] for d in diffs]
        for k in keys:
            keycount[k] = keycount.get(k, 0) + 1
        (status_movers if "status" in keys else other).append(f"{tag}.{arm}")
        print(f"MOVER {tag}.{arm} keys={','.join(keys)}")
        for d in diffs[:12]:
            print(f"    {d}")
print(f"MOVERS_SUMMARY rows {nrows}; status movers {len(status_movers)}; counter/other movers {len(other)}")
for k, v in sorted(keycount.items(), key=lambda x: -x[1]):
    print(f"  KEY {k}: {v} rows")
for r in status_movers:
    print(f"  STATUS_MOVER {r}")
