#!/usr/bin/env python3
"""M7 W0 T2 -- the payload-mask re-measurement's comparator.

Regenerates every figure `reading.md` quotes, from `raw/` and `perf/` alone.
Nothing in that file is hand-transcribed.

It walks an EXACT MANIFEST, in W6 T6 fix1's shape: every comparison this leg
owes is listed here, a missing file is a REFUSAL rather than a skipped pair,
the run checks its own totals against what the manifest demands, and the tool's
EXIT STATUS is the counter check's.

  python3 comparator.py --root . --out -

The counter check shells out to `scripts/compare_replay.py` in the repo; set
HVEN_REPO if the checkout is not at /home/ghecht/Projects/hven.
"""

import argparse
import os
import re
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path

ARMS = ["a00", "a-all", "a-scalars", "a-sink"]
SQP_ARMS = ["a00", "a-all", "a-scalars"]
IPM_BATCHES = ["ipmleg", "ipmleg2", "ipmleg3"]
ROUNDS = [1, 2, 3]
EVENTS = ["instructions", "branches", "cycles", "branch-misses", "L1-icache-load-misses"]

REFUSALS = []
COMPARISONS = 0
CELL_COMPARISONS = 0
DIFFERENCES = 0


def cells(root):
    repo = Path(os.environ.get("HVEN_REPO", "/home/ghecht/Projects/hven"))
    line = (repo / "docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt").read_text()
    return line.splitlines()[2].strip().split(",")


def perf_counts(path):
    """Every event this leg counts, from one `perf stat` file. Missing -> None."""
    if not path.is_file() or path.stat().st_size == 0:
        return None
    out = {}
    for line in path.read_text().splitlines():
        m = re.match(r"\s*([0-9,]+)\s+(\S+)\s*$", line)
        if m:
            name = m.group(2).split(":")[0]
            out[name] = int(m.group(1).replace(",", ""))
    return out or None


def median(xs):
    return statistics.median(xs)


