#!/usr/bin/env python3
"""M8 DN-0: attribute one cell's perf samples to buckets with the frozen bucket_map.tsv.

Input: `perf script --no-inline -F comm,tid,period,ip,sym,dso` text (optionally .gz) of a
`perf record -e instructions:u -c PERIOD --call-graph dwarf --user-callchains` run of the
lane binary. Only samples whose comm is hven_bench_cell are read.

Output: one JSON object (stdout) with instructions-weighted bucket totals (samples x period),
the fallback / unmatched breakdown, chain-depth evidence, and the top normalized self symbols
per bucket. The rule (bucket_map.tsv header) is the only classifier; this file adds none.

Usage: attribute.py BUCKET_MAP SCRIPT[.gz] [--top N]
"""
import collections
import gzip
import json
import re
import sys

BUCKETS = ["B_sym", "B_fac", "B_kkt", "B_eval", "B_setup", "B_eng", "B_proc"]
COMM = "hven_bench_cell"
FB_FAC_SYM = re.compile(r"^(mkl_|idt_|LAPACKE|__kmp|kmp_|_mkl|cdecl_|d(sytr|getr|potr|gemm|gemv|trsm|trsv|syrk|lasyf|lasy))")
FB_FAC_DSO = ("libiomp5",)
FB_EVAL_DSO = ("libcutest", "libpb.so", "libgfortran")
FB_PROC_DSO = ("ld-linux",)


def load_map(path):
    rows = []
    with open(path) as f:
        for line in f:
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.rstrip("\n").split("\t")
            bucket, pattern = parts[0], parts[1]
            if bucket not in BUCKETS:
                raise SystemExit(f"bucket_map: unknown bucket {bucket!r}")
            rows.append((bucket, re.compile(r"(?:^|[ <*&])" + pattern), pattern))
    return rows


def normalize(sym):
    s = sym.replace("(anonymous namespace)", "{anon}")
    k = s.find("(")
    if k >= 0:
        s = s[:k]
    return s.strip()


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
                head = (parts[0], int(parts[-1]))
                frames = []
                continue
            m = re.match(r"\s*([0-9a-f]+)\s+(.*)\s+\((.*)\)\s*$", line)
            if m:
                frames.append((m.group(2).strip(), m.group(3)))
            else:
                frames.append((line.strip(), ""))
        if head is not None:
            yield head, frames


def classify(frames, rows, cache):
    for sym, _dso in frames:
        n = cache.get(sym)
        if n is None:
            n = normalize(sym)
            hit = None
            for bucket, rx, _p in rows:
                if rx.search(n):
                    hit = bucket
                    break
            cache[sym] = n = (n, hit)
        if n[1] is not None:
            return n[1], "rule"
    if not frames:
        return "B_eng", "unmatched"
    leaf_sym, leaf_dso = frames[0]
    if any(t in leaf_dso for t in FB_PROC_DSO):
        return "B_proc", "fallback"
    if FB_FAC_SYM.match(leaf_sym) or any(t in leaf_dso for t in FB_FAC_DSO):
        return "B_fac", "fallback"
    if any(t in leaf_dso for t in FB_EVAL_DSO):
        return "B_eval", "fallback"
    return "B_eng", "unmatched"


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    rows = load_map(sys.argv[1])
    top_n = 8
    if "--top" in sys.argv:
        top_n = int(sys.argv[sys.argv.index("--top") + 1])
    tot = collections.Counter()
    how = collections.Counter()
    fb = collections.Counter()
    selfsym = collections.defaultdict(collections.Counter)
    unmatched_leaf = collections.Counter()
    reach_main = 0
    nsamp = 0
    period = None
    mkl_serv = 0
    kmp = 0
    init = 0
    fac_split = collections.Counter()
    cache = {}
    for (comm, per), frames in samples(sys.argv[2]):
        if comm != COMM:
            continue
        period = per if period is None else period
        nsamp += 1
        bucket, mode = classify(frames, rows, cache)
        tot[bucket] += per
        how[mode] += per
        if mode == "fallback":
            fb[bucket] += per
        if mode == "unmatched":
            unmatched_leaf[normalize(frames[0][0]) if frames else "<empty>"] += per
        leaf = normalize(frames[0][0]) if frames else "<empty>"
        selfsym[bucket][leaf] += per
        syms = [s for s, _ in frames]
        if any(normalize(s) == "main" for s in syms):
            reach_main += 1
        if any(s.startswith("mkl_serv") for s in syms):
            mkl_serv += per
        if any(s.startswith("__kmp") for s in syms):
            kmp += per
        norms = [normalize(s) for s in syms]
        if any(n.endswith("ensure_solver_initialized") for n in norms):
            init += per
        if bucket == "B_fac":
            key = next((n for n in norms if n.startswith("hven::linear::")), "<no hven::linear frame>")
            fac_split[key] += per
    total_all = sum(tot.values())
    total_solver = total_all - tot["B_proc"]
    ss_total = total_solver - init
    ss = {b: tot[b] for b in BUCKETS if b != "B_proc"}
    ss["B_setup"] -= init
    out = {
        "samples": nsamp,
        "period": period,
        "instr_sampled_all": total_all,
        "instr_sampled_solver": total_solver,
        "bucket_instr": {b: tot[b] for b in BUCKETS},
        "share_of_solver": {b: (tot[b] / total_solver if total_solver else None)
                            for b in BUCKETS if b != "B_proc"},
        "proc_share_of_all": tot["B_proc"] / total_all if total_all else None,
        "rule_instr": how["rule"],
        "fallback_instr": how["fallback"],
        "fallback_by_bucket": dict(fb),
        "unmatched_instr": how["unmatched"],
        "unmatched_share_of_solver": how["unmatched"] / total_solver if total_solver else None,
        "chains_reaching_main": reach_main / nsamp if nsamp else None,
        "init_instr": init,
        "steady_state_instr": ss_total,
        "steady_state_share": {b: (ss[b] / ss_total if ss_total else None) for b in ss},
        "fac_split": fac_split.most_common(),
        "mkl_serv_instr": mkl_serv,
        "kmp_instr": kmp,
        "top_unmatched_leaf": unmatched_leaf.most_common(top_n),
        "top_self_by_bucket": {b: selfsym[b].most_common(top_n) for b in BUCKETS},
    }
    json.dump(out, sys.stdout, indent=1)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
