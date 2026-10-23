# M7 W2 A1 -- the implementation round's measurements

The A1 lever `IpqpOptions::ipqp_margin_stop` (design page
`docs/notes/2026-10-m7-w2-a1-stopping-mu-design.md`, rulings C125-C127), built per the page's
sections 3-4 and measured per its pre-registered section 5. **Verdict: neither ON-2 nor ON-2X
passes C1, so under section 5 the field does not land** (section "Verdict" below). The code is on
branch `w2-a1-impl` at the default `kOff`, for the settler's ruling.

## Provenance

* hven: branch `w2-a1-impl` at `ebef6ca2` (off `m7` `f7e5324c`), Release, the uniform flag regime.
  Every measurement leg and both suites ran at that commit; the one later code commit adds a test
  only (the Q11 pin, below), run in Release and Debug.
* Toolchain: clang 22.1.8 (Fedora 22.1.8-4.fc44); Intel oneAPI MKL. Hardware: AMD Ryzen 7 5800X3D.
* Date: 2026-10-23. Terms: every leg its own `flock /tmp/box-build.lock` hold, one solver
  process at a time, `MKL_NUM_THREADS=1`, `OMP_NUM_THREADS=1`, `pgrep -a hven_bench_cell` empty
  before each leg (`logs/chain-meas.txt`). Counters only: no wall-clock figure is quoted; the
  `wall_s` columns are informational. Accelerate: UNOBSERVED. No TRAIN, no HOLDOUT cell was run.

## What is here

* `scripts/chain_meas.sh` -- the measurement chain (every leg below); `scripts/score_all.sh`
  re-derives every `out/*.txt` from the CSVs; `i2_compare.py`, `f7_compare.py` the scorers;
  `mutants.py` the mutation run; `build.sh`, `leg.sh`, `suite.sh`, `chain_suites.sh` the build
  and suite legs.
* `out/` -- one CSV per leg, and the scored transcripts.
* `logs/` -- the chain transcript and every leg's stderr.

## The instruments

| id | what | binary | output |
|---|---|---|---|
| I1 | the section 4 fixture class under OFF / ON-1 / ON-2 / ON-2X | `hven_ipqp_seam_tests`, `DISABLED_IpqpMarginI1` | `out/i1-table.csv` |
| I2 | E1's 29 cells, OFF and ON-2 (the shipped value) | `hven_sqp_ipqp_e1_arm --gate [--margin-stop kExtendAndDemote]` | `out/i2-off.csv`, `out/i2-on2.csv` |
| I2 | ON-1, ON-2X, ON-2 through the seam | `hven_sqp_seam_tests`, `DISABLED_MeasurementArm` | `out/i2-seam-ON{1,2X,2}.csv` |
| I2 diag | final-pass row exponents of committed rows, by truth | same, `FinalPassExponentsOfCommittedRowsByTruth` | `out/i2-diag-exponents.csv` |
| I3 | U0 replay, field `kOff`, three engines | `hven_sqp_corpus` | `out/u0-head-*.csv` |
| I3 | U0's ipm arm at ON-2 vs OFF (in-process) | `hven_sqp_ipqp_e1_arm --f7-path --cells <U0>` | `out/u0-ipm-{on,off-arm}.csv` |
| I3b | the six small F7 path cells, ipm, OFF vs ON-2 | `hven_sqp_ipqp_e1_arm --f7-path` | `out/i3b-{off,on}.csv` |

The I2 measurement arms run in `hven_sqp_seam_tests`, not `hven_ipqp_seam_tests` as the page's
section 3.6 named: `e1arm::solve` hands the face to `QpEngine::refine_on_face` and reads
`ipqp_exit_is_a_usable_step` (`sqp_solver.cpp`), whose closure is the whole library, which that
binary already recompiles under HVEN_TESTING. The seam-reached ON-2 equals the shipped value
bit for bit on all 29 cells (`out/i2-verdicts.txt`, last lines).

## Results

**OFF identity.** I2 OFF reproduces `a4-gate-mu1e-2.csv`: 29 cells x 65 columns, 0 differences
(`out/i2-off-vs-committed.txt`). U0 at `kOff` vs the SCW gate record, exact: walk / ssn / ipm
27 x 75, **0 / 0 / 0** (`out/u0-exact-vs-scw-record.txt`). The suites are below.

