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

## Gates (chain pid 207373, worktree `agent-a696fc244b020507c`, declaration `0a3496a1`, 2026-10-08, `gates/chain-qn-qc1.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**. **GREEN.**

| gate | result |
|---|---|
| G1 Debug suite | 3033 / 3033 (= P1E + C13's 2937 + 96 by name; 3035 started names = baseline 2939 + 96; skipped set = baseline's by name, 7) |
| G2 long test | 1 / 1 |
| G3 Release suite | 3034 / 3034 (= P1E + C13's 2938 + 96 by name; 3036 started names = baseline 2940 + 96; skipped set = baseline's by name, 1) |
| G4 U0 replay vs the M6 pin | the declared **43**-difference set (walk 17 / ssn 13 / ipm 13), measured set EQUALS the declared list; the chain's CSVs compared EXACT against P1E + C13's `gates/replay`: **0 differences** on 75 columns x 27 cells x 3 engines (`gates/replay/exact-vs-p1e-c13-record.txt`, recorded, not asserted) -- no G4 mover |
| G5 P-SYM | Release DIFFERING **76** = class A 13 + class B 63; Debug **77** = class A 13 + class B 64 (one declared Debug-only class B, `test_solve_status.cpp.o`, asserted byte-identical in Release); UNEXPLAINED **0** both configs; non-debug section movers 10 = the declared set both configs; DEBUG_ONLY 0 / 4; manifests 152 -> 160 with ONLY-AFTER = the eight added sources' objects in both; library ADDED = the four new library objects; the outside-universe asserts held |
| G6 | install smoke, export contract (EIGEN_MAX_ALIGN_BYTES 32), clang-format (35 files), `git diff --check`, `DIRTY_FILES=0` |

Observations, not deviations:
- The Release negative-control listing was **complete** in the chain (77 of 77 printed); the offline rehearsal classification had 89. The 12 extra
  are exactly the AFTER-state-only objects: the eight added in-universe objects plus the four `hven_sqp_seam_tests` recompiles of the new
  library sources (`tests/CMakeFiles/hven_sqp_seam_tests.dir/__/src/globalization/sqp/*.cpp.o`), none of which exists in g5's BEFORE-arm
  classifier tree. The completeness assertion is on printed == stated and held.
- The two value-parameterised QN suites (`SqpQnHs`, 6 instances; `SqpQnVariantHs`, 24) are registered by ctest under names that embed the
  raw bytes of their parameter, which holds a `const char*` -- a link-layout address (Debug and Release differ). They were read by stem +
  ordinal (runner header (d)). A name generator on the two suites would make the names stable at the source.
- Leg wall (informational): G1 7.0 min, G2 2.6, G3 5.2, G4 5.6, G5 35.7 (four builds, two configurations, inherent to the segment), G6 1.3.
- `gates/psym-sections-{Release,Debug}.txt` keep the section checker's summary lines only (`SECTIONS_*`, `NON-DEBUG-MOVER`, `DEBUG-ONLY`, `REFUSED`).
- A foreign benchmark cell binary was alive when the chain was launched; every leg of the chain, and the fold proofs, printed `PGREP_EMPTY`
  under the lock hold (the other agent's work queued behind the lock).
