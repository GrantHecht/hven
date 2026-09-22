#!/usr/bin/env python3
"""RULE-P2.md §2 (the observer-solve-index <-> ledger-record MAPPING),
§5 (the IDENTITY GATE) and §3 stage 2 (the SELECTION and the SLICING).

Nothing here runs a solver.  It reads

  * the COMMITTED pass-1 row            rows/<CELL>.txt
  * the pass-2 leg's row                <p2out>/<CELL>.p2.txt
  * the pass-2 leg's flat trace         <p2traces>/<CELL>-all.csv

and writes the mapping table, the gate transcript, and the sliced per-solve
CSVs that RULE-P2.md §3 selects.

WHAT THIS SCRIPT IS NOT ALLOWED TO DO, and the refusals that enforce it:

  * It never repairs a rule.  Where RULE-P2.md §5.1 names a field the pass-2
    instrument does not print, the field is reported ABSENT and the defect is
    named -- the gate is NOT quietly re-scoped and the rule is NOT edited.
  * It never assumes the mapping from an index correspondence (§2).  The
    assignment must be UNIQUE under the (minors, factorizations) match with a
    strictly increasing constraint, or the cell is AMBIGUOUS.
  * It never turns §5.3's two conservation sums into a bool.  They are
    reported as their two numbers and their difference, because the rule
    declares the agreement a CLAIM about the counters and not a fact.

`--selftest` exercises every refusal on constructed inputs (§7 #4).
"""

import argparse
import csv
import hashlib
import math
import os
import sys
from collections import OrderedDict

# --------------------------------------------------------------------------
# RULE-P2.md §5.1 -- the fields, exactly as the rule names them.
# --------------------------------------------------------------------------
BYTE_FIELDS = (
    "status", "major_iters", "qp_minor_iters", "factorizations",
    "steps_accepted", "rejected_steps", "elastic_activations",
    "restoration_entries", "restoration_sub_status", "qp_maxiter_exits",
    "qp_numerical_exits", "qp_failures_routed", "qp_failures_in_a_row_peak",
    "deadline_fired",
)
NEAR_ULP_FIELDS = ("f", "stationarity", "feasibility_e", "feasibility_i")
NEAR_ULP_TOL = 1e-12          # |a-b| <= 1e-12 * max(1,|a|,|b|)

# Fields BOTH rows carry that §5.1 does not name.  Checked as a SUPPLEMENT and
# reported separately; a difference here is a finding, never a gate outcome,
# because the rule's bytes are the pin and this column was not pre-registered.
SUPPLEMENT_BYTE_FIELDS = (
    "n", "me", "mi", "restoration_iters", "symbolic_analyses",
    "border_refine_steps", "complementarity", "infeasibility_certified",
)

# §2's PRE-REGISTERED CAVEAT (M3b C7): either of these set ABORTS the cell.
ABORT_FLAGS = ("summary_truncated", "summary_allocation_failed")


