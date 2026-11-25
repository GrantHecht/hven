# DECLARATION-TU-2: the manual tuning round that picks the shipped defaults

Task: M8 TU-2. This file is the pre-registration. It is committed on hven branch `tu2` (cut from m8 `fd1e318b5`) as
`docs/notes/data/2026-11-m8-tu/DECLARATION-TU-2.md` BEFORE any TU-2 row is read and is never edited afterwards. A
correction is a successor `DECLARATION-TU-2-r2.md` (then `-r3`, ...) that names what it changes and is committed before any
row it governs. Results go to separate files.

The governing pre-registration is DECLARATION-TU-1 (hven `tu1` 9edda3432). TU-2 applies TU-1 section 4 except where section
5 below names a successor reading. Every successor reading only tightens section 4; none relaxes it. The pilot verdict
(TU-1 RESULTS section 1: MANUAL SWEEPS) fixes the method: TU-2 runs no configurator.

The harness is hven-bench branch `tu2`, cut from `tu1` 8388c8b. Its machine copy of sections 2, 4 and 6 is
`tu/tu2_spec.json`; `tests/test_tu.py` holds the two together. Where they disagree, this file is the authority and the
disagreement stops the run. Settler rulings: ledger entry C486 (section 14).

## 1. Pin (re-baseline)

PIN: `fd1e318b5` (hven m8: after RF-1, F-1, F-2, RF-1b, and 4446b814e, which stops a numerically singular K0 throwing out
of the SQP solve). Release install by hven-bench `scripts/build_hven_pin.sh` (clang++, `HVEN_LINK_TIME_OPT=OFF`,
`-DHVEN_MKL_THREADING=sequential`, C449).

The pin event is one declared hven-bench commit, made before any row. It moves `cmake/hven_pin.cmake`, `HVEN_PIN.txt` and
the set pins' `hven_pin_sha` line. The lane binary's sha256 is recorded in the first evidence commit. The runner refuses a
prefix whose `HVEN_PIN.txt` disagrees.

Every arm is re-measured at this PIN, the default included. No TU-2 decision reads a TU-1 row. TU-1 rows appear only in
the informational movers table (section 10).

## 2. Cells, families and mode rows

The TRAIN problems, base labels and folds are exactly TU-1 sections 2 and 3:

* HS 22 and CUTEst 23 problems.
* SQP labels: `default` (exact Hessian) and `sqp-qn` (`hessian_mode = kLbfgsDamped`). Both run `common.max_iterations =
  300` and `column_scaling = kAuto`.
* IPM label: `default`.

Families (TU-1 section 2): SQP-HS (44 cells), SQP-CUTEst (46), IPM-HS (22), IPM-CUTEst (23).

**Mode rows** (new; used by section 5.4). Each SQP family splits by base label:

| mode row | label | families |
|---|---|---|
| exact Hessian | `default` | SQP-HS-x (22 cells), SQP-CUTEst-x (23) |
| quasi-Newton | `sqp-qn` | SQP-HS-q (22), SQP-CUTEst-q (23) |

This is TU-1 section 2's "live under" principle applied to `hessian_mode`: a setting live under one Hessian mode only is
judged on that mode's cells. No family is re-cut. The TU-3 clusters (TU-1 section 6) are untouched.

## 3. HOLDOUT

TU-1 provides no HOLDOUT confirmation leg. Its section 2 says HOLDOUT is never run or read by anything it governs. Its
section 4.6 routes adoption through each default event's census, 27-cell replay and gates.

TU-2 adds none. HOLDOUT is never run or read here; `run_serial` and `tu/rule.py` refuse non-TRAIN rows. The
generalization evidence is the fold-wise re-read of section 10.1, computed from TU-2's own TRAIN rows.

## 4. Arms

### 4.1 SQP (default walk mode; families SQP-HS, SQP-CUTEst and their mode rows)