**I2 (C1(b), (b'), (c), (d))** -- `out/i2-verdicts.txt`. ON-2 and ON-2X are identical on all 29
cells: every candidate's history is informative (`ipqp_margin_fallback_suspects = 0` on every
cell), so the two semantics never differ on E1. 11 cells extend.

| reading | ON-1 | ON-2 | ON-2X |
|---|---|---|---|
| polish accepted, 5 scattered non-RED `m1e-6` (OFF 0) | 1 | **5** | **5** |
| cells worse than OFF END-TO-END (missed / false actives) | 1: `n4000_af30_m1e-6` 0/0 -> 0/204 | **4** | **4** |
| tier + polish factorizations / OFF's 746 | 782 = 1.048 | 782 = 1.048 | 782 = 1.048 |
| new RED cell | none | none | none |

The four cells ON-2 / ON-2X make worse: `e1_f7_n4000_af01_m1e-6` 0/0 -> 0/23,
`e1_f7_n4000_af10_m1e-6` 12/0 -> 0/13, `e1_f7_n20000_af10_m1e-6` 0/0 -> **0/1014**,
`e1blk_f7_n20000_af30_m1e-6` 2/0 -> 11/0. Six cells improve (e.g. `n20000_af01_m1e-6` 129/0 ->
0/0, `e1blk n20000_af10_m1e-6` 848/0 -> 3/0, `n4000_af30_m1e-4` 0/56 -> 0/0). The RED cell does
not reach a candidate at cap 60 (0 suspects), as the page predicted.

**Why (I2 diag, `out/i2-diag-exponents.csv`).** At the final suspect pass every truly active
committed row reads `e` in [-0.03, 0.03]: the exponent test never mistakes a price for noise on
E1. But the truly inactive committed rows spread `e` over roughly 0.3 to 1.0+, and `e >= 0.5`
misses 23 of 59 (`n4000_af01`), 13 of 205 (`n4000_af10`) and 1014 of 3546 (`n20000_af10`) --
rows whose slack is still collapsing across the last step, so their multiplier falls slower than
`mu`. The demotion then hands the polish a smaller but still over-committed face that passes its
rank pre-screen. On `e1blk n20000_af30` the one over-committed row IS demoted; that cell's
worse reading (2 -> 11 missed) comes from the polish the demotion unlocked, not from a miss.

**I1 (C1(a))** -- `out/i1-table.csv`.

* Gate-8, built from `Sigma = 2 mu_stop / s^2`, `mu_stop = 1.25e-9` measured: every member as
  predicted under every arm. `Sigma = 250, 1000, 25000`, `h = -1`: OFF stands flagged; every ON
  arm extends once (one iteration, `mu` 6.25e-12) and DOWNGRADES (read 1); the `h = +1` partners
  certify. `Sigma = 2.5e5`: stands with the flag under every arm; ON-2 / ON-2X report both
  sides UNCERTAIN (demoted 2), ON-1 leaves them ACTIVE. `Sigma = 10`: S empty, identical to OFF.
* The row family `R(rho, lambda*)` (section 4 item 2, the exact symmetric pair): **construction
  failure** for `rho = 10, 200, 2000`. The 1 % assertion did not hold and the declared fixed-point
  fallback (three iterations) left measured `rho` = 0.76, 0.20 and 31.8, outside
  `[rho/2, 2 rho]`; `rho = 2000` reaches the iteration cap under OFF. The pair's own presence
  moves the stop (it converges further as `d` shrinks), which is the case the page anticipated
  and declared as a construction failure, not a verdict. `rho = 1e6` was built (measured
  1.0045e6): OFF commits the pair ACTIVE; no ON arm acts (0 suspects) -- the pair rows'
  multipliers read `e = 0.000` at a candidate sitting on the `mu` floor (`1e-12`), so the
  predicted extension (ON-1) and demotion (ON-2, ON-2X) did not happen; the certificate is
  today's under every arm. (Also a construction coincidence: `sqrt(1e6) = 1000` is exactly the
  band's `kIpqpTightBandFactor * kIpqpWeakActiveFactor`.) The `lambda* = 1` row is committed
  ACTIVE in every member under every arm.
* The in-band truly active member (`lambda* = 50 sqrt(mu_stop)`, measured
  `lambda*/sqrt(mu) = 100.6` at the candidate): informative history, every arm: 0 suspects, no
  extension, ACTIVE. Uninformative (warm from its own point, 0 iterations): ON-1 and ON-2 extend
  (1 fallback suspect, 3 iterations) and leave it ACTIVE -- **ON-2's predicted false demotion
  did not happen**: after three extension iterations the final pass's history is informative
  (`e` = 0, priced) and `lambda*/sqrt(mu_final) = 1768` is outside the band, which the page's
  keyed prediction ("<= 1000 means in band, otherwise not") reads as no demotion. ON-2X: no
  action, identical to OFF, 1 fallback suspect.
* Warm gate-8 `Sigma = 250`: 2 fallback suspects under every ON arm; ON-1 / ON-2 extend and
  downgrade the noise seed; ON-2X does not act (OFF's standing, flagged certificate).

**I3 / I3b (reported, not gating)** -- `out/u0-ipm-on-verdict.txt`, `out/i3b-verdict.txt`.
The page's section 3.5 predicted 0 firings on U0's ipm arm; **2 of 27 cells fire**
(`f7_n1000_path_warm`, `f7_n800_path_warm`), and **all 6 small path cells fire** in I3b, every
suspect through the band fallback (`ipqp_margin_suspects == ipqp_margin_fallback_suspects` on
every cell): late, warm-converged SQP majors. ON-2 acts on them: statuses unchanged (none lost),
`ipqp_face_uncertain` 10-236 -> 1251-1782 per cell, factorizations 35 -> 81 on
`f7_n800_path_physics`, 47 -> 85 on `f7_n800_path_warm`. ON-2X would not act on any of them
(every suspect is a band suspect -- by construction, not run).

## Verdict against section 5, measured vs predicted

| clause | ON-1 pred. | ON-1 meas. | ON-2 pred. | ON-2 meas. | ON-2X pred. | ON-2X meas. |
|---|---|---|---|---|---|---|
| C1(a) gate-8 | PASS | PASS | PASS | PASS | PASS | PASS |
| C1(a) row family | PASS (no data) | construction failure (rho 10/200/2000); rho 1e6 keeps cert, no action | PASS (no data) | same | PASS (no data) | same |
| C1(a) `lambda* = 1` partner | PASS | PASS | PASS | PASS | PASS | PASS |
| C1(a) in-band, informative | PASS | PASS | PASS | PASS | PASS | PASS |
| C1(a) in-band, uninformative | PASS | PASS | **FAIL** by construction | **PASS** (not demoted; final history informative) | PASS | PASS |
| C1(b)/(b') polish >= 4 of 5 | n/a | (1 of 5) | PASS at 4 | **PASS, 5** | as ON-2 | **PASS, 5** |
| C1(b)/(b')/(c) no cell worse | FAIL | **FAIL** (1 cell) | OPEN | **FAIL (4 cells)** | OPEN | **FAIL (4 cells)** |
| C1(b)/(b')/(c) facts <= 1.10 x | OPEN | PASS 1.048 | OPEN | PASS 1.048 | OPEN | PASS 1.048 |
| C1(d) no new RED | PASS | PASS | PASS | PASS | PASS | PASS |
| C2 TRAIN | not run | not run | TIE / NOT EXERCISED -> kOff | **not run** (nothing landed) | not run | not run |

**What ships.** Section 5: "If neither passes, the field does not land, and the round reports
which clause failed." Both fail C1(b)'s second clause (ON-2X its verbatim copy (b')) on four E1
cells. C127's switch to ON-2X applies only when ON-2 fails C1(a) and ON-2X passes; ON-2 passed
every C1(a) member that could be measured, so the switch does not trigger either.
`kExtendAndDemote` keeps ON-2's semantics in the branch's code (`kShippedExtendAndDemote`), and
the default is `kOff`.

**C2 (TRAIN) was not run**: section 5 runs TRAIN on "the landed semantics of
`kExtendAndDemote` only", and nothing landed. The default stays `kOff`.

## Deviations and decisions, declared

* I2's measurement arms live in `hven_sqp_seam_tests` (above).
* The seam's arm enum has a third value, `kExtendAndDemoteBand` (ON-2 by name), so the band-acting
  semantics stays measurable whichever arm the shipped value takes.
* A candidate-returned solve costs no extra read: the candidate's read IS the read OFF takes.
  The extra factorization exists only when the extension converges (the second read). Pinned by
  `IpqpMarginStop.ABudgetExitDuringTheExtensionReturnsTheFirstStopResultWhole`.
* A converged extension whose second read the factorization budget refuses is treated as item 4's
  cap exit and returns the candidate (else it would turn OFF's `kOptimal` into `kMaxIter`).
* `IpqpMarginObserver` (test-only) was added for the I2 diagnosis.

## Suites, OFF inertness, mutants (I4)

**Suites** at `ebef6ca2` (`logs/chain-suites.txt`, `logs/suite-*-tail.txt`): Release
**3216 / 3216**, Debug **3215 / 3215** with the long test excluded, the long test
(`Continuation.ProbeBudgetBoundsAFailingProposal`) **1 / 1** alone. Against the SCW record's
3202 / 3201: +14, the twelve `IpqpMarginStop.*` and two `IpqpMarginSeams.*` tests; the five
`DISABLED_` measurement tests are listed, not run. The Q11 pin added after the first mutation
run (`IpqpMarginSeams.AtAnUninformativeFinalPassOn2DemotesByTheBandAndOn2xDoesNot`) passes in
Release and Debug (`hven_ipqp_seam_tests` 18 / 18 in Debug). Install smoke PASSED, export
contract OK (align 32), `clang-format --dry-run -Werror` on the range's 17 C++ files clean, the
whitespace diff check clean (`scripts/g6.sh`).

**OFF inertness, the light P-SYM form.** Under `kOff`, `resolve_margin_arm` returns `kOff`; the
stopping predicate reads `iopts` itself; no suspect pass, no `prev_yi` copy, no candidate, no
extension branch is reachable; the post-loop path is the same `finalize` with `demote == nullptr`
(the classifier's one statement, unchanged arithmetic) and the three per-solve trace events are
emitted in the same order with nothing between them. Measured: U0 exact 27 x 75 x 3 = 0
differences against the SCW record; E1 OFF 29 x 65 = 0 differences against the committed A4 CSV;
both suites green, every pre-existing golden pin unmoved except the two trace golden lines, which
move by the five declared trailing keys only. No object-level P-SYM classification was run (the
restructured `ipqp_engine.cpp` object necessarily differs); the settler's gate chain would.

**Mutants** (`scripts/mutants.py`, `logs/mutants.txt`, `logs/mutants-m10.txt`): 17, one per new
rule, each applied alone, rebuilt, its suite filter run. **17 / 17 KILLED** -- M1 exponent never
flags; M2 band inverted; M3 ON-2X acts on band suspects; M4 extension keeps the configured slack;
M5 warm-kill restarts cold; M6 an extension evidence failure is carried; M7 demotion forces
INACTIVE; M8 a non-converged extension returns the live result; M9 a budget-refused second read is
finalised; M10 ON-2X demotes by the band at an uninformative final pass (SURVIVED the first run,
killed by the Q11 pin above); M11 validation admits the sentinel; M12 the fold overwrites; M13 a
returned candidate's certify event dropped; M14 rows read a scrambled previous multiplier; M15 the
fallback counter counts informative suspects; M16 demotion disabled; M17 the candidate not
finalised first.

## Proposed for the register (names provisional; the settler's to assign)

* **A1-R4** -- the exponent test's `e >= 0.5` misses truly inactive rows whose slack is still
  collapsing at the final pass (the I2 diagnosis); on E1 every truly active committed row reads
  `|e| <= 0.03`. A different reading (a lower threshold, a slack-trend term) is a NEW rule and
  needs its own pre-registration; nothing here tunes it.
* **A1-R5** -- the row family `R(rho, lambda*)` does not hold its regime: the pair moves the stop.
  A row fixture that stays in regime needs a different construction (re-designed, not re-keyed).
* **A1-R6** -- the band fallback fires on warm-converged SQP majors in the corpus (2 of 27 U0 ipm
  cells, 6 of 6 small path cells), which the page's section 3.5 predicted would not happen; under
  ON-2 it multiplies `ipqp_face_uncertain` and costs factorizations, statuses unchanged.
* The page's "the extra read included" for a returned candidate: there is no extra read on that
  path (declared above).
