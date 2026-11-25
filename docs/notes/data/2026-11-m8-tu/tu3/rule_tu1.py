#!/usr/bin/env python3
"""The FROZEN TU-2 adoption rule (hven brief section TU, frozen at TU-1), applied mechanically.

Verbatim terms (hven docs/notes/data/2026-11-m8-tu/DECLARATION-TU-1.md, section 4):

  CELL.    A cell is (set, problem, base label) on one engine's lane.  The CANDIDATE WINS the
           cell if it reaches and the BASELINE does not, OR both reach and the candidate's
           primary counter is at least 10% lower (integer test 10*lower <= 9*higher).  It LOSES
           symmetrically.  Anything else is a TIE.  "Reaches" = the row's `outcome` is
           `optimal` (the lanes bucket acceptable with optimal); a `dnf_wall`, `dnf_budget`,
           `infeasible` or `error` row does not reach, and its counter is never read.
  COUNTER. hven-sqp: `factorizations`.  hven-ipm: `iterations`.  Wall-clock never.
  FAMILY.  A family is one set's TRAIN cells on one engine, pooling that engine's base labels.
           Its sign is sign(wins - losses).
  MOVE.    A default moves iff its family signs include at least one +1 and no -1.
  CENSUS.  Veto: the 57-cell walk census (hven bench/baselines/2026-10-m7-close-corpus/) may
           not lose a status that is not deadline-dependent.  A LOSS is a cell whose baseline
           status is `Optimal` and whose candidate status is not, where neither status is
           deadline-dependent.  Deadline-dependent: `dnf_budget`, `dnf_setup` (the wall budget
           ended the cell) and the cells the baseline header names as wall-dependent
           (`f7_n10000_path_physics`).  A census that is required but missing refuses.

    python3 tu/rule.py BASELINE_DIR CANDIDATE_DIR --engine sqp [--census BASE.csv CAND.csv]
    python3 tu/rule.py --selftest
Each *_DIR holds one results.csv per base label and set (any depth); rows are keyed by
(problem_set, problem, base label) where base label = options_label up to the first '+'.
"""
import argparse
import csv
import os
import sys

WIN, LOSS, TIE = "WIN", "LOSS", "TIE"
PRIMARY = {"hven-sqp": "factorizations", "hven-ipm": "iterations"}
ENGINE_LANE = {"sqp": "hven-sqp", "ipm": "hven-ipm"}
SET_FAMILY = {"cutest": "cutest", "hs": "hs"}

CENSUS_CONVERGED = frozenset({"Optimal"})
CENSUS_DEADLINE_STATUSES = frozenset({"dnf_budget", "dnf_setup"})
CENSUS_WALL_DEPENDENT_CELLS = frozenset({"f7_n10000_path_physics"})
CENSUS_CELLS = 57


# --------------------------------------------------------------------------- the cell rule
def cell_verdict(cand_reached, cand_count, base_reached, base_count):
    """The candidate's verdict against the baseline for one cell."""
    if cand_reached and not base_reached:
        return WIN
    if base_reached and not cand_reached:
        return LOSS
    if cand_reached and base_reached:
        if cand_count is None or base_count is None:
            raise ValueError("a reaching row without its primary counter")
        if cand_count < base_count and 10 * cand_count <= 9 * base_count:
            return WIN
        if base_count < cand_count and 10 * base_count <= 9 * cand_count:
            return LOSS
    return TIE


def family_sign(wins, losses):
    d = wins - losses
    return (d > 0) - (d < 0)


def default_moves(signs):
    """signs: {family: -1/0/+1}.  True iff >= 1 family +1 and none -1."""
    vals = list(signs.values())
    return any(v > 0 for v in vals) and not any(v < 0 for v in vals)


# --------------------------------------------------------------------------- rows
def read_results(path):
    """The data rows of a row-schema v1 results.csv (comment lines skipped)."""
    with open(path) as handle:
        lines = [ln for ln in handle if not ln.startswith("#")]
    return list(csv.DictReader(lines))


def base_label(options_label):
    return options_label.split("+", 1)[0]


def reading(row, lane):
    """(reached, primary counter or None)."""
    reached = row.get("outcome") == "optimal"
    if not reached:
        return False, None
    value = row.get(PRIMARY[lane])
    if value in (None, "", "null"):
        raise ValueError("%s: reaching row without %s" % (row.get("cell_id"), PRIMARY[lane]))
    return True, int(value)


