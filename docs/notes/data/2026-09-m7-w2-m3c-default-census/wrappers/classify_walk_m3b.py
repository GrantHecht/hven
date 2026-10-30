#!/usr/bin/env python3
"""M7 W2 M3b -- RULE.md's classifier over the first-QP per-minor trace.

THIS FILE IMPLEMENTS RULE.md SECTIONS 3, 4 AND 5 AS THEY ARE WRITTEN, and
section 2 by delegating to `check_identity_m3b.py` (the same checker the legs
were gated with). Nothing here chooses a threshold: every bar below is quoted
from RULE.md with its section number, and a bar that turns out to have been
badly designed is DECLARED as a defect of the rule -- never moved.

  section 2  ABORT conditions, checked before any predicate            -> ABORTED
  section 3  the seven outcomes and their exact predicates
  section 4  precedence, which makes the seven mutually exclusive
  section 5  the nine descriptive read-outs, which are NOT verdicts
  section 6  the nine pre-registered synthetic traces (`--selftest`)

NON-VACUITY (section 6). `--selftest` builds the nine pre-registered synthetic
traces and runs the WHOLE pipeline on each -- the identity gate included -- and
fails unless each lands in its registered class. A classifier that returns MIXED
for all nine is vacuous and its output is not a verdict; that is the failure
this mode exists to forbid, and its transcript is committed beside the verdict.

No verdict this file returns licenses a behaviour change, a default change or a
line of code (RULE.md section 0).
"""
import argparse
import math
import os
import statistics
import sys
import tempfile
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_identity_m3b as gate  # noqa: E402  (path must be set first)

# --- RULE.md's bars, quoted with their section numbers ----------------------
R_CYCLE = 0.20        # 3.1  CYCLING_PROPER
R_FLIP = 0.40         # 3.2  SIDE_PING_PONG, P_flip
R_FLIP_TOP = 0.20     # 3.2  SIDE_PING_PONG, C_top
R_IMM = 0.60          # 3.3  IMMEDIATE_READD
R_IMM_K = 2           # 3.3  the k
R_PROBE = 0.30        # 3.4  ZERO_PROBE_CHURN
R_RIDE = 0.30         # 3.5  NEGATIVE_CURVATURE_BLOCKERS
R_CRAWL_G = 0.70      # 3.6  DEGENERATE_CRAWL, G
R_CRAWL_HALF = 0.60   # 3.6  DEGENERATE_CRAWL, each half

PRECEDENCE = (        # 4
    "CYCLING_PROPER",
    "SIDE_PING_PONG",
    "IMMEDIATE_READD",
    "ZERO_PROBE_CHURN",
    "NEGATIVE_CURVATURE_BLOCKERS",
    "DEGENERATE_CRAWL",
)

ACTIONS = (           # qp_walk_trace.h's eleven QpWalkAction values, in order
    "unset", "start_repair", "post_probe_restart", "drop_dantzig",
    "drop_zero_probe", "suspect_escalation", "terminal", "ride_stepped",
    "ride_unbounded", "add", "move_only",
)
DROP_ACTIONS = ("drop_dantzig", "drop_zero_probe")     # 1.1  DROP pass
ADD_ACTIONS = ("add", "ride_stepped")                  # 1.1  ADD pass
ROUTES = ("none", "dantzig", "zero_probe")

INT_COLS = ("solve", "minor", "block_index", "ratio_tied", "drop_candidates",
            "drop_tied", "shift_adds", "ws_size_in", "ws_size_out",
            "ws_sig_in", "ws_sig_out")
FLOAT_COLS = ("bound_gap", "alpha", "p_inf", "dx_inf", "step_tol", "raw_ratio",
              "decision_multiplier", "decision_tol", "best_multiplier",
              "x_inf", "x_l1")
BOOL_COLS = ("degenerate", "ride", "ratio_capped")


# ---------------------------------------------------------------------------
# Reading the trace
# ---------------------------------------------------------------------------
def _f(text):
    try:
        return float(text)
    except ValueError:
        return float("nan")


def present(v):
    return v is not None and not math.isnan(v)


def load_trace(path):
    """(header dict, [record dict]). Types are applied here, once."""
    header, cols, recs = {}, None, []
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
            raw = dict(zip(cols, line.split(",")))
            rec = {}
            for k, v in raw.items():
                if k in INT_COLS:
                    rec[k] = int(v)
                elif k in FLOAT_COLS:
                    rec[k] = _f(v)
                elif k in BOOL_COLS:
                    rec[k] = (v == "1")
                else:
                    rec[k] = v
            recs.append(rec)
    return header, recs


# ---------------------------------------------------------------------------
# small statistics, written out rather than imported, so the report can say
# exactly what a "decile" here means: the order statistic at ceil(q*(m-1)).
# ---------------------------------------------------------------------------
def deciles(values):
    vals = sorted(v for v in values if present(v))
    if not vals:
        return None
    m = len(vals)
    out = []
    for d in range(0, 11):
        idx = int(round((d / 10.0) * (m - 1)))
        out.append(vals[idx])
    return out


def fmt_deciles(values, fmt="{:.6g}"):
    d = deciles(values)
    if d is None:
        return "(no present value)"
    return "  ".join(fmt.format(v) for v in d)


def summary(values):
    vals = [v for v in values if present(v)]
    if not vals:
        return None
    finite = [v for v in vals if math.isfinite(v)]
    return {
        "count": len(vals),
        "min": min(vals),
        "max": max(vals),
        "mean": (sum(finite) / len(finite)) if finite else float("inf"),
        "median": statistics.median(vals),
        "deciles": deciles(vals),
        "n_inf": sum(1 for v in vals if math.isinf(v)),
    }


def safe_div(a, b):
    return (a / b) if b else 0.0


# ---------------------------------------------------------------------------
# section 3 -- the predicates
# ---------------------------------------------------------------------------
def tol(rec):
    """The pass's own step_tol; an absent one is read as 0 (the strictest
    reading), and the count of such passes is reported."""
    st = rec["step_tol"]
    return st if present(st) else 0.0


