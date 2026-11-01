# M7 W2 M1 — the QP's minor cap, counted apart and given two levers, measured on the CUTEst TRAIN 23

**THE READING, IN ONE LINE: the failure budget SHIPS ON AT 3 and the cap retry
SHIPS OFF.** The budget arm reaches one more `optimal` (`READING6`), introduces
no new error, and moves no cell outside the nine the A-SQP read's §M1 names. The
retry arm rescues nothing, roughly doubles the minor spend on every cell whose
subproblems reach the cap, and moves a cell OUTSIDE that list (`ZAMB2-8`, an
already-optimal cell), which is the clause it fails. `tables.txt` applies the
rule; the lines here restate its output, they are not a second judgement.
**The hs TRAIN split was read too** (§7a, added on review): all 21 cells are
bit-identical between the two arms, so the second TRAIN family neither supports
nor opposes the budget -- it simply never reaches the gate.

## 1. What is measured

Three things land together at M7 W2 M1 and only the last two are measured here.

**The label half (S2/S3), no toggle.** `SqpCounters` gains five fields that say
what a failed subproblem failed OF and what the driver did about it:
`qp_maxiter_exits`, `qp_numerical_exits`, `qp_failures_routed`,
`qp_failures_in_a_row_peak` and `qp_cap_retries`. Before them, a subproblem that
ran out of its minor budget and one whose factorization broke were **one number**
in every consumer column: `SqpSolver::map_status` folds `QpStatus::kMaxIter` and
`QpStatus::kNumericalError` into the same `SolveStatus::kNumericalError`, and
`SqpCounters::rejected_steps` conflates a routed failure with a strategy
rejection by design.

**Lever 1 — the consecutive-failure budget.** `SqpOptions::qp_failure_budget`
(Index; **1** at the HEAD these arms were built from, **3** as shipped after this
reading — §7). The routing gate was `qp_failures_in_a_row == 0` and is now
`qp_failures_in_a_row < qp_failure_budget`: at 1 the first failure at an iterate
shrinks the radius and retries and the second ends the solve, which is exactly
today.

**Lever 2 — the cap retry.** `SqpOptions::qp_cap_retry` (bool, default false,
and it stays false — §7).
A walk subproblem that exits `kMaxIter` with a usable iterate is re-solved ONCE,
same subproblem, same radius, warm from the capped solve's own working set, with
an explicit `QpOptions::max_iter` of twice the cap that solve resolved.

The mechanism both levers address is the A-SQP read's §M1
(`docs/notes/2026-09-m7-w2-sqp-cutest-read.md`): nine of the 23 TRAIN cells exit
`numerical_error` through one gate, the two fatal majors spending the IDENTICAL
minor count — `ACOPP57` 1 795 / 1 795, `ACOPP118` 4 710 / 4 710, `BRITGAS`
4 500 / 4 500, `READING6` 770 / 770 — which is what says a BUDGET and not the
arithmetic stopped them.

The exit criterion is the M7 brief's: a measured win over the lever's own OFF
state on the **TRAIN** split, judged **per lever, on its own arm**. The holdout
is not run.

## 2. The four arms, and how they differ

