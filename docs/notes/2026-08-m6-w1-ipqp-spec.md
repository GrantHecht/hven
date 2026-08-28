# W1 spec — the IPQP tier (IP-PMM QP subproblem engine)

**Status: APPROVED AS AMENDED (owner, 2026-08-27) — spec of record for W1.**

Date: 2026-08-27 (v2; supersedes the 2026-08-27 draft). Authority: the M6 brief
(`docs/notes/2026-08-m6-brief.md` §1 W1), the roadmap of record
(`docs/notes/2026-08-26-roadmap-m6-m8-draft.md`, main d11b6eb), the SQP lane's
submission (`docs/notes/2026-08-26-m6-register-sqp-lane-submission.md`), E1's
GO verdict (`docs/notes/data/2026-08-m6-e1-acquisition/README.md`) and the
owner's BUILD ruling (ledger, 2026-08-27). Amendments in this revision come
from the settler's third pass and the SQP lane's deep review, both adopted by
the owner; see §11.

Algorithm of record, owner-ruled and not re-litigated: **IP-PMM** —
Pougkakiotis & Gondzio, *An interior point-proximal method of multipliers for
convex quadratic programming*, COAP 78:307 (2021), arXiv:1904.10369. License
discipline (CLAUDE.md §6): papers only; PIQP stays an un-vendored oracle; no
GPL/LGPL solver source is read or referenced.

**Vocabulary, fixed here and used consistently.** Prose calls the tier
**IPQP**, matching the roadmap. Code: `QpMode::kIpm`; `class IpqpEngine`;
`IpqpOptions`, `IpqpResult`, `IpqpCounters`, `IpqpEscape`, `IpqpSeed`,
`IpqpKktLayout`. **IP-PMM** names the *algorithm* in comments and papers, never
a C++ identifier. (Amendment H.)

Written against a read-only survey of branch `m6` at a495e13. Every file, type
and function named below was read; where the tree does not have something the
design needs, the spec says so in those words.

---

## 1. Scope and role

IPQP is **a third QP kernel behind a third `QpMode` enumerator**, not a
replacement for either existing one.

- `enum class QpMode { kWalk, kSsn }` at `include/hven/drivers/sqp_types.h:37`
  gains `kIpm`. Its own declaration comment states the constraint the addition
  honours: *"THE DEFAULT IS kWalk AND MUST STAY SO absent an explicit ruling
  otherwise."* W1 does not seek that ruling.
- **Role (the ruled three-tier composition):** IPQP for **cold acquisition**
  and as the **degeneracy fallback**; SSN for warm exploitation; the tier-3
  exact face refinement for the 1e-10 finish. The composition's binding
  property, and the reason §2's routing is what it is: **a converged IPQP
  iterate never re-enters the one-row-at-a-time walk.** Discarding an
  acquired active set of thousands of rows to re-walk it one blocking
  constraint at a time is the specific waste the tier exists to remove.
- **Selection stays explicit and per-toggle.** `SqpOptions::qp_mode`
  (`sqp_types.h:576`) is the only way to reach it. **No automatic switching in
  W1** — mode-selection telemetry is W4 (instrumentation only); heuristics that
  act on it are M7, bound by the roadmap's settings contract.
- **Where it lands.** New `include/hven/detail/qp/ipqp_engine.h` (declarations,
  enums, `inline constexpr` constants, `detail::` free functions) + new
  `src/qp/ipqp_engine.cpp` (every member body), registered in the
  `target_sources(hven PRIVATE ...)` block at `src/CMakeLists.txt:180-191`.
  This is the split `src/CMakeLists.txt`'s banner mandates and W0.1a
  demonstrated (qp_engine.h 4,092 → 1,497 + `src/qp/qp_engine.cpp` 1,882).
  **Header budget: declarations only** — the brief's "never a fourth 4k-line
  header" is an acceptance condition, not a preference.
- **W3 folds into W1.** The warm-restart repair (§5) ships with the tier rather
  than as a separate package; the brief's W3 deliverable is discharged here.
  (Amendment D.)

---

## 2. Problem shape, indefinite Hessians, and routing

### 2.1 The QP

`struct QpProblem` — `include/hven/detail/qp/qp_problem.h:35`:

```
min  g' x + 1/2 x' H x
s.t. Ae x  = be          (me rows)
     Ai x <= bi          (mi rows)
     l <= x <= u
```

`H` is `SpMatRM` (row-major CSR, `StorageIndex == int`), **upper triangle
only** — `validate()` rejects a below-diagonal entry; consumers read it through
`selfadjointView<Upper>()`. `Ae`, `Ai` are row-major sparse. A bound magnitude
`>= detail::kSsnInfBound` (1e20) is infinite. Sign convention, binding on the
tier's export: `grad(f) + Ae' lambda_e + Ai' lambda_i - z = 0`,
`lambda_i >= 0`, `z >= 0` at an active lower bound and `<= 0` at an active
upper bound.

**The trust-region box is not in `QpProblem`.** It is an l-infinity radius
about the solve's own start point: `lo_eff = max(lower, x0 - Delta)`,
`up_eff = min(upper, x0 + Delta)`, computed once at the top of `solve()`.
Sources: `QpOptions::tr_radius` (per-instance, const) and
`SolveOverrides::tr_radius` (per-call; `sqp_driver.cpp:2434` sets
`ssn_overrides.tr_radius = delta`). The tier consumes the same effective box
and reproduces `QpSolution::tr_active` semantics: a TR-pinned index reports
`kFree` in `bound_state`, `z(i)` forced to 0, with the documented stationarity
caveat.

**A precondition the routing depends on, found in the code and flagged here.**
`QpEngine::refine_on_face` (`qp_engine.h:985`) carries a PUBLIC-API
PRECONDITION in its own banner: *"the trust-region gate below assumes its
window is centred at `clamp(0, l, u)` — true today only because every seeding
site zeroes `seed.x` before it reaches this function. This function takes no
centre parameter and does NOT validate that assumption."* The SQP subproblem is
in **step space** (`x0 = 0`), so the tier satisfies it by starting its solve at
the step-space origin — but this is a silent-failure hazard, not a convenience,
and W1 asserts it explicitly at the hand-off site rather than relying on it.

### 2.2 Indefinite `H` — the mechanism, stated without over-claim

**The proximal-point-convergence claim of the draft is RETRACTED.** IP-PMM's
proximal terms do not make an indefinite QP convex, and nothing in this design
asserts convergence to a stationary point of a nonconvex QP by proximal-point
argument. (Amendment B.) What the tier actually does:

1. **Inertia gate first.** Factorize the unmodified system. If the observed
   inertia is the target of §4, the step is the **exact Newton step** — no
   modification of any kind. On a convex subproblem this is the whole story,
   and the modification machinery below is provably inert, exactly as
   `ssn_engine.h`'s step 8 argues for its own gate.
2. **Inertia wrong → inertia-demanded regularization.** A primal shift `rho` is
   applied on a **current-iterate anchor** and escalated until the inertia is
   right — the Wächter–Biegler Algorithm IC ladder, which
   `InteriorPointSolver::factor_impl`
   (`src/drivers/interior_point_solver.cpp:1678-1863`) already implements.
   **Say it plainly, per `ssn_engine.h`'s ladder-banner naming rule: this is
   NOT a proximal-point method.** The solved Hessian is `H + rho I`;
   mathematically `rho` is Ipopt's `delta_w`. It is a *modification* that buys
   a descent direction, not a *proximal term* that buys convergence theory.
3. **The ladder is MONOTONE per solve.** Within one QP solve `rho` never falls
   below the inertia-demanded floor established so far; §3.2's `(rho, delta)`
   schedule gains that floor as a hard constraint on its decrease. A solve that
   raises then lowers then raises again is a **flap** and is counted
   (`ipqp_rho_flaps`, §7) — flapping is a signal about the subproblem, not a
   free operation.
