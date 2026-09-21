#!/usr/bin/env python3
"""M7 W2 M3c -- RULE.md section 4's IDENTITY GATE, and nothing else.

Compares THIS task's leg row for a cell against the HARNESS's OWN COMMITTED
ROWS for that cell at the pin `aeebdccc`, read from the bytes of

  runs/2026-09-m7-w2-h-repin/run-A/results.csv                (status, objective, residuals)
  runs/2026-09-m7-w2-h-repin/run-A/cutest/counters-hven-sqp.csv   (the counters)

MACHINE-WRITTEN: no number this produces is transcribed by hand, and no
expectation in it is a copy of RULE.md's reader-facing table (RULE.md section
1.2 says why: a gate that compares a rule's transcription of a file compares the
transcription).

  usage: identity_m3c.py --rows DIR [--cells A,B,...]
  exit   0  every comparable cell PASSED
         1  at least one cell DIFFERS -- a FINDING (RULE.md section 4.4), and
            the chain continues
         2  a STRUCTURAL REFUSAL -- the instrument, not the engine
         3  EVERY comparable cell differed or refused -- RULE.md section 3.2's
            structural bar; the caller stops before group W

RULE.md section 3.2's bar is implemented as exit 3 and NOT as "abort on the
first difference", because the brief's ruling is that a per-cell difference
between the pin and HEAD is a finding to report, while an instrument that is
wrong on every cell is not a finding at all.
"""
import argparse
import math
import os
import sys

ROOT = "/home/ghecht/Projects/hven"
BENCH = "/home/ghecht/Projects/hven-bench"
RUN = os.path.join(BENCH, "runs/2026-09-m7-w2-h-repin/run-A")
# THE TWO HARNESS FILES. The environment overrides exist for ONE purpose, and
# it is the purpose M3a fix1's `M3A_NEW_ROOT` was added for: the fold proofs
# must be able to hand this checker a DELIBERATELY MALFORMED harness file and
# see it refuse. Unset -- which is what every chain leg does -- these are
# exactly the committed paths. They change no rule, no field list and no
# tolerance, and a leg run with either set says so in its own transcript.
RESULTS_CSV = os.environ.get("M3C_RESULTS_CSV", os.path.join(RUN, "results.csv"))
COUNTERS_CSV = os.environ.get("M3C_COUNTERS_CSV",
                              os.path.join(RUN, "cutest/counters-hven-sqp.csv"))

# RULE.md section 1: the twenty cells, and which group each is in. Group W's
# committed row carries `status = null` and no counters, so it is NOT
# comparable and this gate says so rather than passing.
GROUP_E = ["ACOPP118", "ACOPP57", "ACOPR57", "BRITGAS", "DALLASM", "NET3",
           "STEENBRB", "STEENBRC", "STEENBRD", "STEENBRE", "STEENBRG"]
GROUP_I = ["YORKNET"]
GROUP_W = ["ACOPR118", "BIGBANK", "MSS2", "MSS3", "READING8", "SAWPATH",
           "TAX213322", "TAX53322"]
ALL_CELLS = GROUP_E + GROUP_I + GROUP_W

# RULE.md section 4.3 (c): the two committed headers, pinned in order. A
# harness file whose header is not exactly this is REFUSED, never parsed by
# position against a guess.
RESULTS_HEADER = [
    "cell_id", "problem_set", "problem", "set_version", "set_digest", "split",
    "lane", "options_label", "options_digest", "hessian_provenance",
    "budget_iters", "budget_facts", "budget_wall_s", "status", "outcome",
    "iterations", "factorizations", "eval_f", "eval_g", "eval_jac", "eval_hess",
    "objective", "kkt_stationarity", "kkt_primal_eq", "kkt_primal_ineq",
    "kkt_complementarity", "wall_s",
]
COUNTERS_HEADER = [
    "cell_id", "major_iters", "qp_minor_iters", "factorizations",
    "steps_accepted", "rejected_steps", "soc_steps", "soc_applied",
    "elastic_activations", "restoration_iters", "eqp_refine_steps",
    "border_refine_steps", "verdict_refine_steps", "suspect_escalations",
    "symbolic_analyses", "full_step_majors", "watchdog_restores", "evals_full",
    "evals_values", "sqp_stationarity", "sqp_feasibility",
    "sqp_complementarity", "sqp_kkt_residual", "sqp_infeasibility_certified",
    "harness_eval_f", "harness_eval_grad_f", "harness_eval_g",
    "harness_eval_jac", "harness_eval_hess", "harness_dual_sign_violation",
]

