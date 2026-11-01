#!/usr/bin/env python3
"""M7 W2 M2b: folds the calibration legs into the tables, the OFF-identity
check and the verdict.

MACHINE-WRITTEN, so no number in the artifact is transcribed by hand.

  in   out/screen/<CELL>.txt     the probe at zeta = 0, with the prox_rel column
       out/c<k>/<CELL>.txt       the probe at candidate k's zeta (k = 1..4)
       docs/notes/data/2026-09-m7-w2-m2-restoration/ladder/<CELL>.txt   (M2, cited)
  out  off-identity.txt          the screen arm against M2's committed ladder
       tables/<CELL>.md          the cell's four candidate arms, and the screen
       tables/summary.md         the S5 record per cell per candidate
       verdict.txt               RULE.md section 2, applied to the screen arm

The rule is RULE.md's, committed before any leg ran; this script implements it
and nothing else.
"""
import os
import sys

ROOT = "/home/ghecht/Projects/hven"
S = os.path.join(ROOT, ".scratch/m7w2m2b")
OUT = os.path.join(S, "out")
M2 = os.path.join(ROOT, "docs/notes/data/2026-09-m7-w2-m2-restoration/ladder")

CELLS = ["STEENBRB", "STEENBRC", "STEENBRG", "ZAMB2-8", "YORKNET", "BRITGAS", "READING6"]
RHO = 1.0e3
# (n, h_k) from M2's committed ladder -- RULE.md section 3 lists the resolved
# numbers; these two per cell are what they are computed from.
M2_FACTS = {
    "STEENBRB": (468, 19800.0),
    "STEENBRC": (540, 41800.0),
    "STEENBRG": (540, 41800.0),
    "ZAMB2-8": (138, 24.643618798749973),
    "YORKNET": (312, 48491.383947016526),
    "BRITGAS": (450, 115.03999999999996),
    "READING6": (102, 0.0),
}
MEMBERS = [
    (1, "h_k/(n*rho)", lambda n, h: h / (n * RHO)),
    (2, "h_k/rho", lambda n, h: h / RHO),
    (3, "h_k/n", lambda n, h: h / n),
    (4, "h_k", lambda n, h: h),
]
# The probe prints these only with a callback attached, or they are the probe's
# own recomputation / this task's own new key; they are excluded from the
# OFF-identity comparison by name rather than silently.
NEW_OR_INSTRUMENT = {"proximity"}

LADDER_COLS = ["depth", "event_iteration", "f_event", "radius", "step_norm",
               "f_main", "h_main", "dx_inf", "prox_rel"]


def read_probe(path):
    kv, rows, in_ladder, header = {}, [], False, None
    if not os.path.exists(path):
        return None, None, None
    for line in open(path):
        line = line.rstrip("\n")
        if line == "LADDER_BEGIN":
            in_ladder = True
            continue
        if line == "LADDER_END":
            in_ladder = False
            continue
        if in_ladder:
            if header is None:
                header = line.split(",")
            else:
                rows.append(dict(zip(header, line.split(","))))
            continue
        if "=" in line and not line.startswith(("PROBE_", "#")):
            k, _, v = line.partition("=")
            kv[k] = v
    return kv, rows, header


def zeta_of(cell, member):
    n, h = M2_FACTS[cell]
    return member[2](n, h)


def f(x):
    return float(x)


def depth1(rows):
    return [r for r in rows if r["depth"] == "1"]


# --------------------------------------------------------------------------
# (1) THE OFF-IDENTITY CHECK. The screen arm runs at the shipped zeta = 0 on a
# library that now carries the term: its rows must reproduce M2's committed
# ladder column for column. A difference is a STOP (RULE.md section 5).
# --------------------------------------------------------------------------
def off_identity(handle):
    worst = 0
    print("M7 W2 M2b -- OFF IDENTITY: the screen arm (zeta = 0) vs M2's committed ladder",
          file=handle)
    print("A DIFFERENCE IS A STOP. `proximity` is this task's new key and `prox_rel` is its",
          file=handle)
    print("new column; both are excluded by name, and nothing else is.\n", file=handle)
    for cell in CELLS:
        kv_new, rows_new, _ = read_probe(os.path.join(OUT, "screen", cell + ".txt"))
        kv_m2, rows_m2, hdr_m2 = read_probe(os.path.join(M2, cell + ".txt"))
        if kv_new is None or kv_m2 is None:
            print(f"{cell:10s} MISSING LEG -- new={kv_new is not None} m2={kv_m2 is not None}",
                  file=handle)
            worst = 1
            continue
        keys = (set(kv_new) | set(kv_m2)) - NEW_OR_INSTRUMENT
        diff_keys = [k for k in sorted(keys) if kv_new.get(k) != kv_m2.get(k)]
        shared_cols = [c for c in hdr_m2 if c in LADDER_COLS]
        diff_rows = 0
        if len(rows_new) != len(rows_m2):
            diff_rows = -1
        else:
            for a, b in zip(rows_new, rows_m2):
                for c in shared_cols:
                    if a.get(c) != b.get(c):
                        diff_rows += 1
        ok = (not diff_keys) and diff_rows == 0
        worst |= 0 if ok else 1
        print(f"{cell:10s} keys {len(keys):3d} compared, {len(diff_keys):3d} differing; "
              f"rows {len(rows_m2):3d} x {len(shared_cols)} columns, "
              f"{'LENGTH MISMATCH' if diff_rows < 0 else str(diff_rows) + ' differing'}"
              f"   {'IDENTICAL' if ok else '*** STOP ***'}", file=handle)
        for k in diff_keys:
            print(f"    {k}: m2={kv_m2.get(k)!r}  m2b={kv_new.get(k)!r}", file=handle)
    print(f"\nOFF_IDENTITY_WORST={worst}", file=handle)
    return worst


