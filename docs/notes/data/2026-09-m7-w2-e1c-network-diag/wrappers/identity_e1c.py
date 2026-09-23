#!/usr/bin/env python3
"""M7 W2 E1c -- THE SMOKE'S IDENTITY GATE: this task's probe at `--arm off`
(equality basis at its structural default, ON) against the harness's
COMMITTED E1-RETAKE row for the same cell, read from the bytes of

  hven-bench runs/2026-09-m7-w2-e1-retake/run-A/cutest/results.csv
  hven-bench runs/2026-09-m7-w2-e1-retake/run-A/cutest/counters-hven-sqp.csv

THE COMPARISON IS M3c's, IMPORTED UNMODIFIED. `identity_m3c.py` (M3c's
committed wrapper) is loaded by path; its field map (status, the three CORE
counters, f, the fifteen EXTENDED counters, the four residuals), its pinned
harness headers, its NaN-spelling rule and CLAUDE.md section 7's near-ulp gate
(residual class only) are used as they are. Its two harness paths are pointed at
the E1-retake files through the environment overrides the module itself
provides (`M3C_RESULTS_CSV`, `M3C_COUNTERS_CSV`), set BEFORE the import.

ONE THING IS REPLACED, and it is the STRUCTURAL check: M3c's pins M3c's own
probe's key block (and its LADDER/HISTORY blocks), which this M5-lineage probe
does not print. Its replacement here:
  * sentinels: exactly one PROBE_BEGIN (first line) and PROBE_END (last line);
  * DUPLICATE REFUSAL: a key printed twice anywhere in the row is REFUSED (a
    row that disagrees with itself cannot be compared);
  * the E1c keys this task exists for are present (`opt_equality_basis`,
    `eqb_computed`, `eqb_dropped_count`, `c_major_iters`, ...);
  * FLOOR: every one of the field pairs M3c's map names must be COMPARED (both
    sides present), not merely examined -- 24 at M3c's map.
`trace_out` is a LABEL: it is not in the field map and is never compared; this
gate asserts that, rather than assuming it.

  usage: identity_e1c.py --rows DIR --cells ACOPP57 [--require-zero-near-ulp]
  exit 0 reproduces; 1 differs; 2 structural refusal.
"""
import argparse
import os
import sys

BENCH = "/home/ghecht/Projects/hven-bench"
RUN = os.path.join(BENCH, "runs/2026-09-m7-w2-e1-retake/run-A/cutest")
M3C_WRAPPERS = ("/home/ghecht/Projects/hven/docs/notes/data/"
                "2026-09-m7-w2-m3c-default-census/wrappers")

