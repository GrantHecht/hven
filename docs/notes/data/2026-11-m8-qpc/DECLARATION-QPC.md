# DECLARATION-QPC -- the SQP walk QP's correctness round: converged refinement at the release re-admission, and the certificate's data half

> **STATUS: UNFROZEN DRAFT (2026-11-25, after settler rulings C553).** Not a pre-registration yet. Values marked ⟨...⟩
> are filled before freeze from development readings or the harness, never from a round row. The harness is hven-bench
> `ens` `27c02687` (section 2; settler C559). A Fable review comes before freeze. No leg of this file has run.

Task: M8 QPC, the SQP QP-subproblem correctness round that replaces the deferred P-1a dual-centring round. Once frozen,
this file is committed with every leg script it names (`legs/`), the frozen cell lists (`cells/`), the harness pin and
`BINARIES.txt` BEFORE any QPC row, and is never edited afterwards. A correction is a successor `DECLARATION-QPC-r2.md`
committed before any row it governs; a failed or void leg chain keeps this file frozen and is answered by a successor
with re-derived bars. Results go to `RESULTS-QPC.md`. Criteria are never changed after data.

Governing documents, in order of precedence: the settler rulings on the QPC brief (D-2T; the certificate gate; the
P-1a Q2 ship bar; C537(5) as REPLACED by C539 -- the ensemble rule of the I19-2a results review, section B4, its B2
tail-guard and same-solution amendments, and I1's stability endpoint); the P-1a decider report and diagnosis
(`.superpowers/p1a-deciders-2026-11-24.md`, `p1a-diag-2026-11-24.md`; scratch evidence `origin/p1a-diag`
`51b392194`, `docs/notes/data/2026-11-m8-p1a-diag/dec/`); the P-1a approach review (Fable). Precedents:
DECLARATION-I19-2a and its RESULTS (ship-rule machinery; the placebo lesson); DECLARATION-WA1-r7; PF-1b REPORT.

## 1. The defects and the change

**D1 -- the release re-admission cycle.** The walk's EQP regularizes its constraint rows by -mu*lambda and takes ONE
mu-free refinement step, so a working row on a face whose smallest Schur eigenvalue s_min is ~mu is left off its bound
by up to the footprint mu*|lambda|. When the drop rule releases that row, its allowance collapses from mu*|lambda| to the
feasibility floor and the shift refresh re-admits it: release -> re-admission -> release, to the minor cap (PF-1b:
f7_n1000_path_corrupted QP1, period 4 from minor 311, row 905). Today's answer is a dual_mu rung (x1e-2, at most twice
per solve), which takes mu 1.5 decades below the backend's static-pivot floor (2394 perturbed pivots on that face).
Diagnosis (P-1a deciders): per eigen-direction the multiplier error contracts by mu/(mu+s_i) per mu-free step -- the
same contraction a proximal dual-centre commit gives -- measured 0.74 per step on PF1's cycle face (D-1); converging
puts the row on its bound before release (D-3).

**D2 -- false QP certificates.** Every row test the walk certifies on excuses min(mu*|lambda|, 1e-6*row_scale) beyond
feas_tol*row_scale. With |lambda| ~ 1e5 that is the 1e-6 cap. Under the P-1a prototypes the walk certified QC8 DALLASM
qp7 kOptimal at f = -92397.2848 against the optimum -92397.2698: one equality row 72.9x feas_tol off, complementarity
product 7.0e6x feas_tol. The unregularized EQP of the final working set REPRODUCES the false point (the set is itself
inconsistent at 7.3e-8, rank 299/300), so the EQP oracle P-1a planned would have passed it (D-4).

**The change** (code head ⟨sha⟩ on R; commits ⟨list⟩), unconditional, no option added (the public API is locked):

* **C1 (D-2T).** At the first release re-admission of a solve, `eqp_refinement_` becomes `kConverged` for the rest
  of that solve: every EQP solve (eliminated `solve_eqp`, bordered `solve_bordered_eqp` and the verdict-site
  refinements) takes further mu-free steps against the same proximal target (mu backed out, delta kept), each kept
  only if it strictly reduces the residual's inf-norm, until kBorderRefineRelFloor*max(1,||rhs||inf) or
  kMaxBorderRefineSteps = 10 steps in all. It replaces the accuracy stop (eliminated) and the footprint / STEP RESOLVED
  / accuracy loop (bordered, dim() == 0 included). dual_mu = 0 and dense-shifted factors keep the model step. The
  dual_mu rung is deleted; dual_mu and the factor state are untouched at the trigger. QpCounters /
  SqpCounters `converged_refinement_arms` (0 or 1 per walk). `dual_regularization_decreases` is identically 0
  (kept as that invariant; the API is locked).
