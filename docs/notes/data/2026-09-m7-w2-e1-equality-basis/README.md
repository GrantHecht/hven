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

**Process note, appended at E1b (Codex re-check of fix round 1, item 5).** The fix-round-1
implementer itself retired at about **376k tokens**, again far over the ~200k rule.
It was recorded only in the settler's session handoff, not in this README, the
fix-round STATE or the signed report; this paragraph is the formal record.

**Gates — DECLARED (before the chain ran) vs MEASURED.** Declaration:
`gates-fix1/psym-derivation-e1fix1.txt` (base `26e5c3b2`). Runner:
`wrappers-fix1/gates-e1fix1.sh`. Chain: `wrappers-fix1/chain_gates_e1fix1.sh`,
detached with `setsid` as pid 3712822. It started 2026-09-23T02:33:02-04:00 at
HEAD `dd157ecb` and ended 03:25:45 the same night with all six gates green.
`gates-fix1/README.md` has the full declared-vs-measured detail, with a
transcript file:line citation for every measured cell. The raw transcripts it
cites are copied alongside it (`gates-fix1/gates-e1fix1.txt`,
`gates-fix1/chain.txt`, `gates-fix1/suite-*.txt`,
`gates-fix1/install-smoke.txt`, `gates-fix1/export-contract.txt`,
`gates-fix1/fold-proofs.txt`, `gates-fix1/psym-*.txt`, `gates-fix1/replay/`).

| gate | DECLARED | MEASURED |
|---|---|---|
| G1 Debug suite | 2776 tests, 0 failed (long test excluded) | 2776 tests, 0 failed (`gates-fix1/gates-e1fix1.txt:31-32`) |
| G2 long test alone | 1, 0 failed | 1, 0 failed (`gates-fix1/gates-e1fix1.txt:44-45`) |
| G3 Release suite | 2777 tests, 0 failed | 2777 tests, 0 failed (`gates-fix1/gates-e1fix1.txt:56-57`) |
| G4 U0 replay | 27 cells × 75 columns × 3 engines, **0 differences**. With the default OFF the U0 replay must match E1's/E4's; any moved cell is a DECLARED FINDING and STOPS the round | 27/75/0 on walk, ssn and ipm (`gates-fix1/gates-e1fix1.txt:63,65,67`). No cell moved, no finding from this gate |
| G5 P-SYM | DIFFERING 51 = class A 8 + class B 43 in BOTH, with class B equal across configs; ONLY-AFTER = `{}` (no ADDED source this round); negative control ≥ 1 | matches exactly in both configurations (`gates-fix1/gates-e1fix1.txt:92-106`, `:139-153`, cross-config `:171-176`). Negative control 22 Release, 18 Debug |
| G5b sections | NON-DEBUG-MOVER = {equality_basis, sqp_solver} (derived) + {sqp_options, ipqp_engine, continuation} (declared class B); no ADDED library object; DEBUG_ONLY 0 Release / 2 Debug; REFUSED 0 | matches exactly in both configurations (`gates-fix1/gates-e1fix1.txt:119-121`, `:168-170`). 38 of 43 objects byte-identical in Release, 36 of 43 in Debug |
| G6 | smoke PASSED, export contract OK (align 32), clang-format 0, diff --check 0, dirty 0 | all green, all rc 0 (`gates-fix1/gates-e1fix1.txt:183-198`) |
| fold proofs | 46 / 0 failed | 46 / 0 failed (`gates-fix1/chain.txt:24`) |

No DECLARED FINDING this round: G4 held at 0 differences (the default is OFF)
and every G5/G5b mover fell inside the declared class-A/class-B/DEBUG_ONLY
partition.

## E1b — the production oracle through `hven::linear` (brief `.superpowers/m7-w2-e1b-brief.md`, design `.superpowers/m7-w2-e1b-design.md`)

Step 1 `434129d0` (IPARM-SURFACE): `factor_row_dependence` is the backend's inertia
certificate on the shifted equilibrated Gram `G − tI` at two shifts, a block
inverse-iteration null basis confirmed by Rayleigh–Ritz, pivoted Gram–Schmidt row
choice with the highest-index tie rule, a masked-Gram certificate of the retained
set, refined normal-equation coefficients, and E1's reconstruction test on M as the
arbiter. Eigen `SparseQR`, COLAMD and the pre-check are gone.

Step 2: the lift and projection solve the quasidefinite `K = [I_B C; Cᵀ −I_D]`
through the backend. At compute time `K` must factor on clean evidence with inertia
exactly `(|B|, |D|, 0)` and pass a probe; every solve runs up to 2 explicit refinement
steps and is accepted iff the a-posteriori ESTIMATE `‖r̂‖₂ + γ_{w+1}‖|K||x̂| + |b|‖₂`
(`‖K⁻¹‖₂ ≤ 1`), inflated outward by `1 + 4(w + n_K + 2)u` (fix round 1, F2 below), is
`≤ 1e-8·‖b‖₂`. Any failure at compute time is `kLift` (was `kGram`);
a failure on one call keeps E1's pending-refusal transition. Eigen `SimplicialLLT` and
`solve_residual` are deleted. `SqpEqualityBasisReport::gram_refusals` becomes
`lift_refusals` (a DECLARED schema move), counting compute-time `kLift` refusals and
every failed call at the five sites: the SOC and QP projections, the two commit lifts,
and the SSN prox-centre lift (E1's "left as is" above is closed: the driver folds the
prox failures after each major and refuses the basis at the next build).

