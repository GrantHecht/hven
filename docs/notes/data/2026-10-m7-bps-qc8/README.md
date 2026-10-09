# M7 W2 BPS + QC-8 -- the gates run

> Range **`4a0da559..f58d01dc`** on branch `m7` (base `4a0da559` is the QN + QC-1 record's commit, the previous GREEN gate record).
> Contents: a docs-only ledger commit (`7f97fb1f`); BPS -- the bordered answer's certificate: `a781c3f2` (a bordered refinement that
> stops above its own target takes the eliminated twin for that pass, `QpCounters::border_uncertified_fallbacks`; an undecided
> zero-motion answer is refined past the footprint) and `d761d91f` (riders); and QC-8 -- the implied blocker (`QpCounters::
> dep_implied_passovers`): `c65ab30e`, `b79b012f`, `b3dca346`, `ffab67c0`, `f58d01dc`.

This directory holds the **declaration** (`wrappers-gates/`) and, once the chain has run, the **record** (`gates/`). The run executes in
its own git worktree. Derivations and their evidence: `wrappers-gates/psym-derivation-bps-qc8.txt`; pre-flight transcripts and
falsifiers: `wrappers-gates/preflight-bps-qc8.txt`; rehearsal evidence: `wrappers-gates/rehearsal/`.

## Declared values (every one derived from the range or measured by this declaration's rehearsal)

| item | declared |
|---|---|
| changed surface | 4 headers (`solver_counters.h`, `bordered_eqp.h`, `eqp_solve.h`, `qp_engine.h`); 10 modified `.cpp` (8 inside the P-SYM universe, 2 outside: `test_qp_anti_cycling_arm.cpp`, `test_qp_convex_certificate.cpp`); 2 added `.cpp`, both outside (`test_qp_border_certificate.cpp`, `test_qp_implied_blocker.cpp`, `hven_qp_walk_seam_tests`); 6 added fixture files; no `src/` or `tests/sqp` CMake change |
| G1 / G3 suites | Debug **3045** / Release **3046** = QN + QC-1's 3033 / 3034 + 12 added by name, 0 removed; ONE name re-declared (the N3 fixture: the old name's TEST line removed and re-added with a new body, the doubled-row instance renamed and ADDED); skipped sets equal the baseline's by name (Debug 7, Release 1) |
| G2 | the long test alone, 1 / 1 |
| G4 | the 43-difference set (walk 17 / ssn 13 / ipm 13) vs the M6 pin, as a SET; the set was RE-MEASURED by the rehearsal and equals the previous declaration's list; the exact arm vs QN + QC-1's replay was measured at ssn 0 / ipm 0 / walk 8 (two path_warm cells x four residual columns), recorded, not asserted |
| G5 P-SYM | Release DIFFERING **45** = class A 7 + class B 38; Debug **46** = class A 8 + class B 38 (class A asymmetry: `test_sqp_restoration.cpp.o` is NOISE-ONLY in Release); UNEXPLAINED 0; non-debug section movers 7 both configs; DEBUG_ONLY 0 / 7; manifests unchanged (nothing added in the universe) |
| G6 | install smoke, export contract, clang-format (16 files), `git diff --check`; fold proofs 46 / 0 |

## Runner changes against QN + QC-1's runner (nothing else)

Re-pointed names / scratch / label / BEFORE / baseline / G4 exact arm; the changed surface re-derived; **new** `REDECLARED_TEST_NAMES`
check (a TEST line removed and re-added under the same name); **new** `EXPECT_CLASS_A_NOISE_ONLY_RELEASE` slot (the mirror of the Debug
one); the sentinel loop covers the G4 declaration. Details: runner header and sections 3 / 2 of the derivation.
