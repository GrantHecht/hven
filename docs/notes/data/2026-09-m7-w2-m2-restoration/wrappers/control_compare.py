#!/usr/bin/env python3
"""The STEENBRC control, compared against the A-SQP read's own published ladder.

THE READ'S TABLE IS QUOTED TO THE DIGITS IT PRINTED (docs/notes/2026-09-m7-w2-
sqp-cutest-read.md, section M2), so the comparison is stated at that precision and
the control's full-precision value is printed beside it. A cell matches when the
control's value ROUNDS to the read's at the significant figures the read chose;
anything else is named as a difference and not explained away.

The uncapped leg is additionally compared at FULL precision against H3's retained
run-A row, which is a byte comparison and not a rounded one.
"""
import csv
import os
import re

S = "/home/ghecht/Projects/hven/.scratch/m7w2m2"
RUN_A = "/home/ghecht/Projects/hven-bench/runs/2026-09-m7-h3-cutest/run-A"

# docs/notes/2026-09-m7-w2-sqp-cutest-read.md section M2, the STEENBRC table.
READ = {
    2:  {"majors": "2", "resto": "0",  "f": "4528.31",    "ce": "10000"},
    3:  {"majors": "2", "resto": "1",  "f": "4610.23",    "ce": "9999.5"},
    6:  {"majors": "2", "resto": "4",  "f": "2.0102e+06", "ce": "9992.5"},
    12: {"majors": "2", "resto": "10", "f": "2.1982e+11", "ce": "9488.5"},
    17: {"majors": "2", "resto": "15", "f": "7.0385e+13", "ce": "1808.5"},
    0:  {"majors": "4", "resto": "16", "f": "1.8345e+14", "ce": "9.09e-13"},
}


def probe_row(k):
    row = {}
    with open(os.path.join(S, "out/control/STEENBRC-b%d.txt" % k)) as handle:
        for line in handle:
            if "=" in line:
                key, _, value = line.rstrip("\n").partition("=")
                row[key] = value
    return row


def sigfigs(text):
    digits = re.sub(r"[^0-9]", "", text.split("e")[0].lstrip("-").lstrip("0").replace(".", ""))
    return max(len(digits), 1)


def rounds_to(value, want):
    """True when `value` printed at `want`'s own significant figures IS `want`."""
    n = sigfigs(want)
    got = float(value)
    if "e" in want:
        return "{:.{}e}".format(got, n - 1) == "{:.{}e}".format(float(want), n - 1)
    return "{:.{}g}".format(got, n) == "{:.{}g}".format(float(want), n)


out = ["THE STEENBRC BUDGET-LADDER CONTROL, against the A-SQP read's own table.",
       "",
       "Instrument: the harness's OWN `hven_bench_cell`, built from the harness source at",
       "348932f (the sha run-A's counters header names) against the harness's OWN pinned",
       "install for hven 3904a477 -- the pair that produced the read's rows. The binary",
       "stamps hven_pin_sha=3904a477771524b85b34678fb7d2ff55dd4768f1, which every leg's",
       "own output carries.",
       "",
       "Compared at the digits the read PRINTED; the control's full value is beside it.",
       ""]
bad = 0
for k in (2, 3, 6, 12, 17, 0):
    row = probe_row(k)
    want = READ[k]
    got = {"majors": row["side.major_iters"], "resto": row["side.restoration_iters"],
           "f": row["objective"], "ce": row["kkt_primal_eq"]}
    out.append("--- --budget-iters %d" % k)
    for field in ("majors", "resto", "f", "ce"):
        if field in ("majors", "resto"):
            ok = got[field] == want[field]
        else:
            ok = rounds_to(got[field], want[field])
        out.append("    %-7s read=%-12s control=%-24s %s"
                   % (field, want[field], got[field], "MATCH" if ok else "DIFFERS"))
        if not ok:
            bad += 1
out.append("")
out.append("READ-TABLE CELLS COMPARED: 24   DIFFERING: %d" % bad)
out.append("")

# The uncapped leg against H3's retained run-A row, at FULL precision.
rows = [r for r in open(os.path.join(RUN_A, "results.csv")) if not r.startswith("#")]
reader = csv.reader(rows)
header = next(reader)
ra = {}
for row in reader:
    rec = dict(zip(header, row))
    if rec.get("problem") == "STEENBRC" and rec.get("lane") == "hven-sqp":
        ra = rec
crows = [r for r in open(os.path.join(RUN_A, "counters-hven-sqp.csv")) if not r.startswith("#")]
creader = csv.reader(crows)
cheader = next(creader)
for row in creader:
    rec = dict(zip(cheader, row))
    if rec.get("cell_id", "").split("/")[1:2] == ["STEENBRC"]:
        ra.update(rec)

b0 = probe_row(0)
out.append("--- THE UNCAPPED LEG AGAINST H3 run-A, AT FULL PRECISION (a byte comparison)")
pairs = [("status", "status"), ("outcome", "outcome"), ("objective", "objective"),
         ("factorizations", "factorizations"), ("iterations", "iterations"),
         ("side.major_iters", "major_iters"), ("side.qp_minor_iters", "qp_minor_iters"),
         ("side.restoration_iters", "restoration_iters"),
         ("side.steps_accepted", "steps_accepted"), ("side.rejected_steps", "rejected_steps"),
         ("side.elastic_activations", "elastic_activations"),
         ("side.symbolic_analyses", "symbolic_analyses"),
         ("side.border_refine_steps", "border_refine_steps"),
         ("side.sqp_stationarity", "sqp_stationarity"), ("side.sqp_feasibility", "sqp_feasibility"),
         ("kkt_primal_eq", "kkt_primal_eq")]
n_same = 0
n_diff = 0
for mine, theirs in pairs:
    if mine not in b0 or theirs not in ra:
        out.append("    %-26s NOT CARRIED BY ONE SIDE" % mine)
        continue
    same = b0[mine] == ra[theirs]
    n_same += same
    n_diff += not same
    out.append("    %-26s control=%-24s run-A=%-24s %s"
               % (mine, b0[mine], ra[theirs], "IDENTICAL" if same else "DIFFERS"))
out.append("")
out.append("FULL-PRECISION COLUMNS COMPARED: %d   IDENTICAL: %d   DIFFERING: %d"
           % (n_same + n_diff, n_same, n_diff))
text = "\n".join(out) + "\n"
with open(os.path.join(S, "out/control.txt"), "w") as handle:
    handle.write(text)
print(text)