# RULE.md section 4.2. (probe key, harness column, source file), byte equality.
CORE_INT = [
    ("c_major_iters", "major_iters", "counters"),
    ("c_qp_minor_iters", "qp_minor_iters", "counters"),
    ("c_factorizations", "factorizations", "counters"),
]
CORE_STATUS = ("status", "status", "results")
CORE_RESIDUAL = ("f", "objective", "results")

EXTENDED_INT = [
    ("c_steps_accepted", "steps_accepted", "counters"),
    ("c_rejected_steps", "rejected_steps", "counters"),
    ("c_soc_steps", "soc_steps", "counters"),
    ("c_soc_applied", "soc_applied", "counters"),
    ("c_elastic_activations", "elastic_activations", "counters"),
    ("c_restoration_iters", "restoration_iters", "counters"),
    ("c_eqp_refine_steps", "eqp_refine_steps", "counters"),
    ("c_border_refine_steps", "border_refine_steps", "counters"),
    ("c_verdict_refine_steps", "verdict_refine_steps", "counters"),
    ("c_suspect_escalations", "suspect_escalations", "counters"),
    ("c_symbolic_analyses", "symbolic_analyses", "counters"),
    ("c_full_step_majors", "full_step_majors", "counters"),
    ("c_watchdog_restores", "watchdog_restores", "counters"),
    ("c_evals_full", "evals_full", "counters"),
    ("c_evals_values", "evals_values", "counters"),
]
EXTENDED_RESIDUAL = [
    ("stationarity", "kkt_stationarity", "results"),
    ("feasibility_e", "kkt_primal_eq", "results"),
    ("feasibility_i", "kkt_primal_ineq", "results"),
    ("complementarity", "kkt_complementarity", "results"),
]

# CLAUDE.md section 7's near-ulp gate, and the ONLY class it may touch.
NEAR_ULP_REL = 1.0e-12

# RULE.md section 4.3 (e): the blocks that must exist, exactly once each.
REQUIRED_BLOCKS = ["LADDER", "OPTIONS", "COUNTERS", "HISTORY", "LEDGER"]