# --------------------------------------------------------------------------
# (2) THE RULE, applied to the screen arm. RULE.md section 2, verbatim in code:
#   (a) INCREMENTAL on the measured FINAL step of STEENBRB/STEENBRC/STEENBRG --
#       inadmissible when (zeta/2)*[prox_rel(x_t) - prox_rel(x_{t-1})] exceeds
#       h(x_{t-1}) - h(x_t); the margin is that ratio and must be >= 10.
#   (b),(c) CUMULATIVE at the returned point of ZAMB2-8 and BRITGAS --
#       admissible when (zeta/2)*prox_rel(x_r) is below h_k - h_r; the margin is
#       the RECIPROCAL of that ratio and must be >= 10.
# --------------------------------------------------------------------------
RUNAWAY = ["STEENBRB", "STEENBRC", "STEENBRG"]
PROTECTED = ["ZAMB2-8", "BRITGAS"]


def clause_a(cell, zeta):
    """(ratio, detail) for the incremental final step. ratio > 1 = refused."""
    kv, rows, _ = read_probe(os.path.join(OUT, "screen", cell + ".txt"))
    d1 = depth1(rows)
    if len(d1) < 2:
        return None, f"{cell}: fewer than two depth-1 rows"
    last, prev = d1[-1], d1[-2]
    dprox = f(last["prox_rel"]) - f(prev["prox_rel"])
    gain = f(prev["h_main"]) - f(last["h_main"])
    penalty = 0.5 * zeta * dprox
    if gain <= 0.0:
        return float("inf"), (f"gain={gain!r} <= 0: the step buys no violation, so it is "
                              f"inadmissible at every zeta (penalty={penalty!r})")
    return penalty / gain, f"penalty={penalty!r} gain={gain!r} dprox={dprox!r}"


def clause_bc(cell, zeta):
    """(ratio, detail) cumulative at the returned point. ratio > 1 = refused."""
    kv, rows, _ = read_probe(os.path.join(OUT, "screen", cell + ".txt"))
    d1 = depth1(rows)
    if not d1:
        return None, f"{cell}: no depth-1 row"
    prox_r = f(d1[-1]["prox_rel"])
    h_in, h_out = f(kv["restoration_h_in"]), f(kv["restoration_h_out"])
    gain = h_in - h_out
    penalty = 0.5 * zeta * prox_r
    if gain <= 0.0:
        return float("inf"), f"gain={gain!r} <= 0: the phase bought no violation"
    return penalty / gain, f"penalty={penalty!r} gain={gain!r} prox_rel(x_r)={prox_r!r}"


def verdict(handle):
    print("M7 W2 M2b -- THE VERDICT: RULE.md section 2, applied to the screen arm (zeta = 0)",
          file=handle)
    print("Clause (a) is INCREMENTAL on the final step and must REFUSE by >= 10x.", file=handle)
    print("Clauses (b),(c) are CUMULATIVE at the returned point and must ADMIT by >= 10x.\n",
          file=handle)
    qualifying = []
    for num, name, _ in MEMBERS:
        member = [m for m in MEMBERS if m[0] == num][0]
        print(f"--- member {num}: zeta = {name}", file=handle)
        ok = True
        for cell in RUNAWAY:
            z = zeta_of(cell, member)
            ratio, detail = clause_a(cell, z)
            margin = ratio if ratio is not None else 0.0
            passed = margin >= 10.0
            ok &= passed
            print(f"  (a) {cell:10s} zeta={z!r:24} ratio={ratio!r:24} margin={margin!r:24} "
                  f"{'PASS' if passed else 'FAIL'}   [{detail}]", file=handle)
        for cell in PROTECTED:
            z = zeta_of(cell, member)
            ratio, detail = clause_bc(cell, z)
            margin = (1.0 / ratio) if (ratio and ratio > 0.0) else float("inf")
            passed = margin >= 10.0
            ok &= passed
            clause = "(b)" if cell == "ZAMB2-8" else "(c)"
            print(f"  {clause} {cell:10s} zeta={z!r:24} ratio={ratio!r:24} margin={margin!r:24} "
                  f"{'PASS' if passed else 'FAIL'}   [{detail}]", file=handle)
        print(f"  MEMBER {num} {'QUALIFIES' if ok else 'does not qualify'}\n", file=handle)
        if ok:
            qualifying.append((num, name))
    if qualifying:
        num, name = qualifying[0]  # the SMALLEST qualifying member: the set is ordered
        print(f"VERDICT: member {num}, zeta = {name} -- the smallest member clearing every "
              f"clause by the pre-registered factor of 10.", file=handle)
        print("RECORDED AS A RECOMMENDATION FOR M2c. NO DEFAULT MOVES IN THIS TASK.",
              file=handle)
        return 0
    print("VERDICT: NO MEMBER QUALIFIES. RULE.md section 2: the shape is reported as not "
          "calibratable without a new constant and RETURNS TO THE SETTLER.", file=handle)
    return 0


