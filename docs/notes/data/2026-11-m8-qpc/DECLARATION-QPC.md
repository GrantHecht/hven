# DECLARATION-QPC -- the SQP walk QP's correctness round: converged refinement at the release re-admission, and the certificate's data half

> **STATUS: UNFROZEN DRAFT (2026-11-24).** Not a pre-registration yet. Values marked ⟨...⟩ are filled before freeze
> from development readings or the harness, never from a round row. `TODO-HARNESS` marks script names the hven-bench
> `ens` branch (built separately) will supply. A Fable review comes before freeze. No leg of this file has run.

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
  gray-row returns, `detail::terminal_kkt` is read on the point and multipliers about to be exported, from the QP's
  own data. Certified iff
  `rows <= 10 * feas_tol` AND `product <= 100 * feas_tol` (NaN refuses); otherwise kNumericalError,
  `certificate_gate_refusals`. Norms and scalings (and why):
  - rows = max over equality rows |a_j x - b_j|, inequality rows max(0, a_j x - b_j) and bounds
    max(l_i - x_i, x_i - u_i), each divided by its row scale max(1, |b_j|, ||a_j||_1 ||x||inf) (bounds:
    max(1, |bound|, ||x||inf)) -- the infinity norm because a certificate is a per-row claim, relative to the scale
    every row test of the engine already reads so the gate is invariant to row scaling and to ||x||;
    10 = kInfeasibilityMarginFactor, the walk's own structural margin: the gate asks the exported point exactly what
    the classifier asks a footprint-free violation;
  - product = max over equality rows (rel. residual * |lambda_e,j|), working rows (|rel. slack| * |lambda_i,j|) and
    pinned bounds (|rel. distance| * |z_i|); 100 = kTerminalPolishComplFactor, 1e-7 at the defaults, a decade under
    the drivers' kkt_tol: the quantity a consumer's own KKT check sees (the B-1 term), and on an equality row the
    objective error the residual buys (qp7: 2e5 * 7.3e-8 = 0.015).
  The walk's own tests keep the footprint excuse; only the certificate loses it. Stationarity and signs are NOT read
  by the data half: they are the KKT-point test's and the sign half's, unchanged (open question Q3).

**Why C2 cannot certify a false point (by construction).** A kOptimal exit now requires, on the exported (x,
lambda, z) and the QP's data: (i) every row and bound within 10 feas_tol relative (data half), (ii) the worst
residual*multiplier product within 1e-7 relative (data half), (iii) free-block stationarity within opt_tol*scale (the
KKT-point test, which certification is reached through; an adopted polish must not worsen it) and (iv) multiplier signs
within opt_tol*scale (sign half). No term of (i)-(iv) contains dual_mu or primal_delta, every comparison is written so
NaN fails, and terminal_kkt is computed after the last mutation of x and the multipliers. A "false certificate" in this
round's legs is therefore a kOptimal exit failing (i)-(iv) as recomputed by INDEPENDENT code (`qp_dump_kkt`-class
recomputation on SENS-Q; `tu/stationarity.py` on NLP rows) or an objective off a pinned optimum.

**Why C1 cannot certify anything.** It changes only which answer an EQP solve returns, toward the same proximal
target the single step aims at; every exit still passes the unchanged certificate plus C2.

## 2. Pin, binaries and the ensemble

