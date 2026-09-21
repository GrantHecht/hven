#!/usr/bin/env python3
"""M7 W2 M3c -- RULE.md's TWO AXES, applied to the pass-1 leg rows, and nothing else.

AXIS 1, the TERMINATOR (RULE.md section 5.2): mutually exclusive, TOTAL, in the
precedence the rule fixed BEFORE any leg ran. The first class whose predicate
holds is the cell's class, and the precedence is not re-ordered here.

AXIS 2, the ANTECEDENT flags F1..F7 (RULE.md section 6): INDEPENDENT, each
returned with the numbers that produced it, each `true`, `false` or `N/A`.

  usage
    classify_m3c.py --fold                     the fold proofs; no leg is read
    classify_m3c.py --rows DIR [--out FILE]    classify the committed rows
  exit
    0  every requested cell was classified (CENSORED counts as classified)
    1  a cell REFUSED structurally -- the instrument, not the engine
    2  a setup refusal (a missing input the caller named)

WHAT THIS SCRIPT IS NOT. It is not a cause finder. RULE.md sections 0 and 5.3
bind it: a label says WHICH PREDICATE FIRED, never why the solve died, and
every class name that sounds causal is printed with the fields that produced it
beside it. Where the rule's predicate fires on a cell whose MECHANISM is
something else, that is a DEFECT OF THE RULE and is recorded as one -- RULE.md
is NOT edited, because a pre-registered rule's bytes are the pin (M3b C6/C9).
"""
import argparse
import math
import os
import re
import sys

ROOT = "/home/ghecht/Projects/hven"
BENCH = "/home/ghecht/Projects/hven-bench"
# The three reference lanes of RULE.md section 1.3, read from the bytes of the
# committed CSVs. The environment overrides exist ONLY so the folds can hand a
# deliberately malformed file in and watch the refusal; every real invocation
# leaves them unset and the paths below are exactly the committed ones.
REPIN = os.path.join(BENCH, "runs/2026-09-m7-w2-h-repin/run-A")
EXTERNAL = os.path.join(BENCH, "runs/2026-09-m7-h4-external/run-A/cutest")
HVEN_RESULTS = os.environ.get("M3C_HVEN_RESULTS", os.path.join(REPIN, "results.csv"))
EXT_RESULTS = os.environ.get("M3C_EXT_RESULTS", os.path.join(EXTERNAL, "results.csv"))
SNOPT_COUNTERS = os.environ.get("M3C_SNOPT_COUNTERS",
                                os.path.join(EXTERNAL, "counters-snopt.csv"))

GROUP_E = ["ACOPP118", "ACOPP57", "ACOPR57", "BRITGAS", "DALLASM", "NET3",
           "STEENBRB", "STEENBRC", "STEENBRD", "STEENBRE", "STEENBRG"]
GROUP_I = ["YORKNET"]
GROUP_W = ["ACOPR118", "BIGBANK", "MSS2", "MSS3", "READING8", "SAWPATH",
           "TAX213322", "TAX53322"]
ALL_CELLS = GROUP_E + GROUP_I + GROUP_W
GROUP_OF = dict([(c, "E") for c in GROUP_E] + [(c, "I") for c in GROUP_I] +
                [(c, "W") for c in GROUP_W])

REQUIRED_BLOCKS = ["LADDER", "OPTIONS", "COUNTERS", "HISTORY", "LEDGER"]
EXPECTED_COUNTER_FIELDS = 49
EXPECTED_ITERATE_FIELDS = 29

# RULE.md section 1.3's two lane filters, verbatim.
IPM_COUNTED = {"optimal", "acceptable"}
IPOPT_COUNTED = {"Solve_Succeeded", "Solved_To_Acceptable_Level"}
SNOPT_COUNTED = {"Optimal"}
ZERO_ARM = 1.0e-6          # RULE.md section 1.3's |f| <= 1e-6 arm
SIG_DIGITS = 6             # RULE.md section 1.3's "agree to 6 significant digits"
TR_SHRINK_FACTOR = 0.5     # kTrShrinkFactor, RULE.md section 6 F3's derivation
F4_JUDGED_BAR = 3          # RULE.md section 6 F4: the engine's own budget constant
F2_H_BAND = 0.99           # RULE.md section 6 F2's band around the exact null

RESTORATION_FAILED = {"infeasible", "max_iter", "numerical_error", "interrupted"}


# An identifier followed by `=` at the start of the line, and nothing looser.
KV_LINE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*=")


class Refusal(Exception):
    pass


# --------------------------------------------------------------------------
# the row reader
# --------------------------------------------------------------------------
def read_row(path):
    """Sections a probe row. Refuses anything RULE.md section 4.3 refuses,
    because a classification of a malformed row is worse than no row."""
    if not os.path.exists(path):
        return None                      # CENSORED: RULE.md section 5.2 class 1
    with open(path) as fh:
        lines = [ln.rstrip("\n") for ln in fh if ln.strip()]
    if not lines:
        raise Refusal("the file is empty")
    if lines[0] != "PROBE_BEGIN":
        raise Refusal(f"first non-empty line is {lines[0]!r}, not PROBE_BEGIN")
    if lines[-1] != "PROBE_END":
        raise Refusal("no PROBE_END -- a probe killed mid-write must never read as a row")
    if lines.count("PROBE_BEGIN") != 1 or lines.count("PROBE_END") != 1:
        raise Refusal("PROBE_BEGIN/PROBE_END are not exactly one each")

    for b in REQUIRED_BLOCKS:
        if lines.count(f"{b}_BEGIN") != 1 or lines.count(f"{b}_END") != 1:
            raise Refusal(f"block {b} does not appear exactly once")

    kv, csvs, block = {}, {}, None
    for ln in lines:
        if ln.endswith("_BEGIN") and " " not in ln:
            block = ln[:-6]
            csvs.setdefault(block, [])
            continue
        if ln.endswith("_END") and " " not in ln:
            block = None
            continue
        # A line is a KEY=VALUE line iff it opens with an IDENTIFIER followed by
        # `=`. A bare `"=" in ln` test is WRONG and was a real defect here: the
        # probe prints `qp_status=n/a(qp_solved=0)` INSIDE a history row, so
        # every row for a major whose QP never ran was being routed to `kv` and
        # silently dropped from the table -- including the whole history of the
        # three non-finite-start cells. The row-count check below is the
        # structural guard that must catch any repeat.
        if KV_LINE.match(ln):
            k, v = ln.split("=", 1)
            kv[k] = v
        elif block is not None:
            csvs[block].append(ln)

    if kv.get("c_field_count") != str(EXPECTED_COUNTER_FIELDS):
        raise Refusal(f"the counters block declares c_field_count="
                      f"{kv.get('c_field_count')!r}, not {EXPECTED_COUNTER_FIELDS}")
    if kv.get("h_field_count") != str(EXPECTED_ITERATE_FIELDS):
        raise Refusal(f"the history block declares h_field_count="
                      f"{kv.get('h_field_count')!r}, not {EXPECTED_ITERATE_FIELDS}")

    # The history table's header is the 29 public `SqpIterate` fields PLUS the
    # row's absolute `index`, which the probe prepends -- 30 columns. The 29 is
    # pinned through `h_field_count` above; the width is pinned here.
    hist = parse_csv_block(csvs.get("HISTORY", []), EXPECTED_ITERATE_FIELDS + 1, "HISTORY")
    # NON-VACUITY, and the guard the `=`-in-a-field defect above earned: the
    # probe prints how many rows it wrote, so a parser that dropped one is
    # caught here rather than classifying a truncated history.
    printed = kv.get("h_rows_printed")
    if printed is not None and int(printed) != len(hist):
        raise Refusal(f"the history block says h_rows_printed={printed} but "
                      f"{len(hist)} row(s) parsed")
    # The per-record table lives in the NESTED `LEDGER_RECORDS` sub-block, not
    # in `LEDGER` itself, which also carries `LEDGER_FIRST` and `LEDGER_AGG`.
    # A cell whose ledger printed NO record at all (the three non-finite-start
    # cells) has the sub-block's header and no rows; a cell that has no
    # sub-block at all is a malformed row and is refused.
    if "LEDGER_RECORDS" not in csvs:
        raise Refusal("the LEDGER block carries no LEDGER_RECORDS sub-block")
    ledger = parse_csv_block(csvs["LEDGER_RECORDS"], None, "LEDGER_RECORDS")
    lprinted = kv.get("ledger_records_printed")
    if lprinted is not None and int(lprinted) != len(ledger):
        raise Refusal(f"the ledger says ledger_records_printed={lprinted} but "
                      f"{len(ledger)} record(s) parsed")
    return {"kv": kv, "history": hist, "ledger": ledger}