def moved(rec):
    """RULE.md 3.1: a pass moved the iterate iff dx_inf is present and above
    the walk's own resolution."""
    dx = rec["dx_inf"]
    return present(dx) and dx > tol(rec)


def cycling(recs, n_vars):
    """3.1 CYCLING_PROPER.

    A pass's state is (ws_sig_in, bucket(x_inf), bucket(x_l1)); two passes share
    a bucket iff |dx_inf| <= T and |dx_l1| <= n*T with T = max(step_tol_i,
    step_tol_j). The relation is NOT transitive, so "already seen at a strictly
    earlier pass" is evaluated EXISTENTIALLY against every earlier pass -- which
    is what the grouping below does exactly: passes are grouped by signature,
    then by their exact (x_inf, x_l1) pair, and within a pair the tolerance test
    depends only on step_tol, so testing the pair's largest step_tol first is an
    exact skip and never a widened one.

    Where several earlier passes match, the MOST RECENT one is taken as the
    recurrence's partner. That yields the SHORTEST period, and it is also the
    partner most likely to satisfy the no-move condition (the moved-pass prefix
    is monotone), so checking it is sufficient for the existential second
    conjunct. The choice is stated here because RULE.md does not fix it.
    """
    N = len(recs)
    pref = [0] * (N + 1)
    for m, r in enumerate(recs):
        pref[m + 1] = pref[m] + (1 if moved(r) else 0)

    groups = defaultdict(dict)   # sig -> (x_inf, x_l1) -> [max_tol, [(i, tol)]]
    match = [None] * N
    for j, r in enumerate(recs):
        g = groups[r["ws_sig_in"]]
        tj, xj, lj = tol(r), r["x_inf"], r["x_l1"]
        best = None
        for (xi, li), entry in g.items():
            T = max(entry[0], tj)
            if abs(xi - xj) > T or abs(li - lj) > n_vars * T:
                continue                      # not even the loosest i matches
            for i, ti in reversed(entry[1]):
                Tij = max(ti, tj)
                if abs(xi - xj) <= Tij and abs(li - lj) <= n_vars * Tij:
                    if best is None or i > best:
                        best = i
                    break
        match[j] = best
        key = (xj, lj)
        if key in g:
            e = g[key]
            e[0] = max(e[0], tj)
            e[1].append((j, tj))
        else:
            g[key] = [tj, [(j, tj)]]

    seen = [j for j in range(N) if match[j] is not None]
    R = safe_div(len(seen), N)
    first = seen[0] if seen else None
    periods = Counter(j - match[j] for j in seen)

    no_move_pairs = [j for j in seen if pref[j] - pref[match[j]] == 0]
    drift = None
    if first is not None:
        i = match[first]
        window = recs[i:first]
        drift = (first - i) * (max((tol(x) for x in window), default=0.0))

    return {
        "R": R,
        "seen": len(seen),
        "N": N,
        "distinct_states": N - len(seen),
        "first_recurrence": (match[first], first) if first is not None else None,
        "period": (first - match[first]) if first is not None else None,
        "periods": periods,
        "drift_bound": drift,
        "no_move_pairs": len(no_move_pairs),
        "first_no_move_pair": ((match[no_move_pairs[0]], no_move_pairs[0])
                               if no_move_pairs else None),
        "holds": (R >= R_CYCLE) and bool(no_move_pairs),
        "moved_passes": pref[N],
    }


def side_ping_pong(adds):
    """3.2 SIDE_PING_PONG."""
    bound_adds = [r for r in adds if r["block"] == "bound"]
    last, flips, gaps = {}, Counter(), defaultdict(list)
    odd_sides = Counter()
    for r in bound_adds:
        v, s = r["block_index"], r["block_side"]
        if s not in ("lower", "upper"):
            odd_sides[s] += 1
        if v in last and last[v] != s and last[v] in ("lower", "upper") \
                and s in ("lower", "upper"):
            flips[v] += 1
        last[v] = s
        gaps[v].append(r["bound_gap"])
    total = sum(flips.values())
    P_flip = safe_div(total, len(bound_adds))
    C_top = safe_div(max(flips.values()), total) if total else 0.0
    return {
        "bound_adds": len(bound_adds),
        "flips": total,
        "P_flip": P_flip,
        "C_top": C_top,
        "per_variable": flips,
        "gaps": gaps,
        "odd_sides": odd_sides,
        "holds": (P_flip >= R_FLIP) and (C_top >= R_FLIP_TOP),
    }


def immediate_readd(recs, drops, adds, ks=(1, 2, 3, 5)):
    """3.3 IMMEDIATE_READD. `k = 2` decides; the neighbours are reported."""
    N = len(recs)
    is_add = [False] * N
    ident = [None] * N
    for r in adds:
        is_add[r["minor_pos"]] = True
        ident[r["minor_pos"]] = (r["block"], r["block_index"])
    prev_add = [None] * N            # most recent ADD pass strictly before m
    last = None
    for m in range(N):
        prev_add[m] = last
        if is_add[m]:
            last = m

    immediate, results = [], {}
    for r in drops:
        if r["block"] == "none":
            continue
        j = r["minor_pos"]
        a = prev_add[j]
        if a is not None and a == j - 1 and ident[a] == (r["block"], r["block_index"]):
            immediate.append(j)
    for k in ks:
        readd = 0
        for j in immediate:
            d = (recs[j]["block"], recs[j]["block_index"])
            if any(is_add[m] and ident[m] == d for m in range(j + 1, min(j + k, N - 1) + 1)):
                readd += 1
        results[k] = safe_div(readd, len(drops))
    never = 0
    for j in immediate:
        d = (recs[j]["block"], recs[j]["block_index"])
        if not any(is_add[m] and ident[m] == d for m in range(j + 1, N)):
            never += 1
    return {
        "N_drop": len(drops),
        "immediate": len(immediate),
        "P_imm": results,
        "P_imm_k": results[R_IMM_K],
        "never_readmitted_ever": never,
        "holds": results[R_IMM_K] >= R_IMM,
    }


