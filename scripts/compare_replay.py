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

THE OPT-IN FLOATING-MEASURE GATE (`--residual-gate <rel>`)
----------------------------------------------------------
The FLAG keeps its W6 T1 spelling, `--residual-gate`, though W6 T6b widened what
it covers from the residual class to every non-wall float: the flag is part of
an invocation contract that frozen evidence cites by name
(docs/notes/data/2026-09-m6-w6-lto/replay/comparisons.txt), and renaming it
would break a reader's ability to re-run what that artifact records.  The
concept's INTERNAL name did move -- see `is_float_measure_column` below.

Without the flag this is EXACT STRING EQUALITY on every column but `wall_s` --
byte-identical behaviour and byte-identical output to the artifact copy, which
is what every existing acceptance protocol keeps.

THE QUALIFICATION, PRECISELY (M6 W6 T1 fix1).  That identity is the SUPPORTED,
VALID INVOCATION CONTRACT -- three positionals, A.csv B.csv LABEL -- and not
every possible argv.  Two flag-absent calls differ from the artifact copy:
  * an INVALID invocation (anything but exactly three positionals) prints the
    usage line above, where the artifact copy raised an `IndexError` traceback
    out of `sys.argv[1], sys.argv[2], sys.argv[3]`; and
  * an argument spelled exactly `--residual-gate` (or `--residual-gate=...`) is
    now read as an OPTION, where the artifact copy would have taken it as a
    positional path or label.
Neither is reachable from a valid three-positional call -- which is why the
default is still byte-exact for every invocation the acceptance protocols make
-- but it is said here rather than left for a reader to discover.

With the flag, the FLOATING MEASURE columns relax.  The set is the corpus
schema's, BY INDEX, and is the same set as
tests/sqp/test_corpus_cells.cpp:974 (`is_residual_column`, which keeps its name
there because the RULE is unchanged), carried here with its schema comment
(:937-973).  The cite was :944 until M6 W6 T6b's own second commit moved the
predicate down by 60 lines; repaired at T6 fix1, with every other cite below it
in this file.
    12 kkt_residual
    15 kkt_stationarity   16 kkt_primal        17 kkt_dual_sign
    18 kkt_complementarity 19 dual_scale       20 x_scale
    42 ipqp_rho_demanded_max    43 ipqp_rho_demanded_last
    55 ipqp_restart_shift_max
    72 ipqp_alpha_p_min         73 ipqp_alpha_d_min
Column 13 (wall_s) is a float too and is excluded for a DIFFERENT reason --
timing noise, never a regression contract (CLAUDE.md section 7).  That is the
whole float class: 13 of the schema's 76 columns, twelve of them gated here.
The seven STRINGS -- 0 cell_id, 1 family, 3 window, 4 taxonomy, 6 status,
11 qp_fact_per_qp, 14 kkt_verdict -- stay exact, and so do the remaining 56
INTEGER FIELDS: no floating-point arithmetic produces them, so a single-bit move
in one is a real regression.  FIELDS, NOT "COUNTERS" (T6 fix1): most of the 56
are semantic counters, but `n_nodes` is input metadata and column 50
`ipqp_final_inertia_read` is categorical (solver_counters.h:378) -- integer-typed
and byte-exact either way, which is all this comparator needs of them.

COLUMNS 42 AND 43 READ ZERO IN EVERY COMMITTED CSV (a pinned scan at T6b: 80
76-column files, 2160 rows, every one of them zero), so the gate's relative arm
is not exercised on them by any committed pair; the suite's falsifier SEEDS both
sides to 1e-4 to make it live.  A seeded value is a fixture's, never an observed
reading, and the distinction matters when reading that falsifier's evidence.

CORRECTED HERE AT M6 W6 T6b (settler ruling R-GATE, W6 close), and the
correction is the point of the task.  This comment used to end "Everything at 21
and above is an integer counter", and the gated set was 12 and 15-20 only.  That
was FALSE at source: `bench/bench_corpus.cpp`'s `write_outcome` prints five
columns at or above 21 with `{:.9e}` (the header at :879-899, the format at
:938-946), and each is a `double` member of `IpqpCounters`
(include/hven/core/solver_counters.h:506 `ipqp_rho_demanded_max`, :516
`ipqp_rho_demanded_last`, :663 `ipqp_restart_shift_max`, :842
`ipqp_alpha_p_min`, :849 `ipqp_alpha_d_min` -- :849 is the declaration; the
:844 this file carried until T6 fix1 is the first line of its doc comment).
W6 T3's LTO replay then moved two of them -- `ipqp_alpha_p_min` by 1.76e-10
relative and `ipqp_alpha_d_min` by 2.11e-9 -- on `f7_n1000_path_warm`, while
every counter and every status stayed byte-identical: exactly the
address-sensitivity this gate exists for, on columns the gate did not reach
(docs/notes/data/2026-09-m6-w6-lto/, frozen).  The set is now the schema's
actual non-wall floats and nothing else.

The rule is the suite's, not a new one: byte equality FIRST, and only if that
fails, agreement to within `<rel>` RELATIVE with an ABSOLUTE FLOOR of 1e-13 --

    |a - b| <= max(rel * max(|a|, |b|), 1e-13)

-- which is tests/sqp/test_corpus_cells.cpp:981-1012 (the relative half, with
the derivation of 1e-5 at :908-935) together with the floor at :3618-3623 (its
derivation at :3607-3617).  Non-numeric and non-finite spellings NEVER pass the
gate: they fall back to the byte comparison, so an `inf` or a `nan` is reported,
not waved through (:998-1006).  (Every one of those line numbers except :908-935
moved by +60 at M6 W6 T6b and is repaired here at T6 fix1; :908-935 sits above
the insertion and did not move.)

WHY THE GATE EXISTS: CLAUDE.md section 7 -- MKL's kernels are address-sensitive,
so a residual can differ in its last digits between two processes running
identical code at MKL_NUM_THREADS=1.

A gated-but-agreeing column prints a `WITHIN-GATE` line, a line class distinct
from the difference listing; it does not count as a difference and does not
change the exit status.  The `=== <label> ===` line gains `  within gate: <g>`
only when the flag is given, counting gated COLUMNS.
"""
import csv, sys

# tests/sqp/test_corpus_cells.cpp:974 and :3618.  RENAMED at M6 W6 T6b from
# `RESIDUAL_COLUMNS` / `is_residual_column`: the set is no longer only the
# residual class, so the name would mislead.  A plain rename, NO alias -- nothing
# outside this file calls either (the whole repository was grepped; the only
# callers are `main` below and the suite's own C++ predicate of the same shape).
FLOAT_MEASURE_COLUMNS = frozenset([12, 15, 16, 17, 18, 19, 20, 42, 43, 55, 72, 73])
# The floor keeps its name and its value: it is the RESIDUAL-derived floor
# (test_corpus_cells.cpp:3607-3617 argues it from residual norms formed by
# cancellation from O(1) data), and the RULE it belongs to is unchanged by T6b.
# Only the SET the rule is applied to widened.  On the five columns added at T6b
# the relative arm is the live one anyway: their values are O(1e-7) to O(1), so
# `rel * scale` exceeds the floor by orders.
RESIDUAL_ABS_FLOOR = 1e-13

def is_float_measure_column(i):
    return i in FLOAT_MEASURE_COLUMNS

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
    gated = {ha[i] for i in range(len(ha)) if is_float_measure_column(i)} if rel is not None else set()
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
