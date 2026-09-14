#!/usr/bin/env python3
"""Walk-census comparator: the 13 asserted columns, baseline vs fresh.

Contract (scripts/run_walk_census.sh depends on it):
  * one `MISMATCH <cell>` line per disagreeing cell, followed by the two
    13-column rows, `base :` then `fresh:`;
  * `MISSING CELL <cell>` / `EXTRA CELL <cell>` for roster problems;
  * a closing `<n> baseline cells, <m> fresh cells, <k> mismatches` line;
  * exit 0 iff every cell matched and the rosters agree, else 1. Anything
    else (traceback) means the comparator itself failed.

wall_s (column 14) is INFORMATIONAL and is excluded: this is a counter
replay, not a timing measurement (CLAUDE.md section 7).

THE OPT-IN RESIDUAL GATE (`--residual-gate <rel>`)
--------------------------------------------------
Without the flag this comparator is EXACT STRING EQUALITY on all 13 asserted
columns -- byte-identical behaviour and byte-identical output to the form this
file has always had, which is what every existing invocation (CI, the census
protocols, the frozen evidence that cites this script by path) keeps.

THE QUALIFICATION, PRECISELY (M6 W6 T1 fix1).  That identity is the SUPPORTED,
VALID INVOCATION CONTRACT -- two positional CSV paths -- and not every possible
argv.  Two flag-absent calls differ from the pre-flag file:
  * an INVALID invocation (anything but exactly two positionals) prints the NEW
    usage line, which names the flag, where the pre-flag file printed
    `usage: census_compare.py BASELINE.csv FRESH.csv`; and
  * an argument spelled exactly `--residual-gate` (or `--residual-gate=...`) is
    now read as an OPTION, where the pre-flag file would have taken it as a
    positional path.
Neither is reachable from a valid two-path call -- which is why the default is
still byte-exact for every invocation that has ever been made of this script --
but it is said here rather than left for a reader to discover.

With the flag, ONE column class relaxes: the residual class. The 13 asserted
columns are cell_id .. kkt_residual, so within this comparator the gate reaches
EXACTLY ONE COLUMN -- index 12, `kkt_residual`. The corpus schema's other
residual-class columns (15 kkt_stationarity, 16 kkt_primal, 17 kkt_dual_sign,
18 kkt_complementarity, 19 dual_scale, 20 x_scale) lie beyond the 13 and are
not compared here at all; scripts/compare_replay.py, which compares the whole
76-column schema, gates ALL TWELVE of its floating measure columns -- those
seven plus 42, 43, 55, 72, 73 (M6 W6 T6b widened it; this sentence said "all
seven" until M6 W6 T6 fix1).

The rule is the suite's, not a new one: byte equality FIRST, and only if that
fails, agreement to within `<rel>` RELATIVE with an ABSOLUTE FLOOR of 1e-13 --

    |a - b| <= max(rel * max(|a|, |b|), 1e-13)

-- which is `tests/sqp/test_corpus_cells.cpp:981-1012` (the relative half, with
its derivation of 1e-5 at :908-935) together with the floor at :3618-3623 (its
derivation at :3607-3617: these are residual norms formed by cancellation from
O(1) data, so their absolute accuracy is a few ulps of 1.0 no matter how small
the norm is, and a purely relative test below ~1e-15 measures noise against
noise).  Non-numeric and non-finite spellings NEVER pass the gate: they fall
back to the byte comparison, so an `inf` or a `nan` is reported, not waved
through (the suite's own reasoning, :998-1006).  (Those line numbers moved by
+60 at M6 W6 T6b, whose second commit inserted 60 lines of schema comment above
them, and are repaired here at M6 W6 T6 fix1; :908-935 sits above the insertion
and did not move.)

WHY THE GATE EXISTS: CLAUDE.md section 7 -- MKL's kernels are address-sensitive,
so a residual can differ in its last digits between two processes running
identical code at MKL_NUM_THREADS=1.  Counters and statuses have no such excuse
and stay EXACT with the flag on: no floating-point arithmetic produces them.

A gated-but-agreeing cell prints a `WITHIN-GATE` line, which is a NEW line
class and deliberately not a `MISMATCH`: scripts/run_walk_census.sh invokes this
script at :410, greps `^MISMATCH ` (:421) and `^(MISSING|EXTRA) CELL ` (:422),
and derives its serial-confirm list by awk on those same three prefixes
(:453-461), so a WITHIN-GATE line is invisible to all of them -- it reaches
neither tally, no FATAL branch (:423-433) and no serial-confirm row.
It does not count as a mismatch
and does not change the exit status.  The closing tally gains a
` (<g> within gate)` suffix only when the flag is given, where <g> counts
gated COLUMNS (one line each), not cells.

The gate is decided PER CELL, because the report is per cell: a cell whose
every difference is inside the gate prints its WITHIN-GATE lines and is not a
mismatch; a cell with even one difference outside the gate is a MISMATCH and is
printed whole -- both 13-column rows -- exactly as it always was.
"""

