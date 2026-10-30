#!/usr/bin/env python3
"""A3 TRAIN close (design page s5.3-5.4): pure file processing, no solver.

1. MERGE each (label, set, run)'s one-cell runner outputs into ONE results.csv + counters file,
   refusing a missing cell, a header that differs, or a harness/pin sha that differs.
2. LAY OUT the A4 rule's tree <root>/hven-sqp/<slot>/<set>-run-<R>/: slot `default` holds the
   `sqp-ipm` rows (D, the OFF state of every A3 lever); every A3 label keeps its own name.
3. DETERMINISM: schema/compare.py run A vs run B per label and set.
4. DECIDE: the harness's scripts/w2_a4_decide.py, unmodified, on the tree (run A scored).
5. C-1 (gate): the base clone's sqp-ipm run A vs the head clone's sqp-ipm run A, cell for cell.
   C-2 (descriptive): the base clone's sqp-ipm run A vs A2's committed old-toolchain sqp-ipm run A.

Usage: train_close.py CELLS_ROOT CELLS_BASE_ROOT OUT_ROOT BENCH_CLONE A2_TREE
"""
import csv
import json
import os
import subprocess
import sys

LABELS = ["sqp-ipm", "sqp-ipm-slack1", "sqp-ipm-slack1e3", "sqp-ipm-skip1", "sqp-ipm-skipnever"]
SETS = ("hs", "cutest")


def roster(bench, set_file):
    d = json.load(open(os.path.join(bench, "sets", set_file)))
    return sorted(p["problem"] for p in d["problems"] if p["split"] == "TRAIN")


def split(path):
    head, rows = [], []
    for line in open(path):
        (head if line.startswith("#") else rows).append(line)
    return head, rows


def stamp(head, key):
    for line in head:
        if line.startswith("# {}:".format(key)):
            return line.split(":", 1)[1].strip()
    return None


