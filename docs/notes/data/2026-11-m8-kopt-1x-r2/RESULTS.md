# M8 kopt-1x r2 -- RESULTS (DECLARATION.md, code head b64fd4cfd9b5fd90ea29d199e1e62a1f37ebfaba)

Chain: `legs/orchestrate.sh` (output `orchestrate.out`), declaration commit `ad081ea76`. **ALL_LEGS_PASSED**; no criterion changed after data.

| leg | verdict | evidence |
|---|---|---|
| P1 Release | 100% tests passed, 0 failed out of 3857 | `suites/ctest-rel.log` |
| P1 Debug | 100% tests passed, 0 failed out of 3857 | `suites/ctest-dbg.log` |
| P1b arm A's HS census | `HsSweepCorpus.Corpus` at the base PASSES (its pin asserts the Release census of 4) | `suites/baseA-hs-census.log` |
| P2 replay | 3 repetitions x walk / ssn / ipm, each vs arm A's dev row and B's dev row: 0 differences in all 18 comparisons | `replay/cmp-*.txt` |
| P3 TRAIN | `I4_PASS`: 476 rows, 10 movers = the declared 10 (better 0, worse 0, neutral 10), none missing or capped | `train/train_check.txt`; raw cells dropped (`MANIFEST-dropped.txt`) |
| P3b gray census | arm A 68 exits in 7 cells; arm B 58 in 2 cells (ALLINITC.K 50, LEWISPOL.K 8) | `train/gray_census.txt` |
| P4 runtime (walk, serial, solo) | counter identity on 81 (round, cell) pairs; 162 processes, 0 throttled; filtered instructions:u **B/A = 1.000018**, within [0.995, 1.005] | `runtime/walk/analysis.txt` |
| P5 mutations | control clean; Ma, Mb, Mc, Mg KILLED; Me1, Me2, Me3 terminate (no timeout); tree restored | `mutation/` |
| Intel branch 14 | YORKNET qp5: kOptimal, ratio 7.4e-08, 0 gray exits, `QpRdvRankRepair.YorknetQp5*` pass at ratio <= 1 | `CI.md` |

## Mutations

| id | mutation | killed by |
|---|---|---|
| control | none | clean |
| Ma | the polish acceptance does not read the candidate's row ratio | `QpRowContractPolish.ARepricedCandidateIsNotAdoptedPastOneFloorUnit` |
| Mb | a successful rank-repair `continue` spends the hold | `QpRdvRankRepair.YorknetQp5RepairDoesNotSpendTheHold{Border,Refactorize}` |
| Mc | the certifying site never returns to the loop | `QpCertificateSign.AnOptimalExitPastOneFloorUnitIsCountedAGrayExit`, `QpRdvRankRepair.YorknetQp5CertifiedGrayRowIsReturnedToTheWalk{Border,Refactorize}` |
| Mg | returns without the progress rule | `QpCertificateSign.AnOptimalExitPastOneFloorUnitIsCountedAGrayExit` (17 presentations instead of 2) |
| Me1 | return bound removed (progress rule alone) | none killed; terminates (the progress rule bounds it) |
| Me2 | return bound doubled (32) | none killed; terminates (no test names the number except through the constant) |
| Me3 | bound and progress rule removed | terminates in 13 ms at the minor cap (status kIterationLimit); `AnOptimalExit...` fails |

No (d) mutation: the 1x verdict-site target is not enabled (DECLARATION section 2).

## Movers

