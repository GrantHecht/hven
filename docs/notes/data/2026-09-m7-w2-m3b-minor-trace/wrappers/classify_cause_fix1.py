#!/usr/bin/env python3
"""M7 W2 M3b FIX ROUND 1 -- RULE-G.md's CAUSE classifier over the re-trace.

THIS FILE IMPLEMENTS RULE-G.md AS IT IS WRITTEN. RULE-G.md was committed at
`f916e82d`, BEFORE any leg of the re-trace ran, identity legs included. Nothing
here chooses a threshold: every bar is quoted from RULE-G.md with its section
number, and a bar that turns out to have been badly designed is DECLARED as a
defect of the rule -- never moved, and never with its outcome withdrawn
(RULE-G.md's own preamble requires exactly that).

  section 2  ABORT conditions, checked before any predicate
             2.1 identity gate   -> delegated to check_identity_m3b.py, the
                                    same checker the legs were gated with
             2.2 trace integrity -> delegated, PLUS `allocation_failed`, which
                                    RULE-G.md section 2.2 adds and the M3b
                                    checker (unedited) does not know about
             2.3 neutrality      -> the named fixtures must be `Passed` in BOTH
                                    committed suite transcripts
             2.4 no tail         -> ABORTED_NO_TAIL
  section 3  the tail, over the M3b field set F0, bit-exact, period 2
  section 4  the six outcomes, their predicates and their precedence
  section 5  homogeneity, and the FREQUENCY fallback at share 0.90
  section 6  what the classifier must refuse; `--selftest` fold-proves each
  section 7  the pre-registered descriptive read-outs, which are NOT verdicts

Everything this file prints under `OUTSIDE THE RULE` is descriptive, is marked
as such, was NOT pre-registered, and cannot change an outcome.

No outcome of this rule licenses a behaviour change, a default change or a line
of code (RULE-G.md section 0 and section 8).
"""
import argparse
import math
import os
import struct
import sys
import tempfile
from collections import Counter, OrderedDict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_identity_m3b as gate  # noqa: E402  (path must be set first)

# --- RULE-G.md's bars, quoted with their section numbers --------------------
G_TAIL_MIN = 100          # 3   |T| >= 100
G_TAIL_FRACTION = 0.50    # 3   |T| / N >= 0.50
G_RESIDUAL_BAR = 1e-6     # 4.4 RESIDUAL
G_REFINE_CAP = 10         # 4.4 REFINEMENT_SATURATED (detail::kMaxBorderRefineSteps)
G_SHARE_BAR = 0.90        # 5   the frequency form's share

PRECEDENCE = (            # 4, the causal order along a pass
    "INDEFINITE_IGNORED",
    "CURVATURE_GATE_SILENT",
    "RIDE_VETOED",
    "NUMERICAL_DIRECTION",
    "INERTIA_AS_EXPECTED_UNEXPLAINED",
    "MIXED",
)

VETO_OUTCOMES = ("nullspace_veto", "slope_flat_veto", "sign_disagreement_veto")

# RULE-G.md 2.3. The attached-vs-unattached neutrality fixtures and F1's
# concurrent fixtures, by name. DECLARED here, so a transcript that does not
# carry every one of them Passed is a refusal rather than a quiet pass.
NEUTRALITY_FIXTURES = (
    "QpWalkSeam.AttachingTheObserverChangesNoCounterAndNoAnswer",
    "QpWalkSeam.TheObserverIsNeutralOnAWarmRESOLVETOO",
    "QpWalkSeam.TwoEnginesOnTwoThreadsOneArmedOneNot",
    "QpWalkSeam.TwoEnginesOnTwoThreadsBothArmed",
    "QpWalkSeam.ArmingOnOneThreadIsInvisibleOnAnother",
)

# --- the trace's columns, by type -------------------------------------------
# THE ORDER IS THE PROBE'S HEADER ORDER (fix1_probe.cpp). F0 is derived from it
# below rather than written out twice.
COLUMNS = (
    "solve", "minor", "action", "block", "block_index", "block_side",
    "drop_route", "bound_gap", "alpha", "p_inf", "dx_inf", "step_tol",
    "degenerate", "ride", "raw_ratio", "ratio_tied", "ratio_capped",
    "decision_multiplier", "decision_tol", "best_multiplier",
    "drop_candidates", "drop_tied", "shift_adds", "ws_size_in", "ws_sig_in",
    "ws_size_out", "ws_sig_out", "x_inf", "x_l1",
    # ---- FIX ROUND 1's discriminating columns, appended never interleaved ---
    "inertia_verdict", "inertia_state", "inertia_pos", "inertia_neg",
    "inertia_zero", "inertia_zero_derived", "inertia_perturbed",
    "inertia_expected_pos", "inertia_expected_neg",
    "eqp_path", "border_attempted", "k0_rebuilt", "border_latched",
    "border_needs_refactorization", "border_dim", "eqp_call_refine_steps",
    "pass_factorizations", "pass_eqp_refine_steps", "pass_border_refine_steps",
    "pass_verdict_refine_steps", "pass_schur_updates",
    "post_drop", "curvature_fired", "curvature", "curvature_tol", "p_hp",
    "p_sq", "ride_outcome", "ride_sign_chosen", "ride_slope", "ride_slope_tol",
    "ride_leaves", "ride_leaves_tol",
    "released_block", "released_index", "released_side", "p_released",
    "released_directional", "candidates_recorded", "candidates",
    "primal_delta", "dual_mu", "stat_free_inf", "stat_scale", "active_res_inf",
)

# RULE-G.md 3: F0 is every member of QpWalkMinorRecord declared ABOVE the
# `FIX-ROUND 1` divider, MINUS `solve` and `minor`, which increment by
# construction. In the probe's header that is exactly columns 3..29.
F0 = COLUMNS[2:COLUMNS.index("inertia_verdict")]
FIX1_COLUMNS = COLUMNS[COLUMNS.index("inertia_verdict"):]

FLOAT_COLS = frozenset((
    "bound_gap", "alpha", "p_inf", "dx_inf", "step_tol", "raw_ratio",
    "decision_multiplier", "decision_tol", "best_multiplier", "x_inf", "x_l1",
    "curvature", "curvature_tol", "p_hp", "p_sq", "ride_slope",
    "ride_slope_tol", "ride_leaves", "ride_leaves_tol", "p_released",
    "released_directional", "primal_delta", "dual_mu", "stat_free_inf",
    "stat_scale", "active_res_inf",
))
INT_COLS = frozenset((
    "solve", "minor", "block_index", "ratio_tied", "drop_candidates",
    "drop_tied", "shift_adds", "ws_size_in", "ws_sig_in", "ws_size_out",
    "ws_sig_out", "inertia_verdict", "inertia_pos", "inertia_neg",
    "inertia_zero", "inertia_perturbed", "inertia_expected_pos",
    "inertia_expected_neg", "border_dim", "eqp_call_refine_steps",
    "pass_factorizations", "pass_eqp_refine_steps", "pass_border_refine_steps",
    "pass_verdict_refine_steps", "pass_schur_updates", "ride_sign_chosen",
    "released_index", "candidates_recorded",
))
BOOL_COLS = frozenset((
    "degenerate", "ride", "ratio_capped", "inertia_zero_derived",
    "border_attempted", "k0_rebuilt", "border_latched",
    "border_needs_refactorization", "post_drop", "curvature_fired",
))

# RULE-G.md section 6: "a trace whose header does not carry EVERY field
# section 4, section 5 and section 7 read". Declared, so a missing column is a
# refusal and never a silently skipped predicate.
REQUIRED_COLUMNS = tuple(COLUMNS)

# --- exit codes, one per refusal (RULE-G.md section 6) ----------------------
RC_OK = 0
RC_IDENTITY = 1           # 2.1 failed, or fewer than 21 fields compared
RC_ROW = 3                # an unreadable probe / M3a row
RC_FIELDS = 4             # the comparison would not cover the declared list
RC_INTEGRITY = 5          # 2.2, `allocation_failed` included
RC_TRACE = 6              # missing / empty / unparseable CSV, missing columns
RC_NO_TAIL = 7            # 2.4
RC_MULTIPLE = 8           # more than one outcome holds -- a defect of the rule
RC_NEUTRALITY = 9         # 2.3
RC_SELFTEST = 10


# ---------------------------------------------------------------------------
# Reading a trace
# ---------------------------------------------------------------------------
class TraceError(Exception):
    def __init__(self, rc, message):
        super().__init__(message)
        self.rc = rc
        self.message = message


def _f(text):
    try:
        return float(text)
    except ValueError:
        return float("nan")


def present(v):
    return v is not None and isinstance(v, float) and not math.isnan(v)