# --------------------------------------------------------------------------
# (3) THE TABLES: what each candidate's own leg did, per cell.
# --------------------------------------------------------------------------
def tables():
    os.makedirs(os.path.join(OUT, "tables"), exist_ok=True)
    summary = open(os.path.join(OUT, "tables", "summary.md"), "w")
    print("# M7 W2 M2b -- the S5 record and the outcome, per cell per candidate\n", file=summary)
    print("`zeta` is fixed per (cell, candidate) from M2's committed `h_k` (RULE.md section 3).",
          file=summary)
    print("`h_k drift` is this leg's own `restoration_h_in` against M2's -- RULE.md section 7",
          file=summary)
    print("item 2: a drifted row carries a zeta that is no longer exactly its formula's value",
          file=summary)
    print("at its own entry.\n", file=summary)
    print("| cell | arm | zeta | status | certified | entries | h in -> out | f in -> out | "
          "resto iters | factorizations | h_k drift |", file=summary)
    print("|---|---|---:|---|---|---:|---|---|---:|---:|---:|", file=summary)
    for cell in CELLS:
        n, h_k = M2_FACTS[cell]
        per_cell = open(os.path.join(OUT, "tables", cell + ".md"), "w")
        print(f"# {cell} -- the proximity ladder\n", file=per_cell)
        arms = [("screen", 0.0, "screen")] + [
            (f"c{num}", zeta_of(cell, m), name) for (num, name, _) in MEMBERS
            for m in [[x for x in MEMBERS if x[0] == num][0]]]
        for (d, z, label) in arms:
            kv, rows, _ = read_probe(os.path.join(OUT, d, cell + ".txt"))
            if kv is None:
                print(f"| {cell} | {d} | {z!r} | MISSING | | | | | | | |", file=summary)
                continue
            drift = ""
            if "restoration_h_in" in kv and float(kv.get("restoration_entries", "0")) > 0:
                drift = repr(f(kv["restoration_h_in"]) - h_k)
            print(f"| {cell} | {d} ({label}) | {z!r} | {kv.get('status')} | "
                  f"{kv.get('infeasibility_certified')} | {kv.get('restoration_entries')} | "
                  f"{kv.get('restoration_h_in')} -> {kv.get('restoration_h_out')} | "
                  f"{kv.get('restoration_f_in')} -> {kv.get('restoration_f_out')} | "
                  f"{kv.get('restoration_iters')} | {kv.get('factorizations')} | {drift} |",
                  file=summary)
            print(f"\n## arm `{d}` -- zeta = {z!r} ({label})\n", file=per_cell)
            print(f"status={kv.get('status')} f={kv.get('f')} "
                  f"certified={kv.get('infeasibility_certified')} "
                  f"majors={kv.get('major_iters')} resto={kv.get('restoration_iters')} "
                  f"factorizations={kv.get('factorizations')}\n", file=per_cell)
            d1 = depth1(rows)
            if not d1:
                print("(no restoration phase ran)\n", file=per_cell)
                continue
            print("| # | f_event (h+P at zeta>0) | radius | step_norm | f_main | h_main | "
                  "dx_inf | prox_rel | d(prox_rel) |", file=per_cell)
            print("|---:|---:|---:|---:|---:|---:|---:|---:|---:|", file=per_cell)
            prev = None
            for i, r in enumerate(d1):
                dp = "" if prev is None else repr(f(r["prox_rel"]) - f(prev["prox_rel"]))
                print(f"| {i} | {r['f_event']} | {r['radius']} | {r['step_norm']} | "
                      f"{r['f_main']} | {r['h_main']} | {r['dx_inf']} | {r['prox_rel']} | {dp} |",
                      file=per_cell)
                prev = r
        per_cell.close()
    summary.close()


def main():
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "off-identity.txt"), "w") as handle:
        stop = off_identity(handle)
    print(open(os.path.join(OUT, "off-identity.txt")).read())
    tables()
    with open(os.path.join(OUT, "verdict.txt"), "w") as handle:
        verdict(handle)
    print(open(os.path.join(OUT, "verdict.txt")).read())
    if stop:
        print("*** OFF IDENTITY FAILED -- the verdict above is printed but is NOT to be used ***")
    return stop


if __name__ == "__main__":
    sys.exit(main())