def zero_probe(drops):
    """3.4 ZERO_PROBE_CHURN."""
    hist = Counter(r["drop_route"] for r in drops)
    P = safe_div(hist.get("zero_probe", 0), len(drops))
    return {"hist": hist, "N_drop": len(drops), "P_probe": P,
            "holds": P >= R_PROBE}


def ride_blockers(adds, recs):
    """3.5 NEGATIVE_CURVATURE_BLOCKERS."""
    ride_adds = [r for r in adds if r["action"] == "ride_stepped"]
    P = safe_div(len(ride_adds), len(adds))
    return {"N_add": len(adds), "ride_adds": len(ride_adds), "P_ride": P,
            "actions": Counter(r["action"] for r in recs),
            "holds": P >= R_RIDE}


def crawl(recs, cyc):
    """3.6 DEGENERATE_CRAWL."""
    N = len(recs)
    sigs = [r["ws_sig_in"] for r in recs]
    D = len(set(sigs))
    G = safe_div(D, N)
    half = N // 2
    D1 = len(set(sigs[:half]))
    D2 = len(set(sigs[half:]))
    growth = []
    for d in range(1, 11):
        upto = int(round(d * N / 10.0))
        growth.append((upto, len(set(sigs[:upto]))))
    floor = R_CRAWL_HALF * (N / 2.0)
    return {
        "D": D, "G": G, "D1": D1, "D2": D2, "half_floor": floor,
        "growth": growth,
        "holds": (not cyc["holds"]) and G >= R_CRAWL_G and D1 >= floor and D2 >= floor,
    }


# ---------------------------------------------------------------------------
# section 5 -- the descriptive read-outs
# ---------------------------------------------------------------------------
def readouts(header, recs, adds, drops, row):
    N = len(recs)
    out = {}

    # 5.1 the first minor at which progress stops
    movers = [r["minor"] for r in recs if moved(r)]
    positive = [r["minor"] for r in recs if present(r["dx_inf"]) and r["dx_inf"] > 0.0]
    m_stop = max(movers) if movers else None
    tail = sum(r["dx_inf"] for r in recs
               if present(r["dx_inf"]) and (m_stop is None or r["minor"] > m_stop))
    out["progress"] = {
        "m_stop": m_stop,
        "m_stop_frac": safe_div(m_stop, N) if m_stop is not None else None,
        "last_dx_positive": max(positive) if positive else None,
        "sum_dx_after_stop": tail,
        "moving_passes": len(movers),
        "absent_step_tol": sum(1 for r in recs if not present(r["step_tol"])),
    }

    # 5.2 admissions per identity
    adm = Counter((r["block"], r["block_index"], ) for r in adds)
    counts = sorted(adm.values())
    out["admissions"] = {
        "distinct": len(adm),
        "total": len(adds),
        "mean": safe_div(sum(counts), len(counts)),
        "median": statistics.median(counts) if counts else 0,
        "max": max(counts) if counts else 0,
        "top": adm.most_common(10),
        "full": adm,
        "bound_distinct": len({i for (b, i) in adm if b == "bound"}),
        "bound_total": sum(c for (b, i), c in adm.items() if b == "bound"),
        "row_distinct": len({i for (b, i) in adm if b == "ineq"}),
        "declared_distinct_bound_added": row.get("first_distinct_bound_added"),
        "declared_ws_adds_bound": row.get("first_ws_adds_bound"),
        "declared_distinct_ineq_added": row.get("first_distinct_ineq_added"),
    }

    # 5.3 fixed / near-fixed
    bound_adds = [r for r in adds if r["block"] == "bound"]
    tols = {r["decision_tol"] for r in recs if present(r["decision_tol"])}
    opt_tol = min(tols) if tols else 1e-9
    def classify_gap(g, t):
        if not present(g):
            return "ABSENT"
        if g == 0.0:
            return "FIXED"
        if g <= t:
            return "NEAR_FIXED"
        return "ORDINARY"
    cut_step = Counter(classify_gap(r["bound_gap"], tol(r)) for r in bound_adds)
    cut_opt = Counter(classify_gap(r["bound_gap"], opt_tol) for r in bound_adds)
    out["bound_gap"] = {
        "cut_step_tol": cut_step,
        "cut_opt_tol": cut_opt,
        "opt_tol_used": opt_tol,
        "opt_tol_observed": sorted(tols),
        "summary": summary([r["bound_gap"] for r in bound_adds]),
        "per_variable": {i: bg for (i, bg) in
                         sorted({r["block_index"]: r["bound_gap"]
                                 for r in bound_adds}.items())},
    }

    # 5.4 multipliers at drop
    def mult_block(rows):
        mu = [r["decision_multiplier"] for r in rows]
        ratio = [safe_div(r["decision_multiplier"], r["decision_tol"])
                 if present(r["decision_multiplier"]) and present(r["decision_tol"])
                 else float("nan") for r in rows]
        frac = [safe_div(r["decision_multiplier"], r["best_multiplier"])
                if present(r["decision_multiplier"]) and present(r["best_multiplier"])
                and r["best_multiplier"] != 0.0 else float("nan") for r in rows]
        return {
            "n": len(rows),
            "mu": summary(mu),
            "mu_over_tol": summary(ratio),
            "best": summary([r["best_multiplier"] for r in rows]),
            "mu_over_best": summary(frac),
            "candidates": summary([float(r["drop_candidates"]) for r in rows]),
            "tied": summary([float(r["drop_tied"]) for r in rows]),
        }
    out["multipliers"] = {
        "all": mult_block(drops),
        "by_route": {rt: mult_block([r for r in drops if r["drop_route"] == rt])
                     for rt in ROUTES if any(r["drop_route"] == rt for r in drops)},
    }

    # 5.5 ratio-test ties
    tied = Counter(r["ratio_tied"] for r in adds)
    n_gt1 = sum(c for t, c in tied.items() if t > 1)
    out["ratio_ties"] = {
        "hist": tied,
        "n_gt1": n_gt1,
        "share_gt1": safe_div(n_gt1, len(adds)),
        "max": max(tied) if tied else 0,
        "declared_ratio_ties": row.get("first_ratio_ties"),
        "agree": str(n_gt1) == str(row.get("first_ratio_ties")),
    }

    # 5.6 the incoming warm signature
    out["incoming"] = {
        "signature": header.get("incoming_ws_signature"),
        "size": header.get("incoming_ws_size"),
        "warm": header.get("warm"),
        "note": "RULE.md 5.6: this task traces ONE QP per cell, so the "
                "comparison against later majors is DECLINED BY SCOPE, not a gap.",
    }

    # 5.7 action + route histograms
    out["histograms"] = {
        "actions": Counter(r["action"] for r in recs),
        "routes": Counter(r["drop_route"] for r in recs),
        "blocks": Counter(r["block"] for r in recs),
    }

    # 5.8 the working-set size trajectory
    sizes = [r["ws_size_in"] for r in recs]
    out["ws_size"] = {
        "first": sizes[0] if sizes else None,
        "last": sizes[-1] if sizes else None,
        "min": min(sizes) if sizes else None,
        "max": max(sizes) if sizes else None,
        "deciles": deciles([float(s) for s in sizes]),
        "changed": sum(1 for r in recs if r["ws_size_out"] != r["ws_size_in"]),
        "shift_adds": sum(1 for r in recs if r["shift_adds"] > 0),
    }

    # 5.9 raw_ratio
    rr = [r["raw_ratio"] for r in adds]
    out["raw_ratio"] = {
        "n": len(rr),
        "gt1": sum(1 for v in rr if present(v) and v > 1.0),
        "summary": summary([v for v in rr if present(v) and v <= 1.0]),
        "all": summary(rr),
    }
    return out


