#!/usr/bin/env python3
"""M7 W2 M4c -- THE TRACE STUDY: at each tail drop, does the multiplier
contradict the post-drop direction, and by which of (a)-(e)?

READ-ONLY.  No solver runs here.  Every number is read out of a flat trace
already committed/pinned by M4 and M4b.  Nothing here is pre-registered: the
question is POST-HOC, asked by the M4c brief after M4b's chain had run.

WHAT IT DERIVES FROM, and what it leaves alone:
  * the OFF flat traces at .scratch/m7w2m4/arm/traces/off-<CELL>.csv -- M4's
    own, the walks of the very rows M4b compares against;
  * `classify_cause_fix1` (M3b's fix-round trace reader), imported UNMODIFIED
    for `F0`, `bitkey`, `load_trace` and `parse_candidates`;
  * `readouts_m4.maximal_periodic_tail`'s construction, TRANSCRIBED here (the
    module imports `classify_walk_m3b` at import time and pulls in machinery
    this study does not use); the transcription is byte-faithful to
    readouts_m4.py's `periodic_suffix`/`maximal_periodic_tail`, and the
    RECURRENT predicate is `readouts_driver_m4b.predicate`'s T1/T2/T3.

Usage: m4c_tails.py OUTDIR TRACE [TRACE ...]
"""

import json
import math
import os
import sys
from collections import OrderedDict

# [fix1] RELOCATABLE IMPORT of `classify_cause_fix1` (imported UNMODIFIED).
# The original line joined a repository-RELATIVE tail onto an ABSOLUTE
# directory and computed `<...>/docs/notes/data/docs/notes/data/<...>`, which
# exists nowhere; the module was only ever found because the author's working
# copy happened to sit beside it.  Two resolutions are tried, both derived from
# `__file__`: the SIBLING artifact directory under the same `docs/notes/data/`
# parent, then the same path under the repository root, found by walking up
# until a `.git` entry appears -- `os.path.exists`, not `isdir`, because that
# entry is a FILE in a git worktree and a DIRECTORY in a main checkout.  If
# neither carries the module the script REFUSES, naming every place it looked,
# rather than importing whatever happens to be on `sys.path`.
_HERE = os.path.dirname(os.path.abspath(__file__))
_M4B_REL = os.path.join("docs", "notes", "data",
                        "2026-09-m7-w2-m4b-expand-on-the-ten", "wrappers")


def _repo_root(start):
    """The nearest ancestor of `start` carrying a `.git` entry, or None."""
    d = start
    while True:
        if os.path.exists(os.path.join(d, ".git")):
            return d
        up = os.path.dirname(d)
        if up == d:
            return None
        d = up


_M4B_TRIED = [os.path.abspath(os.path.join(
    _HERE, "..", "..", "2026-09-m7-w2-m4b-expand-on-the-ten", "wrappers"))]
_ROOT = _repo_root(_HERE)
if _ROOT is not None:
    _M4B_TRIED.append(os.path.join(_ROOT, _M4B_REL))
for _cand in _M4B_TRIED:
    if os.path.isfile(os.path.join(_cand, "classify_cause_fix1.py")):
        sys.path.insert(0, _cand)
        break
else:
    sys.exit("m4c_tails.py: cannot find `classify_cause_fix1.py` (the trace "
             "reader this study imports unmodified).  Looked in:\n  "
             + "\n  ".join(_M4B_TRIED)
             + "\nRun this script from a checkout that still carries "
               "docs/notes/data/2026-09-m7-w2-m4b-expand-on-the-ten/wrappers/.")
import classify_cause_fix1 as cc  # noqa: E402

PERIOD_CAP = 64
DROP = ("drop_dantzig", "drop_zero_probe")
ADD = ("add",)
DENOM_TOL = 1e-12          # detail::kEngineDenomTol, qp_engine.h:417


# --- readouts_m4.py's tail construction, transcribed -----------------------
def periodic_suffix(keys, p):
    n = len(keys)
    if n <= p:
        return n, 0
    fails = [m for m in range(p, n) if keys[m] != keys[m - p]]
    t0 = (fails[-1] - p + 1) if fails else 0
    return max(t0, 0), len(fails)


def maximal_periodic_tail(keys, cap=PERIOD_CAP):
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


def fnum(t):
    try:
        v = float(t)
    except (TypeError, ValueError):
        return None
    return None if v != v else v