* **R (= H0, the baseline)** = origin/m8 `99712ec32` (settler C547; code-identical to `96789b9be`, the diff is TU-3
  docs only). **R's ensemble rows ARE the rows of the baseline ensemble census of that pin** (hven-bench `ens`
  `6e42a30`; lane binary sha256 `3155a0834e9e9e7748fe51b818acafc5fd3ff502bf86e1f58ef2bb165f995bc0`; store
  `hven-bench-ens/.scratch/census-m8/store`), verified at freeze by the binary/pin identity check (the store's
  recorded binary sha and pin must equal BINARIES.txt's R entry; a mismatch STOPS). No R ensemble leg is re-run.
* **H** = ⟨sha⟩ = R + the QPC commits (branch `qpc`, rebased onto `99712ec32`). `git diff R H -- src include` is exactly
  C1 + C2 + the counter folds. H is declared as a code-change arm through a BINARIES file the reader checks
  (hven-bench `ens-reader`; flag TODO-HARNESS-ARMS).
* Two installs/lane binaries (R, H); there are no runtime levers. Every arm is (binary, ensemble member).
* **Ensemble (C539; review B4).** Each corpus cell runs as M+1 = 6 members per binary, paired by seed: member 0
  unperturbed; members 1-5 seeded. Perturbation entry points, applied in the harness (binaries untouched):
  primary -- a seeded relative perturbation of x0 of size ~2^-48, projected into the bounds (TODO-HARNESS: key name);
  secondary -- `trust_region_initial` +- k*1e-15 (TODO-HARNESS). Member s of R and member s of H receive the SAME
  perturbation. NLP-46's exclusive tier runs 3 members. The perturbations are inert by construction (below the solver
  tolerances by >= 6 decades); their only effect is to sample the outcome distribution.
* **Cell classes, from R's ensemble, frozen in `cells/classes_R.tsv` before any H row:** STABLE (same status in all
  members, and majors, factorizations and eval_f+eval_jac all tie under the 10% rule); COST-UNSTABLE (same status,
  work differs); STATUS-UNSTABLE (status differs). Per family and pooled.
* **Null distribution:** the 15 R member pairs give, per family and pooled, the distribution of net wins (per
  currency) and of solved differences under no change. floor_f = the largest absolute value among them.

Harness: hven-bench ⟨branch/commit⟩ (TODO-HARNESS; ensemble runner on branch `ens`). R and H run under ONE harness
commit.

## 3. Corpora and families (all TRAIN; HOLDOUT is never run or read -- the runner refuses it)

| corpus | cells per member | terms | families |
|---|---|---|---|
| TRAIN-238 | DN-3 roster's 119 hven-sqp problems x {default, sqp-qn} | I19-2a section 4 terms (run_serial) | hs-x, hs-q, sd-x, sd-q |
| TRAIN-K | the same 119 x `never-default` (sparse arm) | same | hs-k, sd-k (guard families) |
| NLP-46 | cutest-nlp-35's 23 TRAIN problems x {default, sqp-qn} | TU-2 terms; tier per (problem, label) from R member 0; exclusive tier 3 members | nlp-x, nlp-q |
| SENS-Q | the in-repo walk dumps: the 68 `tests/qp/fixtures/e2-d4` files minus FORMAT.txt, x {refactorize, border}, run at the walk seam by a QP dump grid runner (TODO: `bench/qp_dump_grid.cpp`, built for both R and H; prints every QpCounters field, the objective, and the data-half and independent KKT readings) | the dump's minor cap; 900 s kill; QP level, deterministic; no ensemble (single member, the dumps carry their own seed) | sens-r, sens-b |
| PILOT-62 | RF-1 pilot cells | RF-1c leg P terms | certificates only (V2) |
| CENSUS-57 | the walk census against `bench/baselines/2026-10-m7-close-corpus/` (CEN-R3) | census terms; deadline-status cells solo | cen (guard + declared movers) |
| REPLAY-27 | corpus replay walk/ssn/ipm (`u0-replay-cells.txt`) | base x1, head x3 | neutrality + movers |
| FIX | section 7 | ctest | mechanism |

"Strict-optimal" = SQP status `optimal`; "solved" is read on strict-optimal AND on optimal+acceptable. A cell an arm
REACHES ends with a solver status (not a harness error). Cell files frozen by TODO-HARNESS with `cells/SHA256SUMS`.

## 4. Pre-freeze gates (development readings, recorded here before freeze, never round rows)

