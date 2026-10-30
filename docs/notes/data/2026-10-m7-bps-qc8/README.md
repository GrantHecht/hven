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

## Gates (chain pid 821095, worktree `agent-a8738dd7f8e3c1176`, declaration `201b182a`, 2026-10-09, `gates/chain-bps-qc8.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**. **GREEN.**

| gate | result |
|---|---|
| G1 Debug suite | 3045 / 3045 (= QN + QC-1's 3033 + 12 by name; 3047 started names = baseline 3035 + 12; skipped set = baseline's by name, 7) |
| G2 long test | 1 / 1 |
| G3 Release suite | 3046 / 3046 (= QN + QC-1's 3034 + 12 by name; 3048 started names = baseline 3036 + 12; skipped set = baseline's by name, 1) |
| G4 U0 replay vs the M6 pin | the declared **43**-difference set (walk 17 / ssn 13 / ipm 13), measured set EQUALS the declared list. Exact arm vs QN + QC-1's `gates/replay` (`gates/replay/exact-vs-qn-qc1-record.txt`, recorded, not asserted): **ssn 0, ipm 0, walk 8** -- `f7_n1000_path_warm` and `f7_n800_path_warm` x `kkt_residual / kkt_stationarity / kkt_primal / kkt_complementarity`, last-digit residual-class moves inside the gate, identical to the rehearsal's measurement; no status or counter column moved on any engine -- no G4 mover |
| G5 P-SYM | Release DIFFERING **45** = class A 7 + class B 38; Debug **46** = class A 8 + class B 38 (`test_sqp_restoration.cpp.o` NOISE-ONLY in Release, class A in Debug, the declared `EXPECT_CLASS_A_NOISE_ONLY_RELEASE`); UNEXPLAINED **0** both; non-debug section movers 7 = the declared set both; DEBUG_ONLY 0 / 7; manifests 160 / 160 (after = before), ONLY-AFTER and library ADDED empty; the four outside-universe sources each built in exactly one listed target and no listed-target object in either manifest; class B sets equal across configurations (38) |
| G6 | install smoke, export contract (EIGEN_MAX_ALIGN_BYTES 32), clang-format (16 files), `git diff --check`, `DIRTY_FILES=0` |

Observations, not deviations:
- The Release negative control was 112 in the chain against the offline rehearsal's 114 (the offline classification tree is at the AFTER
  state, the chain's classifier tree at the BEFORE arm; the same two-object kind of difference earlier rounds saw). It is not asserted as a number.
- G5's Debug-only class B asymmetry slot is EMPTY this range, so the negative-control completeness assertion (which that slot guards) was not exercised.
- Leg wall (informational): G1 7.3 min, G2 2.8, G3 5.3, G4 5.6, G5 34.0, G6 1.3.
- `gates/psym-sections-{Release,Debug}.txt` keep the section checker's summary lines only.
- Every leg of the chain, the rehearsal and the fold proofs printed `PGREP_EMPTY` under the lock hold; no foreign cell binary was alive at any launch.
