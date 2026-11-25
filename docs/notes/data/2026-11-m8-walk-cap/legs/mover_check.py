#!/usr/bin/env python3
# M8 walk cap (../DECLARATION.md): byte copy of ../../2026-11-m8-dn3/r5/legs/mover_check.py plus this line; the cell list NONE declares no mover (0 differences).
"""M8 DN-3 r5 (../../DECLARATION-r5.md, I3c-A2): read a scripts/compare_replay.py report and assert that every
differing row belongs to a declared mover cell.

A difference line is `    <cell_id>/<treatment>[/<variant>] <column> A=<v> B=<v>`; its cell is the text before the
first '/'. Prints MOVERS_AS_DECLARED (exit 0) iff every difference's cell is in the mover set, and lists the count per
cell; otherwise UNDECLARED_MOVER lines and exit 1. A report without its `=== ... differences: N` summary line exits 2.

Usage: mover_check.py REPORT CELL[,CELL...]
"""
import collections
import re
import sys


def main():
    report, movers = sys.argv[1], set(sys.argv[2].split(","))
    text = open(report).read()
    m = re.search(r"differences: (\d+)", text)
    if not m:
        print("NO_SUMMARY_LINE")
        return 2
    per = collections.Counter()
    bad = []
    for line in text.splitlines():
        if " A=" in line and " B=" in line and line.startswith("    "):
            cell = line.split()[0].split("/")[0]
            per[cell] += 1
            if cell not in movers:
                bad.append(line.strip())
    print(f"differences: {m.group(1)} (parsed {sum(per.values())})")
    for c in sorted(per):
        print(f"  {c}: {per[c]}{'' if c in movers else '  UNDECLARED'}")
    for b in bad:
        print("UNDECLARED_MOVER " + b)
    if sum(per.values()) != int(m.group(1)):
        print("PARSE_COUNT_MISMATCH")
        return 2
    if bad:
        return 1
    print("MOVERS_AS_DECLARED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
