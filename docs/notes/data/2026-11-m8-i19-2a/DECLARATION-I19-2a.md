# DECLARATION-I19-2a -- trust-region growth and shrink levers for the r -> 2r -> r cycle (issue #19)

The harness fields are filled from the hven-bench `i19` branch.

Task: M8 I19-2a. This file is the pre-registration. It is committed, together with every leg script it names (`legs/`),
the frozen cell lists (`cells/`) and the harness pin, BEFORE any I19-2a row is produced, and is never edited
afterwards. A correction is a successor `DECLARATION-I19-2a-r2.md` (then -r3, ...) naming what it changes, committed
before any row it governs; a failed or void leg chain keeps this file frozen and is answered by a successor with
re-derived bars. Results go to separate files (`RESULTS-I19-2a.md`). Criteria are never changed after data.

Governing documents, in order of precedence: the owner and settler rulings of 2026-11-24 (O1-O3, S-D3, S-D4, R1-R7
and the adopted items, plus the harness amendments of the same day: the sixth counter `step_limit_binding_majors`, the
G7 key, the grid CLI contract, the options-dump requirements, K/K-i on SENS-P, the TRAIN-238 run_serial terms, the N2
exclusions and the arm override: harness open questions H1-H11, accepted as written); the phase-1 hand-back
rulings P1-P4; the accepted independent design review (2026-11-24, READY AFTER CHANGES, all seven must-changes
accepted); the independent pre-freeze review (2026-11-24, FREEZE AFTER FIXES) and the settler's rulings F1-F7 on it,
with its should-fix items and code nits; the I19-2 design of 2026-11-24. Precedents: DECLARATION-TU-2 (+r2) and its RESULTS
addendum; DECLARATION-RF-1c (+r2..r4). Settler ledger entry: C524.

Scope (O3): I19-2a only. Elastic persistence (2b) and quasi-Newton / radius warm-start carry (2c) move to M9 with
the first-order work and are not built. The sequential re-solve fixture is not part of 2a.

## 1. The defect and the change

Issue #19 (first-derivative SQP trajectory study, hven fd1e318b5): 67-82 overlapping accepted/rejected/accepted
triplets per solve with trust radius r -> 2r -> r; 429 rejected steps, all at the funnel ceiling (KLV Eq. 8,
`src/sqp/globalization/funnel.cpp`); 114 of 118 acceptances in one run came through SOC.

Mechanism (triage 2026-11-23, spot-checked): `trust_region_grows` grows on the objective-only ratio
actual_df >= 0.75 pred_df for every accepted major whose step the merged flag (radius OR caller-cap pin) marks
region-limited. That includes h-type steps that INCREASE violation (issue iteration 5: ratio 5.06 while h went
0.210 -> 0.605), SOC-rescued steps judged against the ORIGINAL QP's pred_df, and steps held only by caller step-cap
pins. A doubled radius roughly quadruples the second-order violation and lands above the funnel width; the trial
(and its SOC) is rejected and the radius halved, not shrunk to the step.

Change: code head `bae35b20480aa2be43e9438dbd9b9e2b4eea4e2e` on R. Commits:
* `a50c5bbd5` test: the four radius fixtures with their default readings pinned at R (tree = R + tests only);
* `f865d7202` feat: levers G1-G7, the six counters (trust_region_growths ... step_limit_binding_majors), the `trust_region_growth_refusal_reasons` row field;
* `72d35452f` feat: the SENS-P grid runner `hven_sqp_i19_grid` and its 96-cell file;
* `a8380f729` test: four tests strengthened after the mutation dry run (section 8);
* `a032e37fa` feat: the `sensfix` family in the grid (the four fixtures, both Hessian modes, 8 cells; 104 in all) and its test;
* `555763d6a` fix: the pre-freeze counter nits (counter-only; section 3) and three tests;
* `91787d329` refactor: the API-lock names (settler ruling, 2026-11-24): counters `trust_region_growths`,
  `trust_region_growth_refusals`, `trust_region_shrinks_to_step`, `trust_region_grown_step_rejections`,
  `trust_region_unbound_rejections`, `step_limit_binding_majors`; the row field `trust_region_growth_refusal_reasons`
  typed by the public `SqpTrustRegionGrowthRefusal`; options `trust_region_growth_on_step_limit`,
  `trust_region_growth_ratio_max`, `trust_region_growth_funnel_guard` and the enumerator `kRatioByStepType`. Names
  only; this file uses the new names throughout (the harness lane keys, cell labels and CSV columns follow);
