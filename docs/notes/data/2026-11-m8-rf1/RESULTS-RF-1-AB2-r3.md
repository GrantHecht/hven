# RESULTS-RF-1-AB2-r3 -- gate A+B2 read-out on the rebased head (DECLARATION-RF-1-AB2-r3.md)

Arms: C' = `b1fe115d0` (`rf1-C-rebased`), AB2' = `b4f72e5a7` (code; the gate's tests ran at that head). Evidence in `gate-AB2-r3/`.
Counters only; no wall-clock claim. Binaries frozen in `BINARIES.txt` (r3 block) before the first leg. After the legs, one
tests-only commit (`3242fedfe`, section CI) pinned two off-reference readings; `src` and `include` are unchanged since `b4f72e5a7`.

## Leg T -- TRAIN 238 roster cells x {D, K}, both arms (`gate-AB2-r3/train/ab2_check.txt`)

476 rows per arm, 0 missing, 0 lost solves. **Zero false infeasibility certificates**: the 12 hven-sqp rows with status
`infeasible` are the same 12 cells in both arms (BURKEHAN, FLETCHER, HS88, HS92, LEWISPOL, LOOTSMA, each D and K); AB2' adds none.
**CRESC50.D**: `max_iter`, 276 iterations, 6794 factorizations, f 0.59339, in BOTH arms, identical rows: not certified infeasible
(it was a certificate at the pre-rebase AB2). CRESC100 D and K: `optimal` in both arms, identical rows (211 it / 16943 fact; 38 it /
17010 fact): not lost (both were lost at the pre-rebase AB2).

| lane / mode | C' optimal | AB2' optimal | C' fact | AB2' fact | C' iters | AB2' iters |
|---|---|---|---|---|---|---|
| hven-ipm D | 113 | 113 | 6113 | 6113 | 4082 | 4082 |
| hven-ipm K | 111 | 111 | 7364 | 7364 | 5333 | 5333 |
| hven-sqp D | 105 | 105 | 97758 | 100913 | 2508 | 2533 |
| hven-sqp K | 105 | 105 | 102960 | 99494 | 1972 | 2066 |

(Per lane the 119 problems of the roster; optimal+acceptable equals optimal in every column.) hven-sqp totals: optimal 210 ->
210, factorizations 200718 -> 200407 (-311), iterations 4480 -> 4599 (+119). hven-ipm unchanged. Two status movers, both
non-solves, both worse in kind of nothing solved: `sd-hven-sqp-HS99EXP.D` and `.K` `max_iter` -> `stalled` (infeasible points, h 3.99e5 ->
4.05e5; f -4.2267e7 -> -6.5792e4; 11111 -> 6543 and 12550 -> 7013 factorizations; the stall window ends the restoration). Lost
solves versus C': **none**.

## Leg P -- the pilot's 62 failing cells (`gate-AB2-r3/pilot/report.txt`; 0 cells threw)

Solved (optimal or acceptable): C' 12 (all acceptable) -> AB2' **19** (8 optimal + 11 acceptable); baseline 12, prior AB2 19.
**No C'-solved cell lost.** Rescued: ACOPR118 004-008 (kOptimal at f = 129660.68; rungs and the elastic route), ACOPR57 013 optimal
and 018 acceptable, ACOPP118 001/002 acceptable -> optimal. SMBANK (10 cells): acceptable in both, 192-224 majors (C' 154-285),
through the trust-region floor exit.

- **BRITGAS (9 cells)**: none solved in either arm. AB2' factorizations: 019: 10055 (C' 10055, identical); 020/021: 16212 (C' 18227);
  022/023: 18159 (C' 32190); 024: 25937 (C' 27278); 025/026: 19967 (C' 33998); 027: **25373** (C' 84250). Statuses
  max_iter/numerical_error -> stalled at lower f, one major each; the stall window ends the restoration phases.
- **YORKNET 055-057**: still NOT solved and not better: all three stop at major 1 `stalled` with 29241 factorizations, 31 restoration
  iterations, restoration h 51001 -> 5851 (the stall window fired), f_out 128617 (C': stalled at 28849-33132 factorizations, f 14.3-14.5e3
  at 16-56 majors). The stall window did not rescue these cells: the phase lands at a high-f point the resume gates refuse. Other
  YORKNET: 054 certified `infeasible` after 102 majors (C' stalled, f 14682); 050/053/058-061 numerical_error or stalled at similar f.
  Reported, no bar.
- CRESC100.D/K are TRAIN cells (leg T above), not pilot cells.
- MSS2 (7): numerical_error / stalled in both (7 majors against 10); DALLASM (5): unchanged rows; ACOPR57 011:
  numerical_error -> max_iter at f 41737.7 (lower f).

## Leg W -- the QP walk census (`gate-AB2-r3/census/`)

57/57 rows per arm. **Cr3 vs ABr3: 0 mismatches** (`Cr3-vs-ABr3.txt`). Each arm against the frozen m7-close baseline: the SAME 7 cells
disagree on the asserted columns (f7_n1000_path_corrupted, f7_n2000_path_corrupted, f7_n5000_path_physics, f7_n5000_path_activity,
f7_n800_path_neutral, f7_n800_path_corrupted, f7_n800_path_warm): those are rebase-and-protocol movers, present at C', not RF-1.
Protocol deviation as declared (T2 solo within its lane, not machine-solo): any wall-dependent difference is a candidate only; none
arose between the arms.

## Leg M -- mutations (`gate-AB2-r3/mutations.out`)

Control clean; **M1-M20 all killed**, R1 killed (reported). Verdict `MUTATIONS_AS_DECLARED`. Killers: M11 (stall never fires) 5
(`Rf1RestorationStall.*`, `Rf1RestorationSeam.*`); M12 3; **M13** `Rf1RestorationSeam.ThePhaseHandsBackItsBestPointNotItsLast`;
**M19** `Rf1RestorationSeam.TheStallExitTakesTheReductionResume`; **M20 (F-1)** `Rf1RestorationSeam.ARowRefusalOnTheSwitchRunTakesTheSwitch`;
M14 3, M15 `RestorationBestOrder.*`, M16 3 (incl. `SqpLadderSeam.TheReductionResumeTakesTheNoClaimPointAndCountsIt`), M7 4, M9 10. The
predicted survivors (M9, M15, M16) did not survive.

## Legs C, S, Y

ctest Release: 100% passed, 4086/4086 (`suites/ctest-rel.log`); Debug 4086/4086 (`suites/ctest-dbg.log`) at `b4f72e5a7`. Install smoke
and export contract: rc 0 (default and clang front ends). Production `libhven.a` (Release tree): `nm -C | grep -c capture_limit` = 0,
`strings | grep -c restoration_capture_limit` = 0 (`symbol-check.txt`).

## CI

Run 45036848180 at `0ee832a62` (docs on top of `b4f72e5a7`): windows-clang-release success, linux-coverage success, **linux-clang-release
FAILED** (2 tests) and **macos-clang-release FAILED** (1 test). Both are runner/off-reference readings, pinned as observed in `3242fedfe`
(tests only; `src`/`include` unchanged):
- Linux: the runner's MKL dispatch probe read `vendor=AuthenticAMD avx512f=1 mkl_auto_branch=2 -> OFF-REFERENCE (portable arms)`, so
  `HsSweepCorpus.Corpus` and `HsSweepCorpus.Hs15WarmKdOffWithTheFailedSubproblemStep` took the portable arm (21/39, 39) but the runner
  read the reference column (22/40, 40). Linux Release now pins 22/40 and 40 whatever the probe says (the off-reference Windows/macOS
  arms keep 21/39 and 39).
- macOS (Accelerate, real hardware): `Rf1RestorationSeam.APhaseFallingTenPercentPerWindowIsNotCutAndAFlatterOneIs`: restoration_iters 4
  (want 1) at window 1; at window 2 `kStalled`, not certified (want kInfeasible, certified). Pinned as observed under
  `USE_ACCELERATE_SPARSE`; every other assertion of that test held.

Run 45049552467 at `3242fedfe`: **windows-clang-release, linux-clang-release, macos-clang-release, linux-clang-coverage all success.**
The results commit that follows adds documents only.

## Open items for the settler

- YORKNET 055-057 are not rescued by the stall window (stop at major 1 at 29241 factorizations; f 128617 against C' 14.3-14.5e3).
- The Linux Release HS15 pin is now probe-independent (the AMD off-reference runner reads the reference column); Debug and the
  off-reference Windows/macOS Release arms read 21/39.
