#!/usr/bin/env python3
"""M7 W2 E1c -- the READ-OUT over the eight legs (ON/OFF x four network cells).

READ-ONLY.  No solver runs here.  Every number is read from the committed rows
(`rows/`), the pinned flat traces, and the copied `m4c_tails.py` output
(`tables/tails-<leg>.json`, produced by `wrappers/m4c_tails.py`, byte-identical
to M4c's/M5's).

WHAT IS IMPORTED UNMODIFIED:
  * `readouts_driver_m4b.predicate` (M4b's RECURRENT predicate T1/T2/T3 through
    `readouts_m4.maximal_periodic_tail`, plus sum_dx_inf over the solve) and its
    `read_flat` / `by_solve` / `typed_rows`, from M4b's committed wrappers dir.

WHAT IS DERIVED HERE, and labelled DERIVED in every table:
  * the SOLVE ROLE (main / elastic / restoration) of each traced solve, by a
    sequential walk of the row's E1C_HISTORY block against its SOLVE_SUMMARY
    block, with an ASSERTION on every main-QP minor count and on the
    restoration QP count == `restoration_iters`.  Never typed.
  * Q2's over-count `me' + ws_size - n` per pass, with me' = the trace header's
    `me` (the observer's own qp.me(); == the row's `eqb_me_reduced` on every
    leg, asserted) and n = the trace header's `n`.  Valid ONLY on main-QP
    solves (the elastic and restoration solves have other dimensions); printed
    as `n/a` elsewhere.  M5 never wrote `dep_rows_after_drop`; these are the
    same `qp.me() + qp_walk_ws_size(ws)` quantity, recomposed.

Usage: readout_e1c.py ROWS_DIR TRACES_DIR TAILS_DIR OUT_DIR
"""

import json
import os
import sys
from collections import OrderedDict

HERE = os.path.dirname(os.path.abspath(__file__))
M4B = os.path.abspath(os.path.join(HERE, "..", "..",
                                   "2026-09-m7-w2-m4b-expand-on-the-ten", "wrappers"))
sys.path.insert(0, M4B)
import readouts_driver_m4b as D  # noqa: E402

CELLS = ("DALLASM", "STEENBRB", "STEENBRC", "STEENBRG")
LEGS = [("on", c) for c in CELLS] + [("off", c) for c in CELLS]


def parse_row(path):
    kv, blocks, cur = OrderedDict(), {}, None
    with open(path) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.endswith("_BEGIN"):
                cur = line[:-6]
                blocks[cur] = []
                continue
            if line.endswith("_END"):
                cur = None
                continue
            if cur is not None:
                blocks[cur].append(line)
            if "=" in line and "," not in line.split("=", 1)[0]:
                k, v = line.split("=", 1)
                if k not in kv:
                    kv[k] = v
    return kv, blocks


def csv_block(lines):
    rows = [l for l in lines if "," in l and "=" not in l]
    names = rows[0].split(",")
    return [dict(zip(names, r.split(","))) for r in rows[1:]]


def roles(kv, hist, summ):
    """Sequential walk; returns {solve: (role, major)}; asserts as it goes."""
    out, s = {}, 0
    resto_left = int(kv["restoration_iters"])
    n_solves = len(summ)
    for h in hist:
        major = int(h["row"])
        mi = int(h["qp_minor_iters"])
        if h["elastic_applied"] == "0":
            assert int(summ[s]["minors"]) == mi, (kv["problem"], major, s)
            out[s] = ("main", major)
            s += 1
            continue
        # elastic major: the main QP (kInfeasible -- the only status the
        # elastic tier consumes), then rungs until the one whose minors the
        # row reports, then (verdict Restore) the restoration sub-solve's QPs.
        out[s] = ("main(infeasible->elastic)", major)
        s += 1
        while True:
            out[s] = ("elastic_rung", major)
            last = int(summ[s]["minors"])
            s += 1
            if last == mi:
                break
        if h["verdict"] == "Restore":
            for _ in range(resto_left):
                out[s] = ("restoration_qp", major)
                s += 1
            resto_left = 0
    assert s == n_solves, (kv["problem"], s, n_solves)
    return out


