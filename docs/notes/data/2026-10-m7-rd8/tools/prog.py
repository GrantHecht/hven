#!/usr/bin/env python3
"""Per-window progress of a replay CSV: actions, paths, ws size, objective. usage: prog.py CSV [WINDOW]"""
import csv
import sys
from collections import Counter

ACT = {0: "unset", 1: "repair", 2: "repairRef", 3: "postProbe", 4: "dropD", 5: "dropZP", 6: "suspEsc", 7: "term",
       8: "rideStep", 9: "rideUnb", 10: "add", 11: "move", 12: "elimRetreat", 13: "rideUndone", 14: "rankRep",
       15: "stagStop", 16: "stagRefine"}
PATH = {0: "none", 1: "empty", 2: "elim", 3: "border"}
rows = list(csv.DictReader(open(sys.argv[1])))
W = int(sys.argv[2]) if len(sys.argv) > 2 else 100
for s in range(0, len(rows), W):
    ch = rows[s:s + W]
    a = Counter(ACT.get(int(r["action"]), r["action"]) for r in ch)
    p = Counter(PATH.get(int(r["path"]), r["path"]) + ("L" if r["latched"] == "1" else "") for r in ch)
    deg = sum(int(r["deg"]) for r in ch)
    pf = sum(int(r["pf"]) for r in ch)
    print("minors %5d-%5d ws %4s->%4s obj %+.6e -> %+.6e deg %3d facts %4d | %s | %s" % (
        int(ch[0]["minor"]), int(ch[-1]["minor"]), ch[0]["ws_in"], ch[-1]["ws_out"], float(ch[0]["obj"]),
        float(ch[-1]["obj"]), deg, pf, " ".join("%s:%d" % kv for kv in sorted(a.items())),
        " ".join("%s:%d" % kv for kv in sorted(p.items()))))
