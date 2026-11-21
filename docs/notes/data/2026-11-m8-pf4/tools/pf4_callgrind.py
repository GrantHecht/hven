#!/usr/bin/env python3
"""PF-4 step 1: call counts and the caller graph from a callgrind out file.
usage: pf4_callgrind.py <callgrind.out>  -> JSON on stdout
Reports: call totals for the border/analysis functions; the immediate callers of each (callgrind's own caller
records, inlined frames attribute to their outer function); the average border batch = add_border calls from
sync_borders / factor_c calls from sync_borders (end_batch's one factorization per sync that added >= 1 border)."""
import collections, json, re, sys
names, calls = {}, collections.Counter()
callers = collections.defaultdict(collections.Counter)
pending = cur = None
for line in open(sys.argv[1], errors="replace"):
    m = re.match(r"^c?fn=\((\d+)\)(?: (.*))?$", line)
    if m:
        if m.group(2):
            names[m.group(1)] = m.group(2).strip()
        if line.startswith("cfn="):
            pending = m.group(1)
        else:
            cur = m.group(1)
        continue
    if line.startswith("calls=") and pending is not None:
        n = int(line.split("=")[1].split()[0])
        callee = names.get(pending, "?" + pending)
        calls[callee] += n
        callers[callee][names.get(cur, "?" + str(cur))] += n
        pending = None


def short(n):
    return re.sub(r"\(.*", "", n)[:120]


KEYS = ["QpEngine::sync_borders", "SchurComplement::add_border", "SchurComplement::factor_c", "SchurComplement::end_batch",
        "SchurComplement::replay_batch", "SchurComplement::drop_border", "SchurComplement::solve(", "QpEngine::rebuild_k0",
        "SymmetricFactor::analyze", "FactorSession::analyze", "SymmetricFactor::factorize(", "FactorSession::factorize",
        "FactorSession::solve(", "SymmetricFactor::solve_single", "SymmetricFactor::solve(", "detail::solve_vec",
        "factorize_checked", "analyze_if_needed", "take_retake", "retake_on_platform", "QpEngine::run("]
tot = {k: sum(v for n, v in calls.items() if k in n) for k in KEYS}
graph = {}
for k in ["SchurComplement::add_border", "SchurComplement::factor_c", "SymmetricFactor::analyze", "FactorSession::analyze",
          "factorize_checked", "analyze_if_needed", "SymmetricFactor::factorize(", "QpEngine::rebuild_k0", "take_retake",
          "retake_on_platform"]:
    agg = collections.Counter()
    for callee, cs in callers.items():
        if k in callee:
            for c, v in cs.items():
                agg[short(c)] += v
    graph[k] = dict(agg.most_common(20))
adds = graph["SchurComplement::add_border"].get("hven::detail::QpEngine::sync_borders", 0)
syncs_adding = graph["SchurComplement::factor_c"].get("hven::detail::QpEngine::sync_borders", 0)
print(json.dumps({"calls": tot, "callers": graph,
                  "batch": {"add_border_from_sync": adds, "sync_borders_calls": tot["QpEngine::sync_borders"],
                            "factor_c_from_sync (syncs that added >=1)": syncs_adding,
                            "mean_adds_per_sync": adds / max(1, tot["QpEngine::sync_borders"]),
                            "mean_adds_per_adding_sync": adds / max(1, syncs_adding)}}, indent=1))
