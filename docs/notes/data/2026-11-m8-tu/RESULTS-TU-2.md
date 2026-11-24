# RESULTS-TU-2

Governing text: DECLARATION-TU-2.md (`c81a5894f`) as amended by DECLARATION-TU-2-r2.md (`2b730047f`). Evidence: `tu2/`
(`tu2/report.txt` is the full machine report; `tu2/store/` every row, side file and options dump; `tu2/BINARIES.txt`).
Pin `fd1e318b5`; harness hven-bench `tu2` `5858082` (every row's `# harness.sha`); host fedora, Linux only (Apple and Windows
UNOBSERVED). Counters only; wall-clock was never read except the exclusive-tier threshold of section 7.

## 1. Verdict

**The ranked winner is the uniform configuration `second_order_correction = false` with `trust_region_growth = kAnyAccepted`
for both Hessian modes (arm A4).** No mode split wins, so no `SqpTrustRegionGrowth::kAuto` is needed. Its V3 census read
survived (0 losses, section 5). **IPM: B1 (`barrier_initial = 1e-2`) passes.** Both are inputs to default events; each still
needs its own full census, 27-cell replay and gates (section 5.5 of the declaration). Best uniform configuration = the winner.

## 2. Arms against A0 (the PIN default), family and mode-row signs (wins, losses, ties)

| arm | SQP-HS | SQP-CUTEst | hs-x | hs-q | cutest-x | cutest-q | V1 |
|---|---|---|---|---|---|---|---|
| A1 SOC off | 0 (0,0,44) | +1 (3,1,42) | 0 | 0 | +1 (2,1,20) | +1 (1,0,22) | pass |
| A2 kRatioOrFeasibilityStep | +1 (4,2,38) | +1 (5,3,38) | 0 (2,2) | +1 (2,0) | 0 (3,3) | +1 (2,0) | pass |
| A3 kAnyAccepted | +1 (4,2,38) | +1 (5,2,39) | 0 (2,2) | +1 (2,0) | +1 (4,2) | +1 (1,0) | pass |
| A4 SOC off + kAnyAccepted | +1 (5,2,37) | +1 (6,2,38) | +1 (3,2) | +1 (2,0) | +1 (5,2) | +1 (1,0) | pass |
| A5 SOC off + kRatioOrFeasibilityStep | +1 (5,2,37) | +1 (6,3,37) | +1 (3,2) | +1 (2,0) | +1 (5,3) | +1 (1,0) | pass |

A4 and A5 ran: A1 had no family or mode-row sign -1 against A0 (condition holds). V1 held everywhere (no arm lost a solve on
any family or mode row). No arm had a -1 on any family.

## 3. V2: certificates

**There is no certificate on any of the 630 rows** (no `infeasible` row exists; `certificates.csv` was never created). So no
false certificate under any configuration or under A0, and no LP check was needed for a row. Uncertified `infeasible` rows:
none. IPM rows carry none.

## 4. Ranked passing configurations (section 5.4 item 3)

17 non-default configurations, all buildable (A4 and A5 ran). 9 pass (conditions a to d). In order (families won, net wins,
log gain, settings changed, label):

1. `s=0/tx=kAnyAccepted/tq=kAnyAccepted` (SOC off, uniform kAnyAccepted): won 2, net +7, gain 2.9962, 2 settings changed
2. `s=0/tx=kAnyAccepted/tq=kRatioOrFeasibilityStep`: won 2, net +7, gain 2.9656, 3 changed
3. `s=0/tx=kRatioOrFeasibilityStep/tq=kAnyAccepted`: 2, +6, 2.5329, 3
4. `s=0/tx=kRatioOrFeasibilityStep/tq=kRatioOrFeasibilityStep`: 2, +6, 2.5024, 2
5. `s=1/tx=kAnyAccepted/tq=kRatioOrFeasibilityStep`: 2, +6, 2.1000, 2
6. `s=0/tx=kAnyAccepted/tq=kRatio`: 2, +5, 2.5747, 3
7. `s=1/tx=kAnyAccepted/tq=kAnyAccepted`: 2, +5, 2.1925, 1
8. `s=0/tx=kRatioOrFeasibilityStep/tq=kRatio`: 2, +4, 2.1115, 3
9. `s=1/tx=kRatioOrFeasibilityStep/tq=kRatioOrFeasibilityStep`: 2, +4, 1.5891, 1

(`tu2/report.txt` lists all 17 with their signs and conditions; the 8 that do not pass either have a family sign of 0 or lack a
+1.) The winner (1) is uniform, so the split-vs-uniform question does not arise; the best split is rank 2, a tie with the winner
on won/net decided by gain.

## 5. V3: census

