#!/usr/bin/env python3
"""A2 TRAIN close (design page s6.4): pure file processing, no solver.

1. MERGE each (label, set, run)'s one-cell runner outputs into ONE results.csv + counters file,
   refusing a missing cell, a header that differs, or a harness/pin sha that differs.
2. LAY OUT the A4 rule's tree <root>/hven-sqp/<slot>/<set>-run-<R>/: slot `default` holds the
   `sqp-ipm` rows (D, the toggles' OFF state -- the page's stated reading); every A2 label keeps
   its own name. The RETAKE18 controls (`default`, `sqp-qn`) go to a separate tree.
3. DETERMINISM: schema/compare.py run A vs run B per label and set.
4. DECIDE: the harness's scripts/w2_a4_decide.py, unmodified, on the tree (run A scored).
5. CONTROLS: `default` / `sqp-qn` cutest rows vs RETAKE18's chain rows.

Usage: train_close.py CELLS_ROOT OUT_ROOT BENCH_CLONE RETAKE18_RUN_DIR
"""
import csv
import json
import os
import subprocess
import sys

A2_LABELS = {"sqp-ipm": ("hs", "cutest"), "sqp-ipm-mu0c": ("hs", "cutest"),
             "sqp-ipm-mufloor": ("hs", "cutest"), "sqp-ipm-mu1e-3": ("hs",)}
CONTROLS = ("default", "sqp-qn")


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
            f.write("# merged: {} one-cell runs of label {} set {} run {} (A2 train_close.py)\n"
                    .format(len(problems), label, s, run))
            f.write(header)
            f.writelines(body)


def rows_of(path):
    return {r["problem"]: r for r in csv.DictReader(l for l in open(path) if not l.startswith("#"))}


def main(argv):
    cells, out, bench, rt18 = argv[1:5]
    sets = {"hs": roster(bench, "hs-27.json"), "cutest": roster(bench, "cutest-nlp.json")}
    tree = os.path.join(out, "tree")
    ctrl = os.path.join(out, "controls")
    report = []
    for label, fams in A2_LABELS.items():
        slot = "default" if label == "sqp-ipm" else label
        for s in fams:
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
    # The decision, by the harness's own script, unmodified.
    res = subprocess.run(["python3", "-B", os.path.join(bench, "scripts", "w2_a4_decide.py"), tree,
                          "--json", os.path.join(out, "decisions.json")],
                         capture_output=True, text=True)
    with open(os.path.join(out, "decisions.txt"), "w") as f:
        f.write(res.stdout + res.stderr)
    report.append("decide rc={}".format(res.returncode))
    # The controls.
    for label in CONTROLS:
        d = os.path.join(ctrl, "hven-sqp", label, "cutest-run-A")
        merge(cells, label, "cutest", "A", sets["cutest"], d)
        mine = rows_of(os.path.join(d, "results.csv"))
        ref = rows_of(os.path.join(rt18, "run-A-{}".format(label), "cutest", "results.csv"))
        lines = []
        nd = 0
        for p in sets["cutest"]:
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
            if diffs:
                nd += 1
            lines.append("{}: {} | wall {} (ref {})".format(p, "; ".join(diffs) or "identical",
                                                            m["wall_s"], r["wall_s"]))
        with open(os.path.join(out, "control-{}-vs-retake18.txt".format(label)), "w") as f:
            f.write("\n".join(lines) + "\n")
        report.append("control {} vs RETAKE18: {} cell(s) differing of {}".format(
            label, nd, len(sets["cutest"])))
    with open(os.path.join(out, "close-summary.txt"), "w") as f:
        f.write("\n".join(report) + "\n")
    print("\n".join(report))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
