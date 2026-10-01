> Copied verbatim from `.superpowers/` (the round's own report/review) on 2026-10-01; nothing below this preamble is edited.

# M7 W2 E2 — FIX ROUND 9: THE STANDSTILL RETREAT

Implementer report, 2026-10-01. Worktree `/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9`,
branch `e2fix8`. **Commit `8cfc12ed`** on top of fix 8 `35986d1f` (one commit, explicit pathspecs, no
attribution trailer: `git log --format=%B -1 | grep -ci co-authored` = 0). Scratch `.scratch/e2fix9/`
(`STATE.md` current). hven's main tree was written only for this report; hven-bench was never written
(its HEAD archived and built in scratch, binaries run read-only, outputs in scratch). No IPARM change, no
`notices/` change, no SNOPT or GPL/LGPL source. Accelerate: UNOBSERVED (not run, nothing adjusted).
Labels: **OBSERVED** = measured this round; **INFERRED** = not measured.

## 1. What landed (S46's S2, as production code)

`src/qp/qp_engine.cpp`, no getenv, no toggle:

- **Qualifying pass** (`standstill_face`): `independent_working_set` on (the structural flag is computed only
  inside the existing equality-drift block), `ws_algebra == kSchurBorder`, not already `eliminated_only`, the
  drift block's own `violation_is_structural` test finds an equality row violated, and the pass's main inertia
  verdict is **kWrong**.
- **Trigger (i)**: zero motion on a qualifying pass → the border retreat (`eliminated_only = true`), arm
  `recheck_pending`, `continue` (walk seam records `kEliminatedRetreat`; no new seam action).
- **Trigger (ii)**: a qualifying pass ending in a full step (no blocker, alpha >= 1, no ride) extends a run;
  the second consecutive one retreats the same way.
- **Reset (S46)**: the run is taken and zeroed at the TOP of every pass, so any pass that does not extend it,
  by any exit (`continue`, drop, add, retreat), clears it. The F8-1 scratch reset only at loop end; the
  re-measurement below shows the change moved nothing measured (matches F8-1's INFERRED expectation).
- **Counter**: `QpCounters::standstill_retreats` (terse doc in `solver_counters.h` plus a short block in
  `qp_engine.h`). **It is NOT also counted in `dep_border_retreats`.** The F8-1 scratch counted it there, so
  the lane's `harness_qpsum_dep_border_retreats` differs from the F8-1 S2 cells by exactly this split (§4). It
  needs its own lane column at RE-TAKE 10, alongside F8-4's four counters.

Option-off arm: untouched by construction, and measured identical (§3, §4).

Tests (`8cfc12ed`):

- **New `tests/qp/test_qp_standstill_retreat.cpp`** in the walk-seam target, with 7 fixtures copied from
  `.scratch/e2fix8/diag/capdump/*-pd0/` to `tests/qp/fixtures/e2-d4/ACOP{R,P}118_f81_qp*.txt`, about 1.1 MB.
  - Each test asserts:
    - the premise: in-cell `status=1`;
    - kOptimal;
    - `standstill_retreats` = 1 on border and 0 on refactorize;
    - |f − ref| ≤ 1e-6·max(1, |ref|);
    - the KKT battery (eq, ineq, stat, comp ≤ 1e-8; box, sign ≤ 1e-9).
  - References:
    - the kRefactorize algebra's certified objective where that algebra certifies (ACOPR118 qp3–qp6, ACOPP118
      qp0–qp1);
    - SLSQP 1592.5507 for ACOPP118 qp3 (oracle only).
  - Arms: border on all 7; refactorize on ACOPR118 qp3 and ACOPP118 qp1.
  - **Release-only (GTEST_SKIP in Debug, reason stated in the file):**
    - ACOPP118 qp0: certifies in Debug too, but takes 231 s there against 4 s in Release.
    - ACOPP118 qp1, both arms: in Debug it **caps on both algebras, kRefactorize included**, which this rule
      never touches. OBSERVED, so this is Debug rounding on that dump, not the retreat.
  - **Debug cost, OBSERVED:** ACOPP118 qp3 64 s is the worst case, then ACOPR118 qp6 39 s; about 163 s for the
    target's Debug fixtures. Release: about 13 s for all 9.
  - **ACOPP118 qp2 is not kept.** It fails on the eliminated path too (it caps with `standstill_retreats` 1).
- **T7Fix2** (`QpEngineIndefinite.T7Fix2AFailedStartRepair…`, independent arm): border factorizations
  4 → 5 and `standstill_retreats` 1, re-pinned as a **counter pin only**. Refactorize stays 4. The test's
  status (`kInfeasible`), `x(0) = 0.5` and `x(1) = 0` assertions are unchanged and pass (OBSERVED).
- **R9-1**: `case Action::kStartRepairRefused:` added to the walk-seam switch. It asserts base == ws_size_out
  and Block::kNone.
- **R9-2**: comment corrected. The HS sweep table confirms it (OBSERVED): HS15 cold is 1069 in the SOC-on
  cells and 1064 in the SOC-off cells.

## 2. The battery (all OBSERVED, on the uncommitted tree that became `8cfc12ed` byte-for-byte)

| leg | result |
|---|---|
| capdumps, production `dump_replay`, border E2 on | **7/8 kOptimal**: minors, factorizations and f identical to F8-1's S2 arm on all 8. qp2 caps. `standstill_retreats` = 1 on all 8 |
| capdumps, refactorize | 6/8, identical to F8-1 `refac-base`, `sstr` 0 |
| capdumps, E2 off | 8/8 cap (unchanged), `sstr` 0 |
| corpus via `rowcmp` vs fix8 H2, E2 on, both algebras | **0 / 102 rows** (`--skip sstr`), `sstr` 0 on all |
| corpus vs fix8 H2off, E2 off, border | **0 / 51 rows**, `sstr` 0 |
| key (`T7*`, `HsBattery`, `HsSweepCorpus`, `B1Gate`) | 43/43. **HS stays 3342** |
| walk-seam fixtures (fix7, fix8, fix9, QpWalkSeam), Release | 38/38 |
| walk-seam fixtures, Debug | 35 pass + 3 declared skips |
| U0 vs the M6-close pin | walk 17, ssn 13, ipm 13 = **the declared 43 set**. `comparison.txt` matches fix8 H2's |
| U0 vs fix8 H2 | **0 differences, 0 within-gate**, all three engines (byte-equal block) |
| Release suite | **2891/2891** = 2882 + 9 new |
| Debug suite | 2890/2890 = 2881 + 9 new (3 skipped as Release-only), in two legs of 582 s and 264 s |
| the >600 s Debug test (`Continuation.ProbeBudgetBoundsAFailingProposal`), ALONE, detached under setsid, waited on its exact pid | 1/1, 626 s |
| clang-format | 0 replacements on all 8 touched source/test files |
| `git diff --check` | clean |

**Movers and their attribution:**

- **T7Fix2**, border 4 → 5 factorizations. This is the retreat firing on that fixture's inconsistent rows
  (e₀ = 0, e₀ = 1). It is a counter pin, not a correctness change.
- **The fix9 fixtures** themselves.
- Nothing else moved.

## 3. TRAIN cells on the production lane build (OBSERVED)

The lane binary was built from `git archive 8cfc12ed` plus the hven-bench HEAD archive, using fix8's recipe
with the RT9 pin prefix name. libhven.a sha256 starts `a7033a5a…`. Cells ran one at a time:
`MKL_NUM_THREADS=1`, `taskset -c 2`, one box leg per cell, a 600 s cap on the cell's own pid.

**Against the F8-1 S2 measurement (full .out diff):**

- **11 cells are byte-identical**: ACOPP57, ACOPR57, DALLASM, READING6, SMBANK, ZAMB2-8, STEENBRB, STEENBRD,
  STEENBRE, NET3 and STEENBRC.
- **ACOPR118 is OPTIMAL**: 21 majors, f 129660.68370269175, stationarity 7.0e-9, eq 7.8e-7, 15430
  factorizations, 16116 minors, about 135 s.
  - It differs from the S2 cell in one field only: `harness_qpsum_dep_border_retreats` 11 → 8. That is the
    counter split.
  - INFERRED from it: 3 in-cell standstill retreats. The lane does not export the new counter.
- **ACOPP118 is numerical_error**: 7 majors, stationarity 25.1, about 66 s.
  - It also differs in `dep_border_retreats` alone: 6 → 0, i.e. INFERRED 6 standstill retreats.

**The E2-off twins, MEASURED:**

| cell | result | identity check |
|---|---|---|
| ACOPP118 off | numerical_error, 8 majors, 29508 factorizations, f 129657.95589930186, 311 s | byte-identical to the fix8 round's off row (`e2fix8/repro`) |
| ACOPR118 off | numerical_error, 16 majors, 18451 factorizations, f 129660.68514675142, 555 s | re-run on the **fix8** lane binary: **byte-identical** (547 s) |

**Informational cells:**

- **STEENBRG default**: max_iter, 86 majors, 362 s. Byte-identical to fix8.
- **YORKNET default**: wall-capped at 600 s. I stopped the cell by its own exact pid 4143427; it is logged in
  `logs/wallcap.log`. Same outcome as fix8. That leg ran 600 s, the cap, which is a declared overrun of the
  <600 s leg guideline for this one informational cell.

**Against RETAKE9** (`logs/rt9-compare.txt`):

- **Identical to RT9:** ACOPP57, DALLASM, SMBANK, ZAMB2-8 and NET3 (0 fields differ).
- **Differences that come from fix7/fix8, not fix9:** READING6 (8 fields; factorizations 688 → 690),
  ACOPR57 (16 majors, factorizations 1335 → 1632, +22 %: F81-3), and STEENBRB/C/D/E (+1 to +16
  factorizations, same status, majors and f). Each of these cells is field-identical between the fix8 base and
  S2 (F8-1), and S2 equals fix9 here.
- **What fix9 moves:** ACOPR118 and ACOPP118.

## 4. RE-TAKE 10 prediction (TRAIN, default arm unless noted)

| cell | RT9 | prediction | basis |
|---|---|---|---|
| ACOPR118 | error, 21 majors | **optimal**, 21 majors, f 129660.68370269175, about 135 s (under the 300 s budget) | MEASURED |
| ACOPP118 | error, 6 majors, 247 s | error (numerical_error), 7 majors, about 66 s | MEASURED |
| ACOPP57, DALLASM, SMBANK, ZAMB2-8 | optimal | optimal, every field identical | MEASURED |
| READING6 | optimal | optimal, factorizations +2 (fix8 rule A) | MEASURED |
| ACOPR57 | optimal, 15 majors | optimal, 16 majors, factorizations +22 % (F81-3, a cost to declare) | MEASURED |
| STEENBRB/C/D/E, NET3 | error | error, same majors and f; factorizations +0 to +16 | MEASURED (all ≤ 222 s) |
| STEENBRG | dnf_wall at 300 | dnf_wall: it needs 362 s to reach max_iter | INFERRED from the 362 s solo run |
| YORKNET | dnf_wall at 300 | dnf_wall: still running at 600 s | INFERRED |
| ACOPR118 off | dnf_wall at 300 | dnf_wall: 555 s to numerical_error | INFERRED |
| ACOPP118 off | dnf_wall at 300 | dnf_wall: 311 s to numerical_error, just over the budget | INFERRED; borderline, so wall noise could flip it to error |
| the other TRAIN cells (not in this list) | — | not measured this round; nothing in the corpus or U0 evidence moved | INFERRED |

**Optimal count:** RT9's 6 plus ACOPR118 = **7/23**. ACOPR118 is MEASURED; that no other cell moves is
INFERRED for the cells not run. The re-take needs the `standstill_retreats` lane column. The F8-4 counters
are still a precondition.

## 5. Findings and registrations

- **F9-1 (seam gap, pre-existing):** the entry/re-check removal pass `continue`s without recording a walk
  action, so the record is `kUnset` (`qp_engine.cpp`, the `remove_entry_dependency` branch). The retreat arms
  the re-check, so the capdump walks reach it. The exhaustive seam switch would `ADD_FAILURE` on any fixture
  that runs it under the observer. The fix9 fixtures therefore do not attach the observer. It needs its own
  action, as a seam item.
- **Root cause not fixed (S46):** F81-1 (admission of exact dependencies) and F81-2 (the border path's
  impossible ρ, and the eliminated-path admissions behind ACOPP118 and qp2) remain the next E2 conversion
  items.
- **Counter split:** `dep_border_retreats` no longer includes the standstill retreats, unlike F8-1's
  scratch. Compare against the S2 cells with that in mind.

## 6. Evidence (under `.scratch/e2fix9/`)

| path | contents |
|---|---|
| `STATE.md` | running log |
| `logs/battery9-B1.out`, `logs/key-B1.log`, `logs/fixtures-B1-*.log`, `logs/u0-B1.log`, `u0-B1/`, `logs/ctest-{release,debug1,debug2,debug-long}-B1.log` | the battery |
| `corpus/F9*`, `corpus/cap*` | corpus and capdump replays |
| `cells/` | cells on the fix9 lane binary |
| `cells-f8/` | ACOPR118 off on the fix8 binary |
| `logs/rt9-compare.txt` | the RT9 comparison |
| `logs/wallcap.log` | the YORKNET wall cap |

SIGNOFF: M7-W2-E2-FIX9-REPORT