def parse_csv_block(rows, width, label):
    """The CSV table inside a block: the first line starting `index,` is the
    header, every line after it that starts with a digit is a row."""
    header, out = None, []
    for ln in rows:
        if header is None:
            if ln.startswith("index,"):
                header = ln.split(",")
                if width is not None and len(header) != width:
                    raise Refusal(f"{label}: header has {len(header)} columns, not {width}")
            continue
        if not ln[0].isdigit():
            continue
        f = ln.split(",")
        if len(f) != len(header):
            raise Refusal(f"{label}: a row has {len(f)} fields against the header's {len(header)}")
        out.append(dict(zip(header, f)))
    if header is None:
        raise Refusal(f"{label}: no `index,` header row")
    return out


def num(kv, key):
    v = kv.get(key)
    if v is None:
        raise Refusal(f"missing key {key}")
    try:
        return float(v)
    except ValueError:
        raise Refusal(f"key {key} is not numeric: {v!r}")


def integer(kv, key):
    v = num(kv, key)
    if v != int(v):
        raise Refusal(f"key {key} is not an integer: {kv[key]!r}")
    return int(v)


def is_nan(s):
    return s.strip().lower().lstrip("+-") == "nan"


# --------------------------------------------------------------------------
# AXIS 1 -- the terminator, RULE.md section 5.2, in precedence order
# --------------------------------------------------------------------------
def terminator(row):
    """Returns (class, why, evidence-dict). TOTAL: `OTHER` is the last arm and
    is a finding about the RULE, not about the cell (RULE.md section 5.2 #9)."""
    kv = row["kv"]
    ev = {}
    status = kv.get("status")
    if status is None:
        raise Refusal("missing key status")
    ev["status"] = status
    ev["deadline_fired"] = integer(kv, "deadline_fired")
    ev["major_iters"] = integer(kv, "major_iters")
    ev["factorizations"] = integer(kv, "factorizations")
    ev["f"] = kv.get("f", "")
    ev["qp_failures_in_a_row_peak"] = integer(kv, "qp_failures_in_a_row_peak")
    ev["qp_failure_budget"] = integer(kv, "opt_qp_failure_budget")
    ev["restoration_entries"] = integer(kv, "restoration_entries")
    ev["restoration_sub_status"] = kv.get("restoration_sub_status", "")
    ev["max_iter_option"] = integer(kv, "opt_max_iter")

    last = row["history"][-1] if row["history"] else None
    ev["last_qp_solved"] = int(last["qp_solved"]) if last else None
    ev["last_qp_status"] = last["qp_status"] if last else None
    ev["last_elastic_applied"] = int(last["elastic_applied"]) if last else None
    ev["last_verdict"] = last["verdict"] if last else None
    ev["last_tr_radius"] = float(last["tr_radius"]) if last else None

    # 2. WALL_TRUNCATED -- the solve did not choose its exit; section 3.1's
    #    deadline did. Takes precedence over every solver-chosen class because
    #    none of those exits happened. (Class 1, CENSORED, is an ABSENCE and is
    #    decided by the caller: there is no row to classify.)
    if ev["deadline_fired"] == 1:
        return "WALL_TRUNCATED", "deadline_fired == 1", ev

    # 3. START_POINT_NONFINITE -- OBSERVED predicate, INFERRED exit path.
    if (status == "numerical_error" and ev["major_iters"] == 0
            and ev["factorizations"] == 0 and is_nan(ev["f"])):
        return "START_POINT_NONFINITE", \
            "numerical_error with 0 majors, 0 factorizations and a nan objective", ev

    qp_fail_row = (last is not None and ev["last_qp_solved"] == 1
                   and ev["last_qp_status"] in ("max_iter", "numerical_error"))

    # 4. QP_CAP_BUDGET
    if (status == "numerical_error" and qp_fail_row
            and ev["last_qp_status"] == "max_iter"
            and ev["qp_failures_in_a_row_peak"] == ev["qp_failure_budget"]):
        return "QP_CAP_BUDGET", \
            "numerical_error; the last history row's QP ended max_iter; " \
            "qp_failures_in_a_row_peak == qp_failure_budget", ev

    # 5. QP_NUMERICAL
    if status == "numerical_error" and qp_fail_row and ev["last_qp_status"] == "numerical_error":
        return "QP_NUMERICAL", \
            "numerical_error; the last history row's QP ended numerical_error", ev

    # 6. QP_UNRETRYABLE -- INFERRED, and labelled as inferred.
    if (status == "numerical_error" and qp_fail_row
            and ev["qp_failures_in_a_row_peak"] < ev["qp_failure_budget"]):
        return "QP_UNRETRYABLE", \
            "numerical_error; the last history row's QP failed; the budget still " \
            "had room, so the retryability test is INFERRED to have refused", ev

    # 7. RESTORATION_EXIT -- sub-labelled by the sub-solve's OWN terminal status.
    #
    # AN AMBIGUITY IN THE PRE-REGISTERED RULE, DECLARED RATHER THAN RESOLVED
    # SILENTLY. RULE.md section 5.2 #7's conjunct reads: "the last history row
    # is not a QP-failure row (i.e. classes 4-6 did not fire)". Those are TWO
    # different predicates, and they differ on exactly one shape: a cell whose
    # terminal status is NOT `numerical_error` (so classes 4-6 cannot fire,
    # whatever the row says) while its last history row DOES carry a failing
    # `qp_status`. On the shipped path that shape is produced by the elastic
    # tier's own restoration exit, which stamps the ELASTIC LADDER's status onto
    # the row and sets the verdict to Restore (`src/drivers/sqp_solver.cpp`, the
    # `if (!report.usable)` arm) -- the very overwrite RULE.md section 5.1's
    # second semantic note warns about.
    #
    # WHICH READING IS RETURNED, and why: the parenthetical is the rule's OWN
    # definitional gloss of its own phrase, in the same clause, and RULE.md
    # section 5.2 #7 names YORKNET as the cell it expects here. Applying the
    # rule AS WRITTEN means applying the definition the rule supplies. The
    # LITERAL reading is computed too and printed beside it whenever the two
    # disagree, so a reader never has to take this script's word for which arm
    # it took. RULE.md is NOT edited (M3b C6/C9: a pre-registered rule's bytes
    # are the pin; a defect in it is recorded, never repaired after the fact).
    if ev["restoration_entries"] >= 1:
        cls = f"RESTORATION_EXIT({ev['restoration_sub_status']})"
        if not qp_fail_row:
            return cls, ("restoration ran, the last history row is not a QP-failure "
                         "row, and classes 4-6 did not fire -- BOTH readings of "
                         "RULE.md section 5.2 #7 agree here"), ev
        ev["RULE_AMBIGUITY"] = (
            "RULE.md section 5.2 #7's two readings DISAGREE on this cell: under the "
            "rule's own gloss (`classes 4-6 did not fire`) it is "
            f"{cls}; under the literal phrase (`the last history row is not a "
            "QP-failure row`) classes 4-7 are all silent and the cell falls to "
            "OTHER. The row's failing qp_status is the ELASTIC LADDER's, stamped on "
            "by the restoration exit -- RULE.md section 5.1's own warning. The "
            "GLOSS reading is returned")
        return cls, ("restoration ran and classes 4-6 did not fire (the terminal "
                     "status is not numerical_error); see RULE_AMBIGUITY"), ev

    # 8. MAJOR_BUDGET
    if status == "max_iter" and ev["major_iters"] == ev["max_iter_option"]:
        return "MAJOR_BUDGET", "status max_iter at OPTIONS:max_iter majors", ev

    # 9. OTHER -- a finding about THIS RULE.
    return "OTHER", "no predicate of RULE.md section 5.2 held; this is a finding " \
                    "about the rule, not about the cell", ev


