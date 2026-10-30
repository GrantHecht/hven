#!/usr/bin/env python3
"""M7 W2 M3c -- assembles `tables/census.md` from the committed transcripts.

EVERY NUMBER IN THE DELIVERABLE COMES THROUGH HERE, out of
`logs/classify-census.txt` (which `classify_m3c.py` wrote from the leg rows) and
`logs/identity-EI.txt` / `logs/identity-all.txt` (which the chain wrote). The
prose sections are constants in this file and are the IMPLEMENTER'S WORDS; they
quote no figure the generated sections do not print directly above them.

  usage: census_table.py --artifact DIR
"""
import argparse
import os
import sys
import textwrap

ORDER = ["ACOPP118", "ACOPP57", "ACOPR57", "BRITGAS", "DALLASM", "NET3",
         "STEENBRB", "STEENBRC", "STEENBRD", "STEENBRE", "STEENBRG",
         "YORKNET",
         "ACOPR118", "BIGBANK", "MSS2", "MSS3", "READING8", "SAWPATH",
         "TAX213322", "TAX53322"]
GROUP = {c: ("E" if i < 11 else "I" if i == 11 else "W") for i, c in enumerate(ORDER)}


def die(msg):
    print(f"REFUSED [setup] {msg}", file=sys.stderr)
    sys.exit(2)


def read_classification(path):
    if not os.path.exists(path):
        die(f"no classification transcript at {path}")
    cells, cur = {}, None
    for raw in open(path):
        ln = raw.rstrip("\n")
        if ln.startswith("CELL "):
            cur = ln.split()[1]
            cells[cur] = {"flags": [], "raw": []}
            continue
        if cur is None or not ln.strip():
            continue
        cells[cur]["raw"].append(ln)
        s = ln.strip()
        for key in ("TERMINATOR", "WHY", "EVIDENCE", "CHAIN", "CONSENSUS",
                    "LANE_OBJECTIVES", "CLOSENESS_OBJECTIVE",
                    "CLOSENESS_FEASIBILITY", "COST_VS_SNOPT",
                    "QP_STATUS_SEQUENCE", "LEDGER_PRINT_CAP"):
            if s.startswith(key + " "):
                cells[cur][key] = s[len(key) + 1:]
        if s.startswith("FLAG "):
            cells[cur]["flags"].append(s[5:])
        if s.startswith("REFUSED "):
            cells[cur]["REFUSED"] = s
    missing = [c for c in ORDER if c not in cells]
    if missing:
        die(f"the transcript classifies no row for: {', '.join(missing)}")
    return cells


def read_identity(paths):
    """`IDENTITY <CELL> <VERDICT>` lines. The groups-E+I gate is authoritative
    for the twelve cells it covers; the all-cells gate supplies group W."""
    verdict = {}
    for p in paths:
        if not os.path.exists(p):
            die(f"no identity transcript at {p}")
        for raw in open(p):
            f = raw.split()
            if len(f) >= 3 and f[0] == "IDENTITY":
                verdict.setdefault(f[1], f[2])
    return verdict


