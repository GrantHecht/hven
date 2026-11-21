#!/usr/bin/env python3
"""PF-3 extra tables: escape census over every cell, fixed overhead, pathologies, per-minor cost, solve-leaf breakdown.
usage: pf3_extra.py <dirs comma> <out.md>"""
import collections, glob, json, os, statistics, sys

cells = []
for d in sys.argv[1].split(","):
    for f in sorted(glob.glob(d + "/*.json")):
        c = json.load(open(f))
        if "error" in c:
            continue
        c["_set"] = "sd" if os.path.basename(d.rstrip("/")) == "cells-sd" else ("hs" if c["problem"].startswith("hs") else "nlp35")
        cells.append(c)
for c in cells:   # same family-conditional redistribution as pf3_report.py
    comp = dict(c["components"]); un = comp.pop("unattributed.truncated_unwind", 0); moved = 0.0
    for fam, amt in c.get("blas_family_truncated", {}).items():
        d = c.get("blas_family_attributed", {}).get(fam, {}); z = sum(d.values())
        if z > 0:
            for k, v in d.items():
                comp[k] = comp.get(k, 0) + amt * v / z
            moved += amt
    if un - moved > 1e-9 * max(un, 1):
        comp["unattributed.unresolved"] = comp.get("unattributed.unresolved", 0) + un - moved
    c["components"] = comp
md = []
fl = lambda c, k: float(c["fields"].get(k) or 0)
# --- escapes ---
ESC = ["side.ssn.ssn_escapes", "side.ipqp.ipqp_escapes", "side.elastic_from_ipqp_escape"] + [k for k in cells[0]["fields"] if "escape" in k and k not in ("side.ssn.ssn_escapes", "side.ipqp.ipqp_escapes", "side.elastic_from_ipqp_escape")]
ESC = [k for k in dict.fromkeys(ESC) if any(k in c["fields"] for c in cells)]
md += ["## Escape counters over every measured cell (all rows of this task at the pin)", ""]
sq = [c for c in cells if c["lane"] == "hven-sqp" and not c.get("truncated")]
tr = [c for c in cells if c["lane"] == "hven-sqp" and c.get("truncated")]
md.append(f"hven-sqp full cells examined: {len(sq)} (HS {sum(1 for c in sq if c['_set']=='hs')}, nlp35 {sum(1 for c in sq if c['_set']=='nlp35')}, small-dense {sum(1 for c in sq if c['_set']=='sd')}); truncated hven-sqp cells (counters absent, trunc30 mode): {len(tr)}; hven-ipm cells carry no SQP escape counters.")
md += ["", "| counter | sum over cells | cells with value > 0 |", "|---|---:|---|"]
for k in ESC:
    s = sum(fl(c, k) for c in sq); nz = [c["problem"] for c in sq if fl(c, k) > 0]
    md.append(f"| {k} | {s:.0f} | {', '.join(nz[:12]) or '-'}{' ...' if len(nz) > 12 else ''} |")
# --- fixed overhead ---
md += ["", "## Fixed overhead on small cells (per-cell instructions; full cells)", "",
       "| set / lane | cells | median total | median loader | median library_init | median problem_load | median net solver (total - proc.* - library_init) | net as share of total |", "|---|---:|---:|---:|---:|---:|---:|---:|"]
for s in ("hs", "sd", "nlp35"):
    for lane in ("hven-ipm", "hven-sqp"):
        g = [c for c in cells if c["_set"] == s and c["lane"] == lane and not c.get("truncated")]
        if not g:
            continue
        comp = lambda c, k: c["components"].get(k, 0)
        T = [c["instructions_total"] for c in g]
        ld = [comp(c, "proc.loader_startup") for c in g]; li = [comp(c, "setup.library_init") for c in g]; pl = [comp(c, "proc.problem_load") for c in g]
        net = [c["instructions_total"] - sum(v for k, v in c["components"].items() if k.startswith("proc.")) - comp(c, "setup.library_init") for c in g]
        md.append(f"| {s} / {lane} | {len(g)} | {statistics.median(T):.3g} | {statistics.median(ld):.3g} | {statistics.median(li):.3g} | {statistics.median(pl):.3g} | {statistics.median(net):.3g} | {100*statistics.median([n/t for n, t in zip(net, T)]):.1f}% |")
# --- pathologies ---
md += ["", "## Named factorization-pathology cells (small-dense TRAIN)", ""]
for p in ("SPANHYD", "CRESC100", "MSS1"):
    for lane in ("hven-ipm", "hven-sqp"):
        c = next((c for c in cells if c["problem"] == p and c["lane"] == lane), None)
        if not c:
            continue
        f = c["fields"]; T = c["instructions_total"]
        top = sorted(((v / T, k) for k, v in c["components"].items()), reverse=True)[:5]
        dk = {k.replace("side.", ""): f[k] for k in f if any(s in k for s in ("dense_fact", "dense_singular", "dense_flagged", "dense_shifted", "singular_refactors", "inertia_wrong", "qp_minor", "major_iters", "symbolic_analyses", "border_refine", "kkt_factor_counters.factorize_count", "kkt_factor_counters.dense_factorize_count", "kkt_factor_counters.dense_singular_count", "kkt_factor_counters.dense_fallback_count", "kkt_factor_counters.analyze_count")) and f[k] not in ("0", "")}
        md.append(f"- **{c['cell']}**: status {c['status']}, iterations {c['iterations']}, factorizations {c['factorizations']}, instructions {T:.4g}; top components " + ", ".join(f"{k} {100*v:.1f}%" for v, k in top) + f"; counters {dk}")
# --- per-minor ---
md += ["", "## SQP per-minor and per-factorization cost, cells with >= 100 QP minors (full cells)", "",
       "| cell | instructions | minors | factorizations | instr/minor | instr/factorization | la.sparse_solve | la.sparse_analyze | la.sparse_factorize | qp.schur_border | qp.* total |", "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
rows = []
for c in cells:
    if c["lane"] != "hven-sqp" or c.get("truncated"):
        continue
    mi = fl(c, "side.qp_minor_iters")
    if mi < 100:
        continue
    T = c["instructions_total"]; fz = fl(c, "factorizations")
    sh = lambda k: 100 * c["components"].get(k, 0) / T
    q = sum(v for k, v in c["components"].items() if k.startswith("qp.")) / T * 100
    rows.append((T, f"| {c['cell']} | {T:.3g} | {mi:.0f} | {fz:.0f} | {T/mi:.3g} | {T/max(fz,1):.3g} | {sh('la.sparse_solve'):.1f}% | {sh('la.sparse_analyze'):.1f}% | {sh('la.sparse_factorize'):.1f}% | {sh('qp.schur_border'):.1f}% | {q:.1f}% |"))
for _, r in sorted(rows, reverse=True):
    md.append(r)
# --- leaf breakdown within sparse_solve, sqp
leaf = collections.Counter(); z = 0
for c in cells:
    if c["lane"] != "hven-sqp" or c.get("truncated"):
        continue
    for s, d, comp, v in c["top_leaf"]:
        if comp == "la.sparse_solve":
            leaf[s[:70]] += v
    z += c["components"].get("la.sparse_solve", 0)
md += ["", "## Leaf symbols inside la.sparse_solve (hven-sqp full cells; share of the component, from each cell's top-60 leaves)", "", "| leaf | share of la.sparse_solve |", "|---|---:|"]
for k, v in leaf.most_common(12):
    md.append(f"| `{k}` | {100*v/z:.1f}% |")
open(sys.argv[2], "w").write("\n".join(md) + "\n")
print("ok")
