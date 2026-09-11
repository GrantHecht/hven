#!/usr/bin/env python3
"""M6 W5 T8.9r-attrib — the per-task attribution of leg 1's instruction increase.

Reads `table.csv` from this file's own directory (arm, task, sha, mode, cell,
round, instructions, branches, cycles, branch_misses, l1_icache_load_misses;
five rounds per arm x mode x cell, eleven arms) and prints, per mode per cell:

  * each arm's MEDIAN instructions and the round spread;
  * the STEP from the previous arm, absolute and as a fraction of the BASE arm
    (102f729), and the same for branches;
  * the running CUMULATIVE fraction;
  * a verdict per step at the brief's +/-2e-5 band, and for a step outside it
    the branch/instruction fraction RATIO, which separates executed work from a
    code-layout cluster change: work moves branches and instructions by nearly
    the same fraction (this program's average branch density), a layout cluster
    transition moves branches by ~2.4x the instruction fraction. The reference
    for the layout value is this leg's own control, raw/logs/A12-layout-probe.log,
    where ONE binary on ONE cell crosses clusters under nothing but a change in
    the byte length of its --internal-out path: -1.01e-4 instructions against
    -2.48e-4 branches, ratio 2.46.

then the closure check (the steps must sum to the end-to-end delta inside the
floor) and the calibration against the T8.9r artifact's own published counts
(`reading.md` section 3, quoted verbatim below).

No arguments. Deterministic: integer counts in, medians of five out.
"""
import csv
import os
import statistics
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ARMS = [("a01", "base", "102f729"), ("a02", "T8.1", "b3915ff"),
        ("a03", "T8.2", "b43580f"), ("a04", "T8.3", "510a4bb"),
        ("a05", "T8.4", "3c8e43b"), ("a06", "T8.5", "8f95655"),
        ("a07", "T8.6", "8cbaa39"), ("a08", "T8.7", "56042be"),
        ("a09", "T8.7b", "ddac2cf"), ("a10", "T8.8", "b9848bf"),
        ("a11", "T8.9", "e51a7e0")]
MODES = ["ipm", "ssn", "walk"]
CELLS = ["f7_n1000_bound_neutral", "f7_n5000_bound_neutral",
         "f7_n20000_bound_neutral"]
BAND = 2e-5          # the brief's step band, and the artifact's own leg-1 floor
WORK_RATIO = (0.8, 1.6)     # branch fraction / instruction fraction, executed work
LAYOUT_RATIO = (2.0, 3.0)   # ... the same ratio for a layout cluster transition
PROBE_RATIO = 2.46          # measured, raw/logs/A12-layout-probe.log

# reading.md section 3, "The absolute deltas": (base, head) instructions:u, the
# artifact's own medians of three on its pass A. Quoted, not recomputed.
ARTIFACT = {
    ("ipm", "f7_n1000_bound_neutral"):   (749_104_475, 749_451_809),
    ("ipm", "f7_n5000_bound_neutral"):   (3_834_858_737, 3_836_095_428),
    ("ipm", "f7_n20000_bound_neutral"):  (15_958_310_647, 15_965_108_235),
    ("ssn", "f7_n1000_bound_neutral"):   (488_466_427, 488_889_622),
    ("ssn", "f7_n5000_bound_neutral"):   (2_629_019_851, 2_630_729_726),
    ("ssn", "f7_n20000_bound_neutral"):  (11_674_687_728, 11_683_005_904),
    ("walk", "f7_n1000_bound_neutral"):  (261_282_683, 261_625_450),
    ("walk", "f7_n5000_bound_neutral"):  (1_333_963_362, 1_335_651_610),
    ("walk", "f7_n20000_bound_neutral"): (5_640_377_762, 5_645_586_494),
}


def load():
    rows = {}
    with open(os.path.join(HERE, "table.csv"), newline="") as fh:
        for rec in csv.DictReader(fh):
            key = (rec["arm"], rec["mode"], rec["cell"])
            rows.setdefault(key, []).append(
                (int(rec["instructions"]), int(rec["branches"]),
                 int(rec["cycles"])))
    return rows


def med(vals, idx):
    return statistics.median(v[idx] for v in vals)