* **C2 (the certificate's data half).** At the walk's one certified exit, after the bound and sign halves and the
  gray-row returns, `detail::certificate_reading` is read on the point and multipliers about to be exported, from the
  QP's own data. Certified iff
  `rows <= 10 * feas_tol` AND `product <= 100 * feas_tol` (each term over its row's LOCAL dual scale, settler C567) AND
  `stationarity_beyond_rounding <= 10 * opt_tol` (C553 Q3) AND `slack <= 100 * feas_tol` (settler C568) AND
  `off_working == 0` (pre-freeze review S2) AND the
  exported x, lambda_e, lambda_i, z are finite; every reduction is NaN-sticky (review S8), so a NaN anywhere -- in the
  exported vectors or in the QP's data -- refuses; otherwise kNumericalError, `certificate_gate_refusals`.
  Norms and scalings (and why):
  - rows = max over equality rows |a_j x - b_j|, inequality rows max(0, a_j x - b_j) and bounds
    max(l_i - x_i, x_i - u_i), each divided by its row scale max(1, |b_j|, ||a_j||_1 ||x||inf) (bounds:
    max(1, |bound|, ||x||inf)) -- the infinity norm because a certificate is a per-row claim, relative to the scale
    every row test of the engine already reads so the gate is invariant to row scaling and to ||x||;
    10 = kInfeasibilityMarginFactor, the walk's own structural margin: the gate asks the exported point exactly what
    the classifier asks a footprint-free violation;
  - product = max over equality rows, working inequality rows and pinned bounds j of
    (|residual_j| / row_scale_j) * |multiplier_j| / s_j, where s_j is j's **LOCAL dual scale**: with
    t_i = max(|g_i|, |(Hx)_i|, |(Ae'lambda_e + Ai'lambda_i)_i|, |z_i|) the stationarity-term magnitudes of column i,
    s_j = max(max over i in supp(a_j) of t_i, |multiplier_j| * ||a_j||inf) for a row, and s_i = t_i for a pinned bound
    (its own column; the join is |z_i|, already in t_i); a zero numerator reads 0. 100 = kTerminalPolishComplFactor,
    1e-7 at the defaults, a decade under the drivers' kkt_tol. **History (each a defect found before freeze):** (1) the
    first QPC build read the product UNSCALED, which is not invariant to the objective's scaling and refused
    rounding-level elastic-rung points (YORKNET's major-0 ladder: rows 5.2e-16, product 3.0e-7; HS15's cold elastic rung
    at rho 2e8: rows 9.1e-14, product 9.1e-6; the circle-line elastic QP: rows 1.2e-11, product 3.0e-7); (2) the C553
    follow-up divided by a GLOBAL dual scale max(1, ||Hx||, ||g||, ||A'lambda||, ||z||), which the pre-freeze review
    showed a decoupled block can switch off (F-SLACK plus one variable pinned under z = 1e9: a working row 0.1 slack
    certified at an objective error of ~100 on |f*| ~ 1000; settler C567). The local scale is invariant to the
    objective's scaling (every term scales with f) and reads only the columns row j touches, so a block that does not
    touch row j cannot inflate it. When row j's own multiplier dominates s_j the reading is at most
    rel_j / ||a_j||inf -- the row's relative slack -- which is the side the rows reading cannot see: a working
    inequality or a pinned bound held SLACK under a multiplier, a KKT point of the shifted row and not of the QP
    (B1Gate's QP: slack 8e-7 under lambda 1e6, local scale max(|g| = 1e6, |A'lambda| = 1e6, |lambda| ||a||inf = 1e6) = 1e6, so 8e-7 = 800 feas_tol; HS15's warm rungs ⟨HS15⟩ feas_tol). qp7's false point is
    refused by its rows (72.9x).
  - stationarity (C553 Q3) = the free-block residual (Hx + g + Ae'lambda_e + Ai'lambda_i)_free read BEYOND ROUNDING:
    component i sums c_i terms of total magnitude m_i, so an exact KKT point can read c_i u m_i in double (Higham's
    gamma bound, unit roundoff u = eps/2, review S1); the reading is max_i max(0, |r_i| - c_i u m_i) over the GLOBAL
    stationarity scale max(1, ||Hx||, ||g||, ||Ae'lambda_e||, ||Ai'lambda_i||) (free = not pinned at a bound);
    10 opt_tol. The KKT-point test certification is reached through reads the normwise residual at 1x opt_tol on the
    same scale, but on the pass's point and BEFORE the terminal polish and the gray returns; the data half re-reads it
    on the EXPORTED (x, lambda) so every term of the certificate is read last, from the QP's data, on what leaves the
    engine. The rounding floor is taken out because a normwise residual can sit beneath what double resolves where large
    terms cancel (SAWPATH's rung: terms of 2.4e13 summing to 3.6e2; the normwise draft refused it, section 7);
  - slack (settler C568) = max over WORKING inequality rows and pinned bounds j whose multiplier is NOT ROUNDING-ZERO of
    |residual_j| / row_scale_j, where rounding-zero means |multiplier_j| * ||a_j||inf <= 100 * u * s_j (s_j the local
    dual scale above, u = eps/2; a pinned bound: ||a||inf = 1, s = t_i; a NaN multiplier counts as priced); 100 feas_tol.
    Any product-type scaling lets a multiplier small against its own columns' stationarity terms excuse up to footprint
    slack, and TRAIN-238 reaches it: HS101 sqp-qn certified three exits with its working inequality I2 held 471-578
    feas_tol slack under lambda 100 / 1000 against s = 1.3e5 (row-local product 0.44-4.2 feas_tol). The slack test is in
    row units on one side and local-scale units on the other, so it is invariant to the objective's scaling; the
    rounding-zero exemption keeps weakly active rows whose multiplier double cannot resolve on their columns (CRESC4:
    lambda ~ 1e-21 against s ~ 161, slack 9.4e6 / 6.5e7 feas_tol). Equality rows need none: the rows reading holds them
    two-sided at 10 feas_tol. The row-local product test stays;
  - off_working = the largest |lambda_i,j| on a non-working row and |z_i| on a free variable; the pricing writes exact
    zeros there and no other reading looks at those entries, so a nonzero one would leave the engine unread: required
    exactly 0 (review S2);
  - finiteness and NaN: the four exported vectors must be finite, and every running maximum in the reading is
    NaN-sticky (`nan_sticky_max`), so a NaN in the QP's data (b, bounds, g) reaches the verdict too (review S8; a
    `std::max` reduction drops a NaN term).
  The walk's own tests keep the footprint excuse; only the certificate loses it. Signs are NOT re-read by the data
  half: they are the sign half's, unchanged.

**What C2 guarantees, and its limitation (stated, settler C567).** A kOptimal exit requires, on the exported (x,
lambda, z) and the QP's data: (i) every row and bound within 10 feas_tol relative, (ii) every equality row, working row
and pinned bound's residual*multiplier product within 100 feas_tol of its OWN columns' dual scale, (ii') every working
row and pinned bound whose multiplier is not rounding-zero within 100 feas_tol (relative) of its bound, (iii) free-block
stationarity beyond rounding within 10 opt_tol of the global stationarity scale (and normwise within 1x on the pass's
point by the KKT-point test), (iv) no nonzero multiplier off the working set, (v) finite vectors and no NaN anywhere in
the reading, and (vi) multiplier signs within opt_tol*scale (sign half). No term contains dual_mu or primal_delta, and
the reading is computed after the last mutation of x and the multipliers. These are RELATIVE bounds, not a proof that
the exported point is the QP's solution: (ii') bounds a priced working row's or pinned bound's slack by 100 feas_tol
of its row scale, so the objective error a slack constraint carries past (i)-(vi) is at most about
|multiplier_j| * 100 feas_tol * row_scale_j per constraint, and a rounding-zero multiplier carries at most
100 u s_j / ||a_j||inf times its slack; (before C568, (ii) alone let a row priced at a small fraction of the gradient
magnitude on its columns stand off by up to 100 feas_tol * s_j / |multiplier_j|, which HS101 reached); and (iii)'s scale is global, as the upstream KKT-point
test's is, so a decoupled block with large gradient terms widens the stationarity tolerance of every free column
(the analogue of the review's F-SLACK for stationarity; not reached by any fixture or development reading of this
round, section 4). The round's evidence that no false certificate leaves under these bounds is empirical: the
development reading of section 4 (M1) and leg Q's independent recomputation on SENS-Q. A "false certificate" in this
round's legs is a kOptimal exit failing (i)-(vi) as recomputed by INDEPENDENT code (`qp_dump_kkt`-class recomputation
on SENS-Q; `tu/stationarity.py` on NLP rows) or an objective off a pinned optimum.

**Why C1 cannot certify anything.** It changes only which answer an EQP solve returns, toward the same proximal
target the single step aims at; every exit still passes the unchanged certificate plus C2.

## 2. Pin, binaries and the ensemble

* **R (= H0, the baseline)** = origin/m8 `99712ec32` (settler C547; code-identical to `96789b9be`, the diff is TU-3
  docs only). **R's ensemble rows ARE the rows of the baseline ensemble census of that pin** (hven-bench `ens`
  `27c02687d9389ed43f3dbef27b4cbe389ca2b5d0`, the census RESTARTED at that sha (C559); lane binary sha256
  `7d2f9f376d6f9f82d554efc2a41a1e52a36769a061d02060c5a765d236c573ca`; store
  `hven-bench-ens/.scratch/census-m8/store`), verified at freeze by the binary/pin identity check (the store's
  recorded binary sha and pin must equal BINARIES.txt's R entry; a mismatch STOPS). No R ensemble leg is re-run.
* **H** (arm name `QPC`) = ⟨sha⟩ = R + the QPC commits (branch `qpc`, based on `99712ec32`). `git diff R H -- src
  include` is exactly C1 + C2 + the counter folds.
* **Component arms (C553 Q2): `QPC-C1` (C1 only) and `QPC-C2` (C2 only)**, so that either component can ship alone.
  Mechanism: two dev branches off the H commit, each carrying ONE commit that reverts the other component in place:
  - `qpc-c1` = H + "revert the data half": the gate block at the certified exit becomes `status = QpStatus::kOptimal;`
    (the R statement); everything else (C1, the counter fields, their folds, the docs) is H's.
  - `qpc-c2` = H + "revert the converged refinement's arming": the release re-admission block becomes R's dual_mu rung
    (x kDualRegDecreaseFactor, factor state discarded, at most `kMaxDualRegDecreases = 2` per solve, constant restored);
    the EQP sites still pass `eqp_refinement_`, which is then kModel for the whole solve, i.e. R's single model step.
  WHY this mechanism: the shipped code stays exactly H (no option, no macro, no dead branch -- the API is locked and a
  compile-time switch in shipped code would be a second configuration nobody ships); each component arm is H minus one
  reviewed, self-describing diff (`git diff H qpc-c1 -- src include`, `git diff H qpc-c2 -- src include`, recorded in
  `gate-QPC/`), so "C1 alone" is literally H with C2 removed, not a re-implementation; the counter schema is identical in
  all three arms (a reverted component's counter reads 0), so every reader and comparator runs unchanged. If a component
  ships alone, the shipping commit is the corresponding revert landed on `qpc` (its diff is the one the arm measured).
  The branches are pushed but never merged as such. Development commits: `qpc-c1` `e4660dd31` and `qpc-c2` `1ed77a336`,
  both on the code commit `02407ea83` (the reverts are produced by a script kept with the legs, so they are re-made on the
  final H at freeze and their shas entered in BINARIES.txt; `legs/make_arm.py`).
* Every arm is (build, ensemble member); there are no runtime levers. Four installs: R (the census install, reused),
  H, H-C1, H-C2, each built by hven-bench `scripts/census_build.sh` (the census recipe: Release, no tests/bench, no LTO,
  sequential MKL, clang; `HVEN_PIN.txt` in the prefix), each with its lane binary `hven_bench_cell` built from
  hven-bench `ens` `27c02687` (lanes tree `4319ab1512e2de6dbc385eb964be182f0af4eb70`, the census binary's own commit).
* **Harness: hven-bench branch `ens`, commit `27c02687d9389ed43f3dbef27b4cbe389ca2b5d0`** (settler C559, replacing the
  earlier draft's `ens-reader` `37e24bb`: the C556 lane change moved `lanes/`, and the census was restarted at this sha;
  `docs/ensemble-rule.md`; `tu/ens.py`, `tu/ens_members.py`, `tu/i19_run.py`). R's census rows and every arm's rows come
  from lanes built at this ONE commit (lanes tree `4319ab15...`), which the reader's row-producing-path check requires.
* **Declared binaries (`BINARIES.txt`, committed before the first arm row; the reader reads only `declare` lines):**

      declare H0     prefix=/home/ghecht/Projects/hven/.claude/worktrees/census/.scratch/install 99712ec324716e6d08ee0545a43d595ca15eec3f 7d2f9f376d6f9f82d554efc2a41a1e52a36769a061d02060c5a765d236c573ca
      declare QPC    options=H0 prefix=<instH>  base=99712ec324716e6d08ee0545a43d595ca15eec3f bin=<laneH>  lane_src=4319ab1512e2de6dbc385eb964be182f0af4eb70 <H sha>   <laneH sha256>
      declare QPC-C1 options=H0 prefix=<instC1> base=99712ec324716e6d08ee0545a43d595ca15eec3f bin=<laneC1> lane_src=4319ab1512e2de6dbc385eb964be182f0af4eb70 <C1 sha>  <laneC1 sha256>
      declare QPC-C2 options=H0 prefix=<instC2> base=99712ec324716e6d08ee0545a43d595ca15eec3f bin=<laneC2> lane_src=4319ab1512e2de6dbc385eb964be182f0af4eb70 <C2 sha>  <laneC2 sha256>

  plus tag lines (ignored by the reader) for the SENS-Q grids: `QPC grid <arm> <pin> <grid sha256>` for R, QPC,
  QPC-C1, QPC-C2 (`legs/build_grid.sh`). Arm names avoid the spec arms (`C1`, `C2` are I19 spec arms; a binary arm may
  not reuse a spec name with `options=`). `base` = R's pin, an ancestor of every arm pin (checked by the reader).
* **Store.** The arms' rows go to a QPC store `<a hven-bench checkout at 27c02687>/.scratch/qpc-store (not the census checkout)`. The
  census store is READ-ONLY to this round: at freeze its H0 member directories (every corpus below, every member) are
  copied byte-for-byte into the QPC store (`cp -a`; a sha256 manifest of every copied file is compared against the
  census store's and committed as `cells/H0-copy.sha256`; a mismatch STOPS). The freeze's `baseline_digest` hashes those
  bytes, so the copy is the census by construction. No R ensemble leg is re-run.
* **Ensemble (C539; review B4; the reader's members).** TRAIN-238, CAP-238-1 and CAP-238-01: M+1 = 6 runs per arm
  (member 0 plus `x0` seeds 1-5, the primary kind: x0_j += 2^-48 c_j max(1,|x0_j|), splitmix64 by (problem, seed),
  clipped into the box, applied by the lane binary through the label keys `x0_perturb_rel`/`x0_perturb_seed`; hven
  untouched). NLP-46: its OWN ensemble of 3 runs (member 0, x1, x2; `votes_needed(3) = 3`; never pooled with TRAIN).
  The secondary kind `tr` (trust_region_initial +- k ulp) is read as a REPORTED robustness column only (the census
  carries `H0@t1..5`; each arm runs `@t1..5` on TRAIN-238 only). Member s of R and member s of an arm receive the SAME
  perturbation (paired by seed). The perturbations are inert by construction; their only effect is to sample the outcome
  distribution.
* **Cell classes, frozen before any arm row:** `ens.py freeze --store <qpc-store> --arm H0 --corpora
  TRAIN-238,CAP-238-1,CAP-238-01 --m 5 --kind x0 --out cells/FREEZE-TRAIN.json` and `ens.py freeze ... --corpora
  NLP-46 --out cells/FREEZE-NLP46.json` (STABLE / COST-UNSTABLE / STATUS-UNSTABLE / NOT-REACHED; floors from the C(n,2)
  baseline pairs; `p_base`; the freeze requires the baseline certificate leg: `i19_run.py certs --store <qpc-store>
  --binaries BINARIES.txt` over the H0 members first). Both freeze files are committed before the first arm row.
* **Reads:** `ens.py read --store <qpc-store> --base H0 --arm <QPC|QPC-C1|QPC-C2> --freeze cells/FREEZE-TRAIN.json
  --binaries BINARIES.txt --json gate-QPC/read-<arm>-TRAIN.json` and the same with FREEZE-NLP46; power: `ens.py power
  --store <qpc-store> --arm H0 --freeze ... --event "<counter>>0"` is NOT applicable to the QPC counters (the baseline
  has no such column: the reader STOPS on an event column absent from every record), so P-POWER is counted from the
  arms' member-0 side files against the frozen classes (section 4).
* Runs: `i19_run.py arm <arm>@x<s> --store <qpc-store> --binary <lane> --hven-sha <pin> --hven-prefix <inst>
  --binaries BINARIES.txt` per member, then `i19_run.py certs --store <qpc-store> --binaries BINARIES.txt` (each arm
  re-run with its own `bin=`).

## 3. Corpora and families (all TRAIN; HOLDOUT is never run or read -- the runner refuses it)

| corpus | cells per member | terms | families |
|---|---|---|---|
| TRAIN-238 | DN-3 roster's 119 hven-sqp problems x {default, sqp-qn} | I19-2a section 4 terms (run_serial) | hs-x, hs-q, sd-x, sd-q |
| CAP-238-1 | the same 238 cells with `caller_step_limit=1` | as TRAIN-238 (harness spec) | cap1-x, cap1-q |
| CAP-238-01 | the same 238 cells with `caller_step_limit=0.1` | as TRAIN-238 | cap01-x, cap01-q |
| NLP-46 | cutest-nlp-35's 23 TRAIN problems x {default, sqp-qn} | TU-2 terms; the census's tiering (10 declared solo problems exclusive, the rest co-run); 3 members, its own ensemble | nlp-x, nlp-q |
| SENS-Q | the in-repo walk dumps: the 67 `tests/qp/fixtures/e2-d4` dumps (FORMAT.txt excluded) x {refactorize, border}, re-solved at the walk seam by `legs/grid/qp_dump_grid.cpp` (one process per cell; built by `legs/build_grid.sh` against each arm's install, the same source for R and every arm; prints status, every QpCounters field present at R plus the two QPC counters (NA at R), the objective as hexfloat, an x hash, the data-half recomputation and `qp_dump_kkt`) | the dump's own options and minor cap; 900 s kill per cell; deterministic; single member | sens-r, sens-b |
| PILOT-62 | RF-1 pilot cells | RF-1c leg P terms | certificates only (V2) |
| CENSUS-57 | the walk census against `bench/baselines/2026-10-m7-close-corpus/` (CEN-R3) | census terms; deadline-status cells solo | cen (guard + declared movers) |
| REPLAY-27 | corpus replay walk/ssn/ipm (`u0-replay-cells.txt`) | base x1, head x3 | neutrality + movers |
| FIX | section 7 | ctest | mechanism |

"Strict-optimal" = SQP status `optimal`; "solved" is read on strict-optimal AND on optimal+acceptable. A cell an arm
REACHES ends with a solver status (not a harness error). Cell lists are the harness spec's (`tu/i19_spec.json` at
`27c02687`, the census's); their identity across R and the arms is checked by the reader's per-cell row terms
(`set_digest`, `set_version`, budgets, lane). TRAIN-K (the never-default sparse arm of the earlier draft) is DROPPED: it
is not a harness corpus (its label is not in the spec), the census has no R rows for it, and adding it would be a spec
change after the census; the CAP corpora, which the census carries, replace it as guard corpora. SENS-Q's cell list is
the fixture directory's file list at H, committed as `cells/sens_q_cells.txt` with its sha256 in `cells/SHA256SUMS`.

## 4. Gates read before the arm ensembles (rules fixed here; VALUES recorded in a gate file, never in this file)

**Freeze order (pre-freeze review M4; settler C567).** (1) This declaration is frozen and committed with every bar,
leg script and cell list it names (`cells/sens_q_cells.txt`); nothing in it is filled afterwards. (2) Leg E-R: the census
H0 member directories are copied into the QPC store (`cells/H0-copy.sha256`), the baseline certificate leg runs, both
`ens.py freeze` files are written and committed (P-CLASS). Precondition: the census NLP-46 tier is complete. (3) Only
then are the arms' member-0 rows read (leg N2, then P-POWER and P-GATED), and the P readings are recorded in
`gate-QPC/P-GATES.md` (committed before any E-A row) -- so no arm row is seen before R's classes are frozen, and no
declaration placeholder is filled from a row.

* **P-POWER (review B4.9).** From each arm's member-0 development run (QPC, QPC-C1, QPC-C2; one lane, co-run) against
  the FROZEN R classes: the number of R-STABLE cells strict-optimal in every R member on which the arm's member-0 side
  file shows `converged_refinement_arms > 0` (C1 exercised) and `certificate_gate_refusals > 0` (C2 exercised), per
  family and per corpus; and the same on SENS-Q and CENSUS-57. (`ens.py power` cannot count these: its event column must
  exist in the BASELINE rows, and R has no QPC counters; `legs/qpc_ens_extra.py power` counts them from the frozen
  classes and the arm side files, with the reader's loaders.) These member-0 rows are development readings and are NOT
  reused as ensemble rows: the ensemble legs run every member afresh.
  **Rule:** a component with fewer than 10 R-STABLE solved exercised cells in the live corpora (TRAIN-238 + CAP + NLP-46) is
  declared **corpus-EXPLORATORY**: no cost or solved GAIN may be claimed for it from those corpora; its ship case rests
  on the no-harm conditions (5.2 S1, S3, R1, V-QP, V-HONEST, V2) plus its fixtures (A-conditions) and SENS-Q. The
  recorded counts go to `gate-QPC/P-GATES.md`, never here. (The P-1a deciders' priors -- C1 fired on 0 of 238 TRAIN rows;
  C2 refused 194 QP exits in 21 TRAIN rows -- were read for the UNSCALED gate prototype and do not describe this gate.)
* **P-GATED.** SENS-Q cells gated in = cells where R shows any of: `dual_regularization_decreases > 0`, a kOptimal
  exit whose data-half reading fails, `optimal_row_gray_exits > 0`, or an exit the H data half refuses. Frozen as
  `cells/sens_q_gated.tsv` from R and H member-0 grid readings, recorded with P-POWER in the gate file before any
  ensemble leg.
* **P-CLASS.** The R ensemble (the C547 census rows, copied into the QPC store, section 2) read first by `ens.py freeze`;
  `cells/FREEZE-TRAIN.json`, `cells/FREEZE-NLP46.json` (classes, floors, p_base, digests) committed before any arm
  ensemble row.

**M1's development reading (pre-freeze review M1; settler C567; recorded here because it is a pre-freeze reading of
the GATE, not of an arm's outcome).** At H (development build, Release), every certified QP exit whose UNSCALED product
exceeds 100 feas_tol, on SENS-Q (the grid, `cmp_sensq.py m1`) and on TRAIN-238 member 0 (an env-gated, never-committed
diagnostic print at the gate, `legs/` not involved), with its rows reading, the worst term's slack in feas_tol and its
multiplier over its local scale; and every certified exit carrying a priced term with slack past 100 feas_tol
(footprint-level).
  *Row-local gate (962dc1b6e, before C568):* SENS-Q 0 / 0 (both algebras). TRAIN-238 member 0: 14,177 certified / 50
  refused exits; 230 certified with unscaled product > 100 feas_tol over 24 cells (worst slack <= 1 feas_tol in 209,
  1-10 in 18); 5 with a priced term's slack > 100 feas_tol: CRESC4-default x2 (slack 9.4e6 / 6.5e7 feas_tol under
  lambda 8.8e-22 / 5.7e-21, local scale 161: rounding-zero) and HS101-sqp-qn x3 (working inequality I2 471-578 feas_tol
  slack under lambda 100 / 1000, local scale 1.3e5, row-local product 0.44-4.2 feas_tol: FOOTPRINT-LEVEL slack under a
  certified exit) -> settler C568, the slack test (section 1).
  *With the slack test (6f404ee99; diagnostic build bb97de24f, local, never pushed):* SENS-Q: N0 1 mover (PF1)
  0 differences; V-QP 0 failures (the slack check d_pslack_rel included) under R and H, both algebras; V-HONEST 54 / 54,
  0 exceptions; certified exits with slack > 100 feas_tol under a non-rounding-zero multiplier: 0. TRAIN-238 member 0:
  12,698 certified / 51 refused; certified exits with a non-rounding-zero multiplier's slack > 100 feas_tol: **0**;
  slack > 100 feas_tol under any nonzero multiplier: 2, both CRESC4's rounding-zero rows above (exempt, pslack 0).
  Refusals by cell are unchanged but for HS101-sqp-qn (0 -> 1: its first footprint-slack exit, I2 578 feas_tol under
  lambda 100); the only cell whose outcome moved is HS101-sqp-qn (section 7).
  *hs025 / hs033 (both options) fail --assert-reference under H: PRE-EXISTING, not a QPC effect (settler C568).* R
  (census TRAIN-238 H0 member 0, lane 7d2f9f37) and H end at BYTE-IDENTICAL point dumps (hven-bench --dump-point) with
  identical status, iterations and objective in all four cells. Independent reading (exact rationals for HS33, 60-digit
  decimal for HS25, from the published formulas): hs025's end point is the start (100, 12.5, 3), f 32.834999999663587,
  projected gradient 2.0e-8 (6.1e-10 relative to |f|) -- the battery's documented trap (every exponential <= 1.5e-10 at
  the start); its reference f* = 0 at (50, 25, 1.5) is exact (f 1.5e-118 in 60 digits). hs033's end point is (0, 0, 2),
  a first-order KKT point (lambda_2 = 0.25, z = (11, 0), stationarity 0, complementarity 2.6e-11 / 4.4e-16) with the x2
  bound weakly active and d'H_L d = -0.5 along d = (0, 1, 0) in the critical cone -- a saddle, not a minimizer; its
  reference (0, sqrt2, sqrt2), f -4.5858, is a KKT point (lambda = 0.1768 on both rows, z1 = 11). Both are
  local-point / reference issues of the engine at R, recorded, not caused by this round.
## 5. The ship rule (fixed here, before any row)

### 5.1 Currencies and cell rule
**LA** = `factorizations` and **EV** = eval_f + eval_jac: the reader's two currencies (C539; `ens.py`), so the scored
numbers are the instrument's own. **QM** = `qp_minor_iters` (the QP-side work this round touches) is REPORTED beside them
per arm (the median over seeds of the pooled geomean of (arm+1)/(R+1) on same-solution cells, by
`legs/qpc_ens_extra.py qm`), never a ship bar: the reader has no QM currency and a second instrument
would be a second ship rule. **Refine-step work is REPORTED (pre-freeze review S7):** C1's extra mu-free steps and C2's
re-read cost no factorization and no evaluation, so LA and EV cannot see them; per arm, the summed deltas against R of
`eqp_refine_steps`, `border_refine_steps`, `eqp_refine_iters` and `verdict_refine_steps` on SENS-Q (the grid) and on the
same-solution cells of the member-0 side files (`legs/qpc_ens_extra.py qm`) are reported beside QM, never a bar. Guards, never currencies: majors, restoration entries, `qp_numerical_exits`,
`eqp_residual_refusals`. Cell rule: a WIN in a
currency iff `10*lower <= 9*higher` (integers); LOSS symmetrically; else TIE. **Same-solution condition (B4.8):** a
cell counts toward cost only if |f_H - f_R| <= 1e-6 max(1, |f_R|) in that member; otherwise it is scored "different
solution: better / worse objective" in its own column (never a cost win or loss). Wall-clock is never read.

### 5.2 Ship conditions for an arm A in {QPC, QPC-C1, QPC-C2} (all must hold; scored per seed j, R_j against A_j)

Each arm is scored against R ALONE by the same conditions (no arm is scored against another). Below, "H" means the arm
being scored. The reader's printed `Pass` line is NOT this verdict (it requires a pooled "+", a net-win claim this round
does not make, S3); the conditions are read from the reader's JSON (`--json`) fields named with each condition.
* **S1 Solved (no robust loss).** p_cell = fraction of members strict-optimal (and separately optimal+acceptable).
  A **robust loss** is p_R >= 5/6 and p_H <= 3/6 (NLP-46: 3/3 and <= 1/3, the reader's): NONE is allowed, in any
  family, on either reading (reader: `robust_losses`; the optimal+acceptable reading by `legs/qpc_ens_extra.py qm`). Per family
  sum p(H) >= sum p(R) - 0.5 on both readings. Robust gains are reported symmetrically, beside the losses. Every
  member-0 lost cell is listed, re-run ALONE under the serial rule, and diagnosed (V1-cells), whatever its class.
  The floor never excuses a robust loss (the no-loss threshold is 0 robust losses, stated relative to the ensemble:
  a loss inside the R ensemble's own spread is not robust by definition).
* **S2 Cost (P-1a Q2: within ~5%).** On cells strict-optimal in both arms of a seed and same-solution: the pooled
  geomean of (H+1)/(R+1) (one term per problem NAME, the reader's), median over the seeds, <= 1.05 on LA AND on EV, for
  the TRAIN/CAP ensemble and for NLP-46 separately. No family is "-" and the pool is not "-" (the reader's vote:
  T_{j,f} < -floor_f in >= votes_needed(n) seeds; `pooled_minus_vetoes`). QM is reported (5.1).
* **S3 Net-win claims (only if made).** Any claimed cost gain needs T_j > pooled floor in at least 5 of 6 seeds on one
  currency with median geomean <= 0.97; not required for a ship (a correctness-plus-simplifying round, Q2).
* **R1 Tail guard (B2/B4.7, unchanged in form).** Only on cells R solves strict-optimal in >= 5 of 6 members: H's
  median must stay below max(R max + 20, 3 R max) on majors, EV, rejected_steps and reject_funnel_ceiling (the
  reader's tail columns; LA and QM tails are reported). Cells R does not solve are never tail-guarded; GAINED cells are
  listed beside lost cells.
* **V-QP Zero false QP certificates.** Every kOptimal exit on SENS-Q under the arm is re-checked by independent code
  (`legs/cmp_sensq.py vqp` over the grid's readings, all recomputed from QpSolution against the dump's effective box by
  code that shares nothing with `certificate_reading` but its definitions): data-half rows <= 10 feas_tol; the
  ROW-LOCAL product (`d_product_local`, each term over its own columns' dual scale, exactly the gate's definition,
  section 1) <= 100 feas_tol; no nonzero multiplier off the working set (`d_off_working` = 0); `qp_dump_kkt`'s
  stationarity beyond rounding (floor c_i u m_i, u = eps/2) <= 10 opt_tol; signs <= opt_tol; and
  |f - f*| within the pinning test's OWN bound against `legs/pinned_optima.tsv` (PF1, QC8 entry6/qp1/qp7, C11 YORKNET
  qp1: 1e-9 max(1, |f*|); C11 SAWPATH qp1: 0.1 absolute -- its oracle is the value at a point 8e-4 off its rows, and
  `test_qp_rank_repair.cpp` pins that band; the first draft's uniform 1e-9 failed SAWPATH under R and H alike, found by
  running the grid before freeze, review M2). ZERO failures. The NORMWISE free-block stationarity (`d_stationarity`) and
  the UNSCALED product (`d_product`) are REPORTED columns, never verdicts: the gate reads neither (a normwise residual can
  sit beneath double's rounding floor where large terms cancel -- C11 SAWPATH qp1 reads ~502 opt_tol normwise at its
  certified, pinned exit -- and the unscaled product is not invariant to the objective's scaling, section 1). The pinned
  optima are the check that does not depend on the gate's own definitions: at the gate's bounds the admitted objective
  error is about 100 feas_tol * s_j * row_scale_j per row (section 1), larger than the pinned bars, so a pinned cell
  certified inside the gate but off its optimum is a FALSE CERTIFICATE here. (The unregularized-EQP comparison of the
  earlier draft is dropped: it cannot see an inconsistent working set, D-4.) A false certificate under R is reported and
  not a stop (R is what this round fixes); under H it STOPS.
* **V-HONEST No honest certificate refused (pre-freeze review M7; GATES).** On SENS-Q, a cell whose R exit is kOptimal
  AND passes every V-QP check above (an honest R certificate, read by the arm's own stricter definitions) must end kOptimal
  under the arm too, on both algebras, with |f_A - f_R| <= 1e-9 max(1, |f_R|). ZERO exceptions (`legs/cmp_sensq.py
  honest R.csv A.csv`); every exception is listed with both statuses and readings. WHY: C2 can only refuse, so its harm
  is a refused honest point; C1 changes which answer an EQP returns, so its harm is a different (or no) certificate on a
  cell R certified correctly; C1's solo ship case rests on SENS-Q, and without this bar nothing would stop an arm turning
  a correct R certificate into kNumericalError or kIterationLimit there.
* **V2 Zero false NLP certificates** on every row of every leg, every binary, every member (the reader's certificate
  leg, `i19_run.py certs --binaries`, `tu/stationarity.py`; an arm member's false certificate FAILS that arm,
  `V2_no_false_certificates`). One under an R member STOPS the freeze (the reader's rule; a defect outside this round).
* **A1** PF1 (both algebras): kOptimal at q = -3.231904984786688e-03 (rel 1e-12) with no rung
  (`dual_regularization_decreases = 0`, dual_mu = 1e-8 on every pass) and `converged_refinement_arms = 1`.
  **A2** the qp7 false-certificate shape (seam) refused on both algebras. **A3** every in-repo DALLASM fixture's kOptimal
  exit inside the data half (independent recomputation). **A4** qp26 m42 and qp25: unchanged readings (C1 does not
  arm there: deciders D-2T = base).
* **V-STAB (I1; GATES in the no-worsening direction, C553 Q1).** The arm's classes from its OWN ensemble (the same
  class rule the freeze applies to R, computed by `legs/qpc_ens_extra.py vstab` with the reader's `load_ensemble` + `classes_of`). Per
  corpus (TRAIN-238, CAP-238-1, CAP-238-01, NLP-46): the number of distinct PROBLEM NAMES with at least one
  STATUS-UNSTABLE cell under the arm must be <= R's number + 2, else the arm does not ship; AND (pre-freeze review S6, the
  guard against the perverse direction -- a corpus can leave STATUS-UNSTABLE by failing everywhere) the number of distinct
  PROBLEM NAMES with at least one STABLE cell strict-optimal in every member must be >= R's number - 2, else the arm does
  not ship. Everything else -- the class
  counts per family and pooled, the R-class x arm-class transition table, COST-UNSTABLE -- is reported. Directional
  expectation (stated): C2 converts false-kOptimal QP exits into kNumericalError, which routes the SQP through its
  failure ladder, so STATUS-UNSTABLE may rise on cells with `certificate_gate_refusals > 0`.
* **V-PDIV (REPORTED, never a bar; settler C561).** The perturbation-divergence diagnosis
  (`.superpowers/pdiv-diagnosis-2026-11-25.md`) found that SQP outcome flips start in the QP walk, 9 of its 64 first
  divergences being QP status flips (optimal <-> numerical_error / max_iter / escaped) at exactly the exits QPC touches.
  The pdiv harness is re-run on its 26-cell sample (H0, P, X1, X2 per cell: `.scratch/runs/tasks1.tsv` +
  `tasks2.tsv` of the pdiv worktree) for R and for QPC, and reported: the first-divergence class histogram at row level
  and at walk level (`cmp.py`, `qpflip.py`, `harm.py` of the pdiv worktree's `.scratch/tools/`), in particular the count
  of QP-status flips at first divergence, and the harmful/benign table. How the diagnostic logging reaches H: the three
  env-gated DIAGNOSTIC-ONLY commits of branch `pdiv` (`a70c79e34`, `1b3b5e06d`, `c3b4b8f1d`, on `99712ec32`) are
  cherry-picked onto a LOCAL branch `qpc-pdiv` made from the H commit -- never onto `qpc`, never pushed; R's side is the
  `pdiv` branch itself (already 99712ec32 + those commits). Both diagnostic builds must first reproduce their
  non-diagnostic counterparts' H0 rows on the sample (status, majors, factorizations, eval_f, eval_jac, objective: the
  pdiv study's own reproduction control) or the endpoint is reported VOID. Both run under the pdiv harness terms
  (`runcells.py`, one process per pinned physical core of one lane, MKL_NUM_THREADS=1; counters and statuses only).
  A cherry-pick conflict with C1/C2 is resolved in the diagnostic branch only and recorded in `gate-QPC/pdiv/`.
* **Neutrality held** (section 6, legs N0-N3). A precondition.

### 5.3 Decision (C553 Q2: either component may ship alone)
Each of QPC, QPC-C1, QPC-C2 is scored against R by 5.2. Fixtures A1-A4 bind the arm that carries the component they
test (A1 and A4 = C1: QPC and QPC-C1; A2 and A3 = C2: QPC and QPC-C2). The decision, in order:
1. QPC passes -> QPC ships (both components).
2. Else, if exactly one of QPC-C1, QPC-C2 passes -> that component ships alone (the shipping commit is the revert of
   the other component landed on `qpc`, section 2).
3. Else, if both component arms pass and QPC fails -> nothing ships by this file; the interaction is reported to the
   settler with the three reads (a combination that fails while each part passes is a finding, not a tie to break).
4. Else nothing ships and a successor round follows.
No arm is ranked against another; the order above is fixed now.

## 6. Legs and bars

All legs under `~/.local/bin/box-lane --`; only NLP-46's exclusive tier uses `--exclusive`. Evidence in `gate-QPC/`.
Binaries frozen in `BINARIES.txt` before the first leg; runners sha-check them. Toolchain: clang, the default
`HVEN_FP_MODE=SAFER_FAST`, sequential MKL for bench lanes, no LTO, Linux x86-64; Apple and Windows readings are CI only, and an Apple value not
observed is UNOBSERVED.

| leg | instrument | bar |
|---|---|---|
| B | builds (`legs/legB.sh`): for each arm QPC, QPC-C1, QPC-C2: a detached hven worktree at the arm sha, `scripts/census_build.sh` of hven-bench `ens` `27c02687` (install with `HVEN_PIN.txt` + lane binary + `.lane-src`), and `legs/build_grid.sh` against the install; the R grid against the census install; BINARIES.txt's `declare` and grid tag lines written from the printed shas | rc 0 each; every install's `HVEN_CMAKE_ARGS`, `HVEN_EIGEN_ALIGN`, `HVEN_CXX_COMPILER` equal the census install's; every lane `.lane-src` = `4319ab15...` |
| C | FULL ctest Release and Debug at H (`legs/legC.sh`; the tree's default `HVEN_FP_MODE=SAFER_FAST`, clang) | 100% both; counts reported; every `QpCertificateData.*`, `QpReleaseReadmission.*`, `EqpConvergedRefinement.*` green |
| N0 | SENS-Q, R grid vs each arm grid, both algebras (`legs/legN0.sh`, `legs/cmp_sensq.py n0`) | every cell whose arm row has `converged_refinement_arms = 0` AND `certificate_gate_refusals = 0` is IDENTICAL to R on every other column (status, every counter, objective hexfloat, x hash, the independent readings); any other difference STOPS; movers listed |
| N1 | REPLAY-27 R vs H (`legs/legN1.sh`; `compare_replay.py` byte-exact; the H binary's QPC SIDE FILE per mode, `HVEN_BENCH_QPC_SIDE`, pre-freeze review S10) | head repeats byte-identical and their side files identical; a base-vs-head differing cell is a DECLARED MOVER only if its OWN side line carries a non-zero QPC counter, else STOP (the replay CSV itself carries no QPC counter, so the comparison against the base binary stays byte-exact) |
| N2 | each arm's member 0 vs the census H0 rows on TRAIN-238, CAP-238-1, CAP-238-01, NLP-46 (`legs/n2_qpc.py`, built on the reader's `cell_table` / `compare_arm_rows`) | every cell whose arm side row shows both QPC counters 0 is identical to H0 on every row column except `options_digest`, `wall_s` and on every side column present in both (counters/statuses exact; measure columns by the schema's near-ulp gate); the only arm-only columns are the two QPC counters; any other difference STOPS; movers listed with counters |
| N3 | CENSUS-57: H vs the baseline (CEN-R3) | 0 differences on cells with both counters 0; movers listed with counters; no cell loses a status |
| M | mutations (section 8), control first (`legs/mutations_qpc.sh`, `legs/mutate_qpc.py`); the runner's ARGUED set is parsed from section 8 of this file and must EQUAL the script's (parity check, else `VERDICT ARGUED_PARITY_FAIL`) | control clean; every must-kill killed: `VERDICT MUTATIONS_AS_DECLARED` |
| E-R | R ensemble = the census rows (hven-bench `ens` `27c02687`, the C559 restart, not re-run), copied byte-for-byte into the QPC store; baseline certificate leg; `ens.py freeze` x2 | freeze files committed (P-CLASS); identity checks of the reader pass |
| E-A | each arm's ensemble (TRAIN-238, CAP-238-1, CAP-238-01: x0 members 1-5 + member 0; NLP-46: x1, x2 + member 0; TRAIN-238 `@t1..5` reported), then `i19_run.py certs --binaries BINARIES.txt`, then `ens.py read` per arm and ensemble | 5.2 inputs (S1, S2, R1, V2 from the reader JSON; V-STAB from the arm's own classes) |
| Q | SENS-Q verdicts on N0's CSVs (`legs/legQ.sh`, `cmp_sensq.py vqp`, `legs/pinned_optima.tsv`) | V-QP zero under each arm; A-conditions; R's failures reported |
| P | PILOT-62 under each arm (6 members; the census carries R's) through the certificate leg | V2 |
| D | V-PDIV (reported): local `qpc-pdiv` (H + the three pdiv diagnostic commits) and `pdiv` builds, the 26-cell sample x {H0, P, X1, X2} through `runcells.py`, then `cmp.py` / `qpflip.py` / `harm.py` | reproduction control holds, else VOID; histogram reported |
| X | (reported) cross-read with the I19-2a held levers: not run -- I19-2a shipped nothing | -- |
| Sm | install smoke both front ends (`scripts/check_install_smoke.sh`, gcc and clang) at H | rc 0 |
| CI | `gh workflow run ci.yml --ref qpc`; `gh run watch <id> --interval 120` | four lanes green; off-reference readings recorded |

Co-run protocol (CLAUDE.md section 7), declared in every results file: one process per pinned PHYSICAL core of one box
lane, SMT siblings idle; `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`; counters, statuses and objectives asserted; wall-clock
informational, never quoted. A cap/kill cell, or a deviation that decides a bar, is re-run ALONE under the serial rule
before it is called one.

Order (section 4's freeze order): freeze this file -> B -> C -> N0 -> N1 -> N3 -> M -> E-R (census copy, baseline
certificates, class freeze, commit) -> N2 (the first leg that reads an arm's TRAIN/CAP/NLP-46 member-0 rows) ->
P-POWER / P-GATED (recorded in `gate-QPC/P-GATES.md`, committed) -> E-A (QPC, then QPC-C1, then QPC-C2) -> Q -> P -> D ->
Sm -> CI.

## 7. Fixtures (in-repo; readings at R recorded before the fix commit)

| fixture | R reading (measured, `qpc` worktree, Release) | H reading |
|---|---|---|
| F-PF1 PF1 dump, both algebras (`QpReleaseReadmission.F7PathCorruptedQp1SolvesAtItsOwnRegularization`) | kOptimal 4758 minors, f -0.003231904984786682, 1 rung (refactorize; the test FAILS at R, `dev/R-tdd.txt`) | kOptimal 4756 minors on both algebras (refactorize 4756 factorizations; border 3585), f -0.003231904984786683 / -...682, no rung, arms 1 (development, Release) |
| F-QP7 qp7 shape via the certificate seam, both algebras (`QpCertificateData.Qc8DallasmQp7TheP1aFalseCertificateShapeIsRefused`) | kOptimal on both algebras (row 103, lambda -99759.8, move 7.29e-8): a false certificate; the test FAILS at R | kNumericalError, 1 refusal |
| F-ROW / F-PROD / F-BOUND / F-NAN synthetic seam presentations (`QpCertificateData.*`) | row, product and bound presentations certified kOptimal at R (tests FAIL); the NaN point already refused at R | refused / certified at the declared boundaries |
| F-STAT an off-stationary exported point on the row (`QpCertificateData.AnOffStationaryExportedPointIsRefused`, C553 Q3) | certified at R (the KKT-point test read the walk's own point) | refused past 10 opt_tol, certified inside |
| F-SLACK a working inequality held slack under lambda 999.5 (`QpCertificateData.AWorkingRowSlackPastAHundredFeasTolIsRefused`) | certified at R | refused past 100 feas_tol (scaled), certified inside |
| F-SCALE the product against the dual scale (`QpCertificateData.TheProductIsReadAgainstTheDualScale`) | n/a | a row 5 feas_tol off certifies at c = 1000 and c = 1 alike (the first build refused the c = 1000 one) |
| F-NAT the DALLASM QC8/SDL fixtures, independent recomputation | inside: worst product 41.3 feas_tol (entry6 refactorize), worst rows 1e-3 feas_tol | inside |
| F-CONV converged mode on a near-dependent face, eliminated and unbordered-bordered (`EqpConvergedRefinement.*`) | n/a (new API) | row miss 1.11e-5 (model, 0 / 1 steps) -> 2.89e-7 (converged, 9 / 10 steps), eliminated / unbordered |
| F-RESET the refinement mode does not survive a solve (`QpReleaseReadmission.TheConvergedRefinementIsPerSolve`) | n/a | bit-identical to a fresh engine |
| F-FOLD the SQP fold of the two counters (⟨SQP seam test, if built⟩) | n/a | 1 refusal folded |

Re-derived pins (declared, each with its mechanism): `QpReleaseReadmission.F7PathCorruptedQp1LeavesTheCappedEndpoint`
(asserted the rung; replaced by F-PF1 -- the rung no longer exists); `QpWa1AscentFootprint.SyntheticRide...`'s
`dual_regularization_decreases <= kMaxDualRegDecreases` (the constant is gone; the field is identically 0);
`QpDegeneracyArm.CertificateToleratesABoundViolationInsideAndOnTheStructuralMargin` (re-premised: the bound half still
tolerates both points, the data half refuses them, a point inside both halves certifies); the trace goldens (two
appended keys); `QpCertificateData.AProductPastAHundredFeasTolIsRefused` (replaced by F-SCALE and F-SLACK: on an equality
row the dual-scaled product cannot exceed the rows reading, so the product's boundary is shown on a slack working row);
and the sixteen C553 Q5 movers in the table below, each with its mechanism and verdict.

**The ctest movers (C553 Q5): every one diagnosed before freeze.** The first QPC build's Release ctest (d4e3adca8) read
4243 of 4262: three trace goldens (two appended keys, re-derived in 39cec6044) and sixteen movers, the set the P-1a decider
measured for its gate prototype. Each was diagnosed on a development build carrying temporary env-gated instrumentation
(never committed) that printed the data half's reading at every would-be certified exit and could switch C2 off
(`GATE_OFF`, = the C1-only arm) or restore R's rung (`C1_OFF`, = the C2-only arm): with C2 off every mover passes; with
C1 off every mover still fails; with both off the tree reads R. **All sixteen are C2 consequences; none is C1's.** The
diagnosis found TWO DEFECTS IN C2 AS FIRST BUILT, both fixed (section 1): the residual x multiplier product was read
unscaled (not invariant to the objective's scaling; it refused rounding-level elastic-rung points), and the Q3 stationarity
disjunct as first written read the normwise residual (it refused a point whose residual is beneath double precision's
rounding floor). **Re-confirmed on the component-arm builds of section 2** (development, the movers' test binaries
built in each arm's own tree, Release): on `qpc-c1` (C1 only, `e4660dd31`) exactly the C2-dependent re-pins fail -- the
re-premised movers and the `QpCertificateData.*` fixtures -- and nothing else; on `qpc-c2` (C2 only, `1ed77a336`) only
`QpReleaseReadmission.F7PathCorruptedQp1SolvesAtItsOwnRegularization` and `.TheConvergedRefinementIsPerSolve` (C1's own
fixtures) fail, and every mover's new pin holds. Every mover is C2's; C1 moves none.

| test | mechanism (readings from the data half at the refused exit) | verdict |
|---|---|---|
| B1Gate.TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers | R's major-0 QP certified x = 0 with lambda_i = 1e6 on a working row 8e-7 SLACK (scaled product 8e-7 = 800 feas_tol): a KKT point of the shifted row, not of the QP. Refused -> the QPs then certify TR-limited steps; the watchdog still restores the refused triple at major 5 (the fixture's subject); the solve crosses to x* and certifies at major 13. **Independent check from the model's data (exact rational arithmetic, not the solver's residuals):** x = 7.9999999999886589e-07 (x* = 8e-7, diff -1.1e-18), f = -0.79999999999854587 (f* = -0.79999999999968, diff 1.1e-12), lambda = 999999.9999992; |grad L| = 6.1e-12 (6e-18 relative), feasibility 0, lambda c = 1.1e-12, lambda >= 0, box slack. A CORRECT certificate of the true optimum, not of the refused triple. | correct new behaviour (the test's own note described R's QP as the false certificate D2 names); re-premised with the independent KKT check in-test |
| QpWa1AscentFootprint.SyntheticRidePassedOverOutOfTheBudgetIsRefused | final point 1.92e-8 = 19.2 feas_tol off a working row (inside the mu = 1e-10 rung's footprint excuse); scaled product 19.2 feas_tol | correct new behaviour: kNumericalError, 1 refusal; the ride-refusal assertion kept |
| QpStagnationStop.AResidualContractingAtPointEightCertifiesWithoutAReading (renamed ...TakesNoReading) | the KKT-point test passes once |a lambda| < opt_tol, and with a = 1e-6 that happens with the row still 7.3e-7 (731 feas_tol) off | correct new behaviour: kNumericalError, 1 refusal; the stagnation rule's silence (the fixture's subject) unchanged |
| QpDegeneracyArm.CertificateToleratesABoundViolationInsideAndOnTheStructuralMargin | bound violations of 250-10,000 feas_tol inside the bound half's margin | by design (re-premised in 39cec6044: the bound half still tolerates, the data half refuses, a point inside both certifies) |
| HsSweepCorpus.Corpus: cold-arm status failures (HS15, every KD/SOC cell, p = 1.25) | HS15's cold elastic rung at rho 2e8: rows 9.1e-14, unscaled product 9.1e-6 -- a rounding-level point refused by the UNSCALED product | **DEFECT FIXED** (dual-scaled product: 4.6e-14); the cold arm is clean again, no pin moved |
| SqpElasticWitnessStart.YorknetMajor0LadderIsUsableFromTheShippedStartAndTheWitness | ladder rungs: rows 5.2e-16 / 3.9e-14, unscaled product 3.0e-7 / 2.9e-7, dual scale 1.6e11 / 1.6e10 | **DEFECT FIXED** (scaled 1.9e-18 / 1.8e-17); passes unchanged |
| JsonLinesTraceSink.SqpMajorReproducesTheHistoryOnTheRowThatSeedsRestoration | the circle-line elastic QP: rows 1.2e-11, unscaled product 3.0e-7, dual scale 4.7e5 | **DEFECT FIXED** (scaled 6.4e-13); passes unchanged |
| SqpElasticWitnessStart.SawpathMajor0RungSolvesFromItsWitness, ...SawpathMajor0LadderNeedsTheLastOptimalRung (NEW movers of the first Q3 draft, not of the sixteen) | normwise free-block stationarity 5.0e-7 relative (502 opt_tol) on a rung whose free component sums terms of 2.4e13 to 3.6e2 (rounding floor ~5e-3 absolute; the test's own note) | **DEFECT FIXED** (stationarity read beyond rounding, qp_dump_kkt's measure); pass unchanged |
| HsSweepCorpus.Corpus: HS15 lever neutrality, watchdog, warm on/on counts, total minors; HS10 warm counts | HS15's warm failing chain at p = 1 (the LICQ point): working rows of the near-degenerate constraint held SLACK under a multiplier (slack 1.8e-8 under 896, 2.2e-7 under 1462; scaled 680 and 13,100 feas_tol); HS10's warm chain: a working row 0.29 SLACK under lambda 0.41 (scaled 2.4e-2) -- none is a KKT point of its QP | correct new behaviour; declared moves: HS15 warm on/on 22/40 -> 21/49 (full-step majors / majors, reference build), HS15 lever-ON chain takes one watchdog restore (on 49/175/74/13 vs off 44/160/68/12), HS15 exempted from count neutrality (statuses, stop point and answers stay lever-neutral, asserted), HS10 warm 28/42 -> 26/40 (Release; = the Debug column), corpus minors 3406 -> 3434 (Release); every status and answer unchanged |
| HsSweepCorpus.Hs15WarmKdOffWithTheFailedSubproblemStep | the same chain | declared move: held-off 40 -> 44 majors; step arm 43/149/64/6 -> 42/142/58/4 (reference build); failure count, stop point and the rescued branch unchanged |
| SqpSolverRestorationSeed ...SeedOfferIsNotGated...DenseArm, ...ExhaustedLadderSite...DenseArm, ...AJacobianPoisonedCandidate...DenseArm | the dense arm's take rode a QP certified 477 feas_tol off its equality rows (the circle-line linearization is inconsistent; dual_mu's footprint held the rows), scaled product 909 feas_tol | correct new behaviour; declared moves: the dense arm now reads the kNever arm's route (no take in any mode or config -- Debug's ssn/ipm takes went too; the ladder's exhaustion; no k reaches the Jacobian screen -- that demonstration is lost on this model, stated; the screen's safety half and mutant M7's kill stay); the certified infeasibility answer unchanged |
| SqpSolverRestorationSeed.TheSeedsEvaluationIsCountedExactlyOnce{,DenseArm,UnderTheDefaultPlusProbe} | the same refusal removes the take from (2, 2) on both arms; a 48-start scan shows the take on both arms at (3, 1.5) in Release with no refusal, but in Debug the route meets the refused certificate at 47 of 48 starts | declared moves: FIXTURE START (2, 2) -> (3, 1.5); the take and its counts (evals_full 28 -> 30, evals_values 36/37 -> 21; probe arm +1) asserted in RELEASE only, Debug records the route; the partition (the subject) asserted in both. **DECLARED LOST COVERAGE (pre-freeze review S3):** the in-place upgrade is exercised by the Release build alone (the 48-start scan found no Debug start that takes without meeting a refused certificate) |
| SqpSolverAdaptiveMu.MuScheduleIsQuantizedAndMonotoneDenseArm (Debug only; a mover the first build's Release-only ctest could not see) | Debug's dense route met a QP certified 49 feas_tol off its equality row (ScaledRowModel, a = 1e-3; QP feas_tol 1e-9) | correct new behaviour; declared move: the dense arm's Debug decade drop is gone (mu held, as Release and kNever). **DECLARED LOST COVERAGE (pre-freeze review S5):** no arm of this test drops mu any more, so its "the loop refactorizes on a mu change" claim is vacuous on every build; the claim is not exercised by this fixture after QPC |
| TRAIN-238 HS101 sqp-qn (development reading, member 0; the slack test's mover, settler C568) | the first footprint-slack exit (working inequality I2 578 feas_tol slack under lambda 100, local scale 1.3e5, row-local product 0.44 feas_tol) is refused by the slack test | correct new behaviour, and the downstream effect is a gain: R (census member 0) and the row-local H ended optimal at f 1809.7647657932057 in 270 majors / 11,826 factorizations / 278 f-evals; with the slack test optimal at the BIT-IDENTICAL f in 63 majors / 123 factorizations / 64 f-evals, stationarity 2.3e-7 (was 8.7e-7). Registered for QPD (C568): HS101-I2 is a walk-quality case -- the honest remedy is to finish the walk, not certify it |
| SqpSolverRestorationSeed.AJacobianPoisonedSeedCandidateReachesTheScreenAndIsRefused (NEW, review M3) | AJacobianPoisonedCandidate... no longer reaches the Jacobian screen at H (the h guard refuses first), so mutant M7 survived both arms; the separated restoration model's radius-floor candidate is TAKEN by the clean guard on every build, and poisoning the candidate's in-place derivative upgrade (the first sub-start Jacobian fetch below h(start)) makes the screen the only refuser | the screen is reached and refuses (seeded 0, the phase runs from the requesting iterate, no non-finite subproblem, kInfeasible) on both dense policies; Q41 (= M7) killed by it |
| Threads.ZeroLeavesEveryFactorAtTheBackendDefault, Threads.TheCallersThreadSettingSurvivesEveryTierAndTheRestoration | the restoration leg's last walk QP is the same 477-feas_tol refusal, so no hot handle is emitted (the handle exists only after a kOptimal last walk solve) | correct new behaviour; the reference build now reads what Debug and Accelerate already did. **Pre-freeze review S4:** on the reference route (Release and Debug) the absence of the handle and the refusal are ASSERTED, so a route that flips back to a certified last solve fails the test (and the K0 read can be reinstated); off the reference route the K0 count is read when a handle exists. **DECLARED LOST COVERAGE:** the restoration route's parent K0 read is observed by no build on this model; the walk tier's K0 read on HS76 is unchanged |

Development leg-C readings at the post-diagnosis tree (the code commit below; development, NOT evidence): Release 4264 of 4264, Debug 4264 of 4264 (1 skipped and 2 disabled by design in both).

## 8. Mutations (must-kill unless marked ARGUED)

Applied by `legs/mutate_qpc.py <id>` on the Release tree (anchors pre-checked), run by `legs/mutations_qpc.sh` on
`hven_qp_seam_tests` (`QpCertificateData.*:QpReleaseReadmission.*:QpQc8ImpliedBlocker.*:QpCertificateSign.*`),
`hven_qp_tests` (`EqpSolve*:EqpConvergedRefinement.*`), `hven_sqp_tests`
(`B1Gate.TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers:SqpElasticWitnessStart.Sawpath*:`
`SqpSolverRestorationSeed.AJacobianPoisonedSeedCandidateReachesTheScreenAndIsRefused`) and
`hven_sqp_seam_tests` (`SqpQpcFold.*`, the fold fixtures). The runner checks, before any build, that the table's ids equal
the script's (`ID_PARITY_FAIL`) and that the ARGUED set below equals its own (`ARGUED_PARITY_FAIL`); after the run, that
every killer named below for a must-kill id is among that id's observed killers (`KILLER_PARITY_FAIL`; `Suite.*` = any
test of the suite). Killers are named from the development dry run at 22cc157bd (2026-11-25, Release, worktree qpc-mut: control clean, every must-kill id killed, Q17 survived as argued; logs `qpc-mut/.scratch/mut-dry/`; the run's KILLER_PARITY_FAIL was the then-empty killer column, and the runner's parity block re-run on its killers against this table reads 40 checked, 0 failures); leg M is the evidence.

| id | mutation | declared killer(s) |
|---|---|---|
| Q1 | the data half never refuses | `B1Gate.TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers`, `QpCertificateData.Qc8DallasmQp7TheP1aFalseCertificateShapeIsRefused`, `QpCertificateData.ADecoupledLargeDualCannotHideASlackWorkingRow`, `QpCertificateData.AWorkingRowSlackUnderASmallMultiplierIsRefused` (+9 more in the dry run) |
| Q2 | the data half reads rows only | `B1Gate.TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers`, `QpCertificateData.ADecoupledLargeDualCannotHideASlackWorkingRow`, `QpCertificateData.AWorkingRowSlackUnderASmallMultiplierIsRefused`, `QpCertificateData.AMultiplierOffTheWorkingSetIsRefused` (+7 more in the dry run) |
| Q3 | the data half reads the product only | `QpCertificateData.Qc8DallasmQp7TheP1aFalseCertificateShapeIsRefused`, `QpCertificateData.AWorkingRowSlackUnderASmallMultiplierIsRefused`, `QpCertificateData.TheProductIsReadAgainstTheRowsLocalScale`, `QpCertificateData.AMultiplierOffTheWorkingSetIsRefused` (+6 more in the dry run) |
| Q4 | row factor 10 -> 100 | `QpCertificateData.ABoundInsideTheFootprintExcuseButPastTheDataHalfIsRefused`, `QpCertificateData.Qc8DallasmQp7TheP1aFalseCertificateShapeIsRefused`, `QpCertificateData.TheProductIsReadAgainstTheRowsLocalScale`, `QpCertificateData.TheVerdictRefusesNanAndReadsEveryBoundInclusively` |
| Q5 | product factor 100 -> 1000 | `QpCertificateData.TheVerdictRefusesNanAndReadsEveryBoundInclusively` |
| Q6 | a NaN passes (comparisons written !(>)) | `QpCertificateData.TheReadingKeepsANanInTheProblemData`, `QpCertificateData.TheVerdictRefusesNanAndReadsEveryBoundInclusively` |
| Q7 | a refusal exits kInfeasible | `B1Gate.TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers`, `QpCertificateData.Qc8DallasmQp7TheP1aFalseCertificateShapeIsRefused`, `QpCertificateData.ADecoupledLargeDualCannotHideASlackWorkingRow`, `QpCertificateData.AWorkingRowSlackUnderASmallMultiplierIsRefused` (+9 more in the dry run) |
| Q8 | a refusal is not counted | `B1Gate.TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers`, `QpCertificateData.Qc8DallasmQp7TheP1aFalseCertificateShapeIsRefused`, `QpCertificateData.ADecoupledLargeDualCannotHideASlackWorkingRow`, `QpCertificateData.AWorkingRowSlackUnderASmallMultiplierIsRefused` (+14 more in the dry run) |
| Q9 | the data half ignores the bounds' rows reading | `QpCertificateData.ABoundInsideTheFootprintExcuseButPastTheDataHalfIsRefused`, `QpCertificateData.TheReadingKeepsANanInTheProblemData` |
| Q10 | the re-admission never arms | `QpReleaseReadmission.F7PathCorruptedQp1SolvesAtItsOwnRegularization`, `QpReleaseReadmission.TheConvergedRefinementIsPerSolve` |
| Q11 | the rung kept beside the arming | `QpReleaseReadmission.F7PathCorruptedQp1SolvesAtItsOwnRegularization` |
| Q12 | converged mode ignored on the eliminated path | `EqpConvergedRefinement.EliminatedPathConvergesWhereTheModelStepLeavesTheFootprint`, `EqpConvergedRefinement.UnborderedBorderedPathConvergesLikeTheEliminatedOne`, `QpReleaseReadmission.F7PathCorruptedQp1SolvesAtItsOwnRegularization` |
| Q13 | converged mode ignored on the bordered path | `EqpConvergedRefinement.UnborderedBorderedPathConvergesLikeTheEliminatedOne` |
| Q14 | the strict-decrease keep rule dropped (every step kept) | `EqpConvergedRefinement.TheLoopKeepsOnlyStrictlyDecreasingSteps` |
| Q15 | the mode not reset at run() entry | `QpReleaseReadmission.TheConvergedRefinementIsPerSolve` |
| Q16 | armed from pass 0 (global converged refinement, decider D-2) | `QpCertificateSign.*`, `QpQc8ImpliedBlocker.*` |
| Q17 | arms counted more than once | ARGUED: after the first arming the mode is kConverged for the rest of the solve, so re-arming changes nothing but the counter, and on every fixture the converged refinement puts the released row on its bound so no SECOND re-admission occurs (PF1 reads arms = 1 with or without the guard). The counter's "0 or 1 per walk" contract is pinned where it can be observed (PF1 = 1); a fixture with two re-admissions after arming does not exist in the tree |
| Q18 | the fold of the two counters missing at the main-QP site | `B1Gate.TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers`, `SqpQpcFold.MainSubproblemRefusalsAreFoldedOnce` (+3 more in the dry run) |
| Q19 | the converged loop's step budget 10 -> 2 | `EqpConvergedRefinement.EliminatedPathConvergesWhereTheModelStepLeavesTheFootprint`, `EqpConvergedRefinement.UnborderedBorderedPathConvergesLikeTheEliminatedOne` |
| Q20 | the data half drops its stationarity disjunct (C553 Q3) | `QpCertificateData.AnOffStationaryExportedPointIsRefused`, `QpCertificateData.TheVerdictRefusesNanAndReadsEveryBoundInclusively` |
| Q21 | the finiteness guard dropped from the verdict | `QpCertificateData.TheVerdictRefusesNanAndReadsEveryBoundInclusively` |
| Q22 | the product read UNSCALED (the local division dropped; the first QPC build) | `QpCertificateData.ADecoupledLargeDualCannotHideASlackWorkingRow`, `QpCertificateData.AWorkingRowSlackUnderASmallMultiplierIsRefused`, `QpCertificateData.TheReadingDividesEachProductByItsOwnColumnsScale`, `QpCertificateData.TheProductIsReadAgainstTheRowsLocalScale` (+4 more in the dry run) |
| Q23 | the local scale replaced by the GLOBAL one (every column's terms joined; the C553 build) | `QpCertificateData.TheReadingDividesEachProductByItsOwnColumnsScale` |
| Q24 | |z_i| dropped from the column terms of the local scale | `QpCertificateData.TheLocalScaleJoinsTheBoundMultiplierAndTheRowsOwnTerm` |
| Q25 | the row's own join |lambda_j| ||a_j||inf dropped from its local scale | `QpCertificateData.TheLocalScaleJoinsTheBoundMultiplierAndTheRowsOwnTerm`, `QpCertificateData.Qc8DallasmQp7TheP1aFalseCertificateShapeIsRefused`, `QpQc8ImpliedBlocker.*` (+3 more in the dry run) |
| Q26 | stationarity read NORMWISE instead of beyond rounding | `QpCertificateData.TheVerdictRefusesNanAndReadsEveryBoundInclusively`, `SqpElasticWitnessStart.SawpathMajor0LadderNeedsTheLastOptimalRung`, `SqpElasticWitnessStart.SawpathMajor0RungSolvesFromItsWitness` |
| Q27 | the pinned-bound product term dropped | `QpCertificateData.TheLocalScaleJoinsTheBoundMultiplierAndTheRowsOwnTerm` |
| Q28 | the off-working-set reading dropped from the verdict (review S2) | `QpCertificateData.AMultiplierOffTheWorkingSetIsRefused`, `QpCertificateData.TheVerdictRefusesNanAndReadsEveryBoundInclusively` |
| Q29 | the reading's maxima made plain (NaN dropped; review S8) | `QpCertificateData.TheReadingKeepsANanInTheProblemData` |
| Q30 | the rounding floor read with eps instead of u = eps/2 (review S1) | `QpCertificateData.TheRoundingFloorIsTheUnitRoundoff` |
| Q31 | both NaN defences dropped (Q21 + Q29) | `QpCertificateData.ANanExportedMultiplierIsRefusedByTheDataHalf`, `QpCertificateData.TheReadingKeepsANanInTheProblemData`, `QpCertificateData.TheVerdictRefusesNanAndReadsEveryBoundInclusively` |
| Q32 | the fold missing at the elastic-rung site | `SqpQpcFold.ElasticRungRefusalsAreFoldedOnce`, `SqpQpcFold.RestorationPhaseRefusalsAreFoldedOnce` |
| Q33 | the fold missing at the restoration-phase site | `SqpQpcFold.ElasticRungRefusalsAreFoldedOnce`, `SqpQpcFold.RestorationPhaseRefusalsAreFoldedOnce` |
| Q34 | the fold missing at the iteration-limit retry site | `SqpQpcFold.CapRetryRefusalsAreFoldedOnce` |
| Q35 | the fold missing at the SOC re-solve site | `SqpQpcFold.SocResolveRefusalsAreFoldedOnce` |
| Q36 | the slack disjunct dropped from the verdict (settler C568) | `QpCertificateData.APinnedBoundSlackUnderASmallMultiplierIsRefused`, `QpCertificateData.ARoundingZeroMultiplierExemptsItsSlack`, `QpCertificateData.AWorkingRowSlackUnderASmallMultiplierIsRefused`, `QpCertificateData.TheVerdictRefusesNanAndReadsEveryBoundInclusively` |
| Q37 | the rounding-zero exemption dropped (every nonzero multiplier priced) | `QpCertificateData.ARoundingZeroMultiplierExemptsItsSlack` |
| Q38 | the pinned bounds' slack not read | `QpCertificateData.APinnedBoundSlackUnderASmallMultiplierIsRefused`, `QpCertificateData.ARoundingZeroMultiplierExemptsItsSlack` |
| Q39 | the working rows' slack not read | `QpCertificateData.ARoundingZeroMultiplierExemptsItsSlack`, `QpCertificateData.AWorkingRowSlackUnderASmallMultiplierIsRefused` |
| Q40 | slack factor 100 -> 1000 | `QpCertificateData.APinnedBoundSlackUnderASmallMultiplierIsRefused`, `QpCertificateData.AWorkingRowSlackUnderASmallMultiplierIsRefused`, `QpCertificateData.TheVerdictRefusesNanAndReadsEveryBoundInclusively` |
| Q41 | the seed guard's Jacobian screen passes everything (`jacobian_values_finite` returns true; RS-6b-r2 mutant M7; review M3) | `SqpSolverRestorationSeed.AJacobianPoisonedSeedCandidateReachesTheScreenAndIsRefused` |

ARGUED (the runner's set, parity-checked): Q17.

## 9. Stops
A pre-flight mismatch; any N0-N3 difference outside the declared-mover rule; an unreported error row; a false NLP
certificate under R member 0 or any false certificate (QP or NLP) under H; leg B or C failing; leg M not as declared
(including the ARGUED parity); a failed Sm or CI leg; any HOLDOUT read; a binary sha mismatch; a spec/declaration
disagreement. A stop is resolved by a successor declaration, never by an edit.

## 10. Not run
HOLDOUT (never). Wall-clock. IPM lanes beyond REPLAY-27's ipm mode (the change is `detail/qp/walk` plus the SQP counter
folds). Accelerate and Windows (CI only). P-1a's centring arms (deferred; C522).

## 11. Compute budget
R costs nothing new (the census rows). Three arms (QPC, QPC-C1, QPC-C2), each: TRAIN-238 + CAP-238-1 + CAP-238-01 x 6
members (+ TRAIN-238 @t1..5 reported) co-run ~1.5 lane-h; NLP-46 x 3 members: co-run ~3, exclusive tier ~2 (exclusive
hours count double); PILOT-62 x 6 ~1.5; certificate leg ~0.5. Per arm ~9-10 lane-h, so ~28-30 for the three. Plus:
builds (three installs + lanes + four grids) ~1.2; ctest Release + Debug ~1.0; mutations 21 x ~3 min ~1.1; SENS-Q 4 grids
x 2 algebras ~2 (PF1 dominates); census + replay ~1.5; smoke 0.1. About ⟨34-38⟩ lane-hours +-30%; elapsed 3-5 days,
dominated by NLP-46's exclusive tier. All three arms are run whatever QPC's read shows: the component reads are
reported even under case 1 of 5.3, so the attribution of every movement is on the record.

## Open questions for the settler (before freeze)

Answered by C553 and folded in: Q1 (V-STAB gates, no-worsening, 5.2), Q2 (component arms, 2 and 5.3), Q3 (stationarity
in the data half, 1 and 8), Q4 (one combined gate accepted), Q5 (every ctest mover diagnosed, section 7).

* **Q6** The S2 currencies are now the reader's LA and EV (the draft had QM and LA; QM is reported). Accept, or should QM
  stay a bar through a second instrument?
* **Q7** TRAIN-K (never-default) is dropped as not a harness corpus (section 3); CAP-238-1/CAP-238-01 take its guard
  role. Accept, or add it to the spec (a spec change after the census, needing an R supplementary leg)?
* (Q8, answered by C566 and superseded by review S10: N1 now attributes a replay difference by the cell's own side-file
  counters.)
