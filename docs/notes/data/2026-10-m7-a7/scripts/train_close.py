#!/usr/bin/env python3
"""A7 TRAIN close (design page section 4.4): pure file processing, no solver.

1. MERGE each (label, set, run)'s one-cell runner outputs into ONE results.csv + counters file,
   refusing a missing cell, a header that differs, or a harness/pin sha that differs.
2. LAY OUT two A4-rule trees: tree-exact/hven-sqp/{default, sqp-wsrule}/<set>-run-<R>/ and
   tree-qn/hven-sqp/{default <- the sqp-qn rows, sqp-qn-wsrule}/<set>-run-<R>/ (the A2 precedent:
   the toggle's OFF state sits in the script's `default` slot).
3. T-1 DETERMINISM: schema/compare.py run A vs run B per label and set.
4. T-3..T-6 DECIDE: the harness's scripts/w2_a4_decide.py, unmodified, on each tree (run A scored).
5. T-0 CONTROLS: `default` / `sqp-qn` cutest run A vs RETAKE18's chain rows.
6. T-2 INERT UNTIL IT FIRES: every ON run-A cell whose fire log reads 0 switches against its base's
   run-A row on every results.csv column but wall_s and the two options columns.

Usage: train_close.py CELLS_ROOT OUT_ROOT BENCH_CLONE RETAKE18_RUN_DIR FIRE_LOG
"""
import csv
import json
import os
import subprocess
import sys

LABELS = ("default", "sqp-wsrule", "sqp-qn", "sqp-qn-wsrule")
BASE = {"sqp-wsrule": "default", "sqp-qn-wsrule": "sqp-qn"}
SETS = ("hs", "cutest")
NOT_COMPARED = {"wall_s", "options_label", "options_digest", "cell_id"}


KLASS = {}


def near(a, b):
    """schema/compare.py's measure gate: byte equality first, then the near-ulp rule."""
    if a == b:
        return True
    try:
        x, y = float(a), float(b)
    except ValueError:
        return False
    return abs(x - y) <= max(1e-5 * max(abs(x), abs(y)), 1e-13)


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
            f.write("# merged: {} one-cell runs of label {} set {} run {} (A7 train_close.py)\n"
                    .format(len(problems), label, s, run))
            f.write(header)
            f.writelines(body)


def rows_of(path):
    return {r["problem"]: r for r in csv.DictReader(l for l in open(path) if not l.startswith("#"))}


def fire_log(path):
    """{(label, set, run, problem): switches} from the hook's lines, keyed by the RUN marker."""
    out, cur = {}, None
    for line in open(path):
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "RUN":
            cur = tuple(parts[1:5])
            continue
        if cur is None or len(parts) < 4:
            continue
        sw = int(parts[2].split("=")[1])
        qps = int(parts[3].split("=")[1])
        out[cur] = (sw, qps)
    return out


