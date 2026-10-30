#!/usr/bin/env python3
"""M7 W2 M5 -- the trace SLICER.  No solver runs here.

M4b's `slice_write` format, transcribed: the parent's `#` header comments, then
this round's own `# slice_*` block, then the column line, then that solve's
records verbatim.  A slice is a byte-faithful subset of its parent's data lines;
nothing is recomputed, reformatted or reordered.

The parents are NOT committed (298 MB of arm traces plus M4's OFF parents);
they are PINNED by sha256 in `traces-m5/SHA256-parent-traces-m5.txt`, as M4b
did.  Only the slices the central claims rest on are committed, and
`traces-m5/SLICES.txt` says which and why.

Usage: slice_m5.py OUTDIR PARENT SOLVE REASON
"""

import os
import sys


def main():
    outdir, parent, solve, reason = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    os.makedirs(outdir, exist_ok=True)
    base = os.path.basename(parent)[:-4]          # "<arm>-<cell>" or "off-<cell>"
    head, cols, lines = [], None, []
    with open(parent) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.startswith("#"):
                head.append(line)
                continue
            if cols is None:
                cols = line
                continue
            if line.split(",", 1)[0] == str(solve):
                lines.append(line)
    if not lines:
        sys.exit("slice_m5.py: no records for solve %d in %s" % (solve, parent))
    path = os.path.join(outdir, "%s-solve%d.csv" % (base, solve))
    with open(path, "w") as fh:
        for h in head:
            fh.write(h + "\n")
        fh.write("# SLICE of a flat trace (M7 W2 M5 read-out)\n")
        fh.write("# slice_leg=%s\n" % base)
        fh.write("# slice_solve=%d\n" % solve)
        fh.write("# slice_reason=%s\n" % reason)
        fh.write("# slice_source=%s\n" % parent)
        fh.write("# slice_rows=%d\n" % len(lines))
        fh.write(cols + "\n")
        for r in lines:
            fh.write(r + "\n")
    print("%s  %d rows" % (path, len(lines)))


if __name__ == "__main__":
    main()
