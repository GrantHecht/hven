# RESULTS-PF-12 (measurement only; protocol in DECLARATION-PF-12.md)

Binaries: u0 = M6 19f7674f (sha256 32a67ecc217a131c...), m8 = 331b31837 + env levers (main leg binary
sha256 eb603bbb295c0389...; the lever patch later gained `HVEN_PF12_IWS`, rebuilt binary 6d83ec8e0f4c...,
used only for the IWS-off ablation; both patches in scripts/pf12-lever.patch). Instrument: perf stat :u,
3 rounds alternated, median; every cell's three rounds are identical on status/factorizations/qp_minors
(instruction spread 0.000 %). u0 rows equal the U0 baseline row on status/factorizations/qp_minors for all 8
cells (C179 re-confirmed); m8 rows equal the close-census rows except f7_n1000_path_corrupted (PF-1).
Full tables: analysis.txt, attribution.txt. Wall is informational and not quoted.

## PF-1
f7_n1000_path_corrupted at m8: status Optimal, factorizations 3590, qp_minors 4762, qp_subproblems 3
(qp_fact_per_qp 3588;1;1), with qp_failure_budget (max_consecutive_qp_failures) = 1 AND, as a contrast, at the
shipped value 3 (identical row, raw/m8-budget3-*). U0: Optimal, 2976, 4049. Bar: U0 status and
factorizations <= 5952: MET (3590 = 1.21x U0). The minor-cap hit (29828 at the close census) is not
reproduced at 331b31837 at either budget; the cell no longer depends on M1. The mechanism of the hit was not
located because it does not occur; it was removed by changes after the close census (not bisected here).
Instructions: 2.33e12 at budget 3 (same computation as budget 1).

## PF-2 (instructions:u / qp_minors, medians)
| cell | U0 | m8 | ratio | total-instr ratio | minors ratio |
|---|---|---|---|---|---|
| f7_n1000_path_neutral (A) | 1.0877e8 | 1.4047e8 | 1.291 | 1.473 | 1.140 |
| f7_n800_path_neutral (A) | 8.610e7 | 1.1209e8 | 1.302 | 0.467 | 0.359 |
| f7_n825_path_neutral_control (A) | 9.548e7 | 1.1090e8 | 1.161 | 1.286 | 1.108 |
| f7_n750_path_neutral_control (A) | 7.607e7 | 1.0470e8 | 1.376 | 1.525 | 1.108 |
| f7_n1000_path_corrupted (A, incl. setup hop) | 3.730e8 | 4.897e8 | 1.313 | 1.544 | 1.176 |
| f7_n800_path_corrupted (B) | 4.514e8 | 5.880e8 | 1.303 | 1.676 | 1.287 |
| f7_n2000_path_activity (B, 1.5-1.9k minors) | 1.865e8 | 3.791e8 | 2.033 | 2.540 | 1.250 |
| f7_n2000_path_physics (B, 1.4-1.6k minors) | 1.993e8 | 4.797e8 | 2.407 | 2.825 | 1.173 |
Tier B cells with ~1.5k minors carry a fixed setup/assembly cost in the numerator, so their per-minor
ratios overstate; they are context only. Bar (<= 1.15 on tier A): NOT MET on 5 of 5 (best 1.161). The
discharge is open. Note n800_neutral solves in 36 % of U0's minors (the walk changed path) yet each
minor still costs 1.30x.

## Attribution
- Flat self-instruction profile (perf record -e instructions:u -c 1e8, no call graph; --call-graph dwarf
  cannot open on this box), n1000_neutral and n825_control: 95 % (n1000) / >100 % (n825) of the per-minor
  excess is inside MKL Pardiso's SOLVE phase: mkl_pds_*_blkslv_ll (+1.0e7/minor n1000), dsytrs forward/back
  substitution, and mkl_spblas dcsr..mvout_par (the iterative-refinement matvec, +7e6). hven's own detector
  (detect_dependency, dependency_test, ...) self time +1.4e5/minor (0.4 %), polish_* +3e4 (0.1 %), border_*/
  bordered_* +6.7e5 (2 %; negative on n825). So the cost is extra triangular solves and refinement sweeps
  issued BY those components, not their own arithmetic. Factorizations per minor are unchanged
  (0.901 -> 0.905 on n1000), i.e. the excess is solves, not factors.
- Which of the three components issues those solves cannot be read from a flat profile (no call graph, no
  per-component counters in the walk row). Component ablation: the only toggle is
  `independent_working_set` (it gates E2 detector/screen, the E4 in-EQP polish and BPS bordered answer
  together; they are not separately toggleable). With it OFF (scratch lever, one run per cell):
  n1000_neutral 1.067x U0 per minor (Optimal, 11198 fact, 12390 minors); n825_control 1.008x
  (Optimal, 8606 fact, 10713 minors). So the bundle accounts for essentially all of the 1.29 / 1.16 excess
  (about 0.22 of 0.29 and 0.15 of 0.16); the 0.07 left on n1000 is outside the bundle. Per-component split
  of the bundle: UNRESOLVED (needs per-component toggles or a call-graph-capable profile).

## Recommendations (settler decides)
1. Add per-component toggles (or counters of solves issued by detector / polish / bordered refinement) in
   the walk, measurement-only first: the flat profile proves the cost is solves, the ablation proves the
   bundle owns it, nothing splits the bundle yet. Cheapest next measurement; no behavior change.
2. Detector/screen: probes that re-solve against the current factor only when a candidate row's
   dependency verdict could change (skip when the working set and factor are unchanged since the last
   clean verdict; skip the screen on minors that only add a bound row to a face already certified
   independent). Expected effect: bounded by the bundle's 0.15-0.22; needs the split to size.
3. BPS bordered answer: skip the refinement sweeps (the mvout/blkslv pairs) when the first solve's
   backward error is already under the certificate tolerance (bordered_regularized_residual); cap
   refinement at the sweep count that changed the verdict in the census. Same bound.
4. E4 polish: already terminal-only (polish_footprint_open gate); self cost 0.1 %, so keep; check its
   solve count first (extra solves per QP, not per minor, so it should matter little at 10^4 minors).
5. Realistic bar: n825 (1.161) needs ~0.011 cut, n1000/n800 about half of their bundle share, n750
   (1.376) most of it. If conditional solves cannot reach 1.15 on n750/n1000 without changing paths or
   losing a conversion, put the residual (1.07-1.2x per minor, with fewer minors on n800) to the settler as
   an accepted price with these numbers.

## Open items
- Component split of the bundle (item 1); n750 and n1000_corrupted/n800 ablations not run (one IWS-off
  run each on two cells only); PF-4 pending changes would move the m8 numerator; the unobserved cause of
  the PF-1 hit's disappearance (bisect only if wanted); HOLDOUT not run; f7_n10000_path_physics not run.
- Scratch worktrees .scratch/m6-19f7674f, .scratch/m8-331b31837 and build dirs remain (git-ignored) for
  re-use; remove with `git worktree remove` when done.