* `bae35b204` test: the golden SqpCounters line, one key split across a string literal, follows the renamed counter.
Seven opt-in research-tier levers, all default-off (section 3). The default path is unchanged in behaviour
(section 6, legs N1/N2 and leg C's default pins).

## 2. Pin and arms

* **R** = origin/m8 `afb65b804`, code-equal to `67effac74` (`git diff 67effac74 afb65b804 -- src include tests bench`
  is empty; afb65b804 carries the TU-2 addendum).
* **H** = `bae35b20480aa2be43e9438dbd9b9e2b4eea4e2e` = R + the I19-2a commits. `git diff bae35b204 <this file's commit> -- src include tests bench` is empty. The R and H installs, lane binaries and grid binary are built at `91787d329`, whose `src`, `include` and `bench` trees equal the head's (the head adds one test-file commit).
* Every arm is an options label evaluated on the ONE H binary (the levers are runtime options).
* **H0** = H at defaults. H0 is the scoring baseline. R is used only for the neutrality bar (leg N2).
* Harness: hven-bench branch `i19` (from `tu2` `c3d176e`; pushed): commit `i19-base` = `b026453cfde30326d492a0214ccafb7f2d6ce5d6`
  (the `caller_step_limit` key, CAP labels, arm build script, cell lists; builds both the R and the H lanes) and `i19-keys` =
  `1f9f757c2e9701648d639be22d4100612f70b337` (the seven lever keys; H lane only). Rule, runner, audit fixes and the API-lock
  key names: `i19-rule` `03a5209`, `i19-run` `2a9cc67`, `i19-audit` `2b6267b04f5321346dfc0c055bc5284b8876aebe`, `i19-names`
  `0cc0ded23b6d0fa1ce5c5a999cb96984142f4147`; the harness HEAD for every leg is `56fa03fdf40acb38275f0a46058ddbd2da925f0e`
  (R and H0 are run under this ONE harness commit; separately H0 under `i19-base` == H0 under `i19-keys` is the N2 second
  comparison, whose keys-side uses the renamed lane). The runner's arm override
  (`HVEN_BENCH_ARM_SHA` + `HVEN_BENCH_ARM_PREFIX`, both or neither; every trap still applies) selects the R or H
  install for an arm.

Base labels: `default` (kExact) and `sqp-qn` (`hessian_mode = kLbfgsDamped`). An arm label is `<base>+<keys>`.

| arm | keys appended to the base label | what it is |
|---|---|---|
| H0 | (none) | defaults: kRatio, SOC on, every lever off |
| G1 | `advanced.trust_region_growth=kRatioByStepType` | an h-type step grows on the violation ratio |
| G2 | `advanced.trust_region_growth_after_soc=false` | no growth after an acceptance through SOC |
| G3 | `advanced.trust_region_rejection_memory=3` | no growth reaching the smallest grown-and-rejected radius until 3 acceptances pass |
| G4 | `advanced.trust_region_growth_ratio_max=3` | no growth when the granting ratio exceeds 3 |
| G5 | `advanced.trust_region_shrink_to_step=true` | a rejected step the radius did not hold shrinks to min(0.5 Delta_eff, 0.5 \|\|p\|\|inf) |
| G6 | `advanced.trust_region_growth_on_step_limit=false` | a step held only by caller-cap pins is not growth-eligible |
| G7 | `advanced.trust_region_growth_funnel_guard=true` | no growth when h_lin + 4 (h_trial - h_lin)+ > 0.99 width |
| C1 | `advanced.second_order_correction=false+advanced.trust_region_growth=kAnyAccepted` | TU-2 A4 (comparator) |
| C2 | `advanced.second_order_correction=false` | TU-2 A1, SOC off (comparator) |
| P | `trust_region_initial=1.000000000000001` | placebo (R2): the default radius perturbed in the last digits |
| K | the union of the keys of every G-arm that ENTERS the composite (5.2) | the composite |
| K-i | K without G-arm i's keys | leave-one-out, stage 1b, informational; NEVER a candidate (R3) |

Arm option labels (literal; the `sqp-qn` label replaces the leading `default` by `sqp-qn`; `caller_step_limit` is appended by the CAP corpora and sorted into the key order, keys ascending):

| arm | TRAIN-238 / NLP-46 | CAP-238-1 | CAP-238-01 |
|---|---|---|---|
| H0 | `default` | `default+caller_step_limit=1` | `default+caller_step_limit=0.1` |
| G1 | `default+advanced.trust_region_growth=kRatioByStepType` | `default+advanced.trust_region_growth=kRatioByStepType+caller_step_limit=1` | `default+advanced.trust_region_growth=kRatioByStepType+caller_step_limit=0.1` |
| G2 | `default+advanced.trust_region_growth_after_soc=false` | `default+advanced.trust_region_growth_after_soc=false+caller_step_limit=1` | `default+advanced.trust_region_growth_after_soc=false+caller_step_limit=0.1` |
| G3 | `default+advanced.trust_region_rejection_memory=3` | `default+advanced.trust_region_rejection_memory=3+caller_step_limit=1` | `default+advanced.trust_region_rejection_memory=3+caller_step_limit=0.1` |
| G4 | `default+advanced.trust_region_growth_ratio_max=3` | `default+advanced.trust_region_growth_ratio_max=3+caller_step_limit=1` | `default+advanced.trust_region_growth_ratio_max=3+caller_step_limit=0.1` |
| G5 | `default+advanced.trust_region_shrink_to_step=true` | `default+advanced.trust_region_shrink_to_step=true+caller_step_limit=1` | `default+advanced.trust_region_shrink_to_step=true+caller_step_limit=0.1` |
| G6 | `default+advanced.trust_region_growth_on_step_limit=false` | `default+advanced.trust_region_growth_on_step_limit=false+caller_step_limit=1` | `default+advanced.trust_region_growth_on_step_limit=false+caller_step_limit=0.1` |
| G7 | `default+advanced.trust_region_growth_funnel_guard=true` | `default+advanced.trust_region_growth_funnel_guard=true+caller_step_limit=1` | `default+advanced.trust_region_growth_funnel_guard=true+caller_step_limit=0.1` |
| C1 | `default+advanced.second_order_correction=false+advanced.trust_region_growth=kAnyAccepted` | `default+advanced.second_order_correction=false+advanced.trust_region_growth=kAnyAccepted+caller_step_limit=1` | `default+advanced.second_order_correction=false+advanced.trust_region_growth=kAnyAccepted+caller_step_limit=0.1` |
| C2 | `default+advanced.second_order_correction=false` | `default+advanced.second_order_correction=false+caller_step_limit=1` | `default+advanced.second_order_correction=false+caller_step_limit=0.1` |
| P | `default+trust_region_initial=1.000000000000001` | `default+caller_step_limit=1+trust_region_initial=1.000000000000001` | `default+caller_step_limit=0.1+trust_region_initial=1.000000000000001` |

The options dump prints `trust_region_initial` at shortest round-trip precision: `1.000000000000001` dumps as
`trust_region_initial = 1.000000000000001`, distinct from `= 1` (pinned by
`OptionsDump.SqpDumpsEveryI19LeverAndThePlaceboRadiusDistinctly`). Every lever key appears in the SQP options dump.

Values not swept: eta2 = 0.75 (CGT BTR, the existing `kTrGrowThreshold`); memory N = 3; ratio ceiling 3; headroom
fraction 0.99 (KLV's beta); shrink factor 0.5. The ceiling 3 is a declared choice below the issue's offending ratio
5.06; the "too successful iteration" idea it implements is Walmag and Delhez, "A note on trust-region radius
update", SIAM J. Optim. 16(2):548-562 (2005) (bibliographic details verified; their numerical thresholds are not
transcribed). No 2-4 range is attributed to Nocedal-Wright. A different value is a successor round.

Interaction structure. G1 and G2 both act on SOC-rescued h-type steps; G3, G4 and G7 are generic cycle breakers that
also act on f-type steps (G7 is the one stateless lever aimed at the diagnosed mechanism, R6); G5 and G6 act on
shrink/cap geometry. One-at-a-time arms, one data-composed composite K and its leave-one-outs identify each lever's
main effect and its marginal contribution inside K. Two-way interactions outside K are not identified; that is
stated, not hidden.

Pre-flight before any cell: each arm's options dump differs from its base label's in exactly the arm's keys, and
`validate()` accepts it.

## 3. The levers (all `SqpAdvancedOptions`, research tier) -- as landed at `bae35b204`

| key | type | default | semantics |
|---|---|---|---|
| `trust_region_growth` | enum, new enumerator `kRatioByStepType` (appended, value 3) | kRatio | f-type step: kRatio's test. h-type step (verdict kAcceptFeasibilityStep): grows iff pred_dh = h_old - h_lin exceeds the noise floor 100 eps max(1, h_old) and h_old - h_new >= 0.75 pred_dh. h_lin is the l1 violation of the MODEL's linearization at the original step (cE + Je p, cI + Ji p from the iterate's evaluation, engine units; computed only when G1 or G7 is on; available on every major, elastic ones included, and null only for a non-finite step). h_new is the accepted point's (the SOC point after SOC); h_old is the iterate's. A null h_lin refuses |
| `trust_region_growth_after_soc` | bool | true | false: an acceptance through SOC never grows the radius |
| `trust_region_rejection_memory` | Index >= 0 | 0 (off) | N >= 1: a judged kReject of a trial whose radius was set by the previous major's growth records cap = min(cap, Delta at the rejection, before the shrink) and zeroes the clean count; a rejection at a radius no growth set records nothing and restarts nothing (R5). A growth with min(2 Delta, Delta_max) >= cap is refused. Each acceptance, AFTER its growth decision, increments the clean count; the N-th clears the cap. Cleared at a restoration resume and a watchdog rebase. A non-finite rollback is a rejection for the memory (at the radius the step was taken under, if a growth set it) and un-counts the growth it undid; the rollback snapshot carries the memory (R5) |
| `trust_region_growth_ratio_max` | double > 1, +inf allowed | +inf (off) | a growth GRANTED by a ratio is refused when that ratio exceeds the ceiling: the objective ratio under kRatio, kRatioOrFeasibilityStep's f-type branch and kRatioByStepType's f-type branch; the violation ratio on a kRatioByStepType h-type step. Grants made without a ratio (kAnyAccepted; kRatioOrFeasibilityStep's feasibility branch, even when the objective ratio also grants) are untouched |
| `trust_region_shrink_to_step` | bool | false | on a judged kReject of a step the radius did NOT hold (radius_binding false) that is not a failed subproblem's own step, the new radius is min(0.5 Delta_eff, 0.5 \|\|p\|\|inf) with Delta_eff = min(Delta, qp.trust_region_radius) and p the ORIGINAL trial's step (never a SOC re-solve's); at Delta = +inf it is 0.5 \|\|p\|\|inf. The floor test reads that value (shrunk < trust_region_min). A radius-bound step, a failed subproblem's step, the retry ladder's shrink rung and the non-finite rollback keep halving exactly (R4) |
| `trust_region_growth_on_step_limit` | bool | true | false: growth eligibility reads the radius's own pin (radius_binding) instead of the merged flag, so a step held only by caller step-limit pins does not grow; a mixed step (radius and cap pins both) stays eligible |
| `trust_region_growth_funnel_guard` | bool | false | true: a growth is refused when h_lin + 4 max(0, h_trial - h_lin) > 0.99 width (R6). h_trial is the UNCORRECTED trial's h (after SOC as before it), h_lin as for G1 (a null h_lin reads 0), width the funnel's width after this major's judgement. Inert without a funnel (a caller strategy) |

Refusal attribution (adopted). The decision is the pure `trust_region_growth_decision` (trust_region.h). Guards
(failure step, non-finite radius) and eligibility (the merged flag) come first; then the BASE RULE: the configured
rule's grant (kRatioByStepType reads kRatio's grant as its base, and the violation ratio as the h-type candidate's
grant). A major that neither the base rule nor the candidate grants consults no lever and carries no refusal. Otherwise
every enabled lever is read and every refusing lever's bit is recorded in the row field `trust_region_growth_refusal_reasons`
(the public enum class `SqpTrustRegionGrowthRefusal`: kViolationRatio = 1 (G1), kAfterSoc = 2 (G2), kRejectionMemory = 4
(G3), kRatioCeiling = 8 (G4), kStepLimitPins = 16 (G6), kFunnelHeadroom = 32 (G7); the row field is
`trust_region_growth_refusal_reasons`); the growth happens iff the candidate grants and no bit is set. The public
counter `trust_region_growth_refusals` counts accepted majors with a non-zero mask. A growth G1 grants that kRatio would not is
not a refusal.

Clean acceptance (nit adopted): every accepted major counts toward the memory's N, SOC-rescued acceptances and accepted
failure steps (a failed subproblem's own accepted step) included. The `radius_from_growth` flag (the radius was set by
the previous major's growth) is CONSUMED at the start of the next major: a retry-ladder rung that re-solves WITHOUT
shrinking the radius therefore carries a non-grown radius from then on, and a later rejection at that radius records
nothing in the memory and is not a grown-then-rejected rejection. Declared, not a lever effect. Counter details (code
nits fixed before freeze, lever-off path untouched): `trust_region_growth_refusals` = accepted majors at which a CANDIDATE growth
(one the base rule or the configured rule would have made) was refused by a lever, a stop-poll exit counting the
pushed row's refusal too; `trust_region_shrinks_to_step` counts a shrink only when it is taken (a rejection crossing the radius
floor takes none); `trust_region_grown_step_rejections` also counts a non-finite rollback of a step taken at a grown radius.
`kRatioByStepType` is an enum arm and cannot combine with `kAnyAccepted`; that is stated, and C1 is not combinable with G1.

Deviations accepted by the settler (P4): the refusal counter's meaning "a candidate growth was refused"; the G5 floor test
reads the shrink for any Delta; G7's inputs (the post-judgement funnel width; a missing h_lin read as 0; inert under a
caller strategy); SOC acceptances count as clean acceptances; growth eligibility reads the merged region_limited flag
(G6 reads the radius pin).

Restoration. The levers apply inside restoration sub-solves too, because `ropts` copies the advanced block. Lever
costs inside restoration show only in the folded work totals (majors as `restoration_iters`, factorizations); the
six the new counters counters describe the caller's own iterate and radius and are NOT folded (pinned by the scripted resume
test's identities). Each sub-driver has its own memory; the parent clears its own at the resume.

kRestoreMinRejections stays valid: G5 only shrinks faster, by construction at least halving (the min), which
strengthens argument (I) at `funnel.h`. RESTORATION TIMING CHANGES under G5 (R4): the restoration signature can fire
later or not at all, because the step changes after the first unbound rejection instead of re-solving the identical QP;
a tiny unbound rejected step reaches the radius floor in one rejection instead of many (an equivalent outcome).

SOC under G1 (stated, as the review asked): SOC only fires when the uncorrected step had h_new > h_old, so the
linear model already failed at this radius; G1 will often still grow after a good SOC, so G1 alone is not the SOC fix
(G2 or G7 is). G1 leaves f-type SOC acceptances on the original QP's objective ratio.

## 4. Corpora, run terms and families

All TRAIN; HOLDOUT is never run or read (the runner refuses it).

| corpus | cells per arm | terms | families |
|---|---|---|---|
| **TRAIN-238** | DN-3 roster's 119 hven-sqp problems (22 hs + 97 sd) x {default, sqp-qn} | run_serial's terms, stated explicitly as DIFFERING from RF-1c leg T: hs budget_facts 10,000; sd none; `--budget-iters 0`; 300 s cap; one pinned core per process | hs-x, hs-q, sd-x, sd-q |
| **CAP-238-1** | the same 238 with `+caller_step_limit=1` | as TRAIN-238 | cap1-x, cap1-q |
| **CAP-238-01** | the same 238 with `+caller_step_limit=0.1` | as TRAIN-238 | cap01-x, cap01-q |
| **NLP-46** | cutest-nlp-35's 23 TRAIN problems x {default, sqp-qn} | TU-2 terms: engine iteration cap, budget_facts 250,000, 300 s wall, per-(problem, label) tier from H0 | nlp-x, nlp-q |
| **SENS-P** | the frozen grid `cells/sens_p.tsv` (a copy of `bench/i19_sens_p.tsv` at 555763d6a; sha256 below): the PARAMETRIC grid (96 cells: F2CircleNlp; F3SpringChain n 20, 50; F4MovingConstraints; F6PathBoundQuadrature 21, 101 nodes; F7CollocationChain 10, 30 nodes; x 3 design points x {default, sqp-qn}; uncapped and at caller_step_limit 0.1) plus the **`sensfix` family** (P1; 8 cells: the four in-repo fixtures F-CYC, F-H, F-SOC, F-CAP of section 7 at their pinned parameters x {default, sqp-qn}): **104 cells**, run by `hven_sqp_i19_grid --cells cells/sens_p.tsv --set k=v ... --out <csv>` | default iterations, co-run (the 120 s cap is not enforced by the grid; no cell is near it) | sens-x, sens-q, senscap-x, senscap-q, sensfix-x, sensfix-q (V3' veto and the "no -1" / tail-guard reads) |
| **PILOT-62** | RF-1 pilot cells (`legs-rf1c/pilot_cells.tsv`) with the arm's keys appended (the arm's key wins a collision; collisions listed) | RF-1c leg P terms: 300 iterations, 900 s kill | certificates only (V2) |
| **FIX** | in-repo fixtures F-CYC, F-H, F-SOC, F-CAP (ctest; section 7) | ctest | mechanism (pinned in tests) |
| **OWNER** | the issue's four variants (free/fixed TA x cap 0.1/1.0), first planning update, owner's shim | owner's box and serial rule | owner gate V6 |

Cell files (frozen in `cells/` from the harness by `tu/i19_freeze_cells.py`; `cells/SHA256SUMS` is the pin):
* `cells/train_238.tsv` (238 cells) sha256 `3c3139773954e0650f4de1d04f9459812b79885f7ce48308e5ee284c4594cba0`; the CAP
  corpora are the same 238 cells with their label suffix;
* `cells/nlp_46.tsv` (46 cells) `516f54408f0fdafa44284a712b4efcda786d548ec40ec55067e21fc350cad378`;
* `cells/pilot_cells.tsv` (62 cells; a byte copy of RF-1c's pilot list, `legs-rf1c/pilot_cells.tsv`)
  `c53679267d87cef6872fbb7609283b04099c8201256f7444ce3fc020bc24af8e`;
* `cells/sens_p.tsv` (104 cells; a byte copy of `bench/i19_sens_p.tsv` at `bae35b204`)
  `52d0cf1dd4422e5ce404b7b1f9166ef55bca291e4c49821b7db68e1f03b14e18`;
* `cells/confounded.tsv` (33 cells: BRITGAS, DALLASM, YORKNET x {TRAIN-238, both CAP corpora, NLP-46, PILOT-62})
  `fe3333ccf346c39ae1bd7886cc11201cc2ba9ec035d74b7a95f9cbca716e4794`;
* `BINARIES.txt` (the R lane, the H lane and the grid binary, with sha256).
CAP-238 prechecks (adopted): before any candidate leg, the H0 count of cells with at least one cap-binding major
(`step_limit_binding_majors > 0`) is reported per CAP corpus and family; a family with zero such cells is VACUOUS for G6.

SENS-P H0 precheck (adopted; P1). Development reading at H on the PARAMETRIC part (this draft's author, not a round row;
2026-11-24, Release, one lane): 95 of 96 cells optimal; uncapped: 0 cells with a grown-then-rejected trial, rejections
only on the five F2 sqp-qn cells, growths on 6 F3 exact cells; capped: every cell grows (cap-only growths,
`step_limit_binding_majors` 1-58), only the one max_iter cell (senscap-q F3-n50-p0.4) rejects. So the parametric grid FAILS
the "cells show growths and rejections" precheck on 47 of 48 uncapped and 47 of 48 capped cells and is RECORDED
near-vacuous where its precheck fails; it stays as it is (P1). The `sensfix` family (development reading at H0, one
lane): all 8 cells optimal, 35-125 majors, 14-36 rejections, 17-70 growths, 5-17 grown-then-rejected, F-CAP 116-117
cap-binding majors. The precheck is mechanical in `i19_rule.py` (at least one SENS-P H0 cell with trust_region_growths > 0 and
rejected_steps > 0) and is reported PER FAMILY; vacuity of V3' is judged per family and overall (5.3).

Coverage check (pre-flight): TRAIN-238's hs rows plus NLP-46 must contain TU-2's 90 TRAIN cells. A missing cell is
added as a supplementary leg in the same terms before any candidate leg.

CONFOUNDED cells (coordinator addition, 2026-11-24, from a parallel diagnosis; F4, P3). Every cell of the BRITGAS,
DALLASM and YORKNET problems (any corpus, any label) is restoration-path sensitive: the growth levers act inside
restoration sub-solves through `ropts`, and these cells' outcomes there are dominated by walk-QP failures and an
unbounded restoration re-entry loop (a separate round, RST-B, follows I19-2a). Provenance: the RST diagnosis at
afb65b804 read RF-1 pilot 020-026/030 and TU-2 A0-A5 BRITGAS/default rows. They are NAMED, and:
(i) they are INCLUDED, identically to every other cell, in V1, S1(b), K entry, the S2 signs and geomeans, R2 and the
LOPO read, and the R1 tail guard VETOES on them like on any cell (mechanical and conservative; no non-mechanical
step);
(ii) in addition they are listed in a SEPARATE block of the guard report and of every mover report, with their
restoration-entry counts under both arms, including whether the placebo P trips R1 there.
V1 and V2 apply to them unchanged. The confounded cell ids per corpus are frozen in `cells/confounded.tsv`
(33 cells, sha256 above).

"Strict-optimal" = SQP status `optimal`. "Solved" in V1 is read on BOTH strict-optimal and optimal+acceptable (TU-2
R1: acceptable never counts as a solve in the ship comparison). A cell an arm REACHES (R1) is one that ends with a
solver status, not a harness error.

## 5. The ship rule (fixed here, before any row)

### 5.1 Cell rule and currencies

Cost currencies, read only on cells strict-optimal under both arms: **LA** = `factorizations`; **EV** = `eval_f +
eval_jac` (SolveResult::work). A candidate WINS a cell in a currency iff its count is at least 10% lower
(`10*lower <= 9*higher`, integers); LOSS symmetrically; else TIE. Family sign = sign(wins - losses) per currency.
Reported, never ranking inputs except as stated: majors, rejected_steps, reject_funnel_ceiling, eval_f, eval_jac,
evals_values, factorizations, soc_steps/soc_applied, the six counters (trust_region_growths ... step_limit_binding_majors), restoration entries,
trust_region_floor_exits, radius-floor QN resets (`qn_resets_radius_floor`), and the objective difference on common
solves (flagged when |f_c - f_b| > 1e-6 max(1, |f_b|)). Wall-clock is never read. "Moves a row" (V5, stage-1(e))
means a deterministic column OTHER than the new counters differs.

### 5.2 Stage 1 (TRAIN-238, CAP-238-1, CAP-238-01, SENS-P) and stage-2 selection

Stage 1 runs H0, P, G1-G7, C1 and C2. An arm PASSES stage 1 iff (a)-(e) below hold; the placebo condition (f) is NOT part
of the pass (A3): it applies at K ENTRY and at SHIP (5.3):
* (a) V1 holds on hs-x, hs-q, sd-x, sd-q, every CAP family AND every SENS-P family (kept, stricter: a lost SENS-P cell
  fails stage 1 through V1 as well as through V3'; strict-optimal and optimal+acceptable counts >= H0's); every lost
  cell is listed;
* (b) no currency has a family sign of -1 on those families;
* (c) V3' holds (the SENS-P veto of 5.3; a VACUOUS V3' is recorded here and blocks a ship, H3);
* (d) V4 (the tail guard, R1) holds;
* (e) it is non-vacuous on its live corpus: it moves at least one row of TRAIN-238 (for G6: of a CAP corpus);
* (f) [K entry and ship, not the stage-1 pass] R2 (F1, H2): on the qualifying currency, its pooled net wins EXCEED |P's net wins on that currency|; its solved
  gain counts toward K entry only if it exceeds |P's net strict-optimal change|. Pooling: the arm's stage-1 live
  cells, TRAIN-238 (for an arm whose only lever is G6, both CAP corpora); SENS-P is never pooled into a count. The
  placebo needs no solved gain for a SHIP (O1; `placebo_solved_required` is OFF).

**K** = the union of the G-arms that pass stage 1 AND show at least one +1 (a pooled strict-optimal gain exceeding |P's
net solved change|; LA pooled +1 with net wins above |P's net wins on LA|; EV likewise; for G6: a +1 on a CAP family). C1, C2 and P never enter K; K-i is never a candidate (R3). If
exactly one G-arm qualifies, K is that arm. If none qualifies, K does not exist.

Stage 1b: K and its leave-one-outs on TRAIN-238, both CAP corpora AND SENS-P (H10: K and K-i also run SENS-P, which
V3' needs). Informational.

Stage-2 arm set: K, if it passes the stage-1 conditions as an arm; the top-ranked passing stage-1 arm other than K
(G or C, ranked by 5.4 on the STAGE-1 corpora only: no NLP-46 row is read to pick the runner-up); C1, which always runs NLP-46 for the record (TU-2 ruling: A4 measured on the repaired pin;
C1 is a candidate only if it passed stage 1); P (stage 2 also carries the placebo, R2). If no arm passes stage 1,
stage 2 runs only H0, P and C1, and nothing ships.

NLP-46 is not read by any stage-1 or stage-1b decision. NLP-46 WAS ALREADY READ for C1 and C2 in TU-2, and V4's
threshold form was set from one of its cells (ACOPR118, A4 6 -> 88) -- stated (R3).

### 5.3 Ship conditions (stage-2 arms; all must hold)

Live corpora L(X): TRAIN-238 ∪ NLP-46 for every arm, except an arm whose only lever is G6, whose L is CAP-238-1 ∪
CAP-238-01 (S-D3: G6 may ship on the CAP corpora + fixture + owner replay alone; it is inert without a cap, N3 proves
it). Both CAP corpora are guard families for every arm.

* **S1 Solved (O1).** (a) V1 on every family of L(X) and on every CAP family: X's strict-optimal count >= H0's AND
  X's optimal+acceptable count >= H0's -- NO family loses a solve; (b) pooled over L(X), X's strict-optimal count >=
  H0's. A strict solve gain is NOT required.
* **S2 Cost (O1: clearly better).** On L(X): (a) at least one currency has a pooled sign of +1; (b) no currency has
  a pooled or family sign of -1; (c) over cells strict-optimal under both, the geometric mean of (X+1)/(H0+1) is <= 1
  for each of majors, factorizations, eval_f, eval_jac and evals_values.
* **S2-NLP (R3).** NLP-46 read STAND-ALONE: per currency, sign >= 0, and the geomean of S2(c) <= 1 on each column.
* **S2-LOPO (R3, H6, A2).** Applies to EVERY stage-2 arm over its own live corpora (an arm whose only lever is G6: the
  CAP corpora). "Survives" = the SAME currency that gave the pooled +1 keeps pooled sign +1, and the five S2(c)
  geomeans stay <= 1, after removing each problem by NAME from every corpus of L(X) in turn (both base labels, every
  corpus: TRAIN-238, NLP-46 or the CAP corpora, so a problem present in two corpora loses all its cells at once;
  the pooled counts then overlap those corpora, as the pooled counts of S2(a) do). Mechanical in `i19_rule.py`. The
  R2 declared minimum is NOT re-checked on the reduced sets (H6).
* **R2 effect minimum (F1, F2).** On ONE currency, the SAME currency for both conditions: pooled net wins >= 5 AND
  pooled geomean <= 0.97 (LA: factorizations; EV: eval_f + eval_jac, the geomean over common strict-optimal cells of
  (eval_f + eval_jac + 1) / (H0's + 1)); and, on that currency, net wins > |P's net wins|. Pooled over L(X) at stage
  2. The solved gain is not a ship condition (O1).
* **V2 Zero false certificates** on every row of every leg (TU-2 V2 as amended by TU-2-r2 section 3: a certificate at
  v0 <= 1e-6 is FALSE; otherwise the LP via `tu/stationarity.py` decides). Any false certificate under H0 STOPS.
* **V3' SENS-P veto (F3, H3, H4, A5).** The V3' veto and non-vacuity families are EXACTLY sens-x, sens-q, sensfix-x and
  sensfix-q (sensfix = the four fixtures x two Hessian modes). The capped variants senscap-x and senscap-q (cells of the
  same 104-cell grid, section 4) are GUARD families only: V1 and the "no -1 sign" read, never V3' veto or non-vacuity
  families. V3' = PASS iff no H0-optimal cell of a V3' family is non-optimal under X AND at least one V3' family has a
  moved row (a deterministic column other than the new counters differs from H0; residual-class floats compare by the
  N2 near-ulp gate, integers and statuses exactly); VACUOUS iff no V3' family moved (or the H0 precheck of section 4
  fails); the precheck and the moved rows are reported PER FAMILY. A
  VACUOUS V3' is recorded at stage 1 and BLOCKS a ship (H3). The "no -1 sign" read (H4, stricter) applies at stage 2
  as well as at stage 1, on every cap family (cap1, cap01) and every SENS-P family; the R1 tail guard is read on
  SENS-P. Declared: G4 at ceiling 3 alone is EXPECTED to be VACUOUS (it moves no fixture except one F-CYC QN growth,
  section 7), and the sensfix lever outcomes in section 7 were seen in development (shown for non-vacuity only, not
  evidence).
* **V4 / R1 tail guard.** On every cell both arms reach, of every leg (TRAIN-238, both CAP corpora, NLP-46, SENS-P):
  X must not have majors, eval_f + eval_jac, rejected_steps or reject_funnel_ceiling >= max(H0 + 20, 3 H0) (each
  column separately, per cell; a cell is REACHED by an arm iff it ends with a solver status, H5). Reported per arm:
  floor hits (cells with trust_region_floor_exits > 0, plus the sums, H7), restoration entries,
  trust_region_floor_exits, radius-floor QN resets. CONFOUNDED cells (section 4) are NOT exempt: the veto applies
  to them as to any cell (P3, F4), and they are additionally reported in a separate block (section 4).
* **V1-cells.** Every lost cell (strict or optimal+acceptable) is listed, re-run ALONE under the serial rule, and
  diagnosed before a ship.
* **V5 Coverage.** X moves at least one row in an exact family AND at least one in a QN family of L(X).
* **V6 Owner replay (O2).** REQUIRED for any default flip, not for stages 1/2: all four variants strict-optimal under
  X; sum majors(X) <= sum majors(H0); sum reject_funnel_ceiling(X) < sum(H0). Reported by the owner. NO FALL-THROUGH:
  if THE candidate (5.4) fails V6, NOTHING ships and a successor round follows; the next-ranked arm is not tried.
* **Neutrality held** (leg N2). A precondition of the round.

### 5.4 Ranking and the decision

Ranking keys, in order: (1) pooled strict-optimal net gain, descending; (2) currencies with pooled +1, descending;
(3) the sum over common solves of log(H0/X) for LA plus the same for EV, descending; (4) settings changed from
default, ascending (K counts each lever); (5) label. The top-ranked arm satisfying 5.3 is THE candidate. At most one
configuration ships. If none satisfies 5.3, nothing ships, and that is a result; the levers stay as documented opt-in
research-tier options either way. A mode-split default is not in this round's rule (TU-2 OQ-3 precedent).

### 5.5 The default event (if a candidate ships)

Its own declared event (`DECLARATION-I19-2a-D`): the flip commit; every moved pin declared and re-derived
(I19RadiusCycle default pins, route-reference readings, goldens); the flipped-default build reproduces the candidate
arm's TRAIN-238, CAP and NLP-46 co-run rows byte-for-byte on deterministic columns; ctest Release and Debug; the
default-flip mutation killed; V6; CI on four lanes.

## 6. Legs and bars

All legs run under `~/.local/bin/box-lane --`; only NLP-46's exclusive tier uses `--exclusive`. Evidence goes to
`gate-I19-2a/`. Binaries (R and H installs, lane binaries, the grid binary) are frozen in `BINARIES.txt` and committed
before the first leg; the runners sha-check them. Toolchain: clang, `-DHVEN_FP_MODE=SAFER_FAST`, sequential MKL, no LTO,
Linux x86-64; Apple and Windows readings are CI only, and an Apple value not observed is UNOBSERVED.

| leg | instrument | bar |
|---|---|---|
| B | builds: the R and H installs, the two lane binaries, the grid binary, BINARIES.txt | every build rc 0; shas recorded; a build failure is a stop |
| C | FULL ctest Release and Debug at H (every test, not a filter) | 100% pass both; counts reported (development readings at `bae35b204`, one lane: Release 4175 of 4175 pass, Debug 4175 of 4175 pass; the leg C reading at freeze is the evidence); every `I19GrowthRule.*`, `I19RadiusCycle.*`, `I19SensGrid.*` and TU-0 test green; leg C below 100% is a stop |
| N1 | corpus replay R vs H (walk/ssn/ipm; base x1, head x3; `compare_replay.py` byte-exact) | 0 differences (ANY N1 difference is a stop); recorded as INSENSITIVE to SQP globalization (TU-2 addendum): a standing gate, not the neutrality proof |
| **N2** | R vs H0 on TRAIN-238, both CAP corpora, PILOT-62 and NLP-46 co-run cells, R and H0 under the SAME harness commit (`i19-base`); separately H0 under `i19-base` == H0 under `i19-keys` | **0 differences** on every row column except `options_digest` and `wall_s`, and every side-file counter present in both (counters and statuses EXACT; residual-class columns by the calibrated near-ulp gate; every declared corpus must be present on BOTH sides with the declared and equal cell counts, PILOT-62 included; a side row present on one side only is a difference). H-only side columns must carry a declared new-counter name: the six `the new counters` counters, the side-column prefixes `trust_region_growths`, `trust_region_growth_refusals`, `trust_region_shrinks_to_step`, `trust_region_grown_step_rejections`, `trust_region_unbound_rejections`, `step_limit_binding_majors` (`spec.rule.new_counter_prefixes`), and their default-path values are reported. **SENS-P and sensfix are EXCLUDED from N2 (F6): the grid binary is H-only (it is built from the i19-2a tree) and has no R counterpart.** Their stand-ins: the default readings of the four fixtures pinned at R in `a50c5bbd5` and unchanged at the head (section 7), the ctest of leg C, and N1. A coverage table from the H0 rows counts rows exercising each touched site (growth, judged reject, floor test, SOC-applied, cap-binding, restoration sub-solve); unhit branches are named as resting on ctest only. A difference STOPS the round |
| N3 | G6 on TRAIN-238 against H0 | predicted identical (no cap); any moved row STOPS; recorded as the expected VACUOUS reading |
| M | mutations (section 8) on the Release build, control first (`legs/mutations_i19.sh`, `legs/mutate_i19.py`, anchors pre-checked) | control clean; every must-kill killed by at least one test (the declared killer is reported, not required); the argued set {M18, M21} is EXPECTED to survive and a kill of one is reported, not a failure: `VERDICT MUTATIONS_AS_DECLARED`. Leg M not as declared is a stop |
| S1 | H0, P, G1-G7, C1, C2 on TRAIN-238, both CAP corpora, SENS-P | stage-1 table per 5.2; every vacuous leg named |
| S1b | K, K-i on TRAIN-238, both CAP corpora, SENS-P | reported |
| S2-N | H0 (co-run then exclusive tier), P, then each stage-2 arm, on NLP-46 | 5.3 inputs |
| S2-P | PILOT-62 under H0 and each stage-2 arm | V2; solved counts and movers reported |
| S2-O | owner replay of H0 and each stage-2 arm (only before a default event) | V6 |
| Sm | install smoke and export contract, both front ends | rc 0; a failure is a stop |
| CI | `gh workflow run ci.yml --ref i19-2a`; `gh run watch <id> --interval 120` | four lanes green (a CI failure is a stop); off-reference readings recorded as observed |

Co-run protocol (CLAUDE.md section 7), declared in every results file's provenance block: one process per pinned
PHYSICAL core of one box lane, SMT siblings idle; `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`; counters, statuses and
objectives asserted; wall-clock informational, never quoted. A cell ending at a cap or kill, or a deviation that
decides a bar, is a candidate: it is re-run ALONE under the serial rule before it is called one. NLP-46 tiering is
TU-2 section 7, assigned per (problem, label) from H0 before any candidate leg.

Order: B -> C -> N1 -> N2 (both lanes) -> N3 -> M -> S1 -> S1b -> (stage-2 selection, mechanical, by `tu/i19_rule.py`)
-> S2-N -> S2-P -> Sm -> CI; S2-O only before a default event.

## 7. Fixtures and telemetry (committed before this file; readings measured by the author)

All four fixtures are `test_support::I19Sphere` (tests/sqp/support/i19_radius_fixtures.h): f = a.x on the circle
|x|^2 = rho^2 (n = 2), started at (rho, 0), minimiser (0, -rho). Readings below are the DEFAULT readings pinned at R
(commit `a50c5bbd5`, Release and Debug identical) and unchanged at H (exact under HVEN_TEST_ROUTE_REFERENCE; bounds
elsewhere: optimal exit, >= 5 grown-then-rejected, >= 3 r->2r->r triplets, plus each fixture's own signature).

| fixture | parameters | mode | majors | growths | grown-then-rejected | triplets | rejected (all funnel ceiling) | signature |
|---|---|---|---|---|---|---|---|---|
| F-CYC (pure f-type cycle; G3/G7) | rho 100, a = (0, 1e4), SOC off | exact | 44 | 22 | 17 | 17 | 18 | every grown-then-rejected step was an f-type acceptance |
| | | QN | 47 | 22 | 16 | 16 | 18 | |
| F-H (G1) | rho 30, a = (0, 1), SOC off | exact | 36 | 17 | 14 | 14 | 15 | 5 growths on h-type acceptances whose h rose |
| | | QN | 38 | 17 | 14 | 14 | 15 | 5 |
| F-SOC (G2) | rho 100, a = (0, 1), default options | exact | 36 | 18 | 12 | 12 | 14 | 12 SOC-rescued growths then rejected |
| | | QN | 38 | 19 | 13 | 13 | 15 | 13 |
| F-CAP (G5/G6) | rho 100, a = (0, 1), caller_step_limit 0.1 | exact | 126 | 70 | 5 | 5 | 36 | 64 cap-only growths, 36 unbound rejections, 30 identical re-solves |
| | | QN | 127 | 70 | 5 | 5 | 36 | 64, 36, 30 |

F-H and F-SOC were built (no fallback needed); the scripted-verdict driver tests via `advanced.make_strategy` exist in
addition, for the memory's wiring (R7).

Lever readings on the fixtures at H (development, the pins of `I19RadiusCycle.EveryLeverOnEveryFixtureReadsItsPin...`;
columns: majors / trust_region_growths / trust_region_growth_refusals / trust_region_shrinks_to_step / trust_region_grown_step_rejections / trust_region_unbound_rejections /
step_limit_binding_majors; exact | QN):

| fixture | H0 | G1 | G2 | G3 | G4 | G5 | G6 | G7 |
|---|---|---|---|---|---|---|---|---|
| F-CYC | 44/22/0/0/17/1/0 \| 47/22/0/0/16/1/0 | 44/21/1/0/17/1/0 \| 47/21/1/0/16/1/0 | = H0 | 32/9/13/0/5/0/0 \| 35/10/13/0/5/0/0 | = H0 \| 47/21/1/0/16/1/0 | 44/22/0/1/17/1/0 \| 46/22/0/1/16/1/0 | = H0 | 27/4/18/0/0/0/0 \| 29/4/18/0/0/0/0 |
| F-H | 36/17/0/0/14/1/0 \| 38/17/0/0/14/1/0 | 22/2/15/0/0/0/0 \| 24/2/15/0/0/0/0 | = H0 | 26/6/11/0/4/0/0 \| 28/6/11/0/4/0/0 | = H0 | 36/17/0/1/14/1/0 \| 39/17/0/1/14/1/0 | = H0 | 22/2/15/0/0/0/0 \| 24/2/15/0/0/0/0 |
| F-SOC | 36/18/0/0/12/0/0 \| 38/19/0/0/13/1/0 | 43/15/12/0/11/0/0 \| 44/15/12/0/11/0/0 | 32/3/25/0/0/0/0 \| 37/3/26/0/0/0/0 | 29/10/9/0/4/0/0 \| 29/10/9/0/4/0/0 | = H0 | = H0 \| 38/19/0/1/13/1/0 | = H0 | 40/2/34/0/0/0/0 \| 43/2/34/0/0/0/0 |
| F-CAP | 126/70/0/0/5/36/117 \| 127/70/0/0/5/36/116 | 124/53/31/0/3/34/115 \| 125/53/29/0/3/34/114 | 122/58/30/0/0/31/108 \| 135/55/43/0/0/30/99 | 123/67/6/0/2/33/112 \| 124/67/6/0/2/33/111 | = H0 | 105/76/0/4/10/4/84 \| 108/77/0/5/10/5/83 | 98/10/78/0/6/7/85 \| 101/11/76/0/6/8/84 | 118/29/59/0/0/27/104 \| 132/29/69/0/0/27/96 |

F-H-scaled (F-H with the row scaled by 1e3, row scaling on, started at (36, 0) so h_lin > 0 early; the units-mutation
fixture): H0 23/10/0/0/6/1/0 | 29/11/0/0/7/1/0; G1 16/3/8/0/0/0/0 | 18/3/8/0/0/0/0; G3 19/6/4/0/2/0/0 | 25/6/5/0/3/0/0;
G5 23/10/0/1/6/1/0 | 31/11/0/1/7/1/0; G7 17/3/7/0/0/0/0 | 22/3/8/0/0/0/0; G2, G4, G6 = H0.

Every cell ends optimal. The declared-value G4 (ceiling 3) moves no fixture except one F-CYC QN growth: no fixture grows
on a ratio above 3 often, so G4's driver reach is shown at ceiling 1.05 (`TheRatioCeilingReachesTheDriver`); G4 rests
on the unit tests and the corpora. G7 refuses on F-SOC with more majors (40/43 vs 36/38): its effect on F-SOC is a
crawl-type cost, exactly what R1's tail guard is for. Note also G1 on F-SOC costs majors (43/44 vs 36/38).

Telemetry. Six public counters (S-D4 + the harness amendment), appended to SqpCounters, caller's iterate, not folded
from restoration: `trust_region_growths`, `trust_region_growth_refusals`, `trust_region_shrinks_to_step`, `trust_region_grown_step_rejections`,
`trust_region_unbound_rejections`, `step_limit_binding_majors`. No SqpIterationInfo mirror. Row and trace field
`trust_region_growth_refusal_reasons` (integer bit set, after `t_s`, never null; trace-schema-v1 note and freeze row). Identities pinned on
the fixtures (depth-0 history): trust_region_growths = accepted rows whose next radius is larger (minus growths a non-finite
rollback undid, pinned separately); trust_region_grown_step_rejections = those followed by a rejected row; trust_region_unbound_rejections =
rejected rows with tr_binding false; step_limit_binding_majors = rows with cap_binding; trust_region_growth_refusals = rows with a
non-zero mask; all refusals 0 at defaults. Declared additive break: SqpAdvancedOptions 440 -> 464, SqpOptions 768 ->
792, SqpCounters 1552 -> 1600 bytes; `kAggregateArity<SqpIterationRow>` 36 -> 37; the options dump fixtures, the
options reference, the iter/solve.end goldens and `sqp_complete.jsonl` re-derived (keys appended only).

## 8. Mutations and must-kills (the table is part of this file)

Applied by `legs/mutate_i19.py <id>` on the Release tree (anchors pre-checked: `PREFLIGHT_DOES_NOT_APPLY` aborts), run
by `legs/mutations_i19.sh` on hven_sqp_tests `I19*:Tu0*:SqpSolverNonfiniteRollback.*:SqpSolverQpFailure.*:
IterationTelemetry.*:CallerStepLimit*`, hven_solver_tests `Options.*:OptionsDump*` and hven_bench_tests `I19*`. Killers
named from the development dry run (2026-11-24, tree 72d35452f + hand re-checks at a8380f729, and the review
additions of F5 (M32-M38) at 555763d6a (before the rename, which is names only); NOT evidence: control clean, 41 of 47 killed at 72d35452f, four of the six survivors killed by
tests added in a8380f729, two argued (P2)). The leg M reading is the evidence. BAR (F5): every must-kill is killed by at
least one test; the declared killer is reported informationally, a different killer is not a failure. The argued set
{M18, M21} is encoded in `legs/mutations_i19.sh` as EXPECTED TO SURVIVE: its survival is not a failure and a kill of
one is reported. Abbreviations: GR = I19GrowthRule, RC = I19RadiusCycle, NR = SqpSolverNonfiniteRollback,
QF = SqpSolverQpFailure.

| id | mutation | declared killer(s) |
|---|---|---|
| M1 | kRatioByStepType's h branch reads the objective ratio | GR.RatioCeilingReadsTheViolationRatioUnderStepTypeRatio, GR.StepTypeRatioGrowsAnHTypeStepThatDeliversItsPredictedViolationDecrease, GR.StepTypeRatioReadsTheViolationRatioOnAnHTypeStep (+3) |
| M2 | a null linearization grows | GR.StepTypeRatioRefusesWithoutALinearization |
| M3 | pred_dh <= 0 grows | GR.StepTypeRatioRefusesANonPositivePredictedViolationDecrease |
| M3b | the pred_dh noise floor dropped | GR.StepTypeRatioRefusesANonPositivePredictedViolationDecrease |
| M19 | G1's actual_dh from the original trial, not the SOC ctx | RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities |
| M20 | h_lin in caller units (the QP rows under the row scaling) | RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities (F-H-scaled cells; after a8380f729; at 72d35452f it survived: h_lin was 0 on every scaled row) |
| M4 | the SOC lever ignored | GR.NoGrowthAfterSocWhenTheLeverIsOff, GR.RefusalsAreReadAfterTheBaseRuleAndEveryRefusingLeverIsRecorded, RC.EachLeverReducesItsFixturesSignatureAtNoExtraMajors (+2) |
| M5 | memory never recorded at the reject site | RC.EachLeverReducesItsFixturesSignatureAtNoExtraMajors, RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities, RC.ScriptedMemoryIgnoresARejectionAtARadiusNoGrowthSet (+3) |
| M5b | the cap recorded after the shrink (Delta/2) | RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities, RC.ScriptedMemoryIgnoresARejectionAtARadiusNoGrowthSet |
| M5c | every rejection recorded, grown radius or not | RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities, RC.ScriptedMemoryIgnoresARejectionAtARadiusNoGrowthSet |
| M6 | memory never clears | GR.ARejectionRestartsTheCleanCount, GR.MemoryClearsAfterNCleanAcceptancesAndTheNthCannotGrow, RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities (+2) |
| M7 | a recorded rejection does not restart the clean count | GR.ARejectionRestartsTheCleanCount, RC.EachLeverReducesItsFixturesSignatureAtNoExtraMajors, RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities (+3) |
| M7b | clean count incremented before the growth decision | RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities, RC.ScriptedMemoryIgnoresARejectionAtARadiusNoGrowthSet, RC.ScriptedMemoryRefusesTheRejectedRadiusUntilNAcceptancesPass |
| M28 | `2 Delta >= cap` becomes `>` | GR.MemoryClearsAfterNCleanAcceptancesAndTheNthCannotGrow, GR.MemoryRefusesAGrowthReachingTheRememberedRadius, GR.RefusalsAreReadAfterTheBaseRuleAndEveryRefusingLeverIsRecorded (+3) |
| M17 | memory not cleared at a restoration resume | RC.ScriptedMemoryIsClearedAtARestorationResume (after a8380f729; at 72d35452f it survived: the window of 3 cleared the cap before the radius regrew) |
| M18 | memory not cleared at a watchdog rebase | SURVIVED, ARGUED: the memory can hold a cap at a watchdog rebase only if, inside the warm full-step mode (the only state a rebase ends), a trial at a grown radius was rejected as non-finite (the one kReject that mode issues; no rollback is taken in it). No fixture builds that, and the `radius_from_growth` flag a rebase also clears is its sibling (also unreachable in that mode: it is consumed at the start of the major). Accepted as argued (P2); not must-kill |
| M29 | a non-finite rollback not recorded by the memory | NR.TheMemoryRecordsARolledBackRadiusAGrowthSet |
| M8 | the ratio ceiling ignored | GR.RatioCeilingLeavesTheFeasibilityBranchOfRatioOrFeasibilityStepAlone, GR.RatioCeilingReadsTheViolationRatioUnderStepTypeRatio, GR.RatioCeilingRefusesARatioAboveItAndNotAtIt (+3) |
| M9 | the ceiling applied to a ratio-free grant (kAnyAccepted) | GR.RatioCeilingLeavesARatioFreeGrantAlone |
| M9b | the ceiling applied where kRatioOrFeasibilityStep's feasibility branch grants | GR.RatioCeilingLeavesTheFeasibilityBranchOfRatioOrFeasibilityStepAlone |
| M9c | the ceiling reads the objective ratio on a kRatioByStepType h-type step (not the violation ratio) | GR.RatioCeilingReadsTheViolationRatioUnderStepTypeRatio |
| M10 | G5 ignores radius_binding | GR.ShrinkToStepKeepsHalvingARadiusBoundStep, RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities, RC.ShrinkToStepSetsTheRadiusFromTheRejectedStepAlone |
| M10b | G5 does not exclude failed-subproblem steps | GR.ShrinkToStepLeavesAFailedSubproblemsStepAlone |
| M10c | G5 without the min (0.5 \|\|p\|\| alone) | GR.ShrinkToStepHalvesTheStepWhenTheRadiusDidNotHoldIt |
| M10d | G5 reads Delta, not Delta_eff | GR.ShrinkToStepHalvesTheStepWhenTheRadiusDidNotHoldIt |
| M11 | G5's floor test reads 0.5 Delta | GR.ShrinkToStepFloorTestReadsTheShrunkRadius |
| M21 | G5 leaks to the retry ladder's shrink rung | SURVIVED, ARGUED: not equivalent in general; unreached by any fixture; the rung never reads the lever. On every available fixture a routed failure's step is the window-bound walk iterate (norm = Delta), so a leaked half-step equals the halving; QF.TheShrinkRungHalvesTheRadiusUnderShrinkToStep pins the rung. Accepted as argued (P2); not must-kill |
| M22 | G5 leaks to the non-finite rollback's shrink | NR.TheRollbackHalvesTheRadiusEvenUnderShrinkToStep |
| M23 | G5 reads the SOC re-solve's step norm | RC.ShrinkToStepSetsTheRadiusFromTheRejectedStepAlone (the F-SOC QN row; after a8380f729; at 72d35452f it survived: F-CAP SOC steps are cap-pinned to the same norm) |
| M12 | G6 reads the merged flag | GR.CapOnlyStepIsNotEligibleWhenCapPinsDoNotGrow, GR.RefusalsAreReadAfterTheBaseRuleAndEveryRefusingLeverIsRecorded, RC.EachLeverReducesItsFixturesSignatureAtNoExtraMajors (+3) |
| M12b | G6 refuses a mixed step (radius and cap pins) | RC.AMixedStepStaysGrowthEligibleWithoutCapPinGrowth (after a8380f729; at 72d35452f it survived: no fixture had a mixed step) |
| M25 | G7 ignored | GR.FunnelHeadroomReadsTheUncorrectedTrialAfterSoc, GR.FunnelHeadroomRefusesAGrowthWhosePredictedViolationLeavesTheWidth, GR.RefusalsAreReadAfterTheBaseRuleAndEveryRefusingLeverIsRecorded (+3) |
| M26 | G7 reads the SOC point's h, not the uncorrected trial's | RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities |
| M27 | G7 never receives the width | RC.EachLeverReducesItsFixturesSignatureAtNoExtraMajors, RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities, RC.TheRefusalBitsNameTheRefusingLever |
| M31 | levers consulted before the base rule (attribution) | GR.RefusalsAreReadAfterTheBaseRuleAndEveryRefusingLeverIsRecorded, RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities |
| M15 | the grow-then-reject flag never cleared | RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities, RC.ScriptedMemoryIgnoresARejectionAtARadiusNoGrowthSet |
| M16 | trust_region_growths counted before the refusals | RC.EveryLeverOnEveryFixtureReadsItsPinAndKeepsTheIdentities, RC.OnElasticMajorsTheRadiusAndTheCapStaySeparated, NR.AGrowthTheRollbackUndidIsNotCounted |
| M30 | a rolled-back growth stays counted | NR.AGrowthTheRollbackUndidIsNotCounted |
| M24 | restoration's tr counters folded into the parent | RC.ScriptedMemoryIsClearedAtARestorationResume |
| M32 | G7: a missing h_lin refuses (declared: it reads 0) | SURVIVED, ARGUED (F5, P4): h_lin is null only for a non-finite step, which is rolled back and never grows; no fixture or unit case reaches a null h_lin with G7 on. Not must-kill |
| M33 | G7: the max(0, .) on h_trial - h_lin dropped | SURVIVED, ARGUED (F5): the term differs only when the trial's violation is BELOW the linearization's prediction (a nonlinear improvement); the four fixtures and the unit cases all have h_trial >= h_lin. Not must-kill |
| M34 | the memory keeps the LAST rejected radius, not the smallest | GR.ARejectionRestartsTheCleanCount, GR.MemoryRecordsOnlyARadiusThatCameFromAGrowth (killed in the dry run at 555763d6a) |
| M35 | a non-finite rollback keeps the memory state the undone commit advanced (the snapshot restore dropped) | SURVIVED, ARGUED (F5): the restore differs only by the one clean-count increment of the acceptance the rollback undoes, and the rollback's own recorded rejection zeroes the count whenever the rolled-back radius came from a growth; the one case where it is visible (a rollback of a non-grown radius) is unreached by any fixture. Not must-kill |
| M36 | the shrink counted before the floor test (a floor crossing counted as a shrink) | RC.ARejectionCrossingTheRadiusFloorIsNotAShrinkToTheStep |
| M37 | the stop-poll exit does not count the pushed row's refusal | RC.AStopPollExitCountsThePushedRowsRefusal |
| M38 | a rolled-back grown step is not a grown-then-rejected one | NR.AGrowthTheRollbackUndidIsNotCounted |
| D1-D7 | default flips: kRatioByStepType, after_soc false, memory 3, ceiling 3, shrink true, cap_pins false, headroom true | each killed by the options dump test and 11-23 tests (GR.DefaultLeversEqualTodaysRuleOnAGrid, the default pins, RC.RemedyLeversAtDefaultLeaveTheFixturesUnmoved, ...): D1 23, D2 17, D3 20, D4 11, D5 17, D6 16, D7 22 killers |

## 9. Stops

The round stops on any of: a pre-flight mismatch; any N1, N2 or N3 difference; an error row not reported by the
solver; an H0 cell that throws; a false certificate under H0; leg B (a build) failing; leg C (full ctest) below 100%;
leg M not as declared (control not clean, or a must-kill surviving); a failed Sm or CI leg; any attempt to read
HOLDOUT; a spec/declaration disagreement; a binary whose sha differs from BINARIES.txt; a TU-2 coverage-check miss.
(An earlier draft's "reached row above B/2 factorizations" rule is dropped: B is not defined in this round.) A stop is resolved by a
successor declaration, never by an edit.

## 10. Not run

HOLDOUT (never). Wall-clock. The QP walk census (insensitive by construction; TU-2 addendum). The hven-ipm lane (the
change is `detail/sqp` only). Elastic persistence and warm-start carry (M9, O3). Accelerate and Windows (CI only).

## 11. Compute budget

About 17-19 lane-hours (about 9 box-hours), +-30% (the design's C.8, plus the second CAP corpus, P in stage 1 and 2,
and K/K-i on SENS-P); elapsed 1.5-2.5 days, dominated by exclusive queueing. SENS-P is ~6 s per arm on one core. A smoke of the whole chain (H0, P and G3 on 4 hs cells and 4 sensfix cells, then the stage-1 report; NOT evidence) measured the hs cells at ~4 ms each, so the estimate stands: the budget is the sd, CAP and NLP-46 legs, as designed.