4. **REQUIRED at convergence: one extra factorization to read the
   unregularized reduced inertia.** At the converged point `rho` is dropped to
   the schedule's residual level and the system is factorized once more.
   - Inertia right → the certificate stands, `QpStatus::kOptimal`.
   - Inertia wrong → **the certificate is downgraded**. The result is never
     `kOptimal`; it reports through the SSN-shaped indefiniteness channel
     (`IpqpEscape::kIndefinite`, the saddle-suspect class), which is what
     `ssn_engine.h:24` already warns about for its own kernel: *"it can certify
     a SADDLE POINT as kOptimal ... without an inertia verdict read AT THE
     point."*
   - Cost, stated as the T4 precedent states it: **+1 factorization per
     certified guarded solve.** R5's `-1` merge (reading the certificate off
     the tier-3 face factorization instead) is permitted **only** under the
     same precondition R5 itself carries — a weak-active-indefinite fixture
     demonstrating the merge is sound — and is not assumed by this spec.
   - **Certification vocabulary is SSN's**, so the driver's KKT gate reads one
     thing from all three kernels.
5. **The honest IP-PMM edge, stated instead of the retracted one.** The real
   structural advantage is the **dual regularization `delta`**: it makes the
   KKT matrix quasi-definite and therefore factorizable **without LICQ**, so
   degenerate and near-linearly-dependent constraint rows are handled on the
   same factorization path rather than by a special case. That is the property
   that makes IPQP the ruled degeneracy fallback, and it is independent of
   convexity.

**Evidence-failure policies, stated explicitly** (CLAUDE.md §6's
never-fabricate rule applied to inertia):

- **Pardiso perturbed pivots.** If `InertiaEvidence::perturbed_pivots` is
  present and non-zero, **the inertia is not evidence about the assembled
  matrix** — the backend factorized a different one. The tier refactorizes with
  a larger `delta`; it never reads a perturbed factorization's inertia as
  right. (`QpEngine`'s own `InertiaVerdict` helper at `qp_engine.h:577-588`
  already takes exactly this position and is the precedent.)
- **`InertiaEvidence::State != kObserved`** (`kQueryFailed`, or `kUnavailable`
  as on some Accelerate paths): a step is permitted **only** at a conservative
  `rho` floor **and** the certificate is downgraded for the whole solve. The
  counts are never zero-filled or inferred. This is Accelerate checklist item
  (m) and is recorded as UNOBSERVED until real Mac hardware runs it.

**Structured per-piece convexification** (convexifying each model piece's dense
Hessian block independently, rather than shifting the whole diagonal) is a
**registered M7 lever**, and its precondition is now **known rather than
open**: per-piece dense blocks are **not exposed at the consumer seam** —
`AggregateEvalSeam` (`include/hven/detail/drivers/aggregate_eval_seam.h`)
flattens the Level-2 claim stream into **one owned CSR value arena**, three
disjoint segments addressed by a `KktLocationTable` permutation. Recovering
per-piece blocks needs a **second seam view**, and the M7 item is sized as
such, not as a tuning change.

### 2.3 Routing — where a finished IPQP solve goes (Amendment A)

This **replaces** the draft's hand-off-first rule. The walk is the
**last-resort fallback**, never the ordinary successor.

1. **Converge to ~1e-8**, i.e. the relative QP tolerances of §3.4 with a `1e2`
   slack factor (`mu <= 1e2 * opt_tol * scale`, residuals likewise). The tier
   does not chase 1e-10 with barrier iterations; that is what tier 3 is for.
2. **Face-threshold by RATIO rule.** Inequality row `j` is **active** iff
   `s_j < kappa * z_j` with `kappa ~ 1e-2`, **requiring `z_j` above `mu`
   scale**; it is **inactive** iff the reverse holds by the same ratio; and if
   **both** `s_j` and `z_j` are tiny it is **UNCERTAIN** and is *never forced
   either way*. Variable bounds are classified identically from the pairs
   `(x - lo_eff, zL)` and `(up_eff - x, zU)`. A ratio rule rather than an
   absolute threshold is the point: it is scale-free, and it is what makes the
   E1 thresholding-boundary artifact (one row at relative slack `1.33e-8`
   against a `1e-8` absolute rule, whose dual was unambiguous) a non-event.
3. **Hand the face to the tier-3 exact refinement** —
   `QpEngine::refine_on_face(qp, face, overrides, out)` — for the 1e-10
   certified finish. Its accepted-path inertia gate `(n_f, m_f, 0)` is
   positive definiteness of the reduced Hessian on the identified face
   (Gould, Math. Prog. 32, 1985), i.e. it is *also* a certificate, which is why
   R5's merge is even conceivable.
4. **If the refinement REFUSES** (sign violation, infeasible face, failed
   inertia gate) **→ SSN from `(x, lambda)`** as a **warm-grade** start. SSN's
   uncertain band is designed to absorb exactly the tie rows the ratio rule
   left UNCERTAIN, and its bulk flip changes the whole implied active set at
   once. This is the second-choice successor, not the walk.
5. **Only a genuine escape goes to the walk, and it goes COLD.** Genuine
   escapes are: numerical error, infeasible-suspect, and early stall (§6.2).
   The iterate is **discarded**.
   **Code fact, and the reason the earlier crash-basis-transfer idea is
   WITHDRAWN:** `crash_basis_seed(const QpProblem &qp, double feas_tol,
   QpSolution &seed, Index &rows, Index &bounds)`
   (`src/drivers/sqp_driver.cpp:505`) takes **only a `QpProblem`** and begins
   `seed.x = Vec::Zero(n)`. It cannot consume an iterate. Transferring the
   IPQP iterate into the walk would require a new seeding surface, which W1
   does not build.

The driver-side shape is the existing hand-off, generalized: `walk_owns_this_qp`
becomes the *last* branch of a routing chain rather than the immediate `else`.

---

## 3. Algorithm

### 3.1 The iteration

Slack form, matching `KKTVector`'s existing block order
(`include/hven/detail/interior/kkt_vector.h:40`:
`[primals | slacks | eq_lmults | iq_lmults]`): `s = bi - Ai x >= 0` with
`lambda_i >= 0`; variable bounds are **condensed into the (1,1) diagonal** and
add no rows, following `install_primal_diags_with_sigma` /
`accumulate_bound_sigma` and `BoundSet`/`BoundDualState`
(`detail/interior/bound_set.h`). The Newton system:

```
[ H + rho I + Sigma_b     0        Ae'       Ai'    ]
[      0            Lam S^-1        0        -I     ]
[     Ae                  0     -delta I      0     ]
[     Ai                 -I          0     -delta I ]
```

quasi-definite for `rho, delta > 0`, hence LDL^T-factorizable for any symmetric
`H` once `rho` dominates; the inertia gate of §2.2 is what certifies that it
does, per iteration, rather than assuming it.

**Mehrotra predictor-corrector**, one numeric factorization and two triangular
solves per iteration:

1. Affine (`mu = 0`) RHS → solve → `alpha_aff` by fraction-to-boundary →
   `mu_aff` → `sigma = (mu_aff / mu)^3`.
2. Corrector RHS with the `Delta S Delta Lambda` term and the `sigma mu`
   centering → solve → `(alpha_p, alpha_d)` by fraction-to-boundary → step.
3. **No line search.** Globalization is fraction-to-boundary only. The tier uses
   none of `detail/globalization/`'s acceptance strategies, funnels, watchdogs,
   recovery chains or restoration — stated explicitly because those are the
   bulk of the NLP IPM's machinery and none of it is in scope.

`tau` (the `bfrac`) is a settings scalar, default 0.995.