- **Finding at T4 (DECLARED).** The acceptance is relative to `‖b‖`. For the
  projection `b = [0; −γ]`, `‖b‖ = ‖γ‖` does not grow with `C` while the rounding
  term `γ_{w+1}‖C‖‖w‖` does, so with `‖C‖ ≈ 1e8` (T4: `C = 2²⁶[u₁, u₁ + 2⁻²⁶u₂]`) the
  projection estimate is ≈ 4e-8·‖γ‖ and the projection REFUSES. That is the declared
  safe direction (the basis is refused mid-solve); the lift on the same `K` passes
  with estimate 4 against `1e-8·‖y‖ ≈ 4.5e7`. **Settler ruling S5 (the coupling):** the
  acceptance stays relative to `‖b‖` (a normalization by `‖|K||x̂|‖` would be a
  backward-error measure and looser; the design promises a forward one). The
  refusal is real: a projection through `K` carries a relative uncertainty of about
  `‖C‖·u`, so at the coefficient cap `coef_cap = 1e8` it is ≈ 1e-8 = `lift_forward_tol`,
  and T4 refuses near `‖C‖ ≈ 1e8` by construction. The two constants are coupled
  (`equality_basis.h` states it at both). REGISTERED: lower the cap to `1e6` as a
  declared change if any real cell refuses here (no TRAIN cell has coefficients near
  the cap).

**AMENDMENT 1 (settler S1; restated at fix round 1, F1).** A count of 0 at `t_lo` on
clean evidence gives `λ_min(G) ≥ t_lo − η` (Weyl), with η the Gram-formation plus
factorization error (unbounded by any checkable number; the contract below), so the
split is `kFullRank` even when the `t_hi` count is positive: the RETAINED arm with the
narrower margin `t_lo`. The
full-rank collocation chain measured `n_neg(G − t_hi I)` = 0/0/2/3/3 at
400/700/1000/1500/2000 intervals (0 at `t_lo` throughout); as first designed those
would have been undecidable, contradicting the design's "every corpus solve takes the
fast path". The header records it.

**E1b-L1 (settler S2, REGISTERED, not built).** A long collocation chain WITH a real
dependence reads `t_hi = k + 3` against `t_lo = k` and is undecidable. It fails safe,
and no TRAIN cell has that shape. Candidate fixes: accept `k` when the `(k+1)`-th
Ritz value the inverse iteration already computes exceeds a declared multiple of the
`k`-th and of `t_lo`; or scale `t_hi` with `m`.

**C2, iparm[9] (settler S3).** The installed oneMKL 2026.1 example
`pardiso_sym.c` (mtype −2) documents `iparm[9] = 13` as "Perturb the pivot elements
with 1E-13" and `iparm[13]` as the perturbed-pivot count. The scaling of that
threshold by the matrix norm is not stated in anything installed, so it is
UNVERIFIED. The certificate therefore rests on the fail-closed perturbed-pivot check
alone, which the MKL-only exponent-1 arm (`T3PerturbedPivotRefusesTheCertificate`)
shows fires.

**Conservatism (C5, accepted).** Near-dependence in (4e-8, 1e-5) now reads
undecidable where E1's QR would have split it: the safe direction.

**Fixtures.** T1 (the adversarial Gram set is DEPENDENT), T2 (the 1e10 duplicate is
exact; `K`'s lift and projection bounds ≤ 1e-14·‖b‖), T3 (fast path, fail-closed,
MKL-only perturbed pivot), T4 (the multi-column near-cap lift meets its bound; the
projection refuses as above), T5 (the OFF byte-identity pin on a dependent QP against
goldens generated at `8fdeed3e`, `e1b-off-golden/`), T6 (F9's negative control
through the `HVEN_TESTING` seam: hand-off disabled → `max_iter` in every mode), T7 (a
prox-centre lift failure through the seam's tolerance is counted and refuses the
basis). Accelerate: every inertia count on `G − tI` and `K` is UNOBSERVED until the
macOS lane runs them.

## E1b fix round 1 (Codex review `.superpowers/m7-w2-e1b-review-sol.md`, settler adjudication `.superpowers/m7-w2-e1b-fix1-brief.md`)