def main(argv):
    cells, out, bench, rt18, flog = argv[1:6]
    sys.path.insert(0, os.path.join(bench, "schema"))
    import columns  # noqa: E402  (the harness's own schema: the column klasses)
    KLASS.update(columns.KLASS_OF)
    sets = {"hs": roster(bench, "hs-27.json"), "cutest": roster(bench, "cutest-nlp.json")}
    report = []
    merged = os.path.join(out, "merged")
    for label in LABELS:
        for s in SETS:
            for run in ("A", "B"):
                merge(cells, label, s, run, sets[s],
                      os.path.join(merged, label, "{}-run-{}".format(s, run)))
            a = os.path.join(merged, label, "{}-run-A".format(s), "results.csv")
            b = os.path.join(merged, label, "{}-run-B".format(s), "results.csv")
            res = subprocess.run(["python3", "-B", os.path.join(bench, "schema", "compare.py"), a, b],
                                 capture_output=True, text=True)
            with open(os.path.join(out, "determinism-{}-{}.txt".format(label, s)), "w") as f:
                f.write(res.stdout + res.stderr)
            last = (res.stdout.strip().splitlines() or ["(no output)"])[-1]
            report.append("T-1 determinism {} {}: rc={} :: {}".format(label, s, res.returncode, last))
    # The two A4 trees, laid out by copying the merged runs into the script's expected slots.
    for tree, slots in (("tree-exact", {"default": "default", "sqp-wsrule": "sqp-wsrule"}),
                        ("tree-qn", {"default": "sqp-qn", "sqp-qn-wsrule": "sqp-qn-wsrule"})):
        for slot, label in slots.items():
            for s in SETS:
                for run in ("A", "B"):
                    src = os.path.join(merged, label, "{}-run-{}".format(s, run))
                    dst = os.path.join(out, tree, "hven-sqp", slot, "{}-run-{}".format(s, run))
                    os.makedirs(dst, exist_ok=True)
                    for fname in os.listdir(src):
                        with open(os.path.join(src, fname)) as fi, open(os.path.join(dst, fname), "w") as fo:
                            fo.write(fi.read())
        res = subprocess.run(["python3", "-B", os.path.join(bench, "scripts", "w2_a4_decide.py"),
                              os.path.join(out, tree), "--json",
                              os.path.join(out, "decisions-{}.json".format(tree))],
                             capture_output=True, text=True)
        with open(os.path.join(out, "decisions-{}.txt".format(tree)), "w") as f:
            f.write(res.stdout + res.stderr)
        report.append("decide {} rc={}".format(tree, res.returncode))
    # T-0: the controls against RETAKE18.
    for label in ("default", "sqp-qn"):
        mine = rows_of(os.path.join(merged, label, "cutest-run-A", "results.csv"))
        ref = rows_of(os.path.join(rt18, "run-A-{}".format(label), "cutest", "results.csv"))
        lines, nd = [], 0
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
        with open(os.path.join(out, "T0-control-{}-vs-retake18.txt".format(label)), "w") as f:
            f.write("\n".join(lines) + "\n")
        report.append("T-0 control {} vs RETAKE18: {} cell(s) differing of {}".format(
            label, nd, len(sets["cutest"])))
    # T-2: inert until it fires; plus the fire set, per ON label and set, run A.
    fires = fire_log(flog)
    t2_lines, t2_bad = [], 0
    for label, base in BASE.items():
        for s in SETS:
            on = rows_of(os.path.join(merged, label, "{}-run-A".format(s), "results.csv"))
            off = rows_of(os.path.join(merged, base, "{}-run-A".format(s), "results.csv"))
            fired, quiet, unscored = [], [], []
            for p in sets[s]:
                key = (label, s, "A", p)
                if key not in fires:
                    unscored.append(p)
                    continue
                sw, qps = fires[key]
                if sw:
                    fired.append("{}(qps={})".format(p, qps))
                    continue
                strict = [k for k in on[p] if k not in NOT_COMPARED and on[p][k] != off[p][k]]
                gated = [k for k in strict if not (KLASS.get(k) == "measure"
                                                   and near(on[p][k], off[p][k]))]
                if gated:
                    t2_bad += 1
                    t2_lines.append("T-2 VIOLATION {} {} {}: {}".format(label, s, p, gated))
                elif strict:
                    t2_lines.append("T-2 note {} {} {}: measure columns within the harness gate, "
                                    "not byte-equal: {}".format(label, s, p, strict))
                quiet.append(p)
            t2_lines.append("{} {}: fired {} = {}".format(label, s, len(fired), " ".join(fired)))
            t2_lines.append("{} {}: quiet {} (compared against {})".format(label, s, len(quiet), base))
            t2_lines.append("{} {}: unscored (no fire-log line; dnf_wall) {} = {}".format(
                label, s, len(unscored), " ".join(unscored)))
    # And the OFF labels must never fire (G3 at TRAIN level).
    off_fired = sorted("{}/{}/{}/{}".format(*k) for k, v in fires.items()
                       if k[0] in ("default", "sqp-qn") and v[0] != 0)
    t2_lines.append("OFF labels with a nonzero switch: {}".format(off_fired or "none"))
    with open(os.path.join(out, "T2-fire-and-identity.txt"), "w") as f:
        f.write("\n".join(t2_lines) + "\n")
    report.append("T-2 inert until it fires: {} violation(s); OFF fired: {}".format(
        t2_bad, len(off_fired)))
    with open(os.path.join(out, "close-summary.txt"), "w") as f:
        f.write("\n".join(report) + "\n")
    print("\n".join(report))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
