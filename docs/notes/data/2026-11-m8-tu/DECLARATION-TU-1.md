# DECLARATION-TU-1: the tuning pilot, the frozen TU-2 adoption rule, the TU-3 clusters

Task: M8 TU-1 (brief `docs/notes/2026-11-m8-brief.md`, section "TU: tuning and labelled configs", and
section 0). This file is the pre-registration. It is committed on branch `tu1` BEFORE any pilot row is
read and is never edited afterwards. A correction is a successor `DECLARATION-TU-1-r2.md` (then `-r3`, ...)
that names what it changes and is committed before any row it governs. Results go to separate files.

The harness that applies it is hven-bench branch `tu1` (`tu/`; commits in section 11). Its machine copy
of sections 2, 3, 6 and the grids is `tu/tu1_spec.json`; `tests/test_tu.py` holds the two together. Where
they disagree, this file is the authority and the disagreement stops the run.

## 1. Pin

PIN: 0ee37c2ca31c19cef4034d7d985367ddc0e59c08 (hven m8, CI green; set by the settler). It carries TU-0 (the
TU candidate levers exposed as options at today's behaviour, and the `qp` block range checks in
`validate(SqpOptions)`), I17, the I18 EQP accuracy stop, PF-4 batching, TR-S1 (trace `*_caller` keys), the
AP-4 registry and the friction fixes. Every pilot and TU-2 row is taken at this PIN, in a Release install
built by hven-bench `scripts/build_hven_pin.sh` (clang++, `HVEN_LINK_TIME_OPT=OFF`,
`-DHVEN_MKL_THREADING=sequential` per settler ruling C449: the benchmark build links MKL's sequential layer;
the IPOPT lane, unused here, keeps its own threaded binary; under the sequential install its target does
not link and is not built for TU-1). Lane binary `build/lanes/hven/hven_bench_cell` sha256
`06c8e32e472fcc5a450a6ceca0048e38457f62da3e50dfeb9247a9419acce98d` (hven-bench `8253034`). hven-bench `cmake/hven_pin.cmake` names the same
sha (hven-bench pin event 48e1eeae -> 0ee37c2c, declared there). The lane binary's sha256 is recorded in the
pilot's first evidence commit, and the runner refuses a prefix whose `HVEN_PIN.txt` disagrees (the stamp trap).

## 2. The TRAIN families (fixed here; never re-cut after rows are seen)

TRAIN is hven-bench `sets/split-v1.json`'s TRAIN assignment for the two sets the brief names. HOLDOUT is
never run or read by anything this declaration governs.

* **HS (22):** hs003 hs005 hs006 hs007 hs010 hs011 hs012 hs014 hs022 hs025 hs026 hs027 hs028 hs030
  hs033 hs035 hs039 hs043 hs045 hs077 hs079 hs071 (`hs-27 v1` TRAIN plus the H1 bootstrap HS071, which
  shares the `hs` problem_set and the split policy).
* **CUTEst (23):** ACOPP118 ACOPP57 ACOPR118 ACOPR57 BIGBANK BRITGAS DALLASM MSS2 MSS3 NET3 READING6
  READING8 SAWPATH SMBANK STEENBRB STEENBRC STEENBRD STEENBRE STEENBRG TAX213322 TAX53322 YORKNET ZAMB2-8
  (`cutest-nlp-35 v1` TRAIN).

A **cell** is (set, problem, base label) on one engine's lane. A **family** is one set's TRAIN cells on
one engine, pooling that engine's base labels:

| engine | lane | base labels | families | primary counter |
|---|---|---|---|---|
| SQP (default mode) | `hven-sqp` | `default` (exact Hessian), `sqp-qn` (`hessian_mode = kLbfgsDamped`) | SQP-HS (44 cells), SQP-CUTEst (46 cells) | `factorizations` |
| IPM | `hven-ipm` | `default` | IPM-HS (22), IPM-CUTEst (23) | `iterations` |
| SQP at `qp_mode = kIpqp` | `hven-sqp` | `sqp-ipm` | as SQP, one label | `factorizations` |
| SQP at `qp_mode = kSsn` | `hven-sqp` | `sqp-ssn` | as SQP, one label | `factorizations` |

