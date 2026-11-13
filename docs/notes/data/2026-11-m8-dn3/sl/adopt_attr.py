#!/usr/bin/env python3
"""M8 DN-3 (../DECLARATION-r1.md, A2): one (cell, arm)'s init and process shares from its perf samples.

Input: `perf script --no-inline -F comm,pid,tid,period,ip,sym,dso` text (optionally .gz) of the R repeats of
`perf record -e instructions:u -c PERIOD --call-graph dwarf,16384 --user-callchains` on the CFI RECORD binary,
concatenated. Only samples whose comm is hven_bench_cell are read. Every share is period-weighted over those samples.

  s_init  the share of samples whose chain holds ANY frame matching INIT (the declared process-once init set);
  s_proc  the share of samples NOT in s_init that DN-0's frozen bucket_map.tsv (attribute.py's classify, leaf to root,
          its fallbacks included) puts in B_proc;
  se      the binomial standard error of s_init + s_proc on the sample count (informational);
  rd      R-d: samples in neither share, on the main thread (pid == tid), with a TRUNCATED chain (no frame `main` and no
          frame naming hven or hven_bench) and a leaf in libiomp5, libc or ld-linux, as a share (what symbol matching
          cannot claim; reported, not subtracted).

Usage: adopt_attr.py BUCKET_MAP SCRIPT[.gz]   -> one JSON object on stdout
"""
import collections
import gzip
import json
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import attribute as A  # noqa: E402  (DN-0's classifier, byte copy beside this file)

INIT = re.compile(r"hven::detail::ensure_solver_initialized|mkl_get_max_threads|MKL_Read_ThreadENV_Count_Cores|"
                  r"__kmp_(do_)?(serial|middle|parallel)_initialize|__kmp_internal_begin|"
                  r"(mkl_serv_)?dsecnd|mkl_serv_get_clocks_frequency")
RD_DSO = ("libiomp5", "libc.so", "libc-", "ld-linux")


def samples(path):
    opener = gzip.open if path.endswith(".gz") else open
    with opener(path, "rt", errors="replace") as f:
        head = None
        frames = []
        for line in f:
            if not line.strip():
                if head is not None:
                    yield head, frames
                head, frames = None, []
                continue
            if line[0] not in " \t":
                if head is not None:
                    yield head, frames
                parts = line.split()
                comm = parts[0]
                pid = tid = None
                for t in parts[1:]:
                    if "/" in t:
                        a, b = t.split("/", 1)
                        if a.isdigit() and b.isdigit():
                            pid, tid = int(a), int(b)
                            break
                head = (comm, pid, tid, int(parts[-1]))
                frames = []
                continue
            m = re.match(r"\s*([0-9a-f]+)\s+(.*)\s+\((.*)\)\s*$", line)
            if m:
                frames.append((m.group(2).strip(), m.group(3)))
            else:
                frames.append((line.strip(), ""))
        if head is not None:
            yield head, frames


def main():
    rows = A.load_map(sys.argv[1])
    cache = {}
    tot = init = proc = rd = 0
    n = n_ip = 0
    periods = collections.Counter()
    for (comm, pid, tid, per), frames in samples(sys.argv[2]):
        if comm != A.COMM:
            continue
        n += 1
        tot += per
        periods[per] += 1
        norms = [A.normalize(s) for s, _ in frames]
        claimed = False
        if any(INIT.search(x) for x in norms):
            init += per
            n_ip += 1
            claimed = True
        else:
            bucket, _mode = A.classify(frames, rows, cache)
            if bucket == "B_proc":
                proc += per
                n_ip += 1
                claimed = True
        # R-d reads what the two shares do NOT claim (r0 A2: "reported, not subtracted").
        truncated = not any(x == "main" or "hven" in x for x in norms)
        if not claimed and truncated and pid is not None and pid == tid:
            leaf_dso = frames[0][1] if frames else ""
            if any(t in leaf_dso for t in RD_DSO):
                rd += per
    p = n_ip / n if n else float("nan")
    out = {
        "samples": n,
        "repeats": len(periods),
        "sampled_period_total": tot,
        "init_period": init,
        "proc_period": proc,
        "s_init": init / tot if tot else None,
        "s_proc": proc / tot if tot else None,
        "se_share": math.sqrt(p * (1 - p) / n) if n else None,
        "rd_share": rd / tot if tot else None,
    }
    json.dump(out, sys.stdout, indent=1)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