* **P-POWER (review B4.9).** From R member 0 + H member 0 development runs (one lane, co-run): the number of
  R-STABLE strict-optimal cells, per family, on which H shows `converged_refinement_arms > 0` (C1 exercised) and
  on which H shows `certificate_gate_refusals > 0` (C2 exercised); and the same on SENS-Q and CENSUS-57.
  Development priors (P-1a deciders, not this round's binaries): C1 fired on 0 of 238 TRAIN rows; C2 refused 194 QP
  exits in 21 TRAIN rows (SPANHYD x2, HS99EXP, BT7, OPTPRLOC, LEWISPOL, WATER, HS74, HS75, LAKES, HS89, HS91, LOOTSMA,
  MESH, ALLINITC.never ...), most of them in the review's chaotic set.
  **Rule:** a component with fewer than 10 R-STABLE solved exercised cells in the live corpora (TRAIN-238 + NLP-46) is
  declared **corpus-EXPLORATORY**: no cost or solved GAIN may be claimed for it from those corpora; its ship case rests
  on the no-harm conditions (5.2 S1, S3, R1, V-QP, V2) plus its fixtures (A-conditions) and SENS-Q. Expected: C1
  corpus-exploratory (its evidence is SENS-Q's gated cells, CENSUS-57 and A1); C2 ⟨n⟩.
* **P-GATED.** SENS-Q cells gated in = cells where R shows any of: `dual_regularization_decreases > 0`, a kOptimal
  exit whose data-half reading fails, `optimal_row_gray_exits > 0`, or an exit the H data half refuses. Frozen as
  `cells/sens_q_gated.tsv` ⟨n⟩ from R and H member-0 readings before any ensemble leg.
* **P-CLASS.** The R ensemble (the C547 census rows, plus any supplementary corpus) read first; `cells/classes_R.tsv` and the null floors
  committed before any H ensemble row.

## 5. The ship rule (fixed here, before any row)

### 5.1 Currencies and cell rule
**QM** = `qp_minor_iters`; **LA** = `factorizations` (the active-set search's own accounting). Guards, never currencies:
majors, eval_f + eval_jac, restoration entries, `qp_numerical_exits`, `eqp_residual_refusals`. Cell rule: a WIN in a
currency iff `10*lower <= 9*higher` (integers); LOSS symmetrically; else TIE. **Same-solution condition (B4.8):** a
cell counts toward cost only if |f_H - f_R| <= 1e-6 max(1, |f_R|) in that member; otherwise it is scored "different
solution: better / worse objective" in its own column (never a cost win or loss). Wall-clock is never read.

### 5.2 Ship conditions for H (all must hold; scored per seed j = 0..5, R_j against H_j)
* **S1 Solved (no robust loss).** p_cell = fraction of members strict-optimal (and separately optimal+acceptable).
  A **robust loss** is p_R >= 5/6 and p_H <= 3/6: NONE is allowed, in any family, on either reading. Per family
  sum p(H) >= sum p(R) - 0.5 on both readings. Robust gains are reported symmetrically, beside the losses. Every
  member-0 lost cell is listed, re-run ALONE under the serial rule, and diagnosed (V1-cells), whatever its class.
  The floor never excuses a robust loss (the no-loss threshold is 0 robust losses, stated relative to the ensemble:
  a loss inside the R ensemble's own spread is not robust by definition).
* **S2 Cost (P-1a Q2: within ~5%).** On cells strict-optimal in both arms of a seed and same-solution: the geomean of
  (H+1)/(R+1), median over the 6 seeds, <= 1.05 on QM AND on LA (pooled over TRAIN-238 + NLP-46 + TRAIN-K). No family
  is "-": a family is "-" on a currency only if T_{j,f} < -floor_f in at least 5 of 6 seeds (T_{j,f} = net wins of H_j
  against R_j on family f). Votes are by problem NAME (a problem in two families/labels counts once per currency,
  the median of its cells' signs).
* **S3 Net-win claims (only if made).** Any claimed cost gain needs T_j > pooled floor in at least 5 of 6 seeds on one
  currency with median geomean <= 0.97; not required for a ship (a correctness-plus-simplifying round, Q2).
* **R1 Tail guard (B2/B4.7, unchanged in form).** Only on cells R solves strict-optimal in >= 5 of 6 members: H's
  median must stay below max(R max + 20, 3 R max) on QM, LA, majors and eval_f + eval_jac. Cells R does not solve are
  never tail-guarded; GAINED cells are listed beside lost cells.
* **V-QP Zero false QP certificates.** Every kOptimal exit on SENS-Q under H is re-checked by independent code:
  the QP's own KKT recomputed (rows, product, stationarity beyond rounding, signs) at the data half's thresholds and
  at opt_tol*scale for stationarity/signs, and the objective against the pinned optimum where one is pinned (PF1,
  QC8 qp1/qp7/entry6, YORKNET qp25/qp26, ...). ZERO failures. The unregularized-EQP comparison is REPORTED, not a
  verdict (it cannot see an inconsistent working set; D-4). A false certificate under R is reported and not a stop
  (R is what this round fixes); under H it STOPS.
* **V2 Zero false NLP certificates** on every row of every leg, both binaries, every member (TU-2-r2 section 3;
  `tu/stationarity.py`). One under R member 0 STOPS (as in I19-2a: a defect outside this round's scope).
* **A1** PF1 (both algebras): kOptimal at q = -3.231904984786688e-03 (rel 1e-12) with no rung
  (`dual_regularization_decreases = 0`, dual_mu = 1e-8 on every pass) and `converged_refinement_arms = 1`.
  **A2** the qp7 false-certificate shape (seam) refused on both algebras. **A3** every in-repo DALLASM fixture's kOptimal
  exit inside the data half (independent recomputation). **A4** qp26 m42 and qp25: unchanged readings (C1 does not
  arm there: deciders D-2T = base).
* **V-STAB (I1; declared endpoint).** The class counts STABLE / COST-UNSTABLE / STATUS-UNSTABLE per family and pooled
  under R and under H (H's classes from H's own ensemble), and the transition table R-class x H-class. Reported as an
  endpoint of this round. Directional expectation (stated, not a bar): C2 converts false-kOptimal QP exits into
  kNumericalError, which routes the SQP through its failure ladder, so STATUS-UNSTABLE may RISE on cells with
  `certificate_gate_refusals > 0`; a pooled rise of more than ⟨k⟩ STATUS-UNSTABLE cells is reported to the settler
  before any default event (open question Q1 asks whether it should gate).
* **Neutrality held** (section 6, legs N0-N3). A precondition.

### 5.3 Decision
H ships iff every condition of 5.2 holds. There is one candidate; nothing is ranked. If H fails, nothing ships and a
successor round follows; the components are NOT shipped separately by this file (open question Q2).

## 6. Legs and bars

All legs under `~/.local/bin/box-lane --`; only NLP-46's exclusive tier uses `--exclusive`. Evidence in `gate-QPC/`.
Binaries frozen in `BINARIES.txt` before the first leg; runners sha-check them. Toolchain: clang, `-DHVEN_FP_MODE=SAFER_FAST`,
sequential MKL for bench lanes, no LTO, Linux x86-64; Apple and Windows readings are CI only, and an Apple value not
observed is UNOBSERVED.

| leg | instrument | bar |
|---|---|---|
| B | builds: R and H installs, lane binaries, the QP dump grid (both), BINARIES.txt (`legs/legB.sh`) | rc 0 each; shas recorded |
| C | FULL ctest Release and Debug at H (`legs/legC.sh`) | 100% both; counts reported (development: ⟨Release n/n, Debug n/n⟩); every `QpCertificateData.*`, `QpReleaseReadmission.*`, `EqpConvergedRefinement.*` green |
| N0 | SENS-Q R vs H, member 0, QP level (`legs/legN0.sh`, TODO grid) | every cell with `converged_refinement_arms = 0` AND `certificate_gate_refusals = 0` under H is IDENTICAL to R on every column (status, counters, objective hexfloat, x hash) except the two new counters; any other difference STOPS |
| N1 | REPLAY-27 R vs H (`legs/legN1.sh`; `compare_replay.py` byte-exact) | 0 differences on cells whose H rows carry both new counters 0; a differing cell with a non-zero counter is a DECLARED MOVER, listed with its counters (the replay IS sensitive to the walk) |
| N2 | R vs H member 0 on TRAIN-238, TRAIN-K, NLP-46 co-run, PILOT-62 (TODO-HARNESS compare) | every row whose H counters `converged_refinement_arms` and `certificate_gate_refusals` are both 0 is identical to R on every column except `options_digest`, `wall_s` and the two declared H-only columns (counters/statuses exact; residual-class columns by the calibrated near-ulp gate). Any other difference STOPS. Attribution table of movers by counter |
| N3 | CENSUS-57: H vs the baseline (CEN-R3) | 0 differences on cells with both counters 0; movers listed with counters; no cell loses a status |
| M | mutations (section 8), control first (`legs/mutations_qpc.sh`, `legs/mutate_qpc.py`); the runner's ARGUED set is parsed from section 8 of this file and must EQUAL the script's (parity check, else `VERDICT ARGUED_PARITY_FAIL`) | control clean; every must-kill killed: `VERDICT MUTATIONS_AS_DECLARED` |
| E-R | R ensemble = the C547 baseline census rows (not re-run); identity check against BINARIES.txt; any corpus the census lacks (TRAIN-K, PILOT-62) run as a supplementary R leg under the same harness commit | classes and floors frozen (P-CLASS) |
| E-H | H ensemble, paired | 5.2 inputs |
| Q | SENS-Q under R and H (single member) + the independent certificate recomputation (`legs/legQ.sh`) | V-QP; A-conditions; movers attributed |
| P | PILOT-62 under R and H, 6 members | V2 |
| X | (reported) cross-read with the I19-2a held levers: not run -- I19-2a shipped nothing | -- |
| Sm | install smoke both front ends | rc 0 |
| CI | `gh workflow run ci.yml --ref qpc`; `gh run watch <id> --interval 120` | four lanes green; off-reference readings recorded |

Co-run protocol (CLAUDE.md section 7), declared in every results file: one process per pinned PHYSICAL core of one box
lane, SMT siblings idle; `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`; counters, statuses and objectives asserted; wall-clock
informational, never quoted. A cap/kill cell, or a deviation that decides a bar, is re-run ALONE under the serial rule
before it is called one.

Order: B -> C -> N0 -> N1 -> N2 -> N3 -> M -> P-POWER/P-GATED (recorded) -> E-R (classes frozen) -> E-H -> Q -> P ->
Sm -> CI.

## 7. Fixtures (in-repo; readings at R recorded before the fix commit)

| fixture | R reading (measured, `qpc` worktree, Release) | H reading |
|---|---|---|
| F-PF1 PF1 dump, both algebras (`QpReleaseReadmission.F7PathCorruptedQp1SolvesAtItsOwnRegularization`) | kOptimal 4758 minors, f -0.003231904984786682, 1 rung (refactorize; the test FAILS at R, `dev/R-tdd.txt`) | kOptimal 4756 minors on both algebras (refactorize 4756 factorizations; border 3585), f -0.003231904984786683 / -...682, no rung, arms 1 (development, Release) |
| F-QP7 qp7 shape via the certificate seam, both algebras (`QpCertificateData.Qc8DallasmQp7TheP1aFalseCertificateShapeIsRefused`) | kOptimal on both algebras (row 103, lambda -99759.8, move 7.29e-8): a false certificate; the test FAILS at R | kNumericalError, 1 refusal |
| F-ROW / F-PROD / F-BOUND / F-NAN synthetic seam presentations (`QpCertificateData.*`) | row, product and bound presentations certified kOptimal at R (tests FAIL); the NaN point already refused at R | refused / certified at the declared boundaries |
| F-NAT the DALLASM QC8/SDL fixtures, independent recomputation | inside: worst product 41.3 feas_tol (entry6 refactorize), worst rows 1e-3 feas_tol | inside |
| F-CONV converged mode on a near-dependent face, eliminated and unbordered-bordered (`EqpConvergedRefinement.*`) | n/a (new API) | row miss 1.11e-5 (model, 0 / 1 steps) -> 2.89e-7 (converged, 9 / 10 steps), eliminated / unbordered |
| F-RESET the refinement mode does not survive a solve (`QpReleaseReadmission.TheConvergedRefinementIsPerSolve`) | n/a | bit-identical to a fresh engine |
| F-FOLD the SQP fold of the two counters (⟨SQP seam test, if built⟩) | n/a | 1 refusal folded |

Re-derived pins (declared, each with its mechanism): `QpReleaseReadmission.F7PathCorruptedQp1LeavesTheCappedEndpoint`
(asserted the rung; replaced by F-PF1 -- the rung no longer exists); `QpWa1AscentFootprint.SyntheticRide...`'s
`dual_regularization_decreases <= kMaxDualRegDecreases` (the constant is gone; the field is identically 0);
`QpDegeneracyArm.CertificateToleratesABoundViolationInsideAndOnTheStructuralMargin` (re-premised: the bound half still
tolerates both points, the data half refuses them, a point inside both halves certifies); the trace goldens (two
appended keys).

**Development leg-C reading at H (Release, 2026-11-25, NOT evidence): 4243 of 4262 pass.** Three failures are the
trace goldens (fixed in the tree since). The other sixteen are EXACTLY the set the P-1a decider measured for the gate
prototype (`dec/ctest/decGate.failures.txt`), each a consequence of C2 and each awaiting a settler ruling before it is
re-pinned (open question Q5) -- none is re-pinned silently:

| test | mechanism (first failing assertion) | class |
|---|---|---|
| QpDegeneracyArm.CertificateToleratesABoundViolation... | bound violation inside the bound half's margin, past the data half | premise contradicts C2 by design (re-premised in tree) |
| QpWa1AscentFootprint.SyntheticRidePassedOverOutOfTheBudgetIsRefused | the walk ends on a point 1.92e-8 (19 feas_tol) off its working row after the mu = 1e-10 rung; kOptimal -> kNumericalError | premise (QP level) |
| QpStagnationStop.AResidualContractingAtPointEightCertifiesWithoutAReading | a residual contracting at 0.8 certified on the footprint; now refused | premise (QP level) ⟨reading to record⟩ |
| HsSweepCorpus.Corpus, HsSweepCorpus.Hs15WarmKdOffWithTheFailedSubproblemStep | HS15 sweep: 1 status failure per KD/SOC arm; majors 40 -> 44 on the warm KD-off arm | REAL LOSS on a fixture; must be diagnosed before any ship (decider risk 3) |
| SqpElasticWitnessStart.YorknetMajor0LadderIsUsableFromTheShippedStartAndTheWitness | the YORKNET major-0 elastic ladder's rung is no longer usable from (0, 0) | SQP path |
| B1Gate.TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers | the solve now ends kOptimal where the test asserts the refused triple is not certified | SQP path; V2 check required (an NLP certificate) |
| SqpSolverRestorationSeed.* (x6), JsonLinesTraceSink.SqpMajorReproducesTheHistoryOnTheRowThatSeedsRestoration | restoration is no longer entered on the fixture, so no row takes a seed | SQP path; fixtures lose their premise |
| Threads.ZeroLeavesEveryFactorAtTheBackendDefault, Threads.TheCallersThreadSettingSurvivesEveryTierAndTheRestoration | the warm hot-start handle is null (a QP exit that was kOptimal is now refused, so no hot cache is committed) | SQP path; fixture loses its premise |

Debug leg C: ⟨not yet run⟩.

## 8. Mutations (must-kill unless marked ARGUED)

Applied by `legs/mutate_qpc.py <id>` on the Release tree (anchors pre-checked), run by `legs/mutations_qpc.sh` on
`hven_qp_seam_tests` (`QpCertificateData.*:QpReleaseReadmission.*:QpQc8ImpliedBlocker.*:QpCertificateSign.*`) and
`hven_qp_tests` (`EqpSolve*:EqpConvergedRefinement.*`). Killers are named from the development dry run ⟨pending⟩; the
leg-M reading is the evidence.

| id | mutation | declared killer(s) |
|---|---|---|
| Q1 | the data half never refuses | QpCertificateData.* (refused cases) |
| Q2 | the data half reads rows only | QpCertificateData.AProductPastAHundredFeasTolIsRefused |
| Q3 | the data half reads the product only | QpCertificateData.ARowPastTenFeasTolIsRefusedAndOneInsideIsCertified |
| Q4 | row factor 10 -> 100 | QpCertificateData.ARowPastTenFeasTolIsRefusedAndOneInsideIsCertified |
| Q5 | product factor 100 -> 1000 | QpCertificateData.AProductPastAHundredFeasTolIsRefused |
| Q6 | a NaN passes (comparisons inverted to !(>)) | QpCertificateData.TheVerdictRefusesNanAndReadsBothBoundsInclusively (ANanPointIsRefused cannot kill it: R already refuses a NaN point earlier, dev/R-tdd.txt) |
| Q7 | a refusal exits kInfeasible | QpCertificateData.* (status) |
| Q8 | a refusal is not counted | QpCertificateData.* (counter) |
| Q9 | the data half ignores bounds (terminal_kkt's box loop) | QpCertificateData.ABoundInsideTheFootprintExcuseButPastTheDataHalfIsRefused |
| Q10 | the re-admission never arms | QpReleaseReadmission.F7PathCorruptedQp1SolvesAtItsOwnRegularization |
| Q11 | the rung kept beside the arming | QpReleaseReadmission.F7PathCorruptedQp1SolvesAtItsOwnRegularization (dual_mu record, decreases) |
| Q12 | converged mode ignored on the eliminated path | QpReleaseReadmission (refactorize), EqpConvergedRefinement.EliminatedPath* |
| Q13 | converged mode ignored on the bordered path | EqpConvergedRefinement.UnborderedBorderedPath*, ⟨PF1 border⟩ |
| Q14 | the strict-decrease keep rule dropped (every step kept) | ⟨dry run; else ARGUED⟩ |
| Q15 | the mode not reset at run() entry | QpReleaseReadmission.TheConvergedRefinementIsPerSolve |
| Q16 | armed from pass 0 (global converged refinement, decider D-2) | YORKNET qp25 / ACOPR57 fixtures (D-2 moved them), ⟨dry run⟩ |
| Q17 | arms counted more than once | QpReleaseReadmission (arms == 1) |
| Q18 | the fold of the two counters missing at the main-QP site | ⟨F-FOLD if built; else ARGUED⟩ |
| Q19 | the converged loop's step budget 10 -> 2 | ⟨dry run⟩ |

ARGUED (expected to survive; must equal the runner's set): ⟨filled after the dry run, each with its argument⟩.

## 9. Stops
A pre-flight mismatch; any N0-N3 difference outside the declared-mover rule; an unreported error row; a false NLP
certificate under R member 0 or any false certificate (QP or NLP) under H; leg B or C failing; leg M not as declared
(including the ARGUED parity); a failed Sm or CI leg; any HOLDOUT read; a binary sha mismatch; a spec/declaration
disagreement. A stop is resolved by a successor declaration, never by an edit.

## 10. Not run
HOLDOUT (never). Wall-clock. IPM lanes beyond REPLAY-27's ipm mode (the change is `detail/qp/walk` plus the SQP counter
folds). Accelerate and Windows (CI only). P-1a's centring arms (deferred; C522).

## 11. Compute budget
About ⟨16-20⟩ lane-hours, +-30%: builds 0.8; ctest R+D 1.0; mutations ~19 x 3 min ~1.0; SENS-Q 2 binaries x 2
algebras ~1.0 (PF1 dominates); TRAIN-238 + TRAIN-K: 2 binaries x 6 members x ~40 s x 3 labels ~0.4; NLP-46 co-run 2 x 6
members ~6; NLP-46 exclusive tier 2 x 3 members ~4 (exclusive hours count double); PILOT-62 2 x 6 ~1.5; census + replay
~1.5; smoke 0.1. Elapsed 2-3 days, dominated by NLP-46's exclusive tier.

## Open questions for the settler (before freeze)
* **Q1** Should V-STAB gate the ship (a bound on the pooled STATUS-UNSTABLE rise), or stay an endpoint?
* **Q2** If C2 passes and C1 is corpus-exploratory with no harm, or vice versa, may one component ship alone? (Today:
  no; one candidate.) Component binaries (R+C1, R+C2) would cost 2 more ensemble legs.
* **Q3** Should the data half also read stationarity (terminal_kkt's `stationarity`, e.g. <= 10 opt_tol) as a third
  disjunct? Not in the ruling; not measured by the deciders.
* **Q5** The sixteen ctest movers of section 7: which are re-premised (new fixtures that keep each test's purpose),
  which are re-pinned as declared correctness moves, and must the HS15 sweep loss be diagnosed (and fixed or accepted)
  before freeze?
* **Q4** The gate's product disjunct drives 141 of the 165 TRAIN refusals in the deciders; a separate lever for it is
  not possible after the API lock. Accept as one gate?