# RULE.md section 4.3 (b): the probe's PROBE_BEGIN key set -- the keys printed
# BEFORE `LADDER_BEGIN` -- pinned as a literal so that a stripped or truncated
# row cannot pass by agreeing with itself.
#
# HOW THE LITERAL WAS DERIVED, because a pin that is a guess is not a pin: it is
# read off THIS TASK'S OWN probe source (`wrappers/m3c_probe.cpp`), in the order
# that file prints them, and CHECKED against the smoke leg's own output before
# any census leg ran -- the smoke transcript records the check. It is M3a's key
# block plus M3c's two additions (`ladder`, `history_tail`) and nothing else.
# A disagreement between this literal and the probe is an instrument defect and
# is fixed here BEFORE a leg is read, never after.
EXPECTED_KEY_BLOCK = {
    # the arguments and the run's own description
    "problem", "budget_iters_argument", "callback", "proximity", "tr_init",
    "qp_cap_retry", "qp_max_iter", "ledger", "ladder", "history_tail",
    "deadline_seconds", "deadline_fired", "n", "me", "mi",
    # the result
    "status", "f", "stationarity", "feasibility_e", "feasibility_i",
    "complementarity", "infeasibility_certified",
    # the counters M3a's key block prints under their bare names
    "major_iters", "restoration_iters", "qp_minor_iters", "factorizations",
    "symbolic_analyses", "steps_accepted", "rejected_steps",
    "elastic_activations", "elastic_escalations", "elastic_rho0_ceiling_hits",
    "elastic_floor_retries", "qp_maxiter_exits", "qp_numerical_exits",
    "qp_failures_routed", "qp_failures_in_a_row_peak", "qp_cap_retries",
    "border_refine_steps", "elastic_from_ipqp_escape",
    # the S5 restoration record
    "restoration_entries", "restoration_sub_status", "restoration_h_in",
    "restoration_h_out", "restoration_f_in", "restoration_f_out",
    # the first QP, and the history's size
    "history_rows", "first_qp_solved", "first_qp_status",
    "first_qp_elastic_applied", "first_qp_plain_status", "first_qp_verdict",
    "first_qp_tr_radius", "first_qp_tr_binding",
    "first_qp_restoration_seed_used", "first_qp_minor_iters",
    "first_qp_factorizations",
    # the ladder's own provenance key
    "x_k_source",
}
# The two keys the ladder prints only when an iterate was copied. At
# `--ladder off` they are absent BY DESIGN, so they are optional rather than
# required -- and they are the ONLY optional keys.
OPTIONAL_KEYS = {"requesting_h", "requesting_f"}

# RULE.md section 2.1: the counters block is TOTAL. The checker pins the count
# rather than the names of all 49, and the probe prints the count itself.
EXPECTED_COUNTER_FIELDS = 49
EXPECTED_ITERATE_FIELDS = 29


def die(msg):
    print(f"REFUSED [setup] {msg}")
    sys.exit(2)


def read_csv_rows(path, header, label):
    """Reads a committed harness CSV. The header must be EXACTLY `header`, in
    order (RULE.md section 4.3 (c)); anything else is a refusal, not a parse."""
    if not os.path.exists(path):
        die(f"{label}: no such file: {path}")
    got_header = None
    rows = {}
    with open(path) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            fields = line.split(",")
            if got_header is None:
                got_header = fields
                if got_header != header:
                    die(f"{label}: header is not the pinned {len(header)}-column header "
                        f"({len(got_header)} columns read); first mismatch at "
                        f"{next((i for i, (a, b) in enumerate(zip(got_header, header)) if a != b), 'length')}")
                continue
            if len(fields) != len(header):
                die(f"{label}: a row has {len(fields)} fields against the header's {len(header)}")
            rows[fields[0]] = dict(zip(header, fields))
    if got_header is None:
        die(f"{label}: the file has no header row at all")
    return rows


def harness_key(cell, lane_suffix):
    return f"cutest/{cell}/hven-sqp/default"


def results_key(cell):
    return f"cutest/{cell}/hven-sqp/default"


def read_probe(path):
    """The probe's own row, SECTIONED.

    `key` holds the keys printed BEFORE `LADDER_BEGIN` -- the PROBE_BEGIN key
    block, which is the set RULE.md section 4.3 (b) pins. `kv` holds every
    key=value line in the file, whichever block it came from, which is where
    the `c_`, `opt_` and `h_` keys live. Sectioning them rather than filtering
    by prefix is what makes the pinned set exact: a new key printed into the
    key block cannot hide behind a prefix rule."""
    if not os.path.exists(path):
        return None
    kv, key_block = {}, {}
    block_counts = {b: [0, 0] for b in REQUIRED_BLOCKS}
    in_key_block = True
    body = []
    with open(path) as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            if not line.strip():
                continue
            body.append(line)
            for b in REQUIRED_BLOCKS:
                if line == f"{b}_BEGIN":
                    block_counts[b][0] += 1
                    in_key_block = False
                elif line == f"{b}_END":
                    block_counts[b][1] += 1
            if "=" in line and not line.startswith(("PROBE_", "#")):
                k, _, v = line.partition("=")
                if k not in kv:
                    kv[k] = v
                if in_key_block and k not in key_block:
                    key_block[k] = v
    return {"kv": kv, "key": key_block, "blocks": block_counts, "body": body}


