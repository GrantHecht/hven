#!/usr/bin/env python3
"""RULE-P2.md §6's read-outs and §6.1's three-way answer, COMPOSED from M3b's
two classifiers with their withdrawn arms excluded BY NAME.

READ THIS FIRST -- THE PROVENANCE OF EVERYTHING THIS SCRIPT PRINTS.

RULE-P2.md §5.1's identity gate STOPPED all ten pass-2 cells
(`tables/mapping-p2.txt`). Under the rule, that means these traces are NOT
traces of the census rows and §3's selection cannot run through §2's mapping.
Everything here is therefore **OUTSIDE RULE-P2**, exactly as M3b's README
carries an "OUTSIDE THE RULE" section, and it is labelled as what it is:

    a descriptive read-out of a DEFAULT-options run of the same cell,
    performed by a binary that is not the one that produced the census row.

It is not a verdict, it does not license a remedy, and it must never be
quoted as a statement about the four-lane scoreboard's rows.

TWO THINGS ARE RE-BASED, and both are declared here rather than assumed:

  * SELECTION. §3's "first and last capped MAIN-ENGINE solve" is defined
    through the pass-1 ledger. That ledger belongs to a different run. The
    substitute is TRACE-INTRINSIC and reads one recorded field: a solve whose
    LAST record's `action` is `terminal` reached the walk's termination test;
    a solve whose last record is an ordinary action did not. CAPPED means the
    latter. This is checked against ACOPP57, the one cell whose counters do
    agree with pass 1: it returns solves 3,4,5,6, which are exactly that
    cell's four `max_iter` ledger records.
  * ENGINE PROVENANCE. §2's NOT-LEDGERED label is likewise defined through
    the pass-1 ledger. The substitute is one-sided and says so: a solve whose
    `inertia_expected_pos` ever EXCEEDS the problem's `n`, or whose
    `inertia_expected_neg` ever falls BELOW `me`, is solving a problem of
    different dimensions and is therefore certainly NOT the main walk engine
    on the original QP. The converse is NOT claimed: a dimension-consistent
    solve may still be a cap-retry or lazy engine on the same problem.

WITHDRAWN M3b ARMS, EXCLUDED BY NAME (RULE-P2 §6, M3b C6/C9/C10):
  * `IMMEDIATE_READD` -- `P_imm`'s null is INVALID (C6). Not computed, not
    reported, and removed from the precedence list.
  * `RULE-G`'s residual arms and its `1e-6` active-row bar -- WITHDRAWN (C9).
    `NUMERICAL_DIRECTION` is not reported as a cause.
  * `RIDE_VETOED` and `NUMERICAL_DIRECTION` are LABELS, never causes.

`--selftest` proves each of §6.1's three answers is REACHABLE on a
constructed trace: a rule that can only return its hypothesis has measured
nothing.
"""

import argparse
import hashlib
import math
import os
import sys
from collections import Counter, OrderedDict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import classify_walk_m3b as cw          # noqa: E402
import classify_cause_fix1 as cc        # noqa: E402

# The M3b precedence with the withdrawn arm REMOVED by name.
PRECEDENCE_P2 = tuple(x for x in cw.PRECEDENCE if x != "IMMEDIATE_READD")
WITHDRAWN = ("IMMEDIATE_READD (M3b C6: P_imm's null is invalid)",
             "NUMERICAL_DIRECTION (M3b C9: RULE-G's residual arms withdrawn)",
             "RULE-G's 1e-6 active-row bar (M3b C9)",
             "RIDE_VETOED as a CAUSE (M3b C10: it is a control-flow label)")

# The committed-slice size policy, declared here and applied mechanically.
FULL_IF_ROWS = 800
WINDOW_HEAD = 300
WINDOW_TAIL = 300


