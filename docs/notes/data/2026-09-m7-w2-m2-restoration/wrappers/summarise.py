#!/usr/bin/env python3
"""Folds the ladder legs into the per-cell tables and the neutrality file.

MACHINE-WRITTEN, so no number in the artifact is transcribed by hand. Three
inputs and three outputs:

  in   out/ladder/<P>.txt        the probe, callback ON
       out/ladder-off/<P>.txt    the same cell, callback OFF
       the two retained baselines named below
  out  tables/<P>.md             the cell's restoration ladder
       neutrality.txt            proofs (i), (ii) and (iii), per cell per column
       tables/summary.md         the S5 record for all seven cells, one row each

THE BASELINES, and why they are two. H3's `run-A` was taken at hven pin
3904a477, i.e. at `qp_failure_budget = 1`; M7 W2 M1 then SHIPPED 3, which moves
exactly the cells that fail a subproblem. So `run-A` is the right baseline for
ZAMB2-8 alone, and M1's own committed budget arm -- the 23 CUTEst cells at
precisely the shipped default this ladder is built at -- is the right one for
every other cell.
"""
import csv
import os
import sys

ROOT = "/home/ghecht/Projects/hven"
OUT = os.path.join(ROOT, ".scratch/m7w2m2/out")
RUN_A = "/home/ghecht/Projects/hven-bench/runs/2026-09-m7-h3-cutest/run-A"
M1 = os.path.join(ROOT, "docs/notes/data/2026-09-m7-w2-m1-qp-cap/cells-budget-r1.csv")

CELLS = ["STEENBRB", "STEENBRC", "STEENBRG", "ZAMB2-8", "YORKNET", "BRITGAS", "READING6"]
# The probe prints these; `requesting_h`/`requesting_f` are the probe's OWN
# recomputation off the callback's record and `x_k_source` names the rule that
# found the requesting iterate. All three are properties of the INSTRUMENT and
# exist only when a callback is attached, so they are excluded from the arm
# comparison by construction and named here rather than silently dropped.
CALLBACK_ONLY = {"requesting_h", "requesting_f", "callback", "x_k_source"}


def read_probe(path):
    """(key=value block, ladder rows) from one probe log."""
    kv, rows, in_ladder = {}, [], False
    if not os.path.exists(path):
        return None, None
    for line in open(path):
        line = line.rstrip("\n")
        if line == "LADDER_BEGIN":
            in_ladder = True
            continue
        if line == "LADDER_END":
            in_ladder = False
            continue
        if in_ladder:
            rows.append(line.split(","))
            continue
        if "=" in line and not line.startswith(("PROBE_", "#")):
            k, _, v = line.partition("=")
            kv[k] = v
    return kv, rows


def read_run_a(problem):
    """The hven-sqp row of H3 run-A, results + counters, as one dict."""
    out = {}
    with open(os.path.join(RUN_A, "results.csv")) as handle:
        for raw in handle:
            if raw.startswith("#"):
                header = None
                continue
            break
    rows = [r for r in open(os.path.join(RUN_A, "results.csv")) if not r.startswith("#")]
    reader = csv.reader(rows)
    header = next(reader)
    for row in reader:
        rec = dict(zip(header, row))
        if rec.get("problem") == problem and rec.get("lane") == "hven-sqp":
            out.update(rec)
    crows = [r for r in open(os.path.join(RUN_A, "counters-hven-sqp.csv")) if not r.startswith("#")]
    creader = csv.reader(crows)
    cheader = next(creader)
    for row in creader:
        rec = dict(zip(cheader, row))
        if rec.get("cell_id", "").split("/")[1:2] == [problem]:
            out.update(rec)
    return out


def read_m1(problem):
    with open(M1) as handle:
        for rec in csv.DictReader(handle):
            if rec.get("problem") == problem:
                return rec
    return {}


# The probe key -> baseline key map, for the columns both sides carry.
BASE_MAP = {
    "status": "status",
    "f": "objective",
    "major_iters": "major_iters",
    "qp_minor_iters": "qp_minor_iters",
    "factorizations": "factorizations",
    "steps_accepted": "steps_accepted",
    "rejected_steps": "rejected_steps",
    "restoration_iters": "restoration_iters",
    "elastic_activations": "elastic_activations",
    "symbolic_analyses": "symbolic_analyses",
    "border_refine_steps": "border_refine_steps",
    "stationarity": "sqp_stationarity",
    "feasibility_e": "sqp_feasibility",
}
# The M1 arm's CSV is this runner's own fold of the cell binary's key=value
# output, so every counter is under a `side.` prefix and the top-level columns
# are schema v1's names. Spelled out rather than derived, so a column that
# vanished from the baseline is reported as ABSENT instead of silently skipped.
M1_MAP = {
    "status": "status",
    "f": "objective",
    "major_iters": "side.major_iters",
    "qp_minor_iters": "side.qp_minor_iters",
    "factorizations": "side.factorizations",
    "steps_accepted": "side.steps_accepted",
    "rejected_steps": "side.rejected_steps",
    "restoration_iters": "side.restoration_iters",
    "elastic_activations": "side.elastic_activations",
    "symbolic_analyses": "side.symbolic_analyses",
    "border_refine_steps": "side.border_refine_steps",
    "stationarity": "side.sqp_stationarity",
    "feasibility_e": "side.sqp_feasibility",
    # THE TOP-LEVEL COLUMN, NOT THE SIDE ONE, and the distinction is real rather
    # than a spelling: `kkt_complementarity` is the base `SolveResult`'s declared
    # diagnostic, which is what the probe prints, while `side.sqp_complementarity`
    # is `SqpResult`'s own field and reads 0 on these cells. Mapping to the side
    # column reported a difference that was two different quantities, not two
    # different measurements.
    "complementarity": "kkt_complementarity",
    "feasibility_i": "kkt_primal_ineq",
    "infeasibility_certified": "side.sqp_infeasibility_certified",
}