# ---------------------------------------------------------------------------
# the whole pipeline on one cell
# ---------------------------------------------------------------------------
def classify(cell, trace_path, row_path, m3a_path, out=sys.stdout):
    """Returns (outcome, details). Section 2 first, then 3, then 4, then 5."""
    buf = []
    class _Tee:
        def write(self, s):
            buf.append(s)
        def flush(self):
            pass
    tee = _Tee()

    rc_id, compared = gate.check_identity(m3a_path, row_path, out=tee)
    rc_tr = gate.check_trace(trace_path, row_path, out=tee) if rc_id == 0 else None
    gate_text = "".join(buf)
    if rc_id != 0 or rc_tr != 0:
        return "ABORTED", {
            "gate_text": gate_text,
            "reason": ("RULE.md section 2.1 (the identity gate) refused"
                       if rc_id != 0 else
                       "RULE.md section 2.2 (trace integrity) refused"),
            "rc_identity": rc_id, "rc_trace": rc_tr, "fields_compared": compared,
        }

    header, recs = load_trace(trace_path)
    row, _ = gate.read_kv(row_path)
    for pos, r in enumerate(recs):
        r["minor_pos"] = pos
    n_vars = int(header.get("n", "0"))

    adds = [r for r in recs if r["action"] in ADD_ACTIONS and r["block"] != "none"]
    drops = [r for r in recs if r["action"] in DROP_ACTIONS]

    cyc = cycling(recs, n_vars)
    spp = side_ping_pong(adds)
    imm = immediate_readd(recs, drops, adds)
    zpc = zero_probe(drops)
    ncb = ride_blockers(adds, recs)
    crw = crawl(recs, cyc)

    holds = {
        "CYCLING_PROPER": cyc["holds"],
        "SIDE_PING_PONG": spp["holds"],
        "IMMEDIATE_READD": imm["holds"],
        "ZERO_PROBE_CHURN": zpc["holds"],
        "NEGATIVE_CURVATURE_BLOCKERS": ncb["holds"],
        "DEGENERATE_CRAWL": crw["holds"],
    }
    outcome = "MIXED"
    for name in PRECEDENCE:
        if holds[name]:
            outcome = name
            break

    details = {
        "cell": cell, "header": header, "row": row, "N": len(recs),
        "n_vars": n_vars, "gate_text": gate_text, "fields_compared": compared,
        "cycling": cyc, "ping_pong": spp, "immediate": imm, "probe": zpc,
        "ride": ncb, "crawl": crw, "holds": holds,
        "readouts": readouts(header, recs, adds, drops, row),
        "N_add": len(adds), "N_drop": len(drops),
        "declared_ws_adds": row.get("first_ws_adds"),
        "declared_ws_drops": row.get("first_ws_drops"),
    }
    return outcome, details