def main():
    rows_dir, traces_dir, tails_dir, out_dir = sys.argv[1:5]
    os.makedirs(out_dir, exist_ok=True)
    table = []
    ladder = []
    for arm, cell in LEGS:
        leg = "%s-%s" % (arm, cell)
        kv, blocks = parse_row(os.path.join(rows_dir, leg + ".txt"))
        hist = csv_block(blocks["E1C_HISTORY"])
        summ = csv_block(blocks["SOLVE_SUMMARY"])
        role = roles(kv, hist, summ)
        for h in hist:
            major = int(h["row"])
            ss = [k for k, v in sorted(role.items()) if v[1] == major]
            ladder.append(OrderedDict(
                leg=leg, major=major, tr_radius=h["tr_radius"],
                qp_status=h["qp_status"], qp_minor_iters=h["qp_minor_iters"],
                verdict=h["verdict"], elastic_applied=h["elastic_applied"],
                restoration_seed_used=h["restoration_seed_used"],
                f=h["f"], feasibility=h["feasibility"],
                solves=";".join("s%d:%s" % (k, role[k][0]) for k in ss)))
        head, cols, lines, hdr = D.read_flat(os.path.join(traces_dir, leg + ".csv"))
        n, me = int(hdr["n"]), int(hdr["me"])
        assert me == int(kv["eqb_me_reduced"]), (leg, me, kv["eqb_me_reduced"])
        assert n == int(kv["n"]), leg
        with open(os.path.join(tails_dir, "tails-%s.json" % leg)) as fh:
            tails = {int(t["solve"]): t for t in json.load(fh)}
        for k, ls in D.by_solve(lines).items():
            raws = D.typed_rows(cols, ls)
            p = D.predicate(raws)
            r = OrderedDict(leg=leg, solve=k, role=role[k][0], major=role[k][1],
                            N=p["N"], period=p["p"], t0=p["t0"],
                            tail=p.get("tail", 0),
                            tail_share=round(p["tail_share"], 4),
                            recurrent=p["recurrent"],
                            sum_dx_inf_solve=p["sum_dx_inf"])
            tail = raws[p["t0"]:] if p["p"] is not None else []
            tdx = [D.fnum(x["dx_inf"]) for x in tail]
            r["sum_dx_inf_tail"] = sum(v for v in tdx if v is not None)
            talpha = [D.fnum(x["alpha"]) for x in tail]
            talpha = [v for v in talpha if v is not None]
            r["tail_alpha_max_abs"] = max((abs(v) for v in talpha), default=None)
            r["tail_actions"] = "/".join(sorted(set(x["action"] for x in tail)))
            r["tail_identities"] = "/".join(sorted(set(
                "%s:%s:%s" % (x["block"], x["block_index"], x["block_side"])
                for x in tail if x["block"] != "none")))
            if role[k][0].startswith("main"):
                oc_in = [me + int(x["ws_size_in"]) - n for x in tail]
                oc_out = [me + int(x["ws_size_out"]) - n for x in tail]
                oc_all = [me + int(x["ws_size_out"]) - n for x in raws]
                r["DERIVED_overcount_tail_min"] = min(oc_in + oc_out) if tail else None
                r["DERIVED_overcount_tail_max"] = max(oc_in + oc_out) if tail else None
                r["DERIVED_overcount_solve_max"] = max(oc_all)
                r["DERIVED_first_pass_overcount_gt0"] = next(
                    (int(x["minor"]) for x in raws
                     if me + int(x["ws_size_out"]) - n > 0), None)
            else:
                r["DERIVED_overcount_tail_min"] = "n/a"
                r["DERIVED_overcount_tail_max"] = "n/a"
                r["DERIVED_overcount_solve_max"] = "n/a"
                r["DERIVED_first_pass_overcount_gt0"] = "n/a"
            t = tails.get(k)
            if t is not None:
                r["m4c_class"] = t.get("classification")
                r["m4c_unit_classes"] = json.dumps(t.get("unit_classes", {}),
                                                   sort_keys=True)
                r["m4c_units"] = t.get("units")
                r["m4c_core_readd_units"] = t.get("core_readd_units")
                r["m4c_core_alpha_zero"] = t.get("core_alpha_zero")
                r["m4c_rep_dropped"] = t.get("rep_dropped")
                r["m4c_rep_added"] = t.get("rep_added")
                r["m4c_why"] = t.get("why")
            table.append(r)
        print("%s: n=%d me'=%d solves=%d roles ok" % (leg, n, me, len(summ)))
    keys = []
    for r in table:
        for k in r:
            if k not in keys:
                keys.append(k)
    with open(os.path.join(out_dir, "q1-q2-per-solve.csv"), "w") as fh:
        fh.write(",".join(keys) + "\n")
        for r in table:
            fh.write(",".join('"%s"' % r.get(k, "") if "," in str(r.get(k, ""))
                              else str(r.get(k, "")) for k in keys) + "\n")
    lk = list(ladder[0].keys())
    with open(os.path.join(out_dir, "ladder-per-major.csv"), "w") as fh:
        fh.write(",".join(lk) + "\n")
        for r in ladder:
            fh.write(",".join(str(r[k]) for k in lk) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
