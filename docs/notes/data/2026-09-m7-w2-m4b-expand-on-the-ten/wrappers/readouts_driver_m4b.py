#!/usr/bin/env python3
"""M7 W2 M4b -- the PRE-REGISTERED read-outs over the TWENTY traces of the ten.

**PRE-REGISTERED.**  `RULE.md` §6 of THIS task inherits M4 `RULE.md` §§6.2-6.6
by reference and pre-registers them, before any leg, for §2's TEN cells.  That
is the difference between this driver and the one it is derived from.

DERIVATION, DECLARED.  This file is M4's `wrappers/readouts_driver_m4.py` with
exactly these changes, and no others of substance:

  1. `CELLS` is §2's ten, in M4's own cell order, instead of M4's three
     neutrality cells; `ARMS` is `("off", "E")` instead of the four arms.
  2. The two arms' flat traces live under DIFFERENT roots -- OFF under M4's
     `.scratch/m7w2m4/arm/traces/`, E under this task's
     `.scratch/m7w2m4b/traces/` -- so the trace path is taken per arm from two
     directory arguments rather than one.
  3. Cells are loaded and released ONE AT A TIME.  M4 held twelve traces of
     247 MB in memory at once; these twenty are 457 MB.
  4. The POST-HOC labelling is dropped, because on these cells §6 is
     pre-registered.  **`readouts_m4.py`, `classify_cause_fix1.py` and
     `classify_walk_m3b.py` -- the tail finder and every predicate -- are
     IMPORTED UNMODIFIED.**  Nothing that computes a number is derived; only
     the loop around them is.

`readouts_driver_m4.py` is committed beside this file, unmodified, and its own
docstring still says POST-HOC.  It says so about M4's three neutrality cells,
which is where it was true and where it remains true; it is not a statement
about this task, and the file is not edited to make it one.

No solver runs here.
"""

import os
import sys
from collections import OrderedDict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import readouts_m4 as R                  # noqa: E402
import classify_cause_fix1 as cc         # noqa: E402

ARMS = ("off", "E")
CELLS = ("ACOPP57", "DALLASM", "STEENBRB", "ACOPP118", "ACOPR57",
         "STEENBRC", "STEENBRG", "YORKNET", "SAWPATH", "BRITGAS")


def read_flat(path):
    head, cols, rows = [], None, []
    with open(path, errors="replace") as fh:
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
            rows.append(line)
    hdr = {}
    for h in head:
        s = h[1:].strip()
        if "=" in s:
            k, v = s.split("=", 1)
            hdr[k.strip()] = v.strip()
    return head, cols, rows, hdr


def by_solve(rows):
    out = OrderedDict()
    for line in rows:
        k = int(line.split(",", 1)[0])
        out.setdefault(k, []).append(line)
    return out


def typed_rows(cols, lines):
    names = cols.split(",")
    return [dict(zip(names, l.split(","))) for l in lines]


def f0_of(raws):
    return [tuple(cc.bitkey(c, r[c]) for c in cc.F0) for r in raws]


def fnum(t):
    try:
        v = float(t)
    except (TypeError, ValueError):
        return None
    return None if v != v else v


def predicate(raws):
    """M4 RULE.md §6.2/§6.5, applied through readouts_m4's own machinery."""
    n = len(raws)
    keys = f0_of(raws)
    p, t0, nfail = R.maximal_periodic_tail(keys)
    d = {"N": n, "p": p, "t0": t0, "repeat_failures": nfail}
    if p is None:
        d.update(T1=False, tail=0, T2=False, T3=False, recurrent=False)
    else:
        tl = n - t0
        d["tail"] = tl
        d["T1"] = True
        d["T2"] = tl >= 0.10 * n
        d["T3"] = tl >= 8 * p
        d["recurrent"] = d["T1"] and d["T2"] and d["T3"]
    d["tail_share"] = (d.get("tail", 0) / n) if n else 0.0
    dx = [fnum(r.get("dx_inf")) for r in raws]
    d["sum_dx_inf"] = sum(v for v in dx if v is not None)
    d["dx_present"] = sum(1 for v in dx if v is not None)
    d["gone"] = (not d["recurrent"]) and d["sum_dx_inf"] > 0
    return d


