#!/usr/bin/env python3
"""RULE-P2.md §6's read-outs and §6.1's three-way answer, over the PASS-2B
slices, COMPOSED from M3b's two classifiers with their withdrawn arms excluded
BY NAME.

PROVENANCE, first, because it is what makes this file readable at all.

The pass-2B re-take (chain pid 2743689) REPRODUCES the committed pass-1 row on
every compared field on all ten traced cells, with zero near-ulp acceptances
(`logs/gate-p2b.txt`), and `tables/mapping-p2b.txt` identifies every traced
solve against the pass-1 ledger under RULE-P2 §2's UNIQUE (minors,
factorizations) mapping. So unlike pass 2 -- which §5.1 stopped, and whose
read-outs were therefore never taken -- these traces ARE traces of the census
rows, and §6 may be read out on them under the rule rather than beside it.

WITHDRAWN M3b ARMS, EXCLUDED BY NAME (RULE-P2 §6; M3b Corrections C6/C9/C10):

  * `IMMEDIATE_READD` -- `P_imm`'s null is INVALID (C6: it assumed an
    independent/random Dantzig pick and used `n - me` as the candidate count;
    the pick is deterministic and the candidate set is the sign-violating
    subset). Removed from the precedence list BY NAME, not computed, not
    reported.
  * `RULE-G`'s RESIDUAL arms and its `1e-6` active-row bar -- WITHDRAWN (C9).
    `classify_cause_fix1`'s own `classify()` is never called; only its
    trace reader, its period-2 tail finder, its two-state shape test and its
    candidate parser are used, and none of those reads a residual.
    `NUMERICAL_DIRECTION` is never reported as a cause.
  * `RIDE_VETOED` and `NUMERICAL_DIRECTION` are LABELS, never causes (C9, C10).
    Ride outcomes are reported as §6 #7 asks -- as granular COUNTS.

THREE READINGS THIS FILE FIXES, because RULE-P2 does not fix them. Each is a
DEFECT of the rule, recorded here and in `tables/census.md`, and the rule's
bytes are NOT edited (RULE-P2 §7 #1, and RULE.md's own R1 precedent).

  D1 -- §6.1 says "a tail" and never defines one. The only pre-registered tail
     in the inherited instruments is RULE-G.md §3's, which is a PERIOD-2 repeat
     detector (`classify_cause_fix1.find_tail`): t0 minimal such that record m
     equals record m-2 on the F0 field set, bit for bit, for every later m.
     §6.1's own word is "PERIODIC", with no period named, so a period-2-only
     detector cannot answer the question §6.1 asks. THE TAIL USED HERE is the
     MAXIMAL EXACTLY-PERIODIC SUFFIX: for each candidate period p in
     [1, PERIOD_CAP] the minimal t0_p with record m == record m-p on F0 for
     every m in [t0_p+p, N), and the p maximising |T| = N - t0_p (ties broken
     toward the SMALLEST p). This is RULE-G's own construction with its 2
     replaced by a search, so it degenerates EXACTLY to RULE-G's tail when the
     winner is p = 2. RULE-G's own p=2 answer is printed beside it on every
     solve, so nothing is hidden by the generalisation.

  D2 -- §6.1's CYCLE conjunct is "`dx_inf == 0` on every tail pass". A DROP
     pass takes no step and the probe writes `nan` in `dx_inf` and `alpha`
     alike; on a drop/add cycle that is HALF the passes. Read strictly --
     every tail pass carries a PRESENT `dx_inf` equal to 0 -- the CYCLE arm is
     unreachable by construction on any real trace, which is a defect and not a
     measurement. THE READING APPLIED, declared: "exact zero motion on every
     tail pass" means NO tail pass moved the iterate -- no present `dx_inf` is
     non-zero and their sum is exactly 0 -- with the absent ones being passes
     that record no step at all. BOTH numbers are printed on every solve:
     `zero_motion_operative` and `zero_motion_strict`.
     IEEE note, carried because the bytes say it: the recorded value on these
     traces is `-0`, and `-0.0 == 0.0` is exact, not a tolerance.

  D3 -- §6.1's LONG WALK reads "no periodic tail, or a tail with non-zero
     motion: Sum||dx||inf > 0 over the tail and the working set does not
     repeat", which is ambiguous between a DISJUNCTIVE and a CONJUNCTIVE
     parse, and "the working set does not repeat" is not defined either. THE
     READING APPLIED: the CONJUNCTIVE one, with "does not repeat" = the tail's
     `ws_sig_in` sequence has NO minimal period -- the exact complement of
     CYCLE's "periodic", which is what makes the two arms disjoint. The
     DISJUNCTIVE parse's answer is printed beside it on every solve, and where
     the two differ the solve says so.

Nothing here is a verdict about hven, licenses a behaviour change, a default
change, or a line of code (RULE-P2 §0, §8).

`--selftest` proves each of §6.1's three answers is REACHABLE on a constructed
trace, and exercises D1/D2/D3's readings on inputs built to separate them: a
rule that can only return its hypothesis has measured nothing.
"""

import argparse
import math
import os
import sys
from collections import Counter, OrderedDict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import classify_cause_fix1 as cc        # noqa: E402
import classify_walk_m3b as cw          # noqa: E402

# D1's search bound. Declared, and every solve prints whether the winning
# period reached it (a winner AT the cap would mean the search was truncated).
PERIOD_CAP = 64

