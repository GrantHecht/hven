# DECLARATION-RF-1-AB2-r4 -- the narrow re-check of gate A+B2 after F-2 (successor; -AB2, -r1, -r2, -r3 kept unedited)

Written and committed, together with every leg script it names, before any leg; never edited afterwards (a failed or void leg
chain is answered by a `-r5` file, this one kept). Criteria are never changed after data. Design:
`docs/notes/2026-11-m8-rf1-design.md` with amendments C-1 and B2-1; the interpretations of `DECLARATION-RF-1-AB2.md` section 2
and `-r3` section 2 stand. The r3 read-out (`RESULTS-RF-1-AB2-r3.md`) stands for every leg this file does not re-run.

## 1. Why a successor gate: F-2

r3 found pilot cell 054-YORKNET (`default` + `advanced.adaptive_dual_regularization_max=1e-6` +
`advanced.trust_region_growth=kRatioOrFeasibilityStep`) **certified infeasible** at AB2' (102 majors) on a feasible problem
(C' stalled at major 22, f 14682.59, feasibility 4.4e-7). Diagnosis (re-run of both arms with traces, plus a throwaway
instrumented build of AB2' that dumped the restored point; not evidence of this gate, quoted in the fix commit):

- The certificate is the pre-existing certified arm of `enter_restoration`: the 4th restoration's regularized restart ended
  kOptimal (stationarity 5.3e-10) at `kkt_r.feasibility` 8.68e-6 > 2 feas_tol.
- The solve's equality basis had reduced the rows (247 of 256 retained) and the phase restores the retained rows only (E1-R).
  At the restored point the retained rows stand at inf 1.8e-10 (l1 3.7e-10); the whole violation (l1 1.19e-5) is on two
  DROPPED rows (114: -8.68e-6, 221: -3.22e-6), priced by no phase multiplier, whose gradients are out of the retained rows'
  span (least-squares residuals 9.7e-4 and 3.7e-4). The point is **not** stationary for h: the linearized l1 violation over all
  256 rows, in the box, falls 15.5% within an inf-norm step of 1e-3 and 100% within 1e-2. The certificate is false.
- QC-1's margin read `kkt_r.feasibility` over all rows; its argument (beyond the margin a row's slack is free and the phase
  prices it) does not hold for rows the phase does not carry. The hole predates RF-1 (unchanged since C'); RF-1's routes (the
  retry rungs, 49 box projections, the after-rungs elastic route into four restorations) only steered the iterate there.

**F-2 (`994619746`)**: the claim's measure (`optimal_claims`, read by the certified arm and by the reduction resume's
"certifies nothing") is the convergence test's feasibility with the dropped rows left out; with no reduced basis it is
`kkt_r.feasibility` itself, so a solve whose basis does not reduce is untouched. Must-kill
`SqpEqualityBasis.F6bAPhaseThatFeasesOnlyTheRetainedRowsCertifiesNothing` (measured by the author: with F-2 kOptimal at (1, 0),
one reduction resume; without it kInfeasible certified at (1, 0.1)). Development reading (not a bar): 054 at F-2 ends
`numerical_error` (`restoration_refused`, the entry cap) at major 106, its 4th phase handed back through the reduction resume
(h 2.36e-4 -> 1.19e-5). Dev Release ctest at `994619746`: 4087/4087.

Cells 055-057 (`elastic_rung_engine=kTier`, stopped at major 1): diagnosed, NOT changed by this gate (the remedy is proposed to
the settler, not implemented); reported only.

## 2. Arms, binaries, legs and bars

Arms: **C'** = `b1fe115d0` (local branch `rf1-C-rebased`), its r3 lane and pilot binaries reused as frozen in `BINARIES.txt`'s
r3 block (sha-checked by the leg script) and re-run; **ABr4** = `994619746` (F-2 on `ce210b804`; `git diff` of
`src include tests` against the committed tree is empty at this file's commit). The ABr4 lane (NEVER=1) and pilot binaries
are built for this gate and frozen in `BINARIES.txt` (an r4 block, committed BEFORE the first leg). The r3 rows of AB2'
(`gate-AB2-r3/train/ABr3`, `gate-AB2-r3/pilot/ABr3`) are read as committed, not re-run. Toolchain and machine as r3 (clang
22.1.8, `-DHVEN_FP_MODE=SAFER_FAST`, Linux x86-64, AMD Ryzen 7 5800X3D, MKL 2026.2, `MKL_NUM_THREADS=1`). Every step under
`~/.local/bin/box-lane --` (never `--exclusive`); counters only, no wall-clock claim. Evidence in `gate-AB2-r4/`.

| leg | instrument (in `legs/`) | bar |
|---|---|---|
| T | `r4_corun.sh`: `train_enum.sh` (238 roster cells x {D, K}, CAP 300 s, one pinned core) on lane Cr3 and lane ABr4; `ab2_train.py` ABr4 vs Cr3 and ABr4 vs r3's ABr3 (movers F-2 caused); `r4_check.py train` | **hven-sqp: 0 lost solves versus C' and 0 false certificates** (a row of ABr4 with status `infeasible` or `sqp_infeasibility_certified=1` where C' has neither): `VERDICT TRAIN_BARS_MET`. The whole roster runs (hven-ipm rows too, which F-2 cannot move; reported). Missing rows 0. HOLDOUT never |
| P | `r4_corun.sh`: `pilot_run_r1.sh` Cr3 and ABr4 (the 62 cells of `pilot_cells.tsv`; budget 300, 900 s kill reported, one pinned core each); `pilot_report.py` (vs C', and vs r3's ABr3); `r4_check.py pilot` | **054-YORKNET does not certify infeasible; no cell solved by C' or by r3's AB2' is unsolved at ABr4; no false certificate on any cell**: `VERDICT PILOT_BARS_MET`. Solved counts reported. Void if any cell throws for an infrastructure reason |
| M | `r4_chain.sh` -> `mutations_ab2_r4.sh` with `mutate_ab2_r4.py`; control first; the r3 filters plus `SqpEqualityBasis.*` on hven_sqp_tests | control clean; **M19, M20, M21 each killed** (`VERDICT MUTATIONS_AS_DECLARED`) |
| C | `r4_chain.sh`: `ctest.sh` rel and dbg at the committed head | "100% tests passed" in both logs; counts reported |
| CI | `gh workflow run ci.yml --ref rf1` at the pushed head, `gh run watch <id> --interval 120` | green on all four lanes; an off-reference (Accelerate/Windows/AMD runner) pin that differs is recorded as observed and pinned in a separate tests-only commit, then CI re-run; Apple values not observed are UNOBSERVED |

## 3. Mutations (M21 new; M19 and M20 re-run because F-2 reads beside them)

M19 the reduction-resume extension reverted (r3's anchor; F-2 changes what "certifies nothing" reads there); M20 the F-1
exclusion dropped (the sibling false-certificate fix, r3's anchor); **M21 (new) F-2 reverted: the claim's measure reads every
row** (anchor `if (resto_rows) {` + `NlpEval ev_priced = ev_r;`, rewritten `if (false && resto_rows)`; must-kill
`SqpEqualityBasis.F6bAPhaseThatFeasesOnlyTheRetainedRowsCertifiesNothing`). M1-M18 and R1 are not re-run: F-2 touches none of
their code and none of their pinning tests moved (dev Release ctest 4087/4087).

## 4. Not run

HOLDOUT (never). Wall-clock. The QP walk census (F-2 reads only the restoration exit; r3's census stands). Install smoke and the
production-library symbol check (F-2 adds no option, no seam and no symbol). Accelerate and Windows readings (CI only).
