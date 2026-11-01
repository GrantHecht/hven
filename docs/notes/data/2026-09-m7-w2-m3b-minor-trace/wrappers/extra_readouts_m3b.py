#!/usr/bin/env python3
"""M7 W2 M3b -- read-outs the settler asked for that RULE.md did NOT pre-register.

EVERYTHING THIS FILE PRODUCES IS OUTSIDE THE RULE. It is reported in its own
clearly separated section of the README and the report, and it cannot change,
soften or strengthen the outcome `classify_walk_m3b.py` returns. It exists
because the settler asked four questions of the trace after the rule had been
written:

  (a) at which minor does the FINAL cycle begin, what is its period, and how
      many passes does it consume?
  (b) does the walk pass through SEVERAL such cycles on different variables
      before the final one, or does one variable dominate?
  (c) the same for the second cell;
  (d) for the cycling pair, what does the trace show about WHY a bound dropped
      on a wrong-sign multiplier immediately blocks again at zero step -- the
      ratio-test minimum, alpha, the ride flag, the degenerate flag, and the
      suspect-escalation / restart actions in the neighbourhood?

A "cycle segment" here is a maximal run of passes that strictly alternates
`drop_dantzig`/`drop_zero_probe` of one identity with an `add`/`ride_stepped`
of the SAME identity. That definition is this file's own, invented after the
trace was read, and it is stated so that a reader can see it is not RULE.md
section 3.1's (which is a statement about working-set SIGNATURES and the
iterate, not about one variable).
"""
import argparse
import os
import sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from classify_walk_m3b import load_trace, ADD_ACTIONS, DROP_ACTIONS  # noqa: E402

# The fields a record repeats byte-for-byte inside an exact cycle; `minor` is
# excluded because it necessarily counts up.
REPEAT_FIELDS = ("action", "block", "block_index", "block_side", "drop_route",
                 "bound_gap", "alpha", "p_inf", "dx_inf", "step_tol",
                 "degenerate", "ride", "raw_ratio", "ratio_tied",
                 "decision_multiplier", "decision_tol", "best_multiplier",
                 "drop_candidates", "drop_tied", "ws_size_in", "ws_sig_in",
                 "ws_size_out", "ws_sig_out", "x_inf", "x_l1")


def ident(r):
    if r["block"] == "none":
        return None
    return (r["block"], r["block_index"])


def segments(recs, min_len=4):
    """Maximal alternating drop/add runs on ONE identity, length >= min_len."""
    out, i, N = [], 0, len(recs)
    while i < N:
        r = recs[i]
        d = ident(r)
        if d is None or r["action"] not in (DROP_ACTIONS + ADD_ACTIONS):
            i += 1
            continue
        j = i
        want_drop = r["action"] in DROP_ACTIONS
        while j < N:
            rj = recs[j]
            if ident(rj) != d:
                break
            in_drop = rj["action"] in DROP_ACTIONS
            in_add = rj["action"] in ADD_ACTIONS
            if want_drop and not in_drop:
                break
            if (not want_drop) and not in_add:
                break
            want_drop = not want_drop
            j += 1
        if j - i >= min_len:
            out.append({"start": i, "end": j - 1, "len": j - i, "ident": d,
                        "side": recs[i]["block_side"]})
            i = j
        else:
            i += 1
    return out


def byte_repeat(recs, lo, hi, period=2):
    """Is [lo, hi] an EXACT repetition of its first `period` passes, on every
    recorded field but the minor index?"""
    base = recs[lo:lo + period]
    for m in range(lo, hi + 1):
        b = base[(m - lo) % period]
        for f in REPEAT_FIELDS:
            if recs[m][f] != b[f] and not (recs[m][f] != recs[m][f] and b[f] != b[f]):
                return False, (m, f, recs[m][f], b[f])
    return True, None


def show(rec, keys):
    return "  ".join("{}={}".format(k, rec[k]) for k in keys)


