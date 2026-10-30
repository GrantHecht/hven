#!/usr/bin/env python3
"""M7 W2 M4b -- ONE FULL PERIOD of ACOPP57's tail, record by record, off vs E.

**POST-HOC**, answered only from traces already taken.  `RULE.md` section 6
pre-registers M4 sections 6.2-6.6 for the ten cells; it does NOT pre-register
this reconstruction, which the settler asked for after the chain had run.  No
solver runs here and no verdict rests on it.

Emits every trace column for one period of the maximal exactly-periodic suffix,
under both arms, plus the identities the read asserts, each re-derived in IEEE
double and compared for BIT equality.
"""
import sys

TAU = 4.9000000000000006e-14      # this leg's own printed arm_tau


def load(path):
    with open(path, errors="replace") as fh:
        lines = [l.rstrip("\n") for l in fh if not l.startswith("#")]
    cols = lines[0].split(",")
    return cols, [dict(zip(cols, l.split(","))) for l in lines[1:]]


def main():
    offp, ep, t0, p, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), \
        int(sys.argv[4]), sys.argv[5]
    cols, O = load(offp)
    _, E = load(ep)
    w = open(out, "w").write
    w("# M7 W2 M4b — **POST-HOC**: one full period of `ACOPP57`'s tail,\n")
    w("# record by record, arm `off` against arm `E`\n\n")
    w("Written by `wrappers/acopp57_period_m4b.py` from the committed slices\n")
    w("`traces-m4b/off-ACOPP57-solve6.csv` and `traces-m4b/E-ACOPP57-solve6.csv`.\n")
    w("**No solver ran here.** **POST-HOC**: `RULE.md` §6 does not pre-register\n")
    w("this reconstruction, and nothing in `verdict.txt` rests on it.\n\n")
    w("The solve is `ACOPP57` solve 6 — the run's last QP, the one the flat\n")
    w("trace's own header describes (`solve_index=6`, `max_iter=1795`,\n")
    w("`exit_status=max_iter`). Its maximal exactly-periodic suffix on the\n")
    w("bit-exact F0 state key has **period 2** under both arms\n")
    w("(`tables/readouts-m4b.md`). The two records below are minors %d and %d,\n"
      % (t0, t0 + 1))
    w("well inside that suffix.\n\n")
    for i in range(p):
        k = t0 + i
        w("## Record %d of the period — minor %s\n\n" % (i + 1, O[k]["minor"]))
        w("| column | `off` | `E` | |\n|---|---|---|---|\n")
        for c in cols:
            a, b = O[k][c], E[k][c]
            if c == "candidates" and a:
                a = a.replace(";", "; ")
                b = b.replace(";", "; ")
            w("| `%s` | `%s` | `%s` | %s |\n"
              % (c, a, b, "**DIFFERS**" if O[k][c] != E[k][c] else ""))
        w("\n")
    w("## The period repeats EXACTLY\n\n")
    # `solve` and `minor` are the record's ADDRESS, not its state, and the tail
    # finder excludes them for that reason: `classify_cause_fix1.F0` is
    # `COLUMNS[2:COLUMNS.index("inertia_verdict")]`, i.e. everything from
    # `action` onward and nothing before it. The same two are excluded here,
    # and the column count compared is printed so the exclusion is visible.
    state = [c for c in cols if c not in ("solve", "minor")]
    same_o = all(all(O[t0 + j][c] == O[t0 + j + p][c] for c in state)
                 for j in range(p))
    same_e = all(all(E[t0 + j][c] == E[t0 + j + p][c] for c in state)
                 for j in range(p))
    w("Every column of minors %d–%d **except `solve` and `minor`, which are\n"
      % (t0, t0 + p - 1))
    w("the record's ADDRESS and not its state**, is byte-identical to the same\n")
    w("column of minors %d–%d: under `off` **%s**, under `E` **%s**.\n"
      % (t0 + p, t0 + 2 * p - 1, "TRUE" if same_o else "FALSE",
         "TRUE" if same_e else "FALSE"))
    w("That is %d of the trace's %d columns — a SUPERSET of the key the tail\n"
      % (len(state), len(cols)))
    w("finder itself uses (`classify_cause_fix1.F0` is\n")
    w("`COLUMNS[2:COLUMNS.index(\"inertia_verdict\")]`, which stops before the\n")
    w("inertia and cost columns). This check does not stop there and still\n")
    w("finds exact repetition.\n\n")

    w("## The identities this read asserts, re-derived in IEEE double\n\n")
    a = E[t0]
    alpha = float(a["alpha"])
    pr = float(a["p_released"])
    pinf = float(a["p_inf"])
    dx = float(a["dx_inf"])
    xinf_add = float(a["x_inf"])
    xinf_drop = float(E[t0 + 1]["x_inf"])
    w("`tau` is this leg's own printed `arm_tau` = `%r`.\n\n" % TAU)
    w("| identity | left | right | BIT-equal |\n|---|---|---|---|\n")
    w("| `alpha` == `tau / \\|p_released\\|` (the EXPAND floor `alpha_min`) |"
      " `%r` | `%r` | **%s** |\n"
      % (alpha, TAU / abs(pr), alpha == TAU / abs(pr)))
    w("| `dx_inf` == `alpha * p_inf` | `%r` | `%r` | **%s** |\n"
      % (dx, alpha * pinf, dx == alpha * pinf))
    w("| `alpha * \\|p_released\\|` == `tau` (the released variable moves"
      " exactly tau) | `%r` | `%r` | **%s** |\n"
      % (alpha * abs(pr), TAU, alpha * abs(pr) == TAU))
    w("| next record's `x_inf` == this record's `x_inf` + `tau` |"
      " `%r` | `%r` | **%s** |\n"
      % (xinf_drop, xinf_add + TAU, xinf_drop == xinf_add + TAU))
    o = O[t0]
    w("| `off`'s `alpha` on the same record is exactly zero |"
      " `%s` | `0` | **%s** |\n" % (o["alpha"], float(o["alpha"]) == 0.0))
    w("| `off`'s `x_inf` does NOT move across the period |"
      " `%s` | `%s` | **%s** |\n"
      % (o["x_inf"], O[t0 + 1]["x_inf"], o["x_inf"] == O[t0 + 1]["x_inf"]))
    w("\n")
    print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