# ---------------------------------------------------------------------------
# rendering
# ---------------------------------------------------------------------------
def render(cell, outcome, d, out):
    p = lambda *a: print(*a, file=out)
    p("=" * 78)
    p("CELL {}   OUTCOME: {}".format(cell, outcome))
    p("=" * 78)
    if outcome == "ABORTED":
        p("  {}".format(d["reason"]))
        p(d["gate_text"].rstrip())
        return
    N, hdr = d["N"], d["header"]
    p("  records {}   n {}   me {}   mi {}   max_iter {}   exit {}"
      .format(N, hdr.get("n"), hdr.get("me"), hdr.get("mi"),
              hdr.get("max_iter"), hdr.get("exit_status")))
    p("  RULE.md section 2: identity gate {} fields DIFFERING 0; trace integrity OK"
      .format(d["fields_compared"]))
    p("")
    p("-- section 3, every predicate, whatever won (RULE.md 3: 'every quantity")
    p("   below is reported for every cell whatever outcome wins') ------------")
    c = d["cycling"]
    p("  3.1 CYCLING_PROPER            {}".format("HOLDS" if c["holds"] else "does not hold"))
    p("      R = {:.6f}   (bar {:.2f})   seen {} of {} passes".format(
        c["R"], R_CYCLE, c["seen"], c["N"]))
    p("      distinct states {}   passes that moved the iterate {}".format(
        c["distinct_states"], c["moved_passes"]))
    if c["first_recurrence"]:
        i, j = c["first_recurrence"]
        p("      first recurrence  i={}  j={}  period {}   drift bound {:.6g}".format(
            i, j, c["period"], c["drift_bound"]))
    else:
        p("      first recurrence  NONE")
    p("      recurring pairs with NO moving pass in [i,j): {}   first {}".format(
        c["no_move_pairs"], c["first_no_move_pair"]))
    tops = c["periods"].most_common(10)
    p("      period multiset (top 10 by count): {}".format(
        ", ".join("{}x{}".format(k, v) for k, v in tops) if tops else "(empty)"))
    s = d["ping_pong"]
    p("  3.2 SIDE_PING_PONG            {}".format("HOLDS" if s["holds"] else "does not hold"))
    p("      P_flip = {:.6f} (bar {:.2f})   C_top = {:.6f} (bar {:.2f})".format(
        s["P_flip"], R_FLIP, s["C_top"], R_FLIP_TOP))
    p("      side flips {} over {} bound ADD passes; sides outside lower/upper: {}".format(
        s["flips"], s["bound_adds"], dict(s["odd_sides"]) or "none"))
    p("      top flippers: {}".format(s["per_variable"].most_common(10) or "(none)"))
    im = d["immediate"]
    p("  3.3 IMMEDIATE_READD           {}".format("HOLDS" if im["holds"] else "does not hold"))
    p("      P_imm(k=2) = {:.6f} (bar {:.2f})   immediate drops {} of {} drops".format(
        im["P_imm_k"], R_IMM, im["immediate"], im["N_drop"]))
    p("      P_imm at k = 1,2,3,5: {}".format(
        "  ".join("{}:{:.6f}".format(k, v) for k, v in sorted(im["P_imm"].items()))))
    p("      immediate drops never re-admitted anywhere later: {}".format(
        im["never_readmitted_ever"]))
    z = d["probe"]
    p("  3.4 ZERO_PROBE_CHURN          {}".format("HOLDS" if z["holds"] else "does not hold"))
    p("      P_probe = {:.6f} (bar {:.2f})   route histogram over DROP passes: {}".format(
        z["P_probe"], R_PROBE, dict(z["hist"])))
    p("      N_drop {} vs the record's ws_drops {}  -> {}".format(
        z["N_drop"], d["declared_ws_drops"],
        "AGREE" if str(z["N_drop"]) == str(d["declared_ws_drops"]) else "DISAGREE (instrument finding)"))
    rd = d["ride"]
    p("  3.5 NEGATIVE_CURVATURE_BLOCKERS {}".format("HOLDS" if rd["holds"] else "does not hold"))
    p("      P_ride = {:.6f} (bar {:.2f})   ride-adds {} of {} ADD passes".format(
        rd["P_ride"], R_RIDE, rd["ride_adds"], rd["N_add"]))
    p("      N_add {} vs the record's ws_adds {}  -> {}".format(
        rd["N_add"], d["declared_ws_adds"],
        "AGREE" if str(rd["N_add"]) == str(d["declared_ws_adds"]) else "DISAGREE (instrument finding)"))
    cr = d["crawl"]
    p("  3.6 DEGENERATE_CRAWL          {}".format("HOLDS" if cr["holds"] else "does not hold"))
    p("      G = D/N = {}/{} = {:.6f} (bar {:.2f})".format(cr["D"], N, cr["G"], R_CRAWL_G))
    p("      D1 = {}  D2 = {}  floor 0.60*(N/2) = {:.1f}".format(
        cr["D1"], cr["D2"], cr["half_floor"]))
    p("      distinct signatures by decile: {}".format(
        "  ".join("{}:{}".format(a, b) for a, b in cr["growth"])))
    p("")
    p("-- section 4, precedence ------------------------------------------------")
    for name in PRECEDENCE:
        p("     {:<30} {}".format(name, "HOLDS" if d["holds"][name] else "-"))
    p("     {:<30} {}".format("MIXED", "(the residue)"))
    p("  => OUTCOME {}".format(outcome))