def collect(rows, lane):
    """{(set, problem, base label): (reached, counter)} for one lane; refuses duplicates and
    HOLDOUT rows."""
    out = {}
    for r in rows:
        if r.get("lane") != lane:
            continue
        if r.get("split") != "TRAIN":
            raise SystemExit("STOP: non-TRAIN row %s" % r.get("cell_id"))
        key = (r["problem_set"], r["problem"], base_label(r["options_label"]))
        if key in out:
            raise SystemExit("STOP: duplicate cell %s" % (key,))
        out[key] = reading(r, lane)
    return out


def load_dir(path, lane):
    rows = []
    for dirpath, _, files in os.walk(path):
        if "results.csv" in files:
            rows.extend(read_results(os.path.join(dirpath, "results.csv")))
    if not rows:
        raise SystemExit("STOP: no results.csv under %s" % path)
    return collect(rows, lane)


# --------------------------------------------------------------------------- evaluation
def compare(baseline, candidate, family_of=None, restrict=None):
    """baseline/candidate: {cell: (reached, counter)}.  Cell sets must be equal (after the
    optional `restrict` predicate on the cell key).  Returns (rows, tallies, signs)."""
    family_of = family_of or (lambda key: SET_FAMILY[key[0]])
    b = {k: v for k, v in baseline.items() if restrict is None or restrict(k)}
    c = {k: v for k, v in candidate.items() if restrict is None or restrict(k)}
    if set(b) != set(c):
        raise SystemExit("STOP: cell sets differ: only baseline %s; only candidate %s" % (
            sorted(set(b) - set(c))[:5], sorted(set(c) - set(b))[:5]))
    rows, tallies = [], {}
    for key in sorted(b):
        v = cell_verdict(c[key][0], c[key][1], b[key][0], b[key][1])
        fam = family_of(key)
        rows.append((fam, key, c[key], b[key], v))
        t = tallies.setdefault(fam, {WIN: 0, LOSS: 0, TIE: 0})
        t[v] += 1
    signs = {f: family_sign(t[WIN], t[LOSS]) for f, t in tallies.items()}
    return rows, tallies, signs


def read_census(path):
    """{cell_id: status} from a walk-census CSV (prose/protocol '#' header, then rows)."""
    rows = read_results(path)
    out = {}
    for r in rows:
        out[r["cell_id"]] = r["status"]
    return out


def census_losses(baseline, candidate):
    """The cells the veto counts.  Both maps must cover the same 57 cells."""
    if set(baseline) != set(candidate):
        raise SystemExit("STOP: census cell sets differ")
    if len(baseline) != CENSUS_CELLS:
        raise SystemExit("STOP: census has %d cells, expected %d" % (len(baseline), CENSUS_CELLS))
    lost = []
    for cell in sorted(baseline):
        b, c = baseline[cell], candidate[cell]
        if cell in CENSUS_WALL_DEPENDENT_CELLS:
            continue
        if b in CENSUS_DEADLINE_STATUSES or c in CENSUS_DEADLINE_STATUSES:
            continue
        if b in CENSUS_CONVERGED and c not in CENSUS_CONVERGED:
            lost.append((cell, b, c))
    return lost


def decide(signs, census=None, census_required=True):
    """(moves, reason).  `census` is None or the list census_losses() returned."""
    if not default_moves(signs):
        return False, "family rule: signs %s" % signs
    if census is None:
        if census_required:
            raise SystemExit("STOP: the family rule passes; the census veto must be read")
        return True, "family rule passes; census not required for this candidate"
    if census:
        return False, "census veto: %d non-deadline status loss(es): %s" % (len(census), census)
    return True, "family rule passes; census loses no non-deadline status"


def report(rows, tallies, signs, lane):
    out = ["PER-CELL (candidate vs baseline; counter = %s; R = reached)" % PRIMARY[lane],
           "family | set | problem | base label | cand | cand n | base | base n | verdict"]
    for fam, (s, p, lab), c, b, v in rows:
        out.append("%s | %s | %s | %s | %s | %s | %s | %s | %s" % (
            fam, s, p, lab, "R" if c[0] else "-", "-" if c[1] is None else c[1],
            "R" if b[0] else "-", "-" if b[1] is None else b[1], v))
    out.append("")
    for fam in sorted(tallies):
        t = tallies[fam]
        out.append("FAMILY %s | wins %d | losses %d | ties %d | sign %+d" % (
            fam, t[WIN], t[LOSS], t[TIE], signs[fam]))
    return "\n".join(out)