# --------------------------------------------------------------------------
# reading
# --------------------------------------------------------------------------
def read_row(path):
    """A probe row: scalars, the LEDGER_RECORDS table, the SOLVE_SUMMARY table.

    Absent sections come back as None so a caller can say ABSENT rather than
    read an empty list as an empty table.
    """
    scal, ledger, summary = OrderedDict(), None, None
    mode = None
    lcols, scols = None, None
    with open(path, errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line == "LEDGER_RECORDS_BEGIN":
                mode, ledger, lcols = "ledger", [], None
                continue
            if line == "LEDGER_RECORDS_END":
                mode = None
                continue
            if line == "SOLVE_SUMMARY_BEGIN":
                mode, summary, scols = "summary", [], None
                continue
            if line == "SOLVE_SUMMARY_END":
                mode = None
                continue
            if mode == "ledger":
                if lcols is None:
                    lcols = line.split(",")
                else:
                    ledger.append(OrderedDict(zip(lcols, line.split(","))))
                continue
            if mode == "summary":
                if "=" in line and "," not in line:
                    k, v = line.split("=", 1)
                    scal[k] = v
                    continue
                if scols is None:
                    scols = line.split(",")
                else:
                    summary.append(OrderedDict(zip(scols, line.split(","))))
                continue
            if "=" in line:
                k, v = line.split("=", 1)
                scal[k] = v
    return {"scalars": scal, "ledger": ledger, "summary": summary, "path": path}


def _int(rows, key):
    return [int(r[key]) for r in rows]


# --------------------------------------------------------------------------
# RULE-P2.md §5.1 -- the whole-run gate
# --------------------------------------------------------------------------
def near_ulp_equal(a_text, b_text):
    """CLAUDE.md §7's near-ulp gate, with `nan` matched as a BYTE fact.

    Returns (verdict, detail).  A text pair that is byte-equal passes without
    a float conversion at all, which is what makes `nan` == `nan` a byte fact
    and never a numeric one.
    """
    if a_text == b_text:
        return "EQUAL_BYTES", 0.0
    try:
        a, b = float(a_text), float(b_text)
    except ValueError:
        return "UNPARSEABLE", None
    if math.isnan(a) or math.isnan(b):
        # one side is nan and the bytes differ: the rule matches nan by BYTES,
        # so this is a MISMATCH and is never softened numerically.
        return "NAN_BYTE_MISMATCH", None
    d = abs(a - b)
    bar = NEAR_ULP_TOL * max(1.0, abs(a), abs(b))
    return ("WITHIN" if d <= bar else "EXCEEDS"), d


def whole_run_gate(p1, p2):
    out = OrderedDict()
    s1, s2 = p1["scalars"], p2["scalars"]
    for f in BYTE_FIELDS:
        if f not in s1 and f not in s2:
            out[f] = ("ABSENT_BOTH", None, None)
        elif f not in s2:
            out[f] = ("ABSENT_IN_PASS2_INSTRUMENT", s1[f], None)
        elif f not in s1:
            out[f] = ("ABSENT_IN_PASS1_ROW", None, s2[f])
        else:
            out[f] = (("EQUAL" if s1[f] == s2[f] else "DIFFERS"), s1[f], s2[f])
    for f in NEAR_ULP_FIELDS:
        if f not in s1 or f not in s2:
            out[f] = ("ABSENT", s1.get(f), s2.get(f))
        else:
            v, d = near_ulp_equal(s1[f], s2[f])
            out[f] = (v, s1[f], s2[f], d)
    return out


def gate_failed(gate):
    """A cell STOPS on a DIFFERS, an EXCEEDS, a NAN_BYTE_MISMATCH or an
    UNPARSEABLE.  An ABSENT field is NOT a failure -- it is a defect of the
    rule, reported as one, and the caller must surface it."""
    bad = [k for k, v in gate.items()
           if v[0] in ("DIFFERS", "EXCEEDS", "NAN_BYTE_MISMATCH",
                       "UNPARSEABLE", "ABSENT_IN_PASS1_ROW")]
    return bad


def gate_absent(gate):
    return [k for k, v in gate.items()
            if v[0] in ("ABSENT", "ABSENT_BOTH", "ABSENT_IN_PASS2_INSTRUMENT")]


def supplement_gate(p1, p2):
    out = OrderedDict()
    s1, s2 = p1["scalars"], p2["scalars"]
    for f in SUPPLEMENT_BYTE_FIELDS:
        if f in s1 and f in s2:
            out[f] = (("EQUAL" if s1[f] == s2[f] else "DIFFERS"), s1[f], s2[f])
    return out


# --------------------------------------------------------------------------
# RULE-P2.md §2 -- the mapping
# --------------------------------------------------------------------------
def count_assignments(pairs_ledger, pairs_summary, cap=2):
    """The number of strictly increasing injections r -> k with
    pairs_summary[k] == pairs_ledger[r], counted up to `cap`.

    Counting is a DP over (record, first allowed summary index); the count is
    saturated at `cap` so a table with astronomically many assignments is
    still decided in linear time and still reports "more than one".
    """
    R, K = len(pairs_ledger), len(pairs_summary)
    # ways[r][k] = assignments of records r..R-1 into summary indices >= k
    ways = [[0] * (K + 1) for _ in range(R + 1)]
    for k in range(K + 1):
        ways[R][k] = 1
    for r in range(R - 1, -1, -1):
        acc = 0
        for k in range(K - 1, -1, -1):
            if pairs_summary[k] == pairs_ledger[r]:
                acc += ways[r + 1][k + 1]
                if acc > cap:
                    acc = cap + 1
            ways[r][k] = acc
        ways[r][K] = 0
    return ways[0][0], ways


def unique_assignment(pairs_ledger, pairs_summary):
    """(status, assignment).  status is UNIQUE / AMBIGUOUS / NONE."""
    total, ways = count_assignments(pairs_ledger, pairs_summary)
    if total == 0:
        return "NONE", None
    if total > 1:
        return "AMBIGUOUS", None
    assign, k = [], 0
    for r in range(len(pairs_ledger)):
        while not (pairs_summary[k] == pairs_ledger[r] and ways[r][k] == 1):
            k += 1
        assign.append(k)
        k += 1
    return "UNIQUE", assign


def map_cell(p1, p2):
    """§2's mapping plus §5.2's per-solve gate, on one cell."""
    led = p1["ledger"]
    summ = p2["summary"]
    if led is None:
        return {"status": "NO_PASS1_LEDGER"}
    if summ is None:
        return {"status": "NO_PASS2_SUMMARY"}
    pl = [(int(r["minor_iters"]), int(r["factorizations"])) for r in led]
    ps = [(int(r["minors"]), int(r["factorizations"])) for r in summ]
    status, assign = unique_assignment(pl, ps)
    res = {"status": status, "assign": assign,
           "ledger_pairs": pl, "summary_pairs": ps}
    if status == "UNIQUE":
        # §5.2 -- the same pair, now checked against the OTHER run's bytes.
        res["per_solve_gate"] = [
            (r, assign[r], pl[r], ps[assign[r]], pl[r] == ps[assign[r]])
            for r in range(len(pl))]
        res["not_ledgered"] = [k for k in range(len(ps)) if k not in set(assign)]
    else:
        res["not_ledgered"] = None
    return res


# --------------------------------------------------------------------------
# RULE-P2.md §5.3 -- the two conservation sums, as NUMBERS
# --------------------------------------------------------------------------
def conservation(p2):
    summ, s = p2["summary"], p2["scalars"]
    sm = sum(int(r["minors"]) for r in summ)
    sf = sum(int(r["factorizations"]) for r in summ)
    cm, cf = int(s["qp_minor_iters"]), int(s["factorizations"])
    return OrderedDict((
        ("minors_summary_sum", sm), ("minors_counter", cm),
        ("minors_difference", sm - cm),
        ("factorizations_summary_sum", sf), ("factorizations_counter", cf),
        ("factorizations_difference", sf - cf),
    ))


# --------------------------------------------------------------------------
# RULE-P2.md §3 stage 2 -- the SELECTION
# --------------------------------------------------------------------------
CAPPED = ("max_iter", "numerical_error")


def select_solves(p1, p2, mapping, want_not_ledgered):
    """Returns [(kind, solve_index, why)], decided mechanically.

    kind is FIRST_CAPPED / LAST_CAPPED / FIRST_NOT_LEDGERED_CAPPED.
    """
    picks, notes = [], []
    if mapping["status"] != "UNIQUE":
        notes.append("mapping is %s: no main-engine solve is selected through "
                     "it (RULE-P2 §2)" % mapping["status"])
        return picks, notes
    led, assign = p1["ledger"], mapping["assign"]
    capped_r = [r for r, rec in enumerate(led) if rec["status"] in CAPPED]
    if capped_r:
        picks.append(("FIRST_CAPPED", assign[capped_r[0]],
                      "ledger record %d status=%s" % (capped_r[0],
                                                      led[capped_r[0]]["status"])))
        if capped_r[-1] != capped_r[0]:
            picks.append(("LAST_CAPPED", assign[capped_r[-1]],
                          "ledger record %d status=%s" % (capped_r[-1],
                                                          led[capped_r[-1]]["status"])))
        else:
            notes.append("only ONE capped ledger record: FIRST and LAST "
                         "coincide and one solve is selected")
    else:
        notes.append("NO capped ledger record on this cell")
    if want_not_ledgered:
        cap = int(p2["scalars"].get("trace_max_iter", "-1"))
        nl = mapping["not_ledgered"] or []
        hit = None
        for k in nl:
            row = p2["summary"][k]
            if cap > 0 and int(row["minors"]) == cap:
                hit = k
                break
        if hit is None:
            # The per-solve max_iter cap is ABSENT from the summary (§2's
            # declared-absent list): only the LAST solve's header survives at
            # --capture-solve -1.  The rule's predicate `minors == its own
            # max_iter cap` is therefore not evaluable per row, and the honest
            # substitute is stated rather than silently swapped in.
            notes.append("FIRST NOT-LEDGERED capped solve: the per-solve "
                         "max_iter cap is ABSENT at --capture-solve -1 "
                         "(RULE-P2 §2's declared-absent list), so §3's "
                         "predicate `minors == its own max_iter cap` is NOT "
                         "evaluable per summary row. SUBSTITUTE APPLIED, and "
                         "named: the first NOT-LEDGERED row whose minors "
                         "equal the modal minors of the cell's capped ledger "
                         "records, else the first NOT-LEDGERED row with the "
                         "largest minors.")
            capvals = [int(led[r]["minor_iters"]) for r in capped_r]
            modal = max(set(capvals), key=capvals.count) if capvals else None
            for k in nl:
                if modal is not None and int(p2["summary"][k]["minors"]) == modal:
                    hit = k
                    notes.append("substitute matched the modal capped minor "
                                 "count %d at solve %d" % (modal, k))
                    break
            if hit is None and nl:
                hit = max(nl, key=lambda k: int(p2["summary"][k]["minors"]))
                notes.append("substitute fell through to the largest "
                             "NOT-LEDGERED solve, %d with %s minors"
                             % (hit, p2["summary"][hit]["minors"]))
        if hit is None:
            notes.append("NO REACHABLE SUB-DRIVER CAPPED SOLVE")
        else:
            picks.append(("FIRST_NOT_LEDGERED_CAPPED", hit,
                          "NOT-LEDGERED (restoration sub-driver or a lazy "
                          "engine); minors=%s" % p2["summary"][hit]["minors"]))
    # de-duplicate while keeping the first reason
    seen, uniq = set(), []
    for kind, k, why in picks:
        if k in seen:
            uniq.append((kind + "(SAME SOLVE AS AN EARLIER PICK)", k, why))
            continue
        seen.add(k)
        uniq.append((kind, k, why))
    return uniq, notes


def slice_trace(trace_path, solves, out_dir, cell):
    """Writes one CSV per selected solve, carrying the flat trace's own header
    comments plus the slice's provenance.  Returns [(solve, path, rows)]."""
    want = {k for _, k, _ in solves}
    head, cols = [], None
    buckets = {k: [] for k in want}
    with open(trace_path, errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.startswith("#"):
                head.append(line)
                continue
            if not line:
                continue
            if cols is None:
                cols = line
                continue
            k = int(line.split(",", 1)[0])
            if k in buckets:
                buckets[k].append(line)
    made = []
    for kind, k, why in solves:
        path = os.path.join(out_dir, "%s-solve%d.csv" % (cell, k))
        with open(path, "w") as fh:
            for h in head:
                fh.write(h + "\n")
            fh.write("# SLICE of the flat pass-2 trace, RULE-P2 §3 stage 2\n")
            fh.write("# slice_cell=%s\n" % cell)
            fh.write("# slice_solve=%d\n" % k)
            fh.write("# slice_kind=%s\n" % kind)
            fh.write("# slice_reason=%s\n" % why)
            fh.write("# slice_source=%s\n" % trace_path)
            fh.write("# slice_rows=%d\n" % len(buckets[k]))
            fh.write(cols + "\n")
            for r in buckets[k]:
                fh.write(r + "\n")
        made.append((k, path, len(buckets[k])))
    return made


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# --------------------------------------------------------------------------
# §7 #4 -- the folds.  A checker that has never been seen to fail is not a
# checker, so every refusal below is exercised on a CONSTRUCTED input.
# --------------------------------------------------------------------------
def selftest(out=sys.stdout):
    fails = []

    def check(name, got, want):
        ok = got == want
        out.write("  %-58s %s   got=%r want=%r\n"
                  % (name, "PASS" if ok else "FAIL", got, want))
        if not ok:
            fails.append(name)

    out.write("FOLD 1 -- the near-ulp gate\n")
    check("byte-equal passes without a float conversion",
          near_ulp_equal("nan", "nan")[0], "EQUAL_BYTES")
    check("nan against a number is a BYTE mismatch",
          near_ulp_equal("nan", "0")[0], "NAN_BYTE_MISMATCH")
    check("a number against nan is a BYTE mismatch",
          near_ulp_equal("1.0", "nan")[0], "NAN_BYTE_MISMATCH")
    check("a difference inside the bar is WITHIN",
          near_ulp_equal("41737.543107411046", "41737.543107411104")[0], "WITHIN")
    check("a difference outside the bar EXCEEDS",
          near_ulp_equal("1.0", "1.0000001")[0], "EXCEEDS")
    check("a tiny absolute difference on a tiny value still EXCEEDS",
          near_ulp_equal("0.0", "1e-9")[0], "EXCEEDS")
    check("unparseable text REFUSES rather than comparing",
          near_ulp_equal("optimal", "max_iter")[0], "UNPARSEABLE")

    out.write("FOLD 2 -- the whole-run gate\n")
    base = {"scalars": OrderedDict(
        (f, "1") for f in BYTE_FIELDS)}
    base["scalars"].update({f: "1.0" for f in NEAR_ULP_FIELDS})
    base["ledger"] = None
    base["summary"] = None
    same = {"scalars": OrderedDict(base["scalars"]), "ledger": None,
            "summary": None}
    check("identical rows pass the gate", gate_failed(whole_run_gate(base, same)), [])
    moved = {"scalars": OrderedDict(base["scalars"]), "ledger": None,
             "summary": None}
    moved["scalars"]["major_iters"] = "2"
    check("a moved byte field FAILS",
          gate_failed(whole_run_gate(base, moved)), ["major_iters"])
    drifted = {"scalars": OrderedDict(base["scalars"]), "ledger": None,
               "summary": None}
    drifted["scalars"]["f"] = "1.0000001"
    check("a float past the near-ulp bar FAILS",
          gate_failed(whole_run_gate(base, drifted)), ["f"])
    stripped = {"scalars": OrderedDict(
        (k, v) for k, v in base["scalars"].items() if k != "elastic_activations"),
        "ledger": None, "summary": None}
    g = whole_run_gate(base, stripped)
    check("a field the pass-2 instrument omits is ABSENT, not a pass",
          g["elastic_activations"][0], "ABSENT_IN_PASS2_INSTRUMENT")
    check("...and it is REPORTED as absent",
          "elastic_activations" in gate_absent(g), True)
    check("...and it does not silently fail the cell",
          gate_failed(g), [])
    stripped1 = {"scalars": OrderedDict(
        (k, v) for k, v in base["scalars"].items() if k != "status"),
        "ledger": None, "summary": None}
    check("a field the PASS-1 row lacks FAILS (the pin must carry it)",
          gate_failed(whole_run_gate(stripped1, base)), ["status"])

    out.write("FOLD 3 -- the mapping\n")
    check("the identity table maps uniquely",
          unique_assignment([(1, 1), (2, 2)], [(1, 1), (2, 2)]),
          ("UNIQUE", [0, 1]))
    check("an interleaved foreign solve is skipped",
          unique_assignment([(1, 1), (2, 2)], [(9, 9), (1, 1), (7, 7), (2, 2)]),
          ("UNIQUE", [1, 3]))
    check("two identical candidate rows make it AMBIGUOUS",
          unique_assignment([(1, 1)], [(1, 1), (1, 1)])[0], "AMBIGUOUS")
    check("...and AMBIGUOUS returns NO assignment",
          unique_assignment([(1, 1)], [(1, 1), (1, 1)])[1], None)
    check("a repeated pair in BOTH tables is still ambiguous",
          unique_assignment([(5, 5), (5, 5)],
                            [(5, 5), (5, 5), (5, 5)])[0], "AMBIGUOUS")
    check("an unmatchable record is NONE",
          unique_assignment([(3, 3)], [(1, 1), (2, 2)])[0], "NONE")
    check("order is STRICT: a reversed table does not map",
          unique_assignment([(1, 1), (2, 2)], [(2, 2), (1, 1)])[0], "NONE")
    check("a pair matching on minors alone does NOT map",
          unique_assignment([(4, 7)], [(4, 8)])[0], "NONE")
    check("a pair matching on factorizations alone does NOT map",
          unique_assignment([(4, 7)], [(5, 7)])[0], "NONE")
    check("the NOT-LEDGERED residue is the unmatched rows",
          map_cell({"ledger": [{"minor_iters": "1", "factorizations": "1"}]},
                   {"summary": [{"minors": "9", "factorizations": "9"},
                                {"minors": "1", "factorizations": "1"}]}
                   )["not_ledgered"], [0])

    out.write("FOLD 4 -- the conservation sums are NUMBERS, never a bool\n")
    p2 = {"summary": [{"minors": "10", "factorizations": "3"},
                      {"minors": "5", "factorizations": "2"}],
          "scalars": {"qp_minor_iters": "15", "factorizations": "5"}}
    c = conservation(p2)
    check("agreeing sums report a zero DIFFERENCE", c["minors_difference"], 0)
    p2b = {"summary": p2["summary"],
           "scalars": {"qp_minor_iters": "14", "factorizations": "5"}}
    cb = conservation(p2b)
    check("a disagreement reports its size, not False",
          (cb["minors_summary_sum"], cb["minors_counter"],
           cb["minors_difference"]), (15, 14, 1))

    out.write("FOLD 5 -- the selection\n")
    p1 = {"ledger": [{"status": "optimal", "minor_iters": "5",
                      "factorizations": "1"},
                     {"status": "max_iter", "minor_iters": "100",
                      "factorizations": "2"},
                     {"status": "optimal", "minor_iters": "7",
                      "factorizations": "1"},
                     {"status": "max_iter", "minor_iters": "100",
                      "factorizations": "3"}]}
    p2s = {"summary": [{"minors": "5", "factorizations": "1"},
                       {"minors": "100", "factorizations": "2"},
                       {"minors": "100", "factorizations": "9"},
                       {"minors": "7", "factorizations": "1"},
                       {"minors": "100", "factorizations": "3"}],
           "scalars": {"qp_minor_iters": "312", "factorizations": "16"}}
    m = map_cell(p1, p2s)
    check("the mapping on the fixture is unique", m["status"], "UNIQUE")
    picks, notes = select_solves(p1, p2s, m, want_not_ledgered=False)
    check("FIRST and LAST capped are the mapped solves",
          [(k, i) for k, i, _ in picks],
          [("FIRST_CAPPED", 1), ("LAST_CAPPED", 4)])
    picks2, notes2 = select_solves(p1, p2s, m, want_not_ledgered=True)
    check("the NOT-LEDGERED pick is the unmatched capped-size row",
          [i for k, i, _ in picks2 if k.startswith("FIRST_NOT_LEDGERED")], [2])
    single = {"ledger": [{"status": "max_iter", "minor_iters": "100",
                          "factorizations": "2"}]}
    m1 = map_cell(single, {"summary": [{"minors": "100",
                                        "factorizations": "2"}]})
    p1s, n1s = select_solves(single, {"summary": [{"minors": "100",
                                                   "factorizations": "2"}],
                                      "scalars": {}}, m1, False)
    check("one capped record selects ONE solve and says so",
          (len(p1s), any("coincide" in n for n in n1s)), (1, True))
    nocap = {"ledger": [{"status": "optimal", "minor_iters": "5",
                         "factorizations": "1"}]}
    mn = map_cell(nocap, {"summary": [{"minors": "5", "factorizations": "1"}]})
    pn, nn = select_solves(nocap, {"summary": [{"minors": "5",
                                                "factorizations": "1"}],
                                   "scalars": {}}, mn, False)
    check("no capped record selects NOTHING and says so",
          (pn, any("NO capped" in n for n in nn)), ([], True))
    amb = {"status": "AMBIGUOUS", "assign": None, "not_ledgered": None}
    pa, na = select_solves(p1, p2s, amb, True)
    check("an AMBIGUOUS mapping selects nothing",
          (pa, any("AMBIGUOUS" in n for n in na)), ([], True))

    out.write("FOLD 6 -- the slicer\n")
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        tp = os.path.join(td, "T-all.csv")
        with open(tp, "w") as fh:
            fh.write("# cell=T\n# records=4\nsolve,minor,x\n")
            for k, m_ in ((0, 0), (0, 1), (1, 0), (2, 0)):
                fh.write("%d,%d,z\n" % (k, m_))
        made = slice_trace(tp, [("FIRST_CAPPED", 0, "why")], td, "T")
        check("the slice keeps only its own solve's rows",
              [(k, n) for k, _, n in made], [(0, 2)])
        with open(made[0][1]) as fh:
            body = [l for l in fh if not l.startswith("#")]
        check("the slice carries the column header exactly once",
              body[0].strip(), "solve,minor,x")
        check("the slice names its source", any(
            l.startswith("# slice_source=") for l in open(made[0][1])), True)
        empty = slice_trace(tp, [("X", 9, "absent")], td, "T")
        check("a selected solve with no rows yields an EMPTY slice, not a crash",
              [(k, n) for k, _, n in empty], [(9, 0)])

    out.write("\nSELFTEST %s (%d failure%s)\n"
              % ("GREEN" if not fails else "RED", len(fails),
                 "" if len(fails) == 1 else "s"))
    return 0 if not fails else 1


# --------------------------------------------------------------------------
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--p1-rows")
    ap.add_argument("--p2-rows")
    ap.add_argument("--p2-traces")
    ap.add_argument("--cells")
    ap.add_argument("--not-ledgered-cells", default="",
                    help="cells whose pass-1 restoration sub-status is not "
                         "`optimal` (RULE-P2 §3 stage 2's second bullet)")
    ap.add_argument("--slice-out")
    ap.add_argument("--out")
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    for need in ("p1_rows", "p2_rows", "p2_traces", "cells", "out"):
        if not getattr(a, need):
            ap.error("--%s is required" % need.replace("_", "-"))
    cells = [c for c in a.cells.split(",") if c]
    nlc = {c for c in a.not_ledgered_cells.split(",") if c}
    if a.slice_out:
        os.makedirs(a.slice_out, exist_ok=True)

    fh = open(a.out, "w")

    def w(s=""):
        fh.write(s + "\n")

    w("RULE-P2.md §2 MAPPING, §5 IDENTITY GATE, §3 stage 2 SELECTION")
    w("generated by wrappers/map_p2.py; no solver ran.")
    w()
    stop, ambiguous, absent_fields = [], [], None
    selections = {}
    for cell in cells:
        p1p = os.path.join(a.p1_rows, cell + ".txt")
        p2p = os.path.join(a.p2_rows, cell + ".p2.txt")
        trp = os.path.join(a.p2_traces, cell + "-all.csv")
        for p in (p1p, p2p, trp):
            if not os.path.exists(p):
                w("=== %s: REFUSED, missing input %s" % (cell, p))
                stop.append(cell)
                break
        else:
            p1, p2 = read_row(p1p), read_row(p2p)
            w("=" * 74)
            w("CELL %s" % cell)
            w("=" * 74)
            w("-- RULE-P2 §2's pre-registered caveat (M3b C7): the abort flags")
            aborts = []
            for f in ABORT_FLAGS:
                v = p2["scalars"].get(f, "ABSENT")
                w("   %-28s %s" % (f, v))
                if v != "0":
                    aborts.append(f)
            nc = [r["solve"] for r in (p2["summary"] or [])
                  if r.get("minor_contiguous") != "1"]
            w("   %-28s %s" % ("solves with minor_contiguous=0",
                               ",".join(nc) if nc else "(none)"))
            if nc:
                aborts.append("minor_contiguous")
            w("   %-28s %s" % ("deadline_fired",
                               p2["scalars"].get("deadline_fired", "ABSENT")))
            w("   %-28s %s" % ("deadline_seconds",
                               p2["scalars"].get("deadline_seconds", "ABSENT")))
            if aborts:
                w("   => CELL ABORTED by §2: %s" % ",".join(aborts))
                stop.append(cell)
                w()
                continue
            w()
            w("-- RULE-P2 §5.1 the whole-run identity gate")
            g = whole_run_gate(p1, p2)
            for k, v in g.items():
                if len(v) == 4:
                    w("   %-28s %-22s p1=%s p2=%s d=%s"
                      % (k, v[0], v[1], v[2], v[3]))
                else:
                    w("   %-28s %-22s p1=%s p2=%s" % (k, v[0], v[1], v[2]))
            bad, miss = gate_failed(g), gate_absent(g)
            absent_fields = miss
            w("   GATE: %s" % ("IDENTICAL on every field the pass-2 "
                               "instrument prints" if not bad
                               else "STOP -- " + ",".join(bad)))
            if miss:
                w("   ABSENT (a defect of RULE-P2 §5.1, not of this cell): %s"
                  % ",".join(miss))
            w()
            w("-- SUPPLEMENT (fields both rows carry that §5.1 did not name)")
            for k, v in supplement_gate(p1, p2).items():
                w("   %-28s %-10s p1=%s p2=%s" % (k, v[0], v[1], v[2]))
            if bad:
                stop.append(cell)
                w("   => traces NOT read for this cell (§5.1)")
                w()
                continue
            w()
            w("-- RULE-P2 §5.3 the two conservation sums, as numbers")
            for k, v in conservation(p2).items():
                w("   %-32s %s" % (k, v))
            w()
            m = map_cell(p1, p2)
            w("-- RULE-P2 §2 the mapping: %s" % m["status"])
            if m["status"] == "UNIQUE":
                w("   ledger_record  observer_solve  (minors,factorizations)  "
                  "§5.2")
                for r, k, pl, ps, ok in m["per_solve_gate"]:
                    w("   %-14d %-15d %-24s %s"
                      % (r, k, "%s / %s" % (pl, ps), "EQUAL" if ok else "DIFFERS"))
                nl = m["not_ledgered"]
                w("   NOT-LEDGERED (restoration sub-driver or a lazy engine): "
                  "%s" % (",".join(str(x) for x in nl) if nl else "(none)"))
                w("   restoration_entries=%s  restoration_sub_status=%s"
                  % (p1["scalars"].get("restoration_entries"),
                     p1["scalars"].get("restoration_sub_status")))
                w("   elastic_activations=%s (pass-1 row)"
                  % p1["scalars"].get("elastic_activations"))
            else:
                ambiguous.append(cell)
                w("   ledger pairs : %s" % m["ledger_pairs"])
                w("   summary pairs: %s" % m["summary_pairs"])
                w("   every trace selected through it would be "
                  "provenance-UNCERTAIN; none is selected.")
            w()
            picks, notes = select_solves(p1, p2, m, cell in nlc)
            w("-- RULE-P2 §3 stage 2 the selection")
            for n in notes:
                w("   NOTE: %s" % n)
            for kind, k, why in picks:
                w("   %-40s solve=%-4d %s" % (kind, k, why))
            selections[cell] = picks
            if a.slice_out and picks:
                made = slice_trace(trp, picks, a.slice_out, cell)
                for k, path, n in made:
                    w("   sliced solve %-4d -> %s  (%d rows)"
                      % (k, os.path.relpath(path, a.slice_out), n))
            w("   FULL trace (NOT committed, %d bytes): %s"
              % (os.path.getsize(trp), trp))
            w()
    w("=" * 74)
    w("SUMMARY")
    w("  cells read           : %d" % len(cells))
    w("  cells STOPPED        : %s" % (",".join(stop) if stop else "(none)"))
    w("  cells AMBIGUOUS      : %s" % (",".join(ambiguous) if ambiguous else "(none)"))
    w("  §5.1 fields ABSENT from the pass-2 instrument: %s"
      % (",".join(absent_fields) if absent_fields else "(none)"))
    w("  solves selected      : %d"
      % sum(len(v) for v in selections.values()))
    fh.close()
    sys.stdout.write(open(a.out).read())
    return 0


if __name__ == "__main__":
    sys.exit(main())