**F1 — THE NUMERICAL-RANK CONTRACT (ruled option (b)).** The split decides NUMERICAL
rank at declared thresholds, not exact rank; "never false DEPENDENT" and "exact
certificate" are withdrawn wherever they appeared (the design note is historical and
carries a one-line pointer). On the unit-norm rows, with `t_lo = max(20·m·u, 1e-10)·‖G‖∞`,
`t_hi = 1e4·t_lo` and `τ_lo = 20·(|F| + me)·u`:
DEPENDENT = counts `k > 0` at both shifts on clean evidence AND every dropped row
reconstructs to `τ_lo(1 + ‖c̃‖₁)`: a row that close to the retained span is dependent
BY CONTRACT whether or not it is exactly so (the QP regularizes its duals at
`dual_mu = 1e-8 ≫ τ_lo`, so it is dependent for every subproblem hven builds);
RETAINED = `n_neg(G − t_lo I) = 0` on clean evidence, `λ_min(G) ≥ t_lo − η`;
UNDECIDABLE = anything else, drop nothing. The operative DEPENDENT threshold is the
arbiter's `τ_lo` (≈ 1.8e-14 on two rows), far below the shifts' σ scale `√t_lo`
(≈ 1.4e-5): the band between is UNDECIDABLE. A rigorous machine-checkable η for sparse
LDLᵀ is NOT pursued: no practical NLP solver has one and its cost is unbounded; the
probe solve, the perturbed-pivot check and the reconstruction arbiter remain the
fail-closed evidence. Default ON stands under the declared contract. Fixture
`EqualityBasis.T8NumericalRankContractOnTheTwoRowPair` (the reviewer's `(1,0),(1,δ)`
pair, every δ built from the code's own `tau_lo()/tau_hi()/shift_lo()/shift_hi()`):
`δ = τ_lo/10` DEPENDENT (one row dropped: the contract's direction, not a defect);
`δ = √(τ_lo·τ_hi)` and `δ = 10·τ_hi` UNDECIDABLE (`kSpectrum`, counts 1/1, nothing
dropped: the safe direction); `λ_min = √(t_lo·t_hi)` RETAINED on AMENDMENT 1 (counts
1/0, two factorizations); `δ = 10·√(2·t_hi)` RETAINED on the fast path. NOTE: the
brief named the DEPENDENT threshold by the shift formula; in the code the shift only
admits a candidate and the arbiter's `τ_lo` decides, so the contract is written with
both, as the code runs.

**F2 — the lift quantity is an a-posteriori ESTIMATE, inflated outward.** The exact
mathematics (`K² = diag(I + CCᵀ, I + CᵀC)`, `‖K⁻¹‖₂ ≤ 1`) is unchanged; the computed
value `‖r̂‖₂ + γ_{w+1}‖|K||x̂| + |b|‖₂` is itself rounded, so it is now called an
estimate and multiplied by `1 + 4(w + n_K + 2)u` before the comparison (`solve_k()` in
`equality_basis.cpp` carries the standard-model derivation: the `|K||x̂| + |b|`
evaluation, the two max-scaled norms over `n_K = |B| + |D|` entries, `γ`'s own
formation, the comparison's products; first-order total `(w + n_K + 14)u`). NOTE: the
ruling's factor was `1 + 4(w + 2)u`; the recursive sum inside each norm contributes
`γ_{n_K−1}`, which `w` does not cover once `n_K > w`, so the implemented factor adds
`n_K` (strictly more outward). The inflation is ~1e-12 relative at `n_K ~ 1e4` and moves
no fixture: T2, T4, T7 and `LiftSystemCheckRefusesTheSplit` pass unchanged. The
refusal semantics are unchanged; no higher-precision fixture (ruled cost > value).

**F3 — the warm structural probe no longer degrades silently.** `reduce()`'s return is
checked at the probe (`sqp_solver.cpp`, the values-only probe block); a compute-time
`kLift` on the probe's own split or a failed projection is counted in the new
`SqpEqualityBasisReport::warm_probe_refusals` and in `lift_refusals`. The report
field states the degradation: the unreduced probe's structural hash cannot match a
warm start from a reducing solve, so the level resolves below `kWarm` (`kSeeded` when
the values are ingestible). Seam fixture
`SqpEqualityBasisSeam.T8RefusedWarmProbeIsCountedAndDegradesTheWarmLevel` (the fifth
seam's third knob, `warm_probe_forward_tol`; `docs/testing.md`).

**F4 — S5 coupling.** Stated in `equality_basis.h` at `coef_cap` and
`lift_forward_tol`, and in §T4 above (the "left to the settler" sentence is replaced by
the ruling).

**F6 — process (recorded, not blocking).** Per-dispatch token figures for the E1b
dispatches of 2026-09-23, from the harness usage notifications: gates declaration
**162.6k**; record + re-take launch **235.9k**; close **189.8k**. The pre-compaction
implementers' figures are **UNRECORDED**: the settler's summary lists overruns of
250k / 259k / 280k without a per-step mapping, and they stay unrecorded rather than
being assigned. Explicit-pathspec staging is verified by the settler at every
hand-back (`git status` plus the commit's contents); that verification IS the
control, and it is recorded as such.

## Registered, not this task's

- **E1b**, the production oracle behind `factor_row_dependence` (above), which
  turns the default back ON before M7 merges (fix round 1).
- Deviation 5, the unreduced zero-major warm probe.
- The model-level linearity flag (D3): a model-contract change plus a bench bridge.
- The `qp_degeneracy_arm.h` "§4" comments (S4, above).
- The TRAIN re-take (D7 (4)) is a declared hven-bench re-pin event. A successor runs it
  after the E4 re-take's column lands. HOLDOUT is never touched.