def judged_split(cls):
    """The F4 split, COUNTED from `logs/classify-census.txt`'s own flag lines.

    This sentence was hand-written once and was wrong (it said 0 on nine of
    twelve and 2 on the other three); Codex sol's review caught it. It is now
    counted, over both populations a reader could mean, and neither count is
    typed: `WITH A ROW` is every non-CENSORED cell, and the subset that BUILT A
    SUBPROBLEM drops the three `START_POINT_NONFINITE` cells, whose QP never
    ran.
    """
    import re as _re
    vals = {}
    for c, d in cls.items():
        for f in d["flags"]:
            m = _re.search(r"judged_rejections=(-?\d+)", f)
            if f.startswith("F4_REJECTION_STALL") and m:
                vals[c] = int(m.group(1))
    rows = {c: v for c, v in vals.items()
            if cls[c].get("TERMINATOR") not in (None, "CENSORED")}
    built = {c: v for c, v in rows.items()
             if cls[c].get("TERMINATOR") != "START_POINT_NONFINITE"}
    if not rows or not built:
        die("no F4 judged_rejections lines to count")

    def split(d):
        zero = sorted(c for c, v in d.items() if v == 0)
        rest = sorted((c, v) for c, v in d.items() if v != 0)
        return zero, rest

    z_all, r_all = split(rows)
    z_b, r_b = split(built)
    nz = sorted({v for _c, v in r_all})
    return textwrap.fill("`judged_rejections = rejected_steps − qp_failures_routed` is **0** "
            "on %d of the %d cells with a row and %s on the other %d (%s); over "
            "the %d of those that BUILT A SUBPROBLEM at all — the three "
            "`START_POINT_NONFINITE` cells never ran a QP — it is **0** on %d "
            "and %s on %d."
            % (len(z_all), len(rows),
               "/".join(str(v) for v in nz), len(r_all),
               ", ".join("`%s`=%d" % (c, v) for c, v in r_all),
               len(built), len(z_b),
               "/".join(str(v) for v in sorted({v for _c, v in r_b})),
               len(r_b)), width=78)


def flag_summary(flags):
    """The flags that FIRED, by name, shortest form. `-` when none did."""
    fired = [f.split(" = ")[0].replace("_", " ", 1).split()[0] for f in flags
             if " = true" in f]
    return ", ".join(fired) if fired else "(none)"


HEADER = """# M7 W2 M3c — the DEFAULT-PATH failure census

**What kills every failing CUTEst TRAIN cell of the `hven-sqp` lane at DEFAULT
options.** Twenty cells: the eleven `error` cells, the one `infeasible` cell,
and the eight `dnf_wall` cells. Governed by
[`RULE.md`](../RULE.md), which was committed before any leg ran and is applied
here AS WRITTEN.

**This file is GENERATED** by `wrappers/census_table.py` from
`logs/classify-census.txt` (the two axes, machine-applied by
`wrappers/classify_m3c.py`, whose 60 fold proofs are in `logs/fold-classify.txt`)
and from the two committed identity transcripts. The prose sections are the
implementer's; every figure in them is printed by a generated section.

## What this is not

`RULE.md` §0 and §5.3 bind every line below. **A class name says WHICH PREDICATE
FIRED, never why a solve died.** `QP_CAP_BUDGET` says a walk exhausted its minor
cap and the driver's consecutive-failure budget refused another shrink-and-retry;
it says nothing about whether that walk was cycling, saddle-stuck,
ill-conditioned or merely large. That is pass 2's question, and M3b's two cells
landed in DIFFERENT causes with the same outward counters. Every label is printed
with the fields that produced it. **No outcome here licenses a behaviour change**
(`RULE.md` §10); §"Recommendation" is a recommendation to the settler and binds
nothing.

## Provenance

| | |
|---|---|
| tree | `/home/ghecht/Projects/hven`, branch `m7` |
| hven HEAD the legs ran at | `8e4c21a7` (`RULE.md` was written against `b2bbaed8`; `8e4c21a7` is this task's own three data commits on top of it, no `src/` or `include/` change) |
| harness pin the rows are compared against | `aeebdccc` |
| pass-1 chain | detached pid 2702409, 2026-09-21T17:56:43 → 19:42:07 |
| options | hven's own defaults + `common.threads = 1`, `common.print_level = 10` — the lane's `default` label, constructed by the probe and printed per leg in its `OPTIONS` block |
| terms | one solver process at a time, `MKL_NUM_THREADS=1`, `taskset -c 2`, the box lock per leg |
| wall-clock | INFORMATIONAL ONLY (CLAUDE.md §7). The two budgets are STOPS, not measurements |

## The identity gate, first

Twelve cells are identity-comparable against the harness's committed rows at the
pin — the eleven `error` cells and `YORKNET` — and **all twelve reproduce them on
all 24 compared fields**, `IDENTITY_DIFFERS=0`. None of M2, M2b, M3a or M3b moved
default behaviour on any of them, which `RULE.md` §4 existed to CHECK rather than
to assume.

Three group-W cells carry TRUNCATED rows and the gate reports them NOT COMPARABLE:
a `dnf_wall` row at the pin carries `status = null` and no counters, so there is
nothing to reproduce (§4.1). Five group-W cells have NO ROW at all.

**The all-cells gate returns `IDENTITY_VERDICT=STRUCTURAL_REFUSAL`, rc 2, and the
rule says what that is.** It is §4.3 (c)/(d) refusing the five absent rows — "a
missing row is a REFUSAL, never a skip" — and NOT §3.2's structural abort, whose
bar is "EVERY identity-comparable cell of groups E and I differs on the five core
fields". That bar did not fire: the groups-E+I gate over exactly those cells
returns `IDENTITY_VERDICT=ALL_COMPARABLE_CELLS_IDENTICAL`. The twelve PASSes
stand; the census has a five-cell hole in it and says so on every line.
"""