def render_readouts(cell, d, out):
    p = lambda *a: print(*a, file=out)
    r = d["readouts"]
    N = d["N"]
    p("=" * 78)
    p("CELL {} -- RULE.md section 5's DESCRIPTIVE READ-OUTS".format(cell))
    p("These are NOT verdicts and none of them can change the outcome.")
    p("=" * 78)
    g = r["progress"]
    p("5.1 the first minor at which progress stops")
    p("    m_stop (last pass with dx_inf > step_tol) = {}   m_stop/N = {}".format(
        g["m_stop"], "{:.6f}".format(g["m_stop_frac"]) if g["m_stop_frac"] is not None else "n/a"))
    p("    last minor with dx_inf > 0            = {}".format(g["last_dx_positive"]))
    p("    sum of dx_inf over the passes AFTER m_stop = {:.6g}".format(g["sum_dx_after_stop"]))
    p("    passes that moved the iterate: {} of {}   passes with no step_tol: {}".format(
        g["moving_passes"], N, g["absent_step_tol"]))
    a = r["admissions"]
    p("")
    p("5.2 admissions per identity, over ADD passes")
    p("    distinct identities {}   admissions {}   mean {:.3f}   median {}   max {}".format(
        a["distinct"], a["total"], a["mean"], a["median"], a["max"]))
    p("    top 10: {}".format(", ".join("{}{}x{}".format(b, i, c) for (b, i), c in a["top"])))
    p("    bound identities {} (declared distinct_bound_added {}), bound admissions {} "
      "(declared ws_adds_bound {}) -> {}".format(
          a["bound_distinct"], a["declared_distinct_bound_added"], a["bound_total"],
          a["declared_ws_adds_bound"],
          "AGREE" if (str(a["bound_distinct"]) == str(a["declared_distinct_bound_added"])
                      and str(a["bound_total"]) == str(a["declared_ws_adds_bound"]))
          else "DISAGREE (instrument finding)"))
    bg = r["bound_gap"]
    p("")
    p("5.3 are the admitted bounds fixed or near-fixed?")
    p("    at each pass's own step_tol: {}".format(dict(bg["cut_step_tol"])))
    p("    at opt_tol = {:.3g}:          {}".format(bg["opt_tol_used"], dict(bg["cut_opt_tol"])))
    p("    decision_tol values observed in the trace: {}".format(bg["opt_tol_observed"]))
    if bg["summary"]:
        p("    bound_gap min {:.6g}  max {:.6g}  infinite on {} passes".format(
            bg["summary"]["min"], bg["summary"]["max"], bg["summary"]["n_inf"]))
        p("    bound_gap deciles: {}".format(
            "  ".join("{:.6g}".format(v) for v in bg["summary"]["deciles"])))
    m = r["multipliers"]
    p("")
    p("5.4 the multiplier at drop against the drop tolerance")
    for label, blk in [("ALL DROPS", m["all"])] + \
            [("route " + k, v) for k, v in sorted(m["by_route"].items())]:
        if blk["n"] == 0 or blk["mu"] is None:
            p("    {:<18} n={}  (no present multiplier)".format(label, blk["n"]))
            continue
        p("    {:<18} n={}".format(label, blk["n"]))
        p("        |mu|      min {:.6g}  median {:.6g}  max {:.6g}".format(
            blk["mu"]["min"], blk["mu"]["median"], blk["mu"]["max"]))
        p("        |mu| deciles: {}".format(
            "  ".join("{:.4g}".format(v) for v in blk["mu"]["deciles"])))
        if blk["mu_over_tol"]:
            p("        |mu|/tol  min {:.6g}  median {:.6g}  max {:.6g}".format(
                blk["mu_over_tol"]["min"], blk["mu_over_tol"]["median"],
                blk["mu_over_tol"]["max"]))
        if blk["mu_over_best"]:
            p("        |mu|/best min {:.6g}  median {:.6g}  max {:.6g}".format(
                blk["mu_over_best"]["min"], blk["mu_over_best"]["median"],
                blk["mu_over_best"]["max"]))
        if blk["candidates"]:
            p("        drop_candidates min {:.0f} median {:.0f} max {:.0f}   "
              "drop_tied max {:.0f}".format(
                  blk["candidates"]["min"], blk["candidates"]["median"],
                  blk["candidates"]["max"], blk["tied"]["max"]))
    rt = r["ratio_ties"]
    p("")
    p("5.5 ratio-test tie multiplicity over ADD passes")
    p("    histogram {}   share with ratio_tied>1 {:.6f}   max {}".format(
        dict(sorted(rt["hist"].items())), rt["share_gt1"], rt["max"]))
    p("    ADD passes with ratio_tied>1: {}   the record's ratio_ties = {}  -> {}".format(
        rt["n_gt1"], rt["declared_ratio_ties"],
        "AGREE" if rt["agree"] else "DISAGREE (instrument finding)"))
    inc = r["incoming"]
    p("")
    p("5.6 the incoming (warm) working set")
    p("    signature {}   size {}   warm {}".format(inc["signature"], inc["size"], inc["warm"]))
    p("    {}".format(inc["note"]))
    h = r["histograms"]
    p("")
    p("5.7 the action and drop-route histograms")
    for act in ACTIONS:
        p("    {:<20} {}".format(act, h["actions"].get(act, 0)))
    p("    routes {}   blocks {}".format(dict(h["routes"]), dict(h["blocks"])))
    w = r["ws_size"]
    p("")
    p("5.8 the working-set size trajectory")
    p("    first {}  last {}  min {}  max {}".format(w["first"], w["last"], w["min"], w["max"]))
    p("    deciles {}".format("  ".join("{:.0f}".format(v) for v in w["deciles"])))
    p("    passes where ws_size_out != ws_size_in: {}   passes with shift_adds>0: {}".format(
        w["changed"], w["shift_adds"]))
    rr = r["raw_ratio"]
    p("")
    p("5.9 the uncapped ratio test")
    p("    ADD passes {}   with raw_ratio > 1: {}".format(rr["n"], rr["gt1"]))
    if rr["summary"]:
        p("    raw_ratio <= 1: min {:.6g}  median {:.6g}  max {:.6g}".format(
            rr["summary"]["min"], rr["summary"]["median"], rr["summary"]["max"]))
        p("    deciles {}".format("  ".join("{:.4g}".format(v) for v in rr["summary"]["deciles"])))


def render_admissions(cell, d, out):
    p = lambda *a: print(*a, file=out)
    r = d["readouts"]
    p("# M7 W2 M3b -- {} -- RULE.md 5.2, the FULL per-identity admission table".format(cell))
    p("# block index admissions readmissions bound_gap")
    gaps = r["bound_gap"]["per_variable"]
    for (b, i), c in sorted(r["admissions"]["full"].items(), key=lambda kv: -kv[1]):
        p("{:<6} {:>6} {:>10} {:>12}  {}".format(
            b, i, c, c - 1,
            "{:.6g}".format(gaps[i]) if b == "bound" and i in gaps else "-"))


# ---------------------------------------------------------------------------
# section 6 -- the nine pre-registered synthetic traces
# ---------------------------------------------------------------------------
TRACE_COLS = ("solve,minor,action,block,block_index,block_side,drop_route,"
              "bound_gap,alpha,p_inf,dx_inf,step_tol,degenerate,ride,raw_ratio,"
              "ratio_tied,ratio_capped,decision_multiplier,decision_tol,"
              "best_multiplier,drop_candidates,drop_tied,shift_adds,ws_size_in,"
              "ws_sig_in,ws_size_out,ws_sig_out,x_inf,x_l1")

SYN_FIELDS = ("factorizations", "schur_updates", "minor_iters", "eqp_refine_steps",
              "border_refine_steps", "verdict_refine_steps", "suspect_escalations",
              "k0_reused", "symbolic_analyses", "ws_adds", "ws_drops", "shift_adds",
              "degenerate_steps", "degenerate_run_max", "ws_adds_bound",
              "ws_drops_bound", "distinct_ineq_added", "distinct_bound_added",
              "drop_ties", "ratio_ties")