| arm | settings | runs |
|---|---|---|
| A0 | `{}` (the PIN's defaults: `second_order_correction = true`, `trust_region_growth = kRatio`) | always (the re-baseline) |
| A1 | `advanced.second_order_correction=false` | always |
| A2 | `advanced.trust_region_growth=kRatioOrFeasibilityStep` | always |
| A3 | `advanced.trust_region_growth=kAnyAccepted` | always |
| A4 | A1 + A3 | iff A1 has no family or mode-row sign -1 against A0 |
| A5 | A1 + A2 | same condition as A4 |

Rows are deterministic per binary, label and cell in the columns read. The six arms therefore cover every
(`second_order_correction`, `trust_region_growth`) pair on every cell. That lets each of the 17 non-default configurations
C(s, t_x, t_q) be built from measured rows, with no extra run:

* s is the second-order-correction setting, uniform across both labels;
* t_x is the trust-region growth on `default` cells;
* t_q is the trust-region growth on `sqp-qn` cells.

Pre-flight before any cell (TU-1 section 5.2): each arm's options dump differs from its base label's dump in exactly the
arm's keys, and `validate()` accepts it.

### 4.2 IPM (families IPM-HS, IPM-CUTEst)

B0 `{}` (`barrier_initial = 1e-3`) and B1 `barrier_initial=1e-2`. No other value is swept. This is the NLP IPM's own
barrier parameter, not an IPQP barrier default, so TU-1 section 4.6's M7-42 precondition does not apply.

### 4.3 Not swept, each with its disposition

| item | disposition |
|---|---|
| `advanced.elastic_rung_engine = kTier` (M7-1) | stays non-default; not swept |
| `advanced.adaptive_dual_regularization_max` 1e-10 and 1e-6 (M7-15) | excluded: worse on every family in TU-1 |
| `qp.working_set_algebra = kRefactorize` (A7 static arm) | not a default candidate (2.34x factorizations, geomean). Its use as a fallback rung (retry a failed walk QP once under kRefactorize) is registered as an RF design item. No TU-2 leg measures it. |
| M7-7 `advanced.restoration_elastic_seed` | carried to the M8 register; not swept in TU-2 |
| M7-11 `advanced.elastic_rho0_headroom_rungs` | carried to the M8 register; not swept in TU-2 |
| SCL-R2 `row_scaling_power_of_two` | carried to the M8 register; not swept in TU-2 |
| W3-R2 `advanced.ssn_farkas_residual_tol` (with `ssn_infeasibility_rule = kFarkasGated`) | carried to the M8 register; not swept in TU-2 |
| W3-R3 `advanced.ssn_uncertain_tol` | carried to the M8 register; not swept in TU-2 |
| W3-R4 `advanced.ssn_lm_sigma_c` and `advanced.ssn_lm_sigma_exponent` (under a residual sigma rule) | carried to the M8 register; not swept in TU-2 |
| A2-F2, A3-R5, W3-R1 (conditional) | preconditions unmet (T-a/E1-c; A3-R2; F-c); not swept |
| candidates retired in TU-1 section 8 | stay retired |

## 5. The ship rule (fixed here, before any row)

### 5.1 The cell rule (TU-1 section 4.1), with successor reading R1

A candidate WINS a cell if:

* it reaches and the baseline does not; or
* both reach and its primary counter is at least 10% lower (`10 * lower <= 9 * higher`, in integers).

LOSS is defined symmetrically. Anything else is a TIE.

Primary counter: SQP `factorizations`, IPM `iterations` (TU-1 section 4.2). Wall-clock is never the metric.

**R1 (successor reading of "reaches", SQP rows only).** An SQP row reaches iff its `status` column is `optimal`. An SQP
`acceptable` row does not reach, and its counter is never read.

Reason: since RF-1-C (5ba2828ea, after TU-1's pin), SQP `kAcceptable` relabels a failure exit at a loose point
(stationarity 1e-2, feasibility 1e-3). The lane buckets it with `optimal`, so keeping that bucketing would score a quicker
failure as a cost win.

IPM rows keep TU-1's bucketing: the IPM acceptable tier is a consecutive-iterate stop, a real termination. Documented
alternate optima (hs025, hs033) reach, as in TU-1.

### 5.2 Family sign and move (TU-1 sections 4.3 and 4.4, as written)

Family sign = sign(wins - losses). A configuration passes the family rule iff it is +1 on at least one family of its
engine and -1 on none. A family sign of 0 is neither a win nor a loss. A configuration with no +1 family does not move,
whatever its ties.

### 5.3 Vetoes (each one alone stops a move)

**V1. Solved-count guard** (successor; a tightening). On every family, the configuration's count of reaching cells is at
least the baseline's. For a mode-specific setting, the same must hold on every family of each mode row it touches.

Reason: the 10% rule weighs one lost solve the same as one 10% counter win elsewhere. On TU-1's rows it passed
`kAnyAccepted`, which gained one solve and lost two.

**V2. Zero false certificates.**

* A certificate is an SQP row with outcome `infeasible` and side column `sqp_infeasibility_certified = 1`.
* Every certificate on any TU-2 row gets the section 9 LP stationarity check. A certificate the check finds NOT
  STATIONARY is FALSE.
* Any false certificate under a configuration vetoes that configuration.
* Any false certificate under A0 stops the round: it is a hven defect, not a tuning matter.
* Certificates found STATIONARY are reported. On a TRAIN problem with a known feasible point they are local-infeasibility
  points, not false by contract.
* Uncertified `infeasible` rows do not reach and are listed.
* IPM rows carry no certificates; their `infeasible` rows are listed.

**V3. Census veto** (TU-1 section 4.5, re-baselined). It applies to every configuration that changes the exact-Hessian
walk.

1. The census baseline is census C0: the full 57-cell walk census taken at the PIN's defaults (section 8). The M7-close
   CSV is no longer the reference; the RF-1 r3 census already disagrees with it on 7 cells.
2. A LOSS is a cell that is `Optimal` at C0 and not `Optimal` under the candidate.
3. Deadline-dependent cells are outside the veto, as in TU-1 section 4.5: a cell with `dnf_budget` or `dnf_setup` on
   either side, and `f7_n10000_path_physics`.
4. The candidate census C(k) reads only the cells that can produce a LOSS: those `Optimal` at C0 and not wall-dependent.
   It runs on a throwaway build of the PIN with configuration k's defaults flipped.
5. The default event's full census (TU-1 section 4.6) stays binding.
6. The corpus runs no kLbfgsDamped cell, so a setting live under quasi-Newton only cannot move the census. Its census read
   is recorded as "vacuous by construction", never as "passed".
7. IPM configurations run no census (TU-1 section 4.5).

### 5.4 Selection, mode-specific defaults and ties

1. **SQP candidates.** The configurations C(s, t_x, t_q) of section 4.1. Those that need A4 or A5 rows are candidates only
   if those arms ran.
2. **Pass conditions.** A configuration passes iff all of these hold:
   * (a) section 5.2 on the pooled families SQP-HS and SQP-CUTEst;
   * (b) no mode-row family sign of -1, with a uniform setting judged on both modes' cells and a split setting on each
     mode's cells separately;
   * (c) V1 on the pooled families and on every mode-row family;
   * (d) V2.
3. **Ranking of passing configurations** (TU-1 section 5.2 item 2, plus one tie-break), in order:
   1. families won, descending;
   2. net wins over both families, descending;
   3. the sum, over cells both reach, of log(baseline counter / candidate counter), descending;
   4. (new) the number of settings changed from the PIN default, ascending; a split with t_x != t_q counts the
      trust-region setting twice;
   5. label, ascending.
4. **Census.** V3 is read on the top-ranked passing configuration. If it is vetoed, the next-ranked one has its census
   read, and so on. The first one that survives moves. If none passes, nothing moves, and that is a result.
5. **A mode-split winner** (t_x != t_q) ships through a new enumerator `SqpTrustRegionGrowth::kAuto` as the shipped
   default. `kAuto` resolves to t_x under `kExact` and to t_q under `kLbfgsDamped`.
   * It lands in its own declared default event, with mutation testing and a must-kill test for the resolution route.
   * The results report the best uniform configuration (t_x = t_q) under the same rule, alongside the split winner.
   * TU-2 itself adds no hven code.
   * The same procedure applies to `second_order_correction` if it should ever split by mode; in this round it is uniform
     (s in section 4.1).
6. **IPM.** B1 is judged on IPM-HS and IPM-CUTEst by section 5.2 and V1, and moves iff it passes.
7. **Ties.** Cell ties are TU-1's. Configurations tied on every ranking key down to label are separated by label only. A
   configuration whose every family sign is 0 never moves.

### 5.5 What the rule does not replace

TU-1 section 4.6 stands: each adopted default is a declared default event with its own census, 27-cell replay and gates.

## 6. budget_facts

### Values

| set | `budget_facts` |
|---|---|
| HS | **10,000** |
| CUTEst | **250,000** |

The value is applied as `common.max_factorizations` on every hven lane cell, SQP and IPM, and written to the rows'
`budget_facts` column. A stop is `status = work_limit`, `outcome = dnf_budget` (the lane already maps kWorkLimit there).

Both set pins change in one declared set-pin event, made before any row. The event sets `run_terms.budget_facts` from
null to these values, updates its note, and BUMPS each pin's `set_version`: hs-27 v1 -> v2 and cutest-nlp-35 v1 -> v2.
No problem's assignment, digest or split changes.

### Why these values

Maxima over every TU-1 row (all configurations):

| quantity | value |
|---|---|
| highest factorizations on any reached SQP TRAIN row | 105,065 (NET3, `default` label) |
| next highest reached | BRITGAS 85,682; STEENBRG 82,563; STEENBRD 75,094 |
| highest on any non-wall row | 232,882 (STEENBRD/sqp-qn, at the 300-iteration cap) |
| highest on a reached HS row | 184 |
| highest on any IPM row | 1,687 (ACOPR118, `dnf_budget` at the iteration cap) |

* 250,000 is 2.4 times the highest reached count and above every TU-1 row, so it would have cut no TU-1 solve.
  10,000 is about 54 times HS's highest.
* The budget can change a rule verdict only by denying a rescue that needs more than B factorizations. Where the baseline
  reaches with b, a candidate stopped at B >= 10b/9 reads LOSS either way. TU-1's largest rescue used 18,576
  factorizations (YORKNET/sqp-qn).
* It provides a finite ceiling, so factorization-currency summaries and penalty factors are defined
  (`schema/columns.py` `PENALTY_BUDGET_COLUMN`).
* It gives a deterministic, scheduling-invariant stop for cheap-factorization runaways. Without it they end on the wall,
  which is not reproducible.
* It is not a box-hour lever: the cost of one factorization differs by about four orders of magnitude across problems
  (READING8/sqp-qn did 890 in 300 s; STEENBRD/sqp-qn about 9,000 per second).

### Neutrality, checked on the rows

`common.max_factorizations` is read only at iteration boundaries and hands its remainder to restoration sub-solves
(`src/sqp/sqp_solver.cpp` at 6680 and 8516; `src/ipm/ipm_solver.cpp` at 172). It is therefore path-neutral until it
fires.

* Every `work_limit` row is listed in the results.
* If any reached TU-2 row exceeds B/2 factorizations, the run STOPS and reports to the settler before section 5 is read:
  the margin has eroded.

## 7. Measurement protocol (CLAUDE.md section 7)

### Co-run tier (counter-only legs), as TU-1 section 9

* Command: `box-lane -- run_serial.py --corun-width 4`.
* One process per pinned PHYSICAL core, SMT siblings idle.
* `MKL_NUM_THREADS=1` and `OMP_NUM_THREADS=1` in every process, and MKL's sequential layer by build.
* The rule reads only deterministic columns: outcome, status, `factorizations`, `iterations`, `sqp_infeasibility_certified`.
* The protocol is declared in every results file's provenance block (`run.mode: co-run`, `wall.quotable: false`).
* Wall-clock from co-run is informational, never quoted, never compared with a serial timing.

### Exclusive tier (wall-dependent cells)

Command: `box-lane --exclusive -- run_serial.py`; each cell runs serial, at full budget.

* **A0 and B0.** The TU-1 solo problems run exclusive on every label: ACOPR118 BIGBANK BRITGAS MSS2 MSS3 READING8 SAWPATH
  TAX213322 TAX53322 YORKNET.
* **A1 to A5 and B1.** The tier is assigned per (problem, label) from A0 and B0, and fixed before any candidate leg:
  * a cell where the baseline ends `dnf_wall`, or reaches with exclusive `wall_s` above 100 s, runs exclusive;
  * every other cell runs co-run;
  * a co-run cell whose row ends `dnf_wall` is re-measured exclusive; its co-run row is named in `superseded.txt` and
    never read.

  Contention can push a cell only toward a budget or deadline outcome, never toward a false success or a false counter.

### Other terms

* **Ceilings.** Walls: HS 600 s, CUTEst 300 s (the set pins). A capped cell is `dnf_wall` and does not reach. Iteration
  caps are the labels'. Factorization budgets are section 6's.
* **Deviations.** A deviation seen under co-run is a candidate: the cell is re-run alone and the deviation reproduced
  before it is used.
* **Legs.** One `box-lane` acquisition per leg, where a leg is one (arm, label, tier) batch. No multi-hour hold.
  Exclusive legs are announced to co-tenants first. Exclusive `wall_s` is measured inside the hold; queueing time is never
  solve time (TU-1 RESULTS section 10).
* **Order and elimination.** Within an arm, co-run legs run before exclusive legs. An arm whose family rule can no longer
  pass, even if it won every cell not yet measured, skips its remaining legs. This is exact; the bound is TU-1 section 5.3's.
* **Stamp.** Every results file carries harness sha, pin sha, compiler, host, CPU, date and run mode.
  Apple/Accelerate and Windows: UNOBSERVED (TU-2 is Linux only).

## 8. Census legs

* **C0:** the full 57-cell census (`scripts/run_walk_census.sh`, parallel-tiered v1) on a corpus binary built at the PIN.
  It is the TU-2 census baseline.
* **C(k):** the restricted census of section 5.3 V3, on a corpus binary built from the PIN with configuration k's defaults
  flipped. The build is a throwaway branch, never merged.

Census legs assert counters only, under the census script's own tiering, pinning and serial-confirm rules, and each
declares its protocol in its provenance header.

## 9. Registered measurement: stationarity at DALLASM pilot cells 028 and 029

**Cells.** RF-1 pilot cells 028 and 029:

* 028: DALLASM, `sqp-qn` + `adaptive_dual_regularization_max=1e-6`;
* 029: the same + `trust_region_growth=kRatioOrFeasibilityStep`.

F-2 withdrew their certificates. They now end `numerical_error` (h_inf 9.23e-6, restoration h_out 1.87e-5).

**Step 1. Reproduce.** Re-run both cells at the PIN with a point dump. Confirm they reproduce the RF-1 r4 / RF-1b reading
(status, majors, factorizations). A mismatch is reported, and the check then measures the point the PIN returns.

**Step 2. The LP** (the RF-1 r4 YORKNET/LEWISPOL method). At the returned x*, in caller units, over ALL rows (retained and
dropped), inside the variable box:

* v0 is the l1 constraint violation at x*.
* v(Δ) = min over d of the linearized l1 violation sum_i viol_i(c(x*) + J(x*) d), subject to l <= x* + d <= u and
  ||d||_inf <= Δ.
* The decrease is χ(Δ) = v0 - v(Δ).
* Report v0, χ(Δ) and χ(Δ)/v0 for Δ in {1e-4, 1e-3, 1e-2, 1e-1, 1}.

**Classification.**

| reading | condition |
|---|---|
| NOT STATIONARY | χ(1e-3)/v0 >= 1% |
| STATIONARY | χ(1)/v0 < 1e-6 |
| APPROXIMATELY STATIONARY (referred to the settler) | anything else |

The two RF-1 precedents read NOT STATIONARY under this rule: YORKNET 054 at 15.5% and LEWISPOL at 1.3%, both at Δ = 1e-3.
The STATIONARY test uses the largest Δ because the linearized model is convex: at a stationary point the decrease is zero
at every Δ, and the largest Δ is the most sensitive test.

**Calibration** (before any DALLASM number is read). The tool is run on LEWISPOL D and K at the PIN, at the same point as
at r4 per RF-1 r4, through the RF-1 TRAIN runner, after confirming that roster's split places LEWISPOL in TRAIN.

* It must reproduce about 1.3% at Δ = 1e-3 and about 97.4% at Δ = 1e-1.
* If it does not, the tool's definition is reconciled with r4's before any DALLASM reading.

**Tooling.** The LP tool and its selftest (one synthetic stationary point and one synthetic non-stationary point) are
committed in hven-bench, Apache-2.0. The LP is solved by HiGHS through SciPy, both permissively licensed. The point dump
is a lane flag. No hven code changes.

**Scope.** This is a measurement only. No TU-2 default depends on it. It answers whether F-2 withdrew a true stationary
claim.

## 10. Reported quantities (informational; no decision reads them)

1. **Fold-wise re-read.** For each TU-1 fold k, section 5.4 is applied on TRAIN(k), and the selected configuration is
   compared with A0 on HELD(k). The three comparisons are pooled into family signs. No extra runs are needed. If any
   pooled sign is -1, that is put to the settler before the default event.
2. **Movers.** TU-1's default (0ee37c2ca) against A0 (the PIN), per cell: outcome, status and counters.
3. **Outcome counts** per arm and family: optimal, acceptable, work_limit, max_iter, error, infeasible, and certified.
4. **GL-0 report.** `tu/gl0_report.py` over all TU-2 SQP rows (TU-1 section 7 quantities).
5. **Census mode coverage:** the number of census cells under each `hessian_mode`.

## 11. Stops

The round stops on any of:

* a pre-flight mismatch;
* an error row not reported by the solver (status null and not listed in `solver_exceptions.csv`);
* any A0 or B0 cell that throws;
* a false certificate under A0 (V2);
* a reached row above B/2 factorizations (section 6);
* any attempt to read HOLDOUT;
* a disagreement between the spec and this declaration.

A stop is resolved by a successor declaration, never by an edit.

## 12. Harness work (hven-bench `tu2`) and testing obligations

* **H1.** The pin event (section 1).
* **H2.** `budget_facts`:
  * the set-pin event of section 6 (`set_version` bumped);
  * the pin's value reaches `common.max_factorizations` (the options dump shows it);
  * a null value leaves it unset;
  * kWorkLimit maps to outcome `dnf_budget` with status `work_limit`.
* **H3.** `tu/rule.py`: R1, V1, V2, the mode rows, the building of configurations from arm rows, the ranking and the
  elimination bound. `--selftest` covers each on synthetic rows.
* **H4.** The per-(problem, label) tier assignment from A0 and B0 (section 7), written to the store before the first
  candidate leg.
* **H5.** The lane flag `--dump-point` (x, c(x), sparse J(x) and bounds, in caller units) and the LP tool with its
  selftest (section 9).

All of this is harness code with unit tests. TU-2 adds no hven code route, so mutation testing and must-kill tests do not
apply to TU-2 itself. They apply to `SqpTrustRegionGrowth::kAuto` in its own default event, if a mode-split winner is
adopted (section 5.4 item 5).

## 13. Compute budget

Unit: lane-hours. The box has two 4-core lanes, so one exclusive hour costs 2 lane-hours.

Basis, from TU-1's statistics and the RF-1 r3 census log:

* Per full SQP arm: about 1.18 h exclusive, of which about 3,900 s is 13 cells capped at 300 s, plus 500 to 1,100
  cell-seconds of co-run at width 4.
* Per IPM arm: about 883 s exclusive.
* A full census: 6.1 lane-hours.

Expect ±30%, because the PIN moves statuses.

| item | lane-hours |
|---|---|
| SQP A0 re-baseline | 2.4 |
| SQP A1 to A3 (per-cell tiers) | 6.3 |
| SQP A4 and A5 (conditional) | 4.2, or 0 if A1 fails |
| IPM B0 and B1 | 0.8 |
| Census C0 (full) | 6.1 |
| Census C(k), one or two candidates (restricted) | 0.6 to 1.2 |
| Builds (install, lane, one or two corpus builds) | 1.1 |
| DALLASM LP and LEWISPOL calibration | 0.2 |
| **Total** | **about 22 lane-hours (about 11 box-hours); about 18 lane-hours (about 9 box-hours) if A4 and A5 do not run** |

For reference: the same work with no cuts (TU-1's per-problem solo list, full candidate censuses) is about 35
lane-hours. TU-1's SQP exclusive time alone was 48 lane-hours.

Elapsed time is dominated by exclusive queueing; expect 1.5 to 3 days.

## 14. Settler rulings recorded before commit (ledger C486)

Inputs ruled before the design:

1. Re-baseline every arm on m8 `fd1e318b5` (section 1).
2. SQP candidates (section 4.1):
   * `second_order_correction=false`;
   * `trust_region_growth` in {kAnyAccepted, kRatioOrFeasibilityStep};
   * quasi-Newton-specific versus exact-Hessian-specific trust-region growth to be considered;
   * adaptive dual-regularization caps 1e-10 and 1e-6 excluded;
   * kTier stays non-default;
   * kRefactorize is not a default candidate.
3. IPM candidate: `barrier_initial = 1e-2` (section 4.2).
4. Declare a factorization budget (section 6).
5. The DALLASM 028/029 stationarity measurement is registered (section 9).
6. Discipline:
   * TRAIN only;
   * counters are the currency;
   * zero false certificates;
   * the ship rule is fixed before any row;
   * mutation and must-kill testing only for new hven code;
   * the co-run protocol is declared;
   * a compute budget is declared.

Rulings on the design's open questions (C486):

* OQ-1: R1 adopted. An SQP row reaches only on status `optimal` (section 5.1).
* OQ-2: V1, the solved-count guard, adopted (section 5.3).
* OQ-3: adopted. The mode split is measured with no code. A mode-split winner ships through
  `SqpTrustRegionGrowth::kAuto` in its own default event, with mutation and must-kill testing. The best uniform
  configuration is reported alongside it (section 5.4).
* OQ-4: adopted. The census baseline is C0 at the PIN (section 5.3 V3).
* OQ-5: adopted. Candidate censuses use a throwaway build with the default flipped and a restricted read. The default
  event's full census stays binding.
* OQ-6: DECLINED. There is no extended-cap co-run tier and no calibration for one, because co-tenant load on the other
  lane is uncontrolled. The per-(problem, label) tier assignment from A0 stays (section 7).
* OQ-7: HS 10,000 and CUTEst 250,000, in a set-pin event that bumps `set_version` (section 6).
* OQ-8: not in TU-2. M7-7, M7-11, SCL-R2, W3-R2, W3-R3 and W3-R4 are carried to the M8 register by name (section 4.3).
* OQ-9: not in TU-2. The kRefactorize fallback rung is registered as an RF design item, with no informational leg
  (section 4.3).
* OQ-10: no HOLDOUT confirmation leg (section 3).
* OQ-11: the "fewer settings changed" tie-break adopted (section 5.4 item 3).
* OQ-12: the DALLASM classification thresholds adopted, with the LEWISPOL calibration required first (section 9).
* OQ-13: the IPM grid is {1e-3, 1e-2} only (section 4.2).