SUMMARY_INTRO = """
## The census — one row per cell

`TERMINATOR` is `RULE.md` §5.2's axis-1 class, in the rule's own precedence.
`FLAGS FIRED` lists the axis-2 flags that returned `true`; every flag's own
numbers, including the ones that returned `false` or `N/A`, are in that cell's
block below. `CLOSENESS` is the objective read-out of §7.1 — the gap to §1.3's
consensus, machine-derived from the committed lane rows, `ABSENT` where §1.3
produces no consensus.
"""

CENSORED_NOTE = """
**The five CENSORED cells are never filled.** `MSS2`, `MSS3`, `READING8`,
`TAX213322` and `TAX53322` were killed by the outer `timeout 900`; `RULE.md`
§3.1 and §5.2 class 1 forbid filling a row for them later or reconstructing one.
This is the expected outcome and not a surprise: the harness's own SQP cost
census (C1) already showed these five completing no major inside 1800 s. What
the census can say about them is exactly this sentence.
"""

FINDINGS = """
## Findings about the RULE itself

`RULE.md` is **not edited** — a pre-registered rule's bytes are the pin, and a
defect in one is recorded, never repaired after the fact (M3b Corrections C6, C9).
Four defects were found by applying it.

### R1 — §5.2 #7 is AMBIGUOUS, and the ambiguity decides `YORKNET`'s class

Class 7's conjunct reads: *"the last history row is not a QP-failure row (i.e.
classes 4–6 did not fire)"*. Those are two different predicates. They disagree on
exactly one shape — a cell whose terminal status is NOT `numerical_error` (so
classes 4–6 cannot fire whatever the row says) while its last history row DOES
carry a failing `qp_status`. On the shipped path that shape is produced by the
elastic tier's own restoration exit, which stamps the ELASTIC LADDER's status
onto the row and sets the verdict to `Restore`
(`src/drivers/sqp_solver.cpp`, the `if (!report.usable)` arm) — the very overwrite
`RULE.md` §5.1's second semantic note warns about.

`YORKNET` is that cell, and it is the only one. Under the rule's own
parenthetical gloss it is `RESTORATION_EXIT(infeasible)`, which is what §5.2 #7's
"*Expected: YORKNET*" says the rule meant. Under the literal phrase, classes 4–7
are all silent and the cell falls to `OTHER`. **The gloss reading is what the
classifier returns**, because the parenthetical is the rule's own definition of
its own phrase in the same clause; the literal reading is printed beside it on
that cell's line so no reader has to take the script's word for it.

### R2 — §5.2 #4 fires on `BRITGAS`, whose exit is NOT the failure budget

`BRITGAS` satisfies class 4's predicate exactly: terminal `numerical_error`, the
last history row's QP ended `max_iter`, and `qp_failures_in_a_row_peak == 3 ==
qp_failure_budget`. It is labelled `QP_CAP_BUDGET` and that label stands as the
rule's output.

**Its mechanism is something else.** The budget was reached over majors 0–2, all
three `max_iter` and all three routed. Major 3's own subproblem then returned
`infeasible`, which the routed-failure block never sees — "`kInfeasible` reaches
neither field: the elastic tier consumed it one block above" — so the elastic
ladder ran, returned unusable at `max_iter`, and the RESTORATION exit followed.
The restoration sub-solve ended `numerical_error`, and that is the solve's
terminal status. The budget was never the gate that ended this cell. The census
prints `last_elastic_applied=1` and `last_verdict=Restore` on `BRITGAS`'s
evidence line, which is where a reader sees it; §5.2 #4's predicate simply does
not condition on either. This is the same class of defect as R1 and has the same
root: the terminal history row's `qp_*` group is not always the main engine's.

### R3a — F2's h-band degenerates when `h_in == 0`

`SAWPATH` enters restoration with `restoration_h_in = 0` and leaves with
`restoration_h_out = 0`, so `h_out >= 0.99 * h_in` reads `0 >= 0` and
`F2_RESTORATION_STUCK` fires. That is the rule's EXACT null — "a sub-solve that
returns the point it was handed" — so the flag is not wrong; but on a cell whose
violation was already zero it is not the statement the flag's name suggests
either. The pair `(0, 0)` is printed beside it, which is how a reader sees it,
and the ratio column prints `n/a (h_in == 0)` rather than dividing. `YORKNET` is
the flag's real case: `h_in == h_out == 48491.383947016526`, bit for bit, after
18 restoration iterations.

### R3 — the cause-shaped flags F3 and F4 are DEAD on this population

`F3_TR_AT_FLOOR` is `false` on **every** cell with a row (the smallest terminal
radius anywhere is `0.0125`, against a floor test at `2e-10`), and
`F4_REJECTION_STALL` is `false` on every cell too. F4's reason is worth stating
because it is a finding rather than a null: on these cells
__JUDGED_SPLIT__ **Essentially every `Reject` verdict in these
histories is a ROUTED QP FAILURE, not a merit-function rejection.** The
globalization strategy is barely being consulted; the subproblem is where these
solves die. That is an OBSERVED fact about the population and it is the single
most load-bearing line in this census.
"""

