#!/usr/bin/env python3
"""PF-3 aggregator: per-cell JSONs -> markdown/CSV tables.   usage: pf3_report.py <cells_dir> <out_prefix>"""
import csv, glob, json, os, statistics, sys, collections

cd, outp = sys.argv[1], sys.argv[2]
cells = [json.load(open(f)) for f in sorted(glob.glob(cd + "/*.json"))]
bad = [c for c in cells if "error" in c]
cells = [c for c in cells if "error" not in c]
unit = cells[0].get("unit", "instructions")
for c in cells:
    c["set"] = "hs" if c["problem"].startswith("hs") else "cutest"
    c["engine"] = c["lane"].replace("hven-", "")
OC = lambda k: k.split(".")[0]
# FAMILY-CONDITIONAL REDISTRIBUTION of chains truncated inside MKL assembly kernels (DECLARATION-PF-3.md section 4, amended):
# a truncated leaf in BLAS family F is split over components in proportion to the same cell's fully-unwound samples whose
# chain holds an F-family MKL BLAS frame.  The raw (unredistributed) numbers stay in each cell JSON and in -raw tables.
RAW_UNATTR = {}
for c in cells:
    comp = dict(c["components"]); c["components_raw"] = dict(c["components"])
    un_trunc = comp.pop("unattributed.truncated_unwind", 0)
    moved = 0.0
    for fam, amt in c.get("blas_family_truncated", {}).items():
        d = c.get("blas_family_attributed", {}).get(fam, {})
        z = sum(d.values())
        if z > 0:
            for k, v in d.items():
                comp[k] = comp.get(k, 0) + amt * v / z
            moved += amt
    rest = un_trunc - moved
    if rest > 1e-9 * max(un_trunc, 1):
        comp["unattributed.unresolved"] = comp.get("unattributed.unresolved", 0) + rest
    c["components"] = comp
    RAW_UNATTR[c["cell"]] = un_trunc
ALL = sorted({k for c in cells for k in c["components"]})
SOLVER = lambda k: not k.startswith("proc.")


def tot(c, pred=lambda k: True):
    return sum(v for k, v in c["components"].items() if pred(k))


def shares(group, pred=lambda k: True):
    """corpus (sum) shares and median per-cell shares, by component"""
    den = sum(tot(c, pred) for c in group)
    out = {}
    for k in ALL:
        if not pred(k):
            continue
        s = sum(c["components"].get(k, 0) for c in group)
        per = [c["components"].get(k, 0) / tot(c, pred) for c in group if tot(c, pred) > 0]
        out[k] = (s / den if den else 0, statistics.median(per) if per else 0, s)
    return out, den


def md_table(group, title, pred=lambda k: True):
    sh, den = shares(group, pred)
    lines = [f"### {title}", f"cells: {len(group)}; total ({unit}): {den:.6g}", "",
             "| component | corpus share | median per-cell share | total |", "|---|---:|---:|---:|"]
    for k, (a, m, s) in sorted(sh.items(), key=lambda x: -x[1][0]):
        if a < 0.0005 and m < 0.0005:
            continue
        lines.append(f"| {k} | {100*a:.2f}% | {100*m:.2f}% | {s:.4g} |")
    # owner categories
    cat = collections.defaultdict(lambda: [0, []])
    for c in group:
        d = tot(c, pred)
        cc = collections.defaultdict(float)
        for k, v in c["components"].items():
            if pred(k):
                cc[OC(k.replace("unattributed.truncated_unwind", "unattributed").replace("unattributed.other", "unattributed"))] += v
        for k in set(cc) | set(cat):
            pass
    cats = collections.defaultdict(float); per = collections.defaultdict(list)
    for c in group:
        d = tot(c, pred)
        cc = collections.defaultdict(float)
        for k, v in c["components"].items():
            if pred(k):
                cc[OC(k)] += v
        for k, v in cc.items():
            cats[k] += v
        for k in set(OC(x) for x in ALL if pred(x)):
            per[k].append(cc.get(k, 0) / d if d else 0)
    lines += ["", "owner categories:", "", "| category | corpus share | median per-cell share |", "|---|---:|---:|"]
    for k, v in sorted(cats.items(), key=lambda x: -x[1]):
        lines.append(f"| {k} | {100*v/den:.2f}% | {100*statistics.median(per[k]):.2f}% |")
    return "\n".join(lines) + "\n"


full = [c for c in cells if not c.get("truncated")]
trunc = [c for c in cells if c.get("truncated")]
md = [f"# PF-3 attribution tables ({unit})", f"cells read: {len(cells)} ({len(full)} full, {len(trunc)} truncated); failed: {len(bad)}", ""]
if bad:
    md.append("FAILED: " + ", ".join(c["cell"] for c in bad) + "\n")
for name, grp in [("SUITE (all full cells)", full)] + [(f"{e} / all", [c for c in full if c["engine"] == e]) for e in ("ipm", "sqp")] + \
        [(f"{e} / {s}", [c for c in full if c["engine"] == e and c["set"] == s]) for e in ("ipm", "sqp") for s in ("hs", "cutest")]:
    if grp:
        md.append(md_table(grp, name + " -- all-in"))
        md.append(md_table(grp, name + " -- solver only (proc.* excluded)", SOLVER))
if trunc:
    md.append("## Truncated (deadline-class) cells: informational shares, totals not asserted\n")
    for e in ("ipm", "sqp"):
        g = [c for c in trunc if c["engine"] == e]
        if g:
            md.append(md_table(g, f"{e} / truncated -- solver only", SOLVER))