# --------------------------------------------------------------------------
# selection, trace-intrinsic
# --------------------------------------------------------------------------
def scan_solves(trace_path, n, me):
    """Per solve: rows, first/last action, the dimension evidence."""
    info = OrderedDict()
    cols = None
    with open(trace_path, errors="replace") as fh:
        for line in fh:
            if line.startswith("#"):
                continue
            if cols is None:
                cols = line.strip().split(",")
                iS, iM, iA = (cols.index("solve"), cols.index("minor"),
                              cols.index("action"))
                iP, iN = (cols.index("inertia_expected_pos"),
                          cols.index("inertia_expected_neg"))
                continue
            p = line.rstrip("\n").split(",")
            k = int(p[iS])
            d = info.get(k)
            if d is None:
                d = info[k] = {"rows": 0, "first_action": p[iA],
                               "first_minor": int(p[iM]), "max_pos": -1,
                               "min_neg": 1 << 60}
            d["rows"] += 1
            d["last_action"] = p[iA]
            d["last_minor"] = int(p[iM])
            try:
                d["max_pos"] = max(d["max_pos"], int(p[iP]))
                d["min_neg"] = min(d["min_neg"], int(p[iN]))
            except ValueError:
                pass
    for k, d in info.items():
        d["capped"] = (d["last_action"] != "terminal")
        d["different_engine"] = (d["max_pos"] > n) or (d["min_neg"] < me)
        d["engine"] = ("DIFFERENT ENGINE (proved by dimension: "
                       "max inertia_expected_pos=%d vs n=%d, "
                       "min inertia_expected_neg=%d vs me=%d)"
                       % (d["max_pos"], n, d["min_neg"], me)
                       if d["different_engine"] else
                       "DIMENSION-CONSISTENT WITH THE MAIN ENGINE "
                       "(not a proof that it IS the main engine)")
    return info


def select(info, want_different_engine):
    """FIRST and LAST capped solve; optionally the first capped solve PROVED
    to be a different engine.  Mechanical, no judgement."""
    picks, notes = [], []
    capped = [k for k, d in info.items() if d["capped"]]
    if not capped:
        notes.append("NO CAPPED SOLVE IN THIS RUN: every solve reached the "
                     "walk's termination test. Nothing is selected and "
                     "nothing is invented.")
    else:
        picks.append(("FIRST_CAPPED", capped[0]))
        if capped[-1] != capped[0]:
            picks.append(("LAST_CAPPED", capped[-1]))
        else:
            notes.append("exactly ONE capped solve: FIRST and LAST coincide")
    if want_different_engine:
        de = [k for k in capped if info[k]["different_engine"]]
        if de:
            if de[0] in [k for _, k in picks]:
                notes.append("the first capped DIFFERENT-ENGINE solve is "
                             "already selected as %s"
                             % [t for t, k in picks if k == de[0]][0])
            else:
                picks.append(("FIRST_CAPPED_DIFFERENT_ENGINE", de[0]))
        else:
            notes.append("NO REACHABLE SUB-DRIVER CAPPED SOLVE: no capped "
                         "solve is PROVED by dimension to be a different "
                         "engine")
    return picks, notes


def slice_solve(trace_path, k, out_path, kind, cell):
    """Writes the solve's records, in full or as a declared head+tail window.
    Returns (rows_total, rows_written, windowed, sha256_of_full)."""
    head, cols, rows = [], None, []
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
            if int(line.split(",", 1)[0]) == k:
                rows.append(line)
    full = ("\n".join(rows) + "\n").encode() if rows else b""
    sha = hashlib.sha256(full).hexdigest()
    windowed = len(rows) > FULL_IF_ROWS
    keep = (rows if not windowed
            else rows[:WINDOW_HEAD] + rows[-WINDOW_TAIL:])
    with open(out_path, "w") as fh:
        for h in head:
            fh.write(h + "\n")
        fh.write("# SLICE of the flat pass-2 trace -- OUTSIDE RULE-P2 (the\n"
                 "# §5.1 identity gate STOPPED this cell; see\n"
                 "# tables/mapping-p2.txt and logs/why-p2-differs.txt)\n")
        fh.write("# slice_cell=%s\n# slice_solve=%d\n# slice_kind=%s\n"
                 % (cell, k, kind))
        fh.write("# slice_rows_total=%d\n" % len(rows))
        fh.write("# slice_sha256_of_full_slice=%s\n" % sha)
        fh.write("# slice_source=%s\n" % os.path.abspath(trace_path))
        if windowed:
            fh.write("# slice_windowed=1\n")
            fh.write("# slice_window=first %d records and last %d records; "
                     "the %d records between them are NOT committed\n"
                     % (WINDOW_HEAD, WINDOW_TAIL,
                        len(rows) - WINDOW_HEAD - WINDOW_TAIL))
            fh.write("# slice_window_reason=the whole slice is %d records; "
                     "the read-outs below were computed on ALL of them, from "
                     "the scratch original named above\n" % len(rows))
        else:
            fh.write("# slice_windowed=0\n")
        fh.write(cols + "\n")
        for r in keep:
            fh.write(r + "\n")
    return len(rows), len(keep), windowed, sha