def report(cell, trace_path, out):
    p = lambda *a: print(*a, file=out)
    header, recs = load_trace(trace_path)
    N = len(recs)
    p("=" * 78)
    p("{} -- READ-OUTS OUTSIDE RULE.md (not pre-registered, not a verdict)".format(cell))
    p("=" * 78)
    p("records {}   n {}   me {}   mi {}".format(N, header.get("n"),
                                                 header.get("me"), header.get("mi")))
    segs = segments(recs)
    p("")
    p("(a) THE FINAL CYCLE")
    if not segs or segs[-1]["end"] != N - 1:
        p("    the trace does not END inside an alternating drop/add run on one "
          "identity")
    else:
        s = segs[-1]
        exact, why = byte_repeat(recs, s["start"], s["end"])
        p("    begins at minor {}   ends at minor {} (the last pass of the walk)"
          .format(s["start"], s["end"]))
        p("    identity {}{} at side {}   period 2 (one drop, one add)".format(
            s["ident"][0], s["ident"][1], s["side"]))
        p("    passes consumed {} of {} = {:.4f} of the whole walk".format(
            s["len"], N, s["len"] / N))
        p("    every recorded field but `minor` repeats with period 2: {}{}".format(
            exact, "" if exact else "  first departure {}".format(why)))
    p("")
    p("(b) IS THERE ONE CYCLE OR SEVERAL? all alternating runs of length >= 4")
    p("    {:>6} {:>6} {:>6}  {:<14} {}".format("start", "end", "len", "identity", "side"))
    for s in segs:
        p("    {:>6} {:>6} {:>6}  {:<14} {}".format(
            s["start"], s["end"], s["len"], "{}{}".format(*s["ident"]), s["side"]))
    p("    runs found: {}".format(len(segs)))
    adm = Counter(ident(r) for r in recs
                  if r["action"] in ADD_ACTIONS and r["block"] != "none")
    tot = sum(adm.values())
    p("")
    p("    per-variable ADMISSION table (all identities, admissions desc):")
    p("    {:<14} {:>12} {:>14} {:>10}".format("identity", "admissions",
                                               "re-admissions", "share"))
    for d, c in adm.most_common():
        p("    {:<14} {:>12} {:>14} {:>9.4f}".format(
            "{}{}".format(*d), c, c - 1, c / tot))
    p("    distinct identities {}   admissions {}".format(len(adm), tot))
    once = sum(1 for c in adm.values() if c == 1)
    p("    identities admitted EXACTLY ONCE: {}   the single top identity holds "
      "{:.4f} of all admissions".format(once, adm.most_common(1)[0][1] / tot))
    p("")
    p("(d) THE CYCLING PAIR, FIELD BY FIELD")
    if segs and segs[-1]["end"] == N - 1:
        s = segs[-1]
        lo = s["start"]
        for m in (lo, lo + 1):
            r = recs[m]
            p("    minor {}  action {}".format(m, r["action"]))
            p("        {}".format(show(r, ("block", "block_index", "block_side",
                                           "drop_route", "bound_gap"))))
            p("        {}".format(show(r, ("alpha", "p_inf", "dx_inf", "step_tol",
                                           "raw_ratio", "ratio_tied", "ratio_capped"))))
            p("        {}".format(show(r, ("degenerate", "ride"))))
            p("        {}".format(show(r, ("decision_multiplier", "decision_tol",
                                           "best_multiplier", "drop_candidates",
                                           "drop_tied"))))
            p("        {}".format(show(r, ("ws_size_in", "ws_sig_in", "ws_size_out",
                                           "ws_sig_out", "shift_adds"))))
            p("        {}".format(show(r, ("x_inf", "x_l1"))))
        p("    the 20 passes BEFORE the cycle starts:")
        for m in range(max(0, lo - 20), lo):
            r = recs[m]
            p("        {:>5} {:<16} {}{:<6} {:<6} alpha={:<12} p_inf={:<14} "
              "dx={:<12} mu={:<14} cand={} deg={} ride={}".format(
                  m, r["action"], r["block"][:1], r["block_index"], r["block_side"],
                  r["alpha"], r["p_inf"], r["dx_inf"], r["decision_multiplier"],
                  r["drop_candidates"], int(r["degenerate"]), int(r["ride"])))
    p("")
    p("    actions anywhere in the walk that would show the engine reacting to "
      "the stall:")
    acts = Counter(r["action"] for r in recs)
    for a in ("suspect_escalation", "start_repair", "post_probe_restart",
              "drop_zero_probe", "ride_stepped", "ride_unbounded", "terminal",
              "move_only"):
        p("        {:<20} {}".format(a, acts.get(a, 0)))
    p("    the record's suspect_escalations counter is in the committed row; the "
      "trace's own count of that action is above.")


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--leg", action="append", default=[], metavar="NAME,TRACE")
    ap.add_argument("--out")
    args = ap.parse_args(argv)
    fh = open(args.out, "w") if args.out else sys.stdout
    print("M7 W2 M3b -- read-outs OUTSIDE RULE.md. Not pre-registered, not a "
          "verdict, and", file=fh)
    print("incapable of changing the outcome the rule returned. The settler "
          "asked for these", file=fh)
    print("after the rule was written; they are reported here, separately, for "
          "that reason.", file=fh)
    print("", file=fh)
    for spec in args.leg:
        cell, trace = spec.split(",")
        report(cell, trace, fh)
        print("", file=fh)
    if args.out:
        fh.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
