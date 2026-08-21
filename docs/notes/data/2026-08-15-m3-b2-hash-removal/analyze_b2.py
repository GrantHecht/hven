#!/usr/bin/env python3
"""B2 P-BENCH analysis.

Part 1 -- arm S vs arm S2: the B1 direct-attribution scaffold, re-run on the
shipped post-B2 engine over the 11 headline cells. Asserted: the hash count
per factorization and per SSN major. Informational: SSN-major wall.

Part 2 -- arm A / arm B / arm P: per-cell reported solve wall, median of 4
reps, over the 17-cell B1 list. A and B are B1's committed artifacts (base
and the measurement patch); P is the shipped post-B2 engine.
"""

import re
import statistics
import sys

ART = "/home/ghecht/Projects/hven/docs/notes/data/2026-08-15-m3-b1-hash-cost"
SP = "/tmp/claude-1000/-home-ghecht-Projects-hven/befb7a32-f021-43a2-b05c-b078913d7b67/scratchpad"

HEADLINE = [
    "f7_n750_path_neutral_control",
    "f7_n800_path_neutral",
    "f7_n800_path_corrupted",
    "f7_n800_path_warm",
    "f7_n825_path_neutral_control",
    "f7_n1000_path_neutral",
    "f7_n1000_path_corrupted",
    "f7_n1000_path_warm",
    "f7_n5000_path_activity",
    "f7_n10000_path_activity",
    "f7_n20000_path_activity",
]

PROBE = re.compile(
    r"hash_calls=(\d+) hash_nnz=(\d+) hash_ns=(\d+) "
    r"fact_calls=(\d+) fact_ns=(\d+) major_calls=(\d+) major_ns=(\d+)"
)


def read_probe(path):
    """cell -> the solving child's accumulators (the non-zero [B1PROBE] line)."""
    out = {}
    cell = None
    for line in open(path):
        if line.startswith("###"):
            cell = line.split()[1]
            continue
        m = PROBE.search(line)
        if m and cell:
            v = [int(x) for x in m.groups()]
            if v[0] == 0:  # the parent process's empty dump
                continue
            out[cell] = dict(
                zip("hash_calls hash_nnz hash_ns fact_calls fact_ns major_calls major_ns".split(), v)
            )
    return out


def read_csv_wall(path):
    """cell -> (wall_s, the 13 asserted columns) from a corpus CSV."""
    out = {}
    hdr = None
    for line in open(path):
        line = line.rstrip("\n")
        if line.startswith("#") or not line:
            continue
        f = line.split(",")
        if f[0] == "cell_id":
            hdr = f
            continue
        out[f[0]] = (float(f[hdr.index("wall_s")]), f[:13])
    return out


def med(vals):
    return statistics.median(vals)