def main():
    rows = load()
    missing = [k for arm, _, _ in ARMS for m in MODES for c in CELLS
               if (k := (arm, m, c)) not in rows]
    if missing:
        print("MISSING ROWS:", missing)
        return 2

    steps = {}          # (mode, cell, task) -> fraction of base
    print("=" * 100)
    print("PER-TASK INSTRUCTION STEPS -- median of five rounds per arm, one "
          "byte-identical process layout")
    print("=" * 100)
    for mode in MODES:
        for cell in CELLS:
            data = [(t, rows[(a, mode, cell)]) for a, t, _ in ARMS]
            base_i = med(data[0][1], 0)
            base_b = med(data[0][1], 1)
            print(f"\n-- mode={mode}  cell={cell}")
            print(f"   {'task':6s} {'median instr':>16s} {'spread':>9s} "
                  f"{'step':>13s} {'step/base':>12s} {'cum/base':>12s} "
                  f"{'branch/base':>13s} {'b/i':>6s}  verdict")
            prev_i = prev_b = None
            for task, vals in data:
                mi, mb = med(vals, 0), med(vals, 1)
                ins = [v[0] for v in vals]
                spread = max(ins) - min(ins)
                if prev_i is None:
                    print(f"   {task:6s} {mi:16,.0f} {spread:9,.0f} "
                          f"{'--':>13s} {'--':>12s} {0.0:12.6f} "
                          f"{'--':>13s} {'--':>6s}  base")
                else:
                    d = mi - prev_i
                    f = d / base_i
                    fb = (mb - prev_b) / base_b
                    cum = (mi - base_i) / base_i
                    ratio = fb / f if f else float("nan")
                    if abs(f) <= BAND:
                        v, rs = "flat", "--"
                    else:
                        rs = f"{ratio:6.2f}"
                        if WORK_RATIO[0] <= ratio <= WORK_RATIO[1]:
                            v = "STEP  WORK"
                        elif LAYOUT_RATIO[0] <= ratio <= LAYOUT_RATIO[1]:
                            v = "STEP  LAYOUT"
                        else:
                            v = "STEP  UNCLASSIFIED"
                    print(f"   {task:6s} {mi:16,.0f} {spread:9,.0f} "
                          f"{d:+13,.0f} {f:+12.6f} {cum:+12.6f} "
                          f"{fb:+13.6f} {rs:>6s}  {v}")
                    steps[(mode, cell, task)] = f
                prev_i, prev_b = mi, mb

    print("\n" + "=" * 100)
    print("CLOSURE -- the ten steps must sum to the end-to-end delta")
    print("=" * 100)
    print(f"   {'mode':5s} {'cell':24s} {'sum of steps':>14s} "
          f"{'end-to-end':>14s} {'residual':>12s}")
    for mode in MODES:
        for cell in CELLS:
            base_i = med(rows[("a01", mode, cell)], 0)
            head_i = med(rows[("a11", mode, cell)], 0)
            s = sum(steps[(mode, cell, t)] for _, t, _ in ARMS[1:])
            e2e = (head_i - base_i) / base_i
            print(f"   {mode:5s} {cell:24s} {s:+14.6f} {e2e:+14.6f} "
                  f"{s - e2e:+12.9f}")

    print("\n" + "=" * 100)
    print("CALIBRATION against the T8.9r artifact (reading.md section 3)")
    print("=" * 100)
    print(f"   {'mode':5s} {'cell':24s} {'arm':5s} {'this leg':>16s} "
          f"{'artifact':>16s} {'rel diff':>11s}")
    worst = 0.0
    for mode in MODES:
        for cell in CELLS:
            for arm, label in (("a01", "base"), ("a11", "head")):
                mine = med(rows[(arm, mode, cell)], 0)
                theirs = ARTIFACT[(mode, cell)][0 if arm == "a01" else 1]
                rel = (mine - theirs) / theirs
                worst = max(worst, abs(rel))
                print(f"   {mode:5s} {cell:24s} {label:5s} {mine:16,.0f} "
                      f"{theirs:16,.0f} {rel:+11.2e}")
    print(f"\n   worst absolute-count deviation: {worst:.2e}")
    print("   RATIO form (layout-free, the quantity being attributed):")
    print(f"   {'mode':5s} {'cell':24s} {'this leg':>12s} {'artifact':>12s} "
          f"{'diff':>11s}")
    wr = 0.0
    for mode in MODES:
        for cell in CELLS:
            mine = (med(rows[("a11", mode, cell)], 0)
                    / med(rows[("a01", mode, cell)], 0))
            b, h = ARTIFACT[(mode, cell)]
            theirs = h / b
            wr = max(wr, abs(mine - theirs))
            print(f"   {mode:5s} {cell:24s} {mine:12.6f} {theirs:12.6f} "
                  f"{mine - theirs:+11.2e}")
    print(f"\n   worst end-to-end RATIO deviation: {wr:.2e}")

    print("\n" + "=" * 100)
    print("PER-ARM ROUND SPREAD -- the floor this leg can resolve, per cell")
    print("=" * 100)
    print(f"   {'mode':5s} {'cell':24s} {'worst spread':>14s} "
          f"{'as a fraction':>14s}   (band is 2.0e-05)")
    for mode in MODES:
        for cell in CELLS:
            base_i = med(rows[("a01", mode, cell)], 0)
            w = max(max(v[0] for v in rows[(a, mode, cell)])
                    - min(v[0] for v in rows[(a, mode, cell)])
                    for a, _, _ in ARMS)
            flag = "  <-- band below the spread" if w / base_i > BAND else ""
            print(f"   {mode:5s} {cell:24s} {w:14,.0f} {w / base_i:14.2e}{flag}")
    print(f"\n   layout-probe branch/instruction fingerprint: {PROBE_RATIO}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
