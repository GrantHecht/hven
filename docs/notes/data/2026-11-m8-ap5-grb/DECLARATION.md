# M8 AP-5 G-RB -- DECLARATION (one `CommonOptions::return_best`, one best-iterate order; committed before any leg; never edited afterwards -- a failure is DECLARATION-r2.md)

## Change
Code head 89614ba63db246106548c293d5014e6f82f69f67 (`feat(m8-ap5-grb)`) on origin/m8 0cfcec52f (branch g-rb). Design: docs/notes/2026-11-m8-ap5-interface-design.md
section 2.5 O1, section 3.3, rows G-RB, Q5 (owner answer: yes, full unification). Rename table: docs/migration-0.2.md section G-RB; consumer list CATCH-UP.txt.
- A. `hven::ReturnBest {kNever, kOnLimit, kOnAnyNonOptimal}` and `CommonOptions::return_best` (default kNever). `IpmOptions::return_best` (bool) and
  `SqpOptions::return_best_on_max_iterations` are removed (break, per the design table). Options-dump: `common.return_best` replaces the two lines (counts unchanged);
  pins re-derived (declared): kIpmOptionsFields 16->15, kSqpOptionsFields 17->16, kCommonOptionsFields 9->10, sizeof IpmOptions 472->480, SqpOptions 600->608,
  CommonOptions 80->88 (tests/drivers/test_options_dump.cpp).
- B. One rule for which exits substitute (`detail::return_best_applies`): kOnLimit = kIterationLimit, kWorkLimit, kTimeLimit; kOnAnyNonOptimal adds kAcceptable, kStalled,
  kDiverging, kNumericalError; kOptimal, kInterrupted and **kInfeasible (certified or not) never substitute** under any value. Interrupts keep their own point rule
  (stop rule / kStopReturnBest), as today.
  - SQP: `kOnLimit` is widened from the iteration cap to the work and time limits (`st.limit_exit`); a `SolveBudget::max_minor_iterations` probe-budget stop is NOT one
    (it is a per-call budget whose ordinary exit the continuation driver relies on; documented, pinned). `kOnAnyNonOptimal` is new: the non-finite exits
    (manual exit, QP-subproblem failure exit) and the restoration exits return the best iterate in full when one is recorded.
  - IPM: `kOnLimit` is new (the old bool was all-or-nothing = kOnAnyNonOptimal); the substitution sites read the RESOLVED phase status.
  - IPM work/time limit door (pre-factorization break): before this change the result claimed `returned_best_iterate` and read the best iterate's row but returned the LAST
    primal point (found by the new forced-exit test: x != best_iterate.x). It now substitutes the pair, as every other substituting door does. Declared as a defect fix inside
    this event; return-best-on cells only.
- C. THE ORDERING (settler item C355 R1). One order for the tracker behind best_iterate, the callback/token paths and return_best, on both engines:
  `detail::best_improves(violation, f, best_violation, best_f, feasibility_tol)`: key = (max(violation, feasibility_tol), f); a candidate replaces the incumbent only if its
  key is lexicographically STRICTLY smaller (NaN violation never wins; a NaN objective counts as +inf). `violation` is each engine's own convergence feasibility measure:
  IPM `max(equality, inequality)` residual (the value `convergence.feasibility` reports; NaN if either is), SQP `kkt.feasibility` (the inf-norm measure its convergence test
  reads). Reasoning: (1) feasibility first, because a best point is a hand-off (continuation, stop-and-return) and a feasible point with a mediocre objective beats an
  infeasible one with a good objective; (2) the band `feasibility_tol` is the tolerance the convergence test itself calls feasible, so points inside it are equally feasible
  and the objective decides -- clipping at the band (rather than comparing with a tolerance) keeps a weak order, so tracking is transitive and order-independent;
  (3) strict, so the first of equal points wins (the old IPM `<=` moved the best to the latest tie, which on a problem with no equalities meant the latest iterate regardless
  of objective and inequalities); (4) each engine reads its own measure because the two feasibility norms differ (G7 note), the order is the same. IPM
  `best_iterate_criterion` stays as a per-engine override (kEquality, kInequality, kKkt, kObjective keep `<=`); the new value `kFeasibilityFirst` is appended last (existing
  enumerators keep their numbers) and is the new default.
  **CONFLICT WITH THE DESIGN, reported:** design Q5 recommends "the best ordering stays per engine" (IPM by criterion, SQP by (h, f)). This follows the later settler item
  C355 R1 instead; it only changes which iterate is *tracked/returned* when armed (return-best, callback or token), never a solve's trajectory, and it reverts alone (the
  ordering lives in one function and one tracker line per engine).

## Declared movers
- Corpus: NONE. No corpus cell turns return_best on (it was off on both engines; the corpus tool never set it), and the tracker is armed only by return_best, a callback or a
  token; the ARMED replay legs (callback count + unfired token) change which best_iterate is tracked, which no corpus column reads. P3 is byte-exact (EXACT comparator, no
  residual gate), armed included.
- Tests re-derived: the 12 files that set the removed fields are migrated mechanically (`o.return_best = x` -> `o.common.return_best = ...`; SQP budgeted-mode fixtures map
  to kOnLimit; their expectations are unchanged and pass unchanged); `StopPollSeam.T3ALadderStopScoresItsPointIntoTheBestTracker` sets `best_iterate_criterion = kEquality`
  explicitly (its premise is the `<=` tie rule, formerly the default). Pre-declaration dev run of the full Release suite at the first head: 1 failure
  (OptionsDump.IpmDumpHeadIsVerbatim, the new dump line), fixed; no other existing test moved under the new default order.