# --------------------------------------------------------------------------- self-test
def selftest():
    n = 0

    def check(cond, what):
        nonlocal n
        n += 1
        if not cond:
            raise SystemExit("SELFTEST FAIL: %s" % what)

    check(cell_verdict(True, 50, False, None) == WIN, "reach vs not")
    check(cell_verdict(False, None, True, 50) == LOSS, "not vs reach")
    check(cell_verdict(False, None, False, None) == TIE, "neither")
    check(cell_verdict(True, 90, True, 100) == WIN, "exactly 10% lower wins")
    check(cell_verdict(True, 91, True, 100) == TIE, "9% lower ties")
    check(cell_verdict(True, 100, True, 90) == LOSS, "baseline 10% lower loses")
    check(cell_verdict(True, 9, True, 10) == WIN, "small integers: 9 vs 10 is exactly 10%")
    check(cell_verdict(True, 0, True, 0) == TIE, "0 vs 0")
    check(cell_verdict(True, 0, True, 1) == WIN, "0 vs 1")
    check(default_moves({"a": 1, "b": 0}) and not default_moves({"a": 1, "b": -1})
          and not default_moves({"a": 0, "b": 0}) and default_moves({"a": 1, "b": 1}), "move rule")
    check(family_sign(2, 2) == 0 and family_sign(3, 1) == 1, "family sign")
    base = {("cutest", "P", "default"): (True, 100), ("hs", "Q", "default"): (True, 10)}
    cand = {("cutest", "P", "default"): (True, 80), ("hs", "Q", "default"): (True, 10)}
    rows, tallies, signs = compare(base, cand)
    check(signs == {"cutest": 1, "hs": 0}, "compare signs %s" % signs)
    check(decide(signs, census=[])[0] is True, "moves with clean census")
    check(decide(signs, census=[("c", "Optimal", "NumericalError")])[0] is False, "census veto")
    try:
        decide(signs, census=None)
        check(False, "missing census must refuse")
    except SystemExit:
        check(True, "missing census refuses")
    cells = {"f7_n%d_x" % i: "Optimal" for i in range(55)}
    cells["f7_n10000_path_physics"] = "Optimal"
    cells["f7_n5000_path_neutral"] = "dnf_budget"
    cand_c = dict(cells)
    cand_c["f7_n10000_path_physics"] = "dnf_budget"
    cand_c["f7_n5000_path_neutral"] = "NumericalError"
    check(census_losses(cells, cand_c) == [], "deadline cells excluded")
    cand_c["f7_n3_x"] = "WorkLimit"
    check(census_losses(cells, cand_c) == [("f7_n3_x", "Optimal", "WorkLimit")], "real loss counted")
    cand_c["f7_n3_x"] = "dnf_budget"
    check(census_losses(cells, cand_c) == [], "Optimal -> dnf_budget is deadline, excluded")
    print("SELFTEST PASS (%d checks)" % n)


def main(argv):
    if argv[1:] == ["--selftest"]:
        selftest()
        return 0
    ap = argparse.ArgumentParser()
    ap.add_argument("baseline")
    ap.add_argument("candidate")
    ap.add_argument("--engine", choices=sorted(ENGINE_LANE), required=True)
    ap.add_argument("--census", nargs=2, metavar=("BASE_CSV", "CAND_CSV"))
    ap.add_argument("--no-census-required", action="store_true",
                    help="only for candidates the declaration exempts (the IPM engine)")
    args = ap.parse_args(argv[1:])
    lane = ENGINE_LANE[args.engine]
    rows, tallies, signs = compare(load_dir(args.baseline, lane), load_dir(args.candidate, lane))
    print(report(rows, tallies, signs, lane))
    census = None
    if args.census:
        census = census_losses(read_census(args.census[0]), read_census(args.census[1]))
    moves, why = decide(signs, census, census_required=not args.no_census_required)
    print("")
    print("VERDICT: %s -- %s" % ("DEFAULT MOVES" if moves else "DEFAULT STAYS", why))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