# --------------------------------------------------------------------------
# AXIS 2 -- the antecedent flags, RULE.md section 6. INDEPENDENT.
# --------------------------------------------------------------------------
def flags(row):
    kv = row["kv"]
    out = {}
    rent = integer(kv, "restoration_entries")
    sub = kv.get("restoration_sub_status", "")
    feas_tol = num(kv, "opt_feas_tol")

    if rent >= 1:
        h_in, h_out = num(kv, "restoration_h_in"), num(kv, "restoration_h_out")
        f_in, f_out = num(kv, "restoration_f_in"), num(kv, "restoration_f_out")
        # F1's REPORTED ratio, printed whether or not the flag fires.
        ratio = (f_out - f_in) / max(1.0, abs(f_in))
        f1 = (sub == "optimal" and h_out <= feas_tol
              and (f_out - f_in) >= max(1.0, abs(f_in)))
        out["F1_RESTORATION_EXCURSION"] = (
            f1, f"sub_status={sub} h_in={h_in!r} h_out={h_out!r} feas_tol={feas_tol!r} "
                f"f_in={f_in!r} f_out={f_out!r} (f_out-f_in)/max(1,|f_in|)={ratio!r}")
        f2 = sub in RESTORATION_FAILED and h_out >= F2_H_BAND * h_in
        out["F2_RESTORATION_STUCK"] = (
            f2, f"sub_status={sub} h_in={h_in!r} h_out={h_out!r} "
                f"h_out/h_in={'n/a (h_in == 0)' if h_in == 0 else repr(h_out / h_in)}")
    else:
        out["F1_RESTORATION_EXCURSION"] = (None, "N/A: restoration_entries == 0")
        out["F2_RESTORATION_STUCK"] = (None, "N/A: restoration_entries == 0")

    tr_min = num(kv, "opt_tr_min")
    last = row["history"][-1] if row["history"] else None
    if last is None:
        out["F3_TR_AT_FLOOR"] = (None, "N/A: no history row")
    else:
        radius = float(last["tr_radius"])
        out["F3_TR_AT_FLOOR"] = (
            radius < tr_min / TR_SHRINK_FACTOR,
            f"last row's tr_radius={radius!r} against tr_min/{TR_SHRINK_FACTOR}="
            f"{tr_min / TR_SHRINK_FACTOR!r}")

    accepted = integer(kv, "steps_accepted")
    rejected = integer(kv, "rejected_steps")
    routed = integer(kv, "qp_failures_routed")
    judged = rejected - routed            # RULE.md section 5.1: EXACT, not an estimate
    declared = kv.get("c_derived_judged_rejections")
    if declared is not None and int(declared) != judged:
        raise Refusal(f"the probe's c_derived_judged_rejections={declared} disagrees with "
                      f"rejected_steps - qp_failures_routed = {judged}")
    out["F4_REJECTION_STALL"] = (
        accepted == 0 and judged >= F4_JUDGED_BAR,
        f"steps_accepted={accepted} judged_rejections={judged} "
        f"(= rejected_steps {rejected} - qp_failures_routed {routed})")

    mx = integer(kv, "qp_maxiter_exits")
    out["F5_CAP_CHURN"] = (
        mx >= 1,
        f"qp_maxiter_exits={mx} qp_cap_retries={integer(kv, 'qp_cap_retries')} "
        f"qp_minor_iters={integer(kv, 'qp_minor_iters')}")

    ea = integer(kv, "elastic_activations")
    out["F6_ELASTIC_ENGAGED"] = (
        ea >= 1,
        f"elastic_activations={ea} elastic_escalations={integer(kv, 'elastic_escalations')} "
        f"elastic_rho0_ceiling_hits={integer(kv, 'elastic_rho0_ceiling_hits')} "
        f"elastic_floor_retries={integer(kv, 'elastic_floor_retries')}")

    # F7 -- from the LEDGER, whose scope caveat is RULE.md section 2.2's: the
    # MAIN WALK ENGINE only. Never a verdict.
    recs = row["ledger"]
    failing = [(i, r) for i, r in enumerate(recs)
               if r["status"] in ("max_iter", "numerical_error")]
    warm_fail = sum(1 for _, r in failing if r["warm"] == "1")
    cold_fail = len(failing) - warm_fail
    if not recs:
        out["F7_WARM_FAILURES"] = (None, "N/A: the ledger holds no record")
    else:
        idx = [r["index"] for _, r in failing]
        out["F7_WARM_FAILURES"] = (
            len(failing) >= 1,
            f"failing ledger records: warm={warm_fail} cold={cold_fail} of "
            f"{len(recs)} records; first={idx[0] if idx else 'none'} "
            f"last={idx[-1] if idx else 'none'} (MAIN WALK ENGINE ONLY)")
    return out


# --------------------------------------------------------------------------
# RULE.md section 7 -- the descriptive read-outs
# --------------------------------------------------------------------------
def read_lane_csv(path, label):
    if not os.path.exists(path):
        raise Refusal(f"{label}: no such file: {path}")
    header, rows = None, {}
    with open(path) as fh:
        for ln in fh:
            ln = ln.rstrip("\n")
            if not ln or ln.startswith("#"):
                continue
            f = ln.split(",")
            if header is None:
                header = f
                continue
            if len(f) != len(header):
                raise Refusal(f"{label}: a row has {len(f)} fields against the header's "
                              f"{len(header)}")
            rows[f[0]] = dict(zip(header, f))
    if header is None:
        raise Refusal(f"{label}: no header row")
    return rows