* **C0** (full 57 cells, corpus binary clean-built at `fd1e318b5`, `scripts/run_walk_census.sh` lane copy, parallel-tiered v1,
  T1 4-wide / T2 solo within the lane / T3 4-wide, one box lane, MKL 1 thread, pinned physical cores; counters asserted):
  42 Optimal, 7 dnf_budget, 6 dnf_setup, 2 WorkLimit. 10 cells differ from the M7-close CSV (listed in
  `tu2/census/C0/serial_confirm_list.txt`; the M7-close CSV is no longer the reference).
* **C(k) for the rank-1 configuration** (throwaway branch `tu2-ck1`, `21fa8d8ad`, never merged; patch `tu2/ck1-flip.patch`:
  `second_order_correction` false, `trust_region_growth` kAnyAccepted): the restricted read is the 42 cells Optimal at C0 and
  not wall-dependent. **0 losses, 42/42 Optimal.** Protocol: the lane copy of the census script with ONE change (the roster check
  accepts a baseline that is a subset of the binary's list; `tu2/scripts/run_walk_census_sub.sh`), declared in the run header.
  Rank 1 survives, so it moves.
* **Caveat, stated plainly:** the ck1 census is identical to C0 on every compared column of all 42 cells (compare_rc 0, 0
  mismatches), although the two binaries carry different stamps. The walk corpus drives the walk QP engine, not the SQP
  globalization loop, so it is insensitive to these two defaults. The veto read is therefore *passed with no sensitivity*, not
  evidence that the change is safe for the walk. Census mode coverage (section 10.5): the corpus runs 57 cells under
  `hessian_mode = kExact` and 0 under `kLbfgsDamped`.

## 6. IPM

B1 (`barrier_initial = 0.01`) vs B0: pooled signs hs +1, cutest +1 (`tu2/report.txt`),
V1 held (IPM-CUTEst reaches 17 -> 18, IPM-HS 22 -> 22). **B1 passes and moves** subject to its own default event. IPM outcome
counts: B0 CUTEst optimal 14 / acceptable 3 / max_iter 4 / dnf_wall 2; B1 optimal 10 / acceptable 8 / max_iter 3 / dnf_wall 2 (more
acceptable-tier stops; IPM keeps TU-1's reading, acceptable reaches).

## 7. Section 9: DALLASM and the LEWISPOL calibration

Calibration first, through the lane at the pin (a calibration lane build that adds the `linear.dense_factorization` override,
since the RF-1 `never-default` label is not in this lane; source patch is the 8-line override in `.scratch/cal-wt`, not
committed): LEWISPOL (small-dense TRAIN, confirmed in `sets/cutest-small-dense.json`) D and K reproduce r4's points
(f 3.000376849 / 26 factorizations and f 3.000138288 / 35) and the tool reads **1.30% at 1e-3 and 97.4% at 1e-1** (D: 0.0130027,
0.973984; K: 0.0130023, 0.973993): the declared calibration holds, no reconciliation needed.

DALLASM cells 028 and 029 reproduce RF-1 r4 / RF-1b (`numerical_error`, 9 majors, 4742 factorizations, `kkt_primal_eq` 9.23e-6),
and **both read NOT STATIONARY**: v0 = 1.869e-5 and the linearized violation is removed entirely within an inf-norm step of
1e-4 (chi/v0 = 1.0 at every delta). F-2 therefore did not withdraw a true stationary claim; the withdrawn DALLASM certificates
were false. (The two cells are the same point: the `kRatioOrFeasibilityStep` addition changes nothing here.) Dumps: `tu2/points/`.

## 8. Section 10 reported quantities

* **Fold-wise re-read:** selected per fold F1 `s=1/tx=kRatio/tq=kRatioOrFeasibilityStep`, F2 `s=0/kAnyAccepted/kRatioOrFeasibilityStep`,
  F3 `s=0/kAnyAccepted/kAnyAccepted`; pooled held-out signs cutest 0 (2 wins, 2 losses, 42 ties), hs +1 (4,2,38). No pooled -1,
  so nothing is put to the settler on this count. The generalization evidence is mixed on CUTEst (the three folds select three
  different configurations), consistent with TU-1's "differences inside the tie band".
* **Movers, TU-1 default (`0ee37c2ca`) vs A0 (the PIN):** 13 of 90 SQP cells differ in class, iterations or factorizations
  (list in `report.txt`). Two are lost solves at the pin: **cutest BRITGAS/default and NET3/default, `optimal` at TU-1 ->
  `stalled` (1 iteration) at A0**. Informational; no decision reads it, but it is a regression between the two pins worth the
  settler's eye. Walls that were `dnf_wall` at TU-1 (ACOPR118, SAWPATH, YORKNET on `sqp-qn`) are `stalled` now.
* **Outcome counts per arm and family:** `report.txt` section 1. A0: SQP-HS 44/44 optimal; SQP-CUTEst optimal 21, acceptable 1,
  max_iter 4, numerical_error 3, stalled 7, dnf_wall 10 (of 46). No `work_limit` row anywhere; highest reached factorization count
  41,279 (STEENBRC/default), against B/2 = 125,000: the budget margin held and no stop fired.
* **GL-0 report:** `tu2/gl0-report-all-sqp.txt` (477 cells with counters; no traces were taken, so no switching-ratio
  quantiles).
* **Census mode coverage:** 57 `kExact`, 0 `kLbfgsDamped`.

## 9. Run record

* Legs: A0 and B0 first, tier assignment (135 cells, 13 exclusive), A1, A2, A3, B1, A4, A5, C0, C(k=1), calibration, DALLASM.
  Elimination disabled (r2). Three co-run `dnf_wall` cells were re-measured exclusive (superseded files in the store): ACOPR118
  on A4, A5, A2 `sqp-qn`.
* One restart: the first A0 attempt stopped on a dirty-tree refusal (an untracked report script I created); relaunched, cached
  rows kept; no row was affected, all rows carry harness `5858082`.
* The first C(k) attempt used the strict roster check and exited 3 (cell set differs from the restricted baseline); the
  relaxed copy above was run instead. No cell ran under the strict copy.
* Stops (declaration section 11): none fired.
* Nothing touched HOLDOUT. No SNOPT, IPOPT, PIQP or HSL source was read or used.

## Addendum (2026-11-24): settler ruling, independent review, and the BRITGAS/NET3 cause

This addendum is appended after the results above; the earlier text is unchanged. Where they conflict, the addendum governs.

### V3

V3 was vacuous by construction and never passed. The sentence in section 1 that reads "Its V3 census read survived (0 losses,
section 5)" is superseded by this addendum, and so is the conclusion in the same paragraph that A4 and B1 move to default events on
that basis. A read that cannot fail does not support a ship decision.

### Ruling: defaults unchanged

- A4 (the ranked winner: SOC off plus uniform `kAnyAccepted`) is not shipped. A4 and SOC-off fold into a later growth-rule round,
  measured on the repaired pin with extended scoring: majors, rejected_steps, reject_funnel_ceiling, eval_f, eval_jac and
  evals_values alongside factorizations; quasi-Newton coverage; an in-repo radius-cycling fixture; and an owner replay.
- A1 is not shipped.
- B1 is deferred until the IPM ship rule has a strict-optimal column and an objective comparison.

### Independent review findings

- V1 held only because BRITGAS and NET3 were broken at the pin for every arm; with every arm failing the same cells, no arm could
  lose on them.
- ACOPR118, default mode, A0 to A4: majors 22 to 192, rejected steps 6 to 98 (88 of them at the funnel ceiling), eval_f 29 to 193.
  The scoring recorded this as a single 23% factorization loss.
- The gain is concentrated. ACOPP118 is 56% of the log gain, and the fold-wise held-out CUTEst sign is 0.
- The quasi-Newton evidence is about nil: the cutest-q +1 is READING6 alone.
- A1 ranks 16 of 16, and TRAIN barely exercises SOC.
- B1's reach of 17 to 18 hides strict-optimal falling from 14 to 10 (STEENBRB, STEENBRD, STEENBRE, STEENBRG, and YORKNET moving from
  optimal to acceptable). A strict reading gives CUTEst sign -1.