The harness's cell binary builds its options from a small label table and has
**no way to set a field its pin does not know about**. The harness pin is
`3904a477` and these fields are newer, so the harness tree was **not modified**:
each arm is an hven install whose *shipped defaults* for the two fields differ,
with the harness's own unmodified cell source configured **out of tree** against
it (M7 W2 M4's construction, `wrappers/build_arm.sh`).

| arm | `qp_failure_budget` | `qp_cap_retry` | what it answers |
|---|---|---|---|
| **OFF** | 1 | false | today's engine — and the identity check against M4's own OFF arm |
| **budget** | 3 | false | lever 1 alone |
| **retry** | 1 | true | lever 2 alone |
| **both** | 3 | true | the CONFIRMATION, never the evidence for either lever |

**THE PIN STAMP IN THESE BINARIES IS FALSE AND IS NEVER QUOTED**, for M4's
reason: `cmake/hven_pin.cmake` sets `HVEN_PIN_SHA` unconditionally, so every row
carries `hven_pin_sha=3904a477…`, describing an hven these binaries were not
linked against. The true identity of the hven under test is in §4.
`-DEIGEN_MAX_ALIGN_BYTES=32` is passed to every harness TU consuming the
installed package.

**OFF IS RUN ONCE, AND IT IS A FREE IDENTITY CHECK.** This task's counters are
additive and both levers are at their OFF state, so the OFF arm must reproduce
M7 W2 M4's committed OFF arm
(`docs/notes/data/2026-09-m7-w2-m4-start-point/cells-off-r1.csv`) on every column
that existed there. §5 is that check.

## 3. The protocol these rows were taken under

- **Serial, one cell, one process**, `MKL_NUM_THREADS=1` and `OMP_NUM_THREADS=1`
  in every cell process, the arm holding the shared box lock
  (`/tmp/box-build.lock`) for its whole duration — the same granularity another
  lane's sweep uses on this box. Not a co-run.
- **No timeout and no fallback on the lock.** The wrapper queues.
- **A foreign-cell guard inside the lock**, by process NAME resolved through
  `/proc/<pid>/exe`, so a cell running outside the lock aborts the arm (the
  runner's own text contains the string — the declared self-match).
- **The wall ceiling is 300 s per cell** (`sets/cutest-nlp.json`'s
  `run_terms.budget_wall_s`), applied to the CELL and never to the lock wait;
  `--budget-iters 0`, so the engine's own `max_iter = 100` is the iteration cap.
- **Wall is informational and is never asserted** (CLAUDE.md §7). `dnf_wall` is
  the one wall-DEPENDENT outcome here; a cell that changes only between
  `dnf_wall` and something else is reported as CENSORED rather than as a moved
  counter (M7 W2 M4 fix1's rule, applied by `wrappers/make_tables.py` rather than
  restated).

## 4. Provenance

`PROVENANCE.txt` beside this file, verbatim from `wrappers/provenance.sh`
(CLAUDE.md §7: toolchain, hardware, date, commit). It records each arm's
installed `sqp_solver_types.h` sha256 and the two default literals it carries,
and each arm's cell-binary sha256.

## 5. The OFF identity check

**Columns that existed in M7 W2 M4's committed OFF arm and differ here: 0.**
`tables.txt` §"OFF identity" is that comparison, cell by cell and column by
column, against
`docs/notes/data/2026-09-m7-w2-m4-start-point/cells-off-r1.csv`. The five new
counters are additive and both levers are at their OFF state, so this is the
evidence that the counters-only commit changed no behaviour AT THE HARNESS LEVEL
as well as at the object level (the object-level proof is the P-SYM
classification in `gates/`).

The OFF arm itself: **2 `optimal`** (`SMBANK`, `ZAMB2-8`), **12 `error`**,
**8 `dnf_wall`**, **1 `infeasible`** (`YORKNET`, uncertified) — the A-SQP read's
own split, reproduced.

## 6. The three arms against OFF

Each arm was run TWICE. **Run-A against run-B: 0 field differences, in all three
arms** (`tables.txt`, the "repeat agreement" sections) — every asserted column,
not merely the statuses. The measurement is deterministic at
`MKL_NUM_THREADS=1`, so a difference between arms is the lever and nothing else.

### The budget arm (`qp_failure_budget = 3`)

| | OFF | budget |
|---|---|---|
| `optimal` | 2 | **3** |
| `error` | 12 | 11 |
| `dnf_wall` | 8 | 8 |
| `infeasible` | 1 | 1 |
| cells moved | — | **9, and they are exactly the nine M1 cells** |

`READING6` is the cell that turns: `numerical_error` at `f = 0` (a stationary
point of the objective, 2.04 infeasible) becomes **`optimal` at
`f = -144.6597005728`**, 13 majors and 6 837 factorizations against 3 and 3 313,
with `kkt_primal_eq` 2.04 → 2.8e-13. The other three lanes' answer for
`READING6` is −144.6597 (the read's row), so the budget did not merely relabel a
failure — it reached the answer the other lanes reach.

The eight M1 cells that do NOT turn all pay for the extra majors and keep their
status: `ACOPP118` 11 233 → 20 501 factorizations, `ACOPP57` 114 → 128,
`ACOPR57` 4 159 → 4 848, `DALLASM` 4 917 → 4 919, `STEENBRB` 16 582 → 25 973,
`STEENBRC` 32 137 → 42 946, `STEENBRG` 32 291 → 43 096, and `BRITGAS`
**9 265 → 93 749**. Their objectives are unchanged except `BRITGAS`'s, which
moves off `x0` for the first time (`f` 0 → 0.0755, one elastic activation, 7
restoration iterations) and still ends `numerical_error`.

**THE ONE COST FINDING, AND IT IS NOT A RULE VIOLATION.** `BRITGAS` spends
**275.4 s** of the 300 s cell ceiling under the budget arm (22.3 s at OFF), twice
over (275.4 s and 273.7 s). It is still `error` and the `dnf_wall` set is
unchanged, so the rule is satisfied on this box — but the margin is 8 %, and on
a slower machine, or under a co-run, `BRITGAS` would become `dnf_wall` at the
shipped default. Wall is informational here (CLAUDE.md §7) and this sentence is
the caveat it is owed, not a measurement.

The three `error` cells the read attributes to M4 rather than M1 — `NET3`,
`STEENBRD`, `STEENBRE`, the cells whose declared `x0` is outside their own box
and which never reach a subproblem — are **bit-identical** under the budget, as
are the two `optimal` cells, the `infeasible` cell and all eight wall cells. The
lever touches exactly the cells that fail a subproblem.

### The retry arm (`qp_cap_retry = true`)

| | OFF | retry |
|---|---|---|
| `optimal` | 2 | 2 |
| `error` | 12 | 12 |
| `dnf_wall` | 8 | 8 |
| cells moved | — | 9 (eight M1 cells) **+ `ZAMB2-8`, which is not one** |

**No cell's status changes.** The doubled cap is spent and the same subproblems
fail: `ACOPP118` 11 233 → 29 921 factorizations, `BRITGAS` 9 265 → 27 429,
`READING6` 3 313 → 9 395, `STEENBRB` 16 582 → 35 339, `STEENBRC` 32 137 →
52 207, `STEENBRG` 32 291 → 52 275 — all still `numerical_error`. Two cells end
at a different point (`ACOPR57` `f` 41 737.7849 → 41 737.7602; `STEENBRC` and
`STEENBRG` fall from 1.83e14 to 6.33e13, still nine orders above the answer), so
the retried step is not always the capped one — it is simply never good enough to
change the outcome.

**THE CLAUSE IT FAILS, WITH ITS MECHANISM.** `ZAMB2-8` is `optimal` at OFF and
`optimal` under the retry, at the SAME objective to the last digit
(−0.15293592733713887), the same 11 majors, the same accepted steps, the same
restoration iterations — **every trajectory column bit-identical** — while its
QP minors go **6 666 → 12 180** and its factorizations **8 803 → 17 055** (wall
24.9 s → 68.2 s, informational). A capped-but-usable subproblem on this cell was
already delivering the step the walk went on to take; the retry re-derives that
same step at twice the cap and hands back a trajectory it cannot be
distinguished from. That is the lever's cost with none of its benefit, on a cell
that was never failing — the "moves no other cell" clause exists for exactly
this, and the retry fails it.

### The both arm (confirmation only)

`both` reproduces the budget arm's `READING6` rescue and the retry arm's
`ZAMB2-8` cost, and adds one CENSORED transition: **`BRITGAS` `error` →
`dnf_wall`**, twice (300.0 s and 300.1 s). Its mechanism is the two costs
multiplying — the budget's four majors, each of whose capped subproblems is now
re-solved at twice the cap — on the cell the budget arm already left 8 % short of
the ceiling. It is a DNF CONDITION and not a new error (§3), it is reported here
rather than counted, and it is not adjudicated by a solo re-run because the arm
that produced it does not ship.

## 7. The defaults, by the brief's rule

The rule, applied per lever on its OWN arm: **ON iff the arm reaches at least the
same `optimal` count, introduces no new error in the counter currency, and moves
no cell outside the M1 list.** `wrappers/make_tables.py` applies it; the verdict
lines in `tables.txt` are its output.

| lever | optimal | new errors | other cells moved | **verdict** |
|---|---|---|---|---|
| `qp_failure_budget` | 2 → **3** (OK) | 0 (OK) | 0 (OK) | **ON — ships at 3** |
| `qp_cap_retry` | 2 → 2 (OK) | 0 (OK) | **1, `ZAMB2-8`** (FAILS) | **OFF — ships false** |
| both | 2 → 3 (OK) | 0 (OK) | 1 (FAILS) | confirmation only |

**The shipped defaults are `qp_failure_budget = 3` and `qp_cap_retry = false`.**

### 7a. The hs TRAIN split, read because the in-tree HS15 sweep moved

Shipping the default broke four in-tree pins (below), and one of them --
`HsSweepCorpus.Corpus` -- is an hs problem. The A0 contract's TRAIN read covers
BOTH TRAIN families, so the hs half was measured before the default shipped,
through this same rig: `sets/hs-27.json`'s **21 TRAIN cells** under `split-v1`,
the OFF arm and the budget arm, one run each, alone under the box lock.

**Every hs TRAIN cell is bit-identical between the arms, and all 21 are
`optimal` on both** (`hs/table.txt`, `hs/cells-hs-{off,budget}.csv`). The reason
is mechanical and worth stating: these cells solve from a COLD start at default
options and never route a subproblem failure, so the gate the budget widens is
never reached. The in-tree sweep's HS15 row moves because it is a **warm** arm
of a continuation sweep, which does route one. The hs family therefore clears
the rule's bar -- same optimal count, no new error -- without evidence for the
lever: the CUTEst read is what carries it.

### 7b. What shipping the default re-derived, declared

Shipping a non-OFF default was expected to move trajectories, and the U0 27-cell
replay was budgeted for a re-derivation. **It did not need one.** `replay/` here
is the replay at the OFF-default HEAD and `replay-shipped/` is the same three
arms re-run at the shipped defaults; `replay-shipped/comparison.txt` compares the
latter BOTH against the M6 close pin and against the former, and every one of the
six comparisons is **27 cells / 75 columns / 0 differences**. No U0 cell routes a
subproblem failure either, which is the same reason the hs split does not move.

**Both replay sets were RE-TAKEN at fix round 1, and nothing they say changed.**
The CSVs the first take committed carried a false `binary:` stamp: that field is
resolved at CONFIGURE time (`bench/CMakeLists.txt:81-95`, whose own comment names
the trap), and the legs reused a build directory configured while the M1 edits
were still uncommitted — so the CSVs said `-dirty` about binaries that had in
fact run at a clean, committed tree. A stale stamp is re-taken, not waived, so
each set was run again from a build directory removed and re-configured inside
the leg: `replay/` from a git worktree detached at the OFF-default head
`4d27fb02`, `replay-shipped/` from the main tree at `9400690e`, whose library
bytes are the shipped commit's (`git diff --name-only 3f2f6e35 9400690e` touches
`docs/notes/**` only, and the clean build's `libhven.a` sha256 equals the
superseded leg's own). Each leg's verbatim stdout is beside its CSVs as
`wrapper-stdout.txt`, dirty-file count and all, and both reproduce 27/75/0 ×3
against the M6 close pin. `replay-retake-fix1.txt` is the machine-written
comparison against the superseded bytes: **75 of 76 data columns identical in
every one of the 27 × 3 × 2 rows**, the single mover being `wall_s`, which is
informational (CLAUDE.md §7) and asserted nowhere here. `comparison.txt` is the
superseded leg's own output and is kept as it was written; its §B is re-derived
on the re-taken bytes in `replay-retake-fix1.txt` §3, with the same answer.

What DID break is four in-tree pins, each declared here and in the commit that
carries it (CLAUDE.md §7):

1. **`HsSweepCorpus.Corpus`, re-derived.** HS15's warm arm 21/28 → **23/30**
   full-step majors / majors, and the corpus total **3719 → 3737** minors — all
   +18 of it HS15's warm arm (92 → 110). Nothing else in the corpus moves, which
   is the control saying this is the routing gate and not a trajectory change.
2. **That test's Accelerate arm is UNOBSERVED and was NOT adjusted.** It stays at
   3717 with a comment saying 3735 would be the expected +18 and that only an
   observation on real Mac hardware may write it (CLAUDE.md §6: never
   interpolate an Apple value).
3. **`Continuation.ProbeBudgetBoundsAFailingProposal` writes the gate out at 1.**
   At 3 its failing proposal costs 5 majors / 2002 minors rather than the pinned
   3 / 1002, and its M6-vs-OFF minor comparisons flip. The fixture is about the
   PROBE budget, so it pins the routing gate it was measured against — the same
   escape-hatch move M6 W6's own marked correction makes one line above it.
4. **The three `SqpDriverQpFailure` fixtures do the same**, for the same reason:
   they pin the ONE-retry mechanism, not the default. A new fixture beside them
   leaves the field defaulted, and on the same model the raised budget does not
   merely exit three rows later — the solve reaches `kOptimal`.

The cap retry stays in the tree at `false`. It is a two-line opt-in with a
counter (`qp_cap_retries`) and a trace site (`qp.mode` `site = "cap_retry"`)
behind it, and the evidence it should not be the default is in this directory;
turning it on without new evidence would be a change of this reading, not a
tuning choice.

### 7c. What else fix round 1 changed, and the gate that says it changed nothing

Two wording corrections in the tree, no token of code (review LOW-2, LOW-4): the
cap retry's row convention, which `tests/sqp/test_qp_cap_levers.cpp` had
INVERTED — the retry replaces `mj.qs` before the row is written, so the ROW
carries the retry's minors and the capped attempt's live only in the aggregate —
and "capped subproblem ATTEMPTS this solve made", which
`include/hven/core/solver_counters.h` and `docs/trace-schema-v0.md` now scope to
"the capped subproblem exits that reached the routing site", the SOC re-solve and
the elastic rungs reaching neither field. A third commit drops three includes
`test_qp_cap_levers.cpp` does not use.

`gates-fix1/` is the gate for all three, P-SYM in both configs twice over: the
comment-only round leaves every library object and every other TU byte-identical
(the one object that moves is the edited test TU, and its 143 changed
instructions are the gtest line-number immediates the inserted comment lines
shift, `+2` twice and `+6` 141 times); the include round moves that same TU and
nothing else, by `-3` 250 times. The Release suite is **2656 / 2656** after each.
No measurement in this directory is touched by any of it.

## 8. Files

| file | what |
|---|---|
| `cells-off.csv` | the OFF arm, 23 cells, run once |
| `cells-{budget,retry,both}-r{1,2}.csv` | the three arms, twice each |
| `tables.txt` | the identity check, the repeat agreement, the outcome tables, the per-lever verdicts |
| `raw/<tag>/<PROBLEM>.txt` | every cell's own stdout, verbatim, with its wall and rc |
| `gates/` | both suites, the long test alone, the replay, the smokes, the export line, and the P-SYM classification in both configs — its first line is the wrapper's own `=== HEAD … dirty-files=0 ===`, recovered verbatim from the wrapper log at fix round 1 |
| `replay/head-{walk,ssn,ipm}.csv` | the U0 replay at the OFF-default HEAD (27/75/0 against the M6 close CSVs), re-taken clean at fix round 1 |
| `replay-shipped/` | the U0 replay re-run at the SHIPPED defaults, re-taken clean at fix round 1, and `comparison.txt`, the six comparisons that came back 27/75/0 |
| `replay{,-shipped}/wrapper-stdout.txt` | the re-take legs' VERBATIM stdout — the `=== HEAD … dirty-files=0 ===` line, the build's own sha256s, and the leg's own 27/75/0 ×3 |
| `replay-retake-fix1.txt` | why the two sets were re-taken, and the re-taken bytes against the superseded ones, column by column |
| `gates-fix1/` | fix round 1's own gate: P-SYM in both configs for the comment-only round and again for the include round, each leg's verbatim wrapper stdout, and the Release suite after each |
| `hs/` | the hs TRAIN split (21 cells), both arms, with `table.txt` and every cell's raw stdout |
| `gates-shipped/` | the same gates re-run at the SHIPPED commit `3f2f6e35`, including the P-SYM classification, which is identical in both configs |
| `PROVENANCE.txt` | toolchain, hardware, date, commit, and each arm's header sha256 + its two default literals |
| `wrappers/` | every script that produced the above |