# The M3b walk precedence with the withdrawn arm REMOVED BY NAME (C6).
PRECEDENCE_P2 = tuple(x for x in cw.PRECEDENCE if x != "IMMEDIATE_READD")

WITHDRAWN = (
    "IMMEDIATE_READD  -- M3b C6: P_imm's null derivation is INVALID",
    "NUMERICAL_DIRECTION as a CAUSE -- M3b C9: RULE-G's residual arms withdrawn",
    "RULE-G's 1e-6 active-row bar -- M3b C9",
    "RIDE_VETOED as a CAUSE -- M3b C10: it is a control-flow LABEL",
)

INERTIA_NOTE = (
    "M3b C11: on the border path the printed OBSERVED triple is the retained "
    "sparse K0 factor's own inertia, and the dense Schur complement's current "
    "HAYNSWORTH contribution is BACKED OUT of the EXPECTED counts instead "
    "(qp_engine.cpp ~:1768) -- so the comparison is K0's numbers against K0's "
    "expectation, and a non-zero 'excess' away from minor 0 is an ALGEBRAIC "
    "verdict about the full bordered system, not a fresh per-working-set "
    "factorization. At minor 0, where border_dim is 0, no Haynsworth term "
    "exists and the comparison is a direct statement about the cold "
    "equality-constrained KKT."
)


# ---------------------------------------------------------------------------
# D1 -- the tail
# ---------------------------------------------------------------------------
def f0_keys(raws):
    """Each record's F0 state as a bit-exact tuple (cc.bitkey: %.17g floats
    packed back to their 8 bytes, every NaN to one canonical key)."""
    cols = cc.F0
    return [tuple(cc.bitkey(c, r[c]) for c in cols) for r in raws]


def periodic_suffix(keys, p):
    """RULE-G §3's construction at period p: t0 minimal such that
    keys[m] == keys[m-p] for every m in [t0+p, N).  Returns (t0, n_failures)."""
    n = len(keys)
    if n <= p:
        return n, 0
    fails = [m for m in range(p, n) if keys[m] != keys[m - p]]
    t0 = (fails[-1] - p + 1) if fails else 0
    return max(t0, 0), len(fails)


def maximal_periodic_tail(keys, cap=PERIOD_CAP):
    """D1.  Returns (period, t0, n_failures) for the p maximising |T|, ties to
    the SMALLEST p, requiring |T| > p so a 'tail' is at least one full cycle
    plus one repeat.  Returns (None, None, None) when no p qualifies."""
    n = len(keys)
    best = None
    for p in range(1, cap + 1):
        t0, nf = periodic_suffix(keys, p)
        length = n - t0
        if length <= p:
            continue
        if best is None or length > best[3]:
            best = (p, t0, nf, length)
    if best is None:
        return None, None, None
    return best[0], best[1], best[2]