def merge(cells, label, s, run, problems, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for fname in ("results.csv", "counters-hven-sqp.csv"):
        header, body, head0, shas = None, [], None, set()
        for p in problems:
            d = os.path.join(cells, "hven-sqp", label, "{}-run-{}-{}".format(s, run, p))
            path = os.path.join(d, fname)
            if not os.path.exists(path):
                if fname == "results.csv":
                    raise SystemExit("REFUSED: missing {}".format(path))
                continue  # a dnf_wall cell writes no side file
            head, rows = split(path)
            if head0 is None:
                head0 = head
            shas.add((stamp(head, "harness.sha"), stamp(head, "hven.pin.sha")))
            if header is None:
                header = rows[0]
            elif rows[0] != header:
                raise SystemExit("REFUSED: header differs at {}".format(path))
            if len(rows) != 2:
                raise SystemExit("REFUSED: {} data rows in {}".format(len(rows) - 1, path))
            body.append(rows[1])
        if len(shas) > 1:
            raise SystemExit("REFUSED: shas differ across cells of {} {} {}: {}".format(
                label, s, run, shas))
        if header is None:
            continue
        with open(os.path.join(out_dir, fname), "w") as f:
            f.writelines(head0)
            f.write("# merged: {} one-cell runs of label {} set {} run {} (A3 train_close.py)\n"
                    .format(len(problems), label, s, run))
            f.write(header)
            f.writelines(body)


def rows_of(path):
    """Keyed by problem: results.csv carries `problem`; the side file's `cell_id` is
    `<set>/<problem>/<lane>/<label>`."""
    if not os.path.exists(path):
        return {}
    out = {}
    for r in csv.DictReader(l for l in open(path) if not l.startswith("#")):
        out[r["problem"] if "problem" in r else r["cell_id"].split("/")[1]] = r
    return out


def compare_cells(mine_dir, ref_dir, problems, cmp_majors):
    mine = rows_of(os.path.join(mine_dir, "results.csv"))
    ref = rows_of(os.path.join(ref_dir, "results.csv"))
    side_m = rows_of(os.path.join(mine_dir, "counters-hven-sqp.csv"))
    side_r = rows_of(os.path.join(ref_dir, "counters-hven-sqp.csv"))
    lines, nd = [], 0
    for p in problems:
        m, r = mine.get(p), ref.get(p)
        if m is None or r is None:
            lines.append("{}: MISSING mine={} ref={}".format(p, m is not None, r is not None))
            nd += 1
            continue
        diffs = []
        if m["outcome"] != r["outcome"]:
            walls = "dnf_wall" in (m["outcome"], r["outcome"])
            diffs.append("outcome {} -> {}{}".format(r["outcome"], m["outcome"],
                                                    " (WALL EVENT)" if walls else ""))
        else:
            for k in ("iterations", "factorizations", "objective"):
                if m[k] != r[k] and m[k] != "null" and r[k] != "null":
                    diffs.append("{} {} -> {}".format(k, r[k], m[k]))
            if cmp_majors:
                sm, sr = side_m.get(p, {}), side_r.get(p, {})
                k = "major_iters"
                if k in sm and k in sr and sm[k] != sr[k]:
                    diffs.append("{} {} -> {}".format(k, sr[k], sm[k]))
        if diffs:
            nd += 1
        lines.append("{}: {}".format(p, "; ".join(diffs) or "identical"))
    return nd, lines


def main(argv):
    cells, cells_base, out, bench, a2_tree = argv[1:6]
    sets = {"hs": roster(bench, "hs-27.json"), "cutest": roster(bench, "cutest-nlp.json")}
    tree = os.path.join(out, "tree")
    report = []
    for label in LABELS:
        slot = "default" if label == "sqp-ipm" else label
        for s in SETS:
            for run in ("A", "B"):
                merge(cells, label, s, run, sets[s],
                      os.path.join(tree, "hven-sqp", slot, "{}-run-{}".format(s, run)))
            a = os.path.join(tree, "hven-sqp", slot, "{}-run-A".format(s), "results.csv")
            b = os.path.join(tree, "hven-sqp", slot, "{}-run-B".format(s), "results.csv")
            res = subprocess.run(["python3", "-B", os.path.join(bench, "schema", "compare.py"), a, b],
                                 capture_output=True, text=True)
            with open(os.path.join(out, "determinism-{}-{}.txt".format(label, s)), "w") as f:
                f.write(res.stdout + res.stderr)
            last = (res.stdout.strip().splitlines() or ["(no output)"])[-1]
            report.append("determinism {} {}: rc={} :: {}".format(label, s, res.returncode, last))
    res = subprocess.run(["python3", "-B", os.path.join(bench, "scripts", "w2_a4_decide.py"), tree,
                          "--json", os.path.join(out, "decisions.json")],
                         capture_output=True, text=True)
    with open(os.path.join(out, "decisions.txt"), "w") as f:
        f.write(res.stdout + res.stderr)
    report.append("decide rc={}".format(res.returncode))
    # C-1 (gate) and C-2 (descriptive).
    ctrl = os.path.join(out, "controls")
    for s in SETS:
        d = os.path.join(ctrl, "base-sqp-ipm", "{}-run-A".format(s))
        merge(cells_base, "sqp-ipm", s, "A", sets[s], d)
        head_d = os.path.join(tree, "hven-sqp", "default", "{}-run-A".format(s))
        nd, lines = compare_cells(head_d, d, sets[s], True)
        with open(os.path.join(out, "c1-head-vs-base-sqp-ipm-{}.txt".format(s)), "w") as f:
            f.write("\n".join(lines) + "\n")
        report.append("C-1 head vs base sqp-ipm {}: {} cell(s) differing of {}".format(s, nd, len(sets[s])))
        a2_d = os.path.join(a2_tree, "hven-sqp", "default", "{}-run-A".format(s))
        nd2, lines2 = compare_cells(d, a2_d, sets[s], False)
        with open(os.path.join(out, "c2-base-new-vs-a2-old-sqp-ipm-{}.txt".format(s)), "w") as f:
            f.write("\n".join(lines2) + "\n")
        report.append("C-2 (descriptive, toolchain) base new vs A2 old sqp-ipm {}: {} cell(s) differing of {}"
                      .format(s, nd2, len(sets[s])))
    with open(os.path.join(out, "close-summary.txt"), "w") as f:
        f.write("\n".join(report) + "\n")
    print("\n".join(report))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
