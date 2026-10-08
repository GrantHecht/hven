# M7 QN (stages 1-3 + quality) + QC-1 -- the gates run

> Range **`7c24a78b..2d1011e4`** on branch `m7` (base `7c24a78b` is the P1E + C13 record's commit, the previous GREEN gate
> record). Contents: the quasi-Newton mode, behind `SqpOptions::hessian_mode` (default `kExact`): `83a7fe13` (stage 1, the damped
> compact L-BFGS class), `7f9cd829` (`QpOptions::hessian_known_convex`, default false), `772b5a54` (stage 2, the mixed-lift QP adapter and
> the dense-B oracle), `e5054d92` (stage 3, driver integration, `eval_hess_calls` + the `qn_*` counters), `aa4667f0` (stage 1-3 riders),
> `2956050c` (quality: full-memory damped BFGS, self-scaling, nonlinear-variables-only restriction) and `295c5a5b` (quality riders);
> and QC-1: `2bf05c5c` (no infeasibility certificate from a near-feasible restoration entry) and `2d1011e4` (its riders).

This directory holds the **declaration** (`wrappers-gates/`) and, once the chain has run, the **record** (`gates/`). The run
executes in its own git worktree, never in the settler's tree; the runner derives its root from its own location.

## Declared values (every one derived from the range or measured by this declaration's rehearsal)

Derivations and their evidence: `wrappers-gates/psym-derivation-qn-qc1.txt`. Pre-flight transcripts and falsifiers:
`wrappers-gates/preflight-qn-qc1.txt`. Rehearsal evidence: `wrappers-gates/rehearsal/`.

| item | declared |
|---|---|
| changed surface | 13 headers (4 added: `lbfgs_damped.h`, `mixed_lift.h`, `qn_lift_bridge.h`, `qn_models.h`; `solver_counters.h` among the 9 modified); 14 modified `.cpp` (13 in the P-SYM universe, 1 outside: `test_sqp_ladder_seams.cpp`); 8 added `.cpp`, all INSIDE the universe (4 library, 4 `hven_sqp_tests`); `src/CMakeLists.txt` +4 sources and the source count 44 -> 48; `tests/sqp/CMakeLists.txt` +4 lines |
| G1 / G3 suites | Debug **3033** / Release **3034** = P1E + C13's 2937 / 2938 + 96 added by name (66 plain TEST, 6 `SqpQnHs` + 24 `SqpQnVariantHs` instances), 0 removed; skipped sets equal the baseline's by name (Debug 7, Release 1); the by-name baseline is bounded to P1E + C13's `gates/`; the two parameterised suites' instances are read by stem + ordinal because their ctest names embed an address |
| G2 | the long test alone, 1 / 1 |
| G4 | the declared 43-difference set (walk 17 / ssn 13 / ipm 13) vs the M6 pin, as a SET under the 1e-5 relative / 1e-13 absolute gate; re-measured exact-identical to P1E + C13's replay (the exact arm is recorded in the chain, not asserted) |
| G5 P-SYM | Release DIFFERING **76** = class A 13 + class B 63; Debug **77** = class A 13 + class B 64 (one declared Debug-only class B: `test_solve_status.cpp.o`, byte-identical in Release); UNEXPLAINED 0; non-debug section movers 10 both configs; DEBUG_ONLY 0 / 4; manifests 152 -> 160 (eight ADDED in-universe objects, four of them library objects) |
| G6 | install smoke, export contract, clang-format (35 files), `git diff --check`; fold proofs 46 / 0 |

## Runner changes against P1E + C13's runner (nothing else)

Re-pointed names / scratch / label / BEFORE; the by-name baseline pinned to P1E + C13's `gates/`; the uncapped classifier is a
byte-identical copy; the changed surface re-derived (8 added in-universe sources, 1 outside source); a NEW pre-flight check on
`src/CMakeLists.txt`'s added and removed lines; the sentinel loop covers every measured slot; the name reader folds the two
address-bearing parameterised suites to stem + ordinal; G4 records an exact comparison against the previous record's replay. Details:
runner header and sections 0 / 1 / 3 of the derivation.