def sig6(x):
    """The 6-significant-digit rendering RULE.md section 1.3's agreement test is
    stated over. Returns None for a non-finite value, which never agrees."""
    if x is None or not math.isfinite(x):
        return None
    return f"{x:.{SIG_DIGITS - 1}e}"


def consensus(cell, lanes):
    """RULE.md section 1.3, applied EXACTLY: a consensus exists iff at least two
    COUNTED lanes agree to 6 significant digits, or at least two COUNTED lanes
    both report |f| <= 1e-6 (consensus 0). Otherwise ABSENT."""
    counted = [(name, f) for name, f in lanes if f is not None]
    if len(counted) < 2:
        return None, f"ABSENT (no consensus): {len(counted)} counted lane(s)"
    zeros = [n for n, f in counted if abs(f) <= ZERO_ARM]
    if len(zeros) >= 2:
        return 0.0, f"consensus 0 (the |f| <= {ZERO_ARM} arm): lanes {', '.join(zeros)}"
    buckets = {}
    for n, f in counted:
        buckets.setdefault(sig6(f), []).append(n)
    for key, names in buckets.items():
        if key is not None and len(names) >= 2:
            return float(key), (f"consensus {key} (6 significant digits): "
                                f"lanes {', '.join(names)}")
    return None, ("ABSENT (no consensus): counted lanes "
                  + ", ".join(f"{n}={sig6(f)}" for n, f in counted)
                  + " agree nowhere to 6 significant digits")


def readouts(cell, row, hven_rows, ext_rows, snopt_counters):
    kv = row["kv"]
    out = {}
    ipm = hven_rows.get(f"cutest/{cell}/hven-ipm/default")
    ipopt = ext_rows.get(f"cutest/{cell}/ipopt/default")
    snopt = ext_rows.get(f"cutest/{cell}/snopt/default")

    def counted(rowdict, allowed):
        if rowdict is None or rowdict["status"] not in allowed:
            return None
        try:
            return float(rowdict["objective"])
        except ValueError:
            return None

    lanes = [("hven-ipm", counted(ipm, IPM_COUNTED)),
             ("ipopt", counted(ipopt, IPOPT_COUNTED)),
             ("snopt", counted(snopt, SNOPT_COUNTED))]
    star, why = consensus(cell, lanes)
    out["consensus"] = why
    out["lane_objectives"] = "; ".join(
        f"{n}={'NOT COUNTED' if f is None else repr(f)}" for n, f in lanes)

    f_here = kv.get("f", "")
    if is_nan(f_here):
        out["closeness_objective"] = "n/a (f is nan)"
    elif star is None:
        out["closeness_objective"] = "ABSENT (no consensus)"
    else:
        f_val = float(f_here)
        out["closeness_objective"] = (
            f"f={f_val!r} f*={star!r} |f-f*|={abs(f_val - star)!r} "
            f"rel={abs(f_val - star) / max(1.0, abs(star))!r}")

    fe, fi = num(kv, "feasibility_e"), num(kv, "feasibility_i")
    feas_tol = num(kv, "opt_feas_tol")
    h = max(fe, fi)
    out["closeness_feasibility"] = (
        f"max(feasibility_e, feasibility_i)={h!r} feas_tol={feas_tol!r} "
        f"ratio={h / feas_tol!r}")

    sc = snopt_counters.get(f"cutest/{cell}/snopt/default")
    if snopt is None or snopt["status"] not in SNOPT_COUNTED or sc is None:
        out["cost_vs_snopt"] = "n/a (SNOPT did not solve this cell, or has no counter row)"
    else:
        out["cost_vs_snopt"] = (
            f"hven majors={integer(kv, 'major_iters')} minors={integer(kv, 'qp_minor_iters')}"
            f" | SNOPT majors={snopt['iterations']} minors={sc['snopt_minor_iterations']}"
            f" | hven factorizations={integer(kv, 'factorizations')} has NO SNOPT counterpart"
            f" (SNOPT publishes no factorization count)")

    out["qp_status_sequence"] = " ".join(
        f"{r['index']}:{r['status']}{'(warm)' if r['warm'] == '1' else '(cold)'}"
        f"[m={r['minor_iters']},deg={r['degenerate_steps']},dt={r['drop_ties']},"
        f"rt={r['ratio_ties']},+{r['ws_adds']},-{r['ws_drops']}]"
        for r in row["ledger"]) or "(the ledger holds no record)"
    out["ledger_print_cap"] = (
        f"cap={kv.get('ledger_records_print_cap')} printed={kv.get('ledger_records_printed')} "
        f"truncated={kv.get('ledger_records_print_truncated')}")
    return out


def chain_of_death(cell, row, term, ev):
    """The chain of death in ONE LINE, built from the last-K history rows. Every
    number in it is repeated from the row; nothing is inferred here."""
    hist = row["history"]
    if not hist:
        return "(no history row)"
    parts = []
    for r in hist[-6:]:
        qp = (r["qp_status"] if r["qp_solved"] == "1" else "no-qp")
        parts.append(
            f"#{r['index']}"
            f" tr={fmt_short(r['tr_radius'])}"
            f" f={fmt_short(r['f'])}"
            f" h={fmt_short(r['violation_l1'])}"
            f" kkt={fmt_short(r['kkt_residual'])}"
            f" qp={qp}/{r['qp_minor_iters']}m"
            f"{'/elastic' if r['elastic_applied'] == '1' else ''}"
            f" -> {r['verdict']}")
    head = f"{len(hist)} major row(s), last {min(6, len(hist))} shown: "
    return head + "  ".join(parts)


def fmt_short(s):
    try:
        v = float(s)
    except ValueError:
        return s
    if not math.isfinite(v):
        return s
    if v == 0:
        return "0"
    return f"{v:.4g}"