**Re-entrancy assertion (Amendment E).** Predictor and corrector share **one**
numeric factorization: `KktFactorization::solve(ConstVecRef, VecRef) const` is
`const` and does not mutate the factor, so two solves against one factorization
are well-formed. W1 **asserts this rather than assuming it** — an executable
test issues predictor and corrector solves against a single `factorize()` and
checks both residuals, so a future backend change that breaks the property
fails loudly instead of silently corrupting the corrector.

### 3.2 The `(rho, delta)` schedule

- `rho_0 = delta_0 = ipqp_rho_init` (8.0, the paper's), `zeta_0 = x_0`,
  `lambda_est_0 = y_0`. The proximal terms enter the **right-hand side** as
  `+ rho (x - zeta)` on stationarity and `- delta (y - lambda_est)` on the dual
  block. `include/hven/detail/globalization/inertia_regularization.h` implements
  the *shift* with `zeta = 0, lambda_est = 0`; W1 supplies the estimates and
  their update.
- Decrease is **gated**: only when the residuals of the regularized problem
  have contracted by the required factor do `rho`/`delta` fall by
  `ipqp_reg_decrease` and the estimates advance (`zeta <- x_k`,
  `lambda_est <- y_k`). This bounded-decrease condition is what the paper's
  convergence result rests on, and it is why copying `prox_reg_decay`'s
  unconditional geometric decay would be wrong.
- **The monotone floor of §2.2 overrides the decrease**: within a solve,
  `rho` never falls below the inertia-demanded floor reached so far.
- Floor `kProxRegFloor = 1e-10` (Cipolla–Gondzio, arXiv:2205.01775 eq. 19).
  **Ceiling `ipqp_reg_max = 1e6 = detail::kSsnProxMax`**, and the tier adopts
  SSN's **relative cap-slack lesson**: an exact `>= kSsnProxMax` comparison is
  defeated by FP drift in the growth chain (`ssn_engine.h:567` records the
  observed `999999.9999999998`), so the exhaustion guard uses
  `detail::kSsnProxCapSlack = 1e-9` relative slack. Saying so here is the point
  — the tier inherits the *fix*, not just the constant. (Amendment G.)

### 3.3 Barrier `mu`

Mehrotra-adaptive as in §3.1, clamped to `[ipqp_min_mu, ipqp_init_mu]`. The
tier computes `sigma` itself; it does **not** call `ClassicAdaptiveGovernor`,
whose `mpc_mu` is private and takes a `SolverContext`. LOQO
(`loqo_mu(avgcomp, mincomp)`, a pure function of complementarity aggregates) is
registered as an M7 second oracle.

### 3.4 Termination and the outcome contract

The tier reports through the **QP layer's** contract. (`hven::ConvergenceFlags`
and the NLP IPM's absolute four-residual grid are a different vocabulary; the
survey confirms `interior/` carries no Ipopt-style relative residual scaling.)

- Status: `QpStatus { kOptimal, kMaxIter, kInfeasible, kNumericalError }`
  (`include/hven/core/solver_status.h:13`), plus `IpqpResult::escape_reason`.
  **The driver branches on `escape_reason`, never on `status`** — `kInfeasible`
  is a *certificate* here and the elastic tier is its only consumer (§6).
- Tolerances: `QpOptions::feas_tol` / `opt_tol` (both 1e-9) applied to the
  **relatively scaled** residuals `src/qp/qp_engine.cpp` already computes
  (~line 624: `scale = max(1, ||Hx||inf, ||g||inf, ...)` folded over the row
  blocks). Reusing that existing `detail::` free function is what makes
  "matching the existing QP modes' outcome contract" checkable.
- Barrier-phase target is `1e2 x` those tolerances (§2.3 step 1); tier 3 owns
  the last two decades.
- Budgets: `ipqp_max_iter` with `QpOptions::max_iter`'s sentinel discipline
  (`<= 0` → size-derived via `detail::effective_qp_max_iter`,
  `qp_engine.h:499-517`), plus a **hard cap of ~60 iterations** as
  `IpqpEscape::kBudget` **of last resort** (Amendment C) and a separate
  factorization cap, because one iteration costs one factorization plus ladder
  rungs. The tier's factorizations charge the driver's probe budget the way
  `ssn_budget_charge` does.

### 3.5 Reuse ledger — verbatim / specialized / new

**Verbatim (no edit to the reused file):**

| Component | File |
|---|---|
| `class KktFactorization` — owned buffer, `compute()`, `refactorize(PatternCheck)`, `solve()`, `peigs()/neigs()/ppivs()` | `include/hven/detail/interior/kkt_factorization.h`, `src/interior/kkt_factorization.cpp` |
| `detail::max_step_to_boundary(SLI, dSLI, bfrac, count)` | `include/hven/detail/interior/barrier_math.h:62` |
| `dual_regularization(mu)`, `prox_reg_decay`, `kProxRegFloor`, `kDualRegScale/Exponent` | `include/hven/detail/globalization/inertia_regularization.h` |
| `class KKTVector` | `include/hven/detail/interior/kkt_vector.h` |
| `BoundSet`, `BoundDualState` | `include/hven/detail/interior/bound_set.h` |
| `split_bound_multipliers` / `recombine_bound_multipliers` (zL/zU ↔ signed z) | `include/hven/detail/qp/ssn_engine.h:1425,1433` |
| `QpEngine::refine_on_face` (the tier-3 finish) | `include/hven/detail/qp/qp_engine.h:985` |
| `QpProblem`, `QpSolution`, `QpOptions`, `SolveOverrides`, `QpStatus`, `BoundState` | `qp_problem.h`, `qp_types.h`, `solver_status.h` |
| the QP residual-scale helpers | `src/qp/qp_engine.cpp` (~:624) |
| the `InertiaVerdict` perturbed-pivot policy | `include/hven/detail/qp/qp_engine.h:577-588` |

**Specialized** (same math, QP-shaped re-derivation in a new
`detail/qp/ipqp_math.h`): `barrier_objective`, `barrier_gradient`,
`bound_barrier_objective`, `accumulate_bound_barrier_gradient`,
`accumulate_bound_dual_terms`, `augment_bound_complementarity`,
`accumulate_bound_sigma` — all exist in `barrier_math.h` against the NLP
engine's shapes. Where a shape lines up the tier calls the existing kernel;
where it does not, the re-derivation names the kernel it mirrors so a future
divergence is visible. **Deliberately not shared:** the complementarity
reduction — `barrier_math.h`'s banner warns its `.sum()` order feeds `mu` and
is ULP-load-bearing.

**New:** `IpqpKktLayout` (§4); the Mehrotra `sigma` oracle; the `(rho, delta)`
schedule with its monotone floor; Ruiz equilibration (§4.3); the starting point
and warm repair (§5); the relative-KKT test; the ratio face-thresholding and
routing chain (§2.3); `IpqpCounters` — `include/hven/core/solver_counters.h`
has **no IPM counter struct at all** (the NLP IPM predates the counters
contract and carries counts on `SolveResult`/`IterateInfo`).

**Explicitly NOT reused:** `InteriorPointSolver` and its `Settings` /
`SolveResult` / `alg_impl` (~1,480 lines with a documented bit-identical
iteration-count gate on FP operation order); every `BarrierGovernor`,
`AcceptanceStrategy`, `GlobalizationMechanism`, `RecoveryChain`,
`RestorationStrategy`. IPQP is a sibling of `SsnEngine`, not a branch inside
`alg_impl`.

---

## 4. Linear algebra

### 4.1 Symbolic once, values per iteration — the seam exists

- `hven::linear::SymmetricFactor` splits `analyze(const SpMatRM&)` from
  `factorize(const SpMatRM&, PatternCheck)`; `factorize` never re-analyzes.
  `PatternCheck { kVerify, kAssumeAnalyzed }` (`symmetric_factor.h:630`) skips
  the O(nnz) `pattern_hash` guard — a **declaration**, not a hint; a wrong one
  is undefined behaviour inside the backend, and only a caller that can *name*
  the mechanism keeping the pattern fixed may pass it.