def compare_replay(repo, a, b, label):
    global COMPARISONS, CELL_COMPARISONS, DIFFERENCES
    for p in (a, b):
        if not p.is_file():
            REFUSALS.append(f"{label}: MANIFEST REFUSAL -- missing {p}")
            print(f"=== {label} === MANIFEST REFUSAL: {p} is missing")
            return
    r = subprocess.run(
        [sys.executable, str(repo / "scripts/compare_replay.py"), str(a), str(b), label],
        capture_output=True, text=True)
    line = r.stdout.strip()
    print(line if line else f"=== {label} === compare_replay.py produced no output")
    COMPARISONS += 1
    m = re.search(r"cells compared:\s*(\d+)\s+columns compared:\s*(\d+)\s+differences:\s*(\d+)", line)
    if m:
        CELL_COMPARISONS += int(m.group(1)) * int(m.group(2))
        DIFFERENCES += int(m.group(3))
    else:
        REFUSALS.append(f"{label}: compare_replay.py output unparsable ({r.returncode})")
    if r.returncode != 0:
        REFUSALS.append(f"{label}: compare_replay.py exited {r.returncode}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--out", default="-")
    args = ap.parse_args()
    root = Path(args.root).resolve()
    repo = Path(os.environ.get("HVEN_REPO", "/home/ghecht/Projects/hven"))
    sink = sys.stdout if args.out == "-" else open(args.out, "w")
    real_stdout = sys.stdout
    sys.stdout = sink

    cellids = cells(root)
    print("M7 W0 T2 -- THE PAYLOAD-MASK RE-MEASUREMENT, EVERY FIGURE")
    print("=" * 78)
    print()

    # ---------------- the interior leg, three batches ----------------
    print("1. THE TOP-LEVEL INTERIOR-POINT LEG -- instructions, per batch")
    print("-" * 78)
    ipm = {}
    for batch in IPM_BATCHES:
        ipm[batch] = {}
        for arm in ARMS:
            rounds = []
            for r in ROUNDS:
                p = root / "perf" / batch / f"{arm}-r{r}.txt"
                c = perf_counts(p)
                if c is None:
                    REFUSALS.append(f"interior {batch} {arm} r{r}: MANIFEST REFUSAL -- missing {p}")
                    continue
                rounds.append(c)
            if len(rounds) != len(ROUNDS):
                continue
            ipm[batch][arm] = rounds
    for batch in IPM_BATCHES:
        if set(ipm[batch]) != set(ARMS):
            print(f"  {batch}: INCOMPLETE -- refused")
            continue
        base = median([c["instructions"] for c in ipm[batch]["a00"]])
        print(f"  {batch}: a00 median instructions = {base:,}")
        print(f"    {'arm':<12} {'median instr':>18} {'ratio':>10} {'delta %':>9} "
              f"{'own spread %':>13} {'branches':>9} {'cycles':>9}")
        for arm in ARMS:
            xs = [c["instructions"] for c in ipm[batch][arm]]
            m = median(xs)
            spread = 100.0 * (max(xs) - min(xs)) / m
            br = median([c["branches"] for c in ipm[batch][arm]]) / \
                median([c["branches"] for c in ipm[batch]["a00"]])
            cy = median([c["cycles"] for c in ipm[batch][arm]]) / \
                median([c["cycles"] for c in ipm[batch]["a00"]])
            print(f"    {arm:<12} {m:>18,} {m/base:>10.5f} {100*(m/base-1):>+9.3f} "
                  f"{spread:>13.3f} {br:>9.5f} {cy:>9.5f}")
        print(f"    per-round instructions:")
        for arm in ARMS:
            xs = [f"{c['instructions']:,}" for c in ipm[batch][arm]]
            print(f"      {arm:<12} [{', '.join(xs)}]")
        print()

    # ---------------- the SQP corpus, walk mode ----------------
    print("2. THE SQP CORPUS LEG, WALK MODE -- instructions, one round, per cell")
    print("-" * 78)
    sqp = {}
    for arm in SQP_ARMS:
        sqp[arm] = {}
        for cell in cellids:
            p = root / "perf/sqpperf/walk/r1" / f"{arm}-{cell}.txt"
            c = perf_counts(p)
            if c is None:
                REFUSALS.append(f"sqp walk {arm} {cell}: MANIFEST REFUSAL -- missing {p}")
                continue
            sqp[arm][cell] = c
    have = all(len(sqp[a]) == len(cellids) for a in SQP_ARMS)
    if not have:
        print("  INCOMPLETE -- refused")
    else:
        print(f"    {'cell':<32} {'a00 instr':>16} {'a-all':>9} {'a-scalars':>11}")
        ratios = {a: [] for a in SQP_ARMS[1:]}
        totals = {a: 0 for a in SQP_ARMS}
        for cell in cellids:
            b = sqp["a00"][cell]["instructions"]
            row = f"    {cell:<32} {b:>16,}"
            for a in SQP_ARMS:
                totals[a] += sqp[a][cell]["instructions"]
            for a in SQP_ARMS[1:]:
                r = sqp[a][cell]["instructions"] / b
                ratios[a].append(r)
                row += f" {r:>9.5f}" if a == "a-all" else f" {r:>11.5f}"
            print(row)
        print()
        for a in SQP_ARMS[1:]:
            out_of_band = sum(1 for r in ratios[a] if not (0.99 <= r <= 1.01))
            print(f"    {a:<12} per-cell median ratio {median(ratios[a]):.5f}  "
                  f"min {min(ratios[a]):.5f}  max {max(ratios[a]):.5f}  "
                  f"cells outside 0.99-1.01: {out_of_band}/{len(cellids)}")
            corpus = totals[a] / totals["a00"]
            print(f"    {'':<12} whole-corpus ratio    {corpus:.5f}  "
                  f"({100*(corpus-1):+.3f} %)  ->  "
                  f"{'FLAT' if abs(corpus-1) <= 0.005 and out_of_band == 0 else 'MOVED'}")
        print()

    # ---------------- the unattached commit pair (amendment 1) ----------------
    print("2b. THE UNATTACHED PATH, AS A COMMIT PAIR (amendment 1)")
    print("-" * 78)
    up = {}
    for arm in ("old", "new"):
        rounds = []
        for r in ROUNDS:
            pth = root / "perf/unattached" / f"{arm}-r{r}.txt"
            c = perf_counts(pth)
            if c is None:
                REFUSALS.append(f"unattached {arm} r{r}: MANIFEST REFUSAL -- missing {pth}")
                continue
            rounds.append(c)
        if len(rounds) == len(ROUNDS):
            up[arm] = rounds
    if set(up) == {"old", "new"}:
        base = median([c["instructions"] for c in up["old"]])
        for arm in ("old", "new"):
            xs = [c["instructions"] for c in up[arm]]
            m = median(xs)
            joined = ", ".join(f"{x:,}" for x in xs)
            print(f"    {arm:<5} rounds [{joined}]  median {m:,}"
                  f"  spread {100.0 * (max(xs) - min(xs)) / m:.3f} %")
        nm = median([c["instructions"] for c in up["new"]])
        print(f"    ratio new/old = {nm / base:.5f} ({100 * (nm / base - 1):+.3f} %)"
              f" -- see reading.md section 8")
    else:
        print("    INCOMPLETE -- refused")
    print()

    # ---------------- the counter check ----------------
    print("3. THE COUNTER CHECK -- an attached observer changed no trajectory")
    print("-" * 78)
    for batch in IPM_BATCHES:
        for r in ROUNDS:
            a = root / "raw" / batch / f"perf-a00-r{r}.csv"
            for arm in ARMS[1:]:
                b = root / "raw" / batch / f"perf-{arm}-r{r}.csv"
                compare_replay(repo, a, b, f"interior {batch} / perf / r{r} / a00 vs {arm}")
    a = root / "raw/ipmlegwall/wall-a00-r1.csv"
    for arm in ARMS[1:]:
        b = root / "raw/ipmlegwall" / f"wall-{arm}-r1.csv"
        compare_replay(repo, a, b, f"interior wall / r1 / a00 vs {arm}")
    # AMENDMENT 1: the unattached commit pair. Nothing is attached in either
    # arm; what differs is the BINARY, so this asks whether the unattached
    # path's trajectory moved between d8ed6b28 and b8db4b98.
    for r in ROUNDS:
        a = root / "raw/unattached" / f"old-r{r}.csv"
        b = root / "raw/unattached" / f"new-r{r}.csv"
        compare_replay(repo, a, b, f"unattached pair / r{r} / d8ed6b28 vs b8db4b98")
    # THE SINGLE-CELL RUNS WRITE ONE BARE ROW EACH -- `--internal-out` carries no
    # header of its own -- so the 27 rows of an arm are concatenated under the
    # corpus leg's OWN header before the comparison, which is exactly how W6 T6
    # read the same population (its `merge_side`). `schema-header.csv` is one
    # untimed public-leg run of the SAME binary, taken outside every batch and
    # used for its header line alone.
    hdr_src = root / "raw/sqpperf/walk/schema-header.csv"
    if not hdr_src.is_file():
        REFUSALS.append(f"sqp walk: MANIFEST REFUSAL -- missing {hdr_src}")
    else:
        hdr = next(l for l in hdr_src.read_text().splitlines() if not l.startswith("#"))
        # A TEMP DIR, not a directory beside the evidence: running this tool
        # must not add a file to the artifact it reads.
        merged = Path(tempfile.mkdtemp(prefix="m7t2-merged-"))
        sides = {}
        for arm in SQP_ARMS:
            rows = []
            ok = True
            for cell in cellids:
                f = root / "raw/sqpperf/walk/r1" / f"{arm}-{cell}.csv"
                if not f.is_file():
                    REFUSALS.append(f"sqp walk {arm} {cell}: MANIFEST REFUSAL -- missing {f}")
                    ok = False
                    continue
                rows.append(f.read_text().rstrip("\n"))
            if ok:
                dest = merged / f"walk-r1-{arm}.csv"
                dest.write_text(hdr + "\n" + "\n".join(rows) + "\n")
                sides[arm] = dest
        if len(sides) == len(SQP_ARMS):
            for arm in SQP_ARMS[1:]:
                compare_replay(repo, sides["a00"], sides[arm],
                               f"sqp corpus walk / r1 (27 cells merged) / a00 vs {arm}")

    expected = len(IPM_BATCHES) * len(ROUNDS) * 3 + 3 + len(ROUNDS) + 2
    print()
    print("-" * 78)
    print(f"COMPARISONS: {COMPARISONS} (manifest demands {expected})")
    print(f"CELL-COMPARISONS: {CELL_COMPARISONS}")
    print(f"DIFFERENCES: {DIFFERENCES}")
    if COMPARISONS != expected:
        REFUSALS.append(f"comparison total {COMPARISONS} != manifest {expected}")
    if REFUSALS:
        print()
        print("MANIFEST REFUSALS:")
        for r in REFUSALS:
            print(f"  {r}")
    sys.stdout = real_stdout
    if sink is not sys.stdout:
        sink.close()
    if REFUSALS:
        return 2
    return 1 if DIFFERENCES else 0


if __name__ == "__main__":
    sys.exit(main())