def unit_of(d_raw, a_raw, n, me):
    """One DROP/ADD tail unit: the drop pass D and the post-drop pass A."""
    u = OrderedDict()
    u["minor_D"] = int(d_raw["minor"])
    u["minor_A"] = int(a_raw["minor"])
    # --- D: what left, and on what multiplier --------------------------
    u["dropped"] = "%s:%s:%s" % (d_raw["block"], d_raw["block_index"],
                                 d_raw["block_side"])
    u["drop_route"] = d_raw["drop_route"]
    u["lambda_abs"] = fnum(d_raw["decision_multiplier"])
    u["decision_tol"] = fnum(d_raw["decision_tol"])
    u["best_multiplier"] = fnum(d_raw["best_multiplier"])
    u["drop_candidates"] = int(d_raw["drop_candidates"])
    u["drop_tied"] = int(d_raw["drop_tied"])
    u["candidates"] = d_raw["candidates"]
    u["bound_gap_D"] = fnum(d_raw["bound_gap"])
    u["ws_size_in_D"] = int(d_raw["ws_size_in"])
    u["ws_size_out_D"] = int(d_raw["ws_size_out"])
    u["p_inf_D"] = fnum(d_raw["p_inf"])
    u["step_tol_D"] = fnum(d_raw["step_tol"])
    u["x_inf_D"] = fnum(d_raw["x_inf"])
    # --- A: the direction computed after the drop ----------------------
    u["added"] = "%s:%s:%s" % (a_raw["block"], a_raw["block_index"],
                               a_raw["block_side"])
    u["add_route"] = a_raw["drop_route"]
    u["released"] = "%s:%s:%s" % (a_raw["released_block"],
                                  a_raw["released_index"],
                                  a_raw["released_side"])
    u["post_drop"] = a_raw["post_drop"]
    u["p_released"] = fnum(a_raw["p_released"])
    u["released_directional"] = fnum(a_raw["released_directional"])
    u["p_inf_A"] = fnum(a_raw["p_inf"])
    u["alpha"] = fnum(a_raw["alpha"])
    u["raw_ratio"] = fnum(a_raw["raw_ratio"])
    u["alpha_raw_text"] = a_raw["alpha"]
    u["ratio_tied"] = int(a_raw["ratio_tied"])
    u["dx_inf"] = fnum(a_raw["dx_inf"])
    u["degenerate"] = a_raw["degenerate"]
    u["ws_size_in_A"] = int(a_raw["ws_size_in"])
    u["ws_size_out_A"] = int(a_raw["ws_size_out"])
    u["ws_sig_in_A"] = a_raw["ws_sig_in"]
    # --- A: the inertia evidence ---------------------------------------
    u["inertia_verdict"] = int(a_raw["inertia_verdict"])
    u["inertia_state"] = a_raw["inertia_state"]
    u["inertia_pos"] = int(a_raw["inertia_pos"])
    u["inertia_neg"] = int(a_raw["inertia_neg"])
    u["inertia_zero"] = int(a_raw["inertia_zero"])
    u["inertia_zero_derived"] = a_raw["inertia_zero_derived"]
    u["inertia_perturbed"] = int(a_raw["inertia_perturbed"])
    u["inertia_expected_pos"] = int(a_raw["inertia_expected_pos"])
    u["inertia_expected_neg"] = int(a_raw["inertia_expected_neg"])
    u["neg_excess"] = (u["inertia_neg"] - u["inertia_expected_neg"]
                       if a_raw["inertia_state"] == "observed" else None)
    u["eqp_path"] = a_raw["eqp_path"]
    u["border_dim"] = int(a_raw["border_dim"])
    u["eqp_call_refine_steps"] = int(a_raw["eqp_call_refine_steps"])
    u["k0_rebuilt"] = a_raw["k0_rebuilt"]
    # --- A: curvature and the ride --------------------------------------
    u["curvature"] = fnum(a_raw["curvature"])
    u["curvature_tol"] = fnum(a_raw["curvature_tol"])
    u["p_hp"] = fnum(a_raw["p_hp"])
    u["p_sq"] = fnum(a_raw["p_sq"])
    u["curvature_fired"] = a_raw["curvature_fired"]
    u["ride_outcome"] = a_raw["ride_outcome"]
    u["ride_slope"] = fnum(a_raw["ride_slope"])
    u["ride_slope_tol"] = fnum(a_raw["ride_slope_tol"])
    u["ride_leaves"] = fnum(a_raw["ride_leaves"])
    u["ride_leaves_tol"] = fnum(a_raw["ride_leaves_tol"])
    # --- A: residuals and regularization ---------------------------------
    u["primal_delta"] = fnum(a_raw["primal_delta"])
    u["dual_mu"] = fnum(a_raw["dual_mu"])
    u["stat_free_inf"] = fnum(a_raw["stat_free_inf"])
    u["stat_scale"] = fnum(a_raw["stat_scale"])
    u["active_res_inf"] = fnum(a_raw["active_res_inf"])
    u["stat_free_inf_D"] = fnum(d_raw["stat_free_inf"])
    u["stat_scale_D"] = fnum(d_raw["stat_scale"])
    u["active_res_inf_D"] = fnum(d_raw["active_res_inf"])
    # --- derived, all arithmetic on the columns above --------------------
    u["kkt_rows_D"] = me + u["ws_size_in_D"]       # equality rows always in
    u["kkt_rows_A"] = me + u["ws_size_in_A"]
    u["n"] = n
    u["me"] = me
    lam, pr, pin = u["lambda_abs"], u["p_released"], u["p_inf_A"]
    u["lambda_over_tol"] = (lam / u["decision_tol"]
                            if lam is not None and u["decision_tol"] else None)
    u["lambda_over_stat_scale"] = (
        lam / max(u["stat_scale_D"], 1.0)
        if lam is not None and u["stat_scale_D"] is not None else None)
    u["abs_p_released"] = abs(pr) if pr is not None else None
    u["p_released_over_p_inf"] = (abs(pr) / pin
                                  if pr is not None and pin else None)
    u["res_over_x"] = (u["active_res_inf"] / max(u["x_inf_D"] or 1.0, 1.0)
                       if u["active_res_inf"] is not None else None)
    u["res_over_step_tol"] = (u["active_res_inf"] / u["step_tol_D"]
                              if u["active_res_inf"] is not None
                              and u["step_tol_D"] else None)
    # The dual regularization's own floor on how far the EQP "solution" can
    # sit off the rows it holds at equality: the (2,2) block is -mu*I, so an
    # active row is satisfied only to O(mu*|lambda|).
    u["res_over_mu_scale"] = (
        u["active_res_inf"] / (u["dual_mu"] * max(u["stat_scale_D"], 1.0))
        if u["active_res_inf"] is not None and u["dual_mu"]
        and u["stat_scale_D"] is not None else None)
    u["lambda_rel_eps"] = (
        lam / (2.220446049250313e-16 * max(u["stat_scale_D"], 1.0))
        if lam is not None and u["stat_scale_D"] is not None else None)
    u["contradiction"] = (u["released_directional"] is not None
                          and u["released_directional"] > 0.0)
    return u


