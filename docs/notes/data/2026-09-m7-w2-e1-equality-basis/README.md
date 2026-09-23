# M7 W2 E1 — an independent equality basis at pre-solve

Branch `m7`. BASE `53676599`. Step (1), the basis component: `88f1ac40`. Step (2),
the driver integration with the full-rank pre-check: `c0c2f6c7`. Gate declaration:
the commit that adds `gates/psym-derivation-e1.txt`. Brief:
`.superpowers/m7-w2-e1-brief.md` (settler clearing D1–D7, rulings S1–S6). Design:
`.superpowers/m7-w2-e1-design.md`. Proposal: `docs/notes/2026-09-22-sqp-degeneracy-proposal.md`.

## What changed

> Fix round 1 (2026-09-23) turned the default **OFF**; see "Fix round 1" below. The
> text in this section describes E1 as it was gated at `a11846b3`.

`SqpOptions::equality_basis` (bool, **on by default**). At the start point the SQP
driver splits the equality rows into an independent set B and a set D that depends
on B once the fixed columns are eliminated (`detail::EqualityBasis`,
`include/hven/detail/drivers/equality_basis.h`, `src/drivers/equality_basis.cpp`).
Every QP the driver builds (main, SOC, elastic, and the restoration phase's model
through `EqualityRowSubsetModel`) carries only B. Its rhs is projected so the
dropped rows stay consistent. QP multipliers are lifted to the minimum-norm
full-size vector, and external ones are folded. A dropped row that is inconsistent
beyond `feas_tol` at two distinct iterates, with bit-identical Jacobian values on
its group, exits `kInfeasible`. Nonlinear rows are re-checked each major. The split
is recomputed when the check or a QP fails, and three consecutive recompute majors
switch the reduction off for the rest of the solve. `SqpResult::equality_basis`
(`SqpEqualityBasisReport`) says what happened. `SqpCounters` and the
trace/corpus schema are unchanged.

A model whose rows are independent drops nothing, and its QPs are byte-identical to
OFF. So is a model whose spectrum has no gap (undecidable).

### The full-rank Cholesky pre-check (settler ruling S1, a declared design addition)

The design estimated the sparse QR at milliseconds. On the f7 collocation
Jacobians Eigen's left-looking SparseQR took seconds to minutes. Before the QR, the
split therefore factors the equilibrated Gram matrix Mᵀ M = Rᵀ R with SimplicialLLT.
When every pivot L_kk is at least max(1e-5, 10 τ_hi), the nonempty rows are
independent and the QR is skipped. The pre-check can only drop fewer rows, never
more: an exact dependence gives L_kk of order √eps, which falls through to the QR.
**The Eigen factorizations are the SCAFFOLD oracle** (owner ruling relayed
2026-09-22: MKL/Accelerate through hven's own `linear/` interface for as much linear
algebra as possible; Eigen sparse factorizations are scaffolding only). The pre-check,
the QR and the reconstruction all sit behind one internal seam,
`factor_row_dependence` in `src/drivers/equality_basis.cpp`. It takes the
equilibrated matrix and returns the retained and dropped rows and the dependency
coefficients, so the oracle can be swapped without touching the driver. **The
production oracle is registered as E1b**: a pivoted LDLᵀ of the equilibrated Gram
matrix through `hven::linear` (Pardiso on Linux, Accelerate on macOS, where the
values are UNOBSERVED until measured on a Mac). Its null vectors give the dependent
rows, and the existing `verify()` rebuild check is the guarantee.

`EqualityBasis::ran_qr()` and `SqpEqualityBasisReport::qr_factorizations` count
the QRs, so the fixtures pin the path taken as a counter, not as a time.

| fixture | pins |
|---|---|
| `EqualityBasis.U1…`, `U4…` | the network with a redundant balance row: the pre-check fails, the QR runs, and it drops as before |
| `EqualityBasis.U7…` | a full-rank slice: the pre-check settles it (no QR) |
| `EqualityBasis.PreCheckFallsThroughOnARedundantRowAndSkipsTheQrWhenFullRank` | both sides of the line on one network, plus a 1203 × 2005 banded collocation-shaped Jacobian, which is settled without the QR |
| `F7ColdScaleSmoke.EmptyWindowColdSolveAtTenThousandVariables` | `qr_factorizations == 0` on the 10⁴-variable solve |
| `CorpusCellsRunner.TheFastCellsEqualitySplitNeverRunsTheSparseQr` | `qr_factorizations == 0` on `f7_n5000_bound_neutral`, the WallDeadline test's cell, solved in-process |

