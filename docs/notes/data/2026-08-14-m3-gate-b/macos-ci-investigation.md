# Gate B: the macOS CI lane's red, investigated

Date: 2026-08-14. Investigator: delegated CI-forensics agent (read-only; all
evidence from GitHub Actions via `gh` plus local file reads at m2/HEAD-adjacent
checkout). PR #5 (`m3` -> `main`), HEAD run 31824897327 at 6535566c.

## 1. Run history and the failing-set-per-commit table

`ci` runs on `m3`, oldest first. The only failing job in every failed run is
**macos-clang-release** — with one exception noted below the table. Test sets
extracted from each run's "The following tests FAILED" block.

| commit   | run id      | conclusion | HsBattery.CrashBasis | Continuation.ProbeBudget | CorpusRunnerProcess.WallDeadline…SOLVE | CorpusTask6bPhaseB.FourLevers | SsnEngineLocal.WeaklyActive |
|----------|-------------|------------|----|----|----|----|----|
| a6068028 | 31730136830 | success    | (suite not yet wired — "chore: open the M3 branch") | | | | |
| 0ba1b9fc | 31736708703 | failure    | F | F | pass (0.18 s) | F | F |
| ccc10207 | 31746383539 | failure    | F | F | F | F | F |
| b2806d65 | 31795680125 | cancelled  | — | — | — | — | — |
| dbe3db83 | 31795942979 | failure    | F | F | pass | F | F |
| 8d078ed4 | 31798799302 | cancelled  | — | — | — | — | — |
| 8500bbc7 | 31799421479 | failure    | F | F | F | F | F |
| e6e2399b | 31803082323 | failure    | F | F | F | F | F |
| b12b72b7 | 31806242817 | failure    | F | F | F | F | F |
| 95bfb16d | 31807326142 | failure    | F | F | F | F | F |
| d64f1400 | 31816957427 | cancelled  | — | — | — | — | — |
| 92212b9a | 31812442998 | failure    | F | F | F | F | F |
| 258c3376 | 31817935342 | failure    | F | F | F | F | F |
| ab8aeda3 | 31824068254 | cancelled  | — | — | — | — | — |
| 6535566c | 31824897327 | failure    | F | F | F | F | F |

- `cf65a03` (phase A step 3, the commit that wired the suite in) has **no run
  of its own** — its first CI exposure is 0ba1b9f, the phase-A review-round
  commit pushed on top of it.
- **main is green**: M2 merge c6635e1b (31723731821) and M2.5 merge 9ac4a03f
  (31760849488) both succeeded, macOS included. Red begins at exactly the
  first m3 run that contains the SQP suite (0ba1b9f, 2026-08-13T19:36Z) and
  never varies except for WallDeadline's flicker.
- **One-off Linux exception**: run 31803082323 (e6e2399, the docs-only fold
  commit) ALSO failed linux-clang-release, with a different four-test set:
  `SqpDriverRadius.FloorRaisesTheRestorationRequest` (radius 4.768e-07 vs
  tr_min 1e-10), `B1Gate.EqualityOnlyWarmSolvesAreBitIdenticalAcrossTheRepair`,
  plus `CorpusTask6bPhaseB.FourLevers` and `SsnEngineLocal.WeaklyActive`. The
  same source passed Linux at 8500bbc immediately before and at b12b72b
  immediately after, so this is runner-environment variance (GitHub's Linux
  fleet is CPU-heterogeneous and MKL dispatches by ISA), not a code event. It
  is evidence that two of the five macOS failures — and two tests currently
  green on Linux — sit close enough to a numerical edge that a different
  microarchitecture alone can flip them. Worth carrying into gate B's Linux
  honesty too, but out of scope here.

## 2. Attribution verdict: import-era, all five

The failing set is **byte-stable from the first run that contains the suite
(0ba1b9f, import era / phase-A review round) through HEAD (6535566, phase B)**.
Same five tests, same assertion lines, same observed-vs-expected values
(HsBattery 861/858 vs 862/859; Continuation 304 vs 303; SsnEngine the same
six assertion flips; Task6bPhaseB the same float-byte mismatches). No phase-B
commit (b12b72b, 92212b9, 258c337, 6535566) added, removed, or moved a macOS
failure. **Nothing in the red set is retarget-era; nothing here is
attributable to phase-B code.** The only variation is WallDeadline, which
PASSED at 0ba1b9f and dbe3db8 and failed everywhere else — flake, not drift.