# Fields whose constancy over the tail makes one representative unit a
# faithful sample of every unit in it.
HOMO = ("dropped", "added", "drop_route", "add_route", "lambda_abs",
        "p_released", "released_directional", "p_inf_A", "alpha_raw_text",
        "raw_ratio", "ratio_tied", "inertia_state", "inertia_pos",
        "inertia_neg", "inertia_expected_pos", "inertia_expected_neg",
        "eqp_path", "curvature", "curvature_fired", "ride_outcome",
        "ws_size_in_D", "ws_size_in_A", "drop_candidates", "drop_tied",
        "active_res_inf", "stat_free_inf", "p_hp", "p_sq")


EPS = 2.220446049250313e-16


def arms(u):
    """Which of (a)-(e)'s ARMS hold on this unit, independently of precedence.

    Every arm is a comparison of columns the record carries; the inputs are
    printed beside the verdict so a reader re-derives it rather than taking it.
    """
    h = OrderedDict()
    # (d) the released variable does not move: the ratio test's own eligibility
    #     threshold is |p_r| > kEngineDenomTol (qp_engine.cpp:2925/2930), so a
    #     blocker at all means |p_r| > 1e-12 -- the arm is falsifiable and is
    #     expected to be empty.
    h["d_abs"] = (u["abs_p_released"] is not None
                  and u["abs_p_released"] <= DENOM_TOL)
    # the drop bought NO direction at all: the post-drop EQP is a KKT point of
    # the REDUCED working set too, so the loop snaps and drops again.
    h["d_no_direction"] = (u["p_inf_A"] is not None and u["step_tol_D"]
                           is not None and u["p_inf_A"] <= u["step_tol_D"])
    h["d_rel"] = (u["p_released_over_p_inf"] is not None
                  and u["p_released_over_p_inf"] <= 1e-12)
    # (a) the dropped multiplier is at noise level -- ABSOLUTE (barely over
    #     opt_tol) or RELATIVE (below the rounding level of the gradient scale
    #     the multiplier was formed from).
    h["a_abs"] = (u["lambda_over_tol"] is not None
                  and u["lambda_over_tol"] < 10.0)
    h["a_rel"] = (u["lambda_rel_eps"] is not None and u["lambda_rel_eps"] < 1.0)
    # (b) the active set is PROVABLY dependent: more active rows (equalities +
    #     working rows + pinned bounds) than variables.
    h["b_dim_A"] = u["kkt_rows_A"] > u["n"]
    h["b_dim_D"] = u["kkt_rows_D"] > u["n"]
    h["b_perturbed"] = u["inertia_perturbed"] > 0
    # (c) the engine's own inertia gate says the system carries more negative
    #     directions than the working set accounts for.  `c_neg_excess` is the
    #     ONLY predicate of class (c) -- see `classify` below.
    h["c_neg_excess"] = u["neg_excess"] is not None and u["neg_excess"] > 0
    # [fix1] NOT a class predicate (review minor 3): a RECORDED DIAGNOSTIC
    # only.  It is the engine's own adverse verdict, and it fires on units
    # whose inertia excess is NEGATIVE (ACOPR57, ACOPP57), which REPORT.md
    # sec. 6 item 4 declines to interpret -- so it names no mechanism.  Making
    # it a class-(c) predicate would relabel ACOPP57's 1 777 residual units and
    # is refused.  The key keeps the `c_` spelling it was PINNED with in
    # `tails.json`; that prefix is a frozen artifact name, not a claim of class
    # membership.
    h["c_verdict_wrong"] = u["inertia_verdict"] == 2
    # (e) has NO predicate of its own: it is the RESIDUAL class, the
    #     unconditional fallback after (a)-(d) (see `classify`).  The two arms
    #     below are DESCRIPTIVE readings printed for the units that land there
    #     -- how far the iterate sits off the face its own EQP holds at
    #     equality, and whether the active set is exactly square.  Neither is
    #     required for the label, and neither says anything about the
    #     DIRECTION: every core unit's direction crosses the released bound
    #     (`released_directional > 0`, 100 051 / 100 051).
    h["e_off_face"] = (u["active_res_inf"] is not None
                       and u["step_tol_D"] is not None
                       and u["active_res_inf"] > 10.0 * u["step_tol_D"])
    h["e_square"] = u["kkt_rows_A"] == u["n"]
    return h