def slice_write(head, cols, lines, out_dir, cell, arm, k, kind, why, src):
    path = os.path.join(out_dir, "%s-%s-solve%d.csv" % (arm, cell, k))
    with open(path, "w") as fh:
        for h in head:
            fh.write(h + "\n")
        fh.write("# SLICE of a flat trace (M7 W2 M4b, PRE-REGISTERED read-out)\n")
        fh.write("# slice_cell=%s\n" % cell)
        fh.write("# slice_arm=%s\n" % arm)
        fh.write("# slice_solve=%d\n" % k)
        fh.write("# slice_kind=%s\n" % kind)
        fh.write("# slice_reason=%s\n" % why)
        fh.write("# slice_source=%s\n" % src)
        fh.write("# slice_rows=%d\n" % len(lines))
        fh.write(cols + "\n")
        for r in lines:
            fh.write(r + "\n")
    return path


def main():
    off_dir, e_dir, slice_dir, out_md = sys.argv[1:5]
    dirs = {"off": off_dir, "E": e_dir}
    os.makedirs(slice_dir, exist_ok=True)
    md = open(out_md, "w")
    w = md.write
    w("# M7 W2 M4b — the PRE-REGISTERED read-outs over the ten cells' twenty traces\n\n")
    w("Generated by `wrappers/readouts_driver_m4b.py`; **no solver ran here.**\n")
    w("Every figure is machine-written from the flat traces named in\n")
    w("`traces-m4b/SHA256-parent-traces-m4b.txt` and from the slices in\n")
    w("`traces-m4b/`. The tail finder and every predicate are\n")
    w("`readouts_m4.py`'s, imported unmodified.\n\n")
    w("**PRE-REGISTERED.** This task's `RULE.md` §6 inherits M4 `RULE.md`\n")
    w("§§6.2–6.6 by reference and pre-registers them, before any leg, for §2's\n")
    w("ten cells. `readouts_driver_m4.py` is committed beside this driver with\n")
    w("its POST-HOC docstring unedited — that label is true of M4's three\n")
    w("neutrality cells, which is what it speaks about, and the file is not\n")
    w("edited to say something else.\n\n")
    w("**M4 §6.4's DECLARED DEFECT travels with the predicate** and is repeated\n")
    w("in this task's `README.md`: the coverage-only form admits a tail at\n")
    w("`|T| > p`, which at the search cap `p = 64` is 65 passes and would score\n")
    w("as recurrence on a 200-pass solve that merely coincided with itself twice\n")
    w("at a stride of 64. T3 (`|T| ≥ 8p`) closes it.\n\n")
    w("**The OFF traces are M4's**, `.scratch/m7w2m4/arm/traces/off-<CELL>.csv`,\n")
    w("the traces of the very rows §4 copies — not M3c's pass-2 slices. The\n")
    w("reason is identity: the OFF row this task compares against IS M4's row,\n")
    w("so the OFF walk it compares against must be that row's own walk. M3c's\n")
    w("committed slices are slices of ONE solve per cell; these are the whole\n")
    w("run. Their sha256 are pinned in M4's own\n")
    w("`traces-m4/SHA256-parent-traces-m4.txt` and re-verified for this task in\n")
    w("`logs/pin-off-traces.txt`.\n\n")

    totals, per_cell = {}, {}
    w("## M4 §6.2's predicate, per solve, per cell × arm\n\n")
    w("`N` passes; `p` the winning period (`-` = no exactly-periodic suffix at\n")
    w("any p ≤ 64); `|T|` the tail. `REC` = RECURRENT_TAIL (T1∧T2∧T3),\n")
    w("`GONE` = ¬REC ∧ Σ‖Δx‖∞>0.\n\n")
    w("**No minor-cap column is printed**, for M4's reason: the flat trace\n")
    w("carries ONE `max_iter` in its header and that value is the CAPTURED\n")
    w("solve's, not every QP's in the run. Solve LENGTH is printed instead and\n")
    w("nothing is inferred from it.\n\n")

    for cell in CELLS:
        loaded = {}
        for arm in ARMS:
            src = os.path.join(dirs[arm], "%s-%s.csv" % (arm, cell))
            head, cols, rows, hdr = read_flat(src)
            loaded[arm] = (src, head, cols, by_solve(rows), hdr)
            del rows
        w("### %s  (captured solve's minor cap = %s off / %s E)\n\n"
          % (cell, loaded["off"][4].get("max_iter", "?"),
             loaded["E"][4].get("max_iter", "?")))
        w("| arm | solve | N | p | \\|T\\| | T/N | T1 | T2 | T3 | REC | Σ‖Δx‖∞ | GONE |\n")
        w("|---|---|---|---|---|---|---|---|---|---|---|---|\n")
        capped_union, preds = set(), {}
        for arm in ARMS:
            src, head, cols, buckets, hdr = loaded[arm]
            nrec = ngone = rec_minors = tot_minors = 0
            for k in sorted(buckets):
                d = predicate(typed_rows(cols, buckets[k]))
                preds[(arm, k)] = d
                nrec += bool(d["recurrent"])
                ngone += bool(d["gone"])
                tot_minors += d["N"]
                if d["recurrent"]:
                    rec_minors += d["N"]
                    capped_union.add(k)
                w("| %s | %d | %d | %s | %d | %.4f | %s | %s | %s | **%s** | %.6g | %s |\n" % (
                    arm, k, d["N"],
                    d["p"] if d["p"] is not None else "-", d.get("tail", 0),
                    d["tail_share"], int(d["T1"]), int(d["T2"]), int(d["T3"]),
                    "REC" if d["recurrent"] else "-", d["sum_dx_inf"],
                    "GONE" if d["gone"] else "-"))
            totals[(arm, cell)] = (len(buckets), nrec, ngone, rec_minors,
                                   tot_minors)
        w("\n")
        per_cell[cell] = (loaded, capped_union, preds)

        # -- first diverging solve, and the per-minor difference -----------
        ob = loaded["off"][3]
        ab = loaded["E"][3]
        names = loaded["off"][2].split(",")
        div = None
        for k in sorted(set(ob) | set(ab)):
            if ob.get(k) != ab.get(k):
                div = k
                break
        nsolve = nrec2 = 0
        first_k, first = None, "— (every shared record block is byte-identical)"
        for k in sorted(set(ob) & set(ab)):
            diffs = [i for i, (x, y) in enumerate(zip(ob[k], ab[k])) if x != y]
            if diffs or len(ob[k]) != len(ab[k]):
                nsolve += 1
                nrec2 += len(diffs) + abs(len(ob[k]) - len(ab[k]))
            if diffs and first_k is None:
                first_k = k
                i = diffs[0]
                xs, ys = ob[k][i].split(","), ab[k][i].split(",")
                ff = [names[j] for j in range(min(len(xs), len(ys)))
                      if xs[j] != ys[j]]
                first = ("solve %d, minor %s, first fields `%s`"
                         % (k, xs[1], "`, `".join(ff[:5])))
        w("**off vs E, record by record.** solves %d / %d; first diverging solve "
          "index: %s; solves with ≥1 differing record: %d; differing records: "
          "%d; first differing record: %s\n\n"
          % (len(ob), len(ab),
             "none — every record block is byte-identical" if div is None
             else str(div), nsolve, nrec2, first))
        if first_k is not None:
            capped_union.add(first_k)
        per_cell[cell] = (loaded, capped_union, preds)

    # ---- totals ---------------------------------------------------------
    w("## Totals — solves, RECURRENT_TAIL, RECURRENCE_GONE, minors\n\n")
    w("`recurrent minors` is the sum of N over that arm's RECURRENT solves;\n")
    w("`all minors` the sum over every solve, which equals the row's own\n")
    w("`qp_minor_iters`.\n\n")
    w("| cell | arm | solves | RECURRENT | GONE | recurrent minors | all minors |"
      " recurrent share |\n|---|---|---|---|---|---|---|---|\n")
    for cell in CELLS:
        for arm in ARMS:
            t = totals[(arm, cell)]
            w("| %s | %s | %d | %d | %d | %d | %d | %.3f |\n"
              % (cell, arm, t[0], t[1], t[2], t[3], t[4],
                 (t[3] / t[4]) if t[4] else 0.0))
    w("\n### Where the minors moved, per cell\n\n")
    for cell in CELLS:
        o, e = totals[("off", cell)], totals[("E", cell)]
        w("* **%s.** OFF has %d RECURRENT solves carrying %d of its %d minors"
          " (%.1f%%); E has %d carrying %d of its %d (%.1f%%). Minors move"
          " %d → %d (%+d); minors inside RECURRENT solves move %d → %d"
          " (%+d); minors outside them move %d → %d (%+d).\n"
          % (cell, o[1], o[3], o[4], 100.0 * o[3] / o[4] if o[4] else 0.0,
             e[1], e[3], e[4], 100.0 * e[3] / e[4] if e[4] else 0.0,
             o[4], e[4], e[4] - o[4], o[3], e[3], e[3] - o[3],
             o[4] - o[3], e[4] - e[3], (e[4] - e[3]) - (o[4] - o[3])))
    w("\n**Read with care:** a solve INDEX is comparable across the two arms"
      " only up to the first solve at which the two walks diverge, printed per"
      " cell above. Past that point the same index is not the same subproblem,"
      " so only the AGGREGATE columns are read, never a per-index pairing.\n\n")

    # ---- §6.6 on the RECURRENT union ------------------------------------
    w("## M4 §6.6's per-solve read-outs on the RECURRENT solves\n\n")
    w("The union of RECURRENT solve indices per cell is taken over BOTH arms,\n")
    w("plus the first-divergence index, and each such index is read under EVERY\n")
    w("arm that has it, so a recurrence one arm carries and the other does not\n")
    w("is visible from both sides. An index that does not exist under an arm is\n")
    w("reported ABSENT and never silently dropped.\n\n")
    made = []
    for cell in CELLS:
        loaded, capped_union, preds = per_cell[cell]
        ks = sorted(capped_union)
        w("### %s — RECURRENT indices (union over arms) plus the first-divergence index: %s\n\n"
          % (cell, ", ".join(str(k) for k in ks) if ks else "(none)"))
        for k in ks:
            for arm in ARMS:
                src, head, cols, buckets, hdr = loaded[arm]
                if k not in buckets:
                    w("* arm `%s` solve %d: **ABSENT** — this arm's run has no "
                      "solve at that index (it ran %d solves).\n\n"
                      % (arm, k, len(buckets)))
                    continue
                p = slice_write(head, cols, buckets[k], slice_dir, cell, arm, k,
                                "RECURRENT_UNION",
                                "RECURRENT under at least one arm, or the first "
                                "solve at which the two walks diverge", src)
                made.append(p)
                d = R.read_out(cell, p, "RECURRENT_UNION", k, "arm=%s" % arm)
                w("#### arm `%s` — solve %d\n\n" % (arm, k))
                R.render(d, md)
        per_cell[cell] = None
    md.close()
    with open(os.path.join(slice_dir, "SLICES.txt"), "w") as fh:
        for p in sorted(made):
            fh.write(os.path.basename(p) + "\n")
    print("slices: %d" % len(made))
    print("read-outs: %s" % out_md)
    return 0


if __name__ == "__main__":
    sys.exit(main())