RECOMMENDATION = """
## Recommendation to the settler — engine change ⇒ cells it plausibly converts

**A RECOMMENDATION, and nothing more** (`RULE.md` §0 and §10). Each candidate is
its own task with its own pre-registered rule and its own gates; none is selected
or designed here. OBSERVED and INFERRED are marked separately on every line.

### Candidate A — the QP walk's minor cap on a degenerate working set
*Would plausibly convert: `ACOPP118`, `ACOPP57`, `ACOPR57`, `DALLASM`,
`STEENBRB`, `STEENBRC`, `STEENBRG` (7 cells), and is implicated in `BRITGAS`,
`SAWPATH` and `YORKNET` (3 more).*

**OBSERVED.** Eight of the twelve identity-passing cells carry
`qp_failures_in_a_row_peak = 3 = qp_failure_budget` with 4–9 `qp_maxiter_exits`
and 0–1 `qp_numerical_exits`: the budget is spent on subproblems hitting their
ITERATION CAP, not on arithmetic. Every capped ledger record in this census has
the same shape — `degenerate_steps ≈ minors/2`, `ws_adds ≈ ws_drops ≈ minors/2`.
`ACOPP118`'s four capped records are `4710` minors with `2303–2305` degenerate
steps and `2399/2306` adds/drops; `YORKNET`'s single capped record is `5080`
minors with `2539` degenerate steps, `2539` drop ties and `2540/2540` adds/drops.
Three of the seven (`ACOPP118`, `ACOPP57`, `ACOPR57`) never restore at all and
accept 3–8 steps first, then stop moving.

**INFERRED, and pass 2 exists to check it.** That counter shape is what a
zero-motion period-2 add/drop cycle produces, which is the tail M3b traced at
`tr_init = +inf`. It is NOT established here: the ledger sees per-solve totals,
not per-minor motion, and M3b's own warning is that two cells with the same
outward counters had different causes.

### Candidate B — the start point (`project_start_point`, M7 W2 M4, OFF by default)
*Would plausibly convert: `STEENBRD`, `STEENBRE`; would not convert `NET3`
(3 cells in the class, 2 candidates).*

See the section below. **Likely a cheap, separate engine item**, and independent
of every QP question in this census.

### Candidate C — the restoration excursion
*Would plausibly convert: `STEENBRB`, `STEENBRC`, `STEENBRG` (3 cells, all of
them also in candidate A).*

**OBSERVED.** `F1_RESTORATION_EXCURSION` fires on exactly these three. Each
restores from a large violation to `h_out ≤ 5e-12` and lands the objective **nine
to eleven orders of magnitude above where it started**: `STEENBRB` `2.86e3 →
4.09e12`, `STEENBRC` `4.53e3 → 1.83e14`, `STEENBRG` `4.53e3 → 1.83e14`. Against
the three-lane consensus `9075.86` for `STEENBRB`, the restored point is `4.5e8`
relative away. The subsequent QPs at that point are the capped ones. `DALLASM`
restores too and does NOT fire the flag: its ratio is `5.1e-05`, three orders
under the bar — and yet `DALLASM`'s terminal objective `6.20e7` is `1.29e3`
relative from the three-lane consensus `-4.81982e4`, so it is far from the answer
for a reason this flag does not name.

**INFERRED.** Whether a feasibility restoration that ignores the objective
entirely (the shipped `restoration_proximity = 0`, proven per leg by the OPTIONS
block) is what puts these cells somewhere unrecoverable is not established by
these counters. M2b built the proximity term and landed it OFF; this census is
evidence FOR measuring it on these three cells, not evidence that it works.

### Candidate D — the wall
*Would plausibly convert: nothing directly; 5 CENSORED + 3 TRUNCATED cells are
invisible until something else moves.*

**OBSERVED.** `ACOPR118` at the stop is `5.9e-4` relative from the three-lane
consensus after 6 majors, with one capped `5300`-minor record already spent and a
second in flight. `BIGBANK` is still ACCEPTING steps (`AcceptF` on all three of
its majors, radius growing `1 → 2 → 4 → 8`) and has no failure of any kind — it is
simply slow. `SAWPATH` spent `77 037` minors inside ONE major, `6 830` of them in
a single capped elastic solve, against SNOPT's 8 majors and 21 minors.

**INFERRED.** `BIGBANK` looks budget-bound rather than broken; `ACOPR118` and
`SAWPATH` look like candidate A under a clock. None of that is a measurement of
what they would do with more time, and this census does not make one.

### What NOT to build from this census
**`TR_COLLAPSE` and `REJECTION_STALL` are not live on this population** (finding
R3): no cell reached the trust-region floor, and judged rejections are 0 or 2
everywhere. Trust-region floor work and merit/filter acceptance work would be
spent on conditions these twenty cells do not have.
"""