def classify(u):
    """THE CLASSIFICATION, applied mechanically in a DECLARED precedence.

    POST-HOC: the arms and the order were chosen after the distributions were
    seen.  Precedence d -> a -> b -> c -> e, with (b)'s dimension arm above
    (c) because it is an integer PROOF of dependency while (c) is the engine's
    own gate reading, which M3b C11 says is an algebraic verdict about the
    bordered system rather than a fresh per-working-set factorization.

    [fix1] (e) is the RESIDUAL class: it is returned UNCONDITIONALLY when no
    earlier arm holds, and neither `e_off_face` nor `e_square` is required for
    it.  Its label is "residual / unclassified after (a)-(d), often off the
    retained face" -- NOT a direction mechanism.  The only class predicate of
    (c) is `c_neg_excess`; `c_verdict_wrong` is a recorded diagnostic and
    participates in no class (review minor 3).
    """
    h = arms(u)
    if h["d_no_direction"]:
        return "d", h, ("||p||inf = %.3g <= step_tol %.3g: the drop bought no "
                        "direction" % (u["p_inf_A"], u["step_tol_D"]))
    if h["d_abs"] or h["d_rel"]:
        return "d", h, "|p_released| at rounding level"
    if h["a_abs"] or h["a_rel"]:
        return "a", h, ("|lambda|/opt_tol=%.3g, |lambda|/(eps*stat_scale)=%s"
                        % (u["lambda_over_tol"],
                           "%.3g" % u["lambda_rel_eps"]
                           if u["lambda_rel_eps"] is not None else "None"))
    if h["b_dim_A"]:
        return "b", h, ("me+|W| = %d > n = %d AFTER the drop"
                        % (u["kkt_rows_A"], u["n"]))
    if h["c_neg_excess"]:
        return "c", h, "neg_excess = %d" % u["neg_excess"]
    if h["b_perturbed"]:
        return "b", h, "perturbed pivots = %d" % u["inertia_perturbed"]
    return "e", h, ("off-face residual = %.3g vs step_tol %.3g"
                    % (u["active_res_inf"], u["step_tol_D"])
                    if h["e_off_face"] else "no arm holds")


