M8 R11 -- STEP 1, THE MEASUREMENT (before any fix was measured or declared; scratch-instrumented builds, never committed)
================================================================================
QUESTION. DenseSymmetricFactor::block_evidence() decodes a 2x2 Bunch-Kaufman D block as (trace +- disc)/2, which cancels
when |det| << trace^2 (DN-1 design R11, ../../2026-11-m8-dn1-design/results.txt E3d). Does any production path ever hit a
block where that decode and a det/trace decode disagree -- and does any decision built on the evidence change?

CONSUMERS (git grep over src/ include/ tests/ bench/ bindings/ examples/ at a8c540cf):
  production -- ONE: hven::detail::SchurComplement (include/hven/detail/kkt/schur_complement.h), the SQP walk's border
    stack, caches block_evidence() after every factor_c() and reads it through
      neg_eigs        -> expected_neg_eigs_delta()  (QpEngine::border_inertia_verdict, src/qp/qp_engine.cpp, read only
                                                      after needs_refactorization() is false)
      block_abs_eigs  -> cond_estimate(), nearly_singular(), and so needs_refactorization() (7 call sites in
                         qp_engine.cpp) and the conditioning latch's mild test (latch_mild, qp_engine.cpp)
      presence        -> near_null_direction() (the latch's culprit direction) and every reader's evidence_usable()
    The IPM, the IPQP/SSN subproblem modes, the dense session of the DN-1 design (not built yet; it has its own det/trace
    walk), bench/, bindings/ and examples/ never call it.
  tests -- tests/linear/test_dense_symmetric_factor_growth.cpp, tests/sqp/test_schur.cpp (through SchurComplement and
    one direct read), tests/sqp/test_accelerate_probes.cpp (Apple; its own copy of the walk).

INSTRUMENT. probe/probe-v1.diff on a `git archive` export of a8c540cf (build-r11/meas-src, Release, the evidence flags):
block_evidence() computes BOTH decodes of every block and returns the OLD one (so the run is arm A's), counting blocks,
sign disagreements, per-call negative-count disagreements, magnitude bit differences, det == 0, non-finite entries. The
SchurComplement keeps the det/trace evidence beside the old one and, at every reader call, re-evaluates the reader on it
and counts disagreements: expected_neg_eigs_delta's value, needs_refactorization, nearly_singular, cond_estimate's bits
and its side of schur_condition_max, near_null_direction's usability, the latch's mild test. Counters are written per
process at exit to R11_PROBE_OUT. probe/probe-v2.diff adds a 10 s cumulative tick and an immediate line on any
disagreement, for processes killed at a wall budget (whose exit line v1 lost). The engine runs arm A's arithmetic in
both: the replay below is EXACT against the reference.

RUNS (scripts/; MKL_NUM_THREADS=1; every run under flock /tmp/box-build.lock, pgrep empty)
  replay   27 cells x walk/ssn/ipm, one process per cell (scripts/meas_run.sh): EXACT against
           docs/notes/data/2026-10-m7-wx2-gates/gates/replay/head-*.csv, 0 differences each (replay/).
  census   57 cells, scripts/run_walk_census.sh's parallel-tiered v1 (census/): CENSUS_AS_DECLARED against
           bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv with PF-1's two movers (the baseline predates PF-1;
           a8c540cf carries PF-1): 42 exact, 2 = PF-1's rows exactly, 13 deadline-bound with dnf_* statuses. v1 lost the
           13 deadline-bound cells' child counts (killed at their budget), so:
  dnf      the 13 deadline-bound cells re-run on probe-v2, full budget, 7-wide pinned to cores 1-7 (scripts/dnf_run.sh,
           dnf/): statuses = the census's; counters to the last tick before each kill.
  TRAIN    CUTEst TRAIN `default`, hven-sqp lane, PF-1 P17's invocation, on a hven-bench clone (main bcf1d92, local pin
           commit to a8c540cf, the pin install overwritten with probe-v2; scripts/train_build.sh, train_run.sh; train/):
           16/23 optimal; every cell's status, outcome, iterations and factorizations equal PF-1's TRAIN reading
           (hven-bench runs/2026-11-m8-pf1-train/default/results.csv). 6 dnf_wall cells counted to their last tick.

RESULT PER CONSUMER (probe-summary.txt; counts summed over processes)
                                   replay walk  replay ssn/ipm   census (44+13)        TRAIN (23)
  block_evidence() calls                  530               0      21740 + 21745            46244
  1x1 blocks                            33890               0  1376005 + 1486235          3503400
  2x2 blocks                                0               0           15 + 0              55829
  2x2 sign disagreements                    0               0                0                  0
  calls whose negative count differs        0               0                0                  0
  old smaller |eig| == 0 (det != 0)         0               0                0                  0
  2x2 magnitude bits differ                 0               0               15              37066
  det == 0 / non-finite entry               0               0            0 / 0              0 / 0
  SchurComplement::expected_neg_eigs_delta
    reads / value differs              825 / 0          0 / 0   30604+23836 / 0        22277 / 0
  needs_refactorization differs             0               0                0                  0  (85322 calls)
  nearly_singular differs                   0               0                0                  0
  cond_estimate bits differ                 0               0               54                734
  cond side of schur_condition_max          0               0                0                  0
  latch mild test calls / differs       0 / 0           0 / 0            0 / 0          19584 / 0
  near_null_direction usability differs     0               0                0                  0
  2x2 blocks by cell: census f7_n1000_path_neutral 4, f7_n750_path_neutral_control 11 (54 cond reads differ in bits);
  TRAIN 17 of 23 problems (BRITGAS 37559, ACOPR118 4812, TAX53322 4803, READING6 3466, ...).

READING. No production path hits a block where the two decodes disagree on a SIGN: 55,844 2x2 blocks, none. The
magnitudes differ in their last bits on 66% of 2x2 blocks (a different, more accurate formula) and so does
cond_estimate on 788 reads, but no decision built on them moves: needs_refactorization, nearly_singular, the
condition limit, the latch's mild test, the inertia count. The fix is engine-neutral on this corpus; the proof
declares NO MOVERS against arm A.

WHY NONE WAS EXPECTED. A Bunch-Kaufman 2x2 pivot [[a, c], [c, d]] is taken only when |a| < alpha|c|, and with
|d| < alpha*rowmax and |a|*rowmax < alpha*c^2, so |a d| < alpha^2 c^2 and det <= -(1 - alpha^2) c^2 < 0: one eigenvalue
of each sign, |det| >= 0.59 c^2. The old decode loses the negative only when trace^2 >> |det|, i.e. |d| >~ 1e8 |c|,
where the true smaller eigenvalue is ~|det|/|d| <~ 1e-8 |d|; the old decode then returns 0 or ~1e-16|d| for it, so
cond_estimate >= ~1e8 at least and -- where the sign actually flips -- >= ~1e15 > schur_condition_max = 1e9, and the
walk reads the inertia only after needs_refactorization() is false. A wrong sign could reach the walk only through a
C whose condition estimate stays under 1e9, which a wrongly signed block cannot produce. The probe confirms it.
