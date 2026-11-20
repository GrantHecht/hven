#!/usr/bin/env python3
"""I18 mutation check: MUTATIONS_AS_DECLARED iff control has no failing test, every must-kill id has a non-empty killers file,
and every recorded id has a killers file.  DECLARED lines: `<id> must-kill` or `<id> recorded` ('#' comments).
usage: mutation_check.py OUT_DIR DECLARED.txt"""
import os
import sys

out, decl = sys.argv[1], sys.argv[2]
fails = []
ctrl = os.path.join(out, "control.killers.txt")
if not os.path.exists(ctrl) or open(ctrl).read().strip():
    fails.append("CONTROL_NOT_CLEAN")
for line in open(decl):
    line = line.split("#", 1)[0].split()
    if not line:
        continue
    mid, kind = line
    p = os.path.join(out, f"{mid}.killers.txt")
    if not os.path.exists(p):
        fails.append(f"{mid}: NOT_RUN")
        continue
    killers = [l for l in open(p).read().splitlines() if l.strip()]
    print(f"{mid} {kind}: {'KILLED' if killers else 'SURVIVED'} by {len(killers)}: {', '.join(killers[:4])}")
    if kind == "must-kill" and not killers:
        fails.append(f"{mid}: SURVIVED")
for f in fails:
    print("  FAIL " + f)
print("MUTATIONS_AS_DECLARED" if not fails else "MUTATIONS_NOT_AS_DECLARED")
sys.exit(1 if fails else 0)