def main():
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    allrows = []
    for path in sys.argv[2:]:
        header, raws, typed = cc.load_trace(path)
        cell = header.get("cell", os.path.basename(path))
        n = int(header["n"])
        me = int(header["me"])
        by = OrderedDict()
        for r in raws:
            by.setdefault(int(r["solve"]), []).append(r)
        for k, rs in by.items():
            keys = [tuple(cc.bitkey(c, r[c]) for c in cc.F0) for r in rs]
            p, t0, nfail = maximal_periodic_tail(keys)
            N = len(rs)
            if p is None:
                continue
            tl = N - t0
            rec = (tl >= 0.10 * N) and (tl >= 8 * p)
            if not rec:
                continue
            tail = rs[t0:]
            units = []
            # [fix1] A.action, carried BESIDE the unit rather than inside it:
            # the CORE predicate below now asserts it explicitly (review minor
            # 2) while the per-unit record `tails.json` was pinned with keeps
            # exactly the fields it had.
            add_actions = []
            for i in range(len(tail) - 1):
                if (tail[i]["action"] in DROP
                        and tail[i + 1]["post_drop"] == "1"):
                    units.append(unit_of(tail[i], tail[i + 1], n, me))
                    add_actions.append(tail[i + 1]["action"])
            if not units:
                allrows.append(OrderedDict(
                    cell=cell, solve=k, N=N, period=p, t0=t0, tail=tl,
                    units=0, note="NO drop->add unit inside the tail",
                    tail_actions=sorted(set(r["action"] for r in tail))))
                continue
            sig = set(tuple(str(u[f]) for f in HOMO) for u in units)
            # THE DECLARED REPRESENTATIVE: the LAST unit of the tail that is a
            # CORE unit (the post-drop pass re-adds exactly the identity the
            # drop released, through the ordinary ratio test) -- the object the
            # brief asks about -- falling back to the last unit when the tail
            # has no core unit at all.
            _core = [u for u, act in zip(units, add_actions)
                     if act in ADD and u["added"] == u["released"]
                     and u["add_route"] == "none"]
            rep = _core[-1] if _core else units[-1]
            cls, harms, why = classify(rep)
            row = OrderedDict(cell=cell, solve=k, N=N, period=p, t0=t0,
                              tail=tl, tail_share=tl / N, units=len(units),
                              distinct_unit_states=len(sig),
                              homogeneous=(len(sig) == 1),
                              classification=cls, why=why)
            row["arms_held"] = [k for k, v in harms.items() if v]
            row["tail_actions"] = sorted(set(r["action"] for r in tail))
            # every classification input on EVERY unit, so homogeneity of the
            # VERDICT is checked and not assumed from homogeneity of HOMO
            uc, ac = {}, {}
            for u in units:
                c, h, _ = classify(u)
                uc[c] = uc.get(c, 0) + 1
                for k, v in h.items():
                    if v:
                        ac[k] = ac.get(k, 0) + 1
            row["unit_classes"] = uc
            row["unit_arm_counts"] = ac
            # the CORE subset: the post-drop pass is an ADD (`A.action ==
            # "add"`, asserted rather than inferred from the blocker fields --
            # review minor 2) which immediately re-admits exactly the identity
            # the drop released, through the ordinary ratio test.
            core = [u for u, act in zip(units, add_actions)
                    if act in ADD and u["added"] == u["released"]
                    and u["add_route"] == "none"]
            row["core_readd_units"] = len(core)
            cc2 = {}
            for u in core:
                k = classify(u)[0]
                cc2[k] = cc2.get(k, 0) + 1
            row["core_unit_classes"] = cc2
            row["core_alpha_zero"] = sum(
                1 for u in core if u["alpha"] == 0.0)
            row["core_contradiction"] = sum(
                1 for u in core if u["contradiction"])
            row.update(("rep_" + kk, vv) for kk, vv in rep.items())
            allrows.append(row)
    with open(os.path.join(outdir, "tails.json"), "w") as fh:
        json.dump(allrows, fh, indent=1, default=str)
    print("solves with a RECURRENT tail: %d" % len(allrows))
    for r in allrows:
        print("%-10s s%-4d N=%-6d p=%-3s |T|=%-6d units=%-5d homo=%-5s %s  %s"
              % (r["cell"], r["solve"], r["N"], r["period"], r["tail"],
                 r.get("units", 0), r.get("homogeneous"),
                 r.get("classification", "-"), r.get("unit_classes", "")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
