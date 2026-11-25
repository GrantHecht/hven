#!/usr/bin/env python3
"""kopt-1x r2: the gray-exit census over the TRAIN lane rows (side.optimal_row_gray_exits): per cell and in total, arm A vs arm B.
usage: gray_census.py <A cells parent dir> <B dir>  (each holds cells/<tag>.<arm>.out)"""
import glob, os, sys
def census(d):
    out = {}
    for f in sorted(glob.glob(os.path.join(d, "cells", "*.out"))):
        for line in open(f):
            if line.startswith("side.optimal_row_gray_exits="):
                v = int(line.split("=", 1)[1])
                if v:
                    out[os.path.basename(f)[:-4]] = v
    return out
a, b = census(sys.argv[1]), census(sys.argv[2])
for name, c in (("A", a), ("B", b)):
    print(f"ARM {name}: {sum(c.values())} gray exits in {len(c)} cells")
    for k, v in c.items():
        print(f"  {name} {k} {v}")