- `KktFactorization` wraps that in exactly the needed shape: it **owns the
  assembly buffer** (`SpMatRM &matrix()`), offers `compute()` (analyze +
  factorize) and **`refactorize(PatternCheck)`** (numeric only, from the owned
  buffer). This is the named value-only refactorization entry point, already
  production-hardened on the NLP inertia ladder.
- **The plan.** One `compute()` per SQP *solve*; one
  `refactorize(kAssumeAnalyzed)` per IP iteration and per ladder rung. Within a
  subproblem the pattern is fixed by construction (only diagonals change).
  *Across* majors it is fixed whenever the model's structure epoch has not
  moved — the M5 R6 rule of record, "reuse keyed on the structure epoch is
  answer-neutral BY CONSTRUCTION" — so the analysis is hoisted to the first
  subproblem and reused, epoch-gated exactly as
  `InteriorPointSolver::kkt_pattern_check()` gates it
  (`interior_point_solver.cpp:826`), with `ipqp_hoist_symbolic` as the kill
  switch. Executable claim: `analyze_count == 1` per solve,
  `pattern_verify_count == analyze_count` (§8, A9).
- **Factorization path:** LDL^T (`FactorKind::kLDLT` is the only kind
  implemented) with the §2.2 inertia gate. Inertia arrives on
  `FactorizeOutcome::inertia` and via `peigs()/neigs()/ppivs()`. MKL derives
  `n_zero = dim - n_pos - n_neg` with `zero_is_derived = true`; Accelerate
  reports all three natively. The evidence-failure policies of §2.2 govern
  every non-`kObserved` state.
- **Inertia target** for the §3.1 layout, `dim = n + 2*mi + me`:
  `(n + mi, me + mi, 0)`.

### 4.2 What the tree does NOT have, and what W1 builds

There is **no value-only re-valuation path on the SQP KKT assembly**:
`detail/kkt/kkt_assembly.h::assemble_kkt_core` builds a triplet vector and
calls `setFromTriplets` + `makeCompressed()` on **every** call (lines 255-257)
— structure and values together — and its matrix is *working-set shaped* (only
active inequality rows appear), the wrong shape for a barrier method carrying
every row every iteration. `SchurComplement` and `BorderOps` are working-set
machinery and are not on the tier's path.

Two precedents show what to build; the tier follows the second:

- **IPM:** `NonLinearProgram::analyze_sparsity(SpMatRM&)` lays the pattern once
  and records `VectorXi kkt_locations_`, surfaced as `KktLocationTable` /
  `KktScatterView { values_, size_, locations_ }`
  (`include/hven/model/claim_space.h:213`), with offset-addressed diagonal
  writers (`perturb_kkt_p_diags`, `perturb_kkt_c_diags`, `set_primal_diags`)
  doing regularization injection with zero pattern churn. That layout function
  is `NonLinearProgram`'s and cannot be pointed at a `QpProblem`.
- **SSN:** `SsnEngine::sync_matrix(...)` (`ssn_engine.h:1529`,
  `src/qp/ssn_engine.cpp:1156-1230`) caches
  `std::vector<std::size_t> value_pos_` — each emitted entry's position in
  `k_.valuePtr()`, in emission order — behind a `structure_key_` guard with an
  explicit entry-count collision check, zero-fills and re-scatters on a match,
  rebuilds by triplets otherwise. **This is the template.**

**`IpqpKktLayout`**: from `(H, Ae, Ai, n, me, mi)` it computes the pattern once,
materializes it into `KktFactorization::matrix()`, and returns per-source-nonzero
destination offsets plus diagonal slot bases (`primal_diag_base`,
`slack_diag_base`, `eq_pivot_base`, `iq_pivot_base`), so each iteration is an
O(nnz) scatter with no allocation, no re-sort, no `setFromTriplets`. Because
the tier alone writes into that buffer from its own fixed plan, it **can** name
the mechanism — which is what licenses `kAssumeAnalyzed`.

### 4.3 Ruiz equilibration inside the tier (Amendment E)

The tier applies **Ruiz equilibration to its own KKT system** — iterative
infinity-norm row/column scaling to unit scale, the standard preconditioner for
regularized IPM KKT systems.

**Composition with W0.2, stated so the two cannot be confused:**

- **W0.2 scales the declared NLP at the engine boundary** (gradient/row
  equilibration, reported in diagnostics). It changes what problem the SQP
  iterates on.
- **Ruiz normalizes each subproblem's KKT matrix inside the tier.** It changes
  nothing the SQP sees.
- The two compose because they act at different levels and neither reads the
  other's factors.
- **Duals are unscaled on export.** The tier un-applies its scaling before
  writing `QpSolution`, so every multiplier the driver receives is on the
  caller's own scale.
- **Every counter and every pin is stated on unscaled quantities.** A residual
  or multiplier printed, asserted, or baselined is post-unscaling. This is
  non-negotiable: a pin taken on scaled quantities would silently move whenever
  the equilibration changed.

### 4.4 Interface questions for the IPM lane — RULED (see §10)

Recorded here because the dependency ordering named them: whether
`KktFactorization`, `max_step_to_boundary` and `bound_set.h` move out of
`detail/interior/`; whether `mpc_mu` becomes a shared free function; whether
`interior/` adopts the gated `(rho, delta)` schedule. All are ruled in §10;
`interior/` exposes **no** QP-shaped pattern layout, so nothing is asked of it
there — `IpqpKktLayout` is the tier's own.

**Backend `Options`.** The tier needs its own `SymmetricFactor::Options`, not
`detail::sqp_kkt_options()`, and it must be **Accelerate-safe**:
`weighted_matching`, `matrix_scaling`, a non-default `pivot_strategy`,
`factorization_algorithm`, `solve_parallelism`, or `cnr_threads > 0` all
**throw `std::invalid_argument` at construction** on that backend. Any field
mapping to a Pardiso `iparm` slot (`pivot_perturb_exp` → iparm[9],
`max_refinement_iters` → iparm[7], the ordering) makes the commit an
**IPARM-SURFACE** change under CLAUDE.md §6: label + cited validation evidence
+ the tycho-lane review when in scope.

---

## 5. Warm restart (W3, folded into W1)

**No new currency.** `struct WarmStartData`
(`include/hven/warmstart/warm_start_data.h:78`): `primal_`, `eq_lmults_`,
`iq_lmults_`, `bound_lmults_` (n, **signed** `z = zL - zU`), `structure_key_`,
`extensions_`. `struct IpmPolishData` under tag
`kIpmPolishTag = "hven.ipm.polish.v1"`
(`include/hven/warmstart/ipm_polish_extension.h:44`): `mu_`, `z_lower_`,
`z_upper_` (both n, `>= 0`), `iq_values_` (mi, `cI(x) <= 0` at a feasible
point). Lookup is `find_ipm_polish` (`nullptr` = core-only, a capability
statement not an error; duplicate tag throws).

### 5.1 Two flows, kept apart

(a) **Solve level** — the driver's existing ingest (`stage_warm_start` →
`to_sqp_warm_start` → `from_interior_point`, resolving `StartLevel::kSeeded`).
Unchanged.