def structural_refusals(cell, path, probe):
    """RULE.md section 4.3 (a), (b), (d), (e)."""
    msgs = []
    body = probe["body"]
    if not body:
        return [f"REFUSED [sentinels] {cell}: the file is EMPTY -- an empty file "
                f"must never read as agreement ({path})"]
    if body[0] != "PROBE_BEGIN":
        msgs.append(f"REFUSED [sentinels] {cell}: first non-empty line is {body[0]!r}, "
                    f"expected 'PROBE_BEGIN'")
    if body[-1] != "PROBE_END":
        msgs.append(f"REFUSED [sentinels] {cell}: last non-empty line is {body[-1]!r}, "
                    f"expected 'PROBE_END' -- the row is TRUNCATED")
    n_begin, n_end = body.count("PROBE_BEGIN"), body.count("PROBE_END")
    if n_begin != 1 or n_end != 1:
        msgs.append(f"REFUSED [sentinels] {cell}: expected exactly one PROBE_BEGIN and "
                    f"one PROBE_END, found {n_begin} and {n_end}")
    for b in REQUIRED_BLOCKS:
        begins, ends = probe["blocks"][b]
        if begins != 1 or ends != 1:
            msgs.append(f"REFUSED [blocks] {cell}: {b} appears {begins} x BEGIN / {ends} x END, "
                        f"expected exactly one of each")
    kv = probe["kv"]
    got = set(probe["key"]) - OPTIONAL_KEYS
    expected = EXPECTED_KEY_BLOCK
    if got != expected:
        missing, extra = sorted(expected - got), sorted(got - expected)
        msgs.append(f"REFUSED [key-set] {cell}: the key block has {len(got)} keys against the "
                    f"pinned {len(expected)}")
        if missing:
            msgs.append(f"REFUSED [key-set] {cell}:   MISSING: {missing}")
        if extra:
            msgs.append(f"REFUSED [key-set] {cell}:   UNEXPECTED: {extra}")
    if kv.get("c_field_count") != str(EXPECTED_COUNTER_FIELDS):
        msgs.append(f"REFUSED [counters] {cell}: c_field_count is "
                    f"{kv.get('c_field_count')!r}, expected {EXPECTED_COUNTER_FIELDS}")
    if kv.get("h_field_count") != str(EXPECTED_ITERATE_FIELDS):
        msgs.append(f"REFUSED [history] {cell}: h_field_count is "
                    f"{kv.get('h_field_count')!r}, expected {EXPECTED_ITERATE_FIELDS}")
    return msgs


def near_ulp(a, b):
    try:
        x, y = float(a), float(b)
    except (TypeError, ValueError):
        return False, None
    if math.isnan(x) or math.isnan(y):
        return False, None          # NaN agreement is a BYTE fact, never a numeric one
    if math.isinf(x) or math.isinf(y):
        return False, None
    gap = abs(x - y)
    return gap <= NEAR_ULP_REL * max(1.0, abs(x), abs(y)), gap


