#!/usr/bin/env python3
"""M8 DN-0 IPM r1 (post-hoc, INFORMATIONAL; not a rule input): sampling uncertainty of a cell's
(B_sym+B_fac)/total share.

A cell's P samples come from R repeats at distinct periods (DECLARATION-r1 §3), so the period
identifies the repeat. With attribute.py's classifier and bucket_map.tsv unchanged, this computes
the share per repeat, then the pooled share (as the rule reads it), the across-repeat mean, the
standard error of that mean (sd/sqrt(R)), and the binomial standard error on the solver-sample count.

Usage: share_uncertainty.py TOOLS_DIR RECORD_DIR TAG [TAG...]
"""
import collections
import math
import os
import statistics
import sys


def main():
    tools, rec = sys.argv[1], sys.argv[2]
    sys.path.insert(0, tools)
    import attribute as A
    rows = A.load_map(os.path.join(tools, "bucket_map.tsv"))
    print("tag\treps\tsolver_samples\tshare_pooled\tshare_rep_mean\tse_rep_mean\tse_binomial\trep_min\trep_max")
    for tag in sys.argv[3:]:
        per_rep = collections.defaultdict(collections.Counter)
        cache = {}
        nsolver = 0
        for (comm, per), frames in A.samples(os.path.join(rec, tag + ".script.gz")):
            if comm != A.COMM:
                continue
            b, _ = A.classify(frames, rows, cache)
            per_rep[per][b] += per
            if b != "B_proc":
                nsolver += 1
        shares = []
        tot_f = tot_s = 0
        for per, c in per_rep.items():
            s = sum(v for k, v in c.items() if k != "B_proc")
            f = c["B_sym"] + c["B_fac"]
            tot_f += f
            tot_s += s
            if s:
                shares.append(f / s)
        p = tot_f / tot_s
        m = statistics.mean(shares)
        se = statistics.stdev(shares) / math.sqrt(len(shares)) if len(shares) > 1 else float("nan")
        seb = math.sqrt(p * (1 - p) / nsolver) if nsolver else float("nan")
        print(f"{tag}\t{len(shares)}\t{nsolver}\t{p:.4f}\t{m:.4f}\t{se:.4f}\t{seb:.4f}\t{min(shares):.3f}\t{max(shares):.3f}")


if __name__ == "__main__":
    main()
