> Placed by the settler from the fix-round implementer's hand-back (subagent writes to `.superpowers/` are refused). Worktree `.claude/worktrees/agent-a15e08e7de2a57ddc`, branch `nc3fix1`, commit `d11366c9` (on 054e4c78).

**The fix.** `orient_saddle_direction` now takes `probe` and `seen`. A zero-length ride (`a*|d|inf <= step_tol`) counts as a miss in `saddle_exit_misses` when its first blocker is any of:
- the previous pass's drop;
- any `probe.ineq_exempt` / `probe.bound_exempt` member;
- any member carrying `kPtReleased` in the point memory at this point.

The rule is stated in the 4c header text and in the declaration comment.

**Reachability.** The reviewer's scenario is reachable. A temporary fuzz (since removed) covered n = 3–4, 2–4 rows through the origin, g = 0, H with 1–2 negative eigenvalues, and a seeded active subset: 400 trials, 3246 cases.
- The guard changed 66–104 outcomes; the count varies run to run (MKL address sensitivity).
- In 10 of them the unguarded walk certified kOptimal at a vertex that a feasible negative-curvature direction leaves: a false certificate.
- In the rest (about 54) the unguarded walk certified a TRUE local minimizer, and the guard turns these into a miss (kNumericalError).
- The false certificate also occurs with one negative eigenvalue (n = 3, four rows).

**Fixture.** `QpNc3SaddleExit.ADegenerateSaddleVertexIsNeverCertifiedThroughTwoZeroLengthRides`
- It is fuzz trial 44, hardcoded: n = 3, one negative eigenvalue, four rows through 0, rows 2 and 3 seeded active.
- It runs on both algebras and asserts: a feasible descent direction at the origin (grid check); if kOptimal, then not at the vertex and the reduced-Hessian minimum > 0; misses ≥ 1; the seam descent invariant.
- Before the fix, on both algebras: kOptimal, |x| = 1.66e-87, f = 6.6e-175, exits = 1, misses = 0. That is a certificate at the saddle.
- With the fix: kNumericalError, misses = 1.

**Riders.** (b) is in solver_counters.h. (c) re-points the test comment to the task-dir REPORT. (a) was not done.

**Measurements** (Release, MKL_NUM_THREADS=1, core 2, against a separately built 054e4c78):

| Check | Result |
|---|---|
| Corpus, 51 rows per algebra | 0 rows moved, E2 on and off, both algebras |
| U0 | 17/13/13, 0 differences vs NC3's U0 run |
| Key tests | 43/43 |
| Release suite | 2905/2905 |
| Debug suite | 970 + 968 + 966 = 2904 |
| Long test, alone | 161 s |
| clang-format | 0 |

TRAIN was not run, because the fix moves nothing measured.

SIGNOFF: M7-W2-NC3-FIX1-REPORT
