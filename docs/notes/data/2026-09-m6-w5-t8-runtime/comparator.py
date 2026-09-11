#!/usr/bin/env python3
"""W5 T8.9r -- the group-1 runtime comparator.

Reproduces every number in reading.md from raw/ and perf/ alone. No network,
no state, no writes outside --out. Exits 0 when the whole reading is
reproducible, non-zero when it is not -- the caller worst-folds that status
into LEG_RC, so a comparator that cannot read its inputs fails the leg
instead of silently reporting an empty table.

THE RULES IT IMPLEMENTS are docs/notes/2026-09-m6-w5-t6-ownership.md 11.1/11.2:

  FLAT        per-cell median ratio in [0.99, 1.01] for every cell AND the
              corpus figure within +/-0.5 %.
  MOVED       corpus beyond +/-1 %, OR >= 3 cells outside [0.98, 1.02] with
              the same sign in all three alternating runs.
  UNRESOLVED  anything that is neither.

  LAYOUT-MOVED  instructions AND branches identical within 1e-4, with the
                cycle delta accounted for by the icache / branch-miss deltas.
  WORK-MOVED    instructions UP outside that identity band. THE VETO.

AGGREGATION (11.3), and it is not a mean of ratios: per cell, the MEDIAN of
the alternating runs per arm; the ratio of those two medians; the corpus
figure is the SUM of the per-cell medians, and the corpus ratio is the ratio
of the two sums.

THE VETO CHECK IS REPORTED IN BOTH READINGS, deliberately. 11.1's sentence
"a reproducible slowdown -- three solo runs, same sign -- on ANY cell" admits
a strict reading (any cell slower in all three paired rounds, however small)
and a banded one (a cell OUTSIDE 0.99-1.01, slower in all three). The strict
reading fires on sampling noise alone -- with 27 cells, ~3 fire by chance at
p=1/8 -- so this tool prints BOTH counts and leaves the disposition to the
settler and the owner, which is where 11.1.1 puts it anyway.
"""

import argparse
import csv
import math
import os
import re
import statistics
import sys

FLAT_CELL = (0.99, 1.01)
MOVED_CELL = (0.98, 1.02)
CORPUS_FLAT = 0.005
CORPUS_MOVED = 0.01
IDENTITY = 1.0e-4

PROBLEMS = []


def problem(msg):
    PROBLEMS.append(msg)


def read_rows(path):
    """Rows of one bench CSV, '#' provenance lines dropped, header keyed."""
    if not os.path.exists(path):
        problem("missing raw file: %s" % path)
        return []
    with open(path, newline="") as fh:
        lines = [ln for ln in fh if not ln.startswith("#")]
    if not lines:
        problem("empty raw file: %s" % path)
        return []
    return list(csv.DictReader(lines))


def cell_series(paths, key_col, val_col):
    """{cell: [value per run]} over the runs named by `paths`, in order."""
    series = {}
    for idx, path in enumerate(paths):
        rows = read_rows(path)
        for row in rows:
            key = row.get(key_col)
            raw = row.get(val_col)
            if key is None or raw is None:
                problem("%s: row without %s/%s" % (path, key_col, val_col))
                continue
            try:
                val = float(raw)
            except ValueError:
                problem("%s: %s=%r is not a number" % (path, val_col, raw))
                continue
            series.setdefault(key, []).append(val)
        if rows and len(set(len(v) for v in series.values())) > 1:
            problem("%s: run %d did not carry every cell of the earlier runs" % (path, idx + 1))
    return series


