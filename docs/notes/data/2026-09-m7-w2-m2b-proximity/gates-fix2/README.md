# M7 W2 M2b — FIX ROUNDS 2 and 2b — the evidence

This directory holds fix round 2's landed evidence, fix round 2's own FAILED
Release gate (honestly, in `fix2-failed-run/`), and fix round 2b's green
re-run of the full chain at the repaired fixture. **`../gates/` and
`../gates-fix1/` are NOT touched.**

Base for fix round 2: `37dcc9fa` (fix round 1's head). Adjudications:
`.superpowers/m7-w2-m2b-fix2-brief.md`, answering the scoped re-review at
`.superpowers/m7-w2-m2b-fix1-review-sol.md`. Fix round 2b's task:
`.superpowers/m7-w2-m2b-fix2b-brief.md`.

## What each round changed

**Fix round 2** (commits `10aa9daa`, `252201c7`, `60bc049b`) closed the scoped
re-review's three Important findings and its two Minors:

- **B1** (`10aa9daa`). At `zeta == 0.0` exactly, the feasibility gate
  (`feasibility_subsolve_certifies` in `include/hven/detail/globalization/sqp/restoration.h`)
  now certifies WITHOUT reading, copying, or rescaling the Jacobian bundle at
  all — the OFF path is the pre-M2b path by construction, not by margin. A
  structural fixture (`TheZeroWeightGateNeverReadsTheBundle`) proves this by
  poisoning the bundle with NaN and requiring `zeta = 0` to still certify. At
  `zeta > 0` (EXPERIMENTAL), the driver and `SqpResult::infeasibility_certified`
  now say the residual agrees with the raw frame "to rounding", not "exactly" —
  correcting fix round 1's overstatement in the two places it introduced it
  (`sqp_solver.cpp`'s wrapper comment and `restoration.h`'s class note). The
  unrelated "exact" sentence at `src/drivers/assembly_eval_seam.cpp:640`
  (`to_caller_scale`'s inverse-scaling comment) PREDATES fix round 1 — it lands
  at M6 commit `0339749f` (2026-09-14) — so B1 did not touch it and it is left
  alone, registered rather than silently widened into scope.
- **B3** (`10aa9daa`). The seed-guard test now falsifies a production
  regression instead of restating the wrapper's own wiring:
  `TheProductionCentreIsTheRequestingIterateNotTheSeed` computes `e(c)` for
  every admissible candidate from PRODUCTION-reported fields alone (`x_r`,
  `lambda`, `z` — no seam, no private access) and asserts `e(x_k)` at
  tolerance while every other candidate, including `*x_start`, sits an order
  of magnitude above it.
- **B2** (`252201c7`). `wrappers/gates.sh`'s P-SYM segment now clears the
  snapshot/`$OUT` directories before capture, asserts BOTH capture return
  codes (`assert_capture`), asserts the two capture arms enumerate the same
  object count (`assert_capture_pair`), and asserts the declared mover shape
  (differing / class A / class B) rather than merely "at least one mover".
  `fold_proofs.sh` was re-run against the LANDED runner and now records, per
  wrapper, both the worktree digest and `git show HEAD:<path> | sha256sum`;
  folds 27–28 now exercise the runner's own `gates_aggregate` /
  `accumulate_rc` functions instead of re-creating the arithmetic. Ten new
  folds land this round (43 total, 0 failed — `fold-proofs.txt` below).
- **B4** (`60bc049b`). Two corrections appended, append-only, to
  `docs/notes/2026-09-m7-w2-m2-restoration-design.md`'s Outcome section: the
  certificate residual is evaluated at `x_r` (only the proximity displacement
  is centred on `x_k`), and "ζ = 0 unchanged" is now true by construction
  (B1), not merely supported by margin.

Fix round 2's own gates chain (detached PID 2139784, HEAD `60bc049b`,
`.scratch/m7w2m2bfix2/`) then ran the full six gates and **FAILED G3
(Release)**: 1 of 2676 tests failed —
`SqpDriverRestorationProximity.TheProductionCentreIsTheRequestingIterateNotTheSeed`.
The test's own self-checked premise ("no start in {1.5, 1.99, 1.9} fired the
seed guard with an admissible candidate at least 1e-06 from the requesting
iterate") did its job: it caught that the fixture, not the production code,
was configuration-sensitive. The B3 fixture picked its start by scanning
three candidate points for a nearest-admissible-candidate separation, and
that scan had only been MEASURED in Debug; the circle/line route it walks is
a known Debug/Release bifurcation (see `../gates-fix1/cancellation-window.txt`
for that bifurcation's earlier appearance in this same design). In Release,
none of the three candidates cleared the premise, so the test's own guard
refused to produce a hollow pass — it failed loudly, exactly as B3's own
design intended a route failure to fail. `fix2-failed-run/` below is that
run's honest record: G1, G2, G4, G5, G6 all green; only G3 red.

**Fix round 2b** (commit `e7693383`, test-only, touches only
`tests/sqp/test_sqp_restoration.cpp`) replaced the route rather than the
assertion. The new fixture (`SeparatedRestorationModel`, n = 2, me = 1) is
constructed so the seed guard fires with an admissible candidate whose
separation from the requesting iterate is O(1) BY CONSTRUCTION: the request
lands at major 0 (so `x_k` is the known start point, asserted against the S5
record rather than recovered by search), `tr_min` is raised above the first
rejection's floor so the radius never collapses, the major-0 trial measures
better on `h` but is still refused on an f-type Armijo test, and the
restoration phase's own minimizer stays two units away in `x1`. The two-sided
logic and self-checked premise from B3 are kept unchanged; only the fixture
supplying the separation changed. The 100x wrong-centre margin is now an
assertion (`kCentreWrongCentreMargin`) rather than a comment, and the
separation premise is `ASSERT_GE` rather than skip-and-continue.

## The failed gate, stated plainly

Fix round 2's Release suite failed exactly one test, on its own designed-in
premise check, because the B3 fixture's start-point scan had only been run in
Debug before that gate chain — not because the production code under test
(the B1 zero-flop OFF path, or the B3 production-centre read) regressed. G1
(Debug), G2 (the long test), G4 (U0 replay), G5 (P-SYM), and G6 (install
smoke / export contract / clang-format / diff-check) were all green at that
same HEAD. Fix round 2b repaired the one fixture and re-ran the full chain;
every gate is green in both configurations below.

## Fix round 2b's gates, checked against the raw transcripts

Detached chain, PID 2185516, HEAD **`e7693383`**, `dirty-files=0`,
2026-09-21T05:02:46 → 2026-09-21T05:43:14 America/New_York.
`BEFORE=37dcc9fa` (fix round 1's head — fix round 2's own source/header set,
unchanged by 2b's test-only commit). Six lock segments; the box was otherwise
idle at the detach (no foreign cell or ctest running).

**An exit code is not a result.** Every number below was checked against the
raw transcript in this directory, not merely against the runner's own
`ASSERT OK` line or `GATES_SUMMARY`.

| gate | expectation | measured | checked against |
|---|---|---|---|
| G1 Debug suite | 0 failed, 2675 total | `100% tests passed, 0 tests failed out of 2675` | `suite-debug.txt`, ctest summary line read directly |
| G2 the long test alone | 1/1 passed | `Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 670.48 sec`, `100% tests passed, 0 tests failed out of 1` | `suite-debug-long.txt` |
| G3 Release suite | 0 failed, 2676 total, AND the fixture that failed fix round 2's G3 now Passed | `100% tests passed, 0 tests failed out of 2676`; `Test #2412: SqpDriverRestorationProximity.TheProductionCentreIsTheRequestingIterateNotTheSeed ... Passed 0.01 sec` | `suite-release.txt`; grepped by name against `fix2-failed-run/suite-release.txt`'s `***Failed 0.15 sec` for the same test at the same numeric ID |
| G4 U0 replay, 3 arms | 27 cells / 75 columns / 0 differences, ×3 (walk, ssn, ipm) | all three arms `cells compared: 27  columns compared: 75  differences: 0` | `replay/comparison.txt` |
| G5 P-SYM, Release | compare rc 1 (expected); `DIFFERING OBJECTS: 3` = class A 3 + class B 0; `UNEXPLAINED: 0`; negative control 72 (>= 1); both capture return codes 0 | matches on every count | `psym-compare-Release.txt`, `psym-classification-Release.txt`, `gates.txt` lines `capture after-Release: rc=0`, `capture before-Release: rc=0` |
| G5 P-SYM, Debug | same shape | matches on every count | `psym-compare-Debug.txt`, `psym-classification-Debug.txt`, `gates.txt` lines `capture after-Debug: rc=0`, `capture before-Debug: rc=0` |
| G5 classification, Release vs Debug | byte-identical | `diff psym-classification-{Debug,Release}.txt` empty | direct `diff` of the two committed files |
| G6 install smoke | PASSED, both solvers converged through one generic consumer, 19 standalone-include TUs | `IpmSolver converged, objective = 17.0140174`; `SqpSolver converged, objective = 17.0140173`; `== hven install smoke: PASSED` | `install-smoke.txt` |
| G6 export contract | align=32 | `EIGEN_MAX_ALIGN_BYTES OK: exported=32 computed=32`; `export contract OK for platform=linux, align=32` | `export-contract.txt` |
| G6 clang-format | rc 0 | `CLANG_FORMAT_RC=0` | `gates.txt` |
| G6 `git diff --check` | rc 0, `DIRTY_FILES=0` | `DIFF_CHECK_RC=0`, `DIRTY_FILES=0` | `gates.txt` |
| stamp | HEAD `e7693383`, `dirty-files=0`, no `-dirty` anywhere | `=== HEAD e7693383  dirty-files=0 ===`; a grep for `-dirty` across every file in this directory (excluding this README's own prose and `qp_params_dirty_` identifier matches) finds nothing | `gates.txt` header line; direct grep |
| aggregate | `GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`; `GATES_AGGREGATE rc=0` | exact match | `gates.txt` tail |

Two tests did not run in either config, as on every gate of this branch:
`FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas`
(Skipped) and the two `EqpRefinementAb` cases (Disabled).

### Files (this directory's additions)

```
gates.txt                              fix round 2b's chain log: HEAD, segment boundaries, every ASSERT and rc
suite-debug.txt                        fix round 2b's Debug suite tail (long test excluded)
suite-debug-long.txt                   fix round 2b's long test, alone
suite-release.txt                      fix round 2b's Release suite tail (the repaired fixture, green)
replay/head-{walk,ssn,ipm}.csv         fix round 2b's three replay arms at HEAD e7693383
replay/comparison.txt                  their comparison against the M6-close pin
psym-compare-{Release,Debug}.txt       fix round 2b's P-SYM raw object-by-object output
psym-classification-{Release,Debug}.txt fix round 2b's classification, which IS the gate
install-smoke.txt                      fix round 2b's install smoke tail
export-contract.txt                    fix round 2b's export contract output
fold-proofs.txt                        fix round 2's fold-proofs run against the landed B2 runner: 43 folds, 0 failed
fix2-failed-run/gates.txt              fix round 2's OWN chain log — the honest record of the failed Release gate
                                        (GATES_SUMMARY G1=0 G2=0 G3=1 G4=0 G5=0 G6=0, GATES_AGGREGATE rc=1)
fix2-failed-run/suite-release.txt      fix round 2's Release suite transcript, in full (628 KiB, under the 1 MiB
                                        excerpt threshold): the ***Failed line and the "following tests FAILED" block
                                        for SqpDriverRestorationProximity.TheProductionCentreIsTheRequestingIterateNotTheSeed
```