REQUIRED_E1C_KEYS = [
    "opt_equality_basis", "equality_basis_touched", "eqb_computed", "eqb_undecidable",
    "eqb_dropped_count", "eqb_dropped_rows", "eqb_me_reduced", "eqb_recomputes",
    "eqb_switched_off", "eqb_inconsistent_row", "eqb_ce_size", "c_major_iters",
    "c_qp_minor_iters", "c_factorizations", "c_evals_values", "e1c_history_rows",
    "e1c_event_rows",
]
# CSV blocks: their rows carry no `=`, so they cannot collide with a key; the
# `*_rows` count keys inside them are ordinary keys and ARE checked.


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rows", required=True)
    ap.add_argument("--cells", required=True)
    ap.add_argument("--require-zero-near-ulp", action="store_true")
    args = ap.parse_args()

    res = os.path.join(RUN, "results.csv")
    cnt = os.path.join(RUN, "counters-hven-sqp.csv")
    for p in (res, cnt, os.path.join(M3C_WRAPPERS, "identity_m3c.py"), args.rows):
        if not os.path.exists(p):
            print(f"REFUSED [setup] missing {p}")
            return 2
    os.environ["M3C_RESULTS_CSV"] = res
    os.environ["M3C_COUNTERS_CSV"] = cnt
    sys.path.insert(0, M3C_WRAPPERS)
    import identity_m3c as im  # noqa: E402  (UNMODIFIED)
    if im.RESULTS_CSV != res or im.COUNTERS_CSV != cnt:
        print("REFUSED [setup] the module did not take the E1-retake paths")
        return 2

    field_names = ([im.CORE_STATUS[0]] + [k for k, _, _ in im.CORE_INT] + [im.CORE_RESIDUAL[0]]
                   + [k for k, _, _ in im.EXTENDED_INT] + [k for k, _, _ in im.EXTENDED_RESIDUAL])
    floor = len(field_names)
    if "trace_out" in field_names:
        print("REFUSED [setup] trace_out is in the field map; it must be a label")
        return 2

    def structural(cell, path, probe):
        msgs = []
        body = probe["body"]
        if not body:
            return [f"REFUSED [sentinels] {cell}: EMPTY row ({path})"]
        if body[0] != "PROBE_BEGIN" or body[-1] != "PROBE_END":
            msgs.append(f"REFUSED [sentinels] {cell}: first={body[0]!r} last={body[-1]!r}")
        if body.count("PROBE_BEGIN") != 1 or body.count("PROBE_END") != 1:
            msgs.append(f"REFUSED [sentinels] {cell}: PROBE_BEGIN/END not exactly once")
        seen, dups = set(), []
        for line in body:
            if "=" in line and not line.startswith(("PROBE_", "#")):
                k = line.partition("=")[0]
                if k in seen:
                    dups.append(k)
                seen.add(k)
        if dups:
            msgs.append(f"REFUSED [duplicate] {cell}: keys printed more than once: {sorted(set(dups))}")
        missing = [k for k in REQUIRED_E1C_KEYS if k not in probe["kv"]]
        if missing:
            msgs.append(f"REFUSED [e1c-keys] {cell}: missing {missing}")
        return msgs

    im.structural_refusals = structural

    results_rows = im.read_csv_rows(res, im.RESULTS_HEADER, "results.csv")
    counters_rows = im.read_csv_rows(cnt, im.COUNTERS_HEADER, "counters-hven-sqp.csv")
    print("M7 W2 E1c -- SMOKE IDENTITY: probe row vs the COMMITTED E1-retake harness row")
    print(f"  results.csv   {res}")
    print(f"  counters.csv  {cnt}")
    print(f"  rows          {args.rows}")
    print(f"  field map     identity_m3c.py (unmodified), {floor} pairs: {field_names}")
    print("  trace_out     a LABEL: not in the field map, never compared")
    rc = 0
    for cell in [c for c in args.cells.split(",") if c]:
        verdict, lines = im.compare_cell(cell, args.rows, results_rows, counters_rows)
        for ln in lines:
            print(ln)
        if verdict == "REFUSED":
            print(f"IDENTITY {cell} REFUSED")
            rc = max(rc, 2)
            continue
        compared_line = [ln for ln in lines if "fields compared" in ln]
        compared = int(compared_line[-1].split()[1]) if compared_line else -1
        nulp = sum(1 for ln in lines if "NEAR-ULP ACCEPTED" in ln)
        print(f"{cell} FLOOR: {compared} of {floor} pairs compared (both sides present)")
        print(f"{cell} near-ulp acceptances (passed by tolerance, not by bytes): {nulp}")
        if compared != floor:
            print(f"IDENTITY {cell} REFUSED -- below the floor")
            rc = max(rc, 2)
            continue
        if verdict != "PASS":
            print(f"IDENTITY {cell} DIFFERS")
            rc = max(rc, 1)
            continue
        if args.require_zero_near_ulp and nulp:
            print(f"IDENTITY {cell} DIFFERS -- {nulp} near-ulp acceptance(s) where zero is required")
            rc = max(rc, 1)
            continue
        print(f"IDENTITY {cell} REPRODUCES")
    print(f"IDENTITY_RC={rc}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