def compare(probe, base, mapping, label):
    lines, same, diff, absent = [], 0, [], []
    for pk, bk in mapping.items():
        if pk not in probe:
            continue
        if bk not in base:
            absent.append(pk)
            continue
        a, b = probe[pk], base[bk]
        try:
            equal = float(a) == float(b)
        except ValueError:
            equal = a == b
        if equal:
            same += 1
        else:
            diff.append("{}: probe={} {}={}".format(pk, a, label, b))
    lines.append("    columns compared: {}   identical: {}   differing: {}   absent from baseline: {}"
                 .format(same + len(diff), same, len(diff), len(absent)))
    for d in diff:
        lines.append("      DIFFERS  " + d)
    if absent:
        lines.append("      (not carried by the baseline: " + ", ".join(absent) + ")")
    return lines, len(diff)


def main():
    os.makedirs(os.path.join(OUT, "tables"), exist_ok=True)
    neutrality = ["THE THREE-PART NEUTRALITY PROOF, per cell, machine-written by summarise.py.",
                  "",
                  "(i)   the same binary, --callback on vs --callback off, every column the probe prints",
                  "(ii)  ZAMB2-8 only, against H3 run-A (hven pin 3904a477 = qp_failure_budget 1)",
                  "(iii) every other cell, against M1's committed budget arm (the shipped default)",
                  ""]
    worst = 0
    summary = ["| cell | entries | h_in | h_out | f_in | f_out | restoration iters | outcome |",
               "|---|---:|---:|---:|---:|---:|---:|---|"]
    for cell in CELLS:
        on, rows = read_probe(os.path.join(OUT, "ladder", cell + ".txt"))
        off, _ = read_probe(os.path.join(OUT, "ladder-off", cell + ".txt"))
        neutrality.append("=== {} ===".format(cell))
        if on is None:
            neutrality.append("    NO ON-ARM LOG -- the leg did not produce one (censored or failed)")
            worst = 1
            continue
        if off is None:
            neutrality.append("    NO OFF-ARM LOG")
            worst = 1
        else:
            keys = [k for k in on if k not in CALLBACK_ONLY]
            d = [k for k in keys if on.get(k) != off.get(k)]
            neutrality.append("  (i) arms: {} columns compared, {} differing".format(len(keys), len(d)))
            for k in d:
                neutrality.append("      DIFFERS  {}: on={} off={}".format(k, on.get(k), off.get(k)))
            worst |= 1 if d else 0
        if cell == "ZAMB2-8":
            lines, nd = compare(on, read_run_a(cell), BASE_MAP, "run-A")
            neutrality.append("  (ii) against H3 run-A:")
        else:
            lines, nd = compare(on, read_m1(cell), M1_MAP, "M1-budget")
            neutrality.append("  (iii) against M1's budget arm:")
        neutrality.extend(lines)
        neutrality.append("")

        summary.append("| `{}` | {} | {} | {} | {} | {} | {} | {} |".format(
            cell, on.get("restoration_entries", "?"), on.get("restoration_h_in", "?"),
            on.get("restoration_h_out", "?"), on.get("restoration_f_in", "?"),
            on.get("restoration_f_out", "?"), on.get("restoration_iters", "?"),
            on.get("status", "?")))

        # THE CELL'S LADDER. depth-1 rows are the restoration phase's own
        # majors; the depth-0 rows around them are printed for context, and the
        # LAST depth-0 row before the phase is the requesting iterate x_k.
        table = ["# {} -- the restoration excursion ladder".format(cell), "",
                 "Probe: `m2_probe --problem {} --callback on`, uncapped. `f` and `h` are the".format(cell),
                 "CALLER's model recomputed by the probe at the iterate (`h` is the globalization's",
                 "own l1 measure); `dx_inf` is `||x - x_k||_inf` against the requesting iterate;",
                 "`radius` is the sub-solve's own trust radius on a depth-1 row.", "",
                 "S5 at exit: entries={} h {} -> {}  f {} -> {}".format(
                     on.get("restoration_entries"), on.get("restoration_h_in"),
                     on.get("restoration_h_out"), on.get("restoration_f_in"),
                     on.get("restoration_f_out")), "",
                 "| depth | iter | f (caller model) | h (caller model) | dx_inf | radius | f (event) |",
                 "|---:|---:|---:|---:|---:|---:|---:|"]
        for r in rows or []:
            if len(r) < 8 or r[0] == "depth":
                continue
            depth, it, f_event, radius, _step, f_main, h_main, dx = r[:8]
            table.append("| {} | {} | {} | {} | {} | {} | {} |".format(
                depth, it, f_main, h_main, dx, radius, f_event))
        with open(os.path.join(OUT, "tables", cell + ".md"), "w") as handle:
            handle.write("\n".join(table) + "\n")

    with open(os.path.join(OUT, "tables", "summary.md"), "w") as handle:
        handle.write("\n".join(summary) + "\n")
    with open(os.path.join(OUT, "neutrality.txt"), "w") as handle:
        # rstrip: the per-cell blocks end in a blank separator line, and a
        # trailing blank line at EOF is what `git diff --check` refuses.
        handle.write("\n".join(neutrality).rstrip("\n") + "\n")
    print("\n".join(neutrality))
    print("NEUTRALITY_WORST =", worst)
    return 0


if __name__ == "__main__":
    sys.exit(main())