def bitkey(col, raw):
    """RULE-G.md section 3: floats compared by EXACT BIT PATTERN, NaN == NaN.

    The probe writes every float with `%.17g`, which round-trips a double
    exactly and is injective on doubles, so parsing and packing the 8 bytes
    back is the bit pattern the observer held. Every NaN maps to one canonical
    key, which is what "NaN equal to NaN" means, and an ABSENT value therefore
    matches an ABSENT value and never matches a zero (0.0 and -0.0 keep their
    distinct patterns).
    """
    if col in FLOAT_COLS:
        v = _f(raw)
        if math.isnan(v):
            return b"\xffNAN"
        return struct.pack("<d", v)
    return raw


def load_trace(path):
    """(header dict, [record dict of RAW strings], [record dict of TYPED])."""
    if not os.path.isfile(path):
        raise TraceError(RC_TRACE, "no trace at {}".format(path))
    header, cols, raws = {}, None, []
    with open(path, errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.startswith("#"):
                if "=" in line:
                    k, v = line[1:].strip().split("=", 1)
                    header[k] = v
                continue
            if not line:
                continue
            if cols is None:
                cols = line.split(",")
                continue
            cells = line.split(",")
            if len(cells) != len(cols):
                raise TraceError(RC_TRACE,
                                 "trace row {} has {} cells, the header has {}"
                                 .format(len(raws) + 1, len(cells), len(cols)))
            raws.append(dict(zip(cols, cells)))
    if cols is None:
        raise TraceError(RC_TRACE, "the trace at {} has no header row".format(path))
    if not raws:
        raise TraceError(RC_TRACE, "the trace at {} carries no records".format(path))
    missing = [c for c in REQUIRED_COLUMNS if c not in cols]
    if missing:
        raise TraceError(RC_TRACE,
                         "the trace header is missing {} field(s) the rule reads: {}"
                         .format(len(missing), ", ".join(missing)))
    typed = []
    for raw in raws:
        rec = {}
        for k, v in raw.items():
            if k in INT_COLS:
                try:
                    rec[k] = int(v)
                except ValueError:
                    raise TraceError(RC_TRACE,
                                     "column {} carries the unparseable integer {!r}"
                                     .format(k, v))
            elif k in FLOAT_COLS:
                rec[k] = _f(v)
            elif k in BOOL_COLS:
                rec[k] = (v == "1")
            else:
                rec[k] = v
        typed.append(rec)
    return header, raws, typed


# ---------------------------------------------------------------------------
# section 2 -- the ABORT conditions
# ---------------------------------------------------------------------------
def check_neutrality(paths, out):
    """RULE-G.md 2.3. FAIL-CLOSED: every declared fixture must appear `Passed`
    in EVERY transcript handed in (one per configuration)."""
    print("NEUTRALITY (RULE-G 2.3) -- the attached-vs-unattached and concurrent",
          file=out)
    print("  fixtures, required `Passed` in every configuration's transcript", file=out)
    if not paths:
        print("  REFUSED: no suite transcript was named", file=out)
        return RC_NEUTRALITY
    bad = []
    for p in paths:
        if not os.path.isfile(p):
            print("  REFUSED: no transcript at {}".format(p), file=out)
            return RC_NEUTRALITY
        with open(p, errors="replace") as fh:
            text = fh.read()
        for fx in NEUTRALITY_FIXTURES:
            ok = any(fx in line and "Passed" in line for line in text.splitlines())
            print("    {:<22} {:<58} {}".format(os.path.basename(p), fx,
                                                "Passed" if ok else "NOT FOUND Passed"),
                  file=out)
            if not ok:
                bad.append((os.path.basename(p), fx))
    if bad:
        print("  REFUSED (2.3): {} fixture/transcript pair(s) are not green -- a trace "
              "taken through an observer not proven neutral is not evidence"
              .format(len(bad)), file=out)
        return RC_NEUTRALITY
    print("  OK -- {} fixtures x {} transcript(s), all Passed"
          .format(len(NEUTRALITY_FIXTURES), len(paths)), file=out)
    return RC_OK


def check_aborts(trace_path, m3a_row, probe_row, header, typed, out):
    """2.1 and 2.2. Returns an exit code; 0 means no abort fired."""
    rc, compared = gate.check_identity(m3a_row, probe_row, out=out)
    if rc != 0:
        return RC_IDENTITY if rc == 1 else (RC_ROW if rc == 3 else RC_FIELDS)
    if compared < 21:
        print("  REFUSED (2.1): the comparison covered {} fields, fewer than the twenty "
              "QpCounters fields plus the status".format(compared), file=out)
        return RC_IDENTITY
    rc = gate.check_trace(trace_path, probe_row, out=out)
    if rc != 0:
        return RC_INTEGRITY
    # RULE-G.md 2.2 ADDS `allocation_failed`, which check_identity_m3b.py --
    # M3b's, unedited -- does not know about. A capture with it set is
    # INCOMPLETE: a record could not be stored because its allocation threw.
    af = header.get("allocation_failed")
    if af != "0":
        print("  PROBLEM: the observer reports allocation_failed={} -- the capture is "
              "incomplete".format(af), file=out)
        print("REFUSED (2.2): RULE-G.md section 2.2 ABORTS the leg", file=out)
        return RC_INTEGRITY
    print("  allocation_failed=0 (RULE-G 2.2's addition to the M3b checker's conditions)",
          file=out)
    unset = sum(1 for r in typed if r["action"] == "unset")
    if unset:
        print("REFUSED (2.2): {} record(s) carry the `unset` action".format(unset), file=out)
        return RC_INTEGRITY
    return RC_OK


# ---------------------------------------------------------------------------
# section 3 -- the tail
# ---------------------------------------------------------------------------
def find_tail(raws):
    """RULE-G.md 3. Returns (t0, R_failures) with t0 MINIMAL such that R(m)
    holds for every m in [t0+2, N)."""
    n = len(raws)
    failures = []
    for m in range(2, n):
        a, b = raws[m], raws[m - 2]
        if any(bitkey(c, a[c]) != bitkey(c, b[c]) for c in F0):
            failures.append(m)
    t0 = (failures[-1] - 1) if failures else 0
    return max(t0, 0), failures


def tail_shape(tail):
    """RULE-G.md 3's two states. Returns (is_two_state, A indices, D indices,
    other actions)."""
    a_idx, d_idx, other = [], [], Counter()
    for i, r in enumerate(tail):
        if r["action"] == "drop_dantzig":
            d_idx.append(i)
        elif r["action"] == "add" and r["post_drop"]:
            a_idx.append(i)
        else:
            other[(r["action"], "post_drop" if r["post_drop"] else "-")] += 1
    alternating = all(tail[i]["action"] != tail[i + 1]["action"] for i in range(len(tail) - 1))
    two_state = (not other) and alternating
    return two_state, a_idx, d_idx, other, alternating


# ---------------------------------------------------------------------------
# section 5 -- homogeneity
# ---------------------------------------------------------------------------
# The fields section 4 reads, named by RULE-G.md section 5. `p_hp` enters as
# PRESENCE, exactly as the rule writes it.
HOMOGENEITY_FIELDS = (
    "inertia_state", "inertia_pos", "inertia_neg", "inertia_expected_pos",
    "inertia_expected_neg", "inertia_zero", "inertia_zero_derived",
    "curvature_fired", "p_hp:present", "ride_outcome", "eqp_path",
    "eqp_call_refine_steps", "stat_free_inf", "stat_scale", "active_res_inf",
    "x_inf",
)


def hkey(raw, typed, field):
    if field == "p_hp:present":
        return "present" if present(typed["p_hp"]) else "absent"
    return bitkey(field, raw[field])


def homogeneity(raws, typed, idx):
    """Per field: (agrees, the number of distinct values)."""
    out = OrderedDict()
    for f in HOMOGENEITY_FIELDS:
        vals = {hkey(raws[i], typed[i], f) for i in idx}
        out[f] = (len(vals) <= 1, len(vals))
    return out


# ---------------------------------------------------------------------------
# section 4 -- the outcomes
# ---------------------------------------------------------------------------
def inertia_flags(r):
    observed = r["inertia_state"] == "observed"
    as_expected = observed and r["inertia_pos"] == r["inertia_expected_pos"] \
        and r["inertia_neg"] == r["inertia_expected_neg"]
    neg_excess = observed and r["inertia_neg"] > r["inertia_expected_neg"]
    adverse_other = observed and not as_expected and not neg_excess
    return {
        "observed": observed,
        "as_expected": as_expected,
        "neg_excess": neg_excess,
        "adverse_other": adverse_other,
        "unobserved": not observed,
    }


def sub_reasons(r):
    """RULE-G.md 4.4's four sub-reasons, on one state-A pass."""
    f = inertia_flags(r)
    stat_ratio = r["stat_free_inf"] / max(r["stat_scale"], 1.0) \
        if present(r["stat_free_inf"]) and present(r["stat_scale"]) else float("nan")
    act_ratio = r["active_res_inf"] / max(r["x_inf"], 1.0) \
        if present(r["active_res_inf"]) and present(r["x_inf"]) else float("nan")
    return OrderedDict((
        ("RESIDUAL", (present(stat_ratio) and stat_ratio > G_RESIDUAL_BAR)
                     or (present(act_ratio) and act_ratio > G_RESIDUAL_BAR)),
        ("REFINEMENT_SATURATED", r["eqp_path"] == "border_schur"
                                 and r["eqp_call_refine_steps"] >= G_REFINE_CAP),
        ("RANK_OR_ZERO", f["adverse_other"]),
        ("UNOBSERVED_INERTIA", f["unobserved"]),
    )), stat_ratio, act_ratio


def predicates_on_pass(r, no_escalation_in_tail):
    """RULE-G.md 4.1 - 4.5 on ONE state-A pass. MIXED is the residue and is not
    computed here."""
    f = inertia_flags(r)
    subs, _, _ = sub_reasons(r)
    p = OrderedDict()
    p["INDEFINITE_IGNORED"] = (
        f["neg_excess"]
        and not present(r["p_hp"]) and present(r["p_sq"])
        and r["ride_outcome"] == "not_attempted"
        and no_escalation_in_tail)
    p["CURVATURE_GATE_SILENT"] = (
        f["neg_excess"] and present(r["curvature"]) and not r["curvature_fired"])
    p["RIDE_VETOED"] = (
        f["neg_excess"] and r["curvature_fired"] and r["ride_outcome"] in VETO_OUTCOMES)
    p["NUMERICAL_DIRECTION"] = (not f["neg_excess"]) and any(subs.values())
    p["INERTIA_AS_EXPECTED_UNEXPLAINED"] = f["as_expected"] and not any(subs.values())
    return p


def classify(raws, typed, t0, a_idx, d_idx, two_state, homo_a, homo_d, out):
    """RULE-G.md section 4, in the form section 5 licenses. Returns a dict."""
    tail_typed = typed[t0:]
    escalation_actions = {"suspect_escalation", "start_repair", "post_probe_restart"}
    no_escalation = not any(r["action"] in escalation_actions for r in tail_typed)
    homogeneous = all(ok for ok, _ in homo_a.values()) and all(ok for ok, _ in homo_d.values())
    form = "STATE" if (two_state and homogeneous) else "FREQUENCY"

    a_recs = [tail_typed[i] for i in a_idx]
    result = {"form": form, "homogeneous": homogeneous, "two_state": two_state,
              "no_escalation_in_tail": no_escalation, "n_a": len(a_recs)}
    if not a_recs:
        result["holds"] = {}
        result["outcome"] = "MIXED"
        result["why"] = "the tail carries no state-A pass"
        return result

    if form == "STATE":
        rep = a_recs[0]
        p = predicates_on_pass(rep, no_escalation)
        holds = {k: bool(v) for k, v in p.items()}
        shares = {k: (1.0 if v else 0.0) for k, v in p.items()}
    else:
        per = [predicates_on_pass(r, no_escalation) for r in a_recs]
        shares = {k: sum(1 for d in per if d[k]) / float(len(per)) for k in per[0]}
        holds = {k: shares[k] >= G_SHARE_BAR for k in shares}
        # 4.3's dominant veto must itself carry share >= 0.90 in this form.
        if holds["RIDE_VETOED"]:
            modal = Counter(r["ride_outcome"] for r in a_recs).most_common(1)[0]
            if modal[1] / float(len(a_recs)) < G_SHARE_BAR or modal[0] not in VETO_OUTCOMES:
                holds["RIDE_VETOED"] = False
        rep = a_recs[0]

    winners = [k for k in PRECEDENCE[:-1] if holds.get(k)]
    if len(winners) > 1:
        print("  DEFECT OF THIS RULE: {} outcomes hold at once -- {}. RULE-G.md section 4's "
              "mutual exclusion is asserted, not assumed, and a violation is reported rather "
              "than resolved by taking the first.".format(len(winners), ", ".join(winners)),
              file=out)
        result["holds"] = holds
        result["shares"] = shares
        result["outcome"] = "MULTIPLE"
        result["winners"] = winners
        return result
    outcome = winners[0] if winners else "MIXED"
    subs, stat_ratio, act_ratio = sub_reasons(rep)
    result.update({
        "holds": holds, "shares": shares, "outcome": outcome, "rep": rep,
        "sub_reasons": subs, "stat_ratio": stat_ratio, "act_ratio": act_ratio,
        "inertia": inertia_flags(rep),
    })
    if outcome == "RIDE_VETOED":
        modal = Counter(r["ride_outcome"] for r in a_recs).most_common(1)[0]
        result["dominant_veto"] = modal[0]
        result["dominant_veto_share"] = modal[1] / float(len(a_recs))
        result["dominant_veto_count"] = modal[1]
    return result


# ---------------------------------------------------------------------------
# section 7 -- the descriptive read-outs (NOT verdicts)
# ---------------------------------------------------------------------------
def parse_candidates(raw):
    out = []
    for item in raw.split(";"):
        if not item:
            continue
        parts = item.rsplit(":", 1)
        if len(parts) != 2:
            out.append((item, float("nan")))
            continue
        out.append((parts[0], _f(parts[1])))
    return out


def fmt(v):
    if isinstance(v, float):
        if math.isnan(v):
            return "ABSENT(nan)"
        return "{:.17g}".format(v)
    return str(v)


def readouts(cell, raws, typed, t0, a_idx, d_idx, res, out):
    tail_raw, tail_typed = raws[t0:], typed[t0:]
    n = len(typed)
    A = [tail_typed[i] for i in a_idx]
    D = [tail_typed[i] for i in d_idx]
    a0 = A[0] if A else None
    d0 = D[0] if D else None
    p = lambda s="": print(s, file=out)

    p("-- section 7, the PRE-REGISTERED descriptive read-outs. None of these is a")
    p("   verdict and none can change an outcome (RULE-G.md section 7). ---------")
    p()
    p("  7.0 Do the fields FIX ROUND 1 ADDED also repeat with period 2 over T?")
    p("      (section 3 defines the tail on F0 alone precisely so that this is a")
    p("      finding and not an assumption.)")
    non_rep = []
    for c in FIX1_COLUMNS:
        ok = all(bitkey(c, tail_raw[m][c]) == bitkey(c, tail_raw[m - 2][c])
                 for m in range(2, len(tail_raw)))
        if not ok:
            non_rep.append(c)
    p("      {} of {} added fields repeat with period 2 over T.".format(
        len(FIX1_COLUMNS) - len(non_rep), len(FIX1_COLUMNS)))
    if non_rep:
        for c in non_rep:
            vals = Counter(tail_raw[i][c] for i in range(len(tail_raw)))
            p("      DOES NOT REPEAT: {:<28} distinct values over T: {}".format(
                c, dict(vals.most_common(6))))
    else:
        p("      Every one of them repeats.")
    p()
    p("  7.1 Inertia, on state A and state D, with the expectation beside it.")
    for name, r in (("A", a0), ("D", d0)):
        if r is None:
            p("      state {}: no pass".format(name))
            continue
        p("      state {}  observed ({}, {}, {})  expected ({}, {})  state={}  verdict={}"
          .format(name, r["inertia_pos"], r["inertia_neg"], r["inertia_zero"],
                  r["inertia_expected_pos"], r["inertia_expected_neg"],
                  r["inertia_state"], r["inertia_verdict"]))
        p("               zero is {}   perturbed_pivots={}   excess negative = {}"
          .format("DERIVED (dim - pos - neg; not an observation that there are none)"
                  if r["inertia_zero_derived"] else "NATIVELY REPORTED",
                  r["inertia_perturbed"], r["inertia_neg"] - r["inertia_expected_neg"]))
    p()
    p("  7.2 Curvature, on state A, and section 4.2's entailment check.")
    if a0 is not None:
        p("      p_hp={}  p_sq={}".format(fmt(a0["p_hp"]), fmt(a0["p_sq"])))
        p("      curvature={}  curvature_tol={}  fired={}".format(
            fmt(a0["curvature"]), fmt(a0["curvature_tol"]), a0["curvature_fired"]))
        if a0["curvature_fired"]:
            p("      the gate FIRED, so section 4.2's entailment has no antecedent here")
        else:
            lhs = a0["p_hp"]
            rhs = (a0["curvature_tol"] * a0["p_sq"]
                   if present(a0["curvature_tol"]) and present(a0["p_sq"]) else float("nan"))
            p("      ENTAILMENT (4.2): fired==False must give p_hp > curvature_tol*p_sq: "
              "{} > {} is {}".format(fmt(lhs), fmt(rhs),
                                     (present(lhs) and present(rhs) and lhs > rhs)))
    p()
    p("  7.3 The released constraint, on state A.")
    if a0 is not None:
        pr = a0["p_released"]
        p("      released {}:{}:{}".format(a0["released_block"], a0["released_index"],
                                           a0["released_side"]))
        p("      p[released] sign {}   magnitude {}".format(
            ("negative" if pr < 0 else "positive") if present(pr) else "ABSENT",
            fmt(abs(pr)) if present(pr) else "ABSENT"))
        p("      released_directional = {}".format(fmt(a0["released_directional"])))
    p()
    p("  7.4 / 7.5 The slope, the four numbers, and the ride outcome by name.")
    if a0 is not None:
        p("      ride_outcome     = {}".format(a0["ride_outcome"]))
        p("      ride_sign_chosen = {}".format(a0["ride_sign_chosen"]))
        p("      slope  (g'p) = {:<26} slope_tol  = {}".format(
            fmt(a0["ride_slope"]), fmt(a0["ride_slope_tol"])))
        p("      leaves       = {:<26} leaves_tol = {}".format(
            fmt(a0["ride_leaves"]), fmt(a0["ride_leaves_tol"])))
        if present(a0["ride_slope"]) and present(a0["ride_slope_tol"]):
            sign = 1 if a0["ride_slope"] < -a0["ride_slope_tol"] else (
                -1 if a0["ride_slope"] > a0["ride_slope_tol"] else 0)
            p("      which comparison decided it: slope vs slope_tol gives the descending "
              "sign {}; ".format(sign if sign else "NONE (flat)"))
            if sign and present(a0["ride_leaves"]) and present(a0["ride_leaves_tol"]):
                p("      then sign*leaves = {} against -leaves_tol = {} -> {}".format(
                    fmt(sign * a0["ride_leaves"]), fmt(-a0["ride_leaves_tol"]),
                    "FEASIBILITY VETO" if sign * a0["ride_leaves"] < -a0["ride_leaves_tol"]
                    else "admissible"))
        p("      the ride outcome over ALL state-A passes: {}".format(
            dict(Counter(r["ride_outcome"] for r in A))))
    p()
    p("  7.6 The linear algebra, on both states.")
    for name, r in (("A", a0), ("D", d0)):
        if r is None:
            continue
        p("      state {}  eqp_path={}  border_attempted={}  border_dim={}  "
          "eqp_call_refine_steps={}".format(name, r["eqp_path"], r["border_attempted"],
                                            r["border_dim"], r["eqp_call_refine_steps"]))
        p("               k0_rebuilt={}  border_latched={}  needs_refactorization={}".format(
            r["k0_rebuilt"], r["border_latched"], r["border_needs_refactorization"]))
        p("               per-pass counters: factorizations={} eqp_refine={} border_refine={} "
          "verdict_refine={} schur_updates={}".format(
              r["pass_factorizations"], r["pass_eqp_refine_steps"],
              r["pass_border_refine_steps"], r["pass_verdict_refine_steps"],
              r["pass_schur_updates"]))
    p()
    p("  7.7 Residuals and the EFFECTIVE regularization, on both states.")
    for name, r in (("A", a0), ("D", d0)):
        if r is None:
            continue
        sr = r["stat_free_inf"] / max(r["stat_scale"], 1.0)
        ar = r["active_res_inf"] / max(r["x_inf"], 1.0)
        p("      state {}  stat_free_inf={:<24} stat_scale={:<24} ratio={:.6g}".format(
            name, fmt(r["stat_free_inf"]), fmt(r["stat_scale"]), sr))
        p("               active_res_inf={:<23} x_inf={:<27} ratio={:.6g}".format(
            fmt(r["active_res_inf"]), fmt(r["x_inf"]), ar))
        p("               primal_delta={}  dual_mu={}  (EFFECTIVE)".format(
            fmt(r["primal_delta"]), fmt(r["dual_mu"])))
    p()
    p("  7.8 The drop rule's candidate set, on state D.")
    if d0 is not None:
        cands = parse_candidates(d0["candidates"])
        p("      drop_candidates={}  drop_tied={}  candidates_recorded={}".format(
            d0["drop_candidates"], d0["drop_tied"], d0["candidates_recorded"]))
        p("      decision_multiplier={}  best_multiplier={}".format(
            fmt(d0["decision_multiplier"]), fmt(d0["best_multiplier"])))
        chosen = "{}:{}:{}".format(d0["block"], d0["block_index"], d0["block_side"])
        rank = None
        for i, (ident, viol) in enumerate(cands):
            mark = ""
            if ident == chosen and rank is None:
                rank = i + 1
                mark = "   <- CHOSEN"
            p("        rank {:<2} {:<24} violation {}{}".format(i + 1, ident, fmt(viol), mark))
        p("      the chosen candidate's RANK BY VIOLATION: {} (the Dantzig rule makes this 1 "
          "by construction -- reported so that it is checked, not assumed)".format(
              rank if rank else "NOT AMONG THE RECORDED CANDIDATES"))
        p("      ABSENT, restated on the spot (RULE-G.md section 0): no post-drop inertia is")
        p("      predicted for any candidate, and no rank evidence exists for the released")
        p("      bound. Nothing here adjudicates the review's cause (1); it describes it.")
    p()
    p("  7.9 The tail itself.")
    p("      t0={}  |T|={}  |T|/N={:.6f}  N={}".format(t0, len(tail_typed),
                                                       len(tail_typed) / float(n), n))
    p("      action histogram over T: {}".format(
        dict(Counter((r["action"], "post_drop" if r["post_drop"] else "-")
                     for r in tail_typed))))
    p("      period 2, two-state shape: {}   state A passes {}   state D passes {}".format(
        res["two_state"], len(A), len(D)))
    if a0 is not None:
        p("      the repeated identity: state A re-admits {}:{}:{} with alpha={} "
          "(ws_sig {} -> {})".format(a0["block"], a0["block_index"], a0["block_side"],
                                     fmt(a0["alpha"]), a0["ws_sig_in"], a0["ws_sig_out"]))
    if d0 is not None:
        p("                            state D releases {}:{}:{} (ws_sig {} -> {})".format(
            d0["block"], d0["block_index"], d0["block_side"], d0["ws_sig_in"],
            d0["ws_sig_out"]))
    p("      section 5 homogeneity, field by field:")
    for name, h in (("A", res["homo_a"]), ("D", res["homo_d"])):
        for f, (ok, k) in h.items():
            if not ok:
                p("        state {}  {:<24} NOT HOMOGENEOUS ({} distinct values)".format(
                    name, f, k))
    p("        state A: {} of {} fields homogeneous".format(
        sum(1 for ok, _ in res["homo_a"].values() if ok), len(HOMOGENEITY_FIELDS)))
    p("        state D: {} of {} fields homogeneous".format(
        sum(1 for ok, _ in res["homo_d"].values() if ok), len(HOMOGENEITY_FIELDS)))


# ---------------------------------------------------------------------------
# OUTSIDE THE RULE -- descriptive, NOT pre-registered, cannot change an outcome
# ---------------------------------------------------------------------------
def outside_the_rule(cell, raws, typed, t0, a_idx, d_idx, out):
    p = lambda s="": print(s, file=out)
    n = len(typed)
    tail = typed[t0:]
    A = [tail[i] for i in a_idx]
    D = [tail[i] for i in d_idx]
    p("=" * 78)
    p("OUTSIDE THE RULE -- {}".format(cell))
    p("NOT pre-registered in RULE-G.md, NOT a verdict, and incapable of changing")
    p("one. These are read-outs the settler asked for in order to RANK ENGINE")
    p("WORK, and they are reported here, separately, for exactly that.")
    p("=" * 78)

    p()
    p("(a) THE EXCESS NEGATIVE EIGENVALUE COUNT OVER THE WHOLE WALK")
    p("    excess := inertia_neg - inertia_expected_neg, on the passes where the")
    p("    backend observed an inertia. What the engine compares is NOT the")
    p("    current reduced Hessian's own triple: on the border path it is K0's")
    p("    backend triple against a structural expectation the Schur complement's")
    p("    Haynsworth contribution has been backed out of (qp_engine.cpp,")
    p("    border_inertia_verdict). The excess below is therefore the engine's own")
    p("    indefiniteness signal, read exactly as the engine reads it.")
    obs = [r for r in typed if r["inertia_state"] == "observed"]
    exc = Counter(r["inertia_neg"] - r["inertia_expected_neg"] for r in obs)
    r0 = typed[0]
    p("    at minor 0     action={:<14} ws_size_in={:<3} border_dim={:<3} observed "
      "({}, {})  expected ({}, {})  excess {}".format(
          r0["action"], r0["ws_size_in"], r0["border_dim"], r0["inertia_pos"],
          r0["inertia_neg"], r0["inertia_expected_pos"], r0["inertia_expected_neg"],
          r0["inertia_neg"] - r0["inertia_expected_neg"]))
    fd = next((r for r in typed if r["action"].startswith("drop")), None)
    if fd is not None:
        p("    at the FIRST drop (minor {})  ws_size_in={:<3} observed ({}, {})  expected "
          "({}, {})  excess {}".format(
              fd["minor"], fd["ws_size_in"], fd["inertia_pos"], fd["inertia_neg"],
              fd["inertia_expected_pos"], fd["inertia_expected_neg"],
              fd["inertia_neg"] - fd["inertia_expected_neg"]))
    if A:
        p("    in the TAIL   state A excess {}   state D excess {}".format(
            A[0]["inertia_neg"] - A[0]["inertia_expected_neg"],
            D[0]["inertia_neg"] - D[0]["inertia_expected_neg"] if D else "-"))
    p("    histogram over the whole walk: {}".format(dict(sorted(exc.items()))))
    p("    unobserved passes: {}".format(len(typed) - len(obs)))
    p("    the observed triple over the whole walk: {}".format(
        dict(Counter((r["inertia_pos"], r["inertia_neg"]) for r in obs).most_common(6))))
    p("    the walk's first 8 passes, expectation against observation:")
    for r in typed[:8]:
        p("      minor {:<3} ws_in {:<3} bdim {:<3} observed ({}, {})  expected ({}, {})  "
          "excess {}".format(r["minor"], r["ws_size_in"], r["border_dim"], r["inertia_pos"],
                             r["inertia_neg"], r["inertia_expected_pos"],
                             r["inertia_expected_neg"],
                             r["inertia_neg"] - r["inertia_expected_neg"]))

    p()
    p("(b) THE PRE-TAIL POST-DROP PASSES -- DID THE SAME THING HAPPEN, AND WHAT")
    p("    DIFFERED WHEN THEY ESCAPED?")
    pre = [(i, r) for i, r in enumerate(typed[:t0]) if r["post_drop"]]
    p("    {} post-drop passes before t0={}.".format(len(pre), t0))
    p("    minor  action        ride_outcome            slope            slope_tol        "
      "leaves           sign  p[released]      alpha        NEXT PASS")
    for i, r in pre:
        nxt = typed[i + 1] if i + 1 < n else None
        nxt_s = "{} dx_inf={}".format(nxt["action"], fmt(nxt["dx_inf"])) if nxt else "-"
        p("    {:<6} {:<13} {:<23} {:<16.8g} {:<16.8g} {:<16.8g} {:<5} {:<16.8g} {:<12.6g} {}"
          .format(r["minor"], r["action"], r["ride_outcome"], r["ride_slope"],
                  r["ride_slope_tol"], r["ride_leaves"], r["ride_sign_chosen"],
                  r["p_released"], r["alpha"], nxt_s))
    allpd = [r for r in typed if r["post_drop"]]
    c = Counter((r["ride_outcome"],
                 "slope>0" if r["ride_slope"] > 0 else
                 ("slope<0" if r["ride_slope"] < 0 else "slope=0 or ABSENT"),
                 r["action"]) for r in allpd)
    p("    every post-drop pass in the WHOLE walk, by (ride outcome, sign of g'p, action):")
    for k, v in sorted(c.items()):
        p("      {:<24} {:<18} {:<14} {}".format(k[0], k[1], k[2], v))
    mo = [r["minor"] for r in typed if r["action"] == "move_only"]
    p("    move_only passes in the whole walk: {} (last at minor {})".format(
        len(mo), mo[-1] if mo else "none"))
    p("    rides that STEPPED: {}".format(
        [r["minor"] for r in typed if r["ride_outcome"] == "stepped"]))

    p()
    p("(c) THE RESIDUALS OVER THE WALK, BY eqp_path AND BY post_drop")
    p("    stat_rel := stat_free_inf / max(stat_scale, 1);  "
      "act_rel := active_res_inf / max(x_inf, 1)")
    groups = {}
    for r in typed:
        groups.setdefault((r["eqp_path"], bool(r["post_drop"])), []).append(r)
    p("    path           post_drop   n      stat_rel min / median / max            "
      "act_rel min / median / max")
    for k in sorted(groups):
        g = groups[k]
        sr = sorted(r["stat_free_inf"] / max(r["stat_scale"], 1.0) for r in g
                    if present(r["stat_free_inf"]))
        ar = sorted(r["active_res_inf"] / max(r["x_inf"], 1.0) for r in g
                    if present(r["active_res_inf"]))
        med = lambda a: a[len(a) // 2] if a else float("nan")
        p("    {:<14} {:<11} {:<6} {:<10.4g} {:<10.4g} {:<16.4g} {:<10.4g} {:<10.4g} {:.4g}"
          .format(k[0], str(k[1]), len(g), sr[0] if sr else float('nan'), med(sr),
                  sr[-1] if sr else float('nan'), ar[0] if ar else float('nan'), med(ar),
                  ar[-1] if ar else float('nan')))
    neg = [r for r in allpd if present(r["p_released"]) and r["p_released"] < 0]
    pos = [r for r in allpd if present(r["p_released"]) and r["p_released"] >= 0]
    for label, g in (("p[released] <  0", neg), ("p[released] >= 0", pos)):
        if not g:
            p("    {}: no such pass".format(label))
            continue
        sr = sorted(r["stat_free_inf"] / max(r["stat_scale"], 1.0) for r in g)
        small = sum(1 for v in sr if v <= G_RESIDUAL_BAR)
        p("    {}  n={:<6} stat_rel min {:.4g}  median {:.4g}  max {:.4g}   "
          "passes with stat_rel <= 1e-6: {}".format(
              label, len(g), sr[0], sr[len(sr) // 2], sr[-1], small))
    p("    WHEN the stationarity residual is taken (qp_engine.cpp ~:700): at the")
    p("    pass's STARTING iterate, over the FREE block, with the multipliers")
    p("    `price` produced for the CURRENT working set. On a post-drop pass the")
    p("    just-released variable is free and its own dual therefore ENTERS the")
    p("    residual; a large value there is the drop's own signature, not a")
    p("    statement about solve accuracy. The active-row residual carries no such")
    p("    confound: it measures how far the iterate sits off the constraints the")
    p("    EQP holds at equality, and it is identical on both tail states because")
    p("    the tail does not move the iterate.")
    if A:
        p("    tail: act_res_inf = {}   step_tol = {}   ratio {:.4g}".format(
            fmt(A[0]["active_res_inf"]), fmt(A[0]["step_tol"]),
            A[0]["active_res_inf"] / A[0]["step_tol"] if A[0]["step_tol"] else float("nan")))

    p()
    p("(d) THE DROP-CANDIDATE SET ON THE TAIL'S DROP PASS")
    if D:
        d0 = D[0]
        cands = parse_candidates(d0["candidates"])
        chosen = "{}:{}:{}".format(d0["block"], d0["block_index"], d0["block_side"])
        rank = next((i + 1 for i, (ident, _) in enumerate(cands) if ident == chosen), None)
        p("    candidates considered: {}   recorded (cap 8): {}   inside the tie window: {}"
          .format(d0["drop_candidates"], d0["candidates_recorded"], d0["drop_tied"]))
        p("    chosen: {}  rank by violation: {}".format(chosen, rank))
        if len(cands) >= 2:
            v1, v2 = cands[0][1], cands[1][1]
            p("    margin of rank 1 over rank 2: absolute {:.6g}   relative {:.6g}".format(
                v1 - v2, (v1 - v2) / v1 if v1 else float("nan")))
        distinct_tail = len({r["candidates"] for r in D})
        alld = [r for r in typed if r["action"].startswith("drop")]
        p("    DOES THE SAME CANDIDATE SET RECUR? distinct candidate strings over the "
          "{} tail drop passes: {}".format(len(D), distinct_tail))
        p("    distinct candidate strings over all {} drop passes in the walk: {}".format(
            len(alld), len({r["candidates"] for r in alld})))
        p("    drop_candidates over the whole walk: {}".format(
            dict(sorted(Counter(r["drop_candidates"] for r in alld).items()))))
    p()


# ---------------------------------------------------------------------------
# one leg
# ---------------------------------------------------------------------------
def run_leg(cell, trace_path, m3a_row, probe_row, out, tables_dir=None):
    print("=" * 78, file=out)
    print("CELL {}".format(cell), file=out)
    print("=" * 78, file=out)
    try:
        header, raws, typed = load_trace(trace_path)
    except TraceError as e:
        print("REFUSED (section 6): {}".format(e.message), file=out)
        return e.rc, "ABORTED"

    rc = check_aborts(trace_path, m3a_row, probe_row, header, typed, out)
    if rc != RC_OK:
        return rc, "ABORTED"

    n = len(typed)
    t0, failures = find_tail(raws)
    tail_raw, tail_typed = raws[t0:], typed[t0:]
    size_ok = len(tail_typed) >= G_TAIL_MIN and len(tail_typed) / float(n) >= G_TAIL_FRACTION
    print("  section 3: t0={}  |T|={}  |T|/N={:.6f}  (bars: |T| >= {}, |T|/N >= {:.2f})"
          .format(t0, len(tail_typed), len(tail_typed) / float(n), G_TAIL_MIN,
                  G_TAIL_FRACTION), file=out)
    print("             R(m) fails at {} index(es); the last is {}".format(
        len(failures), failures[-1] if failures else "none"), file=out)
    if not size_ok:
        print("ABORTED_NO_TAIL (section 2.4): the tail does not meet section 3's size "
              "conditions", file=out)
        return RC_NO_TAIL, "ABORTED_NO_TAIL"

    two_state, a_idx, d_idx, other, alternating = tail_shape(tail_typed)
    print("             two-state shape: {}  (alternating={}, other actions={})".format(
        two_state, alternating, dict(other) if other else "none"), file=out)
    homo_a = homogeneity(tail_raw, tail_typed, a_idx)
    homo_d = homogeneity(tail_raw, tail_typed, d_idx)
    res = classify(raws, typed, t0, a_idx, d_idx, two_state, homo_a, homo_d, out)
    res["homo_a"], res["homo_d"] = homo_a, homo_d

    if res["outcome"] == "MULTIPLE":
        return RC_MULTIPLE, "MULTIPLE"

    print(file=out)
    print("  OUTCOME: {}".format(res["outcome"]), file=out)
    print("  form: {} ({}; section 5's homogeneity {})".format(
        res["form"],
        "the tail's two states, each a single repeated value" if res["form"] == "STATE"
        else "per-pass shares against the 0.90 bar",
        "HOLDS" if res["homogeneous"] else "FAILS"), file=out)
    print("  state-A passes: {} (the outcome is the value they all carry, reported with "
          "its repetition count)".format(res["n_a"]), file=out)
    for k in PRECEDENCE[:-1]:
        print("    {:<34} {:<14} share {:.4f}".format(
            k, "HOLDS" if res["holds"].get(k) else "does not hold",
            res["shares"].get(k, 0.0)), file=out)
    print("    {:<34} {}".format("MIXED (the residue)",
                                 "HOLDS" if res["outcome"] == "MIXED" else "does not hold"),
          file=out)
    if res["outcome"] == "RIDE_VETOED":
        print("  THE DOMINANT VETO, NAMED (section 4.3): {}  on {} of {} state-A passes "
              "(share {:.4f})".format(res["dominant_veto"], res["dominant_veto_count"],
                                      res["n_a"], res["dominant_veto_share"]), file=out)
    if res["outcome"] == "NUMERICAL_DIRECTION":
        print("  THE SUB-REASONS THAT HOLD (section 4.4; every one that holds is named, "
              "the outcome is one):", file=out)
        for k, v in res["sub_reasons"].items():
            print("    {:<24} {}".format(k, "HOLDS" if v else "does not hold"), file=out)
        print("    stat_free_inf/max(stat_scale,1) = {:.6g}  against the bar {:g}".format(
            res["stat_ratio"], G_RESIDUAL_BAR), file=out)
        print("    active_res_inf/max(x_inf,1)     = {:.6g}  against the bar {:g}".format(
            res["act_ratio"], G_RESIDUAL_BAR), file=out)
    print(file=out)
    readouts(cell, raws, typed, t0, a_idx, d_idx, res, out)

    if tables_dir:
        os.makedirs(tables_dir, exist_ok=True)
        path = os.path.join(tables_dir, "{}-outside-the-rule.txt".format(cell.lower()))
        with open(path, "w") as fh:
            outside_the_rule(cell, raws, typed, t0, a_idx, d_idx, fh)
        path2 = os.path.join(tables_dir, "{}-readouts-G.txt".format(cell.lower()))
        with open(path2, "w") as fh:
            print("M7 W2 M3b fix1 -- RULE-G.md section 7's read-outs for {}".format(cell),
                  file=fh)
            print("NOT verdicts. None of these can change an outcome.", file=fh)
            print(file=fh)
            readouts(cell, raws, typed, t0, a_idx, d_idx, res, fh)
    return RC_OK, res["outcome"]


# ===========================================================================
# section 6 -- THE FOLD PROOFS. Synthetic traces, one per outcome, one per
# refusal, and one at each side of every bar. The classifier is fold-proven
# BEFORE it reads a leg, and the transcript is committed with the verdict.
# ===========================================================================
BASE_A = {
    "solve": "0", "minor": "1", "action": "add", "block": "bound",
    "block_index": "7", "block_side": "lower", "drop_route": "none",
    "bound_gap": "inf", "alpha": "-0", "p_inf": "10", "dx_inf": "-0",
    "step_tol": "1e-07", "degenerate": "0", "ride": "0", "raw_ratio": "-0",
    "ratio_tied": "1", "ratio_capped": "1", "decision_multiplier": "nan",
    "decision_tol": "nan", "best_multiplier": "nan", "drop_candidates": "0",
    "drop_tied": "0", "shift_adds": "0", "ws_size_in": "20",
    "ws_sig_in": "111", "ws_size_out": "21", "ws_sig_out": "222",
    "x_inf": "100", "x_l1": "1000",
    "inertia_verdict": "2", "inertia_state": "observed", "inertia_pos": "80",
    "inertia_neg": "40", "inertia_zero": "0", "inertia_zero_derived": "1",
    "inertia_perturbed": "0", "inertia_expected_pos": "100",
    "inertia_expected_neg": "20",
    "eqp_path": "border_schur", "border_attempted": "1", "k0_rebuilt": "0",
    "border_latched": "0", "border_needs_refactorization": "0",
    "border_dim": "20", "eqp_call_refine_steps": "3",
    "pass_factorizations": "0", "pass_eqp_refine_steps": "0",
    "pass_border_refine_steps": "3", "pass_verdict_refine_steps": "0",
    "pass_schur_updates": "1",
    "post_drop": "1", "curvature_fired": "1", "curvature": "1e-14",
    "curvature_tol": "1e-07", "p_hp": "1e-05", "p_sq": "1e+09",
    "ride_outcome": "sign_disagreement_veto", "ride_sign_chosen": "0",
    "ride_slope": "-0.002", "ride_slope_tol": "5e-06", "ride_leaves": "-100",
    "ride_leaves_tol": "1e-08",
    "released_block": "bound", "released_index": "7", "released_side": "lower",
    "p_released": "-100", "released_directional": "100",
    "candidates_recorded": "0", "candidates": "",
    "primal_delta": "1e-08", "dual_mu": "1e-08",
    "stat_free_inf": "1e-09", "stat_scale": "1000", "active_res_inf": "1e-09",
}
BASE_D = dict(BASE_A)
BASE_D.update({
    "minor": "0", "action": "drop_dantzig", "alpha": "nan", "p_inf": "0",
    "dx_inf": "nan", "raw_ratio": "nan", "ratio_tied": "0",
    "decision_multiplier": "0.001", "decision_tol": "1e-08",
    "best_multiplier": "0.001", "drop_candidates": "3", "drop_tied": "1",
    "ws_size_in": "21", "ws_sig_in": "222", "ws_size_out": "20",
    "ws_sig_out": "111", "inertia_expected_pos": "99",
    "inertia_expected_neg": "21", "border_dim": "21",
    "eqp_call_refine_steps": "5", "pass_border_refine_steps": "5",
    "post_drop": "0", "curvature_fired": "0", "curvature": "nan",
    "curvature_tol": "nan", "p_hp": "nan", "p_sq": "0",
    "ride_outcome": "not_attempted", "ride_slope": "nan",
    "ride_slope_tol": "nan", "ride_leaves": "nan", "ride_leaves_tol": "nan",
    "released_block": "none", "released_index": "-1", "released_side": "free",
    "p_released": "nan", "released_directional": "nan",
    "candidates_recorded": "3",
    "candidates": "bound:7:lower:0.001;bound:8:lower:0.0009;bound:9:lower:0.0005",
})


def make_records(pairs, a_over=None, d_over=None, a_vary=None):
    """`pairs` D/A pairs. `a_vary` is a list of per-A-pass override dicts, cycled,
    so a tail can be made NON-homogeneous in a FIX-ROUND-1 column WITHOUT
    disturbing F0 -- which is how the frequency form is reached."""
    recs = []
    for k in range(pairs):
        d = dict(BASE_D)
        d.update(d_over or {})
        d["minor"] = str(2 * k)
        a = dict(BASE_A)
        a.update(a_over or {})
        if a_vary:
            a.update(a_vary[k % len(a_vary)])
        a["minor"] = str(2 * k + 1)
        recs.append(d)
        recs.append(a)
    return recs


def write_synth(dirpath, name, records, header_over=None):
    path = os.path.join(dirpath, name)
    hdr = {"cell": "FOLD", "tr_init": "inf", "solve_index": "0", "solves_seen": "1",
           "warm": "0", "n": "120", "me": "20", "mi": "0",
           "max_iter": str(len(records)), "incoming_ws_signature": "0",
           "incoming_ws_size": "0", "exit_status": "max_iter", "exit_status_raw": "1",
           "truncated": "0", "allocation_failed": "0", "max_records": "1000000",
           "records": str(len(records))}
    hdr.update(header_over or {})
    cols = list(COLUMNS)
    with open(path, "w") as fh:
        fh.write("# M7 W2 M3b FIX ROUND 1 SYNTHETIC FOLD -- not a measurement\n")
        for k, v in hdr.items():
            fh.write("# {}={}\n".format(k, v))
        fh.write(",".join(cols) + "\n")
        for r in records:
            fh.write(",".join(r[c] for c in cols) + "\n")
    return path


def write_rows(dirpath, minor_iters, tag, over=None, both=None):
    """A probe row and an M3a row. Identical unless `over` moves one field on
    the PROBE side only (which is what the identity gate refuses); `both` moves
    a field on BOTH sides, which passes the identity gate and is how a
    trace-integrity fold is built without tripping 2.1 first."""
    fields = {"first_" + f: "0" for f in gate.FIELDS}
    fields["first_minor_iters"] = str(minor_iters)
    fields[gate.STATUS_KEY] = "max_iter"
    fields.update(both or {})
    a = os.path.join(dirpath, "m3a-{}.txt".format(tag))
    b = os.path.join(dirpath, "probe-{}.txt".format(tag))
    for path, extra in ((a, None), (b, over)):
        vals = dict(fields)
        vals.update(extra or {})
        with open(path, "w") as fh:
            fh.write("PROBE_BEGIN\nLEDGER_FIRST_BEGIN\n")
            for k, v in vals.items():
                fh.write("{}={}\n".format(k, v))
            fh.write("LEDGER_FIRST_END\n")
    return a, b


def write_suite(dirpath, name, drop=()):
    path = os.path.join(dirpath, name)
    with open(path, "w") as fh:
        for fx in NEUTRALITY_FIXTURES:
            if fx in drop:
                continue
            fh.write("  1/1 Test #1: {} ..... Passed 0.01 sec\n".format(fx))
    return path


def selftest(out=sys.stdout):
    """RULE-G.md section 6. Every fold must land in its REGISTERED class."""
    folds, failed = [], 0
    tmp = tempfile.mkdtemp(prefix="rule-g-folds-")
    suite_ok = write_suite(tmp, "suite-ok.txt")
    suite_bad = write_suite(tmp, "suite-bad.txt",
                            drop=("QpWalkSeam.TwoEnginesOnTwoThreadsBothArmed",))

    def fold(label, want_rc, want_outcome, records=None, header_over=None,
             row_over=None, both_over=None, rows=None, trace_path=None,
             neutrality=(suite_ok,), note=""):
        nonlocal failed
        devnull = open(os.devnull, "w")
        try:
            if trace_path is None:
                tp = write_synth(tmp, "fold-{}.csv".format(len(folds)), records, header_over)
            else:
                tp = trace_path
            nrec = len(records) if records is not None else 0
            m3a, probe = rows if rows else write_rows(tmp, nrec, str(len(folds)),
                                                      row_over, both_over)
            rc = check_neutrality(neutrality, devnull)
            outcome = "ABORTED"
            if rc == RC_OK:
                rc, outcome = run_leg("FOLD", tp, m3a, probe, devnull)
        finally:
            devnull.close()
        ok = (rc == want_rc) and (want_outcome is None or outcome == want_outcome)
        failed += 0 if ok else 1
        folds.append((len(folds) + 1, label, want_rc, rc, want_outcome, outcome, ok, note))

    # ---- one fold per OUTCOME ---------------------------------------------
    fold("OUTCOME RIDE_VETOED (the base tail: neg excess, gate fired, sign veto)",
         RC_OK, "RIDE_VETOED", make_records(80))
    fold("OUTCOME INDEFINITE_IGNORED (neg excess, p_hp ABSENT, ride not attempted)",
         RC_OK, "INDEFINITE_IGNORED", make_records(80, a_over={
             "p_hp": "nan", "curvature": "nan", "curvature_tol": "nan",
             "curvature_fired": "0", "ride_outcome": "not_attempted",
             "ride_slope": "nan", "ride_slope_tol": "nan", "ride_leaves": "nan",
             "ride_leaves_tol": "nan"}))
    fold("OUTCOME CURVATURE_GATE_SILENT (neg excess, curvature present, gate silent)",
         RC_OK, "CURVATURE_GATE_SILENT", make_records(80, a_over={
             "curvature_fired": "0", "curvature": "1e-03", "curvature_tol": "1e-10",
             "p_hp": "1e+06", "ride_outcome": "not_attempted"}))
    fold("OUTCOME NUMERICAL_DIRECTION / RESIDUAL (inertia as expected, stat ratio over)",
         RC_OK, "NUMERICAL_DIRECTION", make_records(80, a_over={
             "inertia_expected_pos": "80", "inertia_expected_neg": "40",
             "stat_free_inf": "1", "stat_scale": "1000"}))
    fold("OUTCOME INERTIA_AS_EXPECTED_UNEXPLAINED (as expected, everything clean)",
         RC_OK, "INERTIA_AS_EXPECTED_UNEXPLAINED", make_records(80, a_over={
             "inertia_expected_pos": "80", "inertia_expected_neg": "40"}))
    fold("OUTCOME MIXED (neg excess, gate fired, the ride STEPPED -- the contradiction)",
         RC_OK, "MIXED", make_records(80, a_over={"ride_outcome": "stepped",
                                                  "ride_sign_chosen": "-1"}))
    fold("OUTCOME NUMERICAL_DIRECTION / RANK_OR_ZERO (adverse, neither expected nor excess)",
         RC_OK, "NUMERICAL_DIRECTION", make_records(80, a_over={
             "inertia_expected_pos": "70", "inertia_expected_neg": "50"}))
    fold("OUTCOME NUMERICAL_DIRECTION / UNOBSERVED_INERTIA (no usable verdict)",
         RC_OK, "NUMERICAL_DIRECTION", make_records(80, a_over={
             "inertia_state": "query_failed", "inertia_pos": "-1", "inertia_neg": "-1",
             "inertia_zero": "-1"}))

    # ---- one fold per REFUSAL in section 6 --------------------------------
    fold("REFUSAL: a trace file that does not exist", RC_TRACE, "ABORTED",
         records=[], trace_path=os.path.join(tmp, "nope.csv"))
    empty = os.path.join(tmp, "empty.csv")
    open(empty, "w").close()
    fold("REFUSAL: an EMPTY trace file", RC_TRACE, "ABORTED", records=[], trace_path=empty)
    ragged = os.path.join(tmp, "ragged.csv")
    with open(ragged, "w") as fh:
        fh.write("# records=1\n" + ",".join(COLUMNS) + "\n" + "1,2,3\n")
    fold("REFUSAL: an unparseable trace (a row shorter than the header)", RC_TRACE,
         "ABORTED", records=[], trace_path=ragged)
    short = os.path.join(tmp, "short-header.csv")
    recs = make_records(80)
    with open(short, "w") as fh:
        fh.write("# records=160\n# truncated=0\n# allocation_failed=0\n")
        cols = [c for c in COLUMNS if c != "ride_outcome"]
        fh.write(",".join(cols) + "\n")
        for r in recs:
            fh.write(",".join(r[c] for c in cols) + "\n")
    fold("REFUSAL: a header missing a field the rule reads (ride_outcome)", RC_TRACE,
         "ABORTED", records=[], trace_path=short)
    fold("REFUSAL: truncated=1", RC_INTEGRITY, "ABORTED", make_records(80),
         header_over={"truncated": "1"})
    fold("REFUSAL: allocation_failed=1 (RULE-G 2.2's addition; the M3b checker "
         "does not know this field)", RC_INTEGRITY, "ABORTED", make_records(80),
         header_over={"allocation_failed": "1"})
    fold("REFUSAL: len(records) != minor_iters (a pass went unrecorded)", RC_INTEGRITY,
         "ABORTED", make_records(80), both_over={"first_minor_iters": "161"})
    fold("REFUSAL: a record carrying action == unset", RC_INTEGRITY, "ABORTED",
         make_records(80)[:-1] + [dict(BASE_A, action="unset", minor="159")])
    fold("REFUSAL: the identity gate fails (one QpCounters field moved)", RC_IDENTITY,
         "ABORTED", make_records(80), row_over={"first_ws_adds": "1"})
    bad_row = os.path.join(tmp, "no-ledger.txt")
    with open(bad_row, "w") as fh:
        fh.write("PROBE_BEGIN\nfirst_minor_iters=160\n")
    fold("REFUSAL: a probe row with no LEDGER_FIRST block", RC_ROW, "ABORTED",
         make_records(80), rows=(bad_row, bad_row))
    fold("REFUSAL: a tail below the size conditions (ABORTED_NO_TAIL)", RC_NO_TAIL,
         "ABORTED_NO_TAIL",
         # 200 records whose F0 breaks period 2 until the last 40: |T| = 41 < 100
         [dict(BASE_D, minor=str(i), x_l1=str(1000 + i)) for i in range(160)]
         + make_records(20))
    fold("REFUSAL: MORE THAN ONE OUTCOME holds (4.1 and 4.2 made to overlap)",
         RC_MULTIPLE, "MULTIPLE", make_records(80, a_over={
             "p_hp": "nan", "curvature": "1e-03", "curvature_tol": "1e-10",
             "curvature_fired": "0", "ride_outcome": "not_attempted"}))
    fold("REFUSAL: neutrality evidence not green (one concurrent fixture missing)",
         RC_NEUTRALITY, "ABORTED", make_records(80), neutrality=(suite_bad,))
    fold("REFUSAL: no neutrality transcript named at all", RC_NEUTRALITY, "ABORTED",
         make_records(80), neutrality=())

    # ---- BOTH SIDES OF EVERY BAR ------------------------------------------
    # 4.4 RESIDUAL, stat arm: ratio 1.0000001e-6 (above) and 9.9e-7 (below).
    as_exp = {"inertia_expected_pos": "80", "inertia_expected_neg": "40"}
    fold("BAR 1e-6, stat ratio ABOVE (1.0000001e-06) -> NUMERICAL_DIRECTION", RC_OK,
         "NUMERICAL_DIRECTION", make_records(80, a_over=dict(
             as_exp, stat_free_inf="1.0000001e-03", stat_scale="1000")))
    fold("BAR 1e-6, stat ratio BELOW (9.9e-07) -> INERTIA_AS_EXPECTED_UNEXPLAINED",
         RC_OK, "INERTIA_AS_EXPECTED_UNEXPLAINED", make_records(80, a_over=dict(
             as_exp, stat_free_inf="9.9e-04", stat_scale="1000")))
    fold("BAR 1e-6, active-row ratio ABOVE (1.0000001e-06) -> NUMERICAL_DIRECTION",
         RC_OK, "NUMERICAL_DIRECTION", make_records(80, a_over=dict(
             as_exp, active_res_inf="1.0000001e-04", x_inf="100")))
    fold("BAR 1e-6, active-row ratio BELOW (9.9e-07) -> INERTIA_AS_EXPECTED_UNEXPLAINED",
         RC_OK, "INERTIA_AS_EXPECTED_UNEXPLAINED", make_records(80, a_over=dict(
             as_exp, active_res_inf="9.9e-05", x_inf="100")))
    # 4.4 REFINEMENT_SATURATED at 9 and at 10.
    fold("BAR eqp_call_refine_steps = 9 (below the cap) -> "
         "INERTIA_AS_EXPECTED_UNEXPLAINED", RC_OK, "INERTIA_AS_EXPECTED_UNEXPLAINED",
         make_records(80, a_over=dict(as_exp, eqp_call_refine_steps="9",
                                      eqp_path="border_schur")))
    fold("BAR eqp_call_refine_steps = 10 (the cap) -> NUMERICAL_DIRECTION", RC_OK,
         "NUMERICAL_DIRECTION", make_records(80, a_over=dict(
             as_exp, eqp_call_refine_steps="10", eqp_path="border_schur")))
    # section 5's share bar at 0.89 and 0.90. The variation is in a FIX-ROUND-1
    # column only, so F0's period-2 identity -- and therefore the tail -- is
    # untouched and the frequency form is what changes.
    vary89 = [{}] * 89 + [{"ride_outcome": "stepped", "ride_sign_chosen": "-1"}] * 11
    vary90 = [{}] * 90 + [{"ride_outcome": "stepped", "ride_sign_chosen": "-1"}] * 10
    fold("BAR share = 0.89 (below 0.90) -> MIXED", RC_OK, "MIXED",
         make_records(100, a_vary=vary89))
    fold("BAR share = 0.90 (at the bar) -> RIDE_VETOED", RC_OK, "RIDE_VETOED",
         make_records(100, a_vary=vary90))

    print("RULE-G.md section 6 -- THE FOLD PROOFS", file=out)
    print("Each row is an input the classifier MUST land in its REGISTERED class. A", file=out)
    print("classifier that returns one outcome for all of them classifies nothing, and", file=out)
    print("that is the failure this mode exists to forbid.", file=out)
    print(file=out)
    print("  #   want_rc got_rc  want_outcome                      got_outcome"
          "                       label", file=out)
    for num, label, wrc, grc, wout, gout, ok, _note in folds:
        print("  {:<3} {:<7} {:<7} {:<33} {:<33} {} {}".format(
            num, wrc, grc, str(wout), str(gout), "OK  " if ok else "FAIL", label), file=out)
    print(file=out)
    print("RULE_G_FOLDS_TOTAL={}  RULE_G_FOLDS_FAILED={}".format(len(folds), failed),
          file=out)
    print("RULE_G_FOLDS_DISTINCT_OUTCOMES={}".format(
        len({f[5] for f in folds})), file=out)
    return RC_SELFTEST if failed else RC_OK


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="RULE-G.md's cause classifier")
    ap.add_argument("--leg", action="append", default=[],
                    help="CELL:TRACE_CSV:M3A_ROW:PROBE_ROW")
    ap.add_argument("--neutrality", action="append", default=[],
                    help="a committed suite transcript (one per configuration)")
    ap.add_argument("--tables", default=None)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        return selftest()
    if not args.leg:
        print("classify_cause_fix1: no --leg given", file=sys.stderr)
        return 2

    print("M7 W2 M3b FIX ROUND 1 -- VERDICT-G: RULE-G.md's CAUSE classifier over the")
    print("re-trace of the same two first QPs.")
    print()
    print("RULE-G.md was pre-registered at f916e82d, BEFORE any leg ran, identity legs")
    print("included. The outcome below is whatever the rule returns. No bar was moved,")
    print("no predicate relaxed, and NO OUTCOME HERE LICENSES A BEHAVIOUR CHANGE, A")
    print("DEFAULT CHANGE OR A LINE OF CODE (RULE-G.md sections 0 and 8).")
    print()
    print("RULE.md beside it STANDS UNCHANGED and CYCLING_PROPER has precedence as the")
    print("statement of WHAT the walk is doing; this file says only WHY.")
    print()

    rc = check_neutrality(args.neutrality, sys.stdout)
    print()
    if rc != RC_OK:
        return rc

    results, worst = [], RC_OK
    for spec in args.leg:
        parts = spec.split(":")
        if len(parts) != 4:
            print("classify_cause_fix1: --leg must be CELL:TRACE:M3A_ROW:PROBE_ROW",
                  file=sys.stderr)
            return 2
        cell, trace, m3a, probe = parts
        code, outcome = run_leg(cell, trace, m3a, probe, sys.stdout, args.tables)
        results.append((cell, code, outcome))
        if code != RC_OK:
            worst = code
        print()

    print("=" * 78)
    print("SUMMARY -- the rule is applied PER CELL. There is no cross-cell aggregation")
    print("and no outcome that depends on both cells agreeing (RULE-G.md section 1); if")
    print("they disagree, both outcomes stand.")
    for cell, code, outcome in results:
        print("  {:<12} {:<34} rc={}".format(cell, outcome, code))
    print("=" * 78)
    return worst


if __name__ == "__main__":
    sys.exit(main())
