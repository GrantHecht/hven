> Copied verbatim from `.superpowers/` (the round's own report/review) on 2026-10-01; nothing below this preamble is edited.

# M7 W2 P1 -- the cost round (report)

Worktree `/home/ghecht/Projects/hven/.claude/worktrees/agent-a9f44008cb25541ea`, branch `p1` off m7 `baddb728`.
Commits: **(a) `904350e2`** `kkt(m7): W2 P1 -- the Schur C is incremental, and sync_borders factorizes it once per batch`.
**(b) has no commit: the pattern changes on every probe, so nothing was implemented (see below).**
No attribution trailer (`git log --format=%B baddb728..HEAD | grep -ci co-authored` = 0). No MKL adapter touched; no `iparm` touched; nothing re-pinned.
Scratch: `.scratch/p1/` (STATE.md, scripts, logs, corpus, outputs, profiles). Wall-clock numbers are informational, solo, MKL_NUM_THREADS=1, one pinned core.
(This file lives in the worktree's `.scratch/p1/` because the harness forbade writing the main-tree `.superpowers/` path from the worktree; the caller should copy it to `/home/ghecht/Projects/hven/.superpowers/m7-w2-p1-report.md`.)

## (a) The Schur C

What changed (`include/hven/detail/kkt/schur_complement.h`, `src/qp/qp_engine.cpp`, `tests/sqp/test_schur.cpp`):
* `c_` is kept as a member (square buffer, geometric growth, only the top-left dim() block meaningful). INVARIANT, stated tersely in the header: that block is bit for bit what a from-scratch assembly of the current arrays gives (`-v_[i].dot(k0inv_v_[j])` for every (i,j), `+ d_[i]` on the diagonal).
* add: only the new row and column, each entry by the same expression with the same operand order (C is symmetric only up to rounding and the factorization reads the lower triangle, so both c(k,j) and c(j,k) are computed, not mirrored). drop: the row/column is deleted by plain loops (no allocation).
* All-or-nothing preserved: allocations (arrays and c_) are taken before any mutation; a failed add shrinks the arrays (c_ beyond dim() is meaningless, so it needs no undo); a failed drop re-inserts the three array entries and writes the saved row/column back into storage c_ already has (the saved copies are taken before anything mutates). The factorization is discarded on a failed factor exactly as before (evidence absent, `needs_refactorization()` true).
* Batch path: `begin_batch()` / `end_batch()`. Inside a batch an add extends C and discards the evidence; `end_batch()` factorizes once. If that one factorization throws, `replay_batch()` undoes the batch's adds and redoes them one at a time with a factorization each (reusing the cached K0^-1 v, no K0 solve repeated), so the throw is the per-add path's own (the first non-finite add) and the stack left behind is the per-add one. `sync_borders` batches its three add loops, closes the batch on every exit, and on a replay throw trims its ledger to the surviving stack prefix (and the `schur_updates` counter by the same amount). Drops are NOT batched (a drop inside a batch throws `std::logic_error`).
* The runtime message "...the last rebuild threw before producing block evidence" is deliberately unchanged (a test pins it).

### Readers of the evidence and inertia (batch safety)
Consumers of C's factorization/evidence: `needs_refactorization`, `cond_estimate`, `nearly_singular`, `expected_neg_eigs_delta`, `solve`. Their engine call sites: `border_solve_or_fall_back` (needs_refactorization before and after a rebuild, the inertia read), `refine_face_for_verdict`, `dep_factor_view`, the testing seam's needs_refactorization record, and the bordered solves / dependency tests that call `schur.solve`. All run AFTER `sync_borders` returns; none runs between two adds of one sync, and `add_border` returns nothing. `predictor.h` adds borders itself, per add, and is untouched (not batched; it benefits from the incremental C only). So no reader consumes intermediate per-add evidence, and factoring once per batch changes nothing observable. Inside a batch every reader refuses (evidence absent); a test pins that. The only observable difference possible is under a throw, which the replay reproduces (test `BatchThatThrowsLeavesThePerAddStack`, MKL only; on Apple a non-finite C is demoted to singular and there is no throw to replay: UNOBSERVED, not run).
Not covered by a test: a throw out of `drop_border`'s factorization (practically unreachable: it needs a non-finite C that could not have been added). Its rollback is written and reasoned, not executed.

### Identity (all green)
* rowcmp corpus (`tools/rowcmp.py`; base = the e2fix9 F9/F9off corpus, i.e. this code's baseline; tag A1/A1off): **0 rows changed of 51 in each algebra with E2 on (border + refactorize); 0 of 51 with E2 off (border)**.
* U0 offline: the declared 43-difference set (walk 17 / ssn 13 / ipm 13; within gate 71 / 68 / 72; 27 cells, 75 columns). The comparison block regenerated with the gated replay's own header labels is **byte-identical** to `docs/notes/data/2026-10-m7-w2-e2-fix9-standstill/gates/replay/comparison.txt` (`.scratch/p1/u0cmp.sh`). (The runner `g4.sh` stamps "NC2" in its header label; apart from that label the block is identical.)
* Key tests (`QpEngineStructuralViolation.T7*`, `QpEngineIndefinite.T7*`, HsBattery, HsSweepCorpus, B1Gate): 43/43, nothing re-pinned. E2 fixtures pass in Release and Debug.
* Release ctest 2894/2894 (2891 + 3 new). Debug ctest 1448 + 1445 = 2893, 0 failed (fix-9 baseline 2890 + 3). The long test (`Continuation.ProbeBudgetBoundsAFailingProposal`, Debug) ran ALONE, detached under setsid: PASS, **165 s (626 s at fix 9)**.
* clang-format: 0 replacements on the three touched files (formatted once, then rebuilt and re-tested, including the long test).
* TRAIN cells on the lane build (git archive of `904350e2`; hven-bench read-only): ACOPP57, ACOPR57, ACOPR118, DALLASM, READING6, SMBANK, ZAMB2-8, STEENBRB/C/D/E, NET3, ACOPP118: **0 differing columns of 132 compared per cell** against the RE-TAKE 10 rows (`run-A-default/cutest/results.csv` + `counters-hven-sqp.csv`; wall_s excluded; `.scratch/p1/cellcmp.py`). (The older `measured/fix9/*.out` files in that run dir predate eight counter columns and are not the RT10 rows.)

New tests (`tests/sqp/test_schur.cpp`): `IncrementalCMatchesFromScratchAfterAddsAndDrops` (cond estimate and inertia bitwise against an independent from-scratch assembly + dense factor; solve bitwise against a freshly built stack; adds and drops from several positions), `BatchedAddsLeaveTheStatePerAddWouldHave`, `BatchThatThrowsLeavesThePerAddStack`.

## (b) READING8's start repair: the pattern changes on every probe -- not implemented
* Each probe (`repair_temporary_vertex` -> `probe_inertia` -> `eqp_candidate`) pins one more variable. The probe's KKT is the bound-ELIMINATED K (`solve_eqp`), of dimension n_free + me + |W|; one more pin removes one free row/column, so the dimension changes by one per probe and the pattern cannot be the same. `analysis_decision` re-analyses exactly when the pattern hash differs from the last analysed one; C10's counts (2506 analyses for 2508 factorizations over 1001 probes) show it differed at essentially every probe. The release pass (reverse order) would revisit earlier shapes, but READING8 reads kWrong through all 1001 pins, so it never runs.
* The border path (fixed K0, a pin as a Schur border, no analysis) avoids this and serves the first schur_cap (128) pins; past that `latch_still_holds` latches onto the eliminated path, hence 2508 of 2763 `border_candidate` calls falling to `eliminated_candidate`.
* What reuse would require (none done): (1) reusing a Pardiso analysis across different dimensions is not possible through `hven::linear`'s surface (analyze/factorize are keyed on one pattern; a changed pattern is a new analysis); it would need a user-supplied permutation (iparm[4]), i.e. an `iparm` change -- forbidden here; (2) a fixed superset pattern with pinned rows/columns as identity rows changes the factorization numerically (different matrix, different pivots), so inertia and solves would not be bit-identical; (3) the engine-side routes (a larger/adaptive border cap for the repair so the pin loop stays on the border path, or a cheaper pin search) change behaviour and are not neutral. This round therefore reuses no analysis, and READING8 is unchanged.

## Payoff (solo, `default`, cap 580 s, harness limit 300 s)
Lane = `904350e2`. "Before" = the C10 profile runs and RT10.

| cell | inside 300 s? | status / objective | majors | minors | factorizations | restoration majors | wall |
|---|---|---|---|---|---|---|---|
| YORKNET | **yes** | numerical_error / 116908.12886905369 | 1 | 26097 | 31533 | 26 | **49 s** (was > 580 s) |
| SAWPATH | **yes** | numerical_error / 1579.9407855464206 | 1 | 3926 | 4102 | 12 | **25 s** (was > 580 s) |
| BRITGAS | **yes** | numerical_error / 0.39796565967079678 | 1 | 86808 | 89666 | 14 | **106 s** (was 497 s, same status) |
| READING8 | **no** (killed at 580 s, own pid, logged) | none | -- | -- | -- | -- | > 580 s (unchanged) |

The three that finish still end `numerical_error` at major 1: this round makes them FINISH, not convert; the failures behind them are the C10 ones (R2 / NC3 / ADD_REJ-BLKTERM / capped restoration QPs). QP numerical exits: SAWPATH 7, BRITGAS 3, YORKNET 0.

### Time profile after (perf dwarf; self-time by symbol, grouped; COARSE -- the "Schur C" row also holds MKL dense dgemv/dger kernels that Pardiso uses too, and 15-24% is ungrouped small symbols)
| cell | Schur C | Pardiso symbolic | Pardiso numeric | Pardiso solves + refinement | hven other |
|---|---|---|---|---|---|
| YORKNET | 24.5% | 22.6% | 6.9% | 22.3% | 3.5% |
| SAWPATH | 44.3% | 10.6% | 5.4% | 22.5% | 2.2% |
| BRITGAS | 13.2% | 25.1% | 11.9% | 20.8% | 5.2% |
| READING8 (60 s) | 3.8% | 53.5% | 22.8% | 9.4% | 1.8% |

Exact top-symbol self fractions: `append_border` (the new row/column dots) SAWPATH 34.3%, YORKNET 12.7%, BRITGAS 1.8%, ACOPR118 3.0%. Before (C10): `rebuild_schur` YORKNET 90.7%, SAWPATH 95.9%, BRITGAS 74.4%. SAWPATH's remaining Schur cost is the O(m n0) dots of each new row/column over its large warm seeds; no dense-factor symbol (dsytrf) appears in any top list after batching. READING8: `symfct` 39.9% + `upd_real_block` 17.3% + METIS ~8%: the per-probe analysis is intact, as (b) says.

### One optimal cell before and after: ACOPR118 (optimal, 21 majors, same 15430 factorizations)
* Solo wall: **135.7 s (RT10) -> 77 s** (this lane), about 1.76x. Under perf, before (the e2fix9 lane, same code minus this change) 136 s -> 77 s after.
* Profile: `rebuild_schur` 40.3% self before; after, `append_border` 3.0% and nothing dense above noise; the coarse "Schur C" group 54.6% -> 25.7%; Pardiso solves 20.6% -> 35.3% (now the dominant cost).
* Other finishing cells, RT10 wall -> now (informational): NET3 184 -> 44 s, STEENBRC 220 -> 29 s, ACOPP118 66 -> 44 s.

SIGNOFF: M7-W2-P1-REPORT