def syn_rec(minor, action, block="none", index=-1, side="free", route="none",
            gap="nan", dx="0", st="1e-09", sig_in=0, sig_out=0, size_in=1,
            size_out=1, x_inf="1", x_l1="1", ride="0", mult="nan", tol_="nan",
            best="nan"):
    return ",".join(str(v) for v in (
        0, minor, action, block, index, side, route, gap, "0", "0", dx, st, "0",
        ride, "nan", 1, 1, mult, tol_, best, 0, 0, 0, size_in, sig_in, size_out,
        sig_out, x_inf, x_l1))


def write_syn(dirpath, name, rows, *, n=100, truncated="0", exit_status="max_iter",
              warm="0", solve_index="0", minor_iters=None, drop_field=None,
              row_status="max_iter", omit_field=None):
    """Writes a synthetic trace CSV and the probe row + M3a row that gate it."""
    trace = os.path.join(dirpath, name + ".csv")
    N = len(rows) if minor_iters is None else minor_iters
    with open(trace, "w") as fh:
        fh.write("# cell={}\n# n={}\n# me=0\n# mi=0\n# max_iter={}\n".format(name, n, N))
        fh.write("# incoming_ws_signature=0\n# incoming_ws_size=0\n")
        fh.write("# exit_status={}\n# truncated={}\n# warm={}\n# solve_index={}\n"
                 .format(exit_status, truncated, warm, solve_index))
        fh.write("# records={}\n".format(len(rows)))
        fh.write(TRACE_COLS + "\n")
        for r in rows:
            fh.write(r + "\n")

    def write_row(path, bump=None, omit=None, status="max_iter"):
        with open(path, "w") as fh:
            fh.write("PROBE_BEGIN\nLEDGER_FIRST_BEGIN\nfirst_label=f_qp_0\n"
                     "first_warm=0\nfirst_qp_record_status={}\n".format(status))
            for f in SYN_FIELDS:
                if omit == f:
                    continue
                v = N if f == "minor_iters" else 0
                if bump == f:
                    v += 1
                fh.write("first_{}={}\n".format(f, v))
            fh.write("LEDGER_FIRST_END\nPROBE_END\n")
    row = os.path.join(dirpath, name + ".row")
    m3a = os.path.join(dirpath, name + ".m3a")
    write_row(row)
    write_row(m3a, bump=drop_field, omit=omit_field, status=row_status)
    return trace, row, m3a


def _two_cycle(N=2000):
    """add A / drop A, x constant: RULE.md 6 row 1 -> CYCLING_PROPER."""
    rows = []
    for m in range(N):
        if m % 2 == 0:
            rows.append(syn_rec(m, "add", "bound", 7, "lower", sig_in=111,
                                sig_out=222, size_in=0, size_out=1))
        else:
            rows.append(syn_rec(m, "drop_dantzig", "bound", 7, "lower", "dantzig",
                                sig_in=222, sig_out=111, size_in=1, size_out=0,
                                mult="1e-3", tol_="1e-9", best="1e-3"))
    return rows


