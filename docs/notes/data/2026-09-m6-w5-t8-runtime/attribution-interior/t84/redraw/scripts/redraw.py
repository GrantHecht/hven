#!/usr/bin/env python3
"""W5 T8.9r-attrib4 -- the redraw experiment's per-row wall table and recovery.

Reads the five rounds' per-arm `--engine interior` CSVs, emits the tidy
`wall.csv`, and prints the four-arm per-row table with the recovery each
experiment arm achieves against the base.

    RECOVERY(E) = 1 - (E/base - 1) / (head/base - 1)

per row and on the corpus figure, exactly as predeclaration.txt fixes it.
The FIRST ROW every process writes is excluded BY POSITION (rule R3); the cell
order is pinned so that row is the same row at every arm.
"""
import os
import statistics
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.join(ROOT, "art", "csv")
ARMS = [("a1", "base 102f729"), ("a2", "head e51a7e0"), ("a3", "E1"), ("a4", "E2")]
ROUNDS = [1, 2, 3, 4, 5]


def load(path):
    rows, hdr = [], None
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        f = line.rstrip("\n").split(",")
        if hdr is None:
            hdr = f
            continue
        rows.append(dict(zip(hdr, f)))
    return rows


def main():
    # arm -> round -> [row dicts in file order]
    data = {}
    for arm, _ in ARMS:
        data[arm] = {}
        for r in ROUNDS:
            p = os.path.join(CSV, "w-%s-r%d.csv" % (arm, r))
            data[arm][r] = load(p)

    # R3: drop the first row of every file, by POSITION.
    excluded = {}
    for arm, _ in ARMS:
        for r in ROUNDS:
            rows = data[arm][r]
            excluded.setdefault(arm, rows[0]["cell_id"])
            data[arm][r] = rows[1:]
    print("R3 -- the excluded first row, per arm: %s" % excluded)
    assert len(set(excluded.values())) == 1, "the excluded row is not the same row at every arm"

    # The scored population: rows present at EVERY arm in EVERY round, F7 only.
    keysets = [set(x["cell_id"] for x in data[a][r]) for a, _ in ARMS for r in ROUNDS]
    common = set.intersection(*keysets)
    scored = [k for k in (x["cell_id"] for x in data["a2"][1]) if k in common and k.startswith("f7_")]
    print("SCORED ROWS %d: %s" % (len(scored), scored))

    wall = {}  # (arm, key) -> [5 walls]
    for arm, _ in ARMS:
        for r in ROUNDS:
            for x in data[arm][r]:
                if x["cell_id"] in common:
                    wall.setdefault((arm, x["cell_id"]), []).append(float(x["wall_s"]))

    out = open(os.path.join(ROOT, "wall.csv"), "w")
    out.write("arm,arm_label,row,round,wall_s\n")
    for arm, label in ARMS:
        for r in ROUNDS:
            for x in data[arm][r]:
                if x["cell_id"] in common:
                    out.write("%s,%s,%s,%d,%s\n" % (arm, label, x["cell_id"], r, x["wall_s"]))
    out.close()

    med = {k: statistics.median(v) for k, v in wall.items()}
    print("\n## The four-arm per-row wall table (median of five rounds, seconds)\n")
    print("| row | base 102f729 | head e51a7e0 | E1 | E2 | head/base | E1/base | E2/base | "
          "E1 recovery | E2 recovery |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    recs = {"a3": [], "a4": []}
    for k in scored:
        b, h = med[("a1", k)], med[("a2", k)]
        e1, e2 = med[("a3", k)], med[("a4", k)]
        step = h / b - 1.0
        r1 = 1.0 - (e1 / b - 1.0) / step
        r2 = 1.0 - (e2 / b - 1.0) / step
        recs["a3"].append(r1)
        recs["a4"].append(r2)
        print("| `%s` | %.6f | %.6f | %.6f | %.6f | %.4f | %.4f | %.4f | %+.1f %% | %+.1f %% |"
              % (k, b, h, e1, e2, h / b, e1 / b, e2 / b, 100 * r1, 100 * r2))
    corp = {a: sum(med[(a, k)] for k in scored) for a, _ in ARMS}
    step = corp["a2"] / corp["a1"] - 1.0
    cr = {a: 1.0 - (corp[a] / corp["a1"] - 1.0) / step for a in ("a3", "a4")}
    print("| **scored corpus (11 rows)** | **%.6f** | **%.6f** | **%.6f** | **%.6f** | "
          "**%.4f** | **%.4f** | **%.4f** | **%+.1f %%** | **%+.1f %%** |"
          % (corp["a1"], corp["a2"], corp["a3"], corp["a4"],
             corp["a2"] / corp["a1"], corp["a3"] / corp["a1"], corp["a4"] / corp["a1"],
             100 * cr["a3"], 100 * cr["a4"]))
    print("| **median per-row recovery** | | | | | | | | **%+.1f %%** | **%+.1f %%** |"
          % (100 * statistics.median(recs["a3"]), 100 * statistics.median(recs["a4"])))
    print("\nCORPUS STEP head/base = %.6f (%+.3f %%)" % (corp["a2"] / corp["a1"], 100 * step))
    print("E1 CORPUS RECOVERY %+.2f %%   median per-row %+.2f %%"
          % (100 * cr["a3"], 100 * statistics.median(recs["a3"])))
    print("E2 CORPUS RECOVERY %+.2f %%   median per-row %+.2f %%"
          % (100 * cr["a4"], 100 * statistics.median(recs["a4"])))
    best = max(cr["a3"], cr["a4"])
    print("E3 TRIGGER: best corpus recovery %+.2f %% -- %s"
          % (100 * best, "E3 RUNS (under 80 %)" if best < 0.80 else "E3 does NOT run"))

    print("\n## Per-round spread, for the record (min..max of the five, seconds)\n")
    print("| arm | scored corpus min | max | spread |")
    print("|---|---|---|---|")
    for arm, label in ARMS:
        sums = [sum(wall[(arm, k)][i] for k in scored) for i in range(len(ROUNDS))]
        print("| %s %s | %.6f | %.6f | %.4f %% |"
              % (arm, label, min(sums), max(sums), 100 * (max(sums) / min(sums) - 1)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