def score(base_paths, head_paths, key_col="cell_id", val_col="wall_s", label=""):
    base = cell_series(base_paths, key_col, val_col)
    head = cell_series(head_paths, key_col, val_col)
    keys = [k for k in base if k in head]
    only_base = sorted(set(base) - set(head))
    only_head = sorted(set(head) - set(base))
    cells = []
    for key in keys:
        b, h = base[key], head[key]
        if len(b) != len(h):
            problem("%s %s: %d base runs vs %d head runs" % (label, key, len(b), len(h)))
        bm, hm = statistics.median(b), statistics.median(h)
        ratio = hm / bm if bm else float("nan")
        # Same-sign across the alternating rounds: round k of head against
        # round k of base. Only defined when the two arms ran equally often.
        paired = [hv / bv for hv, bv in zip(h, b) if bv]
        slower_all = len(paired) == len(b) and all(p > 1.0 for p in paired)
        faster_all = len(paired) == len(b) and all(p < 1.0 for p in paired)
        cells.append(
            dict(cell=key, base_runs=b, head_runs=h, base_median=bm, head_median=hm,
                 ratio=ratio, paired=paired, slower_all=slower_all, faster_all=faster_all)
        )
    cells.sort(key=lambda c: c["cell"])
    base_corpus = sum(c["base_median"] for c in cells)
    head_corpus = sum(c["head_median"] for c in cells)
    corpus_ratio = head_corpus / base_corpus if base_corpus else float("nan")

    outside_flat = [c for c in cells if not (FLAT_CELL[0] <= c["ratio"] <= FLAT_CELL[1])]
    outside_moved_same_sign = [
        c for c in cells
        if not (MOVED_CELL[0] <= c["ratio"] <= MOVED_CELL[1])
        and (c["slower_all"] or c["faster_all"])
    ]
    corpus_flat = abs(corpus_ratio - 1.0) <= CORPUS_FLAT
    corpus_moved = abs(corpus_ratio - 1.0) > CORPUS_MOVED

    if corpus_moved or len(outside_moved_same_sign) >= 3:
        band = "MOVED"
    elif not outside_flat and corpus_flat:
        band = "FLAT"
    else:
        band = "UNRESOLVED"

    veto_strict = [c["cell"] for c in cells if c["slower_all"]]
    veto_banded = [c["cell"] for c in cells if c["slower_all"] and c["ratio"] > FLAT_CELL[1]]

    return dict(label=label, cells=cells, base_corpus=base_corpus, head_corpus=head_corpus,
                corpus_ratio=corpus_ratio, band=band, outside_flat=outside_flat,
                outside_moved_same_sign=outside_moved_same_sign,
                veto_strict=veto_strict, veto_banded=veto_banded,
                only_base=only_base, only_head=only_head)


PERF_RE = re.compile(r"^\s*([0-9][0-9,]*|<not counted>|<not supported>)\s+([A-Za-z0-9_.:\-]+)\s*$")


def read_perf(path):
    """{event: count} from one `perf stat` capture."""
    if not os.path.exists(path):
        problem("missing perf file: %s" % path)
        return {}
    out = {}
    with open(path) as fh:
        for line in fh:
            m = PERF_RE.match(line.rstrip())
            if not m:
                continue
            val, ev = m.group(1), m.group(2)
            if val.startswith("<"):
                problem("%s: event %s was %s -- multiplexed or unsupported" % (path, ev, val))
                continue
            out[ev] = int(val.replace(",", ""))
    if not out:
        problem("%s: no perf events parsed" % path)
    return out


def perf_medians(paths):
    per_event = {}
    for path in paths:
        for ev, val in read_perf(path).items():
            per_event.setdefault(ev, []).append(val)
    return {ev: statistics.median(vals) for ev, vals in per_event.items()}, per_event


def classify_perf(base_paths, head_paths):
    bmed, braw = perf_medians(base_paths)
    hmed, hraw = perf_medians(head_paths)
    ratios = {}
    for ev in sorted(set(bmed) & set(hmed)):
        ratios[ev] = hmed[ev] / bmed[ev] if bmed[ev] else float("nan")
    verdict = "UNCLASSIFIED"
    ins = ratios.get("instructions:u")
    bra = ratios.get("branches:u")
    if ins is not None:
        identical = abs(ins - 1.0) <= IDENTITY and (bra is None or abs(bra - 1.0) <= IDENTITY)
        if ins > 1.0 + IDENTITY:
            verdict = "WORK-MOVED (instructions UP) -- THE VETO"
        elif ins < 1.0 - IDENTITY:
            verdict = "INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1)"
        elif identical:
            verdict = "instructions+branches inside the 1e-4 identity band"
        else:
            verdict = "instructions inside the band, branches outside -- UNRESOLVED"
    return dict(ratios=ratios, base=bmed, head=hmed, base_raw=braw, head_raw=hraw,
                verdict=verdict)