def compare_cell(cell, rows_dir, results_rows, counters_rows):
    """Returns (verdict, lines). verdict in PASS / DIFFERS / REFUSED / NOT_COMPARABLE."""
    lines = []
    path = os.path.join(rows_dir, f"{cell}.txt")
    probe = read_probe(path)
    if probe is None:
        return "REFUSED", [f"REFUSED [missing] {cell}: no leg row at {path} -- a CENSORED leg "
                           f"has NO ROW and is reported as censored, never as agreement"]
    refusals = structural_refusals(cell, path, probe)
    if refusals:
        return "REFUSED", refusals
    kv = probe["kv"]
    lines.append(f"{cell:10s} STRUCTURE OK: sentinels, {len(EXPECTED_KEY_BLOCK)}-key block, "
                 f"{len(REQUIRED_BLOCKS)} blocks, {EXPECTED_COUNTER_FIELDS} counters, "
                 f"{EXPECTED_ITERATE_FIELDS} iterate fields")

    if cell in GROUP_W:
        return "NOT_COMPARABLE", lines + [
            f"{cell:10s} NOT COMPARABLE (dnf_wall: the pin's row carries status=null and no "
            f"counters -- there is nothing to reproduce; RULE.md section 4.1)"]

    rk = results_key(cell)
    ck = harness_key(cell, "default")
    if rk not in results_rows:
        return "REFUSED", [f"REFUSED [harness] {cell}: no row {rk} in results.csv"]
    if ck not in counters_rows:
        return "REFUSED", [f"REFUSED [harness] {cell}: no row {ck} in counters-hven-sqp.csv"]
    res, cnt = results_rows[rk], counters_rows[ck]
    src = {"results": res, "counters": cnt}

    diffs, core_diffs, tolerated = [], [], 0
    compared = 0
    # EXAMINED counts every field pair the rule names, present or absent, and is
    # what RULE.md section 4.3 (d)'s totality check reads. COMPARED counts the
    # pairs that were actually both there. The two are different numbers and the
    # fold proofs distinguish them: a differing field is COMPARED, an absent one
    # is not, and only EXAMINED can be checked against the rule's own count.
    examined = 0

    def cmp_bytes(pk, hc, s, is_core):
        nonlocal compared, examined
        examined += 1
        if pk not in kv or hc not in src[s]:
            diffs.append((pk, "ABSENT" if pk not in kv else kv[pk],
                          "ABSENT" if hc not in src[s] else src[s][hc]))
            if is_core:
                core_diffs.append(pk)
            return
        compared += 1
        if kv[pk] != src[s][hc]:
            diffs.append((pk, kv[pk], src[s][hc]))
            if is_core:
                core_diffs.append(pk)

    def cmp_residual(pk, hc, s, is_core):
        nonlocal compared, tolerated, examined
        examined += 1
        if pk not in kv or hc not in src[s]:
            diffs.append((pk, "ABSENT" if pk not in kv else kv[pk],
                          "ABSENT" if hc not in src[s] else src[s][hc]))
            if is_core:
                core_diffs.append(pk)
            return
        compared += 1
        a, b = kv[pk], src[s][hc]
        if a == b:
            return
        ok, gap = near_ulp(a, b)
        if ok:
            tolerated += 1
            lines.append(f"{cell:10s} NEAR-ULP ACCEPTED {pk}: {a} vs {b}  gap={gap:.3e}")
            return
        diffs.append((pk, a, b))
        if is_core:
            core_diffs.append(pk)

    cmp_bytes(CORE_STATUS[0], CORE_STATUS[1], CORE_STATUS[2], True)
    for pk, hc, s in CORE_INT:
        cmp_bytes(pk, hc, s, True)
    cmp_residual(CORE_RESIDUAL[0], CORE_RESIDUAL[1], CORE_RESIDUAL[2], True)
    for pk, hc, s in EXTENDED_INT:
        cmp_bytes(pk, hc, s, False)
    for pk, hc, s in EXTENDED_RESIDUAL:
        cmp_residual(pk, hc, s, False)

    # RULE.md section 4.3 (d): a gate that compared nothing reports REFUSED.
    expected_compared = 1 + len(CORE_INT) + 1 + len(EXTENDED_INT) + len(EXTENDED_RESIDUAL)
    if examined != expected_compared:
        return "REFUSED", lines + [
            f"REFUSED [field-set] {cell}: {examined} field pairs examined against the "
            f"rule's {expected_compared} -- the field set is not total"]

    if kv.get("deadline_fired") == "1":
        lines.append(f"{cell:10s} NOTE: this leg is TRUNCATED (deadline_fired=1); its terminal "
                     f"status is not ranked and identity cannot hold by construction")

    verdict = "PASS" if not diffs else "DIFFERS"
    lines.append(f"{cell:10s} {compared:2d} fields compared, {len(diffs)} differing "
                 f"({len(core_diffs)} of them CORE), {tolerated} near-ulp accepted   {verdict}")
    for k, a, b in diffs:
        lines.append(f"{cell:10s}   DIFF {k}: head={a}  pin={b}")
    return verdict, lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rows", required=True, help="the directory holding <CELL>.txt leg rows")
    ap.add_argument("--cells", default="", help="comma-separated subset; default: all twenty")
    ap.add_argument("--bar", action="store_true",
                    help="apply RULE.md section 3.2's structural bar (exit 3 when EVERY "
                         "comparable cell of a FULL group E+I invocation failed). The bar is "
                         "scoped to that group by the rule, so it is opt-in rather than a "
                         "property of any one-cell call.")
    args = ap.parse_args()

    # M3b Corrections C8: every path handed to this script is checked to exist.
    if not os.path.isdir(args.rows):
        die(f"--rows is not a directory: {args.rows}")
    results_rows = read_csv_rows(RESULTS_CSV, RESULTS_HEADER, "results.csv")
    counters_rows = read_csv_rows(COUNTERS_CSV, COUNTERS_HEADER, "counters-hven-sqp.csv")

    cells = [c.strip() for c in args.cells.split(",") if c.strip()] or ALL_CELLS
    unknown = [c for c in cells if c not in ALL_CELLS]
    if unknown:
        die(f"--cells names a cell outside RULE.md section 1's twenty: {unknown}")

    print("M7 W2 M3c -- IDENTITY GATE (RULE.md section 4): this task's legs at HEAD")
    print("against the harness's COMMITTED rows at the pin aeebdccc, read from their bytes.")
    print(f"  results.csv   {RESULTS_CSV}")
    print(f"  counters.csv  {COUNTERS_CSV}")
    print(f"  leg rows      {args.rows}")
    print(f"  near-ulp gate, residual class only: |a-b| <= {NEAR_ULP_REL:g} * max(1,|a|,|b|)")
    print("")

    tally = {"PASS": 0, "DIFFERS": 0, "REFUSED": 0, "NOT_COMPARABLE": 0}
    for cell in cells:
        verdict, lines = compare_cell(cell, args.rows, results_rows, counters_rows)
        tally[verdict] += 1
        for line in lines:
            print(line if line.startswith(("REFUSED", "        ")) else line)
        print(f"IDENTITY {cell} {verdict}")
        print("")

    comparable = tally["PASS"] + tally["DIFFERS"] + tally["REFUSED"]
    print(f"IDENTITY_PASS={tally['PASS']}")
    print(f"IDENTITY_DIFFERS={tally['DIFFERS']}")
    print(f"IDENTITY_REFUSED={tally['REFUSED']}")
    print(f"IDENTITY_NOT_COMPARABLE={tally['NOT_COMPARABLE']}")
    print(f"IDENTITY_COMPARABLE={comparable}")

    # PRECEDENCE, and it is deliberate: a STRUCTURAL refusal outranks the
    # all-cells bar. A refusal says the file handed to this gate is not a probe
    # row at all, which is a statement about the instrument and must not be
    # reported as "the engine moved on every cell".
    if tally["REFUSED"]:
        print("IDENTITY_VERDICT=STRUCTURAL_REFUSAL")
        return 2
    if args.bar and comparable and tally["PASS"] == 0:
        print("IDENTITY_VERDICT=ALL_COMPARABLE_CELLS_FAILED -- RULE.md section 3.2's "
              "structural bar: this is an instrument failure, not a finding")
        return 3
    if tally["DIFFERS"]:
        print("IDENTITY_VERDICT=DIFFERENCES_ARE_FINDINGS (RULE.md section 4.4)")
        return 1
    print("IDENTITY_VERDICT=ALL_COMPARABLE_CELLS_IDENTICAL")
    return 0


if __name__ == "__main__":
    sys.exit(main())