# --------------------------------------------------------------------------
# the driver
# --------------------------------------------------------------------------
def classify_dir(rows_dir, cells, stream):
    hven_rows = read_lane_csv(HVEN_RESULTS, "hven results.csv")
    ext_rows = read_lane_csv(EXT_RESULTS, "external results.csv")
    snopt_counters = read_lane_csv(SNOPT_COUNTERS, "counters-snopt.csv")
    refused = 0
    for cell in cells:
        path = os.path.join(rows_dir, f"{cell}.txt")
        print(f"CELL {cell}  group={GROUP_OF.get(cell, '?')}", file=stream)
        try:
            row = read_row(path)
        except Refusal as e:
            print(f"  REFUSED [structure] {e}", file=stream)
            print(f"  TERMINATOR REFUSED", file=stream)
            refused += 1
            print("", file=stream)
            continue
        if row is None:
            # RULE.md section 5.2 class 1. NO ROW, and no field is ever filled.
            print("  TERMINATOR CENSORED", file=stream)
            print("  WHY the outer `timeout 900` killed the leg: there is NO ROW, and "
                  "RULE.md section 3.1 forbids filling one later or reconstructing it",
                  file=stream)
            print("  FLAGS (none -- there are no counters to read)", file=stream)
            print("", file=stream)
            continue
        try:
            term, why, ev = terminator(row)
            fl = flags(row)
            ro = readouts(cell, row, hven_rows, ext_rows, snopt_counters)
        except Refusal as e:
            print(f"  REFUSED [fields] {e}", file=stream)
            print(f"  TERMINATOR REFUSED", file=stream)
            refused += 1
            print("", file=stream)
            continue
        print(f"  TERMINATOR {term}", file=stream)
        print(f"  WHY {why}", file=stream)
        print("  EVIDENCE " + " ".join(f"{k}={v}" for k, v in ev.items()), file=stream)
        for name in sorted(fl):
            val, detail = fl[name]
            label = "N/A" if val is None else ("true" if val else "false")
            print(f"  FLAG {name} = {label}   [{detail}]", file=stream)
        print(f"  CHAIN {chain_of_death(cell, row, term, ev)}", file=stream)
        for k in ("consensus", "lane_objectives", "closeness_objective",
                  "closeness_feasibility", "cost_vs_snopt", "qp_status_sequence",
                  "ledger_print_cap"):
            print(f"  {k.upper()} {ro[k]}", file=stream)
        print("", file=stream)
    return 1 if refused else 0


# --------------------------------------------------------------------------
# THE FOLD PROOFS -- every class, every flag, every precedence conflict, and
# every refusal, on CONSTRUCTED rows, before this script reads a leg.
# A classifier that has never been seen to return a class is not a classifier.
# --------------------------------------------------------------------------
COUNTER_NAMES = [
    "major_iters", "qp_minor_iters", "factorizations", "steps_accepted",
    "rejected_steps", "soc_steps", "soc_applied", "soc_qp_infeasible",
    "soc_rejected", "elastic_activations", "elastic_escalations",
    "restoration_iters", "elastic_from_ipqp_escape", "ipqp_suspicion_disproved",
    "ipqp_fallback_rung_b", "elastic_rho0_ceiling_hits", "elastic_floor_retries",
    "eqp_refine_steps", "border_refine_steps", "verdict_refine_steps",
    "suspect_escalations", "symbolic_analyses", "full_step_majors",
    "watchdog_restores", "evals_full", "evals_values", "probe_budget_stops",
    "crash_seeded_rows", "crash_seeded_bounds", "n_seeded", "seeded_clamped",
    "ip_activity_inferred", "active_set_delta_total", "active_set_delta_peak",
    "weak_active_peak", "near_active_peak", "polish_ignored",
    "start_point_clamps", "qp_maxiter_exits", "qp_numerical_exits",
    "qp_failures_routed", "qp_failures_in_a_row_peak", "qp_cap_retries",
    "restoration_entries", "restoration_h_in", "restoration_h_out",
    "restoration_f_in", "restoration_f_out", "restoration_sub_status",
]
assert len(COUNTER_NAMES) == EXPECTED_COUNTER_FIELDS

ITERATE_COLS = ["index", "trial", "f", "stationarity", "feasibility",
                "complementarity", "kkt_residual", "violation_l1", "tr_radius",
                "mu", "step_norm", "qp_solved", "ipqp_least_infeasible_primal",
                "ipqp_farkas_corroborated", "qp_status", "qp_minor_iters",
                "qp_factorizations", "tr_binding", "verdict", "soc_applied",
                "elastic_applied", "elastic_rho0_ceiling_hit",
                "restoration_seed_used", "watchdog_restored", "active_set_delta",
                "weak_active_rows", "near_active_rows", "active_rows",
                "active_lower_sides", "active_upper_sides"]
assert len(ITERATE_COLS) == EXPECTED_ITERATE_FIELDS + 1   # + the prepended `index`

LEDGER_COLS = ["index", "label", "warm", "status", "factorizations",
               "schur_updates", "minor_iters", "eqp_refine_steps",
               "border_refine_steps", "verdict_refine_steps",
               "suspect_escalations", "k0_reused", "symbolic_analyses",
               "ws_adds", "ws_drops", "shift_adds", "degenerate_steps",
               "degenerate_run_max", "ws_adds_bound", "ws_drops_bound",
               "distinct_ineq_added", "distinct_bound_added", "drop_ties",
               "ratio_ties", "ws_adds_minus_drops", "ws_net_change",
               "degenerate_fraction"]

TOP_DEFAULTS = {
    "problem": "SYNTH", "deadline_fired": 0, "deadline_seconds": 600,
    "status": "numerical_error", "f": "1.0", "stationarity": "1.0",
    "feasibility_e": "1e-3", "feasibility_i": "0", "complementarity": "0",
    "major_iters": 5, "qp_minor_iters": 100, "factorizations": 100,
    "steps_accepted": 1, "rejected_steps": 3, "elastic_activations": 0,
    "elastic_escalations": 0, "elastic_rho0_ceiling_hits": 0,
    "elastic_floor_retries": 0, "qp_maxiter_exits": 0, "qp_numerical_exits": 0,
    "qp_failures_routed": 3, "qp_failures_in_a_row_peak": 3, "qp_cap_retries": 0,
    "restoration_entries": 0, "restoration_sub_status": "not_run",
    "restoration_h_in": 0, "restoration_h_out": 0,
    "restoration_f_in": 0, "restoration_f_out": 0,
    "ledger_records_print_cap": 5000, "ledger_records_print_truncated": 0,
}
OPT_DEFAULTS = {
    "opt_tr_init": 1, "opt_tr_min": "1e-10", "opt_tr_max": "1e10",
    "opt_feas_tol": "1e-6", "opt_kkt_tol": "1e-6", "opt_max_iter": 100,
    "opt_qp_failure_budget": 3, "opt_qp_cap_retry": 0, "opt_qp_max_iter": 0,
    "opt_restoration_proximity": 0, "opt_qp_mode_is_walk": 1,
    "opt_project_start_point": 0,
}
HIST_DEFAULTS = dict.fromkeys(ITERATE_COLS, "0")
HIST_DEFAULTS.update({"tr_radius": "1", "qp_status": "optimal", "verdict": "AcceptF",
                      "f": "1.0", "violation_l1": "0", "kkt_residual": "1.0"})
LED_DEFAULTS = dict.fromkeys(LEDGER_COLS, "0")
LED_DEFAULTS.update({"label": "f_qp_0", "status": "optimal"})