def instrument_floor(files):
    """The worst |passA/passB - 1| the SAME arm shows on `instructions:u`.

    This is the leg's own noise floor. 11.2's LAYOUT/WORK rule turns on a 1e-4
    identity band; a leg whose two passes of ONE binary disagree by more than
    that cannot answer the question the band asks, and saying so is the
    difference between a measurement and a coin flip. (T6.d finding F1 recorded
    exactly this on the HS leg: `instructions:u` is bimodal per process at
    0.16-0.22 %.)
    """
    worst = 0.0
    for arm in ("base", "head"):
        fa = [f for f in files if os.path.basename(f).startswith("passA-" + arm)]
        fb = [f for f in files if os.path.basename(f).startswith("passB-" + arm)]
        if not fa or not fb:
            continue
        ma, _ = perf_medians(fa)
        mb, _ = perf_medians(fb)
        if ma.get("instructions:u") and mb.get("instructions:u"):
            worst = max(worst, abs(ma["instructions:u"] / mb["instructions:u"] - 1.0))
    return worst


def apply_floor(perf, files):
    """Gate a perf verdict on the leg's own noise floor."""
    floor = instrument_floor(files)
    perf["floor"] = floor
    if floor > IDENTITY:
        perf["verdict"] = ("NO VERDICT AT 1e-4 -- this leg's own two passes of the SAME binary "
                           "disagree by %.4f %%, above the identity band; the ratio below is "
                           "reported, not classified (original label: %s)"
                           % (100.0 * floor, perf["verdict"]))
    return perf


def fmt_cells(res, out):
    out.write("| cell | base median (s) | head median (s) | ratio | paired per round |\n")
    out.write("|---|---|---|---|---|\n")
    for c in res["cells"]:
        out.write("| `%s` | %.6f | %.6f | %.4f | %s |\n" % (
            c["cell"], c["base_median"], c["head_median"], c["ratio"],
            " ".join("%.4f" % p for p in c["paired"])))


def emit(res, out, perf=None):
    out.write("\n### %s\n\n" % res["label"])
    if res["only_base"]:
        out.write("Keys only in the BASE arm (no band): %s\n\n" % ", ".join("`%s`" % k for k in res["only_base"]))
    if res["only_head"]:
        out.write("Keys only in the HEAD arm (head-only, informational, no band): %s\n\n"
                  % ", ".join("`%s`" % k for k in res["only_head"]))
    out.write("corpus base %.4f s -> head %.4f s, **ratio %.4f** (%+.3f %%); cells compared %d; "
              "outside 0.99-1.01: **%d**; band **%s**\n\n" % (
                  res["base_corpus"], res["head_corpus"], res["corpus_ratio"],
                  100.0 * (res["corpus_ratio"] - 1.0), len(res["cells"]),
                  len(res["outside_flat"]), res["band"]))
    out.write("veto check -- cells slower in ALL alternating rounds: strict reading %d %s; "
              "banded reading (also outside 0.99-1.01) %d %s\n\n" % (
                  len(res["veto_strict"]), res["veto_strict"] or "",
                  len(res["veto_banded"]), res["veto_banded"] or ""))
    fmt_cells(res, out)
    if perf:
        out.write("\nperf: %s\n\n" % perf["verdict"])
        out.write("| event | base median | head median | ratio |\n|---|---|---|---|\n")
        for ev in sorted(perf["ratios"]):
            out.write("| `%s` | %d | %d | %.5f |\n" % (ev, perf["base"][ev], perf["head"][ev],
                                                       perf["ratios"][ev]))