- Integrity notes, both disclosed and of low severity: an undeclared calibration K build, and a relaxed roster check (see the
  closing notes above for the C(k) attempt).

### The BRITGAS/NET3 cause

At the pin fd1e318b5, the SQP restoration stall window (added by the RF-1 merge 641dedeab) was observed once per restoration pass
with cumulative factorizations. A single pass that cost more than the 5000-factorization window fired it, and a regularized
restart inherited the failed run's samples.

The fix landed on m8 at 67effac74 (RF-1c): a five-pass floor plus a fresh window per restart. See
`docs/notes/data/2026-11-m8-rf1/RESULTS-RF-1c.md`, leg U and the TU-2 cell table, with evidence under
`docs/notes/data/2026-11-m8-rf1/gate-RF-1c/tu2/`.

On the 48 TU-2 cells (BRITGAS, NET3, SAWPATH, YORKNET x default/sqp-qn x A0-A5), solved went from 12 to 26 with 0 lost and no false
certificate; `stalled` went from 21 to 0. BRITGAS default is optimal under A0 and A1 and numerical_error under A2 to A5. NET3
default is optimal under all six arms. The arm ranking at fd1e318b5 is therefore not valid on the repaired pin, which supports
holding the defaults.

### Default-event instrument list for any future default flip

1. Re-pin after the fix, plus the 90 TRAIN cells, with V1 re-read.
2. TRAIN 238 x 2 exact and quasi-Newton counter legs.
3. nlp-35.
4. An SQP-sensitive corpus gated by a sensitivity precheck: the flipped build must move at least one row, otherwise the leg is
   recorded as vacuous.
5. RF-1 pilot cells, for certificates.
6. The radius-cycling owner replay plus the in-repo fixture.
7. The IPM corpus plus TRAIN IPM strict-optimal and objective columns.

### DALLASM 028/029

NOT STATIONARY stays as recorded above and is registered separately.
