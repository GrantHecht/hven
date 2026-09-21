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