- New tests: ReturnBestForcedExits.* (7, tests/drivers/test_exit_door_walk.cpp) and EqualityBasisModes.F2InconsistentEqualitiesNeverSubstituteTheBestIterate (x6, tests/sqp).
  ctest -N expectation: 3852 at the head (3850 at origin/m8 + 1 + the 6 parameterised... see P1 for the observed count, pinned in the verdict).

## Forced-exit pins, per value (inside P1)
For kNever, kOnLimit, kOnAnyNonOptimal on BOTH engines (tests/drivers/test_exit_door_walk.cpp): iteration limit, work limit, time limit (substitute under kOnLimit and
kOnAnyNonOptimal only; the primary point IS best_iterate.x); non-finite abort (kOnAnyNonOptimal only); certified kInfeasible via restoration on the SQP (never; IPM at this
head still reports the restoration-locally-infeasible exit as kStalled, so its kInfeasible pin is G-S's); uncertified kInfeasible (inconsistent equalities, SQP, all modes,
never); the probe-budget stop (never); the shared rule table over every status; the order's unit pins (band, strictness, NaN, f-after-h).

## Runtime gate
NOT REQUIRED. The per-iteration tracker comparison is reached only when the tracker is armed (`best_tracking_armed()` / `st.best_armed`, unchanged guards; the arming
expression now reads `common.return_best != kNever` in place of the bool); an unarmed solve (every corpus cell and every default solve) executes the same per-iteration code
as arm A. The new exit-time code is a few status comparisons on the exit path (once per solve). The counter-identity of P3/P4 and the armed legs stand in for it.

## Arms and binaries
Arm A = origin/m8 0cfcec52f, `git archive` build (legs/prebuild.sh; build-grb/armA), build-grb/armAbin/hven_sqp_corpus sha256
33d68b317524bf69bd35f565fe016b29ddafc553b71104daa44a5064936b0db0.
Arm B = build-grb/rel/bench/hven_sqp_corpus at the code head, frozen by legs/arm_b.sh to build-grb/armBbin, sha256
08b0800683543d91639c5e409fb39a8a3c7cb574e9c20d572424352d544f525b.
Trees build-grb/rel (Release), build-grb/dbg (Debug), all targets, clang++, SAFER_FAST, GIT_DIR/GIT_WORK_TREE at configure, GIT_CEILING_DIRECTORIES = the worktree root on every
command, googletest from the main checkout's fetched source.

## Proofs and pass criteria (each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first as its own command, setsid -w, foreground, MKL_NUM_THREADS=1, ctest -j4)
P1  ctest Release and Debug at the code head: 100% passed (count recorded in suites/*-summary.txt and VERDICTS).
P3  replay 27 cells x (walk, ssn, ipm): arm A vs arm B, scripts/compare_replay.py EXACT, "differences: 0" x3; ARMED (--callback count + HVEN_LEG_UNFIRED_TOKEN=1) vs arm B
    unarmed, 0 x3.
P4  interior: unmoved (arm A vs arm B) 0; promoted (vs bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv) 0; armed 0.
P5  install smoke, default and clang front ends, each followed by scripts/check_export_contract.sh: pass x2.
M   mutations (legs/mutations.sh + legs/mutate.py; a Release `git archive` copy of the code head; hven_sqp_seam_tests, hven_drivers_tests, hven_interior_tests in full and
    hven_sqp_tests filtered to Continuation.*:WarmStart.*:SqpDriverNonfiniteRollback.*:QpModes/EqualityBasisModes.*, each --gtest_fail_fast; the control fails nothing; each
    id alone). MUST BE KILLED by a failing test:
      s1  a kInfeasible exit substitutes (the rule)              s2  SQP restoration exits bypass the rule
      l1  kOnLimit does not cover work/time limits (the rule)    l2  SQP limit exit narrowed back to the iteration cap
      o1  the order compares f before the violation              o2  non-strict improvement
      o3  no tolerance band                                      n1  IPM non-finite abort substitutes under kOnLimit
      n2  SQP non-limit exits never substitute                   p1  SQP probe-budget stop not excluded
      t1  IPM limit door claims the best and returns the last point
    REPORTED, may survive: r1 (the IPM shared measure reads the equality residual only).
    Verdict: legs/mutation_verdict.py.
Not run: TSan (no concurrency or stop-path change), goldens (no trace key changes), the runtime gate (above), HOLDOUT (never).

## Order
legs/chain.sh <code head sha> 08b0800683543d91639c5e409fb39a8a3c7cb574e9c20d572424352d544f525b 33d68b317524bf69bd35f565fe016b29ddafc553b71104daa44a5064936b0db0 (full code-head sha):
ctest rel, dbg -> replay -> interior -> install smoke -> mutations -> mutation verdict; STOPS at the first failed leg. Evidence: suites/, replay/, interior/, install-smoke/,
mutation/, VERDICTS.txt; raw dumps under the ignored .scratch/grb-*, MANIFEST.txt (sha256, size, path) committed. ON FAIL: stop, commit the evidence, report; DECLARATION-r2.md.

## hven-bench (outside this repository; not an hven proof)
No change needed: lanes/hven/hven_bench_cell.cpp neither sets nor reads the removed options (the string `return_best` appears in a comment only).

## Pre-declaration runs (NOT evidence, disclosed)
Dev Release builds; the full Release ctest on the first compiled head (1 failure, fixed, above); targeted runs of the new tests (a time-limit fixture and the IPM limit-door
defect were found and fixed with them); the head's Release and Debug trees and both arm binaries built; legs/mutate.py dry-applied to a scratch copy of the head (every id
matches exactly once); bash -n / py_compile on every instrument. No replay, interior, install, ctest-at-head or mutation leg has run.
