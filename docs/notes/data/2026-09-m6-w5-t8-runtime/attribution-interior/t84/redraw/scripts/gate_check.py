#!/usr/bin/env python3
"""W5 T8.9r-attrib4 -- the correctness gate's comparison.

Every column of the patched arm's `--engine interior --cells all` CSV against
e51a7e0's, cell by cell. The ONLY column allowed to differ is `wall_s`; a single
other differing cell fails the gate. Exit 0 when every compared arm passes.
"""
import sys

def load(path):
    rows, hdr = [], None
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        f = line.rstrip("\n").split(",")
        if hdr is None:
            hdr = f
            continue
        rows.append(f)
    return hdr, rows

def key(hdr, r):
    return tuple(r[hdr.index(c)] for c in ("cell_id", "treatment") if c in hdr)

def main():
    base = sys.argv[1]
    ok = True
    bh, br = load(base)
    print("REFERENCE %s rows=%d cols=%d" % (base, len(br), len(bh)))
    for p in sys.argv[2:]:
        ph, pr = load(p)
        if ph != bh:
            print("GATE %s FAIL -- header differs" % p); ok = False; continue
        if len(pr) != len(br):
            print("GATE %s FAIL -- %d rows against %d" % (p, len(pr), len(br))); ok = False; continue
        wall = bh.index("wall_s") if "wall_s" in bh else None
        if wall is None:
            print("GATE %s FAIL -- no wall_s column to exempt" % p); ok = False; continue
        diffs, cells = [], 0
        for i, (a, b) in enumerate(zip(br, pr)):
            for j, (x, y) in enumerate(zip(a, b)):
                if j == wall:
                    continue
                cells += 1
                if x != y:
                    diffs.append((i, bh[j], x, y))
        print("GATE %s rows=%d compared_cells=%d mismatches=%d -- %s"
              % (p, len(pr), cells, len(diffs), "PASS" if not diffs else "FAIL"))
        for d in diffs[:20]:
            print("   row %d col %s: %r != %r" % d)
        if diffs:
            ok = False
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
