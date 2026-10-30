#!/usr/bin/env python3
"""Copy a D4QP dump with be (and bi) replaced by zeros: the cold start x = 0 then meets every equality row.
usage: zero_be.py IN OUT"""
import sys
src, dst = sys.argv[1:3]
lines = open(src).read().split("\n")
out = []
i = 0
while i < len(lines):
    L = lines[i]
    parts = L.split()
    out.append(L)
    i += 1
    if len(parts) == 2 and parts[0] == "be":
        n = int(parts[1])
        out.extend(["0"] * n)
        i += n
open(dst, "w").write("\n".join(out))