(b) **Subproblem level** — the tier's own. Each major's QP restarts from the
previous major's IPQP state `(x, s, lambda_e, lambda_i, zL, zU, mu, zeta,
lambda_est)`, carried on the engine instance as an internal `IpqpSeed` the way
`QpEngine` carries `BorderState`/`HotState`. **Not** through `QpSolution`,
which has no slack or barrier blocks and whose shape is published.

**Trust-region shrink-retries are warm restarts, never escapes.** A retry at a
smaller `Delta` re-enters the tier with the previous attempt's state; it does
not count against the escape ladder of §6.2 and does not reset the seed.
(Amendment D.)

### 5.2 The repair

The submission's named weak point: a primal-only near-solution can be *worse
than neutral* (Chen–Goulart–Jones, arXiv:2512.00693). Shipped in W1:

1. **Strict positivity.** `s_i <- max(bi_i - (Ai x)_i, eps_s)`;
   `lambda_i <- max(lambda_i, eps_l)`; `dL = max(x - lo_eff, eps)`,
   `dU = max(up_eff - x, eps)`; `zL, zU <- max(., eps)`. W0.3/R6 already sweeps
   negative face prices at export, so the currency delivers `>= 0`; the repair
   still clamps, because a polish payload legitimately carries exact zeros
   (documented: 0 where a side is infinite, eliminated, or unpriced).
2. **Centrality — Skajaa–Andersen–Ye (2013) two-scalar shift.** The Mehrotra
   starting-point shift in its warm form: two scalars `(delta_p, delta_d)` push
   every complementary pair toward `mu_0` at O(n + mi) cost and **no extra
   factorization**. Gondzio–Grothey (2003) multiple centrality correctors are
   the stronger instrument at one extra solve per corrector — **M7 lever**, own
   toggle, not built in W1.
3. **Proximal re-centering.** `zeta <- x_repaired`, `lambda_est <- y_repaired`.

### 5.3 The `mu_0` clamp rule (the reserved-`mu` ruling — RULED)

`IpmPolishData::mu_` is documented *"Neither consumer in this library reads
it"*, and the NLP IPM's staging refuses it deliberately
(`interior_point_solver.h:1611`: *"the payload's barrier parameter is NOT
consumed: the barrier schedule is a setting; consuming `init_mu_` would be a
value rewriting a setting"*). The rule keeps that principle and still uses the
evidence:

> `mu` is **never adopted as a setting**. `IpqpOptions::ipqp_init_mu` remains
> the setting. `mu` **is** adopted as **initial state**, and only through a
> clamp against the repaired point's own measured complementarity:
>
> ```
> mu_meas = (sum_i s_i*lambda_i + sum dL*zL + sum dU*zU) / (mi + nL + nU)
> mu_0    = clamp( max(mu_meas, kappa_mu * mu_payload), ipqp_min_mu, ipqp_init_mu )
> ```
>
> `kappa_mu = ipqp_mu_adopt_factor` (default 1.0; 0 disables adoption).

Reconciliation for the record: the NLP IPM refuses because its `mu` is a
*schedule* parameter for a multi-phase solve whose phases restart the schedule.
The tier's `mu` is a *state* variable of one QP solve with no phase structure,
produced by an engine that solved the **same declared problem** (the
`DeclarationKey` stamp is what makes that true). It is evidence about an
iterate, not a rewrite of a setting. The clamp is safe in both directions: the
payload can only **raise** `mu_0` off the measured floor, never below what the
repaired point supports, never above the cold default. Observable via
`ipqp_mu_adopted`.

### 5.4 Payload grades — which warm data is accepted at which grade

| Payload | Grade | Rule |
|---|---|---|
| Core + `hven.ipm.polish.v1` | **full warm** | `zL`/`zU`/`iq_values_`/`mu_` read directly; §5.2 repair; §5.3 clamp |
| **Core only** (signed `z`, no extension) | **base warm** | `zL = max(z, 0) + eps`, `zU = max(-z, 0) + eps`, with `eps` derived from `mu_0`; `s` recomputed from `bi - Ai x`; `mu_payload` absent so `mu_0 = clamp(mu_meas, ...)`. (Amendment D.) The split is lossy at a two-sided bound — that is exactly why the polish extension exists — so this grade is documented as a *degradation*, not an equivalent. |
| Stamp mismatch / wrong dimensions / non-finite | **cold** | the driver's existing ingest already degrades these; the tier sees no warm data |
| Foreign / unknown extension tags | ignored | M5 R3 capability downgrade, unchanged |

### 5.5 The WARM-KILL rule (Amendment D)

A warm restart that is going badly must not spend a whole budget proving it:

> A warm-started IPQP solve gets an iteration budget of **≈ the cold median
> (~15 iterations)**. On overrun the solve is **restarted COLD, exactly once**,
> and the counter `ipqp_warm_restart_abandoned` fires. A second overrun is an
> ordinary budget escape.

This is the mechanism the tycho Task 13 observation demands (F8 perturbed
continuation: **warm 24 IPM iterations vs cold 5**, a stale primal forcing
`alpha ~ 0.25`) — a warm start that is worse than cold is detected in ~15
iterations rather than paid for in full, and the cold restart recovers the
5-iteration path.

**Trust thresholds are relative to the QP tolerance**, never absolute: warm
data is trusted when `mu <= 1e2 * opt_tol` and the residual `<= 1e2 * opt_tol`
on the receiving problem's own scale.

### 5.6 Cold start

`x_0` = the SQP's own `x0` (the step-space origin, which also discharges §2.1's
`refine_on_face` centre precondition), pushed into the interior of the
effective box; `s_0 = max(bi - Ai x_0, 1)`; `lambda_0 = 0` shifted positive by
the same SAY rule; `mu_0 = ipqp_init_mu`; `zeta_0 = x_0`, `lambda_est_0 = 0`.
A least-squares `x_0` (one extra factorization) is registered, not built. The
M5 R5 determinism pin is restated for the tier: staging the same payload twice
from cold produces bit-identical first iterates.

---

## 6. Infeasibility, stalls, and the escape ladder

IP-PMM has **no homogeneous self-dual embedding and therefore no infeasibility
certificate**. Infeasibility is a *signature*, never a proof.

### 6.1 The escape ladder (Amendment C)

- **Fresh decision every major. No memory.** The tier's suitability is
  re-evaluated per subproblem; there is **no bench/backoff** — that idea is
  withdrawn. A subproblem is not penalized for its predecessor.
- **Retirement:** after **K = 3 consecutive escapes within one SQP solve** the
  tier is retired for the remainder of that solve; counter
  `ipqp_tier_retired_after` records the major at which it happened. **Any
  success resets the count.**
- **Budget:** the ~60-iteration hard cap is `IpqpEscape::kBudget`, **of last
  resort** — the stall test below should fire first on anything that is
  genuinely stuck.

### 6.2 Early stall detection

Built on SSN's three window properties, which are adopted verbatim in kind:
**the window advances on ACCEPTED steps only**; **improvement is demanded over
the whole window, not per step**; **any regularization change discards the
window and starts a fresh one** ("slow progress under a sigma that just changed
is the safeguard's doing, not the problem's" — `ssn_engine.h:620`). The barrier
form:

> `W = 5` accepted iterations. **Stall iff ALL of:**
> (i) `mu` has not been reduced by `>= 2x` across the window;
> (ii) `max(primal_inf, dual_inf)` improved by `< 1%` relative across the
>      window (SSN's `kSsnStallImproveFactor = 0.99`, deliberately feeble and
>      relative so a flat residual cannot be mistaken for progress by rounding
>      noise);
> (iii) `min(alpha_p, alpha_d) < 1e-2` on **every** step in the window.
>
> **Reset** on a Mehrotra target change that actually dropped `mu`.
> **Never abort on one tiny-`alpha` iteration** — conjunct (iii) is a
> whole-window property by construction.

`W = 5` matches `detail::kSsnStallWindow`, for the same reason its banner
gives: five accepted steps is an order of magnitude above the local regime, so
no healthy trajectory reaches it.

### 6.3 Infeasible-suspect

SSN's **two-conjunct** test, mirrored:

> (a) the primal residual is **flat on a positive floor** over a no-progress
> window, **AND** (b) `||(y, z)||` has grown by `>= 1e4x` over that window
> (`detail::kSsnDualGrowthFactor`, floored at 1 so a zero-multiplier reference
> is measured absolutely).
>
> **Exhaustion-route variant:** the growth conjunct is measured **per accepted
> step at 10x** (`detail::kSsnDualStepGrowth`) — an order of magnitude in one
> step is not multipliers settling, it is multipliers with no limit to settle
> onto.

The tier emits `IpqpEscape::kInfeasibleSuspect` with an **evidence block**
(`IpqpResult::infeasibility_evidence`: each signal with its value, the window,
and the least-infeasible point). **It never returns `QpStatus::kInfeasible`** —
that is a *certificate* in this driver (`sqp_driver.h:1701`: under kSsn a
`kInfeasible` can only have come from the walk) and the elastic ladder is its
only consumer. Promoting a suspicion to a certificate at the driver layer is
precisely the failure the SSN contract exists to prevent. Optional Farkas
corroboration (`ipqp_farkas_gate`, one matvec + O(m), no factorization) may arm
the report; it never certifies.

### 6.4 The W2 hook, and a W2 registration

**W1's hook** is a named single entry point the routing chain calls in the
escape branch:

```cpp
QpSolution certified_feasibility_fallback(const QpProblem &qp,
                                          const IpqpInfeasibilityEvidence &ev,
                                          const QpSolution *seed);