import sys

ASSERTED = 13  # cell_id .. kkt_residual; wall_s (14th) excluded

# The corpus schema's floating-point measure columns, by index -- a SUBSET of
# tests/sqp/test_corpus_cells.cpp:974 (`is_residual_column`), carried here with
# its schema comment (:937-973).  The cite was :944/:937-943 until M6 W6 T6b
# moved the predicate down by 60 lines; repaired at M6 W6 T6 fix1:
#   12 kkt_residual
#   15 kkt_stationarity   16 kkt_primal        17 kkt_dual_sign
#   18 kkt_complementarity 19 dual_scale       20 x_scale
# Column 13 (wall_s) is excluded for a different reason -- timing noise, never a
# regression contract.  Column 14 (kkt_verdict) is a string and stays exact.
# Of these seven, only 12 is inside this comparator's 13 asserted columns.
#
# THIS COMMENT USED TO END "Everything at 21 and above is an integer counter".
# That is FALSE at source and was corrected in the suite and in
# scripts/compare_replay.py at M6 W6 T6b, and here at M6 W6 T6 fix1: five
# columns at or above 21 are `double` members of `IpqpCounters` printed with
# `{:.9e}` -- 42 ipqp_rho_demanded_max, 43 ipqp_rho_demanded_last,
# 55 ipqp_restart_shift_max, 72 ipqp_alpha_p_min, 73 ipqp_alpha_d_min
# (include/hven/core/solver_counters.h:506, :516, :663, :842, :849).  THE SET
# BELOW DOES NOT CHANGE and this comparator's gate is unchanged by the
# correction: all five lie beyond its 13 asserted columns, which end at 12
# kkt_residual, so there is nothing here for them to relax.  The remaining
# fields are integer-typed; "fields" rather than "counters", since `n_nodes` is
# input metadata and column 50 is categorical.
RESIDUAL_COLUMNS = frozenset([12, 15, 16, 17, 18, 19, 20])

# tests/sqp/test_corpus_cells.cpp:3618 (was :3558 before T6b's insertion).
RESIDUAL_ABS_FLOOR = 1e-13


def is_residual_column(i):
    return i in RESIDUAL_COLUMNS


def residual_columns_agree(a, b, rel):
    """True when two spellings are byte-equal or agree to within the gate.

    Mirrors tests/sqp/test_corpus_cells.cpp:981-1012 with the absolute floor of
    :3618-3623.  A column that does not parse as a finite number is NOT quietly
    waved through: it falls back to the byte comparison the caller reports.
    """
    if a == b:
        return True
    try:
        va = float(a)
        vb = float(b)
    except ValueError:
        return False
    # Non-finite never passes the numeric path.  Without this, the relative form
    # below waves through every infinity pairing: inf against a finite value
    # gives |inf - x| = inf and scale = inf, so the test reads inf <= inf and
    # SUCCEEDS.  An infinite residual is the loudest regression this column can
    # report.  NaN is caught here too (a NaN fails every comparison below, but
    # saying so here is clearer than relying on that).
    if va != va or vb != vb or va in (float("inf"), float("-inf")) or vb in (
        float("inf"),
        float("-inf"),
    ):
        return False
    scale = max(abs(va), abs(vb))
    return abs(va - vb) <= max(rel * scale, RESIDUAL_ABS_FLOOR)


