# M8 TR-A -- DECLARATION-r4 (successor to DECLARATION-r3.md, which stays frozen; committed before any r4 leg; never edited afterwards -- a failure is DECLARATION-r5.md)

## Why r4
r3 (code head db4f8e60f) ran: A/A calibration PASS (all four modes within [0.999, 1.001]: the frozen band [0.995, 1.005] is CONFIRMED, spread table in runtime/aa/), runtime P2 PASS (interior 1.000080, ipm 1.000000, ssn 1.000000, walk 1.000002), ctest Release 100% (3823, 0 failed). **ctest Debug FAILED**: HumanTraceGoldens.{FourProblemsTwoEnginesFourRungs, TheRestorationFixture, TheElasticFixture} -- the goldens were derived from a Release build and carry residuals at roundoff (3.72e-13 vs 3.71e-13, 4.11e-15 vs 6.24e-15, 1.30e-23 vs 1.43e-23) and one tie (1.125 printed 1.13 vs 1.12) that Debug reads differently. The chain STOPPED at ctest-dbg; replay/interior/install/mutations did not run (r3 evidence kept under r3/).
This is a test-instrument defect (a golden comparing last digits), found by the first Debug run; not a library defect.

## What moved since r3 (code head d5fe12d26; the diff r3..r4 is: console phase-exit line text, the goldens' comparison, 4 IPM detail goldens)
- `ConsoleTraceSink` phase-exit detail line reads `stationarity` / `complementarity` (was `kkt_inf` / `barr_inf`), consistent with the G7 JSON keys of rider 12; the four IPM `*_detail` goldens are re-derived (only that text moves).
- `HumanTraceGoldens` compare by value: structure, words and integers exact; each scientific-notation token within 2 % or both below 1e-9. Self-tested (TheNumericGateAcceptsRoundoffAndRefusesRealDrift: roundoff accepted; a changed value, integer or word refused). CLAUDE.md section 7's near-ulp rule applied to text.
- No other src/ or include/ change: the engines, writer and counters are byte-identical to r3's head; the runtime/replay/interior legs are nevertheless re-run (the corpus binary is re-linked: arm B sha256 below).

## Arms and binaries
Arm A = origin/m8 `fa97d6bf0` (unmoved), sha256 8ad4cc11eb5ea78b9988bdd7fa5cbb01e4f4993ed7151c187420adef97b3e477 (fail-closed). Arm B = code head d5fe12d26, `hven_sqp_corpus` sha256 c661694c1aaace8e7365a205523c023f5008bfa16a1a1dd43090c4efca1dd9d2. The A/A calibration of r3 STANDS (arm A binary, instrument and cells are head-independent; the band is confirmed, not revised).

## Proofs and pass criteria: as DECLARATION-r3.md (P1 ctest Release AND Debug at the code head, P2 runtime with the confirmed band, P3 replay, P4 interior, P5 install smoke + export, P6 off-path scan incl. planted cases, P7 contract tests / goldens, M mutations: the r2 table + p1a p1b p1c p2 p3 p4 p5 p11a p11b, v4 killed)
Pre-declaration dev evidence (NOT evidence): a dev run of the ten new/changed mutations against d5fe12d26 killed each one (v4 by the exact factorization pin; p2 by the single-accumulation scan AND, shown separately in r3/p2demo.out, by the no-sink regression test reading 8.15 against 12.41), the control failing nothing; the Debug build of every target at d5fe12d26.

## Order
runtime/run_all.sh <armB> <armA> (solo, serial, first) -> legs/chain.sh d5fe12d26 <armB> <armA> (ctest rel, dbg -> replay -> interior -> install smoke -> mutations -> verdict). Stops at the first failure.