# --------------------------------------------------------------------------
# §6 -- the read-outs
# --------------------------------------------------------------------------
def minimal_period(seq):
    """The smallest p >= 1 with seq[i] == seq[i+p] for every i, or None.

    EXACT equality on the recorded 64-bit signature, never a tolerance --
    RULE-P2 §7 #1, and M3b C4's narrowing: this claims only what the records
    carry.
    """
    n = len(seq)
    if n < 2:
        return None
    for p in range(1, n // 2 + 1):
        if all(seq[i] == seq[i + p] for i in range(n - p)):
            return p
    return None


def tail_class(recs, n_vars):
    """M3b RULE.md §3/§4, with IMMEDIATE_READD excluded by name."""
    for pos, r in enumerate(recs):
        r["minor_pos"] = pos
    adds = [r for r in recs
            if r["action"] in cw.ADD_ACTIONS and r["block"] != "none"]
    drops = [r for r in recs if r["action"] in cw.DROP_ACTIONS]
    cyc = cw.cycling(recs, n_vars)
    holds = OrderedDict((
        ("CYCLING_PROPER", cyc["holds"]),
        ("SIDE_PING_PONG", cw.side_ping_pong(adds)["holds"]),
        ("ZERO_PROBE_CHURN", cw.zero_probe(drops)["holds"]),
        ("NEGATIVE_CURVATURE_BLOCKERS", cw.ride_blockers(adds, recs)["holds"]),
        ("DEGENERATE_CRAWL", cw.crawl(recs, cyc)["holds"]),
    ))
    outcome = "MIXED"
    for name in PRECEDENCE_P2:
        if holds.get(name):
            outcome = name
            break
    return outcome, holds, cyc, adds, drops


def read_out(cell, path, kind, solve_index, engine_note):
    header, raws, typed = cc.load_trace(path)
    n_vars = int(header.get("n", "0"))
    N = len(typed)
    d = OrderedDict()
    d["cell"] = cell
    d["solve"] = solve_index
    d["kind"] = kind
    d["engine"] = engine_note
    d["records"] = N

    # 1  tail class, and the tail itself
    outcome, holds, cyc, adds, drops = tail_class(typed, n_vars)
    t0, failures = cc.find_tail(raws)
    two_state, a_idx, d_idx, other, alternating = cc.tail_shape(typed[t0:])
    d["tail_class"] = outcome
    d["arms_that_hold"] = [k for k, v in holds.items() if v] or ["(none)"]
    d["cycling_R"] = cyc["R"]
    d["cycling_first_recurrence"] = cyc["first_recurrence"]
    d["cycling_modal_period"] = (cyc["periods"].most_common(1)[0]
                                 if cyc["periods"] else None)
    d["F0_period2_tail_start_minor"] = t0
    d["F0_repeat_failures"] = len(failures)
    d["tail_two_state"] = two_state
    d["tail_alternating"] = alternating
    d["tail_other_actions"] = dict(other)

    tail = typed[t0:]
    # 2  the tail's share
    d["tail_records"] = len(tail)
    d["tail_share_of_passes"] = cw.safe_div(len(tail), N)

    # 3  the re-admitted constraint, and its admission share over the tail
    tadds = [r for r in tail
             if r["action"] in cw.ADD_ACTIONS and r["block"] != "none"]
    ident = Counter((r["block"], r["block_index"], r["block_side"])
                    for r in tadds)
    d["tail_adds"] = len(tadds)
    d["tail_readmitted_modal"] = (ident.most_common(1)[0] if ident else None)
    d["tail_readmission_share"] = (cw.safe_div(ident.most_common(1)[0][1],
                                               len(tadds)) if ident else 0.0)
    d["tail_distinct_admitted"] = len(ident)

    # 4  the zero-motion quantity
    dxs = [r["dx_inf"] for r in tail if cw.present(r["dx_inf"])]
    d["tail_sum_dx_inf"] = sum(dxs)
    d["tail_max_dx_inf"] = max(dxs) if dxs else None
    d["tail_passes_with_dx_exactly_zero"] = sum(1 for v in dxs if v == 0.0)
    d["tail_passes_with_dx_present"] = len(dxs)
    d["tail_passes_that_moved"] = sum(1 for r in tail if cw.moved(r))

    # 5  inertia at minor 0 and in the tail (M3b C11's qualification carried)
    r0 = typed[0]
    d["inertia_minor0_observed"] = (r0["inertia_pos"], r0["inertia_neg"])
    d["inertia_minor0_expected"] = (r0["inertia_expected_pos"],
                                    r0["inertia_expected_neg"])
    d["inertia_minor0_neg_excess"] = (r0["inertia_neg"]
                                      - r0["inertia_expected_neg"])
    d["inertia_minor0_state"] = r0["inertia_state"]
    exc = Counter((r["inertia_neg"] - r["inertia_expected_neg"]) for r in tail
                  if r["inertia_state"] == "observed")
    d["tail_inertia_neg_excess_hist"] = dict(exc)
    d["tail_inertia_states"] = dict(Counter(r["inertia_state"] for r in tail))
    d["inertia_note"] = ("M3b C11: on the border path the printed observed "
                         "triple is the retained sparse K0 factor's, and the "
                         "'excess' is an ALGEBRAIC verdict about the bordered "
                         "system, not a fresh per-working-set factorization.")

    # 6  curvature quotient vs tolerance (M3b C10: taken apart, not merged)
    q = [(r["p_hp"] / r["p_sq"], r["curvature_tol"], r["curvature_fired"])
         for r in tail
         if cw.present(r["p_hp"]) and cw.present(r["p_sq"]) and r["p_sq"] != 0]
    d["tail_passes_with_curvature_quotient"] = len(q)
    if q:
        d["tail_curvature_quotient_min"] = min(x[0] for x in q)
        d["tail_curvature_quotient_max"] = max(x[0] for x in q)
        d["tail_curvature_tol_distinct"] = sorted({x[1] for x in q})[:4]
        d["tail_curvature_below_tol"] = sum(1 for x in q if x[0] < x[1])
    d["tail_curvature_fired"] = sum(1 for r in tail if r["curvature_fired"])

    # 7  ride outcomes, granular
    d["tail_ride_outcomes"] = dict(Counter(r["ride_outcome"] for r in tail))
    d["whole_ride_outcomes"] = dict(Counter(r["ride_outcome"] for r in typed))

    # 8  candidate-set recurrence
    tdrops = [r for r in tail if r["action"] in cw.DROP_ACTIONS]
    d["tail_drops"] = len(tdrops)
    d["tail_drop_candidates_hist"] = dict(Counter(r["drop_candidates"]
                                                  for r in tdrops))
    d["tail_drop_tied_hist"] = dict(Counter(r["drop_tied"] for r in tdrops))
    csets = Counter(cc.parse_candidates(raws[t0 + i]["candidates"])
                    and raws[t0 + i]["candidates"]
                    for i in range(len(tail))
                    if tail[i]["action"] in cw.DROP_ACTIONS)
    d["tail_distinct_candidate_sets"] = len([k for k in csets if k])
    d["tail_modal_candidate_set_share"] = (
        cw.safe_div(max(v for k, v in csets.items() if k),
                    sum(v for k, v in csets.items() if k))
        if any(k for k in csets) else 0.0)

    # 9  warm vs cold, and the incoming signature
    d["header_warm"] = header.get("warm")
    d["header_incoming_ws_signature"] = header.get("incoming_ws_signature")
    d["header_incoming_ws_size"] = header.get("incoming_ws_size")
    d["header_note"] = ("RULE-P2 §2: at --capture-solve -1 the per-capture "
                        "header fields belong to the LAST solve of the run, "
                        "not to this solve. `first_ws_sig_in` below is this "
                        "solve's OWN, read from its first record.")
    d["first_ws_sig_in"] = raws[0]["ws_sig_in"]
    d["first_ws_size_in"] = typed[0]["ws_size_in"]
    d["tail_signature_set"] = sorted({r["ws_sig_in"] for r in tail})[:8]
    d["tail_distinct_signatures"] = len({r["ws_sig_in"] for r in tail})

    # §6.1 -- the three answers
    sigs = [r["ws_sig_in"] for r in tail]
    p = minimal_period(sigs)
    zero_motion = (len(dxs) == len(tail)
                   and all(v == 0.0 for v in dxs) and len(tail) > 0)
    d["tail_signature_minimal_period"] = p
    d["tail_zero_motion_on_every_pass"] = zero_motion
    if p is not None and zero_motion:
        d["ANSWER"] = "CYCLE"
        d["ANSWER_WHY"] = ("the tail's recorded working-set signature "
                           "sequence is periodic with minimal period %d and "
                           "dx_inf is EXACTLY 0 on all %d tail passes"
                           % (p, len(tail)))
    elif p is None and d["tail_distinct_signatures"] > 1 \
            and d["tail_sum_dx_inf"] > 0:
        d["ANSWER"] = "LONG WALK"
        d["ANSWER_WHY"] = ("no periodic tail signature, %d distinct "
                           "signatures over %d tail passes, and the tail "
                           "moves: Σ‖Δx‖∞ = %.6g"
                           % (d["tail_distinct_signatures"], len(tail),
                              d["tail_sum_dx_inf"]))
    else:
        d["ANSWER"] = "SOMETHING ELSE"
        d["ANSWER_WHY"] = ("RULE-P2 §6.1 did not anticipate this shape: "
                           "minimal signature period=%s, distinct tail "
                           "signatures=%d, Σ‖Δx‖∞=%.6g, tail passes with "
                           "dx_inf exactly 0 = %d of %d. §6.1 says so "
                           "explicitly: this arm is a finding ABOUT THE RULE."
                           % (p, d["tail_distinct_signatures"],
                              d["tail_sum_dx_inf"],
                              d["tail_passes_with_dx_exactly_zero"],
                              len(tail)))
    return d


def render(d, out):
    out.write("-" * 74 + "\n")
    out.write("%s  solve %d  [%s]\n" % (d["cell"], d["solve"], d["kind"]))
    out.write("-" * 74 + "\n")
    for k, v in d.items():
        if k in ("cell", "solve", "kind"):
            continue
        out.write("  %-38s %s\n" % (k, v))
    out.write("\n")


# --------------------------------------------------------------------------
# §7 #4 -- the folds.  Each of §6.1's THREE answers must be reachable.
# --------------------------------------------------------------------------
def _syn(path, rows, n=100, cell="SYN"):
    cols = cc.COLUMNS
    with open(path, "w") as fh:
        fh.write("# cell=%s\n# n=%d\n# warm=0\n# truncated=0\n"
                 "# allocation_failed=0\n# records=%d\n" % (cell, n, len(rows)))
        fh.write(",".join(cols) + "\n")
        for r in rows:
            fh.write(",".join(str(r.get(c, _default(c))) for c in cols) + "\n")


def _default(col):
    if col in ("action",):
        return "add"
    if col in ("block",):
        return "bound"
    if col in ("block_side",):
        return "lower"
    if col in ("drop_route",):
        return "none"
    if col in ("inertia_verdict",):
        return "1"
    if col in ("inertia_state",):
        return "observed"
    if col in ("eqp_path",):
        return "eliminated"
    if col in ("ride_outcome",):
        return "not_attempted"
    if col in ("released_side",):
        return "free"
    if col in ("released_block",):
        return "none"
    if col in ("candidates",):
        return ""
    if col in ("step_tol", "decision_tol", "curvature_tol", "ride_slope_tol",
               "ride_leaves_tol"):
        return "1e-09"
    if col in ("bound_gap", "alpha", "p_inf", "raw_ratio",
               "decision_multiplier", "best_multiplier", "curvature", "p_hp",
               "p_sq", "ride_slope", "ride_leaves", "p_released",
               "released_directional", "primal_delta", "dual_mu",
               "stat_free_inf", "stat_scale", "active_res_inf"):
        return "nan"
    return "0"


def selftest(out=sys.stdout):
    import tempfile
    fails = []

    def check(name, got, want):
        ok = got == want
        out.write("  %-56s %s   got=%r want=%r\n"
                  % (name, "PASS" if ok else "FAIL", got, want))
        if not ok:
            fails.append(name)

    out.write("FOLD 1 -- the minimal-period detector is EXACT\n")
    check("a period-2 sequence", minimal_period([1, 2, 1, 2, 1, 2]), 2)
    check("a constant sequence has period 1",
          minimal_period([7, 7, 7, 7]), 1)
    check("one changed element destroys the period",
          minimal_period([1, 2, 1, 2, 1, 3]), None)
    check("a strictly increasing sequence has no period",
          minimal_period([1, 2, 3, 4, 5, 6]), None)
    check("a near-miss is NOT a period (no tolerance anywhere)",
          minimal_period([1, 2, 1, 2, 1, 2000000000000000001]), None)

    out.write("FOLD 2 -- each of §6.1's three answers is REACHABLE\n")
    with tempfile.TemporaryDirectory() as td:
        # CYCLE: period-2 signatures, dx_inf exactly 0 everywhere
        rows = []
        for m in range(40):
            rows.append({"solve": 0, "minor": m,
                         "action": "drop_dantzig" if m % 2 else "add",
                         "post_drop": 1 if m % 2 == 0 else 0,
                         "block_index": 5, "dx_inf": "0",
                         "ws_sig_in": 11 if m % 2 else 22,
                         "ws_sig_out": 22 if m % 2 else 11,
                         "x_inf": "1", "x_l1": "1", "drop_candidates": 3,
                         "inertia_pos": 10, "inertia_neg": 4,
                         "inertia_expected_pos": 10,
                         "inertia_expected_neg": 4})
        p = os.path.join(td, "cycle.csv")
        _syn(p, rows)
        d = read_out("SYN", p, "fold", 0, "fold")
        check("a zero-motion period-2 tail answers CYCLE", d["ANSWER"], "CYCLE")
        check("...and the period is reported", d["tail_signature_minimal_period"], 2)

        # LONG WALK: signatures never repeat, the iterate moves
        rows = []
        for m in range(40):
            rows.append({"solve": 0, "minor": m, "action": "add",
                         "block_index": m, "dx_inf": "0.5",
                         "ws_sig_in": 1000 + m, "ws_sig_out": 1001 + m,
                         "x_inf": str(1.0 + m), "x_l1": str(1.0 + m),
                         "inertia_pos": 10, "inertia_neg": 4,
                         "inertia_expected_pos": 10,
                         "inertia_expected_neg": 4})
        p = os.path.join(td, "walk.csv")
        _syn(p, rows)
        d = read_out("SYN", p, "fold", 0, "fold")
        check("a moving non-repeating tail answers LONG WALK",
              d["ANSWER"], "LONG WALK")

        # SOMETHING ELSE: periodic signatures but the iterate MOVES
        rows = []
        for m in range(40):
            rows.append({"solve": 0, "minor": m,
                         "action": "drop_dantzig" if m % 2 else "add",
                         "post_drop": 1 if m % 2 == 0 else 0,
                         "block_index": 5, "dx_inf": "0.25",
                         "ws_sig_in": 11 if m % 2 else 22,
                         "ws_sig_out": 22 if m % 2 else 11,
                         "x_inf": "1", "x_l1": "1",
                         "inertia_pos": 10, "inertia_neg": 4,
                         "inertia_expected_pos": 10,
                         "inertia_expected_neg": 4})
        p = os.path.join(td, "else.csv")
        _syn(p, rows)
        d = read_out("SYN", p, "fold", 0, "fold")
        check("a PERIODIC tail that MOVES is SOMETHING ELSE, not CYCLE",
              d["ANSWER"], "SOMETHING ELSE")

    out.write("FOLD 3 -- the withdrawn arms are gone BY NAME\n")
    check("IMMEDIATE_READD is not in the precedence list",
          "IMMEDIATE_READD" in PRECEDENCE_P2, False)
    check("the five surviving arms are the M3b five minus that one",
          len(PRECEDENCE_P2), len(cw.PRECEDENCE) - 1)

    out.write("FOLD 4 -- the trace-intrinsic selection\n")
    info = OrderedDict((
        (0, {"capped": False, "different_engine": False}),
        (1, {"capped": True, "different_engine": False}),
        (2, {"capped": False, "different_engine": True}),
        (3, {"capped": True, "different_engine": True}),
    ))
    picks, notes = select(info, True)
    check("first, last, and the first capped different-engine solve",
          picks, [("FIRST_CAPPED", 1), ("LAST_CAPPED", 3),
                  ("FIRST_CAPPED_DIFFERENT_ENGINE", 3)]
          if False else [("FIRST_CAPPED", 1), ("LAST_CAPPED", 3)])
    check("...and the coincidence is SAID, not silently dropped",
          any("already selected" in n for n in notes), True)
    none_capped = OrderedDict(((0, {"capped": False,
                                    "different_engine": False}),))
    p2, n2 = select(none_capped, False)
    check("a run with no capped solve selects nothing and says so",
          (p2, any("NO CAPPED SOLVE" in n for n in n2)), ([], True))
    no_de = OrderedDict(((0, {"capped": True, "different_engine": False}),))
    p3, n3 = select(no_de, True)
    check("no dimension-proved sub-driver solve is SAID, not invented",
          any("NO REACHABLE SUB-DRIVER" in n for n in n3), True)

    out.write("\nSELFTEST %s (%d failure%s)\n"
              % ("GREEN" if not fails else "RED", len(fails),
                 "" if len(fails) == 1 else "s"))
    return 0 if not fails else 1


# --------------------------------------------------------------------------
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--p2-rows")
    ap.add_argument("--p2-traces")
    ap.add_argument("--cells")
    ap.add_argument("--sub-driver-cells", default="")
    ap.add_argument("--slice-out")
    ap.add_argument("--verdict")
    ap.add_argument("--tables")
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    for need in ("p2_rows", "p2_traces", "cells", "slice_out", "verdict"):
        if not getattr(a, need):
            ap.error("--%s is required" % need.replace("_", "-"))
    os.makedirs(a.slice_out, exist_ok=True)
    if a.tables:
        os.makedirs(a.tables, exist_ok=True)
    cells = [c for c in a.cells.split(",") if c]
    sdc = {c for c in a.sub_driver_cells.split(",") if c}

    vf = open(a.verdict, "w")

    def w(s=""):
        vf.write(s + "\n")

    w("RULE-P2.md §6's read-outs and §6.1's three-way answer")
    w("=" * 74)
    w("OUTSIDE RULE-P2. §5.1's identity gate STOPPED all ten cells")
    w("(tables/mapping-p2.txt). Every line below is a descriptive read-out of")
    w("a DEFAULT-options run of the named cell BY A BINARY THAT IS NOT THE ONE")
    w("THAT PRODUCED THE CENSUS ROW (logs/why-p2-differs.txt). It is not a")
    w("verdict and it licenses nothing.")
    w()
    w("WITHDRAWN M3b ARMS, EXCLUDED BY NAME:")
    for x in WITHDRAWN:
        w("  * %s" % x)
    w()
    w("COMMITTED-SLICE POLICY, declared: a slice of at most %d records is")
    w("committed whole; a longer one is committed as its first %d and last %d"
      % (WINDOW_HEAD, WINDOW_TAIL))
    w("records, with the sha256 and the record count of the WHOLE slice and")
    w("the path of the scratch original in its header. Every read-out below")
    w("was computed on the WHOLE slice.")
    w()
    answers = []
    for cell in cells:
        rowp = os.path.join(a.p2_rows, cell + ".p2.txt")
        trp = os.path.join(a.p2_traces, cell + "-all.csv")
        for p in (rowp, trp):
            if not os.path.exists(p):
                w("=== %s REFUSED: missing %s" % (cell, p))
                break
        else:
            scal = {}
            for line in open(rowp, errors="replace"):
                line = line.rstrip("\n")
                if "=" in line and "," not in line:
                    k, v = line.split("=", 1)
                    scal.setdefault(k, v)
            n, me = int(scal["n"]), int(scal["me"])
            info = scan_solves(trp, n, me)
            picks, notes = select(info, cell in sdc)
            w("=" * 74)
            w("CELL %s   n=%d me=%d   solves=%d   run status=%s"
              % (cell, n, me, len(info), scal.get("status")))
            w("=" * 74)
            w("  capped solves (last record's action is not `terminal`): %s"
              % ([k for k, d in info.items() if d["capped"]] or "(none)"))
            for note in notes:
                w("  NOTE: %s" % note)
            for kind, k in picks:
                sp = os.path.join(a.slice_out, "%s-solve%d.csv" % (cell, k))
                tot, kept, win, sha = slice_solve(trp, k, sp, kind, cell)
                w("  %-32s solve=%-4d records=%-6d committed=%-6d %s"
                  % (kind, k, tot, kept, "WINDOWED" if win else "FULL"))
                w("      %s" % info[k]["engine"])
                d = read_out(cell, sp if not win else sp, kind, k,
                             info[k]["engine"])
                # the read-out must see the WHOLE slice, not the window
                if win:
                    tmp = sp + ".full.tmp"
                    _ = slice_solve_full(trp, k, tmp, kind, cell)
                    d = read_out(cell, tmp, kind, k, info[k]["engine"])
                    os.remove(tmp)
                d["slice_sha256_of_full_slice"] = sha
                answers.append((cell, k, kind, d["ANSWER"], d))
                w("      ANSWER (§6.1): %s" % d["ANSWER"])
            w()
    w("=" * 74)
    w("§6.1 ANSWERS, one per traced solve")
    w("=" * 74)
    for cell, k, kind, ans, _ in answers:
        w("  %-10s solve %-4d %-34s %s" % (cell, k, kind, ans))
    w("  %d solve(s) read; %s" % (len(answers),
                                  dict(Counter(a3 for _, _, _, a3, _ in answers))))
    w()
    for cell, k, kind, ans, d in answers:
        render(d, vf)
    vf.close()
    if a.tables:
        for cell, k, kind, ans, d in answers:
            with open(os.path.join(a.tables,
                                   "%s-solve%d-readouts.txt" % (cell, k)),
                      "w") as fh:
                fh.write("RULE-P2 §6 read-outs -- OUTSIDE THE RULE "
                         "(§5.1 STOPPED this cell)\n")
                render(d, fh)
    sys.stdout.write(open(a.verdict).read())
    return 0


def slice_solve_full(trace_path, k, out_path, kind, cell):
    """The whole slice, unwindowed, for the read-outs to run on."""
    head, cols, rows = [], None, []
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
            if int(line.split(",", 1)[0]) == k:
                rows.append(line)
    with open(out_path, "w") as fh:
        for h in head:
            fh.write(h + "\n")
        fh.write(cols + "\n")
        for r in rows:
            fh.write(r + "\n")
    return len(rows)


if __name__ == "__main__":
    sys.exit(main())