def _side_alternation(N=2000):
    """one variable, lower/upper, every set distinct -> SIDE_PING_PONG."""
    rows = []
    for m in range(N):
        side = "lower" if (m // 2) % 2 == 0 else "upper"
        if m % 2 == 0:
            rows.append(syn_rec(m, "add", "bound", 7, side, sig_in=1000 + m,
                                sig_out=2000 + m, gap="0"))
        else:
            rows.append(syn_rec(m, "drop_dantzig", "bound", 7, side, "dantzig",
                                sig_in=1000 + m, sig_out=2000 + m,
                                mult="1e-3", tol_="1e-9", best="1e-3"))
    return rows


def _immediate(N=2000):
    """add A / drop A / add A ..., one side, every set distinct -> IMMEDIATE_READD."""
    rows = []
    for m in range(N):
        if m % 2 == 0:
            rows.append(syn_rec(m, "add", "bound", 7, "lower", sig_in=1000 + m,
                                sig_out=2000 + m))
        else:
            rows.append(syn_rec(m, "drop_dantzig", "bound", 7, "lower", "dantzig",
                                sig_in=1000 + m, sig_out=2000 + m,
                                mult="1e-3", tol_="1e-9", best="1e-3"))
    return rows


def _zero_probe(N=2000):
    """every drop by the probe, a DIFFERENT variable than the one just added,
    every set distinct -> ZERO_PROBE_CHURN."""
    rows = []
    for m in range(N):
        if m % 2 == 0:
            rows.append(syn_rec(m, "add", "bound", 1000 + m, "lower",
                                sig_in=1000 + m, sig_out=2000 + m))
        else:
            rows.append(syn_rec(m, "drop_zero_probe", "bound", m, "lower",
                                "zero_probe", sig_in=1000 + m, sig_out=2000 + m,
                                mult="0", tol_="1e-9", best="0"))
    return rows


def _rides(N=2000):
    """every ADD a ride, every set distinct -> NEGATIVE_CURVATURE_BLOCKERS."""
    rows = []
    for m in range(N):
        if m % 2 == 0:
            rows.append(syn_rec(m, "ride_stepped", "bound", 1000 + m, "lower",
                                sig_in=1000 + m, sig_out=2000 + m, ride="1"))
        else:
            rows.append(syn_rec(m, "drop_dantzig", "bound", m, "lower", "dantzig",
                                sig_in=1000 + m, sig_out=2000 + m,
                                mult="1e-3", tol_="1e-9", best="1e-3"))
    return rows


def _crawl(N=2000):
    """a monotone crawl, every set distinct -> DEGENERATE_CRAWL."""
    return [syn_rec(m, "add", "bound", 1000 + m, "lower", sig_in=1000 + m,
                    sig_out=1001 + m, size_in=m, size_out=m + 1) for m in range(N)]


def _mixed(N=2000):
    """sets repeat with period 200, but EVERY pass moves the iterate, so 3.1's
    second conjunct fails; nothing else reaches its bar -> MIXED."""
    rows = []
    for m in range(N):
        x = "{:.17g}".format(1.0 + m * 1e-3)
        if m % 2 == 0:
            rows.append(syn_rec(m, "add", "bound", 1000 + m, "lower",
                                sig_in=m % 200, sig_out=m % 200, dx="1e-3",
                                x_inf=x, x_l1=x))
        else:
            rows.append(syn_rec(m, "drop_dantzig", "bound", m, "lower", "dantzig",
                                sig_in=m % 200, sig_out=m % 200, dx="1e-3",
                                x_inf=x, x_l1=x, mult="1e-3", tol_="1e-9",
                                best="1e-3"))
    return rows


def selftest(out=sys.stdout):
    """RULE.md section 6. Each synthetic must land in its registered class."""
    p = lambda *a: print(*a, file=out)
    failed = 0
    total = 0
    with tempfile.TemporaryDirectory() as tmp:
        cases = [
            ("6.1 strict two-cycle, x constant", "twocycle", _two_cycle(), {},
             "CYCLING_PROPER"),
            ("6.2 lower/upper alternation on one variable", "sides",
             _side_alternation(), {}, "SIDE_PING_PONG"),
            ("6.3 immediate drop/re-add, sets drifting", "immediate",
             _immediate(), {}, "IMMEDIATE_READD"),
            ("6.4 every drop by the zero-multiplier probe", "probe",
             _zero_probe(), {}, "ZERO_PROBE_CHURN"),
            ("6.5 every add a negative-curvature ride", "rides", _rides(), {},
             "NEGATIVE_CURVATURE_BLOCKERS"),
            ("6.6 a monotone crawl, every set distinct", "crawl", _crawl(), {},
             "DEGENERATE_CRAWL"),
            ("6.7 none of the above at their bars", "mixed", _mixed(), {},
             "MIXED"),
            ("6.8a an EMPTY trace", "empty", [], {"minor_iters": 10},
             "ABORTED"),
            ("6.8b a trace carrying an `unset` action", "unset",
             [syn_rec(m, "unset") for m in range(10)], {}, "ABORTED"),
            ("6.8c truncated = true", "trunc",
             [syn_rec(m, "add", "bound", 7, "lower") for m in range(10)],
             {"truncated": "1"}, "ABORTED"),
            ("6.9a an identity row with 19 of the 20 fields", "missing",
             [syn_rec(m, "add", "bound", 7, "lower") for m in range(10)],
             {"omit_field": "drop_ties"}, "ABORTED"),
            ("6.9b an identity row with one field DIFFERENT", "differs",
             [syn_rec(m, "add", "bound", 7, "lower") for m in range(10)],
             {"drop_field": "ws_adds"}, "ABORTED"),
        ]
        for title, name, rows, kw, want in cases:
            total += 1
            trace, row, m3a = write_syn(tmp, name, rows, **kw)
            got, d = classify(name, trace, row, m3a, out=out)
            ok = (got == want)
            failed += 0 if ok else 1
            p("FOLD {:<46} want={:<28} got={:<28} {}".format(
                title, want, got, "OK" if ok else "*** FAILED ***"))
            if got == "ABORTED":
                p("        {}".format(d["reason"]))
            elif not ok:
                p("        holds: {}".format(
                    {k: v for k, v in d["holds"].items() if v} or "nothing"))
        p("")
        p("WALK_FOLDS_TOTAL={}  WALK_FOLDS_FAILED={}".format(total, failed))
        distinct = len({c[4] for c in cases})
        p("distinct classes exercised: {} (a classifier that returned one class "
          "for all of them would be vacuous)".format(distinct))
    return 1 if failed else 0


# ---------------------------------------------------------------------------
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--leg", action="append", default=[],
                    metavar="NAME,TRACE,ROW,M3A_ROW",
                    help="a cell to classify: four comma-separated paths")
    ap.add_argument("--verdict", help="where verdict.txt is written")
    ap.add_argument("--tables", help="directory for the per-cell tables")
    ap.add_argument("--selftest", action="store_true",
                    help="RULE.md section 6's nine pre-registered synthetics")
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest()
    if not args.leg:
        ap.error("nothing to do: pass --leg or --selftest")

    results = []
    for spec in args.leg:
        parts = spec.split(",")
        if len(parts) != 4:
            ap.error("--leg wants NAME,TRACE,ROW,M3A_ROW, got {!r}".format(spec))
        cell, trace, row, m3a = parts
        outcome, d = classify(cell, trace, row, m3a)
        results.append((cell, outcome, d))

    vfh = open(args.verdict, "w") if args.verdict else sys.stdout
    print("M7 W2 M3b -- VERDICT: RULE.md's classifier over the first-QP "
          "per-minor trace", file=vfh)
    print("The outcome below is whatever the pre-registered rule returns. No "
          "bar was moved,", file=vfh)
    print("no predicate relaxed, and no verdict here licenses a behaviour or "
          "default change.", file=vfh)
    print("", file=vfh)
    print("SUMMARY", file=vfh)
    for cell, outcome, d in results:
        print("  {:<12} {}".format(cell, outcome), file=vfh)
    print("", file=vfh)
    for cell, outcome, d in results:
        render(cell, outcome, d, vfh)
        print("", file=vfh)
    for cell, outcome, d in results:
        if outcome != "ABORTED":
            render_readouts(cell, d, vfh)
            print("", file=vfh)
    if args.verdict:
        vfh.close()

    if args.tables:
        os.makedirs(args.tables, exist_ok=True)
        for cell, outcome, d in results:
            if outcome == "ABORTED":
                continue
            with open(os.path.join(args.tables, cell.lower() + "-admissions.txt"), "w") as fh:
                render_admissions(cell, d, fh)
            with open(os.path.join(args.tables, cell.lower() + "-readouts.txt"), "w") as fh:
                render_readouts(cell, d, fh)
            with open(os.path.join(args.tables, cell.lower() + "-predicates.txt"), "w") as fh:
                render(cell, outcome, d, fh)
    return 0


if __name__ == "__main__":
    sys.exit(main())
