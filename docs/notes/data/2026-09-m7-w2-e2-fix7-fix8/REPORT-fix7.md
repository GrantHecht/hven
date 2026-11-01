> Copied verbatim from `.superpowers/` (source: the round's own report/review) on 2026-10-01; nothing below this preamble is edited.

# M7 W2 E2 — FIX ROUND 7: the un-run dependency test, and the screen that skipped the repair pin

Implementer report, 2026-09-27. Branch `e2fix7` off `84fb520e` (the E4 head; sources ==
m7 `e212a308` == the RETAKE9 pin `ff02789b`). Worktree
`/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9`, scratch
`.scratch/e2fix7/` (`STATE.md` current). hven's main tree and hven-bench were never
written. `.scratch/e4-fix5/` was read only.

## 0. The one-paragraph answer

**E4-37(b)'s defect is real and is fixed — and it is NOT what costs ACOPR118 its
`optimal` row.** The un-run test (`ws_independence.cpp:206-209`) is fixed at the cause:
a test that made no solve is no longer read as dependent. Landing that ALONE leaves all
four killing sub-QPs bit-identical — `kNumericalError` at 2-3 minors, both algebras
(MEASURED). Diagnosing one level further found the actual cause, in the same E2 lane, one
call away: **`repair_temporary_vertex` lets the INTEGER SCREEN skip candidate pins, and on
ACOPR118's majors 17-20 the screen fires after 70 pins and skips the remaining 245 — five
pins short of the one that repairs the inertia.** The screen proves the FAMILY
`{A_W, e_i}` dependent, not `e_i ∈ span(A_W)`; the two coincide only for an independent
`A_W`, which a rank-deficient equality Jacobian denies. With the screen no longer deciding
there, **all four dumps reach `kOptimal` in BOTH algebras**, at the objective the
option-off walk reaches, stationarity 1.7e-14 … 4.4e-14 — and on one of them the ON walk
now beats the OFF walk, which spends its whole budget. Both rules are landed in ONE commit,
separable: rule A is `ws_independence.cpp`'s four lines, rule B is
`qp_engine.cpp`'s repair block. The battery moves three things and nothing else: 3 corpus
dumps per algebra in dep_* honesty columns only (rule A), one HS counter pin +2 minors
(rule B, HS15's cold arm, everything else on that row identical), and one E2 correctness
fixture's counter ROUTE (rule A) with every correctness statement in it unchanged. U0's
declared 43-difference set is byte-identical cell by cell; the census is 120/120; both
suites are 0 failures.

## 1. The diagnosis, with the line

All OBSERVED, Release, `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, `taskset -c 2`, on
`tests/qp/fixtures/e2-d4/ACOPR118_e4_qp5.txt` (E4 fix5's dump of ACOPR118's major-17
sub-QP). The instrument was a scratch `fprintf`/`getenv` patch on `src/qp/qp_engine.cpp`
driven by a temporary gtest inside `hven_qp_walk_seam_tests`. **It was NEVER committed**
(library code may not read `getenv` here); it is kept at
`.scratch/e2fix7/qp_engine.cpp.probe` and the transcripts at `.scratch/e2fix7/logs/`.

### 1a. Which path returned kUndecidable having computed nothing

`src/qp/ws_independence.cpp`, `dependency_test_to`, has exactly THREE paths that can return
`kUndecidable`:

| line | condition | `solves` | `rho` |
|---|---|---|---|
| **206-209** | `!f.usable()` and the integer screen silent | **0** | the NaN default |
| 276-277 | a STALLED refinement with `rho < kDepUndecidable` | ≥ 1 | computed |
| 278-279 | `isnan(rho)` after a solve | ≥ 1 | NaN from the arithmetic |

fix5 OBSERVED `solves = 0`, `rho = NaN`, `screen = 0`, so only line 206-209 can have fired.
The probe confirms it DIRECTLY, in BOTH algebras, at minor 0 (`counters.minor_iters == 1`):

```
border      PROBE minor=1 rows=9 pins=29 view_elim=0 view_schur=0 face_holds=0
                  face_factorized=0 border_has=1 border_needsref=0 border_holds=0
refactorize PROBE minor=1 rows=9 pins=29 view_elim=0 view_schur=0 face_holds=0
                  face_factorized=1 ... bs_equal=0 rows_equal=1 face_rows=9 ws_rows=9
            PROBE-DEFER minor=1 is_ineq=0 idx=291 moves=1 alpha=5.405382e-01
                  verdict=2 rho=nan solves=0 screen=0 stall=0 usable=0
```

`pass_view` (built at `src/qp/qp_engine.cpp:1015` from `QpEngine::dep_factor_view`) is
**unusable at the first pass and usable at every later one.**

### 1b. WHY the pass held no factor: a FAILED start repair leaves a stale one

On the refactorize arm the probe shows the face's captured `session_id`, `epoch` and
`rows` all MATCHING `kkt` and `ws`, and only `bound_state` differing — in **70** indices,
every one of them `face = kAtLower` against `ws = kFree`, with `counters.factorizations`
already at 73 on that first pass. That is `repair_temporary_vertex`
(`src/qp/qp_engine.cpp:3760`…): the first pass read `kWrong`, `reread_wrong_inertia` did not
overturn it (2 re-reads, 0 flips), and the repair then pinned 70 variables, each
`probe_inertia` re-factorizing and re-CAPTURING `face`, without ever repairing the inertia.
It took its failure branch:

```cpp
    if (verdict != detail::InertiaVerdict::kOk) {
        ws.bound_state() = saved_state;
        x = saved_x;
        return false;
    }
```

`ws` and `x` are restored; **`face` and the border stack are not** (the comment there says
"the border stack is left alone: `sync_borders` reconciles it against ws unconditionally on
the next iteration" — but on a `false` return the walk does NOT `continue`, it falls through
into the SAME pass). So the add site of that very pass is reached with no factor, and pin
291 — the blocker at `alpha = 0.540538`, a MOVING step — gets a test that cannot run.

### 1c. What the E2 contract INTENDED for that case

Settler **S1** named this miss class explicitly — "factor view not matching the working set
at that pass — after an add there is no factor for the grown set" — and required each class
to be "fixed or counted as `dep_untested_growth` … **never counted as 'independent'**". The
add site's own comment did treat it as a case of `kUndecidable` ("no factor for this pass's
set, or a stalled refinement"), and settler **S17** accepted the deferred add on the ground
that "**admission is the pre-E2 behaviour**". Those two together are the intent: with
nothing computed, the blocker is ADMITTED, and no arithmetic claim is made about it. What
the code did instead was let `DepTest::dependent()` — which counts `kUndecidable` as
dependent — charge the blocker the deferred-add path: a re-check pass, a
`kPtKnownDependent` mark if that re-check removed it, and `dep_deferred_adds` /
`dep_undecidable` in the ledger, all on the strength of a `rho` that was never computed.
E4-37(b)'s wording is the same reading: "a dependency test that computed nothing (rho NaN,
solves 0, screen 0) must not be read as dependent".

### 1d. Rule A alone does NOT recover the cell — MEASURED

With rule A landed and nothing else, `qp5`…`qp8` are **bit-identical to the base** in
status, minors, factorizations, objective and KKT, in both algebras. What changes is only
the ledger's honesty: `dep_deferred_adds` 1 → 0, `dep_undecidable` 1 → 0,
`dep_unrun_tests` 0 → 1. The walk still dies at minor 3 on the same `kWrong` verdict.

### 1e. The TRUE cause: the integer screen skipping repair pins

Instrumenting the repair itself, per candidate, over its whole 315-candidate `order`:

| arm | pins tried | skipped | why skipped | outcome |
|---|---|---|---|---|
| E2 **on** | 70 | **245** | the INTEGER SCREEN, short-circuited before any test (`dep_pin_skips` 245 with `dep_dependent` 0) | verdict still `kWrong` → undo → `return false` |
| E2 **off** | **75** | 0 | — | at pin 75 (candidate 172) `probe_inertia` reads **kOk** → the repair SUCCEEDS |

70 + 245 = 315 exactly. The screen is `me + |W| + 1 > n`, i.e.
`236 + 9 + (29 + k) + 1 > 344`, which turns true at `k = 70` — so the repair aborts five
pins before the one that works. The E2-off walk's minor 0 is a `kStartRepair` record and
every later pass reads `kOk`; the E2-on walk's minor 0 is an ordinary `kAdd` on an
unrepaired, indefinite face and every pass reads `kWrong`.

**The screen's premise is false here, and the falsity is measured.** `me + |W| + 1 > n`
proves that `{A_W rows, e_i}` is a dependent FAMILY. The repair needs
`e_i ∈ span(A_W)` ("a pin dependent on the set leaves `null(A_W)`, and so the inertia,
unchanged"), and family dependence gives that only when `A_W` is itself independent. At the
repairing pin `me + |W| = 236 + 9 + 104 = 349 > n = 344`, so `A_W` is NECESSARILY
rank-deficient — and `probe_inertia` reads `kOk` there, i.e. that pin DID change the
inertia. (The `Q1` finding of M5 — rank deficit in the MANDATORY equality rows — is the
structural reason no working-set rule can restore the premise.) At the ADD site the
screen's reading is the SAFE direction; in the repair it is the unsafe one, because a
skipped pin that would have cut the negative curvature costs the solve.

## 2. The rule as landed — two rules, one commit, separable

### 2a. Rule A — `src/qp/ws_independence.cpp` and `include/hven/detail/qp/ws_independence.h`

```cpp
    if (!f.usable()) {
        // AN UN-RUN TEST IS NOT A VERDICT OF DEPENDENCE (E2 fix7, settler
        // E4-37(b)). There is no factor for this pass's set, so nothing was
        // computed: rho stays at its NaN default and `solves` at 0. The
        // integer screen is the only statement available, and it is exact --
        // positive it PROVES dependence; negative it proves nothing, and the
        // candidate is admitted as the pre-E2 walk admits it (S17). `unrun`
        // says the verdict rests on the screen alone so the caller can count
        // it (QpCounters::dep_unrun_tests) and so no reader mistakes the NaN
        // rho for a measurement. R-A still runs at every growth site: an
        // exact structural dependency needs no factor and overrules this.
        t.verdict = t.screen ? DepVerdict::kDependent : DepVerdict::kIndependent;
        t.unrun = true;
        return t;
    }
```

`DepTest` gains `bool unrun = false;` with the same statement in three lines. The
`ws_independence.h` contract block gains a fourth verdict bullet (the header text is in the
file; it states the rule, that `rho` stays absent and `solves` 0, that it is counted
`dep_unrun_tests`, that R-A still overrules it, and what the pre-fix7 behaviour was).

**Why kIndependent and not "run it".** E4-37(b) offers both. Running it is not available:
there is no factor to run it ON, and manufacturing one is a factorization the pass has
not paid for — the very cost the `DepFactorView` design exists to avoid. So the second
branch of the ruling applies, and it is also S17's pre-E2 admission. `rho` is deliberately
left NaN rather than zero-filled: the repository's ABSENT-not-zero discipline, and NaN also
makes R-E's grey-band re-read (`t.rho < kDepUndecidable`) false by construction, so no
downstream reader treats it as a measurement.

**Counted `QpCounters::dep_unrun_tests`** (new; the brief's suggested name). It is
incremented in `QpEngine::test_candidate` AFTER R-A, so a candidate R-A decided EXACTLY off
the row is not counted — the counter is "verdicts the integer screen alone carries", not
"passes that held no factor". The `solver_counters.h` comment says exactly that.

### 2b. Rule B — `src/qp/qp_engine.cpp`, `repair_temporary_vertex`

```cpp
            bool certified_dependent = false;
            if (screen) {
                ++counters.dep_repair_screen_pins;
                certified_dependent = detail::structural_dependency(qp, ws, /*is_ineq=*/false, i)
                                          .found; // exact, no factor, no screen
            } else if (view.usable()) {
                certified_dependent =
                    test_candidate(qp, ws, view, /*is_ineq=*/false, i, counters, opts).dependent();
            }
            if (certified_dependent) {
                ++counters.dep_pin_skips;
                continue;
            }
```

A screen-POSITIVE candidate is now decided by the EXACT structural dependency alone — which
needs neither a factor nor the screen — and is otherwise TRIED, counted
`QpCounters::dep_repair_screen_pins` (new). A screen-NEGATIVE candidate keeps **exactly**
the old code path: `view.usable()` then `test_candidate(...).dependent()`, whose own
`screen` flag is false there, so its verdict is the measured `rho`'s. The screen is still
read, for its telemetry and for the pre-existing
`dep_screen_under_mandatory_dependence` count.

The `qp_engine.h` Degeneracy contract gains one paragraph per rule; both are in the file
(`E2 FIX7` and `E2 FIX7, SECOND RULE`). The second one states the family-vs-membership
distinction, that the two coincide only for an independent `A_W`, that the add site's
reading is the safe direction and the repair's is not, what decides instead, the counter,
and the ACOPR118 measurement. The add site's `kUndecidable` comment is corrected: a test
with no factor no longer reaches it.

### 2c. To revert rule B alone

`src/qp/qp_engine.cpp`'s repair block above becomes
`if (screen || (view.usable() && test_candidate(...).dependent()))`, its contract paragraph
and `dep_repair_screen_pins` go, and the HS pin returns to 3340. Nothing else in the commit
depends on it.

## 3. Fixtures, with values per configuration

All in `tests/qp/test_qp_unrun_dependency_test.cpp` (the walk-seam target
`hven_qp_walk_seam_tests`, `HVEN_TESTING` target-wide, docs/testing.md's convention) except
the unit-level contract, which is in `tests/sqp/test_qp_ws_independence.cpp` beside E2's own
`WsIndependenceCalibration` tests.

### 3a. ACOPR118 majors 17 and 18 — `tests/qp/fixtures/e2-d4/ACOPR118_e4_qp{5,6}.txt`

New fixture files (E2-D4 `D4QP 1`, 163 KiB each; fix5's dumps verbatim). One test per
(dump, algebra), four in all. Each asserts, before anything else, the dump's own
`status=3` premise; then `dep_repair_screen_pins >= 1` (rule B fired),
`dep_deferred_adds == 0` and `dep_undecidable == 0`, at least five recorded minors with no
`kUnset` action, `records[0].action == kStartRepair` with `ws_size_out > ws_size_in`,
`records[1].inertia_verdict == kOk`, then `kOptimal`, the objective within 1e-6 relative of
the option-off reference, and the KKT battery (eq/ineq 1e-5, box 1e-9, stationarity 1e-6,
sign 1e-9, complementarity 1e-6). Minor counts are PRINTED, never asserted — the reason
`test_qp_flat_face_saddle.cpp` already gives for the R2 dumps.

| dump | algebra | cfg | status | minors | facts | f | stationarity | screen_pins | Debug ms |
|---|---|---|---|---|---|---|---|---|---|
| qp5 | refactorize | Rel | kOptimal | 367 | 519 | −0.154376247286 | 3.808e-14 | 5 | — |
| qp5 | refactorize | Dbg | kOptimal | 367 | 519 | −0.154376252011 | 4.325e-14 | 5 | 2990 |
| qp5 | border | Rel | kOptimal | 397 | 21 | −0.154376253941 | 2.469e-14 | 5 | — |
| qp5 | border | Dbg | kOptimal | 396 | 21 | −0.154376253948 | 3.523e-14 | 5 | 76131 |
| qp6 | refactorize | Rel | kOptimal | 353 | 503 | +0.0395282055522 | 1.697e-14 | 5 | — |
| qp6 | refactorize | Dbg | kOptimal | 353 | 503 | +0.0395282102954 | 3.595e-14 | 5 | 2841 |
| qp6 | border | Rel | kOptimal | 416 | 18 | +0.0395282103146 | 2.095e-14 | 5 | — |
| qp6 | border | Dbg | kOptimal | 416 | 18 | +0.0395282036387 | 5.125e-14 | 5 | 78325 |

Before this round all eight cells were `kNumericalError` at 2–3 minors with f +0.1054 (qp5)
/ +0.1054 (qp6). The references are the option-off walk's own answers on the same dumps:
qp5 −0.154376254013 (border) / −0.154376247309 (refactorize), qp6 +0.0395282055275
(refactorize) — and on **qp6's border arm the option-off walk does NOT finish** (kMaxIter at
its 1000-minor budget, f +0.0395282036), so there the ON walk now does better than the OFF
walk rather than matching it. That is why qp6 is kept beside qp5.

`screen_pins = 5` on all eight: exactly the five candidates between the old abort point and
the pin that repairs. The Debug worst case is 78.3 s, inside the 600 s per-(dump, algebra)
budget, so no split and no skip was needed. **qp7 and qp8 were measured and NOT landed**:
they are the same subproblem at trust radius 0.0625 and 0.03125, they behave identically in
class (both `kOptimal` both algebras — qp7 223/6 and 316/468 at f +0.108826520, qp8 283/8
and 365/517 at f +0.14955229), and they add 326 KiB of fixture and ~160 s of Debug for no
distinct statement. Their dumps stay in `.scratch/e2fix7/dumps/ACOPR118_e4/`.

### 3b. The synthetic — a MOVING blocker whose test would have been un-run

`QpE2Fix7Unrun.AMovingBlockerWithNoFactorIsAnOrdinaryAdd`, both algebras, 4 variables:

```
min  1/2 (x0^2 + x1^2 + x2^2) - 1/2 x3^2 + 1/2 (x0 + x1 + x2) + 2 x3
s.t. x3 <= 1,  -x3 <= 1,  -1/2 <= x0,x1,x2 <= 10,  x3 UNBOUNDED in the box
```

`x3` carries the negative curvature and has no finite box bound, so `pin_at_best_bound` can
never pin it: the first pass's repair pins `x0`, `x1`, `x2` in turn, each probe still
`kWrong`, the candidate list runs out, and the repair undoes itself — leaving `face` and the
border stack describing the last probed set. The same pass then reaches its add site with
`pass_view.usable()` false and a blocker (row 0) at a full unit of motion.

Asserted, identical in both algebras and both configurations: `dep_unrun_tests == 1`,
`dep_deferred_adds == 0`, `dep_undecidable == 0`; `records[0].action == kAdd` with
`block == kIneq`, `block_index == 0`, `dx_inf > step_tol` and `degenerate == false`; then
`kOptimal` at the **exact** optimum `x = (−1/2, −1/2, −1/2, −1)`, `f = −23/8 = −2.875`
(x3 sits at the ROW, not at a box bound, so it is a genuine active-set answer). 5 minors.

### 3c. The rule's own contract, at the unit level

Three tests appended to `tests/sqp/test_qp_ws_independence.cpp`:
`NoFactorAndASilentScreenIsINDEPENDENT` (4 variables, one working row, the candidate
EXACTLY equal to that row — truly dependent, and the test still may not say so because it
did not look: `kIndependent`, `unrun`, `isnan(rho)`, `solves == 0`, `!stall_admit`, and
`certificate_coefficients` gives the same answer);
`NoFactorAndAFiringScreenIsSTILLDEPENDENT` (me 2 + |W| 2 + 1 = 5 > n = 4, `num_free == 4` so
the no-free-column branch is not the one: `kDependent`, `screen`, `unrun`, NaN rho, 0
solves); and `AUsableFactorIsNeverAnUnrunTest` (both algebras: `!unrun`, `solves >= 1`,
`!isnan(rho)`, `kDependent`) as the guard against the flag leaking into the ordinary path.

### 3d. The existing E2 fixtures

Unchanged except one, whose COUNTER ROUTE moved — see §4c. No dump fixture was re-pinned;
no correctness assertion anywhere was weakened.

## 4. The battery — every mover declared, and attributed to ONE rule

Each mover below was attributed by running the two rules OFF ONE AT A TIME, through a
scratch `getenv` switch on the two source files that was reverted before the commit
(`.scratch/e2fix7/patch_off.py`; the reverted sources are byte-identical to the committed
ones — `grep getenv` over both returns nothing).

### 4a. The 51-dump corpus, both algebras, E2 ON — 3 rows per algebra, dep_* only

Base = `.scratch/e4/corpus/k3-*` (the E4 fix-3 row). That IS this base's walk: OBSERVED
`git diff 5ed7bf16 84fb520e -- src/ include/ bench/` touches `src/CMakeLists.txt` only (the
clang-only precompiled-header guard), so nothing between k3 and `84fb520e` moves a solve.
`python3 -B tools/rowcmp.py k3 f7`, all columns:

```
## border   (k3: 51 rows, missing [])  (f7: 51 rows, missing [])
  Hj_STEENBRD/qp4 [opt->opt]: sfirst: 0 -> 1; undec: 1 -> 0
  STEENBRB_def/qp0 [opt->opt]: undec: 1 -> 0; deferred: 1 -> 0
  STEENBRB_def/qp1 [num->num]: undec: 1 -> 0; deferred: 1 -> 0
## refactorize   (k3: 51 rows, missing [])  (f7: 51 rows, missing [])
  ... the same three rows, the same deltas
TOTAL ROWS WITH ANY COLUMN CHANGE: 6
```

**No status, minors, factorization, objective, eq/ineq/box, stationarity, sign or
complementarity column moves on any of the 102 rows.** All three are RULE A: the un-run test
stops being counted undecidable, and on `Hj_STEENBRD/qp4` R-A's exact structural reading then
decides it (`dep_structural_first` 0 → 1) — an EXACT verdict where there was none. RULE B
fires on no corpus dump (no screen-positive repair candidate occurs in the 51).

### 4b. The E2-OFF corpus — unchanged, and unchangeable by construction

Rows produced (`f7off-border-{old36,new15}`). Both rules sit inside
`independent_working_set`-gated code: `dependency_test` is reached only from
`QpEngine::test_candidate` (the add site, inside `if (eff_opts.independent_working_set)` at
`qp_engine.cpp:1774`; the repair pins; the ride add) and rule B's whole block is inside
`if (opts.independent_working_set)`. With the option off, `pass_view` is a default-constructed
`DepFactorView` and no dependency test runs at all.

### 4c. `QpE2WorkingSet.AFullyPinnedRowDecidesAnUndecidablePinExactly` — a counter ROUTE, re-pinned

RULE A. On the border arm the first pass holds no factor (that is the fixture's own premise),
so the pin's test no longer returns `kUndecidable`; R-A inside `test_candidate` applies the
same exact row reading ONE STEP EARLIER than the add site's D3(c) branch did. Re-pinned with
attribution in the fixture's own comment: `dep_undecidable` 1 → 0,
`dep_structural_exact` 1 → 0, `dep_structural_first` 0 → **1**. `dep_unrun_tests` stays **0**
in both arms — it is read AFTER R-A, and R-A did decide this one, so the admission rests on
no uncomputed rho (that is the counter's stated meaning and is now asserted here).
**Every correctness statement is asserted UNCHANGED**: `kInfeasible`,
`dep_infeasible_certificates == 1`, `dep_blocker_terminations == 0`,
`dep_deferred_adds == 0`, `dep_border_retreats == 0`, `zero_motion_cap_hits == 0`, the
certificate re-derives, `blocker == 2` and not an inequality, `minor_iters == 1`,
`y_e(0) == 1.0`, `implied == 0.5`, `limit == 0.0`.

### 4d. `HsSweepCorpus.Corpus` — `total_minors` 3340 → 3342, +2, all of it HS15's cold arm

RULE B, measured: with rule B off and rule A on the pin reads 3340; with rule B on and rule
A off it reads 3342. The base table was taken from the same binary through the scratch
switch; `diff` of the two 48-row tables is FOUR lines, all HS15 cold, minors 1067 → 1069 in
each KD/SOC cell:

```
< HS15   cold  on   on   7  86  1067  185  70/16  0/0/2/2  48/288  0  0  0  7/0/0  0
> HS15   cold  on   on   7  86  1069  185  70/16  0/0/2/2  48/288  0  0  0  7/0/0  0
```

Majors 86, factorizations 185, accepted/rejected 70/16, SOC 0/0/2/2, elastic 48/288,
restoration 0, watchdog 0, levels 7/0/0 and fail 0 all hold, and the other eleven
(problem, arm) groups are byte-identical. Two screen-positive repair pins that used to be
skipped are now tried, each costing its probe's minor. 22/3342 = 0.66 %, as the note quotes.
The Accelerate arm of the same pin (3717) is **UNOBSERVED** here and was NOT adjusted — no
Mac hardware ran, CLAUDE.md §6.

### 4e. Census T7, HS battery, B1Gate

`QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet` **OK — 120/120, no
cell lost, none newly certified.** The 39-test key filter
(`T7*` + `HsBattery.*` + `HsSweepCorpus.*` + `B1Gate.*`) is **39 PASSED, 0 failed**
(HsBattery 13/13, B1Gate 23/23).

### 4f. U0 — 27 cells × 75 columns × 3 engines against the M6-close pin, residual gate 1e-5

walk **17** differences (within gate 71), ssn **13** (68), ipm **13** (72) = **43**, which is
the E4-declared set exactly, and the whole comparison block is **byte-identical** to the E4
head's (`diff .scratch/e4/u0/comparison.txt .scratch/e2fix7/u0/comparison.txt` → no output).
Cell by cell there is no change: comparing the CSVs field by field, the only differing column
is **`wall_s`** (27 cells × 3 engines, informational). **No U0 mover.**

### 4g. The suites

| | tests | failures | time |
|---|---|---|---|
| Release, `ctest -j4` under `setsid` | **2880** | **0** | 114.86 s |
| Debug, `ctest -j4` under `setsid`, the >600 s test excluded by name | **2879** | **0** | 723.80 s |
| the >600 s test ALONE, `ctest -j1` (`Continuation.ProbeBudgetBoundsAFailingProposal`) | 1 | 0 | 610.45 s |

Base was Release 2870 / Debug 2869, so **+10 by name** in both: 4
`QpE2Fix7Acopr118.*`, 2 `BothAlgebras/QpE2Fix7Unrun.*`, 2 `WsIndependenceUnrun.*`, 2
`BothAlgebras/WsIndependence.AUsableFactorIsNeverAnUnrunTest/*`. Debug carries the four
declared skips (`QpE2WorkingSet.AGreyPinOnYorknet…`, `QpE4ProximalEqp.TheYorknetRestoration…`,
`QpNc2FlatFace.…Qp2Border`, `FailByDesignControl.…`) and the two disabled
`EqpRefinementAb.*`; Release carries one skip and the same two disabled.

An earlier run of the same battery, taken BEFORE the two re-pins, is kept at
`.scratch/e2fix7/logs/ctest-{release,debug}-f7.log` — Release 2 failures, Debug the SAME 2,
nothing Debug-only.

### 4h. clang-format

`clang-format --output-replacements-xml` reports **0 replacements** on all nine touched
files; `git diff --check` is clean.

## 5. The ACOPR118 CELL — the failure mode changed, the cell did not convert

**Build.** `.scratch/e2fix7/cellbuild.sh`: `git archive` of `e2fix7` HEAD `06cd4b31` plus the
two submodule trees, configured with the pin's own flags
(`Release / clang++ / HVEN_BUILD_TESTS=OFF / HVEN_BUILD_BENCH=OFF / HVEN_LINK_TIME_OPT=OFF`),
installed, then a `git archive` of hven-bench HEAD `4097f5c` (tracked tree only) builds
`hven_bench_cell` against it. hven-bench was NEVER written. **The install prefix is NAMED
`install-ff02789b46a7`** because hven-bench's `CMakeLists.txt:58` refuses any prefix that is
not the one `cmake/hven_pin.cmake` names; `HVEN_PIN.txt` carries an
`HVEN_E2FIX7_TREE 06cd4b31…` line saying so in as many words, exactly as fix5's
`HVEN_DIAG_TREE` did. `libhven.a` sha256 `75145d63…`, `hven_bench_cell` sha256 `e42d410c…`.

**Run.** `hven_bench_cell --lane hven-sqp --problem ACOPR118 --options default
--budget-iters 100 --assert-reference`, cwd `hven-bench`, `MKL_NUM_THREADS=1
OMP_NUM_THREADS=1`, `taskset -c 2`, alone under `flock /tmp/box-build.lock` with no timeout
(`PGREP_EMPTY` / `FOREGROUND_START 2026-09-27T23:30:07-04:00` / `FOREGROUND_END 23:33:25
rc=0`). Transcript `.scratch/e2fix7/repro/ACOPR118-f7.{out,err}`.

| | RETAKE8 (pre-E4) | RETAKE9 (the E4 pin) | **E2 FIX7 `06cd4b31`** |
|---|---|---|---|
| status | **optimal** | numerical_error | numerical_error |
| major_iters | 15 | 21 | **15** |
| objective | 129660.6833784326 | 129660.5787807497 | **129660.68777485074** |
| kkt_stationarity | 3.22e-10 | **125.63** | **0.04498** |
| kkt_primal_eq | 1.94e-09 | 0.028299 | 0.021147 |
| kkt_complementarity | 1.83e-07 | 4.6718 | 3.3858 |
| **qp_maxiter_exits** | **0** | **0** | **4** |
| dep_blocker_terminations | 2 | 3 | **2** |
| **dep_pin_skips** | 3084 | 4955 | **163** |
| qp_minor_iters | 17301 | 21515 | 32929 |
| factorizations | 11136 | 11629 | 15489 |
| steps_accepted / rejected | 12 / 3 | 12 / 8 | 8 / 6 |
| border_refine_steps | 8295 | 12770 | 27833 |
| symbolic_analyses | 987 | 1300 | 2645 |
| eq_drift_violations | 233 | 356 | 5337 |
| wall_s (INFORMATIONAL) | 63.91 | 81.10 | ≈198 |

**What this says, and what it does not.** The defect the round fixed IS gone from this cell:
`dep_pin_skips` collapses 4955 → **163** (the screen no longer skips repair pins),
`major_iters` returns to RETAKE8's **15**, the objective returns to RETAKE8's neighbourhood
(3.4e-8 relative above it, where RETAKE9 sat 8.1e-7 BELOW it at an infeasible point), and
stationarity improves by **three and a half decades**, 125.63 → 0.045. And the four killing
sub-QPs no longer break: `qp_maxiter_exits` 0 → **4** is the whole of the new failure —
the QPs now SOLVE those faces but want more minor iterations than their cap allows, and
`map_status` folds a `kMaxIter` QP into NLP `numerical_error` exactly as **registered item R2(ii)**
says it does. The cell still exits at an infeasible point (`kkt_primal_eq` 0.021 > feas_tol)
with the last successful subproblem's prices, which is R2(i) unchanged.

**It is not a localized fix and the report will not claim it is.** Rule B changes the repair
in EVERY QP, so this cell's trajectory differs from major 0 (accepted 12 → 8, rejected 3 → 6,
`dep_pin_skips` down by 97 % cell-wide). The four dumps this round certifies are the majors
17–20 subproblems of the OLD trajectory; the cap exits in the new run are on subproblems I
did not dump and cannot name without another instrumented round.

**The cost is real and is a RETAKE10 risk.** `qp_minor_iters` +53 %, `border_refine_steps`
+118 %, `symbolic_analyses` +103 % against RETAKE9, and wall 81 → ≈198 s on a suite where
three cells already sit at or within 53 s of the 300 s ceiling (registered R3). Wall is
informational per CLAUDE.md §7 and is quoted only as such — but the counter growth is the
asserted part, and it points the same way.

## 6. The RE-TAKE 10 prediction, per cell

Derived from RETAKE9's own side file
(`hven-bench/runs/2026-09-m7-w2-retake9/run-A-default/cutest/counters-hven-sqp.csv`), which
is the only per-cell witness available. The lane exports
`harness_qpsum_dep_pin_skips`, `harness_qpsum_dep_deferred_adds` and
`harness_qpsum_dep_undecidable`; it does **not** export `dep_unrun_tests` or
`dep_repair_screen_pins` (they did not exist at its HEAD), so neither rule has an exact
witness there and the two columns below are SUPERSETS.

| cell | RT9 status | pin_skips | deferred / undec | rule B can move it? | rule A can move it? |
|---|---|---|---|---|---|
| ACOPR118 | numerical_error | 4955 | 9 / 30 | **YES — MEASURED, §5** | yes (superset) |
| STEENBRC | numerical_error | 1968 | 19 / 33 | **yes** | yes |
| ACOPR57 | optimal | 1144 | 2 / 2 | **yes** | yes |
| ACOPP118 | numerical_error | 533 | 2 / 19 | **yes** | yes |
| NET3 | numerical_error | 331 | 0 / 0 | **yes** | **no** |
| STEENBRD | numerical_error | 128 | 9 / 14 | **yes** | yes |
| STEENBRE | numerical_error | 32 | 3 / 3 | **yes** | yes |
| READING6 | optimal | 23 | 1 / 1 | **yes** | yes |
| STEENBRB | numerical_error | 1 | 4 / 6 | **yes** (one candidate) | yes |
| ACOPP57 | optimal | 0 | 0 / 0 | **no** | **no** |
| DALLASM | optimal | 0 | 0 / 0 | **no** | **no** |
| SMBANK | optimal | 0 | 0 / 0 | **no** | **no** |
| ZAMB2-8 | optimal | 0 | 0 / 0 | **no** | **no** |
| YORKNET, STEENBRG | dnf_wall | — | — | **not predictable** | **not predictable** |
| BIGBANK, BRITGAS, MSS2, MSS3, READING8, SAWPATH, TAX213322, TAX53322 | null | — | — | not predictable | not predictable |

**Reading it.** (a) `pin_skips == 0` is a PROOF that no candidate pin was skipped for any
reason, so rule B cannot reach those four cells; the two conversions RETAKE9 bought
(**DALLASM**, **ACOPP57**) and **SMBANK** and **ZAMB2-8** are therefore predicted
BIT-IDENTICAL, and so are RT9's three other `optimal` rows except **ACOPR57** and
**READING6**, which do carry skips and are the two `optimal` rows at risk. (b) A nonzero
`pin_skips` is a superset: it counts the screen's skips AND the numerical test's, and only
the screen's half moves. (c) Rule A is predicted trajectory-NEUTRAL everywhere — MEASURED on
all 51 corpus dumps in both algebras (§4a) and on the whole HS corpus (§4d) — with only
`dep_undecidable`, `dep_deferred_adds`, `dep_structural_first` and the new `dep_unrun_tests`
moving; that is a measurement over 102 corpus rows and 48 HS rows, not a proof, since
dropping `recheck_pending` does change the next pass and could in principle fork a
trajectory. (d) The four `dnf_wall`/`null` rows carry NO counters at all (fix5's §6 point), so
nothing about them can be predicted from evidence; and the cost growth in §5 makes them the
likeliest place for a new timeout. **The honest summary: one cell MEASURED (ACOPR118 — the
kWrong dead end gone, the row still `numerical_error` on QP caps), four cells PROVEN
unreachable by rule B, nine cells reachable, and the cost is the open risk.**

## 7. Registered items

**F7-1 (the cause behind the cause, now fixed but worth naming).** `repair_temporary_vertex`
returning `false` restores `ws` and `x` but leaves `face` and the border stack describing the
last set it probed, and the walk does NOT `continue` on that `false` — it falls through into
the same pass, whose `pass_view` is then unusable. Rule A makes that harmless (the test's
verdict no longer claims anything) but the pass still runs its add site and its whole EQP
without a factor it can test against. A cheaper, more honest arrangement would be for the
failed repair to invalidate `face` explicitly, or for the walk to spend a pass re-factorizing
the restored set. Not touched here: it is a behaviour change on every failed repair and needs
its own measurement.

**F7-2 (the ACOPR118 cap exits).** With the kWrong dead end gone, the cell's four failures are
`qp_maxiter_exits`. Two sub-items: (i) whether the QP minor cap is simply too tight for these
subproblems on the new trajectory — testable with one cap-raised cell run, which needs an
options label hven-bench does not have today; (ii) R2(ii) again — `map_status` sends a
`kMaxIter` QP to `numerical_error`, and `qp_maxiter_exits` IS now exported to the lane
(it reads 4 here), so the NLP row could be made to distinguish "capped" from "broken" without
a schema addition.

**F7-3 (the cost of rule B).** `qp_minor_iters` +53 %, `border_refine_steps` +118 %,
`symbolic_analyses` +103 %, wall 81 → ≈198 s on ACOPR118 alone, and `eq_drift_violations`
233 → 356 → **5337**, a 15× jump over RETAKE9 that says the iterate spends far more passes off
its equality rows. Rule B lets the repair pin up to every free variable where it used to stop
at the screen; the worst case is `n` probe factorizations per failed repair. If RETAKE10 loses
cells to the wall, this is the first place to look, and a BOUND on the repair's pin count
(rather than the screen's accidental one) is the obvious remedy to evaluate.

**F7-4 (the two new counters are not in the lane).** `dep_unrun_tests` and
`dep_repair_screen_pins` are in `QpCounters` but hven-bench's `hven_bench_cell.cpp` exports a
curated list, so they do not reach the side file. Adding them is a lane schema event
(the same kind E4-37(d) already registered for R2's three columns) and would let a re-take
attribute both rules per cell instead of by superset.

**F7-5 (the screen's premise elsewhere).** The integer screen is read at three sites: the add
site's `dependency_test_to` (where it decides a candidate — the SAFE direction, unchanged),
the pass-top over-count check (`ws_count_violations`, telemetry plus the detector trigger,
unchanged), and the repair (fixed here). The family-vs-membership distinction applies to all
three; only in the repair is its conclusion the unsafe one. `ACOPR118` has
`ws_count_violations` 342 at RETAKE9 and 504 now, so the screen fires OFTEN on this problem
and the add site's use of it is worth a separate look — `dep_screen_under_mandatory_dependence`
exists for exactly that risk and reads **0** on this cell, because
`mandatory_dependent_entries` is 0, i.e. the detector never established that the mandatory
rows are dependent even though the counting says they must be at 349 members in 344 variables.
That gap — the detector not resolving a set the arithmetic proves rank-deficient — is the same
`Q1` the M5 round registered.

## 8. What I could not determine

- **Why the four sub-QPs need more minors in-cell than in replay.** Standalone, `qp5`…`qp8`
  certify in 353–416 minors against a 5300 cap. In-cell the new run takes four `kMaxIter`
  exits. Either the cap-exiting subproblems are different ones on the new trajectory (likely —
  the trajectory diverges from major 0) or the in-cell warm/hot state changes the walk's
  path on them. Naming them needs another instrumented dump round.
- **Whether rule B converts or costs the other nine reachable cells.** The prediction in §6 is
  a reachability statement from RETAKE9's superset columns, not a measurement. Only RETAKE10
  answers it.
- **Any Accelerate value.** The Mac lane did not run. `HsSweepCorpus.Corpus`'s Accelerate pin
  (3717) is **UNOBSERVED** under this round and was deliberately left unadjusted; whether the
  +2 reproduces there is unknown, and the arithmetic "3717 + 2" is a guess I did not make.
- **Whether a bound on the repair's pin count would be better than no bound.** F7-3's remedy
  is unmeasured. I did not evaluate it because the round's brief is the verdict rule, not the
  repair's budget.
- **Whether the un-run class occurs on TRAIN cells at all outside ACOPR118.** The lane cannot
  separate it from the stall class, and the 51-dump corpus witnesses it on exactly 3 dumps.

## 9. Disposition and evidence index

**ONE commit on `84fb520e`, branch `e2fix7`: `06cd4b31610caa62b42f0056de0c1b7e8ee3df0c`.**
12 files, no attribution trailer of any kind, explicit pathspecs only. The E4 head's own
tree is otherwise untouched; no gates declaration is made (the settler's step). No IPARM
surface was touched. No SNOPT material was read.

| path (under the worktree unless absolute) | what |
|---|---|
| `.scratch/e2fix7/STATE.md` | the running log |
| `.scratch/e2fix7/qp_engine.cpp.probe` | the scratch diagnostic patch (NEVER committed) |
| `.scratch/e2fix7/patch_off.py` | the scratch per-rule off switch (applied and reverted) |
| `.scratch/e2fix7/logs/probe1.log`, `logs/rp.log` | the un-run test and the repair, per candidate |
| `.scratch/e2fix7/logs/noscreen.log` | qp5–qp8 × 2 algebras × 2 arms, the refutation and the fix |
| `.scratch/e2fix7/corpus/f7-*`, `f7off-*` | the 51-dump corpus rows, both algebras, E2 on and off |
| `.scratch/e2fix7/hs-table-{off,f7}.txt` | the HS sweep tables the +2 was attributed from |
| `.scratch/e2fix7/u0/` | the U0 CSVs and the comparison block |
| `.scratch/e2fix7/logs/ctest-{release,debug}-f7c.log`, `…-f7c-long.log` | the shipping suites |
| `.scratch/e2fix7/logs/key-release3.log` | census T7 + HsBattery + HsSweepCorpus + B1Gate, 39/39 |
| `.scratch/e2fix7/logs/cellbuild2.out`, `repro/ACOPR118-f7.out` | the lane build and the cell |
| `.scratch/e2fix7/dumps/ACOPR118_e4/qp{0..8}.txt` | fix5's dumps (qp7, qp8 measured, not landed) |

`.scratch/e4-fix5/`'s build trees (`build-rel/`, `bench-build/`, `replay-build-*`) were left
in place — E4-37(f) permits deleting them, and ~1 GB is not worth the risk of removing
evidence a settler may still want to re-run.

SIGNOFF: M7-W2-E2-FIX7-REPORT
