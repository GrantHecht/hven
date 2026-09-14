#!/usr/bin/env python3
"""Compare two corpus replay CSVs on every non-wall column, joined on cell_id.

A COPY of docs/notes/data/2026-09-m6-w4-acceptance/probes/compare_replay.py
(sha256 c9dc9a65260f49e44bf2fb03aa165041ef3872d46aba756fbdef6bebc229c624), the
comparator every M6 W2-W5 acceptance replay used, moved into scripts/ so the
record is reproducible from a fresh clone.  The artifact copy is NOT touched,
NOT moved and NOT symlinked: its bytes are a pin, cited by content in
docs/notes/data/2026-09-m6-w5-acceptance/PROVENANCE.txt:118-119 (CLAUDE.md
section 7, and section 1's second exception).  This copy carries ONE addition,
`--residual-gate`, and is otherwise that file.

usage: compare_replay.py [--residual-gate REL] A.csv B.csv LABEL

THE OPT-IN RESIDUAL GATE (`--residual-gate <rel>`)
--------------------------------------------------
Without the flag this is EXACT STRING EQUALITY on every column but `wall_s` --
byte-identical behaviour and byte-identical output to the artifact copy, which
is what every existing acceptance protocol keeps.

With the flag, the residual-class columns relax.  The set is the corpus
schema's, BY INDEX, and is the same set as
tests/sqp/test_corpus_cells.cpp:944 (`is_residual_column`), carried here with
its schema comment (:937-943):
    12 kkt_residual
    15 kkt_stationarity   16 kkt_primal        17 kkt_dual_sign
    18 kkt_complementarity 19 dual_scale       20 x_scale
Column 13 (wall_s) is excluded for a different reason -- timing noise, never a
regression contract (CLAUDE.md section 7).  Column 14 (kkt_verdict) is a string
and stays exact.  Everything at 21 and above is an integer counter and stays
exact: no floating-point arithmetic produces them, so a single-bit move in one
is a real regression.

The rule is the suite's, not a new one: byte equality FIRST, and only if that
fails, agreement to within `<rel>` RELATIVE with an ABSOLUTE FLOOR of 1e-13 --

    |a - b| <= max(rel * max(|a|, |b|), 1e-13)

-- which is tests/sqp/test_corpus_cells.cpp:949-980 (the relative half, with
the derivation of 1e-5 at :908-935) together with the floor at :3558-3563 (its
derivation at :3547-3557).  Non-numeric and non-finite spellings NEVER pass the
gate: they fall back to the byte comparison, so an `inf` or a `nan` is reported,
not waved through (:966-975).

WHY THE GATE EXISTS: CLAUDE.md section 7 -- MKL's kernels are address-sensitive,
so a residual can differ in its last digits between two processes running
identical code at MKL_NUM_THREADS=1.

A gated-but-agreeing column prints a `WITHIN-GATE` line, a line class distinct
from the difference listing; it does not count as a difference and does not
change the exit status.  The `=== <label> ===` line gains `  within gate: <g>`
only when the flag is given, counting gated COLUMNS.
"""
import csv, sys

# tests/sqp/test_corpus_cells.cpp:944 and :3558.
RESIDUAL_COLUMNS = frozenset([12, 15, 16, 17, 18, 19, 20])
RESIDUAL_ABS_FLOOR = 1e-13

def is_residual_column(i):
    return i in RESIDUAL_COLUMNS

def residual_columns_agree(a, b, rel):
    """Byte-equal, or agreeing to within the gate.  See the module docstring."""
    if a == b:
        return True
    try:
        va = float(a)
        vb = float(b)
    except ValueError:
        return False
    # Non-finite never passes the numeric path: inf against a finite value gives
    # |inf - x| = inf and scale = inf, so a bare relative test reads inf <= inf
    # and SUCCEEDS.  An infinite residual is the loudest regression this column
    # can report; NaN is caught here too.
    if va != va or vb != vb or va in (float("inf"), float("-inf")) or vb in (
        float("inf"), float("-inf")):
        return False
    scale = max(abs(va), abs(vb))
    return abs(va - vb) <= max(rel * scale, RESIDUAL_ABS_FLOOR)

def load(path):
    rows, hdr = {}, None
    with open(path, newline="") as f:
        lines = [l for l in f if not l.startswith("#")]
    r = csv.reader(lines)
    hdr = next(r)
    for row in r:
        if not row or len(row) != len(hdr):
            continue
        rows[row[0]] = dict(zip(hdr, row))
    return hdr, rows

def main(a, b, label, rel=None):
    ha, ra = load(a)
    hb, rb = load(b)
    if ha != hb:
        print(f"=== {label} === SCHEMA MISMATCH")
        print(" only-in-A:", [c for c in ha if c not in hb])
        print(" only-in-B:", [c for c in hb if c not in ha])
        return 2
    cols = [c for c in ha if c != "wall_s"]
    # The gate is keyed by SCHEMA INDEX, then resolved to the column names this
    # comparator joins on, so it follows the schema rather than a name list.
    gated = {ha[i] for i in range(len(ha)) if is_residual_column(i)} if rel is not None else set()
    keys = sorted(set(ra) & set(rb))
    missing = sorted(set(ra) ^ set(rb))
    diffs = []
    within = []
    for k in keys:
        for c in cols:
            if ra[k][c] != rb[k][c]:
                if c in gated and residual_columns_agree(ra[k][c], rb[k][c], rel):
                    within.append((k, c, ra[k][c], rb[k][c]))
                else:
                    diffs.append((k, c, ra[k][c], rb[k][c]))
    print(f"=== {label} === cells compared: {len(keys)}  columns compared: {len(cols)}  "
          f"differences: {len(diffs)}"
          + (f"  within gate: {len(within)}" if rel is not None else "")
          + (f"  UNMATCHED CELLS: {missing}" if missing else ""))
    for d in diffs[:200]:
        print(f"    {d[0]:32s} {d[1]:34s} A={d[2]}  B={d[3]}")
    for w in within[:200]:
        print(f"    WITHIN-GATE {w[0]} {w[1]} A={w[2]}  B={w[3]}")
    return 1 if (diffs or missing) else 0

def parse_args(argv):
    """The three positionals, plus the optional gate.  `rel` is None by default."""
    rel, positional, i = None, [], 0
    while i < len(argv):
        arg = argv[i]
        if arg == "--residual-gate" or arg.startswith("--residual-gate="):
            if arg == "--residual-gate":
                if i + 1 >= len(argv):
                    raise SystemExit("--residual-gate needs a relative tolerance, e.g. 1e-5")
                value, i = argv[i + 1], i + 2
            else:
                value, i = arg.split("=", 1)[1], i + 1
            try:
                rel = float(value)
            except ValueError:
                raise SystemExit(f"--residual-gate: not a number: {value}")
            if not (rel >= 0.0) or rel == float("inf"):
                raise SystemExit(f"--residual-gate: need a finite, non-negative value: {value}")
            continue
        positional.append(arg)
        i += 1
    if len(positional) != 3:
        raise SystemExit("usage: compare_replay.py [--residual-gate REL] A.csv B.csv LABEL")
    return positional[0], positional[1], positional[2], rel

if __name__ == "__main__":
    _a, _b, _label, _rel = parse_args(sys.argv[1:])
    sys.exit(main(_a, _b, _label, _rel))