The two slow cells, Release (wall time, **informational**, ctest's own figure):

| test | before the pre-check | after |
|---|---|---|
| `F7ColdScaleSmoke.EmptyWindowColdSolveAtTenThousandVariables` | 19.6 s | 0.06 s (0.08 s in the full suite) |
| `CorpusRunnerProcess.WallDeadlineDoesNotFireAtTheRealBudgetOnAFastCell` | ~300 s | 0.15 s (0.21 s in the full suite) |

"Before" is the first implementer's step-2 Release suite (`.superpowers/m7-w2-e1-STATE.md`).
The pre-change ctest cost history gives 0.17 s and 0.4 s for the same two tests.

## Deviations from the design note (settler ruling S2: accepted as recorded)

| # | deviation | reason |
|---|---|---|
| 1 | The restoration hand-back scatters the sub-solve's \|B\| multipliers (zeros on D). It does not use the min-norm lift | The phase certifies from the sub-solve's own z on the fixed columns. Only the scatter keeps Je(:,all)ᵀ λ equal to what the phase certified; the lift preserves it on F only |
| 2 | A failed reconstruction or gap test makes the whole split undecidable, so nothing is dropped. There is no per-row keep | A kept-back row would be a retained pivot below τ_lo, which fails the gap test anyway |
| 3 | At an exactly feasible point roundoff reads as kFeasible, not kNoise | Harmless: both bands drop the row and project. u4 says so |
| 4 | "Inconsistent but not certified" keeps the row dropped with the projected rhs | The design's "stays in the reduced QP" read that way. The funnel's full h then decides |
| 5 | The zero-major warm-start probe in `make_warm_start` is **not** reduced | A solve that converges at major 0 on a reducing model exports an unreduced structure hash, so a later warm solve degrades from hot to warm. **REGISTERED**, not fixed |
| 6 | `SqpOptions::equality_basis` is a bool, not the design's kAuto/kOff enum | D2 says ON/OFF |
| 7 | u2's coefficient-cap refusal is pinned with an overridden cap (0.5) on an orthogonal-sum row | An order-independent 1e8 near-dependence cannot be built against COLAMD's ordering |
| 8 | The full-rank Cholesky pre-check (above) | S1: a declared design addition, safe direction only |

## The eleven tests whose premise was a dependent equality pair (settler ruling S3)

Each of these tests builds its inconsistency from **dependent** equality rows. In
production E1 removes exactly that state: it drops the dependent row, projects the
rhs, and certifies the inconsistency itself. Each test now sets
`equality_basis = false` with a one-line comment. Each still proves what it was
written for, on the OFF path, which remains a supported configuration (the identity
control and A/B):

| test | model | what it proves now (basis OFF) |
|---|---|---|
| `M7T3ElasticRungVehicle.TheTierRungIsImmuneToTheDriversTrustRegionRadius`, `…TheConstructedCellFiresTheElasticRungAndPricesBothEngines` (via `vehicle_options`) | F7InconsistentChain | the tier rung ignores the TR radius; the cell fires the elastic rung and prices both engines |
| `SqpDriverCertifiedFallback.T7TheEscapeBranchTernaryCONSUMESAnAttachedReport…` | W2T7ScaledInconsistentEqualities (s = 1e8) | the escape branch consumes an attached rung-A report and does not re-run the ladder |
| `SqpDriverCertifiedFallback.T7TheAttachedReportsRestorationOfferIsATROUNDING…` | same | the attached report's restoration offer is decided at rounding |
| `SqpDriverCertifiedFallback.T7F1TheSameInconsistentLinearizationInAllThreeModes…` | same (s = 1e2) | the three QP modes reach kInfeasible through their own escape route |
| `SqpDriverCertifiedFallback.T3TheTierSolvedRungReachesTheSameVerdictOnTheSameLinearization` | same | the tier-solved rung and the walk rung agree |
| `SqpDriverRestoration.AlgebraModeDivergenceIsANonUniqueSubproblemOptimum` | InfeasibleCircleLineModel | the two working-set algebras diverge on a non-unique subproblem optimum |
| `SqpDriverRestorationSeed.AWorseElasticCandidateIsRefusedOnTheSameModel` | InfeasibleCircleLineModel | the restoration seed refuses a worse elastic candidate |
| `SqpDriverRestorationSeed.TheGuardAndThePhaseRunOnTheCallerScale` | BoxWatchingSpreadModel | the guard and the phase run on the caller's scale |
| `SqpDriverRestorationExcursion.TheSubStatusTellsACutOffSubSolve…`, `…TheSubStatusDiscriminatesAtOneParentStatus` | InfeasibleCircleLineModel (+ RunawayValleyModel arms, full rank, OFF harmlessly) | the restoration sub-status separates a cut-off sub-solve from a converged one |
| `SqpDriverRestorationProximity.TheZeroWeightCertifiesOnAScaledSolve` | BoxWatchingSpreadModel | the zero proximity weight certifies on a scaled solve |
| `SqpDriverRestorationSeed.TheSeedsEvaluationIsCountedExactlyOnce` (**the eleventh, found by the second implementer's Debug suite**: with the basis on, Debug measured evals_values 39 against the pinned 41/40; Release still passed) | SeedCountingModel (wraps InfeasibleCircleLineModel) | a taken restoration seed's evaluation is reclassified and counted once |

`tests/drivers/test_options_dump.cpp` moves with the new option: 31 → 32 fields,
62 → 63 dump lines.

**The production path on the same premise** (S3's one new test):
`SqpDriverCertifiedFallback.E1TheDependentInconsistentPairIsCertifiedByTheEqualityBasis`.
This is W2T7ScaledInconsistentEqualities (s = 1e2) with the basis **on**, in all three
QP modes. The pair is split at x0 and the dropped row's gap (4 s = 400) is read at
two distinct iterates. The exit is the basis's certified kInfeasible, with
`inconsistent_row` = the dropped row, a one-row support, two evidence majors, and
**zero elastic activations**: the OFF path's ladder is never climbed.

## Paper-section checks (D5), and the S4 correction

Checked against the text: N&W Lemma 16.1 and §18.5; Golub & Van Loan 4e §5.4 (5.4.1
numerical rank with a gap, 5.4.2 QRCP); Wächter & Biegler 2006 §3.1 (δ_c for a
rank-deficient Jacobian); GMW 1981 §5.8.2 "Linearly Dependent Constraints"; the Eigen
SparseQR documentation (threshold, warning, rank(), the Davis Alg. 915 citation, and
the vendored header's pivot rule, read directly: β ≥ threshold, default 20 (m+n)
max‖col‖ eps); Heath 1982 and Davis 2011 via Foster & Davis (Alg. 933), full text.
Checked at abstract or table-of-contents level only: Andersen & Andersen 1995 (Math.
Prog. 71), Andersen 1995 (OMS 6), Hansen 1998 §3.1, Izmailov & Solodov ch. 7, Wright
1998, Hager 1999 (secondary source), Fletcher's book chapters (unverified), Fletcher 1993.

**S4.** The citation "Gill & Wong 2015 §4" is wrong. The dependency test and the
working-set exchange are in **§3 (Results 3.3–3.5)**. The regularized KKT
(H + ρAᵀA) is in **§6.2**. The proposal note is corrected. E1's code cites no
Gill & Wong section. The test-only arm header `include/hven/detail/qp/qp_degeneracy_arm.h`
still says "§4" in three comments. It is included by `src/qp/qp_engine.cpp`, so
correcting it here would add movers to this round's P-SYM shape. **REGISTERED**:
the correction rides that header's next change.

## The offline basis check on the 27 U0 cells (D7 (3), S5)

`basis-check/basis_check.cpp` is a scratch tool, not built by the project. It was
compiled against the step-2 `libhven.a` with `hven_sqp_corpus`'s own Release flags.
For each of the 27 cells and each engine (walk, ssn, ipm) it repeats
`corpus::detail::run_cell_engine`'s taxonomy: the same setup hop and the same
designated start. For every solve it records:

1. the basis computed at the solve's start point, with no solver involved;
2. the solve's own `SqpEqualityBasisReport`, which covers every in-solve split,
   including trigger-(b) recomputes and the warm probe;
3. the basis recomputed at the solve's **final iterate** (S5).

Result (`basis-check/basis-check.csv`): **117 solves** (81 designated hops and
36 setup hops). Every one is `full_rank` at its start **and** at its final
iterate, with dropped_peak 0, recomputes 0, qr_factorizations 0, switched_off 0,
status optimal, and a smallest retained pivot of 0.5. Nothing is dropped
anywhere, so every QP is byte-identical to OFF. **G4 at 0 differences is
predicted.** S5's trigger-(b) risk was measured rather than left to G4: no re-split
happened on any of these cells.

## Gates — DECLARED (before the chain ran) vs MEASURED

Declaration: `gates/psym-derivation-e1.txt`. Runner: `wrappers/gates-e1.sh`. Chain:
`wrappers/chain_gates_e1.sh`, detached with `setsid` as pid 3452015. It started
2026-09-22T22:07:45-04:00 at HEAD `a11846b3` and ended 23:19:42 the same day with
all six gates green. `gates/README.md` has the full declared-vs-measured detail,
with a transcript file:line citation for every measured cell. The raw transcripts
it cites are copied alongside it (`gates/gates-e1.txt`, `gates/chain.txt`,
`gates/suite-*.txt`, `gates/install-smoke.txt`, `gates/export-contract.txt`,
`gates/fold-proofs.txt`, `gates/psym-*.txt`, `gates/replay/`).

| gate | DECLARED | MEASURED |
|---|---|---|
| G1 Debug suite | 2766 tests, 0 failed (long test excluded) | 2766 tests, 0 failed (`gates/gates-e1.txt:31-32`) |
| G2 long test alone | 1, 0 failed | 1, 0 failed (`gates/gates-e1.txt:44-45`) |
| G3 Release suite | 2767 tests, 0 failed | 2767 tests, 0 failed (`gates/gates-e1.txt:56-57`) |
| G4 U0 replay | 27 cells × 75 columns × 3 engines, **0 differences**. Any moved cell is a DECLARED FINDING and STOPS the round | 27/75/0 on walk, ssn and ipm (`gates/gates-e1.txt:63,65,67`). No cell moved, no finding from this gate |
| G5 P-SYM | DIFFERING 53 = class A 8 + class B 45 in BOTH, with class B equal across configs; ONLY-AFTER = the 3 ADDED objects; negative control ≥ 1 | matches exactly in both configurations (`gates/gates-e1.txt:92-106`, `:138-152`, cross-config `:171-176`). Negative control 22 Release, 14 Debug |
| G5b sections | NON-DEBUG-MOVER = {sqp_options, sqp_solver} (derived) + {ipqp_engine, continuation} (declared class B); library ADDED = {equality_basis}; DEBUG_ONLY 0 Release / 4 Debug; REFUSED 0 | matches exactly in both configurations (`gates/gates-e1.txt:118-120`, `:168-170`). 38 of 42 objects byte-identical in Release, 34 of 42 in Debug |
| G6 | smoke PASSED, export contract OK (align 32), clang-format 0, diff --check 0, dirty 0 | all green, all rc 0 (`gates/gates-e1.txt:183-198`) |
| fold proofs | 46 / 0 failed | 46 / 0 failed (`gates/chain.txt:24`) |

**The class-B shape was measured before it was declared.** A rehearsal of this
round's own G5 segment ran at `c0c2f6c7` (`gates/psym-rehearsal-*.txt`). The two
public structs change layout, so class B cannot be empty. The design note (Q7)
asked for class B to be derived from the includers, and the exact set came from the
rehearsal. The chain re-measures from fresh builds and must reproduce the shape
exactly.

**Three runner changes relative to E4's**, all forced by ADDED sources:

1. The before arm re-configures and runs `ninja -t cleandead`. Without this, an
   object left over from the after arm would sit in the before capture as a
   byte-identical ghost.
2. The capture pair is judged as after = before + ADDED, and ONLY-AFTER must equal
   the ADDED set derived from git.
3. The section check runs on objects present on both sides, and the ADDED library
   object is asserted separately.

## Fix round 1 (review `.superpowers/m7-w2-e1-review-sol.md`, brief `.superpowers/m7-w2-e1-fix1-brief.md`)

Base `26e5c3b2`. Every item was adjudicated ACCEPTED by the settler.

| Item | Commit | What changed | Fixture |
|---|---|---|---|
| C1 | `2eea86dd` | Row norms (compute, verify) and the QR residual are max-scaled two-norms. Any non-finite norm, scaled entry, coefficient (QR or model units) or `I + CᵀC` entry refuses the split (`kUndecidable`, `refusal() == kNonFinite`, nothing dropped), counted in `SqpEqualityBasisReport::nonfinite_refusals`. A non-finite gap reads band `kNonFinite`, which is evidence of nothing. | `EqualityBasis.ExtremeScalingNeverDropsAnIndependentRow`: rows at 1e-160, 1e+160 and 1e-200 stay; dependent pairs at 1e150 and 1e-170 keep one row with finite C; NaN and Inf refuse. |
| C2 | `2714dc8f` | `SqpOptions::equality_basis` defaults **false**. Options dump: 63 lines, 32 fields, `sizeof` 416, all unchanged. The dumped line now reads `equality_basis = false`. The options digest moves (DECLARED; re-pinned at E1b, below). The eleven OFF fixtures keep `= false`. The F-fixtures, the S3 test, the f7 1e4 smoke and the f7_n5000 in-process test set `= true` explicitly. | `OptionsDump.SqpDumpsTheEqualityBasisOff` |
| I1 | `2eea86dd` | The pre-check also checks `max|PGPᵀ − LLᵀ| ≤ 100 (m+1) ε max Gᵢᵢ`. **Beyond the adjudication:** the fixture showed that the guard alone passes a deficient set. Forming G squares away what the guard would need to see. So a 4-step inverse-iteration upper estimate of σ_min is also held to the pivot floor. The header calls the pre-check a heuristic, not a certificate. E1b replaces it. | `EqualityBasis.PreCheckSendsAGramHiddenDeficiencyToTheQr`: rows e1, e1 + 5e-4 e2, e2. Every Cholesky pivot is ≥ 1e-5 and the backward error is 0, so the adjudicated guard passes it. The σ estimate (~4e-9) sends it to the QR, which drops the row. |
| I2 | `2eea86dd` | Every `I + CᵀC` solve checks its normwise backward error against `solve_residual = 1e-10`, and finiteness. This covers the compute-time probe, `project_rhs`, `reduce_rhs`, `reduce` and `lift`. A failed probe refuses the split (`kGram`, `gram_refusals`). Mid-solve, a failed `reduce()` leaves the QP unreduced and refuses the split. A failed `lift` returns the zero-scatter lift, and a failed SOC projection returns the retained rows' own rhs. Both refuse the split at the next build. A conditioning cap `1 + ‖C‖₁‖C‖∞` was tried and removed: it refused U2's 1e10-scaled duplicate. Its `I + CᵀC` is scalar and exact, so the bound is far too pessimistic there. | `EqualityBasis.GramSolveCheckRefusesTheSplit` (`solve_residual = 1e-300` refuses; the default accepts) |
| I3 | `9b26bac9` | The two-iterate rule stores the first iterate itself. It completes only at an iterate that differs bytewise on a **free column of the row's group**, with the group's Jacobian bytes identical at both. A refresh that reads a `kInconsistent` band marks the split suspect. A zero step under a suspect split hands off: every row returns to the QPs for the rest of the solve (`handed_off`), and the existing elastic/restoration ladder decides from the same x. The wording is now "evidence", not "certificate". | `SqpEqualityBasis.F9ZeroStepAtAnInconsistencyHandsOffToTheLadder` (3 modes: kInfeasible, well inside `max_iter`). Negative control, not committed: with the hand-off disabled, 100/100 majors and `kMaxIter` in every mode. |
| I4 | `ac4a8c68` | Every split change (recompute, retryable-QP re-split, mid-solve refusal, hand-off) drops the QP seed, the IPQP cross-major carry and any staged IPQP seed. Each is counted in `split_changes`. | `SqpEqualityBasis.F8SameCardinalityRowSwapDropsTheRowCarries`: D goes {0} → {1} with \|B\| = 1, asserted on the component, and all 3 modes converge. The carry reset itself cannot be observed from the boundary; `split_changes` is counted at the same site. |
| I5 | (above) | Extreme scaling (C1), an adversarial Gram (I1, constructed; see the note below), a stationary inconsistency (I3) and a dynamic row swap under IPQP (I4). | |
| M1 | `2eea86dd` | `group_bytes()`. The rule compares bytes; the hash stays as the fast negative. | `EqualityBasis.GroupBytesAreTheGroupsValues` |
| M2 | `2eea86dd`, `2714dc8f` | The component header and the option comment are cut to terse contract text. The history lives here. | |
| M3 | `829ab3cb` | `.gitattributes` gives `-whitespace` to `docs/notes/data/**/gates*/*.txt` and `**/logs/*.txt`. The raw transcripts are records, so they are exempted rather than edited. `git diff --check 53676599..HEAD` is rc 0. | |
| M4 | this commit | Process note, below. | |

**How the adversarial Gram case was found.** A scratch sweep
(`.scratch/m7w2e1fix1/adv/`, not tracked) ran rows e1, e1 + δ e2, e2 with and
without padding by independent identity rows. With no padding, the pre-check
without the σ estimate accepted as full rank for δ ∈ {7e-4, 5e-4, 1e-4, 1e-5}. An
independent `SparseQR` at τ_lo found rank 2 of 3 each time. With padding, COLAMD's
different order made the Cholesky fall through anyway. With the σ estimate, every
case goes to the QR and is reduced.

**Harness telemetry (review fix item 5): REGISTERED, not run.** Registered as a
DECLARED hven-bench schema event: the `SqpResult::equality_basis` fields
(`computed`, `undecidable`, `dropped_rows`, `dropped_peak`, `recomputes`,
`qr_factorizations`, `switched_off`, `nonfinite_refusals`, `gram_refusals`,
`split_changes`, `handed_off`, `inconsistent_row`, `max_gap`) go into the hven-sqp
side file. This round runs no re-take. With the default OFF, the TRAIN column is
expected to be byte-identical to the `e4-retake` column. That identity control and
a labelled `equality-basis-on` set over the four network cells (DALLASM, STEENBRB,
STEENBRC, STEENBRG) are **E1b's re-pin event**, together with the options-digest
move this round declares.

**E1b: REGISTERED, to land before M7 merges.** E1b is the production oracle behind
`factor_row_dependence`: a pivoted LDLᵀ of the equilibrated Gram matrix through
`hven::linear`, with a defensible rank certificate and checked projection/lift
solves. It replaces the Eigen `SimplicialLLT` / `SparseQR` scaffold and the
pre-check heuristic, and it turns `SqpOptions::equality_basis` back ON as its own
task. The review's note (§5) holds: `SymmetricFactor` exposes neither pivots nor
null vectors, so E1b needs a linear-layer extension or a designed solve sequence.

**Left as is, stated.** The SSN proximal-centre export lifts with the zero-scatter
fallback when a solve fails its check. It is an exact representative, but it is
not counted: that export site holds a const view of the basis.

**Process notes (M4).** The first E1 implementer ran to about **419k tokens**
before retiring at D7 (2). The brief's retirement rule is about 200k. The E1
STATE note did not record this, and the review flagged the omission (Minor 4). The
orphaned corpus subprocess and the one-hour box-lock incident from the same
implementer are disclosed in the E1 STATE note.

**Gates.** Declared before the chain in `gates-fix1/psym-derivation-e1fix1.txt`
(base `26e5c3b2`). G4 is declared at 0 differences: with the default OFF, the U0
replay must match E4's.

## Registered, not this task's

- **E1b**, the production oracle behind `factor_row_dependence` (above), which
  turns the default back ON before M7 merges (fix round 1).
- Deviation 5, the unreduced zero-major warm probe.
- The model-level linearity flag (D3): a model-contract change plus a bench bridge.
- The `qp_degeneracy_arm.h` "§4" comments (S4, above).
- The TRAIN re-take (D7 (4)) is a declared hven-bench re-pin event. A successor runs it
  after the E4 re-take's column lands. HOLDOUT is never touched.