This is the migrated suite meeting real Apple hardware for the first time,
exactly as the gate-A record anticipated. Reconciliation with the gate-A
note's claim (docs/notes/2026-08-14-m3-gate-a-review-request.md:77-80): the
note says the Accelerate halves "compile and run in the macOS CI lane
(first Apple execution of this increment is gate B's Mac leg; CI results
before then are watched, not asserted against pins — float pins are
context-pinned to this machine anyway)", and the review APPROVED "macOS
watch-only until gate B" (same note, ~line 228). "Compile and run" was never
a claim of green; the gate-A-era macOS runs (0ba1b9f, ccc1020) were red with
this same set, and that red is precisely the watched divergence gate B now
has to adjudicate. **No contradiction; the bill has simply come due.**

## 3. Per-test diagnosis and disposition

### 3.1 HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions — class (a)

tests/sqp/test_hs_battery.cpp:999-1000. Battery-total minor-iteration pins:
`minors_off` observed 861 vs pinned 862, `minors_on` 858 vs 859 — off by
exactly one in both arms (one HS problem spends one fewer QP minor on
Accelerate), while `facts_off`/`facts_on` (320/323), the seeded counts, and
the seeding/changed problem lists all PASS on Accelerate. The pin's own
comment (line ~992): "ASSERTED AS THE OBSERVED VALUES (MKL Pardiso, clang++).
No Apple value exists for these — they are newer than the last Mac pass — so
if they move on Accelerate the counts belong in the divergence register."
**Disposition: per-backend arm**, values 861/858 taken as CI-observed Apple
values (stable across 8 failed runs, 0ba1b9f..6535566; provenance: runs
31736708703 and 31824897327, macos-15/arm64 runner class, Release,
clang/AppleClang). Route through the divergence register per the comment's
own instruction. Not a code fix; the answer-invariance half of the test
(status/f equality) passes.

### 3.2 Continuation.ProbeBudgetBoundsAFailingProposal — class (a), pre-adjudicated

tests/sqp/test_continuation.cpp:1082. `factorizations` 304 vs pinned 303 —
and the assertion's own message is "MKL Pardiso; Accelerate observed 304
(D19)". The comment block above it records divergence-register entry D19
(origin note `docs/notes/2026-08-01-accelerate-register-3.md`, Grant's Mac
pass at BASE fe47aef — note: that file did NOT migrate; the reference
dangles in this repo) and states the pin was deliberately left MKL-only "so
the next Mac pass re-adjudicates a number rather than a range". CI has now
performed that Mac pass 8 times: **304, every run, Release** — exactly D19.
**Disposition: write the per-backend arm the comment was structured to
receive** (Accelerate expects 304), citing D19 plus CI run provenance. Also:
either migrate/recreate the register note or repoint the reference. Arms D/E
of the same file are MKL-only by declaration and currently PASS on
Accelerate, so no new observation is owed there.

### 3.3 CorpusRunnerProcess.WallDeadlineEmitsADnfBudgetRowWhenTheSOLVEPhaseIsForcedTiny — class (c), infrastructure

tests/sqp/test_corpus_cells.cpp:869-901. The test forces a 1 ms SOLVE budget
and expects the parent's 20 ms poll to catch and kill the child, emitting a
`dnf_budget` row. Its stated timing assumption: the cell "genuinely takes low
hundreds of milliseconds (~150-270 ms) — comfortably past the runner's 20 ms
poll interval". At HEAD the emitted row shows the child FINISHED —
`Optimal`, wall_s 0.0837 — i.e. on the Apple-silicon runner the SOLVE phase
fits inside roughly one poll gap (plus CI scheduler jitter), so the parent
never observes an over-budget child and no kill happens. Flaky in the
observed direction (passed at 0ba1b9f in 0.18 s and at dbe3db8; failed the
other 6 runs). **Disposition: infrastructure fix, not an arm** — the
enforcement mechanism works (the Linux/Windows legs and the SETUP-phase twin
prove it); the fixture is too fast for this hardware. Slow the cell on this
lane (bigger n / a heavier cell) or assert the deadline in-child rather than
by parent poll. No Apple numerics are involved; nothing to observe or pin.

### 3.4 CorpusTask6bPhaseB.TheShippedKSsnConfigurationIsUnmovedByTheFourLevers — class (a), with an unpinnable twist

