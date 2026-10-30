# M7 W2 M2b — FIX ROUND 1 — the evidence

This directory is fix round 1's own evidence. **`../gates/` is NOT edited**: it
is the artifact commit's transcript and stays exactly as it was written.

Base: `cb8b88c7` (branch `m7`). Adjudications:
`.superpowers/m7-w2-m2b-fix1-brief.md`. Review that ordered the round:
`.superpowers/m7-w2-m2b-review-sol.md`.

## The certificate (A1)

| file | what it is |
|---|---|
| `cancellation-window.txt` | the `(ζ, kkt_tol)` sweep that LOCATED a point where the old `‖∇P‖∞` proxy certifies and the exact `‖Jᵀλ − z_sub‖∞` rule refuses, in both configurations, and the arithmetic that placed the pinned cell at the window's midpoint |
| `fail-before-Release.txt`, `fail-before-Debug.txt` | the new fixture run ONCE against the PRE-FIX expression, in both configurations. Both of its PREMISES pass — the proxy is under `kkt_tol`, the exact residual is over it — and the verdict assertion FAILS, with `infeasibility_certified` reported `true`. That is the false certificate, observed |
| `pass-after-Release.txt`, `pass-after-Debug.txt` | the same fixtures against the exact rule: 21 tests, all passing, in both configurations |

The fail-before run was taken on a tree whose `src/` and `include/` were
UNMODIFIED against `cb8b88c7` and whose `tests/` already carried this round's
fixtures — no history was rewritten and no scratch copy of the tree was made.

## The gate runner (A2)

`fold-proofs.txt` — one run of `../wrappers/fold_proofs.sh`. **28 refusals, 28
fired, 0 failures.** Every refusal added this round is exercised against a
deliberately broken input and required to exit non-zero, and every one of them
is also shown to ACCEPT the corresponding good input, so none of them is a
refusal that refuses everything. It covers:

* the classifier's R1 (empty / malformed / absent comparison), R2 (a summary
  line and no differing object — the vacuous pass), R3 (a negative control
  below the required minimum), R4 (an unexplained object), and the
  changed-header list disagreeing with the one the classifier declares;
* `assert_ctest` on a log with no summary line (the zero-test green), on the
  right shape at the wrong total, on a nonzero failed count, and on an empty
  file;
* `assert_replay` on a comparison missing an engine, on one with differences,
  and on an absent file;
* `assert_psym` on a compare rc of 0, a classifier rc of 1, movers not
  accounted for by class A + class B, a nonzero UNEXPLAINED, a zero negative
  control, and zero movers;
* `assert_same_classification` on two configs that disagree — which the first
  round accepted with an `echo`;
* `ctest --no-tests=error` over a filter that matches no test, which is the
  wrong-binary incident's own shape (report §8);
* the final aggregate carrying `rc5`, and G6 propagating a failed sub-gate
  through the green ones that follow it.

## The gates

The full set, run by `../wrappers/gates.sh` with the expected counts declared at
its top (Debug 2672 with the long test excluded, Release 2673, the long test 1;
each derived and cross-checked in the runner's own comment). The transcripts
land here beside this file.

Detached chain, PID 2061221, on the committed tree at **`b1c153f6`**,
`dirty-files=0`, 2026-09-20T23:26:55 → 2026-09-21T00:27:52 America/New_York.
`BEFORE=cb8b88c7` (this round's own base commit; a transcripts-only commit, so
its compiled surface equals its parent `ef6284f8`'s). Six lock segments; a
sibling lane's census (`run_rest.sh`, not this task's) interleaved and was
never signalled.

**An exit code is not a result.** Each number below was independently checked
against the raw transcript, not merely against the runner's own `ASSERT OK`
line.

| gate | result | checked against |
|---|---|---|
| G1 Debug suite (long test excluded) | `100% tests passed, 0 tests failed out of 2672` (`suite-debug.txt`) | matches `EXPECT_DEBUG_TESTS=2672` declared at the chain's head; confirmed by reading the ctest summary line directly, not the wrapper's `ASSERT OK` ✔ |
| G2 the long test ALONE | `Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 679.21 sec`, 1/1 (`suite-debug-long.txt`) | the one test excluded from G1, run with the suite to itself ✔ |
| G3 Release suite (fresh configure) | `100% tests passed, 0 tests failed out of 2673` (`suite-release.txt`) | matches `EXPECT_RELEASE_TESTS=2673`; equals G1 + G2 (2672 + 1) ✔ |
| G4 U0 replay, 3 arms | walk / ssn / ipm each `cells compared: 27  columns compared: 75  differences: 0` (`replay/comparison.txt`) | the expected 27/75/0 ×3 shape, against the same M6-close pin the artifact round used ✔ |
| G5 P-SYM, Release and Debug | compare rc **1** (expected — the gate is the classification); `DIFFERING OBJECTS: 3` = class A **3** + class B **0**, `UNEXPLAINED: 0`, negative control **72**; classification files **byte-identical between configs** (`diff` confirms) | `BEFORE=cb8b88c7`. Class A's 3 objects (`sqp_solver.cpp.o`, `soc_elastic_restoration.cpp.o`, `test_sqp_restoration.cpp.o`) are exactly this round's changed sources; the two under `src/` match `git diff --name-only cb8b88c7 -- src/ include/` exactly (`include/` changes produced 0 class-B movers, so only the `src/`-side match applies) ✔ |
| G6 install smoke | `PASSED` — `IpmSolver converged, objective = 17.0140174`, `SqpSolver converged, objective = 17.0140173`, both through one generic consumer, 19 standalone-include TUs compiled and linked (`install-smoke.txt`) | the script's own pass lines, read directly ✔ |
| G6 export contract | `export contract OK for platform=linux, align=32`; `EIGEN_MAX_ALIGN_BYTES OK: exported=32 computed=32` (`export-contract.txt`) | the third argument parsed from that same build's configure log, never a literal ✔ |
| G6 clang-format | rc 0 on this round's five changed C++ files (2 headers, 2 `.cpp`, 1 test) | `--dry-run -Werror`, per `gates.txt` ✔ |
| G6 `git diff --check` | rc 0, `DIRTY_FILES=0` | the tree the gates certified is the tree that was committed ✔ |

**No stamp is `-dirty`.** A grep for that token across every transcript in this
directory (excluding this README's own prose and unrelated identifier matches
like `qp_params_dirty_`) finds nothing; the chain log's header reads
`HEAD b1c153f6  dirty-files=0`.

Two tests did not run in either config, as on every gate of this branch:
`FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas`
(Skipped) and the two `EqpRefinementAb` cases (Disabled).

### Files (this section's additions)

```
gates.txt                              the chain log: HEAD, segment boundaries, every ASSERT and rc
suite-debug.txt                        the Debug suite's tail (long test excluded)
suite-debug-long.txt                   the long test, alone
suite-release.txt                      the Release suite's tail
replay/head-{walk,ssn,ipm}.csv         the three replay arms at this HEAD
replay/comparison.txt                  their comparison against the M6 close pin
psym-compare-{Release,Debug}.txt       P-SYM's raw object-by-object output
psym-classification-{Release,Debug}.txt the classification, which IS the gate
install-smoke.txt                      the install smoke's tail
export-contract.txt                    the export contract's output
```