NONFINITE = """
## OUTSIDE THE RULE — the three `START_POINT_NONFINITE` cells

**`RULE.md` pre-registers no read-out for this class beyond §5.2 #3's predicate,
so everything in this section is OUTSIDE THE RULE and is labelled as such.** It
is written from the PUBLIC probe surface and the committed harness rows only. No
leg was run for it, no source was changed, and nothing below is a verdict.

`NET3`, `STEENBRD` and `STEENBRE` die at major 0 having built no subproblem:
0 majors, 0 factorizations, 0 minors, `restoration_sub_status = not_run`, an
empty ledger, and one history row whose QP never ran.

**OBSERVED — the objective itself is NaN at the start point.** Each row carries
`c_evals_full = 1`: exactly one full model evaluation happened, at `x0`, and the
probe's `f` on this exit is the model's own objective there
(`src/drivers/sqp_solver.cpp`: the non-finite exit sets `out.f = ev.f`, and
scaling is off by default). All three print `f = -nan`. So `f(x0)` is NaN, which
is on its own sufficient for the exit.

**ABSENT — which OTHER evaluation is also non-finite.** The driver's gate is
`kkt.finite`, which conjoins `isfinite(f)`, `grad.allFinite()`,
`ce.allFinite()` and `ci.allFinite()` into one bool, and on failure it sets every
residual to NaN. The public surface cannot separate them, and this section does
not guess. `c_start_point_clamps = 0` on all three is likewise NOT an observation
that `x0` is inside its box: `solver_counters.h` states the field is written only
under `project_start_point`, and at the toggle's OFF state it is identically 0.

**OBSERVED — what the other lanes did on the same three cells**, from the
committed rows:

| cell | hven-sqp @HEAD | hven-ipm @`aeebdccc` | IPOPT | SNOPT |
|---|---|---|---|---|
| `NET3` | `numerical_error`, 0 majors | `max_iter` (dnf_budget), 1000 iters, f = 5859616.03 | `Restoration_Failed`, 194 iters | `PointCannotBeImproved`, 10 iters |
| `STEENBRD` | `numerical_error`, 0 majors | **`optimal`**, 247 iters, f = 9030.0817643754581 | `Error_In_Step_Computation`, 192 iters | **`Optimal`**, 155 iters, f = 9030.0818676051877 |
| `STEENBRE` | `numerical_error`, 0 majors | **`optimal`**, 223 iters, f = 28396.373260963694 | `Error_In_Step_Computation`, 309 iters | **`Optimal`**, 175 iters, f = 27458.821606407844 |

`NET3` defeats all four lanes and is not a start-point story. **`STEENBRD` and
`STEENBRE` are solved by hven's OWN IPM engine, on the same model through the
same harness bridge** — so the model is evaluable somewhere near that start
point, and the SQP lane is the only lane that stops at it.

**OBSERVED — the option exists and is OFF.** Every leg's `OPTIONS` block prints
`opt_project_start_point=0`. `SqpOptions::project_start_point` (M7 W2 M4,
`include/hven/drivers/sqp_solver_types.h:715`) defaults to `false`; at `true` it
projects the `x0` ARGUMENT componentwise into the seam's declared box,
`min(max(x, l), u)`, above the `begin` line, so the whole solve describes one
point. It is byte-identical to OFF when every component is already in its box.
Its scope note says it projects the `x0` argument and not a warm solve's resolved
start point. `solver_counters.h` records that the IPM's twin is
`IpmSolver::push_initial_point_interior` — the IPM does not start at the raw `x0`
at all.

**INFERRED, and explicitly not established.** The obvious reading — that CUTEst's
declared start point for these cells lies outside the declared box, that the
model is not evaluable there, and that the IPM's interior push is why its lane
does not see this — is CONSISTENT with every observation above and is NOT
measured by any of them.

**What would have to be measured to confirm it**, as its own small task:

1. Evaluate the model's `f`, `cE`, `cI` and `grad` at the CUTEst start point for
   `NET3`, `STEENBRD` and `STEENBRE` through the harness bridge, and report WHICH
   of the four is non-finite and in which components.
2. Report, for each, the count of start-point components strictly outside
   `[l, u]` — i.e. run one leg per cell with `project_start_point = true` and read
   `start_point_clamps`, which is exactly the number that answers it.
3. Re-run those three cells at `project_start_point = true` at default options
   otherwise, and report the terminal status. Two cells converting would make this
   a one-flag change; `NET3` converting would be a surprise, since no lane solves it.

All three are cheap (seconds per cell: these solves stop at major 0 today) and
none of them touches the QP walk. **This is a separate engine item from everything
else in this census**, and it is the only class here whose remedy candidate is
already written and shipped-but-off.
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--artifact", required=True)
    a = ap.parse_args()
    art = a.artifact
    cls = read_classification(os.path.join(art, "logs/classify-census.txt"))
    ident = read_identity([os.path.join(art, "logs/identity-EI.txt"),
                           os.path.join(art, "logs/identity-all.txt")])
    for c in ORDER:
        if c not in ident:
            die(f"no identity verdict for {c} in either transcript")

    out = [HEADER, SUMMARY_INTRO]
    out.append("| cell | grp | identity | TERMINATOR | FLAGS FIRED | CLOSENESS (objective) |")
    out.append("|---|---|---|---|---|---|")
    for c in ORDER:
        d = cls[c]
        term = d.get("TERMINATOR", "—")
        if term == "CENSORED":
            out.append(f"| `{c}` | {GROUP[c]} | {ident[c]} | **CENSORED** | — | — "
                       f"(no row: RULE.md §3.1) |")
            continue
        close = d.get("CLOSENESS_OBJECTIVE", "—")
        if close.startswith("f="):
            # the RELATIVE gap, whole and unrounded -- the summary column is a
            # pointer to the per-cell block, never a rounding of it
            close = "rel `" + close.split("rel=")[1] + "`"
        out.append(f"| `{c}` | {GROUP[c]} | {ident[c]} | `{term}` | "
                   f"{flag_summary(d['flags'])} | {close} |")
    out.append(CENSORED_NOTE)

    out.append("\n## Per cell — the evidence behind each line\n")
    for c in ORDER:
        d = cls[c]
        out.append(f"### `{c}`  (group {GROUP[c]}, identity {ident[c]})\n")
        if d.get("TERMINATOR") == "CENSORED":
            out.append("**CENSORED.** The outer `timeout 900` killed the leg. There is "
                       "NO ROW, and `RULE.md` §3.1 forbids filling one later or "
                       "reconstructing it. No counter, no flag, no chain of death.\n")
            continue
        out.append(f"* **TERMINATOR** `{d['TERMINATOR']}` — {d['WHY']}")
        out.append(f"* **EVIDENCE** `{d['EVIDENCE']}`")
        out.append(f"* **CHAIN OF DEATH** `{d['CHAIN']}`")
        for f in d["flags"]:
            out.append(f"* **{f}**")
        out.append(f"* **CONSENSUS** {d['CONSENSUS']}")
        out.append(f"* **LANE OBJECTIVES** `{d['LANE_OBJECTIVES']}`")
        out.append(f"* **CLOSENESS (objective)** `{d['CLOSENESS_OBJECTIVE']}`")
        out.append(f"* **CLOSENESS (feasibility)** `{d['CLOSENESS_FEASIBILITY']}`")
        out.append(f"* **COST vs SNOPT** {d['COST_VS_SNOPT']}")
        out.append(f"* **LEDGER `QpStatus` SEQUENCE** (MAIN WALK ENGINE ONLY — "
                   f"`RULE.md` §2.2; the restoration sub-driver and the lazy engines "
                   f"are never handed the ledger, so this is a LOWER BOUND on the "
                   f"solve's QP solves)\n\n      {d['QP_STATUS_SEQUENCE']}\n")
        out.append(f"* **ledger print cap** `{d['LEDGER_PRINT_CAP']}`\n")

    out.append(FINDINGS.replace("__JUDGED_SPLIT__", judged_split(cls)))
    out.append(RECOMMENDATION)
    out.append(NONFINITE)
    out.append("\n---\n\n*Generated by `wrappers/census_table.py`. "
               "`RULE.md` governs; it is not edited. "
               "No outcome in this file licenses a behaviour change.*\n")

    dest = os.path.join(art, "tables/census.md")
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with open(dest, "w") as fh:
        fh.write("\n".join(out))
    print(f"WROTE {dest}  ({len(ORDER)} cells)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