tests/sqp/test_corpus_cells.cpp:2415-2419. Compares a live `run_cell`
against the COMMITTED task-6b artifact CSV; the banner says in capitals
"MKL-OBSERVED, NOT RE-VERIFIED ON ACCELERATE." On Accelerate every integer
column, status, and the per-QP shape PASS for both cells — only the five
float residual columns fail their `{:.9e}` byte-compare, differing in the
6th-10th significant digit (e.g. kkt_residual 6.295824606e-14 vs the
artifact's 6.295816157e-14). Crucially, the live Accelerate value for
f7_n1000_path_neutral is **not even stable run-to-run on the CI runner**:
3.053657327e-10 / 3.053661768e-10 / 3.053663988e-10 across runs 31817935342,
31824897327, 31812442998 (f7_n1000_bound_neutral IS byte-stable across all
runs). The test pins MKL_NUM_THREADS=1 for exactly this reason on MKL, but
hven's Accelerate session stores-without-applying num_threads (the ci.yml
rig-step comment, lines 282-290, states this), so Accelerate's reduction
order is uncontrolled. **Disposition: backend-scope the five float
byte-compares** — keep them asserted on MKL, and on Accelerate follow
docs/testing.md's context-pinned float discipline (line 739): report, don't
assert (or compare at a tolerance), because a byte-pin is dishonest on a
backend whose threading the code cannot yet pin. Do NOT commit an
Accelerate-observed byte value for path_neutral — three runs already
disagree, and the workflow's own rule requires two-run reproduction before a
row is committed. The integer columns' clean pass is a genuine gate-B result
worth stating.

### 3.5 SsnEngineLocal.WeaklyActiveRowFinishesUncertain — class (b), genuine divergence of the predicted kind

tests/sqp/test_ssn_engine.cpp:1932-1960. The fixture is BUILT on a tie: row 1
has c* = 0 and lambda* = 0, "a coin flip whose outcome … decided by
rounding" (the test's own banner). On Accelerate every coin lands the other
way: default-run row 1 active-not-uncertain (expected inactive+uncertain),
bare-mode inactive (expected active), full-mode active-not-uncertain
(expected inactive+uncertain) — and `ssn_bulk_flips` = 4 vs the pinned 1.
This is the §2.3-flavored divergence the design note predicts (Accelerate's
zeroTolerance/zero-pivot semantics perturbing a solve that sits exactly on a
tie), not a broken safeguard — but the flips counter is the one assertion
that is a CLAIM ("no oscillation") rather than a coin call, and 4 > 1 means
the Accelerate trajectory oscillates more before settling. **Disposition:
adjudicate before pinning.** The CHR-partition property itself ("a tie must
end UNCERTAIN, whichever way the activity coin lands") is what the test
should assert portably; the specific coin outcomes and the flip count need a
real-Mac (or careful CI-based, two-run-reproduced) adjudication of whether 4
flips is legitimate for the Accelerate trajectory or evidence the oscillation
guard under-damps there. UNOBSERVED-hold on any rewritten pin until that
ruling; the CI observations (stable across all 8 runs) are citable input to
it. This is the only one of the five where a design-level ruling, not just a
value, is owed.

## 4. The rig-report-accelerate.txt artifact: failure-ordering, not a gap

At HEAD the macOS job's step conclusions are: Test (step 7) **failure**;
"Assert PCH engagement", "install smoke", "export contract", and **"Emit the
Accelerate observations for derivation" (step 11) all SKIPPED**; "Upload the
Accelerate observations" (step 12, `if: always()`) ran, found nothing, and
errored per `if-no-files-found: error`. The rig binary WAS built (log shows
`hven_golden_rig_report` linking), but the emit step has no `if: always()`,
so ctest's failure short-circuits it. **Answer: failure-ordering.** The lane
has never yet produced the artifact on m3 (every suite-bearing run failed
Test first). Two follow-ups: (1) the derivation this artifact exists for is
blocked behind five failures that are adjudications, not regressions —
consider `if: always()` (or `!cancelled()`) on the emit step so observation
and adjudication can proceed in parallel; (2) note the skip also silently
suppresses the PCH gate, install smoke, and export contract on macOS — those
have not run on m3 either.

## 5. What gate B can honestly claim about the Mac leg

- The Accelerate build compiles, links, and executes the full migrated suite
  on Apple hardware; 1042-of-1047-ish executed tests pass; the failing set is
  five, closed, stable across 8 runs, and entirely import-era — phase B
  moved nothing on macOS.
- Four of five are the documented context-pin bill coming due (two counter
  pins with pre-declared divergence-register routing, one MKL-committed
  float artifact, one timing fixture); one (SsnEngineLocal) is a genuine
  predicted behavioral divergence needing a design ruling.
- CI runs on real Apple hardware ARE citable observations (with run-id
  provenance and the runner-class label) — but the Accelerate lane's
  unpinned threading means float observations need two-run byte-agreement
  before commitment, and f7_n1000_path_neutral already demonstrably fails
  that bar. Counters observed stable across 8 runs meet it comfortably.
- Gate B cannot claim a green Mac lane yet, and should not chase one by
  widening pins: the honest path is the five dispositions above, after which
  green is meaningful.