TRAIN (10, all as declared, all NEUTRAL): ALLINITC.K (minors 316 -> 377, numerical_error both); CRESC100.D (iterations 45 -> 40, minors 13586 -> 23791);
CRESC4.D (26 -> 65 iterations); CRESC4.K (26 -> 48); CRESC50.K (max_iter -> numerical_error, both unsolved); HS84.D, HS84.K (54 -> 53 minors);
LEWISPOL.K (67 -> 83 minors, infeasible both); OPTPRLOC.D, .K (42 -> 40 iterations). The CRESC, OPTPRLOC, HS84 and CRESC50 rows are the cells' values before
the first round (a hold that should not have fired is gone); ALLINITC.K and LEWISPOL.K pay one return on a gray exit they cannot repair.
Replay: none. Suites: the first round's HS-battery pins are withdrawn to the pre-round values (HS10 warm Release 28/42, total minors Release 3355 / Debug 3346,
HS15 failed-step arm Release 34/99/39/7, gray census 0); +7 new tests; one counter-count re-derivation (`AnOptimalExit...`: 2 presentations).

## Which of Codex's paths were real

1. Multiplier repricing at the polish: **real** -- the synthetic (two identical equality rows, incumbent [+25,-25], candidate [+0.198,-0.198], 6.88 floor
   units) adopts the candidate at the base (complementarity improves 25x while the row tolerance falls to the floor); RED -> GREEN under (a).
2. The hold consumed by a repair `continue`: real by construction (copied at loop entry); fixed by (b); `YorknetQp5RepairDoesNotSpendTheHold*` pins it
   (the post-repair seam on its own is absorbed by the 373 following passes: GREEN on the base, kept as a regression pin).
3. The polish opening and refusing both candidates: **real on the real fixture** -- with row 246 at 6.88x (certificate seam, the Intel numbers
   2.082e-7 / 30.26 / 0.198474) both stage candidates were infeasible by 2.4e-3, the ordinary step is a degenerate add (alpha ~1e-14, then 0), and only 8
   returns (each adding one working member) reach ratio 6.7e-8; at the base it certifies at 6.880 with one gray exit (RED), now GREEN.
4. Zero-motion bypassing the hold, 6. a revert restoring a bad incumbent: covered by (c)'s re-read with the final multipliers; no separate fixture.
5. Verdict-site refinement targeting 10x: the 1x target was built and measured and **not enabled** (a gray-row trigger moved ACOPP30 615 -> 2494
   minors; a 1x-first target at a structural dead end moved the classifier's refined point). (c) covers a refined point that certifies past 1x.

**Plus a defect found here:** the first round's `worst_row_ratio` read a working inequality row two-sided, so a zero-multiplier working row sitting
well inside its row counted as a gray exit (ratio up to 1.16e8) and fired the hold. Read one-sided, the HS battery returns to its pre-round
numbers and the gray census is 0; the TRAIN lane census moves 68 exits in 7 cells -> 58 in 2.

## Gray-exit census

- HS battery: Release **4 -> 0** (the four HS15 warm exits were the two-sided read), Debug 0 -> 0.
- TRAIN lane (`optimal_row_gray_exits`, per QP solve exit): arm A 68 in 7 cells (ALLINITC.K 50, LEWISPOL.K 8, CRESC100.D 3, CRESC4.D 3, CRESC4.K 2,
  CRESC50.K 1, OPTPRLOC.D 1); arm B 58 in 2 (ALLINITC.K 50, LEWISPOL.K 8) -- both are cells whose final status is not optimal (numerical_error,
  infeasible): equality rows at 2.5x with multipliers 49-76 that neither the polish nor the walk can close. The counter stays the record; the
  settler's "no hard refusal until a census finds the zone empty" stands (the zone is not empty).

## Open items

- (d) as specified was not delivered (above); a kOptimal that comes out of a verdict-site refinement past 1x is returned by (c), not refined to 1x.
- `kGrayReturnBudget` = 16 is set by the one fixture that needed 8 returns; the cost of unrepairable gray exits is bounded by the progress rule, not
  by the 16 (ALLINITC.K pays +61 minors over 50 exits).
- ALLINITC.K and LEWISPOL.K still exit gray in their elastic/restoration QPs; whether those equality rows deserve a refusal is the census decision.
- Apple/Accelerate: UNOBSERVED here (the macOS CI job passed).