def runs(root, *parts, **kw):
    """The measurement files in one directory, in a stable order.

    `ext` is not cosmetic: the leg scripts park a `.stdout` transcript beside
    every `.csv`, and a substring match on the arm name would otherwise pick
    the transcript up as if it were data.
    """
    ext = kw.pop("ext", ".csv")
    d = os.path.join(root, *parts)
    if not os.path.isdir(d):
        problem("missing directory: %s" % d)
        return []
    return [os.path.join(d, f) for f in sorted(os.listdir(d)) if f.endswith(ext)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True, help="the artifact directory")
    ap.add_argument("--out", default="-", help="where the reading tables go")
    args = ap.parse_args()
    root = args.root
    out = sys.stdout if args.out == "-" else open(args.out, "w")

    out.write("# W5 T8.9r comparator output\n")
    out.write("\nRegenerate: `python3 comparator.py --root . --out -`\n")

    results = {}

    # ---- leg 1: the U0 27-cell corpus, three SQP modes -------------------
    out.write("\n## Leg 1 -- the U0 27-cell corpus (102f729 -> e51a7e0)\n")
    for mode in ("ipm", "ssn", "walk"):
        b = [p for p in runs(root, "raw", "leg1", mode) if "/base-" in p]
        h = [p for p in runs(root, "raw", "leg1", mode) if "/head-" in p]
        res = score(b, h, label="leg 1 / %s" % mode)
        emit(res, out)
        # THE PERF CELLS ARE SCORED ONE AT A TIME. Taking a median across the
        # three designated cells would average a 1,000-node solve into a
        # 20,000-node one and call the result an instruction count.
        perf_files = runs(root, "perf", "leg1", mode, ext=".txt")
        cells = sorted(set(
            os.path.basename(f).split("-", 2)[2].rsplit("-r", 1)[0] for f in perf_files))
        pa_all = {}
        for cell in cells:
            for pass_name, label in (("passA", "pass A"), ("passB", "pass B (Zen 3 front end)")):
                cell_files = [f for f in perf_files
                              if ("-" + cell + "-r") in os.path.basename(f)]
                pb = apply_floor(classify_perf(
                    [f for f in cell_files if os.path.basename(f).startswith(pass_name + "-base-" + cell + "-r")],
                    [f for f in cell_files if os.path.basename(f).startswith(pass_name + "-head-" + cell + "-r")]),
                    cell_files)
                out.write("\n%s, cell `%s`: %s\n\n" % (label, cell, pb["verdict"]))
                out.write("| event | base median | head median | ratio |\n|---|---|---|---|\n")
                for ev in sorted(pb["ratios"]):
                    out.write("| `%s` | %d | %d | %.5f |\n" % (
                        ev, pb["base"][ev], pb["head"][ev], pb["ratios"][ev]))
                if pass_name == "passA":
                    pa_all[cell] = pb
                else:
                    for name, d in (("base", pb["base"]), ("head", pb["head"])):
                        line = []
                        if d.get("cycles:u"):
                            line.append("front-end-bound (dq-empty/cycles) = %.4f"
                                        % (d.get("de_dis_uop_queue_empty_di0:u", 0) / d["cycles:u"]))
                        hit = d.get("op_cache_hit_miss.op_cache_hit:u")
                        miss = d.get("op_cache_hit_miss.op_cache_miss:u")
                        if hit is not None and miss is not None and (hit + miss):
                            line.append("op-cache miss share = %.4f" % (miss / (hit + miss)))
                        if line:
                            out.write("%s arm: %s\n" % (name, "; ".join(line)))
        # The mode's pass-A verdict is the WORST of its cells': one cell with
        # instructions up is instructions up.
        order = {"WORK-MOVED (instructions UP) -- THE VETO": 3,
                 "instructions inside the band, branches outside -- UNRESOLVED": 2,
                 "INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1)": 1}
        worst = None
        for cell, pb in pa_all.items():
            if worst is None or order.get(pb["verdict"], 0) > order.get(worst["verdict"], 0):
                worst = pb
        results["leg1/%s" % mode] = (res, worst, None)

    # ---- leg 2: the 27 HS problems --------------------------------------
    out.write("\n## Leg 2 -- the 27 Hock-Schittkowski problems, --repeat N\n")
    leg2root = os.path.join(root, "raw", "leg2")
    leg2_perf_files = []
    if os.path.isdir(leg2root):
        for mode in sorted(os.listdir(leg2root)):
            for trace in sorted(os.listdir(os.path.join(leg2root, mode))):
                leg2_perf_files.extend(runs(root, "perf", "leg2", mode, trace, ext=".txt"))
    if os.path.isdir(leg2root):
        for mode in sorted(os.listdir(leg2root)):
            for trace in sorted(os.listdir(os.path.join(leg2root, mode))):
                b = [p for p in runs(root, "raw", "leg2", mode, trace) if "/base-" in p]
                h = [p for p in runs(root, "raw", "leg2", mode, trace) if "/head-" in p]
                res = score(b, h, key_col="hs", val_col="wall_median_s",
                            label="leg 2 / %s / trace=%s" % (mode, trace))
                # THE FLOOR IS A PROPERTY OF THE LEG, NOT OF ONE ARM PAIRING.
                # The HS population is bimodal per process (T6.d F1), so a
                # single mode/trace combination whose two passes happen to land
                # in the same cluster is a coincidence of clustering, not a
                # sound instrument. The floor used here is the WORST any leg-2
                # combination shows.
                pa = apply_floor(classify_perf(
                    [p for p in runs(root, "perf", "leg2", mode, trace, ext=".txt") if "passA-base" in p],
                    [p for p in runs(root, "perf", "leg2", mode, trace, ext=".txt") if "passA-head" in p]),
                    leg2_perf_files)
                emit(res, out, pa)
                pbb = [p for p in runs(root, "perf", "leg2", mode, trace, ext=".txt") if "passB-base" in p]
                pbh = [p for p in runs(root, "perf", "leg2", mode, trace, ext=".txt") if "passB-head" in p]
                if pbb and pbh:
                    pb = classify_perf(pbb, pbh)
                    out.write("\npass B: %s\n\n" % pb["verdict"])
                    out.write("| event | base median | head median | ratio |\n|---|---|---|---|\n")
                    for ev in sorted(pb["ratios"]):
                        out.write("| `%s` | %d | %d | %.5f |\n" % (
                            ev, pb["base"][ev], pb["head"][ev], pb["ratios"][ev]))
                results["leg2/%s/%s" % (mode, trace)] = (res, pa, None)
    else:
        problem("missing directory: %s" % leg2root)

    # ---- the interior leg ------------------------------------------------
    out.write("\n## The interior leg (b9848bf -> e51a7e0)\n")
    b = [p for p in runs(root, "raw", "interior") if "/base-" in p]
    h = [p for p in runs(root, "raw", "interior") if "/head-" in p]
    all_res = score(b, h, label="interior / ALL comparable rows")
    # The F7 rows carry the band (leg-1 rules); the millisecond-scale rows do
    # not (A7 (ii)) and are listed with their ratios for the record only.
    def is_f7(cell):
        return cell.startswith("f7_")
    f7_b = b
    f7_h = h
    res_f7 = score(f7_b, f7_h, label="interior / F7 rows (banded, leg-1 rules)")
    res_f7["cells"] = [c for c in res_f7["cells"] if is_f7(c["cell"])]
    res_ms = dict(res_f7)
    # recompute the F7 aggregate over the filtered set
    res_f7["base_corpus"] = sum(c["base_median"] for c in res_f7["cells"])
    res_f7["head_corpus"] = sum(c["head_median"] for c in res_f7["cells"])
    res_f7["corpus_ratio"] = (res_f7["head_corpus"] / res_f7["base_corpus"]
                              if res_f7["base_corpus"] else float("nan"))
    res_f7["outside_flat"] = [c for c in res_f7["cells"]
                              if not (FLAT_CELL[0] <= c["ratio"] <= FLAT_CELL[1])]
    res_f7["outside_moved_same_sign"] = [
        c for c in res_f7["cells"]
        if not (MOVED_CELL[0] <= c["ratio"] <= MOVED_CELL[1]) and (c["slower_all"] or c["faster_all"])]
    if abs(res_f7["corpus_ratio"] - 1.0) > CORPUS_MOVED or len(res_f7["outside_moved_same_sign"]) >= 3:
        res_f7["band"] = "MOVED"
    elif not res_f7["outside_flat"] and abs(res_f7["corpus_ratio"] - 1.0) <= CORPUS_FLAT:
        res_f7["band"] = "FLAT"
    else:
        res_f7["band"] = "UNRESOLVED"
    res_f7["veto_strict"] = [c["cell"] for c in res_f7["cells"] if c["slower_all"]]
    res_f7["veto_banded"] = [c["cell"] for c in res_f7["cells"]
                             if c["slower_all"] and c["ratio"] > FLAT_CELL[1]]
    pa = apply_floor(
        classify_perf([p for p in runs(root, "perf", "interior", ext=".txt") if "passA-base" in p],
                      [p for p in runs(root, "perf", "interior", ext=".txt") if "passA-head" in p]),
        runs(root, "perf", "interior", ext=".txt"))
    pa["verdict"] += (" -- AND the row counts are NOT matched: the head arm's leg runs 43 rows, "
                      "the base arm's 41")
    emit(res_f7, out, pa)
    pbb = [p for p in runs(root, "perf", "interior", ext=".txt") if "passB-base" in p]
    pbh = [p for p in runs(root, "perf", "interior", ext=".txt") if "passB-head" in p]
    if pbb and pbh:
        pb = classify_perf(pbb, pbh)
        out.write("\npass B: %s\n\n" % pb["verdict"])
        out.write("| event | base median | head median | ratio |\n|---|---|---|---|\n")
        for ev in sorted(pb["ratios"]):
            out.write("| `%s` | %d | %d | %.5f |\n" % (ev, pb["base"][ev], pb["head"][ev],
                                                       pb["ratios"][ev]))
    # THE FIRST ROW OF THE PROCESS IS EXCLUDED FROM THE BAND, DECLARED.
    # The interior leg runs in process and writes its rows in order; the first
    # solve pays MKL's first-call initialisation, the allocator's first growth
    # and the page faults for the whole working set. The BASE arm's own three
    # runs of that row span 0.3168-0.9986 s -- a factor of 3.2 on ONE binary --
    # while its sibling row two lines below is stable to 0.3 %. A quantity whose
    # own arm disagrees with itself by 3x cannot carry a 1 % band, so it is
    # reported in full, separately, and left out of the corpus figure. Nothing
    # else is excluded.
    FIRST_ROW = "f7_n1000_bound_neutral/MakeParameter"
    first = [c for c in res_f7["cells"] if c["cell"] == FIRST_ROW]
    kept = [c for c in res_f7["cells"] if c["cell"] != FIRST_ROW]
    if first:
        c = first[0]
        out.write("\n#### The excluded first row -- `%s`\n\n" % FIRST_ROW)
        out.write("base runs %s; head runs %s. The base arm's own three runs span %.3fx. "
                  "It is the FIRST solve of the process and carries the one-time "
                  "initialisation of everything below it; the band is computed WITHOUT it "
                  "and this row is reported here instead.\n\n" % (
                      ", ".join("%.6f" % v for v in c["base_runs"]),
                      ", ".join("%.6f" % v for v in c["head_runs"]),
                      max(c["base_runs"]) / min(c["base_runs"])))
        bc = sum(x["base_median"] for x in kept)
        hc = sum(x["head_median"] for x in kept)
        out.write("Corpus WITH that row: base %.4f -> head %.4f, ratio **%.4f**.\n"
                  % (res_f7["base_corpus"], res_f7["head_corpus"], res_f7["corpus_ratio"]))
        out.write("Corpus WITHOUT it (%d rows): base %.4f -> head %.4f, ratio **%.4f** (%+.3f %%); "
                  "cells outside 0.99-1.01: **%d**; band **%s**.\n\n" % (
                      len(kept), bc, hc, hc / bc, 100.0 * (hc / bc - 1.0),
                      len([x for x in kept
                           if not (FLAT_CELL[0] <= x["ratio"] <= FLAT_CELL[1])]),
                      "FLAT" if (abs(hc / bc - 1.0) <= CORPUS_FLAT and not [
                          x for x in kept
                          if not (FLAT_CELL[0] <= x["ratio"] <= FLAT_CELL[1])])
                      else ("MOVED" if abs(hc / bc - 1.0) > CORPUS_MOVED else "UNRESOLVED")))
        res_f7 = dict(res_f7)
        res_f7["cells"] = kept
        res_f7["base_corpus"] = bc
        res_f7["head_corpus"] = hc
        res_f7["corpus_ratio"] = hc / bc
        res_f7["outside_flat"] = [x for x in kept
                                  if not (FLAT_CELL[0] <= x["ratio"] <= FLAT_CELL[1])]
        res_f7["outside_moved_same_sign"] = [
            x for x in kept
            if not (MOVED_CELL[0] <= x["ratio"] <= MOVED_CELL[1])
            and (x["slower_all"] or x["faster_all"])]
        if abs(res_f7["corpus_ratio"] - 1.0) > CORPUS_MOVED or len(res_f7["outside_moved_same_sign"]) >= 3:
            res_f7["band"] = "MOVED"
        elif not res_f7["outside_flat"] and abs(res_f7["corpus_ratio"] - 1.0) <= CORPUS_FLAT:
            res_f7["band"] = "FLAT"
        else:
            res_f7["band"] = "UNRESOLVED"
        res_f7["veto_strict"] = [x["cell"] for x in kept if x["slower_all"]]
        res_f7["veto_banded"] = [x["cell"] for x in kept
                                 if x["slower_all"] and x["ratio"] > FLAT_CELL[1]]

    ms_cells = [c for c in all_res["cells"] if not is_f7(c["cell"])]

    out.write("\n#### The millisecond-scale interior rows -- NO BAND (A7 (ii))\n\n")
    out.write("HS071 and the infeasible rows run in milliseconds and `--repeat` is HS-only "
              "(`bench_corpus.cpp`), so these carry NO wall band; their ratios are recorded, "
              "and the leg's instruction verdict above is what speaks for them.\n\n")
    out.write("| row | base median (s) | head median (s) | ratio |\n|---|---|---|---|\n")
    for c in ms_cells:
        out.write("| `%s` | %.6f | %.6f | %.4f |\n" % (
            c["cell"], c["base_median"], c["head_median"], c["ratio"]))
    if all_res["only_head"]:
        out.write("\nHEAD-ONLY rows (no base arm, informational): %s\n"
                  % ", ".join("`%s`" % k for k in all_res["only_head"]))
    if all_res["only_base"]:
        out.write("\nBASE-ONLY rows: %s\n" % ", ".join("`%s`" % k for k in all_res["only_base"]))
    results["interior"] = (res_f7, pa, None)

    # ---- the counter identity, recomputed here -------------------------
    # The wall band says the two arms take the same TIME. This says they do the
    # same WORK: every column of every leg-1 row, `wall_s` excluded, compared
    # arm against arm in every round. It is what turns the instruction finding
    # into a statement about how the work is executed rather than how much of
    # it there is, so the reading should not have to take it on trust from
    # another task's replay.
    out.write("\n## Counter identity, leg 1 (recomputed from raw/)\n\n")
    out.write("| mode | cells | columns compared | rounds | differing columns |\n")
    out.write("|---|---|---|---|---|\n")
    for mode in ("ipm", "ssn", "walk"):
        files = runs(root, "raw", "leg1", mode)
        diffs = {}
        ncells = ncols = nrounds = 0
        for tag in sorted(set(os.path.basename(f).split("-")[1] for f in files)):
            bf = [f for f in files if os.path.basename(f) == "base-" + tag]
            hf = [f for f in files if os.path.basename(f) == "head-" + tag]
            if not bf or not hf:
                continue
            nrounds += 1
            brows = {r["cell_id"]: r for r in read_rows(bf[0])}
            hrows = {r["cell_id"]: r for r in read_rows(hf[0])}
            ncells = len(brows)
            for cid, b in brows.items():
                h = hrows.get(cid)
                if h is None:
                    diffs.setdefault("<missing row>", set()).add(cid)
                    continue
                ncols = len(b) - 1
                for k in b:
                    if k == "wall_s":
                        continue
                    if b[k] != h[k]:
                        diffs.setdefault(k, set()).add(cid)
        out.write("| %s | %d | %d | %d | %s |\n" % (
            mode, ncells, ncols, nrounds,
            ", ".join("`%s` (%d cells)" % (k, len(v)) for k, v in sorted(diffs.items()))
            or "**NONE -- byte-identical**"))

    # ---- the instrument's own checks -------------------------------------
    # Two things can make an instruction ratio mean something other than what
    # it says, and both are measured here rather than argued.
    out.write("\n## Instrument checks\n")

    # (1) PASS A vs PASS B ON THE SAME ARM. `instructions` and `cycles` appear
    # in both passes precisely so the two can be tied together. When the same
    # arm's two passes disagree by more than the 1e-4 identity band, the band
    # is BELOW that leg's noise floor and no instruction verdict at 1e-4 is
    # available from it. (T6.d finding F1: HS `instructions:u` is bimodal per
    # process at 0.16-0.22 %.)
    out.write("\nThe same arm, measured twice -- pass A against pass B. A leg whose own two "
              "passes disagree by more than 1e-4 cannot carry a 1e-4 instruction verdict.\n\n")
    out.write("| leg | arm | pass A instructions | pass B instructions | A/B |\n|---|---|---|---|---|\n")
    def selfcheck(label, adir, bnames):
        for arm in ("base", "head"):
            fa = [f for f in adir if os.path.basename(f).startswith("passA-" + arm)]
            fb = [f for f in adir if os.path.basename(f).startswith("passB-" + arm)]
            if not fa or not fb:
                continue
            ma, _ = perf_medians(fa)
            mb, _ = perf_medians(fb)
            if "instructions:u" in ma and "instructions:u" in mb and mb["instructions:u"]:
                out.write("| %s | %s | %d | %d | %.5f |\n" % (
                    label, arm, ma["instructions:u"], mb["instructions:u"],
                    ma["instructions:u"] / mb["instructions:u"]))
    for mode in ("ipm", "ssn", "walk"):
        files = runs(root, "perf", "leg1", mode, ext=".txt")
        for cell in sorted(set(os.path.basename(f).split("-", 2)[2].rsplit("-r", 1)[0]
                               for f in files)):
            selfcheck("leg 1 / %s / %s" % (mode, cell),
                      [f for f in files if ("-" + cell + "-r") in os.path.basename(f)], None)
    leg2root = os.path.join(root, "raw", "leg2")
    if os.path.isdir(leg2root):
        for mode in sorted(os.listdir(leg2root)):
            for trace in sorted(os.listdir(os.path.join(leg2root, mode))):
                selfcheck("leg 2 / %s / %s" % (mode, trace),
                          runs(root, "perf", "leg2", mode, trace, ext=".txt"), None)
    selfcheck("interior", runs(root, "perf", "interior", ext=".txt"), None)

    # (2) THE INTERIOR LEG'S ROW COUNTS ARE NOT MATCHED. The head arm's leg
    # runs 43 rows and the base arm's 41: the two `parts2` rows exist only at
    # the head, and no flag suppresses them. `perf stat` counts the PROCESS, so
    # the interior instruction ratio charges the head with two solves the base
    # never ran. The wall share of those two rows is printed beside it -- an
    # instruction increase of about that size is the extra rows, not the code.
    out.write("\n### The interior leg's instruction comparison is NOT like-for-like\n\n")
    tot_h = tot_p2 = 0.0
    for f in [f for f in runs(root, "raw", "interior") if "/head-" in f]:
        rows = read_rows(f)
        tot_h += sum(float(r["wall_s"]) for r in rows)
        tot_p2 += sum(float(r["wall_s"]) for r in rows if r["cell_id"].endswith("/parts2"))
    if tot_h:
        out.write("The head arm's leg runs 43 rows, the base arm's 41 -- the two `parts2` rows "
                  "exist only at the head and no flag suppresses them. They are **%.2f %%** of "
                  "the head arm's measured wall. `perf stat` counts the whole process, so the "
                  "interior instruction ratio above charges the head with two solves the base "
                  "never ran, and an increase of that order is those rows rather than the "
                  "code. The interior leg therefore carries NO instruction verdict at 1e-4.\n"
                  % (100.0 * tot_p2 / tot_h))

    # (3) THE ATTRIBUTION PROBE. `--dump-qp` on a kNeutral cell runs program
    # start, model construction, the first QP build and the dump write, and
    # SOLVES NOTHING -- so its delta bounds the share of leg 1's instruction
    # delta that lies OUTSIDE the solve.
    adir = os.path.join(root, "perf", "attribution")
    if os.path.isdir(adir):
        out.write("\n### Attribution probe -- where leg 1's extra instructions are\n\n")
        out.write("`--dump-qp` on a `kNeutral` cell runs program start, model construction, the "
                  "cell's FIRST QP build and the dump write, and solves nothing. Its delta is "
                  "the NON-SOLVE share of leg 1's delta.\n\n")
        out.write("| probe cell | base instructions | head instructions | delta | ratio |\n")
        out.write("|---|---|---|---|---|\n")
        files = [os.path.join(adir, f) for f in sorted(os.listdir(adir)) if f.endswith(".txt")]
        for cell in sorted(set(f.split("dumpqp-")[1].split("-", 1)[1].rsplit("-r", 1)[0]
                               for f in (os.path.basename(x) for x in files))):
            vals = {}
            for arm in ("base", "head"):
                m, _ = perf_medians([f for f in files
                                     if os.path.basename(f).startswith("dumpqp-%s-%s-r" % (arm, cell))])
                vals[arm] = m.get("instructions:u")
            if vals["base"] and vals["head"]:
                out.write("| `%s` | %d | %d | %+d | %.5f |\n" % (
                    cell, vals["base"], vals["head"], vals["head"] - vals["base"],
                    vals["head"] / vals["base"]))

    # ---- the summary and the composed chain ------------------------------
    out.write("\n## Summary\n\n")
    out.write("| leg | corpus ratio | cells outside 0.99-1.01 | band | perf verdict |\n")
    out.write("|---|---|---|---|---|\n")
    for name in sorted(results):
        res, pa, _ = results[name]
        out.write("| %s | %.4f | %d/%d | %s | %s |\n" % (
            name, res["corpus_ratio"], len(res["outside_flat"]), len(res["cells"]),
            res["band"], pa["verdict"] if pa else "n/a"))

    if PROBLEMS:
        out.write("\n## PROBLEMS (%d) -- this reading is NOT complete\n\n" % len(PROBLEMS))
        for p in PROBLEMS:
            out.write("- %s\n" % p)
    if out is not sys.stdout:
        out.close()
    if PROBLEMS:
        for p in PROBLEMS:
            sys.stderr.write("comparator: %s\n" % p)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