def load(path):
    rows = {}
    header = None
    with open(path, newline="") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split(",")
            if fields[0] == "cell_id":
                if header is None:
                    header = fields
                continue
            if len(fields) < ASSERTED:
                continue
            rows[fields[0]] = fields[:ASSERTED]
    if header is None:
        raise SystemExit(f"{path}: no header row found")
    if len(header) < ASSERTED:
        raise SystemExit(f"{path}: header has {len(header)} columns, need >= {ASSERTED}")
    return header, rows


def parse_args(argv):
    """The two positionals, plus the optional gate.  Returns (base, fresh, rel).

    `rel` is None when the flag was not given, which is the byte-exact default.
    """
    rel = None
    positional = []
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == "--residual-gate":
            if i + 1 >= len(argv):
                raise SystemExit("--residual-gate needs a relative tolerance, e.g. 1e-5")
            try:
                rel = float(argv[i + 1])
            except ValueError:
                raise SystemExit(f"--residual-gate: not a number: {argv[i + 1]}")
            if not (rel >= 0.0) or rel in (float("inf"),):
                raise SystemExit(f"--residual-gate: need a finite, non-negative value: {argv[i + 1]}")
            i += 2
            continue
        if arg.startswith("--residual-gate="):
            value = arg.split("=", 1)[1]
            try:
                rel = float(value)
            except ValueError:
                raise SystemExit(f"--residual-gate: not a number: {value}")
            if not (rel >= 0.0) or rel in (float("inf"),):
                raise SystemExit(f"--residual-gate: need a finite, non-negative value: {value}")
            i += 1
            continue
        positional.append(arg)
        i += 1
    if len(positional) != 2:
        raise SystemExit(
            "usage: census_compare.py [--residual-gate REL] BASELINE.csv FRESH.csv"
        )
    return positional[0], positional[1], rel


def main():
    base_path, fresh_path, rel = parse_args(sys.argv[1:])
    base_hdr, base = load(base_path)
    fresh_hdr, fresh = load(fresh_path)
    if base_hdr[:ASSERTED] != fresh_hdr[:ASSERTED]:
        raise SystemExit(
            "schema disagreement over the asserted columns:\n"
            f"  base : {','.join(base_hdr[:ASSERTED])}\n"
            f"  fresh: {','.join(fresh_hdr[:ASSERTED])}"
        )

    bad = 0
    for cell in base:
        if cell not in fresh:
            print(f"MISSING CELL {cell}")
            bad += 1
    for cell in fresh:
        if cell not in base:
            print(f"EXTRA CELL {cell}")
            bad += 1

    mismatches = 0
    within_gate = 0
    for cell, row in base.items():
        other = fresh.get(cell)
        if other is None or other == row:
            continue
        if rel is not None:
            # Every column that differs is either inside the residual gate or a
            # real disagreement.  A cell all of whose differences are inside the
            # gate is NOT a mismatch; a cell with even one other difference is,
            # and is reported whole (both rows) exactly as before.
            gated = []
            for i in range(ASSERTED):
                if row[i] == other[i]:
                    continue
                if is_residual_column(i) and residual_columns_agree(row[i], other[i], rel):
                    gated.append(i)
                else:
                    gated = None
                    break
            if gated is not None:
                for i in gated:
                    print(
                        f"WITHIN-GATE {cell} {base_hdr[i]} "
                        f"base={row[i]} fresh={other[i]}"
                    )
                within_gate += len(gated)
                continue
        mismatches += 1
        print(f"MISMATCH {cell}")
        print(f"  base : {','.join(row)}")
        print(f"  fresh: {','.join(other)}")

    tally = f"{len(base)} baseline cells, {len(fresh)} fresh cells, {mismatches} mismatches"
    if rel is not None:
        tally += f" ({within_gate} within gate)"
    print(tally)
    sys.exit(1 if (mismatches or bad) else 0)


if __name__ == "__main__":
    main()
