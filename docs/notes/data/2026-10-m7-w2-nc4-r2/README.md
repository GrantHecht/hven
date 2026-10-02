# M7 W2 NC4 + R2 (+ R2 fix 1) -- the gates run

> Range **`79775010..21369067`** on branch `m7` (base `79775010` is NC3 run 2's record commit; its sources
> equal NC3 fix 1's gated head `b7993013`). Contents: NC4 (`37864475` fixtures + `eb710598` the convex
> certificate), R2 (`726a52a2` the failed subproblem's own step + the non-finite guard), `92d13c4e` (ledger,
> docs only), R2 fix 1 (`98a5bec1` + `21369067`). This is the first measurement of NC4 and R2 built on top of
> NC3 fix 1.

This directory holds the **declaration** (`wrappers-gates/`) and, once the chain has run, the **record**
(`gates/`). The run executes in its own git worktree, never in the settler's tree; the runner derives its
root from its own location.

## Declared values (every one derived from the range or measured by this declaration's rehearsal)

Derivations and their evidence: `wrappers-gates/psym-derivation-nc4-r2.txt`. Pre-flight transcripts and
falsifiers: `wrappers-gates/preflight-nc4-r2.txt`. Rehearsal evidence: `wrappers-gates/rehearsal/`.

| item | declared |
|---|---|
| changed surface | 5 headers (`solver_counters.h`, `ladder_testing.h` [HVEN_TESTING test seam, whole body gated], `trust_region.h`, `qp_engine.h`, `sqp_solver_types.h`); 13 modified `.cpp` (11 in the P-SYM universe, 2 outside: `test_qp_saddle_exit.cpp`, `test_sqp_ladder_seams.cpp`); 1 added `.cpp` (`test_qp_convex_certificate.cpp`, outside); 8 added fixtures |
| counters | QpCounters +1 (`convex_certificates`), SqpCounters +3 (`qp_failure_steps_judged`, `qp_failure_steps_accepted`, `nonfinite_subproblems`) |
| G1 / G3 suites | Debug **2925** / Release **2926** = NC3 r2's 2904 / 2905 + 21 added by name, 0 removed; skipped sets equal the baseline's by name (Debug 7, Release 1); the by-name baseline is bounded to NC3 r2's `gates-r2/` |
| G2 | the long test alone, 1 / 1 |
| G4 | the declared 43-difference set (walk 17 / ssn 13 / ipm 13), as a SET under the 1e-5 relative / 1e-13 absolute gate; re-measured exact-identical to NC3 r2's replay |
| G5 P-SYM | Release DIFFERING **70** = class A 11 + class B 59; Debug **71** = class A 11 + class B 60 (one declared Debug-only class B: `test_solve_status.cpp.o`, byte-identical in Release); UNEXPLAINED 0; non-debug section movers 9 both configs; DEBUG_ONLY 0 / 3 |
| G6 | install smoke, export contract, clang-format (19 files), `git diff --check`; fold proofs 46 / 0 |

## Runner changes against NC3 run 2's runner (nothing else)

Re-pointed names / scratch / label; `ROOT` derived from the file location (worktree run); **two** outside-universe
targets (`hven_qp_walk_seam_tests`, `hven_sqp_seam_tests`) handled as a list with per-source assertions; one
fail-closed relaxation of the class B asymmetry check (the classifier caps the negative-control listing at 80 and
this range's Release control has 81, so a present object is read positively and an absent one is still refused).
Details: runner header and section 2 of the derivation.