```

In W1 it forwards to the cold walk (§2.3 step 5); in W2 it forwards to
feasibility mode. W2 changes the body and no call site.

**Registered for W2 (Amendment C):** the **elastic `l1`-penalized QP
formulation** — an always-feasible QP — as the **QP-level** answer when the
funnel suspects infeasibility. This is SNOPT's structure, and it is preferable
to a second engine-level infeasibility mechanism: the elastic QP is always
solvable, so the question "is this subproblem infeasible" is answered by
*solving something*, not by accumulating symptoms in two kernels
independently.

---

## 7. Counters and telemetry (the W4 hook)

New `struct IpqpCounters` in `include/hven/core/solver_counters.h`
(`QpCounters` at :44, `SsnCounters` at :290 are the models), folded onto
`SqpCounters` as `ipqp` beside `ssn`, with `accumulate_ipqp_counters` mirroring
`accumulate_ssn_counters` (`sqp_driver.cpp:629`). Conventions honoured: `Index`
fields zero-initialized, snake_case, and each field's doc comment states what
is counted **and what is excluded**. **DNF sentinel convention adopted: `1e6`**
for a cell that did not finish, so a sweep row is never silently blank.

| Counter | Meaning |
|---|---|
| `ipqp_iters` | IPQP iterations (one predictor+corrector pair each) |
| `ipqp_factorizations` | numeric factorizations; `>= ipqp_iters`, exceeding it by ladder rungs + the final inertia read |
| `ipqp_symbolic_analyses` | **1 per SQP solve** under §4.1's hoisting rule |
| `ipqp_solves` | backend triangular solves (2 per iteration nominal) |
| `ipqp_pattern_verifies` | mirrors `SymmetricFactor::Counters::pattern_verify_count`; proves `kAssumeAnalyzed` was used |
| `ipqp_rho_demanded_max`, `ipqp_rho_demanded_last` (double) | the inertia-demanded `rho`, high-water and final |
| `ipqp_inertia_retries` | factorizations rejected on wrong inertia |
| `ipqp_iters_at_elevated_rho` | iterations taken with `rho` above the schedule's residual level |
| `ipqp_rho_flaps` | monotone-floor violations attempted, i.e. down-then-up cycles |
| `ipqp_final_inertia_read` | outcome of §2.2's required extra factorization: `0` right, `1` wrong (certificate downgraded), `2` unreadable |
| `ipqp_reg_decreases` / `ipqp_reg_increases` | `(rho, delta)` schedule moves |
| `ipqp_prox_center_updates` | proximal-estimate advances |
| `ipqp_restart_repairs` | warm restarts where the repair moved a component |
| `ipqp_restart_shift_max` (double, max-folded) | largest repair shift — the honest-magnitude field, cf. `ssn_sign_sweep_max` |
| `ipqp_mu_adopted` | 1 iff the payload `mu` raised `mu_0` off the measured floor |
| `ipqp_warm_restart_abandoned` | warm-kill fired (§5.5) |
| `ipqp_tier_retired_after` | major at which K=3 consecutive escapes retired the tier |
| `ipqp_face_uncertain` | rows/bounds left UNCERTAIN by the ratio rule (§2.3) |
| `ipqp_refine_accepted` / `ipqp_refine_refused` | tier-3 hand-off outcomes |
| `ipqp_to_ssn` / `ipqp_to_walk` | routing outcomes; `ipqp_to_walk` should be rare by design |
| `ipqp_escapes` | subproblems escaped |
| `ipqp_escape_budget`, `ipqp_escape_stall`, `ipqp_escape_indefinite`, `ipqp_escape_numerical`, `ipqp_escape_infeasible_suspect` | escape census; **the five must sum to `ipqp_escapes`** (`SsnCounters:522-527` discipline) |
| `ipqp_stall_reason_mu`, `ipqp_stall_reason_residual`, `ipqp_stall_reason_alpha` | stall-reason census; **must sum to `ipqp_escape_stall`** |
| `ipqp_alpha_p_min` / `ipqp_alpha_d_min` (double, min-folded) | smallest fraction-to-boundary steps — the acquisition-health signal |

**Machine-trace schema v0** (W4's JSON-lines stream, versioned here so M7's
heuristics and M8's human trace share one bookkeeping path):

```
{"v":0,"ev":"ipqp.iter","solve":id,"major":k,"it":i,"mu":…,"rho":…,"delta":…,
 "res_p":…,"res_d":…,"res_c":…,"sigma":…,"alpha_p":…,"alpha_d":…,
 "inertia":[np,nn,nz],"zero_derived":true,"perturbed":…,"facts":…}
{"v":0,"ev":"ipqp.reg","dir":"down|up","rho":…,"delta":…,
 "reason":"accept|inertia|stall|floor"}
{"v":0,"ev":"ipqp.restart","grade":"full|base|cold","repaired":true,
 "shift_p":…,"shift_d":…,"mu0":…,"mu_payload":…,"adopted":true,"abandoned":false}
{"v":0,"ev":"ipqp.route","to":"refine|ssn|walk","uncertain":…,
 "face_rows":…,"face_bounds":…}
{"v":0,"ev":"ipqp.certify","final_inertia":"ok|wrong|unreadable","downgraded":false}
{"v":0,"ev":"ipqp.escape","reason":"budget|stall|indefinite|numerical|infeasible_suspect",
 "evidence":{…}}
{"v":0,"ev":"qp.mode","mode":"ipqp","outcome":"optimal|routed|escaped","facts":…,"iters":…}
```

All numeric fields at 17 significant digits so the stream is a replayable
artifact. Per CLAUDE.md §7 the stream is **instrumentation**: counters and
statuses in it are assertable; any wall field in it is not. Ledger integration
reuses the existing seam — one `SolveRecord` per tier subproblem via
`attach_ledger`, label prefix `"ipqp"`.

---

## 8. Acceptance cells

### 8.1 E1's four residual risks (A1–A4)

- **A1 — block placement at real F7 junctions, two-junction structure.** E1's
  contiguous variant fixed *layout* but drew the offset uniformly on
  `[1, mi-k]`. A1 takes F7's own junction windows from a real bound-arc hop
  (`bench/corpus_cells.h`'s `f7_*_bound_*` cells) so the active set is **two**
  blocks at the geometry's own locations. Assert exact recovery under the
  ratio rule of §2.3 (with E1's Rule-A/Rule-B counts reported alongside for
  comparability) and iterations inside the gate.
- **A2 — a real trajectory hop (not a manufactured `x*`).** E1's `g/be/bi` are
  manufactured by KKT inversion, so `x*` is not a point the family visits. A2
  takes a subproblem dumped mid-solve from a real F7 major
  (`tests/sqp/test_bench_dump.cpp` is the existing dump seam) and pins the
  tier's iterations/factorizations plus agreement with the walk's solution to
  the QP tolerances. Real-hop conditioning is exercised here.
- **A3 — simultaneous bound and row activity.** E1 constructed the box inactive
  at `x*` by design, so no cell has bound activity. A3 constructs cells with
  `>= 1` active variable bound **and** `>= 1` active inequality row at the
  solution, at each margin class, asserting exact recovery of **both** sets and
  the bound-multiplier signs.
- **A4 — the IP-PMM itself, not the PIQP proxy, on the E1 taxonomy.** Re-run
  all 29 E1 cells (20 pre-registered + 9 contiguous) against the shipped tier;
  the cells regenerate byte-identically from
  `docs/notes/data/2026-08-m6-e1-acquisition/generator/` plus the recorded
  seeds, so only the solve arm is new. Gate: E1's pre-registered criteria —
  every cell converges, `< 40` iterations, exact active-set recovery, **and no
  monotone blow-up across the active-fraction sweep at BOTH sizes**
  (`nx = 2e4` and `1e5`), which is asserted rather than spot-checked
  (Amendment F). **The brief's proxy caveat is retired only when A4 is green.**

### 8.2 The BUILD-ruling cell

- **A5 — the 2/8 dead population cells** (tycho_sqp P7 G1/G2: the `kFullWarm`
  setup hop, a cold wide-window solve). The tier is their named fix. hven pins
  the **QP-level surrogate** — a cold wide-window F7 subproblem at the same
  size, solved by the tier within budget — and tycho_sqp pins the population
  cells themselves; both recorded against the same BUILD ruling.

### 8.3 Equivalence and no-regression (A6–A10)

- **A6 — kWalk and kSsn untouched.** The 27-cell U0-corpus counter replay
  (`bench/baselines/2026-08-16-u0-corpus`) at **0 differences on both arms**,
  run SOLO per CLAUDE.md §7 — the standard W0.1a and W0.3 already met
  (W0.3's walk arm: 0 differences across 36 columns). Full suite green at its
  current 1873/1871/0 or better.
- **A7 — structural inertness at the default.** At `qp_mode == kWalk` no
  `IpqpEngine` is constructed (lazy, like `ssn_engine_`), no analysis runs, and
  every `ipqp_*` counter is structurally zero — the "inert and structurally so"
  discipline documented at `sqp_driver.cpp:2414`, proven the same way.
- **A8 — determinism.** Staging the same warm payload twice from cold gives
  bit-identical first iterates (M5 R5); two independent full runs of the A4
  sweep are bit-identical on every non-timing column (E1's own standard, met
  twice by E1).
- **A9 — the symbolic-analysis pin.** `analyze_count == 1` per SQP solve and
  `pattern_verify_count == analyze_count`; non-vacuity demonstrated by
  mutation, per the W0.3 precedent.
- **A10 — Accelerate.** Every Mac-side number is **UNOBSERVED** until real
  hardware runs it (CLAUDE.md §6, absolute). The inertia path genuinely differs
  (native three-way vs derived `n_zero`; no `perturbed_pivots`), and several
  `Options` fields throw on that backend — so A9 and A2 carry Mac legs recorded
  as UNOBSERVED, never zero-filled. Linux-side coverage of the
  `kQueryFailed`/`kUnavailable` branches is required through the
  `HVEN_TESTING` seam convention (CLAUDE.md §8).

### 8.4 New cells (Amendment F)

- **A11 — HS indefinite-Hessian rows with the final-inertia certificate
  asserted.** F7 is convex and **cannot validate the nonconvex mechanism of
  §2.2 at all**. A11 runs the Hock–Schittkowski problems with indefinite
  Hessians (and the parametric `IndefiniteBoxModel` family used by
  `tests/sqp/test_qp_engine_indefinite.cpp`) and asserts: the inertia gate
  fires, the ladder is monotone within the solve, the required final
  unregularized inertia read happens, and a wrong read **downgrades the
  certificate rather than reporting `kOptimal`**.
- **A12 — the perturbed-continuation re-solve with the warm-kill rule
  asserted.** tycho Task 13's cell: **warm 24 IPM iterations vs cold 5**, stale
  primal, `alpha ~ 0.25`. Assert that the warm-kill rule (§5.5) fires at the
  ~15-iteration budget, that `ipqp_warm_restart_abandoned` increments, and that
  the cold restart recovers the short path.
- **A13 — a same-occasion PSIOPT-envelope re-read at `nx = 1e5`.** The
  **30.15x natural gap** is the number the tier exists to move; re-read on the
  same occasion as A4 so the comparison is against a contemporaneous envelope
  rather than an aged one. §7 terms apply: serialized, solo, wall-asserting.
- **A14 — an LTO-on / threads-on context leg, REGISTERED and never a gate.**
  Run once for context; its numbers are informational and no pin depends on
  them.

---

## 9. Settings surface

- **`QpMode::kIpm`** added to `include/hven/drivers/sqp_types.h:37`;
  `SqpOptions::qp_mode` default **stays `kWalk`**.
- **`struct IpqpOptions`** declared in `sqp_types.h` (the same reason `QpMode`
  and the three `Ssn*Rule` enums are declared there: `SqpOptions` carries it,
  and `sqp_types.h` is the header the engine includes, not the reverse).
  Semantics are derived at the fields in `detail/qp/ipqp_engine.h`. Carried as
  `SqpOptions::ipqp` and forwarded verbatim, as the SSN levers are.

| Field | Default | Note |
|---|---|---|
| `Index ipqp_max_iter` | `0` | `<= 0` = size-derived sentinel |
| `Index ipqp_hard_iter_cap` | `60` | kBudget of last resort |
| `Index ipqp_max_factorizations` | `0` | `<= 0` → `3 x` effective `ipqp_max_iter` |
| `double ipqp_init_mu` | `0.1` | measured default, see §10 Q4 |
| `double ipqp_min_mu` | `1e-12` | matches `Settings::min_mu_` |
| `double ipqp_rho_init`, `ipqp_delta_init` | `8.0` | the paper's |
| `double ipqp_reg_floor` | `1e-10` | `kProxRegFloor` |
| `double ipqp_reg_max` | `1e6` | `= kSsnProxMax`; guarded with `kSsnProxCapSlack` |
| `double ipqp_reg_decrease` | `0.1` | gated decrease |
| `double ipqp_tau` | `0.995` | fraction-to-boundary |
| `double ipqp_face_kappa` | `1e-2` | the §2.3 ratio rule |
| `double ipqp_converge_slack` | `1e2` | barrier-phase target = slack × QP tolerance |
| `bool ipqp_ruiz` | `true` | §4.3 equilibration |
| `bool ipqp_warm_repair` | `true` | §5.2 |
| `Index ipqp_warm_iter_budget` | `15` | §5.5 warm-kill |
| `double ipqp_mu_adopt_factor` | `1.0` | `kappa_mu`; `0` disables adoption |
| `Index ipqp_stall_window` | `5` | `= kSsnStallWindow` |
| `Index ipqp_retire_after` | `3` | consecutive escapes per solve |
| `bool ipqp_farkas_gate` | `true` | §6.3 corroboration |
| `bool ipqp_hoist_symbolic` | `true` | §4.1 cross-major reuse; kill switch |
| `bool ipqp_require_final_inertia` | `true` | §2.2 item 4; off = certificate always downgraded |

- **Opt-in, default OFF in M6.** Reachable only by setting `qp_mode`
  explicitly. No default flips, no bundles, no automatic switching. Every
  existing pin, battery and published figure is measured on kWalk and stays
  there — the roadmap's per-toggle settings contract applied one milestone
  early, deliberately.
- **Validation.** `validate_sqp_options` (`src/drivers/sqp_options.cpp`) gains
  the `IpqpOptions` predicates, each **written as the negation of the
  acceptance condition** so NaN is rejected rather than admitted — that TU's
  stated rule, resting on `-fno-finite-math-only` and pinned by disassembly.

**Driver-side changes the addition forces.** `sqp_driver.cpp:1089` reads
`const bool ssn_mode = opts_.qp_mode == QpMode::kSsn;` and the dispatch at
:2419 is `bool walk_owns_this_qp = !ssn_mode;` plus one `if`. A third mode
turns that into `switch (opts_.qp_mode)` with the routing chain of §2.3; the
`walk_owns_this_qp` hand-off generalizes, the `bool` does not. Seam functions
the tier mirrors one-for-one: `ssn_exit_is_a_usable_step` (:563),
`ssn_result_to_qp_solution` (:573), `ssn_start_from_qp_seed` (:598),
`charge_ssn_subproblem_cost` (:615), `accumulate_ssn_counters` (:629),
`SqpDriver::ssn_options` (:3193), `SqpDriver::ssn_engine` (:3186, lazy).
Unchanged: SOC, the elastic ladder and the restoration sub-solve all run the
**walk unconditionally** (`ropts.qp_mode = QpMode::kWalk` at :2092); only the
MAIN subproblem dispatches on `qp_mode`.

**R6 registration (Amendment E).** The tier is a **THIRD producer of exported
inequality face prices**, after `ssn_result_to_qp_solution` and
`refine_on_face`. It must reach `SqpDriver::finish()` — the single export
boundary W0.3 placed the sign sweep at — and **the R6 measurement-point pin
must be extended to cover it**, so the disclosed "terminal KKT is measured at
pre-sweep multipliers" caveat and its executable guard remain true with three
producers rather than two. Registered now so it is not discovered at close.

---

## 10. Open questions — dispositions

All twelve are RULED. Q1–Q4 and Q6–Q12 per the settler (2026-08-27), on the
recommendations carried from the draft; Q5 per the owner, per §2.2.

1. **Where `KktFactorization` lives.** **RULED:** include it in place for W1;
   register the promotion out of `detail/interior/` (to `detail/linear/` or the
   currently-empty `include/hven/kkt/`) for the W5 break window. Same for
   `max_step_to_boundary` and `bound_set.h`.
2. **Tier file placement.** **RULED:** `include/hven/detail/qp/ipqp_engine.h`
   (declarations only) + `src/qp/ipqp_engine.cpp`, mirroring `SsnEngine`,
   registered at `src/CMakeLists.txt:180-191`. Not a new top-level component.
3. **The reserved-`mu` ruling.** **RULED:** the clamped-state rule of §5.3 —
   never a setting, adopted as initial state only through
   `clamp(max(mu_meas, kappa*mu_payload), ipqp_min_mu, ipqp_init_mu)`,
   `kappa` default 1.0, observable via `ipqp_mu_adopted`. This discharges the
   brief's W3 reserved-`mu` item.
4. **`ipqp_init_mu` default.** **RULED:** not guessed — sweep
   `{1e-3, 1e-2, 1e-1, 1}` on the A4 taxonomy and adopt the winner as a
   **measured** default with the sweep on the record; `0.1` is the placeholder
   until then.
5. **Indefinite `H`: hand off, or convexify and report?** **RULED (owner):**
   neither as previously framed. The proximal-point-convergence claim is
   retracted; the mechanism is the inertia gate → inertia-demanded
   Wächter–Biegler `rho` on a current-iterate anchor → monotone-per-solve
   ladder → **required final unregularized inertia read**, with a wrong read
   **downgrading the certificate** rather than reporting `kOptimal`. §2.2 is
   the ruled text.
6. **The tier's `SymmetricFactor::Options`.** **RULED:** its own,
   Accelerate-safe (no `weighted_matching`, no `matrix_scaling`, default
   `pivot_strategy`/`factorization_algorithm`/`solve_parallelism`,
   `cnr_threads == 0` — each throws on Accelerate). Any Pardiso `iparm` slot
   that moves makes the commit **IPARM-SURFACE**-labelled with cited validation
   evidence.
7. **Budget accounting.** **RULED:** yes — the tier's factorizations charge the
   driver's probe budget at the same site and by the same rule as
   `ssn_budget_charge`, so an escaping tier is not free. (The `ssn_prox_carry`
   measurement had to correct exactly this accounting error.)
8. **Restart state carrier.** **RULED:** engine-internal `IpqpSeed`.
   `QpSolution` is a published shape with no slack/barrier blocks; extending it
   is an API break and W5 is already carrying four.
9. **A new `hven.ipqp.v1` warm extension** for cross-*solve* restart.
   **RULED: no** in W1 — the submission is explicit that
   `WarmStartData + ipm.polish.v1` is "exactly the state an IP-PMM restart
   consumes" and that no new currency is wanted. Registered for M7.
10. **A5's cross-tree split.** **RULED:** hven pins the QP-level surrogate now
    — W1 does not block on another tree — with the population cells closing in
    tycho_sqp, both against the same BUILD ruling.
11. **Scope valve on the linear algebra.** **RULED:** yes — if
    `IpqpKktLayout`'s value-write plan overruns, ship W1 with full
    `setFromTriplets` re-assembly per iteration (correct, slower) and land the
    layout as a separately-verified follow-on provable by a bit-identity pin.
    CLAUDE.md §5's "migrate first, rationalize second", applied to new code.
12. **Mac/Accelerate at W1 close.** **RULED:** UNOBSERVED is acceptable (the
    O12 Mac session is the standing mechanism), **provided** the
    `kQueryFailed`/`kUnavailable` inertia branches carry Linux-side coverage
    through the `HVEN_TESTING` seam, so the Accelerate-specific control flow is
    not merely unexecuted.

---

## 11. Review record

**Settler third pass (2026-08-27).** Reversed the draft's hand-off-first
routing after checking the practice of Ipopt, Knitro, SNOPT, WORHP and acados:
every production peer that composes an interior-point acquisition phase with an
active-set finish routes the *converged* interior iterate into a face-based
refinement, not back into a one-constraint-at-a-time walk, and the draft's rule
would have discarded the acquired active set that is the tier's entire reason
for existing. Produced §2.3's ratio-rule face threshold with an explicit
UNCERTAIN class, the tier-3-then-SSN-then-cold-walk chain, the escape ladder
without memory, the SSN-derived stall and infeasible-suspect tests, and the
warm-kill rule; withdrew the crash-basis-transfer idea after reading
`crash_basis_seed`'s signature, which takes only a `QpProblem`.

**SQP-lane deep review (2026-08-27).** Found the structural collision that the
draft's `bool ssn_mode` dispatch cannot carry a third mode, and that the tier
becomes a third producer of exported face prices whose R6 measurement-point pin
must be extended. Found and required retraction of the **proximal-point
over-claim** on indefinite Hessians, replacing it with the inertia-gate /
inertia-demanded-`rho` / monotone-ladder / required-final-inertia-read
mechanism and the honest `delta`-without-LICQ edge, and established that
per-piece structured convexification is blocked at the consumer seam
(`AggregateEvalSeam`'s single CSR arena) and therefore needs a second seam
view — an M7 item now sized rather than open. Added Ruiz equilibration with its
composition rule against W0.2, the `KktFactorization` solve re-entrancy
assertion, the base-currency-only warm split, the warm-kill rule's tycho Task 13
evidence, the tail/DNF and cap-slack counter conventions, and the A11–A14
acceptance cells. Both passes adopted by the owner; this document is the
amended result.