A candidate is judged on the families of the engine row whose mode it is live in (section 8, "live
under"). The last two rows exist only for candidates whose field is inert at `qp_mode = kWalk`; such a
default moves only for users of that mode. The base labels are the lane's existing labels
(hven-bench `lanes/hven/hven_bench_cell.cpp`): SQP labels run `common.max_iterations = 300` (RETAKE13
budget event) and `column_scaling = kAuto`; IPM `default` is `IpmOptions{}` with threads 1 and silent.

## 3. The folds (3-fold CV inside TRAIN; seeded and fixed)

Seed: `hven-bench/tu1-folds/2026-11-19`. Grouped and stratified by set. Problems that are variants of one
model always share a fold, so a held-out fold never holds a near-copy of a training problem. Groups (all
CUTEst; every other problem is its own group): ACOP = {ACOPP118, ACOPP57, ACOPR118, ACOPR57}; MSS = {MSS2,
MSS3}; READING = {READING6, READING8}; STEENBR = {STEENBRB, STEENBRC, STEENBRD, STEENBRE, STEENBRG};
TAX = {TAX213322, TAX53322}.

Assignment, per set independently: order the groups by (size descending, h ascending), h = sha256(seed
NUL set "/" group); give each group in turn to the fold holding the fewest problems of that set so far,
ties to the lowest fold index. (`tu/folds.py`; `--verify` re-derives the lists below.) The lists are the
authority:

| fold | CUTEst | HS | problems |
|---|---|---|---|
| F1 | BIGBANK NET3 STEENBRB STEENBRC STEENBRD STEENBRE STEENBRG ZAMB2-8 | hs003 hs006 hs012 hs014 hs026 hs039 hs043 hs071 | 16 |
| F2 | ACOPP118 ACOPP57 ACOPR118 ACOPR57 DALLASM MSS2 MSS3 SMBANK | hs005 hs010 hs022 hs025 hs027 hs028 hs030 | 15 |
| F3 | BRITGAS READING6 READING8 SAWPATH TAX213322 TAX53322 YORKNET | hs007 hs011 hs033 hs035 hs045 hs077 hs079 | 14 |

A fold holds both base labels of each of its problems, on every engine.

## 4. The frozen TU-2 adoption rule

Frozen here, at TU-1, per the brief. It is RETAKE21's rule (M7 close section 4.2, ruling C184) with the
brief's two changes: the primary counter is engine-specific, and the census veto is added. Applied
mechanically by hven-bench `tu/rule.py` (`--selftest`; `tests/test_tu.py`).

1. **Cell.** The candidate WINS a cell if it reaches and the baseline does not, OR both reach and the
   candidate's primary counter is at least 10% lower. It LOSES symmetrically. Otherwise the cell is a TIE.
   * "Reaches" = the row's `outcome` is `optimal` (the lanes bucket acceptable with optimal). A
     `dnf_wall`, `dnf_budget`, `infeasible` or `error` row does not reach, and its counter is never read.
     A wall-capped cell is therefore not-reached on both arms or on one; both cases are read as written,
     as RETAKE21 read them.
   * "10% lower" = `10 * lower <= 9 * higher` in integers (exactly 10% is a win).
   * A cell converging to a documented alternate local optimum (hs025, hs033; hven-bench `docs/sets.md`)
     counts as reaching, as in RETAKE21.
2. **Primary counter.** SQP (every SQP row of section 2): the row's `factorizations`. IPM: the row's
   `iterations`. Wall-clock is never the metric.
3. **Family sign.** sign(wins - losses) over the family's cells.
4. **Move.** A default moves only if it wins on at least one family and loses on none. With the baseline
   = the current default and the candidate = the moved value, on every cell of the engine row's families.
5. **Census veto.** For every candidate on an SQP row (all of which can move the walk), the 57-cell walk
   census (`scripts/run_walk_census.sh`, against `bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv`)
   may not lose a status that is not deadline-dependent:
   * a LOSS is a cell whose baseline status is `Optimal` and whose candidate status is not;
   * deadline-dependent, and so outside the veto: a cell whose status on either side is `dnf_budget` or
     `dnf_setup` (the wall budget ended it), and `f7_n10000_path_physics` (named wall-dependent in the
     baseline's header);
   * a deadline-dependent change is a candidate to re-run alone at full budget (CLAUDE.md section 7),
     reported, and not part of the veto;
   * an SQP candidate whose family rule passes and whose census has not been read does not move.
   IPM candidates do not run the census (they cannot move the SQP walk; CEN-R3 is a walk rule).
6. **What the rule does not replace.** Each adopted default is a declared default event with its own
   census and 27-cell replay (brief TU-2), and lands through the ordinary gates. The brief's TU-2
   precondition stands: if a candidate moves an IPQP barrier default, M7-42's certification fixture class
   (`Sigma = 2 mu_stop / s^2`) is built first.

## 5. The pilot rule: when automated configuration is adopted over manual sweeps

### 5.1 Scope

The pilot runs on the SQP default-mode row (section 2) only. It decides the method TU-2 uses for that
row. The IPM row's pool is one key (A2-R2) and the mode-specific rows hold conditional candidates
(section 8), so neither has a configuration space to search; they are tuned by manual sweep whatever the
pilot says. The pilot reads no census: it chooses a method, not a default.

### 5.2 The manual sweep (`tu/sweep.py`)

Grid {key: [values]}, the shipped default first. Arms: the default {} and, per key, one arm per
non-default value (one factor at a time). Pre-flight before any cell: each arm's options dump (lane
`--dump-options-only`) differs from its base label's dump in exactly the arm's keys and hven's
`validate()` accepts it. Selection on a set of training cells:

1. each arm is judged against {} by the family rule of section 4 (items 1-4);
2. passing arms rank by (families won, descending; total net wins, descending; sum over cells both arms
   reach of log(baseline counter / arm counter), descending; label, ascending);
3. per key the best passing value is kept; if two or more keys keep one, their COMBINATION is measured and
   judged the same way;
4. the manual best is the highest-ranked passing configuration among the single arms and the combination;
   if none passes, it is the default {}.

### 5.3 The configurator (`tu/configurator.py`)

Written in hven-bench (Apache-2.0) from published descriptions only: racing (Birattari et al., GECCO 2002;
Lopez-Ibanez et al., Oper. Res. Perspect. 3, 2016) over seeded random sampling (Bergstra and Bengio, JMLR
13, 2012). No configurator is executed as a tool; irace is not used in any form.

* Space: the product of the grid's value lists. The first incumbent is the default {}.
* Order: the space minus {}, shuffled by `random.Random("hven-bench/tu1-configurator/2026-11-19/<fold>")`.
* Race: a challenger meets the incumbent on the training cells in blocks of 4 problems (block order
  shuffled once per fold by the same generator). After each block the per-family tallies (section 4 cell
  rule) are read and the challenger is eliminated as soon as the family rule cannot pass: some family's
  losses minus wins exceed its unseen cells, or no family can still reach +1.
* Promote: a challenger that survives every block and passes the family rule against the incumbent
  becomes the incumbent.
* Budget: challenger (configuration, cell) evaluations, at most the manual sweep's count on the same
  training cells: (number of non-default OFAT arms + 1) x training cells. The race stops at the first
  block that would exceed it.
* Result: the final incumbent.

### 5.4 The pilot grid (SQP default-mode row)

| key | values (default first) | candidate |
|---|---|---|
| `advanced.elastic_rung_engine` | kWalk, kTier | M7-1 |
| `advanced.second_order_correction` | true, false | GL-S1 |
| `advanced.adaptive_dual_regularization_max` | 1e-8, 1e-6, 1e-10 | M7-15 (section 8) |
| `qp.working_set_algebra` | kSchurBorder, kRefactorize | A7 static arm (section 8) |
| `advanced.trust_region_growth` | kRatio, kRatioOrFeasibilityStep, kAnyAccepted | GL-R1 |

Joint space 72 configurations; OFAT arms 7. The pilot grid is the brief's two globalization candidates plus
the default-mode candidates eligible before TU-0 (M7-1, M7-15, the A7 static arm). The other TU-0 levers
(M7-7, M7-11, SCL-R2, W3-R2..R4) are TU-2 candidates only; TU-2's value grids are declared in TU-2's own
declaration before its rows.

M7-15's axis is the ceiling of the shipped adaptive dual-regularization schedule
(`advanced.adaptive_dual_regularization_max`, settler direction): with the schedule on (the default) the walk
never reads `qp.dual_regularization`, so a sweep of that field would not measure the walk's mu.

### 5.5 The rule

For each fold k in F1, F2, F3: TRAIN(k) = the cells of the other two folds; HELD(k) = fold k's cells.
manual(k) = 5.2 on TRAIN(k); auto(k) = 5.3 on TRAIN(k). On HELD(k), the section 4 cell rule with
candidate = auto(k) and baseline = manual(k). The three held-out comparisons are POOLED (each TRAIN cell is
held out exactly once) and the family rule (section 4 items 3-4) is applied to the pool.

**Automated configuration is adopted for TU-2 (SQP default-mode row) iff auto wins at least one family
of the pool and loses none. Otherwise TU-2 runs manual sweeps.** The outcome is recorded either way.
Applied by `tu/pilot.py`. A row measured once is reused (rows are deterministic per binary, label and
cell in the columns read), so the configurator and the manual sweep share one cell store.

## 6. TU-3 label clusters (defined before any TU-2 row)

From the CUTE classification code (`CLASSF.DB` at SIF commit `ebbc673b`, sha256 `380078b8...a2b1`, the
file `cutest-nlp-35 v1`'s rule read; HS codes from the same file's `HSn` rows) and `n`, `m` from that
file. Code `OCrd-Ki`: O objective, C constraints. Predicates in `tu/clusters.py`:

| cluster | predicate | TRAIN members | problems |
|---|---|---|---|
| K1 network | C = N | BIGBANK DALLASM SMBANK STEENBRB STEENBRC STEENBRD STEENBRE STEENBRG | 8 |
| K2 quadratic objective, general constraints | O = Q and C = O | ACOPP118 ACOPP57 ACOPR118 ACOPR57 SAWPATH | 5 |
| K3 constraint-dominant | m > n | ACOPP118 ACOPP57 ACOPR118 ACOPR57 NET3 SAWPATH TAX213322 TAX53322 | 8 |
| K4 large | n >= 500 | BIGBANK MSS2 MSS3 READING8 SAWPATH STEENBRC STEENBRE STEENBRG TAX213322 | 9 |
| K5 sum-of-squares objective | O = S | YORKNET hs011 hs014 hs025 hs027 hs028 | 6 |

Each holds at least five TRAIN problems (so at least five cells on every engine row). Considered and not
formed (fewer than five TRAIN problems): bound-constrained only (C = B: hs003 hs005 hs025 hs045), irregular
(r = I: BIGBANK BRITGAS NET3), linear objective (O = L: hs010 hs039), real-world origin (K = R: BIGBANK
BRITGAS NET3 ZAMB2-8).

**Label rule** (brief TU-3): a label ships for cluster K only if a configuration, against the TUNED
default (after TU-2), wins on K by the section 4 rule (K's cells on the engine row as one family, sign
+1) while losing on no family of section 2 for that engine row -- that is, its sign is not -1 on
either that engine row's HS family or its CUTEst family. Labels are curated settings of existing
fields. If no cluster separates, no label ships, and that is a result. Clusters are never redefined after
TU-2 rows are read.

## 7. The pilot's reported quantities (informational; no decision reads them)

`tu/gl0_report.py`, from the pilot's rows and per-cell traces (hven-bench `run_serial.py --trace`), for
every SQP cell:

* factorizations and QP minor iterations per accepted step (accepted = `accepted_f_type +
  accepted_h_type`, checked against `steps_accepted`);
* restoration cost: `restoration_entries`, `restoration_iters`, and `restoration_iters / major_iters`;
* the GL-0 counters: rejections by cause (`reject_funnel_ceiling`, `reject_f_armijo`, `reject_h_funnel`,
  `reject_qp_failed`, `reject_other`), f-type vs h-type accepts, SOC attempts (`soc_steps`) and rescues
  (`soc_applied`), and `rejection_run_histogram`;
* the switching ratio R_k (`sqp.switching_ratio` on every depth-0 iter row) by size band of n:
  S (n <= 10), M (10 < n < 500), L (n >= 500): count, share >= 1, the 10/50/90% quantiles of the finite
  values, and the inf/-inf/nan/null counts. This is GL-X1's input (brief).

Restoration factorizations are not separately counted at this pin; restoration cost is reported in
iterations and entries.

## 8. The TU-2 candidate pool

The brief's pool: section 5 group 1, plus the registers that are levers, plus GL-R1 and GL-S1. Each name
resolved to its source. "Field" is the dump key the lane override grammar accepts (hven-bench
`lanes/hven/hven_bench_cell.cpp`, closed whitelist). **NO FIELD** = no option field exists at the PIN, so
the harness cannot sweep it; it enters TU-2 only if a field lands (a successor declaration names it).

| candidate | what | source | field (default) | live under | status |
|---|---|---|---|---|---|
| M7-1 | elastic rung engine kWalk vs kTier | A6 final lists; close 4.3; C182 | `advanced.elastic_rung_engine` (kWalk) `sqp_options.h:186` | every mode | eligible; pilot grid |
| M7-7 | take vs decline the elastic-site restoration offer (D6) | A6 triage :94; M6 ledger :5104 | `advanced.restoration_elastic_seed` (true) `sqp_options.h:459` (TU-0) | every mode | eligible (TU-2) |
| M7-11 | `rho_0` headroom cap `rho_0 <= kElasticRhoMax / kElasticRhoFactor^k` (D12); k = 1 already ships (W2 T5) | A6 triage :98 | `advanced.elastic_rho0_headroom_rungs` (1), [1, 6], `sqp_options.h:463` (TU-0) | every mode | eligible (TU-2) |
| M7-13 | rejected-SOC point `x_soc` as a restoration candidate (D14) | A6 triage :100 | none | | RETIRED (TU-0 spec): where SOC did not run `x_soc` does not exist and producing it is a new mechanism; no RST/YKN/RD8 cell was seed-limited (C124 (3)) |
| M7-15 | the two `mu = 1e-6` pathologies (D16): ill-scaled feasible family reads kInfeasible; `w2_box_blocked_qp(0.5)` elastic at rho 1e7 | A6 triage :102; M6 ledger `docs/notes/2026-08-m6-ledger.md:2290`, `:2310` (`mu` = the walk's dual regularization; mapping accepted by the settler) | `advanced.adaptive_dual_regularization_max` (1e-8), [1e-12, 1], `sqp_options.h:467` (TU-0; the ceiling of the shipped adaptive schedule, settler direction) | kWalk | eligible; pilot grid |
| M7-41 | restoration seed generalization | A6 final lists; C124 (3) | none | | RETIRED (TU-0 spec): no mechanism defined; no restoration cell was seed-limited |
| SCL-R2 | power-of-two row factors (NIT-1) | m7-w2-c10-report :409, :418 | `row_scaling_power_of_two` (false) `sqp_options.h:604` (TU-0); live where the row layer is engaged (`row_scaling`, or kAuto engaging it) | every mode | eligible (TU-2) |
| A1-R4, A1-R5, A1-R6 | exponent threshold vs collapsing-slack rows; row-family fixture; band fallback on warm majors | C128 | none | | RETIRED (TU-0 spec): A1 a pre-registered negative; field held on `w2-a1-impl` |
| A2-F1 | an un-ceilinged complementarity mu_0 (a new design) | C129 | none (`advanced.ipqp.barrier_initial_rule` holds only the ceilinged form A2 measured losing) | | RETIRED to the register (needs a new design) |
| A2-F2 | residual floor vs stopping-mu face accuracy (one dial with A1-R4) | C129, C130 | `advanced.ipqp.barrier_residual_floor` (false) `qp_options.h:264` | kIpqp | conditional: re-offered only with T-a and E1-c passing (C129 ruling 2); M7-42 precondition |
| A2-F3, A2-F3b | the harness side file could not see three IPQP counters | C129, C130 | not a lever (TR-E's `dump_counters` side file now carries every `ipqp.` counter) | | not a candidate |
| A2-F4 | a lost fixture cell vetoes a default move | C129 ruling 2 | not a lever (a ruling) | | not added to the frozen rule (settler ruling): the default event's ctest gates suffice |
| A2-R1 | PIQP Alg. 2 proximal coupling (paper only) | C129 | none | | RETIRED (new design; no constant) |
| A2-R2 | the NLP IPM's initial barrier parameter | C129 | `barrier_initial` (1e-3) `ipm_options.h:212` | IPM | eligible; IPM manual grid {1e-3, 1e-4, 1e-2, 1e-1} |
| A2-R3 | warm-kill restart under the mu_0 rule | C129 | none | | RETIRED (depends on kComplementarity, which ships OFF and lost) |
| A3-R2 | the ladder-skip latch is one-way | C138 | none | | RETIRED (a re-arm criterion is undesigned); A3-R5's precondition is therefore unmet in M8 |
| A3-R3 | a per-major convergence-slack schedule | C138 | none | | RETIRED (new design; the static slack measured losing or vetoed) |
| A3-R4 | pre-registration lesson (no "new counter is zero at OFF" clause when the default is the current rule) | C138 | not a lever | | applied: no such clause here |
| A3-R5 | `ladder_skip_after` 1 re-test | C138 | `advanced.ipqp.ladder_skip_after` (3) `qp_options.h:273` | kIpqp | conditional on A3-R2 resolved |
| A7-R1..R4 | re-probe after a switch; cap-retry keeps the algebra; witness net of shared factorizations; size-aware cost currency | C134 | none | | RETIRED (TU-0 spec): A7 a pre-registered negative; `ws_algebra_rule` held on `w2-a7`; A7-R4 stays an input to M8's cost model |
| A7 static arm | the fixed working-set algebra kSchurBorder vs kRefactorize | C134; settler ruling (TU-1) | `qp.working_set_algebra` (kSchurBorder) `qp_types.h:139` | kWalk | eligible; pilot grid |
| W3-R1 | R5 preconditions (certify from face) | C146 | `advanced.ssn_certify_from_face` (false) `sqp_options.h:237` | kSsn | conditional on F-c (the SSN weak-active-indefinite fixture and the factorize_count pin) |
| W3-R2 | R4 at a Farkas tolerance of 1e-4 | C146 | `advanced.ssn_farkas_residual_tol` (1e-6), (0, 1), `sqp_options.h:447` (TU-0), read with `advanced.ssn_infeasibility_rule = kFarkasGated`; the IPQP Farkas gate keeps its constant | kSsn | eligible (TU-2, `sqp-ssn` row) |
| W3-R3 | the SSN uncertain-band tau | C146 | `advanced.ssn_uncertain_tol` (0.1), [0, 1), `sqp_options.h:450` (TU-0) | kSsn | eligible (TU-2, `sqp-ssn` row) |
| W3-R4 | R1 (c, exponent) sensitivity | C146 | `advanced.ssn_lm_sigma_c` (1.0, > 0) `sqp_options.h:453` and `advanced.ssn_lm_sigma_exponent` (2.0, [1, 2]) `:456` (TU-0), read under `advanced.ssn_sigma_rule` kResidualArmed/kResidualAlways | kSsn | eligible (TU-2, `sqp-ssn` row) |
| W3-R5 | tau 1000 solves BRITGAS | C146 | `kFunnelTauBar` constexpr | | EXCLUDED by the brief (the initial funnel width is deprioritized) |
| GL-R1 | trust-region growth: A current (grow x2 when active and ratio >= 0.75), B A or an accepted h-type step when active, C KLV (any accepted step when active) | brief GL | `advanced.trust_region_growth` (kRatio; kRatioOrFeasibilityStep; kAnyAccepted) `sqp_options.h:444` (TU-0) | every mode | eligible; pilot grid |
| GL-S1 | second-order correction on vs off | brief GL | `advanced.second_order_correction` (true) `sqp_options.h:104` | every mode | eligible; pilot grid |
| GL-X1 | infeasibility measure / switching condition | brief GL | research register | | not a candidate (brief) |

A candidate surfaced by the pilot enters TU-2 only through a successor declaration committed before its
rows. File and line references are at 48e1eeae2, except those marked TU-0, which are at the PIN.

## 9. Measurement protocol

Every leg runs under the box scheduler `box-lane` (one leg per step, never a multi-hour hold), with HOLDOUT
never run. The harness's cell store (`tu/cells.py`) splits each (configuration, base label, set) batch:

* **Co-run tier (counter-only legs).** `box-lane -- run_serial.py --corun-width 4` (HB-2,
  `docs/corun.md`): one lane of 4 physical cores, one process per pinned PHYSICAL core (core map from sysfs
  within the lane's affinity, SMT siblings idle), `MKL_NUM_THREADS=1` and `OMP_NUM_THREADS=1` in every
  process (and MKL's sequential layer by build). The rule reads only deterministic columns (outcome,
  `factorizations`, `iterations`). The protocol is declared in every results file's provenance block
  (`run.mode: co-run`, `wall.quotable: false`). Wall-clock from it is informational, never quoted, never
  compared with a serial timing.
* **Exclusive tier (wall-dependent cells).** `box-lane --exclusive -- run_serial.py` (serial, the whole
  box): (a) the declared solo problems -- ACOPR118 BIGBANK BRITGAS MSS2 MSS3 READING8 SAWPATH TAX213322
  TAX53322 YORKNET, the CUTEst TRAIN problems with a wall-capped hven cell in RETAKE21's ON arms or the H3
  roster -- and (b) every co-run cell whose reported row ends `dnf_wall`: it is re-measured here, and the
  co-run row is named in that run's `superseded.txt` and never read. Each cell runs with its full budget.
* **Ceilings.** The set pins' `budget_wall_s`: HS 600 s, CUTEst 300 s. A capped cell is `dnf_wall`
  (not reached). Iteration caps are the labels' (SQP 300, IPM `common.max_iterations` 1000).
* **Deviations.** A deviation seen under the co-run is a candidate: the cell is re-run alone and the
  deviation reproduced before it is used.
* **Stamp.** Every results file carries the runner's provenance block (harness sha, pin sha, compiler,
  host, CPU, date, run mode). Apple/Accelerate and Windows: UNOBSERVED (the pilot is Linux only).

## 10. The dry run (pipeline evidence only; NOT pilot data)

Before this declaration was committed, the pipeline was exercised end to end on 4 cells: hs071 and hs035
(TRAIN, fold F1 and F3) x {`default`, `default+advanced.second_order_correction=false`} on `hven-sqp`,
traced, co-run width 2, at the earlier pin 48e1eeae2 (hven-bench `runs/2026-11-m8-tu1-dryrun/`). Its rows prove the fold
lookup, the override labels, the pre-flight, the co-run store, the rule evaluator and the GL-0 report run;
they are not pilot data, are not reused by the pilot (the pilot measures into its own store), and no
decision reads them. It also confirmed the `switching_ratio` key in the traced cells' iter rows. Its
record is `runs/2026-11-m8-tu1-dryrun/README.md` in hven-bench, not this file.

## 11. Harness

hven-bench branch `tu1`: `e2afabf` (override grammar, folds, rule, sweep, configurator, pilot, clusters,
GL-0 report, `tu/tu1_spec.json`, unit tests on synthetic rows), `8458b82` (sweep `--base-labels`), `d6cb3b8`
(the dry-run record, section 10), `7f75aae` (the A7 static arm), `8253034` (the pin event to the PIN, the TU-0 keys
in the lane whitelist, the two-tier box-lane store, 28 unit tests).

## 12. Settler rulings recorded before commit

1. PIN: 0ee37c2ca (after I17 and TU-0); benchmark builds use `-DHVEN_MKL_THREADING=sequential` (C449).
   M7-15's axis is `advanced.adaptive_dual_regularization_max`.
2. M7-15 maps to `qp.dual_regularization`: accepted.
3. A2-F4 is not added to the frozen rule; the default event's ctest gates suffice.
4. The static `qp.working_set_algebra` arm (kRefactorize) enters the pool as an SQP candidate.
5. Grouped folds (section 3): confirmed (no model-family leakage across folds).
6. The cluster rule's "losing on no other family" is read as stated in section 6.
7. `validate(SqpOptions)` accepting `qp.dual_regularization = -1` is a defect; fixed in TU-0.