def synth(top=None, opts=None, hist=None, ledger=None, omit=(), corrupt=None):
    """Builds a WELL-FORMED probe row, then applies exactly the corruption asked
    for. `hist` and `ledger` are lists of dicts overriding the defaults."""
    t = dict(TOP_DEFAULTS); t.update(top or {})
    o = dict(OPT_DEFAULTS); o.update(opts or {})
    hrows = hist if hist is not None else [dict(HIST_DEFAULTS)]
    lrows = ledger if ledger is not None else []
    t["history_rows"] = len(hrows)

    L = ["PROBE_BEGIN"]
    for k, v in t.items():
        if k not in omit:
            L.append(f"{k}={v}")
    L += ["LADDER_BEGIN", "depth,event_iteration,f_event", "LADDER_END", "OPTIONS_BEGIN"]
    for k, v in o.items():
        if k not in omit:
            L.append(f"{k}={v}")
    L += ["OPTIONS_END", "COUNTERS_BEGIN",
          f"c_field_count={corrupt if corrupt == 'counter_count' else EXPECTED_COUNTER_FIELDS}"]
    if corrupt == "counter_count":
        L[-1] = "c_field_count=48"
    for n in COUNTER_NAMES:
        L.append(f"c_{n}={t.get(n, 0)}")
    L.append(f"c_derived_judged_rejections="
             f"{int(t['rejected_steps']) - int(t['qp_failures_routed'])}"
             if corrupt != "judged" else "c_derived_judged_rejections=99")
    L += ["COUNTERS_END", "HISTORY_BEGIN", f"h_field_count={EXPECTED_ITERATE_FIELDS}",
          f"h_rows_total={len(hrows)}",
          f"h_rows_printed={99 if corrupt == 'hist_count' else len(hrows)}"]
    cols = ITERATE_COLS if corrupt != "hist_width" else ITERATE_COLS[:-1]
    L.append(",".join(cols))
    for i, r in enumerate(hrows):
        rr = dict(HIST_DEFAULTS); rr.update(r); rr["index"] = str(i)
        L.append(",".join(rr[c] for c in cols))
    L += ["HISTORY_END", "LEDGER_BEGIN", f"ledger_qp_records={len(lrows)}",
          f"ledger_records_printed={99 if corrupt == 'ledger_count' else len(lrows)}",
          "LEDGER_RECORDS_BEGIN", ",".join(LEDGER_COLS)]
    for i, r in enumerate(lrows):
        rr = dict(LED_DEFAULTS); rr.update(r); rr["index"] = str(i)
        L.append(",".join(str(rr[c]) for c in LEDGER_COLS))
    L += ["LEDGER_RECORDS_END", "LEDGER_END", "PROBE_END"]
    if corrupt == "no_ledger_records":
        L = [x for x in L if not x.startswith("LEDGER_RECORDS_")]
    if corrupt == "no_end":
        L = L[:-1]
    if corrupt == "no_block":
        L = [x for x in L if not x.startswith("OPTIONS_")]
    return "\n".join(L) + "\n"