def minimal_period(seq):
    """The smallest p >= 1 with seq[i] == seq[i+p] for every i, or None.
    EXACT equality on the recorded 64-bit signature, never a tolerance."""
    n = len(seq)
    if n < 2:
        return None
    for p in range(1, n // 2 + 1):
        if all(seq[i] == seq[i + p] for i in range(n - p)):
            return p
    return None


# ---------------------------------------------------------------------------
# §6 #1 -- M3b's walk arms, IMMEDIATE_READD excluded by name
# ---------------------------------------------------------------------------
def walk_arms(typed, n_vars):
    for pos, r in enumerate(typed):
        r["minor_pos"] = pos
    adds = [r for r in typed
            if r["action"] in cw.ADD_ACTIONS and r["block"] != "none"]
    drops = [r for r in typed if r["action"] in cw.DROP_ACTIONS]
    cyc = cw.cycling(typed, n_vars)
    spp = cw.side_ping_pong(adds)
    zpc = cw.zero_probe(drops)
    ncb = cw.ride_blockers(adds, typed)
    crw = cw.crawl(typed, cyc)
    holds = OrderedDict((
        ("CYCLING_PROPER", cyc["holds"]),
        ("SIDE_PING_PONG", spp["holds"]),
        ("ZERO_PROBE_CHURN", zpc["holds"]),
        ("NEGATIVE_CURVATURE_BLOCKERS", ncb["holds"]),
        ("DEGENERATE_CRAWL", crw["holds"]),
    ))
    outcome = "MIXED"
    for name in PRECEDENCE_P2:
        if holds.get(name):
            outcome = name
            break
    return outcome, holds, cyc, spp, zpc, ncb, crw, adds, drops


# ---------------------------------------------------------------------------
# the per-solve read-out
# ---------------------------------------------------------------------------
def read_out(cell, path, kind, solve_index, provenance):
    header, raws, typed = cc.load_trace(path)
    n_vars = int(header.get("n", "0"))
    N = len(typed)
    keys = f0_keys(raws)

    d = OrderedDict()
    d["cell"] = cell
    d["solve"] = solve_index
    d["kind"] = kind
    d["provenance"] = provenance
    d["records"] = N
    d["source_slice"] = os.path.basename(path)

    # --- D1: the tail -----------------------------------------------------
    p_star, t0, nfail = maximal_periodic_tail(keys)
    rg_t0, rg_fail = cc.find_tail(raws)
    d["RULEG_p2_tail_start_minor"] = rg_t0
    d["RULEG_p2_tail_records"] = N - rg_t0
    d["RULEG_p2_repeat_failures"] = rg_fail
    if p_star is None:
        d["tail_definition"] = ("NO EXACTLY-PERIODIC SUFFIX at any period "
                                "<= %d; the WHOLE solve is read as the tail "
                                "and the solve is labelled NO_PERIODIC_TAIL"
                                % PERIOD_CAP)
        d["tail_period"] = None
        d["tail_period_at_search_cap"] = False
        t0 = 0
    else:
        d["tail_definition"] = ("maximal exactly-periodic suffix on F0 "
                                "(D1), period %d" % p_star)
        d["tail_period"] = p_star
        d["tail_period_at_search_cap"] = (p_star == PERIOD_CAP)
        d["tail_F0_repeat_failures_at_that_period"] = nfail
    tail = typed[t0:]
    traw = raws[t0:]
    d["tail_start_minor"] = typed[t0]["minor"]
    d["tail_records"] = len(tail)
    d["tail_share_of_passes"] = cw.safe_div(len(tail), N)

    # --- §6 #1: the M3b walk arms, and the tail's signature cycle ---------
    outcome, holds, cyc, spp, zpc, ncb, crw, adds, drops = walk_arms(typed,
                                                                     n_vars)
    d["m3b_walk_class"] = outcome
    d["m3b_arms_that_hold"] = [k for k, v in holds.items() if v] or ["(none)"]
    d["m3b_arms_excluded_by_name"] = list(WITHDRAWN)
    d["m3b_cycling_R"] = cyc["R"]
    d["m3b_cycling_first_recurrence"] = cyc["first_recurrence"]
    d["m3b_cycling_modal_period"] = (cyc["periods"].most_common(1)[0]
                                     if cyc["periods"] else None)
    d["m3b_cycling_no_move_pairs"] = cyc["no_move_pairs"]
    d["m3b_crawl_distinct_signatures_over_N"] = (crw["D"], crw["G"])
    two_state, a_idx, dr_idx, other, alternating = cc.tail_shape(tail)
    d["tail_two_state_A_D"] = two_state
    d["tail_actions_alternate"] = alternating
    d["tail_actions"] = dict(Counter(r["action"] for r in tail))
    sig_seq = [r["ws_sig_in"] for r in tail]
    d["tail_signature_minimal_period"] = minimal_period(sig_seq)
    d["tail_distinct_signatures"] = len(set(sig_seq))
    if p_star is not None:
        d["tail_signature_cycle"] = [str(s) for s in sig_seq[:p_star]]
        d["tail_ws_size_cycle"] = [r["ws_size_in"] for r in tail[:p_star]]
        d["tail_action_cycle"] = [r["action"] for r in tail[:p_star]]
        d["tail_block_cycle"] = [(r["block"], r["block_index"],
                                  r["block_side"]) for r in tail[:p_star]]

    # --- §6 #3: the re-admitted constraint ---------------------------------
    tadds = [r for r in tail
             if r["action"] in cw.ADD_ACTIONS and r["block"] != "none"]
    ident = Counter((r["block"], r["block_index"], r["block_side"])
                    for r in tadds)
    d["tail_add_passes"] = len(tadds)
    d["tail_distinct_admitted_identities"] = len(ident)
    d["tail_readmitted_modal_identity"] = (ident.most_common(1)[0][0]
                                           if ident else None)
    d["tail_readmission_share"] = (cw.safe_div(ident.most_common(1)[0][1],
                                               len(tadds)) if ident else 0.0)
    d["tail_readmitted_top3"] = ident.most_common(3)

    # --- §6 #4: the zero-motion quantity -----------------------------------
    present_dx = [r["dx_inf"] for r in tail if cw.present(r["dx_inf"])]
    d["tail_sum_dx_inf"] = sum(present_dx)
    d["tail_max_dx_inf"] = max(present_dx) if present_dx else None
    d["tail_passes_dx_present"] = len(present_dx)
    d["tail_passes_dx_absent"] = len(tail) - len(present_dx)
    d["tail_passes_dx_exactly_zero"] = sum(1 for v in present_dx if v == 0.0)
    d["tail_passes_dx_nonzero"] = sum(1 for v in present_dx if v != 0.0)
    d["tail_passes_that_moved_m3b"] = sum(1 for r in tail if cw.moved(r))
    d["tail_dx_absent_actions"] = dict(Counter(
        r["action"] for r in tail if not cw.present(r["dx_inf"])))
    zero_strict = bool(tail) and len(present_dx) == len(tail) \
        and all(v == 0.0 for v in present_dx)
    zero_operative = bool(tail) and all(v == 0.0 for v in present_dx)
    d["zero_motion_strict"] = zero_strict
    d["zero_motion_operative"] = zero_operative

    # --- §6 #5: inertia ----------------------------------------------------
    r0 = typed[0]
    d["inertia_minor0_observed_triple"] = (r0["inertia_pos"], r0["inertia_neg"],
                                           r0["inertia_zero"])
    d["inertia_minor0_expected_pair"] = (r0["inertia_expected_pos"],
                                         r0["inertia_expected_neg"])
    d["inertia_minor0_excess_pos"] = r0["inertia_pos"] - r0["inertia_expected_pos"]
    d["inertia_minor0_excess_neg"] = r0["inertia_neg"] - r0["inertia_expected_neg"]
    d["inertia_minor0_state"] = r0["inertia_state"]
    d["inertia_minor0_verdict"] = r0["inertia_verdict"]
    d["inertia_minor0_border_dim"] = r0["border_dim"]
    d["inertia_minor0_haynsworth_present"] = (
        "NO  -- border_dim is 0 at minor 0, so no Schur/Haynsworth term "
        "exists and the comparison is a DIRECT statement about the cold "
        "equality-constrained KKT (M3b's own case)"
        if r0["border_dim"] == 0 else
        "YES -- border_dim is %d at minor 0 (this solve does NOT start cold), "
        "so the Haynsworth contribution is already backed out of the EXPECTED "
        "pair here too and the minor-0 comparison is an ALGEBRAIC verdict "
        "like the tail's, NOT M3b's direct cold-KKT statement"
        % r0["border_dim"])
    obs = [r for r in tail if r["inertia_state"] == "observed"]
    d["tail_inertia_states"] = dict(Counter(r["inertia_state"] for r in tail))
    d["tail_inertia_excess_neg_hist"] = dict(Counter(
        r["inertia_neg"] - r["inertia_expected_neg"] for r in obs))
    d["tail_inertia_excess_pos_hist"] = dict(Counter(
        r["inertia_pos"] - r["inertia_expected_pos"] for r in obs))
    d["tail_inertia_observed_triples"] = dict(Counter(
        (r["inertia_pos"], r["inertia_neg"], r["inertia_zero"]) for r in obs))
    d["tail_border_dim_hist"] = dict(Counter(r["border_dim"] for r in tail))
    d["inertia_note"] = INERTIA_NOTE

    # --- §6 #6: the curvature quotient vs its tolerance --------------------
    quot = [(r["p_hp"] / r["p_sq"], r["curvature_tol"])
            for r in tail
            if cw.present(r["p_hp"]) and cw.present(r["p_sq"])
            and r["p_sq"] != 0.0]
    d["tail_passes_with_curvature_quotient"] = len(quot)
    if quot:
        d["tail_curvature_quotient_min"] = min(q for q, _ in quot)
        d["tail_curvature_quotient_max"] = max(q for q, _ in quot)
        d["tail_curvature_tol_distinct"] = sorted({t for _, t in quot})[:4]
        d["tail_curvature_quotient_below_tol"] = sum(1 for q, t in quot
                                                     if q < t)
        d["tail_curvature_quotient_negative"] = sum(1 for q, _ in quot if q < 0)
    d["tail_curvature_fired"] = sum(1 for r in tail if r["curvature_fired"])
    d["whole_curvature_fired"] = sum(1 for r in typed if r["curvature_fired"])

    # --- §6 #7: ride outcomes, granular ------------------------------------
    d["tail_ride_outcomes"] = dict(Counter(r["ride_outcome"] for r in tail))
    d["whole_ride_outcomes"] = dict(Counter(r["ride_outcome"] for r in typed))
    d["whole_ride_attempts"] = sum(1 for r in typed if r["ride"])

    # --- §6 #8: candidate-set recurrence -----------------------------------
    tdrops = [r for r in tail if r["action"] in cw.DROP_ACTIONS]
    tdraw = [traw[i] for i, r in enumerate(tail)
             if r["action"] in cw.DROP_ACTIONS]
    d["tail_drop_passes"] = len(tdrops)
    d["tail_drop_candidates_hist"] = dict(Counter(r["drop_candidates"]
                                                  for r in tdrops))
    d["tail_drop_tied_hist"] = dict(Counter(r["drop_tied"] for r in tdrops))
    csets = Counter(r["candidates"] for r in tdraw if r["candidates"])
    d["tail_drop_passes_with_recorded_candidates"] = sum(csets.values())
    d["tail_distinct_candidate_sets"] = len(csets)
    d["tail_modal_candidate_set_share"] = (
        cw.safe_div(csets.most_common(1)[0][1], sum(csets.values()))
        if csets else 0.0)
    d["tail_modal_candidate_set_size"] = (
        len(cc.parse_candidates(csets.most_common(1)[0][0])) if csets else 0)

    # --- §6 #9: warm vs cold, and the incoming signature -------------------
    d["incoming_ws_sig_in_own"] = str(typed[0]["ws_sig_in"])
    d["incoming_ws_size_in_own"] = typed[0]["ws_size_in"]
    d["incoming_started_with_a_working_set"] = typed[0]["ws_size_in"] > 0
    d["header_warm_flag"] = header.get("warm")
    d["header_solve_index"] = header.get("solve_index")
    d["header_note"] = (
        "RULE-P2 §2's declared-absent list: at --capture-solve -1 the "
        "PER-CAPTURE header fields (warm, max_iter, incoming_ws_signature, "
        "exit_status) belong to the LAST solve of the run, not to this one. "
        "They are reported ONLY when header_solve_index equals this solve's "
        "index; the trace-intrinsic `incoming_ws_sig_in_own` above is this "
        "solve's own first record and is always its own.")
    d["header_fields_are_this_solves"] = (
        str(header.get("solve_index")) == str(solve_index))
    d["tail_signature_set_first8"] = [str(s) for s in
                                      sorted(set(sig_seq))[:8]]
    # The FULL sets, kept for §6 #9's comparison and NOT printed: an
    # eight-element sample would make a NON-EQUALITY unsound on a tail with
    # hundreds of distinct signatures, which is exactly STEENBRB solve 1.
    d["_tail_sig_full"] = {str(s) for s in sig_seq}
    d["_tail_ws_sizes_full"] = sorted({r["ws_size_in"] for r in tail})

    # --- OUTSIDE THE RULE ------------------------------------------------
    # RULE-P2 §6 pre-registers NINE read-outs and these are not among them.
    # They are the per-pass COST of a tail pass, which the rule never asked
    # for and which the traces make available for nothing. They are prefixed
    # so no reader can mistake them for a pre-registered quantity, and they
    # are reported in the census under a separated heading.
    d["OUTSIDE_RULE_note"] = (
        "the four fields below are NOT among RULE-P2 §6's nine "
        "pre-registered read-outs; they are reported as OUTSIDE THE RULE")
    d["OUTSIDE_RULE_tail_factorizations"] = sum(r["pass_factorizations"]
                                                for r in tail)
    d["OUTSIDE_RULE_tail_schur_updates"] = sum(r["pass_schur_updates"]
                                               for r in tail)
    d["OUTSIDE_RULE_tail_border_refine_steps"] = sum(
        r["pass_border_refine_steps"] for r in tail)
    d["OUTSIDE_RULE_tail_eqp_paths"] = dict(Counter(r["eqp_path"]
                                                    for r in tail))
    d["OUTSIDE_RULE_tail_border_latched"] = dict(Counter(
        r["border_latched"] for r in tail))
    d["OUTSIDE_RULE_tail_k0_rebuilt"] = dict(Counter(r["k0_rebuilt"]
                                                     for r in tail))

    # --- §6.1 -------------------------------------------------------------
    sig_p = d["tail_signature_minimal_period"]
    periodic = p_star is not None
    ws_repeats = sig_p is not None
    sum_dx = d["tail_sum_dx_inf"]
    if periodic and zero_operative:
        answer = "CYCLE"
        why = ("the tail is an EXACTLY periodic suffix of period %d on the F0 "
               "state (%d of %d passes, %.1f%%), its recorded working-set "
               "signature sequence has minimal period %s, and NO tail pass "
               "moved the iterate: every present dx_inf is exactly 0 and their "
               "sum is exactly 0 (%d passes record no dx at all, all of them "
               "%s)" % (p_star, len(tail), N, 100.0 * d["tail_share_of_passes"],
                        sig_p, d["tail_passes_dx_absent"],
                        ",".join(sorted(d["tail_dx_absent_actions"])) or "-"))
    elif (not periodic) and sum_dx > 0 and not ws_repeats:
        answer = "LONG WALK"
        why = ("no exactly-periodic suffix at any period <= %d, the tail's "
               "working-set signature sequence has no minimal period, and the "
               "tail MOVES: Sum||dx||inf = %.6g over %d passes, %d of which "
               "moved by M3b's own step_tol test"
               % (PERIOD_CAP, sum_dx, len(tail),
                  d["tail_passes_that_moved_m3b"]))
    else:
        answer = "SOMETHING ELSE"
        why = ("RULE-P2 §6.1 did not anticipate this shape, and §6.1 says "
               "explicitly that this arm is a finding ABOUT THE RULE. "
               "periodic suffix: %s (period %s); working-set signature "
               "minimal period: %s; Sum||dx||inf over the tail = %.6g; tail "
               "passes with dx_inf exactly 0: %d of %d present, %d absent; "
               "tail passes with dx_inf non-zero: %d"
               % (periodic, p_star, sig_p, sum_dx,
                  d["tail_passes_dx_exactly_zero"], d["tail_passes_dx_present"],
                  d["tail_passes_dx_absent"], d["tail_passes_dx_nonzero"]))
    d["ANSWER"] = answer
    d["ANSWER_WHY"] = why

    # D2/D3's alternative readings, printed on every solve.
    d["D2_answer_under_STRICT_zero_motion"] = (
        "CYCLE" if (periodic and zero_strict) else
        ("SOMETHING ELSE" if periodic else answer))
    d["D3_answer_under_DISJUNCTIVE_long_walk"] = (
        answer if answer != "SOMETHING ELSE" or periodic
        else "LONG WALK")
    d["D3_working_set_never_repeats_at_all"] = (
        d["tail_distinct_signatures"] == len(tail))
    return d


# ---------------------------------------------------------------------------
# §6 #9's cross-solve question
# ---------------------------------------------------------------------------
def cross_solve(per_cell):
    """Does a LATER capped solve START in the previous capped solve's cycle
    state?  Compared as an EQUALITY of recorded 64-bit values, with M3b C7's
    qualification carried."""
    out = []
    for cell, rows in per_cell.items():
        main = [d for d in rows if d["kind"].startswith(("FIRST_CAPPED",
                                                         "LAST_CAPPED"))]
        if len(main) < 2:
            out.append(OrderedDict((
                ("cell", cell),
                ("comparison", "DECLINED: only %d main-engine capped solve is "
                               "selected on this cell, so there is no LATER "
                               "solve to compare" % len(main)))))
            continue
        first, last = main[0], main[-1]
        sigset = first["_tail_sig_full"]
        out.append(OrderedDict((
            ("cell", cell),
            ("first_solve", first["solve"]),
            ("last_solve", last["solve"]),
            ("last_solve_incoming_ws_sig", last["incoming_ws_sig_in_own"]),
            ("last_solve_incoming_ws_size", last["incoming_ws_size_in_own"]),
            ("first_solve_tail_distinct_signatures", len(sigset)),
            ("first_solve_tail_signatures_first8",
             first["tail_signature_set_first8"]),
            ("first_solve_tail_ws_sizes", first["_tail_ws_sizes_full"][:12]),
            ("EQUALITY", "EQUAL -- the later solve STARTS in a signature the "
                         "earlier solve's cycle visits"
                         if last["incoming_ws_sig_in_own"] in sigset
                         else "NOT EQUAL -- the later solve's incoming "
                              "signature is none of ALL %d distinct signatures "
                              "the earlier solve's tail visits" % len(sigset)),
            ("note", "M3b C7: the SplitMix64 working-set signature's collision "
                     "figure is a uniform-hash HEURISTIC and not a bound, so "
                     "an EQUALITY is evidence of the same working set and not "
                     "a proof of it; a NON-EQUALITY is conclusive."),
        )))
    return out


# ---------------------------------------------------------------------------
# rendering
# ---------------------------------------------------------------------------
def render(d, out):
    out.write("### %s  solve %d  -- %s\n\n" % (d["cell"], d["solve"],
                                               d["kind"]))
    out.write("```\n")
    for k, v in d.items():
        if k in ("cell", "solve", "kind") or k.startswith("_"):
            continue
        if isinstance(v, list) and len(v) > 1 and k.startswith("m3b_arms_ex"):
            out.write("  %-42s\n" % k)
            for item in v:
                out.write("      %s\n" % item)
            continue
        out.write("  %-42s %s\n" % (k, v))
    out.write("```\n\n")


# ---------------------------------------------------------------------------
# the folds
# ---------------------------------------------------------------------------
def _synth(path, rows, n=10, me=4):
    """Writes a minimal trace carrying every column cc.load_trace requires."""
    cols = list(cc.COLUMNS)
    with open(path, "w") as fh:
        fh.write("# cell=SYNTH\n# n=%d\n# me=%d\n# solve_index=0\n"
                 "# warm=0\n# max_iter=%d\n" % (n, me, len(rows)))
        fh.write(",".join(cols) + "\n")
        for i, over in enumerate(rows):
            rec = {c: "0" for c in cols}
            for c in cc.FLOAT_COLS:
                rec[c] = "nan"
            rec["solve"] = "0"
            rec["minor"] = str(i)
            rec["action"] = "add"
            rec["block"] = "bound"
            rec["block_side"] = "lower"
            rec["drop_route"] = "none"
            rec["inertia_state"] = "observed"
            rec["inertia_verdict"] = "0"
            rec["eqp_path"] = "dense"
            rec["ride_outcome"] = "not_attempted"
            rec["candidates"] = ""
            rec["x_inf"] = "1"
            rec["x_l1"] = "1"
            rec.update(over)
            fh.write(",".join(rec[c] for c in cols) + "\n")


def selftest(out=sys.stdout):
    import tempfile
    fails = []

    def check(name, got, want):
        ok = got == want
        out.write("  %-64s %s   got=%r want=%r\n"
                  % (name, "PASS" if ok else "FAIL", got, want))
        if not ok:
            fails.append(name)

    with tempfile.TemporaryDirectory() as td:
        # --- FOLD 1: CYCLE is reachable -- a zero-motion period-2 tail -----
        rows = []
        for i in range(40):
            if i % 2 == 0:
                rows.append({"action": "drop_dantzig", "block_index": "8",
                             "drop_route": "dantzig", "ws_sig_in": "111",
                             "ws_size_in": "5", "ws_size_out": "4",
                             "drop_candidates": "4", "drop_tied": "1"})
            else:
                rows.append({"action": "add", "block_index": "8",
                             "dx_inf": "-0", "alpha": "-0", "step_tol": "1e-9",
                             "degenerate": "1", "post_drop": "1",
                             "ws_sig_in": "222", "ws_size_in": "4",
                             "ws_size_out": "5"})
        p = os.path.join(td, "cyc.csv")
        _synth(p, rows)
        d = read_out("SYNTH", p, "FOLD", 0, "constructed")
        check("FOLD 1  a zero-motion period-2 tail returns CYCLE",
              d["ANSWER"], "CYCLE")
        check("FOLD 1  ...with the period read off the F0 state",
              d["tail_period"], 2)
        check("FOLD 1  ...and STRICT zero motion does NOT hold (D2 is real)",
              d["zero_motion_strict"], False)
        check("FOLD 1  ...while the OPERATIVE reading does",
              d["zero_motion_operative"], True)
        check("FOLD 1  ...and D2's strict reading would have said otherwise",
              d["D2_answer_under_STRICT_zero_motion"], "SOMETHING ELSE")

        # --- FOLD 2: LONG WALK is reachable -------------------------------
        rows = [{"action": "add", "block_index": str(i), "dx_inf": "0.5",
                 "alpha": "0.5", "step_tol": "1e-9", "ws_sig_in": str(1000 + i),
                 "ws_size_in": str(i), "ws_size_out": str(i + 1),
                 "x_l1": str(1.0 + i)} for i in range(40)]
        p = os.path.join(td, "walk.csv")
        _synth(p, rows)
        d = read_out("SYNTH", p, "FOLD", 0, "constructed")
        check("FOLD 2  a moving, never-repeating tail returns LONG WALK",
              d["ANSWER"], "LONG WALK")
        check("FOLD 2  ...and no periodic suffix was found",
              d["tail_period"], None)
        check("FOLD 2  ...and the sum of motion is the sum of the steps",
              round(d["tail_sum_dx_inf"], 9), 20.0)

        # --- FOLD 3: SOMETHING ELSE is reachable: a MOVING exact cycle -----
        rows = []
        for i in range(42):
            k = i % 3
            if k == 0:
                rows.append({"action": "drop_dantzig", "block_index": "7",
                             "drop_route": "dantzig", "ws_sig_in": "111",
                             "ws_size_in": "5", "ws_size_out": "4"})
            elif k == 1:
                rows.append({"action": "add", "block_index": "7",
                             "dx_inf": "0", "alpha": "0", "step_tol": "1e-9",
                             "ws_sig_in": "222", "ws_size_in": "4",
                             "ws_size_out": "5"})
            else:
                rows.append({"action": "move_only", "block": "none",
                             "block_index": "-1", "block_side": "free",
                             "dx_inf": "0.001", "alpha": "0.001",
                             "step_tol": "1e-9", "ws_sig_in": "111",
                             "ws_size_in": "5", "ws_size_out": "5"})
        p = os.path.join(td, "movecyc.csv")
        _synth(p, rows)
        d = read_out("SYNTH", p, "FOLD", 0, "constructed")
        check("FOLD 3  an exactly periodic tail that MOVES returns SOMETHING ELSE",
              d["ANSWER"], "SOMETHING ELSE")
        check("FOLD 3  ...and its period is found although it is not 2",
              d["tail_period"], 3)
        check("FOLD 3  ...and RULE-G's period-2 finder sees no such tail",
              (N := d["records"]) - d["RULEG_p2_tail_records"] > 0, True)

        # --- FOLD 4: D1 -- the generalised finder degenerates to RULE-G's --
        check("FOLD 4  at a winning period of 2 the tail IS RULE-G's tail",
              (lambda: (lambda dd: (dd["tail_period"],
                                    dd["tail_start_minor"] ==
                                    dd["RULEG_p2_tail_start_minor"]))(
                  read_out("SYNTH", os.path.join(td, "cyc.csv"), "FOLD", 0,
                           "constructed")))(),
              (2, True))

        # --- FOLD 5: the withdrawn arms are gone BY NAME -------------------
        check("FOLD 5  IMMEDIATE_READD is absent from the precedence",
              "IMMEDIATE_READD" in PRECEDENCE_P2, False)
        check("FOLD 5  ...and the five surviving arms are M3b's own, in order",
              list(PRECEDENCE_P2),
              [a for a in cw.PRECEDENCE if a != "IMMEDIATE_READD"])
        # The needle is ASSEMBLED so the fold does not contain the token it
        # forbids -- a check that fails on its own text is not a check.
        needle = "cc" + "." + "classify" + "("
        check("FOLD 5  RULE-G's cause classifier is never called",
              needle in open(os.path.abspath(__file__)).read(), False)

        # --- FOLD 6: the tail is a SUFFIX and its share is a real fraction --
        d = read_out("SYNTH", os.path.join(td, "cyc.csv"), "FOLD", 0, "c")
        check("FOLD 6  the tail's share is records/N and never exceeds 1",
              d["tail_share_of_passes"] <= 1.0
              and d["tail_records"] <= d["records"], True)

        # --- FOLD 7: an empty candidate column yields no phantom sets ------
        check("FOLD 7  no recorded candidates means no distinct candidate set",
              d["tail_distinct_candidate_sets"], 0)

        # --- FOLD 8: the header is refused when it is another solve's ------
        check("FOLD 8  a header belonging to another solve is flagged as such",
              read_out("SYNTH", os.path.join(td, "cyc.csv"), "FOLD", 9,
                       "c")["header_fields_are_this_solves"], False)
        check("FOLD 8  ...and is accepted when it is this solve's",
              d["header_fields_are_this_solves"], True)

        # --- FOLD 9: the cross-solve comparison ----------------------------
        a = read_out("SYNTH", os.path.join(td, "cyc.csv"), "FIRST_CAPPED", 0, "c")
        b = read_out("SYNTH", os.path.join(td, "cyc.csv"), "LAST_CAPPED", 1, "c")
        xs = cross_solve({"SYNTH": [a, b]})
        check("FOLD 9  a later solve starting in the earlier cycle is EQUAL",
              xs[0]["EQUALITY"].startswith("EQUAL"), True)
        one = cross_solve({"SYNTH": [a]})
        check("FOLD 9  a cell with ONE capped solve DECLINES, never guesses",
              one[0]["comparison"].startswith("DECLINED"), True)

    out.write("\nSELFTEST %s (%d failure%s)\n"
              % ("GREEN" if not fails else "RED", len(fails),
                 "" if len(fails) == 1 else "s"))
    return 0 if not fails else 1


# ---------------------------------------------------------------------------
def parse_selection(path):
    """Reads tables/mapping-p2b.txt's own selection lines.  The selection is
    NOT retyped here: it is read back out of the artifact map_p2.py wrote."""
    sel, cell = OrderedDict(), None
    for line in open(path, errors="replace"):
        s = line.strip()
        if s.startswith("CELL "):
            cell = s.split()[1]
            sel.setdefault(cell, [])
            continue
        for kind in ("FIRST_CAPPED", "LAST_CAPPED", "FIRST_NOT_LEDGERED_CAPPED"):
            if s.startswith(kind) and " solve=" in s:
                k = int(s.split("solve=")[1].split()[0])
                why = s.split("solve=")[1].split(None, 1)
                sel[cell].append((kind, k, why[1] if len(why) > 1 else ""))
                break
    return sel


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--mapping", help="tables/mapping-p2b.txt")
    ap.add_argument("--slices", help="the directory of committed slices")
    ap.add_argument("--readouts-out")
    ap.add_argument("--verdict-out")
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    for need in ("mapping", "slices", "readouts_out", "verdict_out"):
        if not getattr(a, need):
            ap.error("--%s is required" % need.replace("_", "-"))

    sel = parse_selection(a.mapping)
    per_cell, order = OrderedDict(), []
    md = open(a.readouts_out, "w")
    md.write("# M7 W2 M3c — RULE-P2.md §6's read-outs over the PASS-2B traces\n\n")
    md.write("Generated by `wrappers/readouts_p2b.py`; **no solver ran here.**"
             " Every figure below is machine-written from the committed slices"
             " in `traces-p2b/`, whose parents' sha256 are in"
             " `traces-p2b/SHA256-all-traces-p2b.txt`. The selection is read"
             " back out of `tables/mapping-p2b.txt` rather than retyped.\n\n")
    md.write("The three readings this file fixes where RULE-P2 does not — D1"
             " (what a tail is), D2 (what an absent `dx_inf` is), D3 (how"
             " §6.1's LONG WALK clause parses) — are stated in full in the"
             " script's own docstring and recorded as defects OF THE RULE in"
             " `tables/census.md`. **The rule is not edited.**\n\n")
    md.write("Withdrawn M3b arms, excluded BY NAME before anything was read:\n\n")
    for w in WITHDRAWN:
        md.write("* `%s`\n" % w)
    md.write("\n---\n\n")

    for cell, picks in sel.items():
        if not picks:
            continue
        md.write("## %s\n\n" % cell)
        per_cell[cell] = []
        for kind, k, why in picks:
            path = os.path.join(a.slices, "%s-solve%d.csv" % (cell, k))
            if not os.path.exists(path):
                md.write("* `solve %d` (%s): REFUSED, no slice at %s\n"
                         % (k, kind, path))
                continue
            d = read_out(cell, path, kind, k, why)
            per_cell[cell].append(d)
            order.append(d)
            render(d, md)

    md.write("---\n\n## §6 #9's cross-solve question, per cell\n\n")
    md.write("**Does a LATER capped solve START in the previous capped solve's"
             " cycle state?**\n\n```\n")
    xs = cross_solve(per_cell)
    for row in xs:
        for k, v in row.items():
            md.write("  %-34s %s\n" % (k, v))
        md.write("\n")
    md.write("```\n")
    md.close()

    v = open(a.verdict_out, "w")
    v.write("M7 W2 M3c -- RULE-P2.md §6.1's THREE-WAY ANSWER, per TRACED SOLVE\n")
    v.write("generated by wrappers/readouts_p2b.py; no solver ran.\n")
    v.write("The full read-outs these answers are drawn from: "
            "tables/readouts-p2b.md\n\n")
    v.write("D1/D2/D3 -- the three readings RULE-P2 leaves open, and which one\n")
    v.write("this file applies -- are in the script's docstring and in\n")
    v.write("tables/census.md's findings-about-the-rule section. The rule is\n")
    v.write("NOT edited.\n\n")
    v.write("%-10s %-5s %-28s %-16s %-8s %-7s %-9s %s\n"
            % ("CELL", "SOLVE", "KIND", "ANSWER", "PERIOD", "|T|", "SHARE",
               "SUM|dx|inf"))
    v.write("-" * 108 + "\n")
    for d in order:
        v.write("%-10s %-5d %-28s %-16s %-8s %-7d %-9.3f %.6g\n"
                % (d["cell"], d["solve"], d["kind"], d["ANSWER"],
                   d["tail_period"], d["tail_records"],
                   d["tail_share_of_passes"], d["tail_sum_dx_inf"]))
    v.write("\nTOTALS: " + "   ".join(
        "%s %d" % (k, sum(1 for d in order if d["ANSWER"] == k))
        for k in ("CYCLE", "LONG WALK", "SOMETHING ELSE")) + "\n")
    v.write("\nPER-SOLVE REASONS\n")
    for d in order:
        v.write("\n  %s solve %d [%s] -> %s\n" % (d["cell"], d["solve"],
                                                  d["kind"], d["ANSWER"]))
        v.write("    %s\n" % d["ANSWER_WHY"])
        v.write("    under D2's STRICT zero-motion reading: %s\n"
                % d["D2_answer_under_STRICT_zero_motion"])
        v.write("    under D3's DISJUNCTIVE LONG WALK reading: %s\n"
                % d["D3_answer_under_DISJUNCTIVE_long_walk"])
    v.write("\n" + "-" * 74 + "\n")
    v.write("§6 #9's cross-solve question, per cell\n\n")
    for row in xs:
        for k, val in row.items():
            v.write("  %-34s %s\n" % (k, val))
        v.write("\n")
    v.write("Nothing above licenses a behaviour change, a default change, or a\n"
            "line of code (RULE-P2 §0, §8).\n")
    v.close()
    sys.stdout.write(open(a.verdict_out).read())
    return 0


if __name__ == "__main__":
    sys.exit(main())