# overlays
md += ["## Overlays (non-additive; share of each cell group's total)", "", "| group | alloc_leaf | iomp_leaf | fortran_leaf | unattributed |", "|---|---:|---:|---:|---:|"]
for name, grp in [("suite", full)] + [(e, [c for c in full if c["engine"] == e]) for e in ("ipm", "sqp")] + [(f"{e}/{s}", [c for c in full if c["engine"] == e and c["set"] == s]) for e in ("ipm", "sqp") for s in ("hs", "cutest")]:
    if not grp:
        continue
    d = sum(tot(c) for c in grp)
    f = lambda k: sum(c["overlays"].get(k, 0) for c in grp) / d
    un = sum(v for c in grp for k, v in c["components"].items() if k.startswith("unattributed")) / d
    md.append(f"| {name} | {100*f('alloc_leaf'):.2f}% | {100*f('iomp_leaf'):.2f}% | {100*f('fortran_leaf'):.2f}% | {100*un:.2f}% |")
# per cell csv
cols = ["cell", "lane", "set", "mode", "status", "iterations", "factorizations", "qp_minor_iters", "instructions_total", "est_over_total", "passB_status_matches"] + ALL
with open(outp + "-percell.csv", "w", newline="") as fh:
    w = csv.writer(fh); w.writerow(cols + ["instr_per_factorization", "instr_per_minor", "instr_per_major"])
    for c in sorted(cells, key=lambda c: (c["lane"], c["set"], c["problem"])):
        f = c.get("fields", {})
        fz = int(f.get("factorizations") or 0); mi = int(f.get("side.qp_minor_iters") or 0); mj = int(f.get("side.major_iters") or f.get("iterations") or 0)
        row = [c["cell"], c["lane"], c["set"], c.get("mode"), c.get("status"), c.get("iterations"), fz, mi, c["instructions_total"], f"{c['est_over_total']:.4f}", c["passB_status_matches"]]
        row += [c["components"].get(k, 0) for k in ALL]
        T = c["instructions_total"]
        row += [f"{T/fz:.0f}" if fz else "", f"{T/mi:.0f}" if mi else "", f"{T/mj:.0f}" if mj else ""]
        w.writerow(row)
# dominance
md += ["", "## Cells where a component dominates (>= 50% of the cell's solver-only total)", ""]
dom = collections.defaultdict(list)
for c in cells:
    d = tot(c, SOLVER)
    for k, v in c["components"].items():
        if SOLVER(k) and d and v / d >= 0.5:
            dom[k].append(f"{c['cell']} ({100*v/d:.0f}%{'*' if c.get('truncated') else ''})")
for k, v in sorted(dom.items()):
    md.append(f"- `{k}`: " + ", ".join(v))
md.append("(* = truncated cell, share only)")
# top leaf symbols over full cells (solver-only components)
leafs = collections.Counter(); lc = {}
for c in full:
    for s, d, comp, v in c["top_leaf"]:
        leafs[(s, d)] += v; lc[(s, d)] = comp
den = sum(tot(c) for c in full)
md += ["", "## Top leaf symbols, full cells (corpus; from each cell's top 60)", "", "| leaf symbol | dso | component | share |", "|---|---|---|---:|"]
for (s, d), v in leafs.most_common(40):
    md.append(f"| `{s[:90]}` | {d} | {lc[(s,d)]} | {100*v/den:.2f}% |")
fr = collections.Counter()
for c in full:
    for k, v in c["hven_frames_inclusive"]:
        fr[k] += v
md += ["", "## Top hven frames, inclusive (full cells; from each cell's top 80)", "", "| frame | inclusive share |", "|---|---:|"]
for k, v in fr.most_common(45):
    md.append(f"| `{k}` | {100*v/den:.2f}% |")
# who drives the backend
bc = collections.defaultdict(collections.Counter)
for c in full:
    for comp_, d in c.get("backend_callers", {}).items():
        for k, v in d.items():
            bc[(c["engine"], comp_)][k] += v
md += ["", "## Backend drivers: the first hven frame above the linear/KKT glue, by backend component (full cells, corpus)", ""]
for (e, comp_), cn in sorted(bc.items()):
    z = sum(c["components"].get(comp_, 0) for c in full if c["engine"] == e)
    if z <= 0 or z < 0.005 * den:
        continue
    md.append(f"### {e} / {comp_} (total {z:.4g})")
    md += ["", "| driver frame | share of this backend component |", "|---|---:|"]
    for k, v in cn.most_common(8):
        md.append(f"| `{k}` | {100*v/z:.1f}% |")
    md.append("")
lp = collections.defaultdict(collections.Counter)
for c in full:
    for a, b, v in c.get("leaf_by_hven_caller", []):
        lp[c["engine"]][(a, b)] += v
md += ["## Non-hven leaf symbols by nearest hven caller (full cells; from each cell's top 80 pairs)", ""]
for e, cn in sorted(lp.items()):
    z = sum(tot(c) for c in full if c["engine"] == e)
    md += [f"### {e}", "", "| leaf | nearest hven frame | share |", "|---|---|---:|"]
    for (a, b), v in cn.most_common(14):
        md.append(f"| `{a}` | `{b}` | {100*v/z:.2f}% |")
    md.append("")
md.append(f"Raw unattributed.truncated_unwind before redistribution (full cells): {100*sum(RAW_UNATTR[c['cell']] for c in full)/den:.2f}% of all-in total; unresolved after: {100*sum(c['components'].get('unattributed.unresolved',0) for c in full)/den:.2f}%.")
open(outp + ".md", "w").write("\n".join(md) + "\n")
print("wrote", outp + ".md", outp + "-percell.csv")