def fold(stream):
    """Every arm, on constructed inputs. Fails loudly, and the count is printed."""
    import tempfile
    passed = failed = 0

    def check(name, text, want_class=None, want_refusal=False, want_flags=None):
        nonlocal passed, failed
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as fh:
            fh.write(text)
            p = fh.name
        try:
            try:
                row = read_row(p)
                if row is None:
                    got, kind = "CENSORED", "class"
                else:
                    got, _, _ = terminator(row)
                    kind = "class"
                    # ALWAYS evaluated, not only when a flag is asserted: axis 2
                    # carries its own refusals (the judged-rejection
                    # cross-check), and a fold that skipped it could never see
                    # one fire.
                    fl = flags(row)
                    if want_flags is not None:
                        for fname, fwant in want_flags.items():
                            gotf = fl[fname][0]
                            if gotf != fwant:
                                raise AssertionError(
                                    f"{fname}: got {gotf!r}, wanted {fwant!r} "
                                    f"[{fl[fname][1]}]")
            except Refusal as e:
                got, kind = f"REFUSED({e})", "refusal"
            if want_refusal:
                ok = (kind == "refusal")
                exp = "a REFUSAL"
            else:
                ok = (kind == "class" and got == want_class)
                exp = want_class
            if ok:
                passed += 1
                print(f"  FOLD PASS  {name:<52} -> {got}", file=stream)
            else:
                failed += 1
                print(f"  FOLD FAIL  {name:<52} -> {got}   (wanted {exp})", file=stream)
        except AssertionError as e:
            failed += 1
            print(f"  FOLD FAIL  {name:<52} -> {e}", file=stream)
        finally:
            os.unlink(p)

    def no_file(name):
        nonlocal passed, failed
        p = os.path.join(tempfile.gettempdir(), "m3c-fold-does-not-exist.txt")
        if os.path.exists(p):
            os.unlink(p)
        got = read_row(p)
        if got is None:
            passed += 1
            print(f"  FOLD PASS  {name:<52} -> CENSORED (no row)", file=stream)
        else:
            failed += 1
            print(f"  FOLD FAIL  {name:<52} -> a row was invented", file=stream)

    print("PART 1 -- every TERMINATOR class is REACHABLE", file=stream)
    no_file("class 1 CENSORED: the leg file does not exist")

    check("class 2 WALL_TRUNCATED: deadline_fired == 1",
          synth(top={"deadline_fired": 1, "status": "interrupted"}),
          "WALL_TRUNCATED")
    check("class 3 START_POINT_NONFINITE: 0 majors, 0 facts, nan f",
          synth(top={"major_iters": 0, "factorizations": 0, "f": "-nan",
                     "qp_failures_routed": 0, "rejected_steps": 0,
                     "qp_failures_in_a_row_peak": 0},
                hist=[{"qp_solved": "0", "qp_status": "n/a(qp_solved=0)",
                       "verdict": "Reject", "f": "-nan"}]),
          "START_POINT_NONFINITE")
    check("class 4 QP_CAP_BUDGET: last QP max_iter, peak == budget",
          synth(hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET")
    check("class 5 QP_NUMERICAL: last QP numerical_error",
          synth(hist=[{"qp_solved": "1", "qp_status": "numerical_error",
                       "verdict": "Reject"}]),
          "QP_NUMERICAL")
    check("class 6 QP_UNRETRYABLE: last QP max_iter, peak < budget",
          synth(top={"qp_failures_in_a_row_peak": 1, "qp_failures_routed": 1,
                     "rejected_steps": 1},
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_UNRETRYABLE")
    for sub in ("infeasible", "max_iter", "numerical_error", "interrupted", "optimal"):
        check(f"class 7 RESTORATION_EXIT({sub})",
              synth(top={"status": "infeasible", "restoration_entries": 1,
                         "restoration_sub_status": sub, "restoration_h_in": 10,
                         "restoration_h_out": 10, "restoration_f_in": 1,
                         "restoration_f_out": 1},
                    hist=[{"qp_solved": "1", "qp_status": "optimal",
                           "verdict": "Restore", "elastic_applied": "1"}]),
              f"RESTORATION_EXIT({sub})")
    check("class 8 MAJOR_BUDGET: max_iter at OPTIONS:max_iter majors",
          synth(top={"status": "max_iter", "major_iters": 100},
                hist=[{"qp_solved": "1", "qp_status": "optimal", "verdict": "AcceptF"}]),
          "MAJOR_BUDGET")
    check("class 9 OTHER: a terminal `optimal` no arm claims",
          synth(top={"status": "optimal"},
                hist=[{"qp_solved": "1", "qp_status": "optimal", "verdict": "AcceptF"}]),
          "OTHER")

    print("", file=stream)
    print("PART 2 -- the PRECEDENCE, on rows that satisfy TWO arms at once", file=stream)
    check("2 beats 3: deadline fired AND 0 majors/0 facts/nan f",
          synth(top={"deadline_fired": 1, "status": "numerical_error",
                     "major_iters": 0, "factorizations": 0, "f": "nan"},
                hist=[{"qp_solved": "0", "qp_status": "n/a(qp_solved=0)"}]),
          "WALL_TRUNCATED")
    check("2 beats 4: deadline fired AND last QP max_iter at the budget",
          synth(top={"deadline_fired": 1},
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "WALL_TRUNCATED")
    check("2 beats 7: deadline fired AND restoration ran",
          synth(top={"deadline_fired": 1, "status": "interrupted",
                     "restoration_entries": 1, "restoration_sub_status": "infeasible",
                     "restoration_h_in": 1, "restoration_h_out": 1,
                     "restoration_f_in": 1, "restoration_f_out": 1},
                hist=[{"qp_solved": "0"}]),
          "WALL_TRUNCATED")
    check("3 beats 9: the nonfinite-start arm claims a row OTHER would take",
          synth(top={"major_iters": 0, "factorizations": 0, "f": "nan",
                     "qp_failures_routed": 0, "rejected_steps": 0},
                hist=[{"qp_solved": "0", "qp_status": "n/a(qp_solved=0)"}]),
          "START_POINT_NONFINITE")
    check("4 beats 7: last QP max_iter at the budget AND restoration ran",
          synth(top={"restoration_entries": 1, "restoration_sub_status": "numerical_error",
                     "restoration_h_in": 100, "restoration_h_out": 99,
                     "restoration_f_in": 1, "restoration_f_out": 1,
                     "elastic_activations": 1},
                hist=[{"qp_solved": "1", "qp_status": "max_iter",
                       "verdict": "Restore", "elastic_applied": "1"}]),
          "QP_CAP_BUDGET")
    check("5 beats 6: last QP numerical_error AND the budget still had room",
          synth(top={"qp_failures_in_a_row_peak": 1, "qp_failures_routed": 1,
                     "rejected_steps": 1},
                hist=[{"qp_solved": "1", "qp_status": "numerical_error",
                       "verdict": "Reject"}]),
          "QP_NUMERICAL")
    check("4 beats 6: last QP max_iter, peak == budget (6 would also match a "
          "peak<budget row)",
          synth(hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET")
    check("7, the AMBIGUOUS shape: terminal infeasible, last row max_iter/elastic",
          synth(top={"status": "infeasible", "restoration_entries": 1,
                     "restoration_sub_status": "infeasible", "restoration_h_in": 5,
                     "restoration_h_out": 5, "restoration_f_in": 2,
                     "restoration_f_out": 2, "elastic_activations": 19,
                     "qp_failures_routed": 0, "rejected_steps": 0,
                     "qp_failures_in_a_row_peak": 0},
                hist=[{"qp_solved": "1", "qp_status": "max_iter",
                       "verdict": "Restore", "elastic_applied": "1"}]),
          "RESTORATION_EXIT(infeasible)")
    check("7 needs classes 4-6 to be silent: a non-QP-failure last row",
          synth(top={"status": "infeasible", "restoration_entries": 1,
                     "restoration_sub_status": "infeasible", "restoration_h_in": 5,
                     "restoration_h_out": 5, "restoration_f_in": 2,
                     "restoration_f_out": 2},
                hist=[{"qp_solved": "0", "qp_status": "n/a(qp_solved=0)",
                       "verdict": "Reject"}]),
          "RESTORATION_EXIT(infeasible)")

    print("", file=stream)
    print("PART 3 -- every FLAG, TRUE and FALSE and N/A", file=stream)
    check("F1 true: sub optimal, h_out <= feas_tol, f doubled",
          synth(top={"restoration_entries": 1, "restoration_sub_status": "optimal",
                     "restoration_h_in": 100, "restoration_h_out": "1e-13",
                     "restoration_f_in": 10, "restoration_f_out": 1000},
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET", want_flags={"F1_RESTORATION_EXCURSION": True})
    check("F1 false: the gap is below the bar",
          synth(top={"restoration_entries": 1, "restoration_sub_status": "optimal",
                     "restoration_h_in": 100, "restoration_h_out": "1e-13",
                     "restoration_f_in": 1000, "restoration_f_out": 1001},
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET", want_flags={"F1_RESTORATION_EXCURSION": False})
    check("F1 false: h_out ABOVE feas_tol, however big the f gap",
          synth(top={"restoration_entries": 1, "restoration_sub_status": "optimal",
                     "restoration_h_in": 100, "restoration_h_out": "1e-3",
                     "restoration_f_in": 1, "restoration_f_out": "1e12"},
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET", want_flags={"F1_RESTORATION_EXCURSION": False})
    check("F1/F2 N/A: restoration never ran",
          synth(hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET", want_flags={"F1_RESTORATION_EXCURSION": None,
                                       "F2_RESTORATION_STUCK": None})
    check("F2 true: sub infeasible, h_out == h_in (the EXACT null)",
          synth(top={"status": "infeasible", "restoration_entries": 1,
                     "restoration_sub_status": "infeasible",
                     "restoration_h_in": 48491.383947, "restoration_h_out": 48491.383947,
                     "restoration_f_in": 1, "restoration_f_out": 1},
                hist=[{"qp_solved": "0"}]),
          "RESTORATION_EXIT(infeasible)", want_flags={"F2_RESTORATION_STUCK": True})
    check("F2 false: the sub-solve cut h by 5 per cent",
          synth(top={"status": "infeasible", "restoration_entries": 1,
                     "restoration_sub_status": "numerical_error",
                     "restoration_h_in": 100, "restoration_h_out": 95,
                     "restoration_f_in": 1, "restoration_f_out": 1},
                hist=[{"qp_solved": "0"}]),
          "RESTORATION_EXIT(numerical_error)", want_flags={"F2_RESTORATION_STUCK": False})
    check("F2 false: h stood still but the sub-solve returned OPTIMAL",
          synth(top={"status": "infeasible", "restoration_entries": 1,
                     "restoration_sub_status": "optimal",
                     "restoration_h_in": 10, "restoration_h_out": 10,
                     "restoration_f_in": 1, "restoration_f_out": 1},
                hist=[{"qp_solved": "0"}]),
          "RESTORATION_EXIT(optimal)", want_flags={"F2_RESTORATION_STUCK": False})
    check("F3 true: the last row's radius is under tr_min/0.5",
          synth(opts={"opt_tr_min": "1e-3"},
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject",
                       "tr_radius": "1e-3"}]),
          "QP_CAP_BUDGET", want_flags={"F3_TR_AT_FLOOR": True})
    check("F3 false: the radius is one shrink above the floor test",
          synth(opts={"opt_tr_min": "1e-3"},
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject",
                       "tr_radius": "2e-3"}]),
          "QP_CAP_BUDGET", want_flags={"F3_TR_AT_FLOOR": False})
    check("F4 true: 0 accepted steps and 3 JUDGED rejections",
          synth(top={"steps_accepted": 0, "rejected_steps": 6, "qp_failures_routed": 3},
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET", want_flags={"F4_REJECTION_STALL": True})
    check("F4 false: 3 rejections that are all ROUTED QP failures",
          synth(top={"steps_accepted": 0, "rejected_steps": 3, "qp_failures_routed": 3},
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET", want_flags={"F4_REJECTION_STALL": False})
    check("F4 false: 3 judged rejections but a step WAS accepted",
          synth(top={"steps_accepted": 2, "rejected_steps": 6, "qp_failures_routed": 3},
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET", want_flags={"F4_REJECTION_STALL": False})
    check("F5 true / F6 true: a capped subproblem and an elastic activation",
          synth(top={"qp_maxiter_exits": 4, "elastic_activations": 1},
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET", want_flags={"F5_CAP_CHURN": True, "F6_ELASTIC_ENGAGED": True})
    check("F5 false / F6 false: neither fired",
          synth(hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET", want_flags={"F5_CAP_CHURN": False,
                                       "F6_ELASTIC_ENGAGED": False})
    check("F7 true: a failing ledger record, warm",
          synth(hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}],
                ledger=[{"status": "optimal", "warm": "0"},
                        {"status": "max_iter", "warm": "1"}]),
          "QP_CAP_BUDGET", want_flags={"F7_WARM_FAILURES": True})
    check("F7 false: the ledger's records all ended optimal",
          synth(hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}],
                ledger=[{"status": "optimal", "warm": "0"}]),
          "QP_CAP_BUDGET", want_flags={"F7_WARM_FAILURES": False})
    check("F7 N/A: the ledger holds no record",
          synth(hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          "QP_CAP_BUDGET", want_flags={"F7_WARM_FAILURES": None})

    print("", file=stream)
    print("PART 4 -- the STRUCTURAL REFUSALS. A row this script cannot trust is "
          "REFUSED, never classified.", file=stream)
    check("no PROBE_END (a probe killed mid-write)",
          synth(corrupt="no_end"), want_refusal=True)
    check("a missing block (OPTIONS)", synth(corrupt="no_block"), want_refusal=True)
    check("the counters block declares 48 fields, not 49",
          synth(corrupt="counter_count"), want_refusal=True)
    check("the history header is one column short",
          synth(corrupt="hist_width"), want_refusal=True)
    check("the probe's own judged-rejection derivation disagrees with the counters",
          synth(corrupt="judged",
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          want_refusal=True)
    check("a key the predicates read is missing (opt_qp_failure_budget)",
          synth(omit=("opt_qp_failure_budget",),
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          want_refusal=True)
    check("a key the predicates read is missing (restoration_entries)",
          synth(omit=("restoration_entries",),
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          want_refusal=True)
    check("a history row whose qp_status CONTAINS `=` is still a row, not a key",
          synth(top={"major_iters": 0, "factorizations": 0, "f": "-nan",
                     "qp_failures_routed": 0, "rejected_steps": 0,
                     "qp_failures_in_a_row_peak": 0},
                hist=[{"qp_solved": "0", "qp_status": "n/a(qp_solved=0)",
                       "verdict": "Reject", "f": "-nan"}]),
          "START_POINT_NONFINITE", want_flags={"F3_TR_AT_FLOOR": False})
    check("the history's own h_rows_printed disagrees with the rows parsed",
          synth(corrupt="hist_count",
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          want_refusal=True)
    check("the ledger's own ledger_records_printed disagrees with the records parsed",
          synth(corrupt="ledger_count",
                hist=[{"qp_solved": "1", "qp_status": "max_iter", "verdict": "Reject"}]),
          want_refusal=True)
    check("the LEDGER block carries no LEDGER_RECORDS sub-block",
          synth(corrupt="no_ledger_records"), want_refusal=True)
    check("the file is not a probe row at all",
          "hello\nworld\n", want_refusal=True)

    print("", file=stream)
    print("PART 5 -- RULE.md section 1.3's CONSENSUS, both arms and the ABSENT case",
          file=stream)

    def ccheck(name, lanes, want_star, want_absent=False):
        nonlocal passed, failed
        star, why = consensus("SYNTH", lanes)
        ok = (star is None) if want_absent else (
            star is not None and want_star is not None
            and abs(star - want_star) <= 1e-9 * max(1.0, abs(want_star)))
        if ok:
            passed += 1
            print(f"  FOLD PASS  {name:<52} -> {why}", file=stream)
        else:
            failed += 1
            print(f"  FOLD FAIL  {name:<52} -> {why}", file=stream)

    ccheck("two lanes agree to 6 significant digits",
           [("hven-ipm", 41737.786836186), ("ipopt", 41737.786849711),
            ("snopt", None)], 41737.8)
    ccheck("the |f| <= 1e-6 arm gives consensus 0",
           [("hven-ipm", 1e-14), ("ipopt", 3e-9), ("snopt", 5.0)], 0.0)
    ccheck("three counted lanes that agree nowhere -> ABSENT",
           [("hven-ipm", 1.0), ("ipopt", 2.0), ("snopt", 3.0)], None, want_absent=True)
    ccheck("one counted lane -> ABSENT",
           [("hven-ipm", 1.0), ("ipopt", None), ("snopt", None)], None, want_absent=True)
    ccheck("no counted lane -> ABSENT",
           [("hven-ipm", None), ("ipopt", None), ("snopt", None)], None, want_absent=True)
    ccheck("agreement at the 6th digit but not the 7th still agrees",
           [("hven-ipm", 129660.686196), ("ipopt", 129660.686120), ("snopt", None)],
           129661.0)
    ccheck("a lane that is NOT COUNTED cannot form a consensus with the other",
           [("hven-ipm", 5.0), ("ipopt", None), ("snopt", 5.0)], 5.0)

    print("", file=stream)
    print("PART 6 -- the LANE CSV reader refuses a malformed harness file", file=stream)
    with tempfile.NamedTemporaryFile("w", suffix=".csv", delete=False) as fh:
        fh.write("a,b,c\n1,2\n")
        bad = fh.name
    try:
        read_lane_csv(bad, "deliberately ragged")
        failed += 1
        print("  FOLD FAIL  a ragged CSV was parsed rather than refused", file=stream)
    except Refusal as e:
        passed += 1
        print(f"  FOLD PASS  a ragged CSV is refused                      -> {e}",
              file=stream)
    finally:
        os.unlink(bad)
    try:
        read_lane_csv(os.path.join(tempfile.gettempdir(), "m3c-no-such.csv"), "absent")
        failed += 1
        print("  FOLD FAIL  a missing CSV was not refused", file=stream)
    except Refusal:
        passed += 1
        print("  FOLD PASS  a missing CSV is refused", file=stream)

    print("", file=stream)
    print(f"FOLDS_PASSED={passed}", file=stream)
    print(f"FOLDS_FAILED={failed}", file=stream)
    return 0 if failed == 0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fold", action="store_true")
    ap.add_argument("--rows")
    ap.add_argument("--cells")
    ap.add_argument("--out")
    a = ap.parse_args()
    stream = open(a.out, "w") if a.out else sys.stdout
    try:
        if a.fold:
            return fold(stream)
        if not a.rows:
            print("REFUSED [setup] neither --fold nor --rows", file=stream)
            return 2
        if not os.path.isdir(a.rows):
            print(f"REFUSED [setup] --rows is not a directory: {a.rows}", file=stream)
            return 2
        cells = a.cells.split(",") if a.cells else ALL_CELLS
        try:
            return classify_dir(a.rows, cells, stream)
        except Refusal as e:
            print(f"REFUSED [setup] {e}", file=stream)
            return 2
    finally:
        if a.out:
            stream.close()


if __name__ == "__main__":
    sys.exit(main())
