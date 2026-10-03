# M7 W2 W1 + CERT-1 -- the gates run

> Range **`2d2f92b1..6bcffcd9`** on branch `m7` (base `2d2f92b1` is the NC4 + R2 record's commit, the previous GREEN gate
> record). Contents: `5f035075` (ledger, docs only); CERT-1 `3c942780` (the restoration's infeasibility certificate is stated in
> the convergence test's own feasibility measure); W1: `cee22473` (the BRITGAS elastic qp1 fixture), `4ddeb90c` (the ascent guard in
> the QP walk + the GPROJ rider in the SQP driver), `1c4d59bd` (guard v4), `6bcffcd9` (review riders: a fixture assertion + a comment).

This directory holds the **declaration** (`wrappers-gates/`) and, once the chain has run, the **record** (`gates/`). The run
executes in its own git worktree, never in the settler's tree; the runner derives its root from its own location.

## Declared values (every one derived from the range or measured by this declaration's rehearsal)

Derivations and their evidence: `wrappers-gates/psym-derivation-w1-cert1.txt`. Pre-flight transcripts and falsifiers:
`wrappers-gates/preflight-w1-cert1.txt`. Rehearsal evidence: `wrappers-gates/rehearsal/`.

| item | declared |
|---|---|
| changed surface | 4 headers (`solver_counters.h`, `qp_engine.h`, `qp_walk_trace.h`, `sqp_solver_types.h`); 9 modified `.cpp` (6 in the P-SYM universe, 3 outside: `test_qp_saddle_exit.cpp`, `test_qp_unrun_dependency_test.cpp`, `test_sqp_ladder_seams.cpp`); 1 added `.cpp` (`test_qp_ascent_guard.cpp`, outside); 1 added fixture (`C11_BRITGAS_qp1.txt`) |
| counters | QpCounters +1 (`ascent_rides`), SqpCounters 60 -> 61 (`nonfinite_box_projections`) |
| G1 / G3 suites | Debug **2930** / Release **2931** = NC4 r2's 2925 / 2926 + 5 added by name (3 `QpW1AscentGuard.*`, the CERT-1 fixture, the GPROJ test), 0 removed; skipped sets equal the baseline's by name (Debug 7, Release 1); the by-name baseline is bounded to NC4 r2's `gates/` |
| G2 | the long test alone, 1 / 1 |
| G4 | the declared 43-difference set (walk 17 / ssn 13 / ipm 13), as a SET under the 1e-5 relative / 1e-13 absolute gate; re-measured exact-identical to NC4 r2's replay |
| G5 P-SYM | Release DIFFERING **67** = class A 6 + class B 61; Debug **68** = class A 6 + class B 62 (one declared Debug-only class B: `test_solve_status.cpp.o`, byte-identical in Release); UNEXPLAINED 0; non-debug section movers 8 both configs; DEBUG_ONLY 0 / 3 |
| G6 | install smoke, export contract, clang-format (14 files), `git diff --check`; fold proofs 46 / 0 |

## Runner changes against NC4 r2's runner (nothing else)

Re-pointed names / scratch / label / BEFORE; the by-name baseline pinned to NC4 r2's `gates/`; **the classifier's
80-object listing cap is lifted** (`wrappers-gates/classify_psym_m4_uncapped.py` = the shared `classify_psym_m4.py` minus the
two `[:80]` slices; the shared file is untouched) and NC4 r2's relaxation of the class B asymmetry check is therefore reverted --
the completeness assertion (printed == stated) is back; the outside-universe declaration is three modified + one added source
(the two outside targets and the per-source assertions are NC4 r2's). Details: runner header and section 0 / 2 of the derivation.
