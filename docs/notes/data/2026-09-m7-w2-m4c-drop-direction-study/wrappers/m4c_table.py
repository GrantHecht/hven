#!/usr/bin/env python3
"""M7 W2 M4c -- the read-out of `m4c_tails.py`'s JSON as markdown tables.

Derives from `.scratch/m7w2m4c/out/tails.json` ONLY.  It computes nothing the
JSON does not already carry except sums and shares, and it leaves the JSON
alone.
"""
import json
import sys
from collections import OrderedDict


def g(r, k):
    return r.get("rep_" + k)


def f(x, fmt="%.3g"):
    if x is None:
        return "—"
    if isinstance(x, float):
        return fmt % x
    return str(x)


def main():
    rows = json.load(open(sys.argv[1]))
    out = open(sys.argv[2], "w")
    w = out.write

    w("## A. Per recurrent solve — the tail, its representative unit, "
      "and the classification\n\n")
    w("One row per RECURRENT OFF solve (52 of them). `units` is the number of "
      "drop→post-drop pairs inside the tail; `cls dist` is the classification "
      "of EVERY one of them; the printed numbers are the LAST unit of the "
      "tail (the declared representative).\n\n")
    w("| cell | solve | N | period | \\|T\\| | T/N | units | cls dist | "
      "dropped | re-adds same id | me+\\|W\\| − n (after drop) | "
      "\\|λ\\|/opt_tol | \\|λ\\|/(ε·stat_scale) | drop cands / tied | "
      "p[released] | sign | \\|p_r\\|/‖p‖∞ | ‖p‖∞ | α | ratio tied | "
      "inertia obs/exp | excess | verdict | curv fired | ride | "
      "active_res/step_tol | res/(μ·scale) | CLASS |\n")
    w("|" + "---|" * 28 + "\n")
    for r in sorted(rows, key=lambda x: (x["cell"], x["solve"])):
        if r.get("units", 0) == 0:
            w("| %s | %d | %d | %s | %d | — | 0 | — | — | — | — | — | — | — |"
              " — | — | — | — | — | — | — | — | — | — | — | — | — | "
              "NO UNIT |\n" % (r["cell"], r["solve"], r["N"], r["period"],
                               r["tail"]))
            continue
        pr = g(r, "p_released")
        w("| %s | %d | %d | %s | %d | %.4f | %d | %s | %s | %s | %+d | %s | %s "
          "| %d/%d | %s | %s | %s | %s | %s | %d | (%d,%d)/(%d,%d) | %+d | %d "
          "| %s | %s | %s | %s | **%s** |\n" % (
              r["cell"], r["solve"], r["N"], r["period"], r["tail"],
              r["tail_share"], r["units"],
              " ".join("%s:%d" % kv for kv in sorted(r["unit_classes"].items())),
              g(r, "dropped"),
              "yes" if g(r, "added") == g(r, "released") else "no (%s)" % g(r, "added"),
              g(r, "kkt_rows_A") - g(r, "n"),
              f(g(r, "lambda_over_tol")), f(g(r, "lambda_rel_eps")),
              g(r, "drop_candidates"), g(r, "drop_tied"),
              f(pr), "−" if (pr is not None and pr < 0) else "+",
              f(g(r, "p_released_over_p_inf")), f(g(r, "p_inf_A")),
              g(r, "alpha_raw_text"), g(r, "ratio_tied"),
              g(r, "inertia_pos"), g(r, "inertia_neg"),
              g(r, "inertia_expected_pos"), g(r, "inertia_expected_neg"),
              g(r, "neg_excess") if g(r, "neg_excess") is not None else 0,
              g(r, "inertia_verdict"), g(r, "curvature_fired"),
              g(r, "ride_outcome"), f(g(r, "res_over_step_tol")),
              f(g(r, "res_over_mu_scale")), r["classification"]))

    # --- per cell rollup ---------------------------------------------------
    w("\n## B. Per cell — every tail unit of every recurrent solve\n\n")
    cells = OrderedDict()
    for r in rows:
        c = cells.setdefault(r["cell"], {"solves": 0, "units": 0,
                                         "dist": {}, "arms": {}})
        c["solves"] += 1
        c["units"] += r.get("units", 0)
        for k, v in r.get("unit_classes", {}).items():
            c["dist"][k] = c["dist"].get(k, 0) + v
        for a in r.get("arms_held", []):
            c["arms"][a] = c["arms"].get(a, 0) + 1
    w("| cell | recurrent solves | tail units | classification of the units | "
      "cell verdict | arms held on the representatives |\n")
    w("|---|---|---|---|---|---|\n")
    for c, d in sorted(cells.items()):
        if not d["dist"]:
            w("| %s | %d | 0 | — | NO UNIT | — |\n" % (c, d["solves"]))
            continue
        tot = sum(d["dist"].values())
        top = max(d["dist"].items(), key=lambda kv: kv[1])
        w("| %s | %d | %d | %s | **%s** (%.1f%%) | %s |\n" % (
            c, d["solves"], d["units"],
            " ".join("%s:%d" % kv for kv in sorted(d["dist"].items())),
            top[0], 100.0 * top[1] / tot,
            " ".join("%s:%d" % kv for kv in sorted(d["arms"].items()))))

    w("\n## C. Totals over the 52 recurrent OFF solves\n\n")
    tot = {}
    for r in rows:
        for k, v in r.get("unit_classes", {}).items():
            tot[k] = tot.get(k, 0) + v
    n = sum(tot.values())
    w("| class | tail units | share |\n|---|---|---|\n")
    for k in "abcde":
        w("| %s | %d | %.4f |\n" % (k, tot.get(k, 0), tot.get(k, 0) / n))
    w("| **total** | **%d** | 1.0000 |\n" % n)
    out.close()
    print("written:", sys.argv[2])


if __name__ == "__main__":
    main()