def main():
    base = read_probe(f"{ART}/scaffold-raw-corpus.txt")
    post = read_probe(f"{SP}/pbench-raw.txt")

    print("=" * 106)
    print("PART 1 -- arm S (base, 31e57b0) vs arm S2 (shipped, post-B2): the B1 scaffold, 11 headline cells")
    print("=" * 106)
    print(
        f"{'cell':<32}{'facts':>6}{'majors':>7}{'h/fact':>9}{'h/fact':>9}"
        f"{'h/maj':>8}{'h/maj':>8}{'ns/maj S':>12}{'ns/maj S2':>12}{'maj wall':>10}{'pred':>8}"
    )
    print(f"{'':<32}{'':>6}{'':>7}{'S':>9}{'S2':>9}{'S':>8}{'S2':>8}{'':>12}{'':>12}{'recov':>10}{'X%maj':>8}")
    print("-" * 106)
    counter_moves = []
    recov = []
    preds = []
    tot_maj_s = tot_maj_s2 = 0
    for c in HEADLINE:
        b, p = base[c], post[c]
        if b["fact_calls"] != p["fact_calls"] or b["major_calls"] != p["major_calls"]:
            counter_moves.append(c)
        hf_b = b["hash_calls"] / b["fact_calls"]
        hf_p = p["hash_calls"] / p["fact_calls"]
        hm_b = b["hash_calls"] / b["major_calls"]
        hm_p = p["hash_calls"] / p["major_calls"]
        nsm_b = b["major_ns"] / b["major_calls"]
        nsm_p = p["major_ns"] / p["major_calls"]
        r = 100.0 * (nsm_b - nsm_p) / nsm_b
        # B1's own per-cell prediction: one hash at the measured per-call cost,
        # as a share of the base SSN-major wall.
        x = 100.0 * (b["hash_ns"] / b["hash_calls"]) / nsm_b
        recov.append(r)
        preds.append(x)
        tot_maj_s += b["major_ns"]
        tot_maj_s2 += p["major_ns"]
        print(
            f"{c:<32}{b['fact_calls']:>6}{b['major_calls']:>7}{hf_b:>9.2f}{hf_p:>9.2f}"
            f"{hm_b:>8.2f}{hm_p:>8.2f}{nsm_b:>12.0f}{nsm_p:>12.0f}{r:>9.2f}%{x:>7.2f}%"
        )
    print("-" * 106)
    pooled = 100.0 * (tot_maj_s - tot_maj_s2) / tot_maj_s
    print(f"h/fact:  S = {min(base[c]['hash_calls']/base[c]['fact_calls'] for c in HEADLINE):.2f}"
          f"-{max(base[c]['hash_calls']/base[c]['fact_calls'] for c in HEADLINE):.2f}"
          f"   S2 = {min(post[c]['hash_calls']/post[c]['fact_calls'] for c in HEADLINE):.2f}"
          f"-{max(post[c]['hash_calls']/post[c]['fact_calls'] for c in HEADLINE):.2f}")
    print(f"h/major: S = {min(base[c]['hash_calls']/base[c]['major_calls'] for c in HEADLINE):.2f}"
          f"-{max(base[c]['hash_calls']/base[c]['major_calls'] for c in HEADLINE):.2f}"
          f"   S2 = {min(post[c]['hash_calls']/post[c]['major_calls'] for c in HEADLINE):.2f}"
          f"-{max(post[c]['hash_calls']/post[c]['major_calls'] for c in HEADLINE):.2f}")
    print(f"SSN-major wall recovery: per-cell {min(recov):.2f}%-{max(recov):.2f}% "
          f"(median {med(recov):.2f}%), pooled {pooled:.2f}%")
    print(f"B1 prediction (X%maj):   per-cell {min(preds):.2f}%-{max(preds):.2f}% "
          f"(median {med(preds):.2f}%)")
    print(f"counter movement (facts/majors): {counter_moves or 'NONE'}")

    # ---------------------------------------------------------------- part 2
    print()
    print("=" * 106)
    print("PART 2 -- reported solve wall, median of 4 reps: arm A (base) / arm B (B1 patch) / arm P (shipped)")
    print("=" * 106)
    arms = {}
    for arm, pat in (("A", f"{ART}/corpus_A_r%d.csv"), ("B", f"{ART}/corpus_B_r%d.csv"),
                     ("P", f"{SP}/prodab/corpus_P_r%d.csv")):
        arms[arm] = [read_csv_wall(pat % r) for r in (1, 2, 3, 4)]

    cells = list(arms["A"][0].keys())
    pure = [c for c in cells if all(
        read_csv_wall(f"{ART}/corpus_A_r1.csv")[c][1] for _ in [0])]
    del pure

    # counter identity across all three arms, all reps, all 13 asserted columns
    bad = []
    for c in cells:
        ref = arms["A"][0][c][1]
        for arm in arms:
            for rep in arms[arm]:
                if rep[c][1] != ref:
                    bad.append((c, arm))
    print(f"13-column counter identity over {len(cells)} cells x 3 arms x 4 reps: "
          f"{'IDENTICAL' if not bad else 'DIFFERENCES ' + str(sorted(set(bad)))}")
    print()
    print(f"{'cell':<32}{'A (s)':>11}{'B (s)':>11}{'P (s)':>11}{'P vs A':>10}{'P vs B':>10}")
    print("-" * 85)
    tot = {a: 0.0 for a in arms}
    hl_tot = {a: 0.0 for a in arms}
    for c in sorted(cells):
        m = {a: med([rep[c][0] for rep in arms[a]]) for a in arms}
        for a in arms:
            tot[a] += m[a]
            if c in HEADLINE:
                hl_tot[a] += m[a]
        dpa = 100.0 * (m["P"] - m["A"]) / m["A"]
        dpb = 100.0 * (m["P"] - m["B"]) / m["B"]
        mark = " *" if c in HEADLINE else ""
        print(f"{c:<32}{m['A']:>11.4f}{m['B']:>11.4f}{m['P']:>11.4f}{dpa:>9.2f}%{dpb:>9.2f}%{mark}")
    print("-" * 85)
    for label, t in (("all 17 pooled", tot), ("11 headline pooled", hl_tot)):
        print(f"{label:<32}{t['A']:>11.4f}{t['B']:>11.4f}{t['P']:>11.4f}"
              f"{100.0*(t['P']-t['A'])/t['A']:>9.2f}%{100.0*(t['P']-t['B'])/t['B']:>9.2f}%")
    print()
    print("B1's headline prediction: -5.58 % pooled over its 13 pure-SSN cells (arm B vs arm A).")


if __name__ == "__main__":
    main()
