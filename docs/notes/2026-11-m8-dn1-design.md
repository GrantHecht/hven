# M8 DN-1: the dense arm behind the factor layer (design)

Date: 2026-11-08. Author: the DN-1 design lane. Status: DRAFT for the settler's read.
Base: `m8` at `44af1c16d`. No code in this round.

Authority:
- the M8 brief, `docs/notes/2026-11-m8-brief.md`, §DN (`:218-300`);
- the AP-1 review record, `docs/notes/2026-11-m8-ap1-api-review.md` §4.4, as renamed by AP-2 B8
  (`docs/notes/2026-11-m8-ap2-b8-naming.md:445-447`);
- the settler's rulings C259 and C262. Their eight binding inputs are answered in §§2-12; the map
  is in Appendix A.

Evidence:
- DN-0, `docs/notes/data/2026-11-m8-dn0/`;
- DN-0 r1, `docs/notes/data/2026-11-m8-dn0-ipm-r1/`;
- this page's own analysis over DN-0's raw replay data, `docs/notes/data/2026-11-m8-dn1-design/`:
  `dn1_design_analysis.py`, `results.txt`, `matrices.csv.gz`, `PROVENANCE.txt`.

A citation of the form `[A]`, `[C2b]` or `[D1]` names a section of `results.txt`. Every number is
Linux x86-64 with MKL. Apple/Accelerate is UNOBSERVED throughout (§10).

---

## 0. Decisions at a glance

| topic | decision | section |
|---|---|---|
| seam | inside `SymmetricFactor`: a runtime dense arm beside the platform session, chosen at `analyze()` | §2 |
| TU placement | new Apache-2.0 files `include/hven/detail/linear/dense_session.h` + `src/linear/dense_session.cpp`; a few lines of dispatch in each platform adapter (`symmetric_factor_mkl.cpp`, `symmetric_factor_accelerate.cpp`); the MPL-derived session files are untouched | §2 |
| factor-layer fields | `SymmetricFactor::Options::dense_factorization` (`DensePolicy`, default `kNever`) and `SymmetricFactor::Options::max_dense_dimension` (`std::optional<Index>`, `kAuto` only) | §3 |
| new names (owner Q1) | `SymmetricFactor::Counters::dense_factorize_count` (AP-1); `Counters::dense_fallback_count` and `FactorEvidence::dense_factor_entries` (new) | §3 |
| selection | DN-1: `kNever` (default) and `kAlways`. `kAuto` and its threshold stay refused by both engines' `validate()` until DN-3 | §4 |
| exactly singular | per-factorization **fallback to the platform session**. The outcome and evidence are that backend's, verbatim | §6 |
| near-singular | no threshold. The dense arm reports Bunch-Kaufman inertia as observed, with `perturbed_pivots` ABSENT. A threshold zero class would send 38-46 % of matrices to the fallback | §6, §7 |
| agreement proof | against an exact-eigenvalue **reference**, with the literal C262 form (Pardiso-unperturbed agreement) reported and gated at n ≤ 32. At larger n, Pardiso is the arm that is wrong | §7, §14 |
| allocation | every buffer is sized at `analyze()`; `factorize()`/`solve()` allocate nothing (LAPACK `_work` entry points) | §8 |
| Accelerate | Accelerate's LAPACK through `lapacke_shim.h`, extended with a caller-workspace `dsytrf`. Every Apple figure UNOBSERVED | §10 |
| licences | LAPACK through MKL / Accelerate only. No new dependency, nothing vendored, no `notices/` entry | §11 |
| per-call alternatives | dense first, confirmed. Against Pardiso call trimming the gap is an order of magnitude (0.72-0.74 vs ≤ 0.04). Against F2 the "order of magnitude" is **refuted**: a realistic F2 saves 0.26-0.41 (2-3x less), its ceiling equals the dense arm's, and after the dense arm it is worth 0.04-0.06 | §12 |

---

## 1. What the evidence says

### 1.1 Cost (the SQP is the case the arm is built for)

DN-0 SQP median cells:
- `(B_sym + B_fac)` = 0.877 (hs043) and 0.863 (HS84) of solver instructions;
- per-cell median over the populations: 0.874 (HS) and 0.867 (small-dense) (`dn0/results/summary.txt`).

The call structure, from DN-0's `index.tsv` [D1]:

| SQP, per-cell median | HS TRAIN (21) | small-dense TRAIN (95) |
|---|---|---|
| Pardiso calls per solve | 276 | 735 |
| analyses / factorizations / solves | 2 / 9 / 248 | 12 / 22 / 698 |
| solves per factorization | 28.2 (hs043: 248/7 = 35.4) | 28.8 |
| `(B_sym + B_fac)` per Pardiso call | 39.2 k instructions | 43.2 k |
| isolated sparse analyze / factorize / solve | 112 k / 12.6 k / 31.7 k | 131 k / 16.6 k / 37.9 k |
| isolated dense densify+factorize / solve | 3.6 k / 3.2 k | 6.3 k / 5.4 k |

Every replayed KKT system in DN-0 has n ≤ 406, and 73 % of the SQP's have n ≤ 16. So "per call" is close to "fixed
per call": the solve phase alone costs ~32-38 k instructions at n = 2-16.

A correction to C262's wording. Dense factor+solve is ≈ 4-6 % of a *fresh* analyze+factorize+solve
(DN-0's `rho` = 0.046-0.061 on the SQP median cells). Against an *average engine call* it is ≈ 17-27 %,
because the engine reuses the symbolic analysis. The engine-weighted ratio is the one that projects
a saving: `rho_engine_weighted`, median 0.106 (HS) and 0.140 (small-dense).

Projected saving [D1]: the dense arm replaces every analysis with nothing, every factorization with
densify+dsytrf, and every solve with dsytrs. The measured B is apportioned by the replay's per-phase
split; calibration (measured/replay-predicted) has median 1.20 (HS) and 1.09 (small-dense).

| SQP, share of solver instructions, median (q25-q75) | HS | small-dense |
|---|---|---|
| dense arm | 0.769 (0.752-0.795) | 0.735 (0.674-0.770) |
| dense arm, process-once OpenMP init excluded | **0.744** (0.697-0.776) | **0.717** (0.627-0.756) |
| residual `(B_sym + B_fac)` after the arm | 0.089 | 0.117 |

**A finding the r1 notes missed.** The SQP also pays the process-once OpenMP initialization:
- MKL's first Pardiso call runs `MKL_Read_ThreadENV_Count_Cores -> __kmp_serial_initialize`, as
  `ensure_solver_initialized` does on the IPM;
- r1's marker, applied to the SQP's raw perf chains, puts it at a median 0.100 (HS) and 0.042
  (small-dense) of solver instructions, max 0.50 [D1];
- the frozen map's leaf fallback files it as B_fac (libiomp5 leaf) or B_eng (libc leaf).

A dense arm calling threaded MKL pays the same initialization, so this design projects only the
init-excluded figure. Even so, the arm clears the brief's 20 % bar more than three times over.

**The IPM** carries no projected-saving weight (C262 input 4). For the record only: on r1's
init-excluded reading the same projection gives 0.339 (HS) and 0.243 (small-dense) [D1]. These
numbers are informational and are not an input to any decision here.

### 1.2 Inertia

The ruling's agreement table is the SQP small-dense population. This analysis reproduces it to
the third digit:
- agreement 0.984 / 0.982 / 1.000 / 0.788 / 0.821 / 0.598;
- `rho` 0.050 / 0.080 / 0.126 / 0.181 / 0.368 / 1.066 [A'].

Adding an exact reference changes how those numbers read [C2b]:
- **Reference:** `eigvalsh` (LAPACK `?syevd`) on the densified matrix. A matrix is *decidable* when
  min |λ| > n·eps·max |λ|.
- **The dense arm (MKL `dsytrf`, DN-0's dense column)** matches the reference's negative count on
  **100 % of decidable matrices at every size, both engines**: IPM 2018/2018, SQP 2761/2761.
- **Pardiso** matches on 99.36 % (IPM) and 94.13 % (SQP). On the matrices Pardiso did *not*
  perturb: IPM 100 %, SQP 94.44 %.
- **Every one of the 140 SQP unperturbed disagreements** (all at n ≥ 9, 135 of them at n ≥ 33) is
  a case where the dense count equals the reference and Pardiso's does not [C2]. Example: MESH,
  n = 77, Pardiso 42 negatives, dense and reference 40, min |λ| = 5e-9·max|a|. The eigenvalues near
  the SQP's 1e-8 regularization are where Pardiso's restricted (supernodal) pivoting misreads signs.
- **Undecidable matrices** (min |λ| ≤ n·eps·max|λ|): 666 IPM (barrier scaling) and 56 SQP. Neither
  arm's sign decision there is meaningful. Two Bunch-Kaufman implementations disagree on 23
  matrices, all undecidable [C1].

Who perturbs, and what the dense arm says there [C2c]:

| | Pardiso perturbed | of which undecidable | decidable, dense right | dense exactly singular |
|---|---|---|---|---|
| IPM | 264 | 179 | 85 / 85 | 15 |
| SQP | 259 | 14 | 245 / 245 | 10 |

C262 premise 2 says "most small-n disagreements are Pardiso-perturbed or dense-singular". That holds
at n ≤ 8 for both engines (all 21 SQP and 12 IPM disagreements). Above n = 32, SQP disagreement is
unperturbed and is Pardiso's error.

### 1.3 Solve accuracy

Replay relative residual (`‖Ax−b‖∞ / (‖A‖∞‖x‖∞ + ‖b‖∞)`, 5476 matrices where both arms solved):

| arm | max | q99 |
|---|---|---|
| dense | 2.4e-16 | 1.0e-16 |
| sparse | 0.33 | 1.3e-8 |

The sparse tail is perturbed pivots solved without refinement. No matrix has a dense residual
above max(1e-10, 100× sparse).

---

## 2. The dispatch seam and TU placement

**The seam goes inside `SymmetricFactor`, as a runtime arm beside the platform session.** There is
no two-way choice at the four consumers. Five reasons:

1. **One place honours the contract.** The identity triple, `share()`/`adopt()`, `InertiaEvidence`,
   `Counters` and `solve_partial` are specified once, on one class. A consumer-side seam would make
   each of four consumers re-implement them, and two of them (`KktFactor`, `KktFactorization`) hold
   the factor by value with mirror state (`analyzed`, `analyzed_pattern`). The `kkt_calls.h`
   invariant comment shows how fragile that mirror already is.
2. **The fallback needs both arms in one object** (§6). An exactly singular dense factorization is
   re-taken by the platform session within the same `factorize()` call. Only the factor sees both.
3. **The consumers stay unchanged** except for configuration (§5.7). The walk's `inertia_verdict`,
   the IPM's inertia ladder and `ws_independence` read `InertiaEvidence` exactly as today.
4. **Precedent:** the contract already carries backend-varying capabilities
   (`supports_partial_solve()`, false on Accelerate).
5. **Neutrality is easy to state.** With `kNever` the arm pointer is null and every public entry point
   takes one predictable branch. The proof's ±0.5 % instruction bar (§14 N4) has ~4 orders of
   magnitude of headroom: a branch per call against ~39 k instructions per call.

**TU placement** (CLAUDE.md §5, §6):

| file | licence | content |
|---|---|---|
| `include/hven/detail/linear/dense_session.h` (new) | Apache-2.0 | `detail::DenseSession`: column-major n×n workspace, `ipiv`, the LAPACK workspace, the CSR→dense scatter map, session id, epoch, inertia counts, numerics-valid flag. Internal: no public header includes it except through the incomplete type |
| `src/linear/dense_session.cpp` (new) | Apache-2.0 | `analyze` (size, map, workspace query), `factorize` (zero triangle, scatter, `dsytrf_work`, allocation-free block walk), `solve` (`dsytrs`), platform `#if` for MKL LAPACKE vs `lapacke_shim.h` exactly as `dense_symmetric_factor.cpp` does |
| `src/linear/symmetric_factor_mkl.cpp`, `symmetric_factor_accelerate.cpp` (edited) | Apache-2.0 adapters | the dispatch: selection at `analyze()`, dense-then-fallback at `factorize()`, routing of solves, evidence, counters, identity |
| `include/hven/linear/symmetric_factor.h` (edited) | Apache-2.0 | two `Options` fields, two `Counters` fields, `FactorEvidence::dense_factor_entries`, `class DenseSession;` forward declaration, `std::shared_ptr<detail::DenseSession>` members on `SymmetricFactor` and `Factorization` |
| `include/hven/detail/linear/lapacke_shim.h` (edited) | Apache-2.0 | a caller-workspace `dsytrf` (lwork query + call) beside the existing allocating one |
| `pardiso_session.*`, `accelerate_session.*` | MPL-derived | **untouched** (brief §0, CLAUDE.md §6) |
| `dense_symmetric_factor.*` | Apache-2.0 | **untouched**. The Schur border stack's path stays byte-identical. See §8 for the decode helper |

The dense session is a separate class and does not reuse `DenseSymmetricFactor`, for three reasons:
- that class copies its input into its own buffer (a second n² copy after densification);
- LAPACKE allocates its workspace on every `dsytrf` call;
- `block_evidence()` allocates a vector.

Fixing those in place would change the Schur border's code path, which is walk-touching (CEN-R3).
DN-1 keeps that path byte-identical. The border's per-call allocation is registered as a follow-on
(§15 R8).

The dispatch is a few lines per platform adapter, repeated in both. Everything with substance lives
once, in `dense_session.cpp`. If the duplication grows past selection and routing, it moves into a
shared `src/linear/symmetric_factor_dispatch.cpp`. That is the implementer's call, proven by the
same flags diff.

## 3. The Options field and naming

| layer | name | type, default | source |
|---|---|---|---|
| public option | `IpmOptions::linear.dense_factorization`, `SqpOptions::linear.dense_factorization` | `DensePolicy`, `kNever` | AP-1 §4.4; landed RESERVED in AP-2 |
| public threshold | `IpmOptions::linear.max_dense_dimension`, `SqpOptions::linear.max_dense_dimension` | `std::optional<Index>`, empty | AP-2 B8 (renamed from AP-1's `dense_max_dimension`) |
| enum | `hven::linear::DensePolicy` (`dense_policy.h`) | `kNever`, `kAlways`, `kAuto` | landed |
| factor-layer selection | `SymmetricFactor::Options::dense_factorization` | `DensePolicy`, `kNever` | AP-1 |
| factor-layer threshold | `SymmetricFactor::Options::max_dense_dimension` | `std::optional<Index>`, empty | AP-1 named it `dense_max_dimension`; this page applies B8's `max_` rule so the two layers spell it alike |
| counter | `SymmetricFactor::Counters::dense_factorize_count` | `Index` | AP-1 |
| counter | `SymmetricFactor::Counters::dense_fallback_count` | `Index` | **new, owner Q1** |
| evidence | `FactorEvidence::dense_factor_entries` | `std::optional<Index>` | **new, owner Q1** |

The public name is the invariant (AP-1 §4.4). The seam is inside the factor, so the factor-layer
names sit on `SymmetricFactor::Options` and `SymmetricFactor::Counters`, as AP-1 anticipated.

Why the two new names exist:
- **`dense_fallback_count`** counts `factorize()` calls the dense arm attempted, found exactly
  singular, and handed to the platform session. Without it, "the arm took every factorization" and
  "the arm handed back k of them" are indistinguishable from the counters. That is the only
  observation that makes the fallback assertable (§13, §14 A3).
- **`dense_factor_entries`** is n(n+1)/2: the entries of L and D that `dsytrf` writes into the
  stored triangle. It is a real observable of the dense factorization. It cannot go into
  `factor_nonzeros`, because `FactorEvidence`'s rule is that no field ever carries more than one
  backend's meaning. It is needed because `KktFactorization::record()` *throws* when an MKL build's
  outcome lacks `factor_nonzeros` (`src/interior/kkt_factorization.cpp:171-174`). The IPM projection
  must learn a dense branch. `IpmResult::factor_memory` ("memory reported by the last factorization")
  then reports the dense entry count. Its unit is already per-backend: entries on MKL, bytes on
  Accelerate.

No other public name is added. In particular, `FactorizeOutcome` gains **no** arm tag: a dense-taken
outcome is identified by its evidence (`dense_factor_entries` present).

## 4. Selection and the size gate

**DN-1 ships two policies.** C262 input 5 says to follow the brief's split ("Selection: an explicit
option, default OFF until DN-3"; DN-3 chooses `(n_kkt, nnz/n_kkt²)`), and this does:

| policy | DN-1 behaviour |
|---|---|
| `kNever` (default) | sparse, byte-identical to today (§14 N) |
| `kAlways` | dense for every analyzed pattern, at any n. The user's explicit choice: memory 8n² bytes, time O(n³/3) per factorization (§15 R4) |
| `kAuto` | **refused** by both engines' `validate()` until DN-3, as today, and `max_dense_dimension` stays refused unless empty |

The gate:
- **The decision is per analyzed pattern, at `analyze()`.** n is known there, and the dense session's
  buffers are sized there. A pattern keeps its arm for its whole life; the next `analyze()`
  re-decides. The IPM analyzes once per solve. The SQP analyzes once per pattern (HS84: 55 patterns
  over 85 factorizations), so its selection follows the KKT size as it changes.
- **The factor layer already implements `kAuto`'s mechanics** (dense iff n ≤ `max_dense_dimension`;
  `kAuto` with an empty threshold throws at construction, because the factor layer has no library
  rule). That keeps DN-3 to a rule, a default and an engine-side acceptance, with no factor-layer
  change. The mechanics are tested at the factor layer only (§13).
- **The threshold coordinate is `n_kkt` alone.** Density is recorded for DN-3 and unused here: DN-0's
  replayed matrices span density 1.0 to 0.011, and `rho` is ordered by n (§1.2, [A]).
- **DN-3 reads this page's cumulative tables** (`results.txt` [B], [C2b]) for its threshold rule. The
  brief's DN-3 rule (the largest n at which every replayed matrix of that size or smaller has
  `rho < 1`) gives n = 105 for the SQP and 167 for the IPM on DN-0's replay. The first matrix with
  `rho ≥ 1` is at n = 106 (SQP) and 168 (IPM). That `rho` counts a fresh analysis per matrix. Whether
  DN-3 should read the engine-weighted ratio instead is DN-3's to declare.

## 5. How the dense arm honours the factor contract

### 5.1 The identity triple `(pattern_hash, session_id, epoch)`

- `analyze()` on a dense-selected pattern creates a fresh `DenseSession` with:
  - a new process-unique id from the same allocator (`detail/linear/session_id.h`), so ids never
    collide across arms;
  - an epoch that continues the engine's sequence.

  Outstanding handles keep the old session alive and solvable. This is today's fork rule, unchanged.
- **The engine's epoch is engine-wide and monotone across arms.** Each successful factorization,
  whichever arm takes it, gets the next epoch. The dense session's epoch is set by the Apache-2.0
  adapter (our code). The platform session's epoch lives in the MPL file, is seeded only at
  construction (`FactorSession(cfg, initial_epoch)`) and is not touched. The fallback therefore uses
  one rule: **reuse the fallback session iff the engine's previous factorization was also taken by
  it** (its epoch is then the engine's); otherwise **fork** a fresh platform session, analyzed on the
  current pattern and seeded with the engine's epoch.
  - The epoch never runs backwards.
  - Every `(session_id, epoch)` pair names one set of numerics.
  - No MPL file changes.

  Cost on DN-0's replay window: 14 forks in 7 cells, 1.8 M instructions over the whole population
  [D3]. BT8's 10 fallbacks are consecutive and fork once.
- `session_id()` reports the session that took the most recent successful factorization. A
  consumer keyed on the triple, like `qp_engine.cpp:599`'s hot-start condition, sees a different id
  after an arm switch and declines reuse. That is the conservative reading, and it is correct.

### 5.2 `share()` / `adopt()`

**Supported.** `Factorization` gains a `std::shared_ptr<detail::DenseSession>` beside its platform
session pointer. Exactly one is non-null, fixed at emission.

| situation | behaviour |
|---|---|
| handle solve | reflects the session's current numerics; staleness detected by the triple, as today |
| a co-owner's dense attempt finds the matrix exactly singular | that attempt has overwritten the dense session's buffer in place, so the session has **no usable numerics** until the next dense success. A handle on it is refused with `hven::BackendError`: today's rule for "a session a co-owner's failed `factorize()` left without numerics". The fallback's numerics live in the platform session and are not reachable through a dense handle |
| `adopt()` of a dense handle | the engine drives the dense session, with its `Options` taken from the session (the dense session stores them), as today |

No library code calls `share()` or `adopt()` today (`grep` over `src/`); the contract is kept for
tycho and tests. Supporting it costs one pointer per handle, and refusing it would be a regression
in the factor's contract.

### 5.3 `InertiaEvidence` mapping

On a factorization **taken by the dense arm** (dsytrf `info == 0`):

| field | value | why |
|---|---|---|
| `state` | `kObserved` | the counts are read off the completed factorization |
| `positive`, `negative` | counted from D's 1×1 and 2×2 blocks (Sylvester's law: inertia(A) = inertia(D)), with the 2×2 trace/determinant decode `block_evidence()` uses | the dense arm's real observation |
| `zero` | `0`, **counted, not derived** | `info == 0` means no exactly zero 1×1 pivot, and every 2×2 block BK accepts is nonsingular, so the block walk finds no zero eigenvalue |
| `zero_is_derived` | `false` | all three classes are counted from D (the MKL sparse path derives zero as dim − p − n) |
| `perturbed_pivots` | **ABSENT** (`std::nullopt`) | Bunch-Kaufman never perturbs a pivot. Zero would claim "counted and found none", which is a statement about a mechanism that does not exist here |

On a factorization **taken by the fallback** (§6): the platform session's `InertiaEvidence`
verbatim, including `perturbed_pivots` present on MKL and absent on Accelerate.

Consequences, read against each consumer:
- **SQP walk, `inertia_verdict` (`qp_engine.h:1107`).** A dense-taken outcome never trips the
  perturbed rule or the short-sum rule, so it reads kOk or a *trusted* kWrong. A fallback outcome
  reads exactly as today (kSuspect on perturbed pivots). §6 and §7 argue why this is correct and
  what it changes.
- **IPM ladder (`ipm_solver.cpp:1917-1931`).** It reads `neigs()`, `peigs()` and `kkt_dim_`: the
  same integers. `ppivs()` reads 0, as it does on Accelerate (the projection's `value_or(0)`).
  `Singular()` is false on a dense-taken outcome, exactly as on a Pardiso-perturbed one, whose
  fabricated sign also makes the counts sum to n.
- **IPQP (`ipqp_engine.cpp:76-88`).** The same verdict structure as the walk: the same reading.
- **`ws_independence.cpp:961`.** It treats a present nonzero `perturbed_pivots` as untrustworthy;
  absent is not a trigger. The same reading as Accelerate.

### 5.4 `FactorEvidence`

| field | dense-taken outcome |
|---|---|
| `factor_nonzeros` (Pardiso iparm[17]) | absent |
| `factor_mflops` (Pardiso iparm[18]) | absent |
| `factor_size_bytes` (Accelerate) | absent |
| `dense_factor_entries` (new) | n(n+1)/2 |

`KktFactorization::record()` gains the dense branch (`factor_mem_ = dense_factor_entries`,
`factor_flops_ = 0` under the existing `> 0` print guard). This is one of the two consumer-side code
changes; the other is the options plumbing (§5.7).

### 5.5 `Counters`

| counter | dense-arm meaning |
|---|---|
| `analyze_count` | +1 per completed `analyze()`, either arm. It counts the lifecycle call. A fallback fork's internal phase 11 is **not** counted here: it is implied one-for-one by `dense_fallback_count` on the factorizations that forked |
| `factorize_count` | +1 per `factorize()` that reached a backend, either arm. A fallback is one call |
| `dense_factorize_count` | +1 per `factorize()` the dense arm took (dsytrf `info == 0`) |
| `dense_fallback_count` | +1 per `factorize()` the dense arm found exactly singular and handed to the platform session |
| derived identity | `factorize_count = dense_factorize_count + dense_fallback_count + (sparse-selected factorizations)` |
| `solve_count`, `partial_solve_count` | unchanged meaning; they count calls that reached either backend |
| `pattern_verify_count` | unchanged. The dense arm runs the same guard (`kVerify` / `kAssumeAnalyzed`) |

The SQP's own `symbolic_analyses` counter (driven by `needs_analysis()`) is unaffected.

### 5.6 `solve_partial`, `SolveInfo`, threads

- **`supports_partial_solve()`** is false whenever the current numerics are dense-taken.
  `solve_partial()` on dense numerics **throws `std::logic_error`**: LAPACK `dsytrs` has no
  phase-split entry point, so there is no result to return. Misuse throws, by the contract's second
  property. No library caller uses partial solves (`schur_complement.h:51-57`).
- **`SolveInfo::refinement_steps`** is absent on a dense solve: no refinement mechanism exists.
  `max_refinement_steps` configures the fallback session only. The dense solve's backward error is
  already at roundoff (§1.3).
- **Threads.** `Options::threads` and `set_threads()` apply at call scope around `dsytrf`/`dsytrs`
  (`MklThreadScope`, as `DenseSymmetricFactor` does), and to the fallback session when one exists.
  On Apple the count is stored and applied to nothing (UNOBSERVED, §10).

### 5.7 Options validation and plumbing

At the factor layer (constructor, `std::invalid_argument` naming the conflict):
- `kAuto` with an empty `max_dense_dimension`, or a value < 1, throws.
- An engaged `max_dense_dimension` with `kNever` or `kAlways` throws (meaningless; the same rule as
  the engines' RESERVED check).
- **`conditional_reproducibility_threads > 0` with any policy other than `kNever` throws.** CNR is a
  per-session Pardiso guarantee. MKL's LAPACK has only the process-global `mkl_cbwr_set`, which the
  factor must never touch (`Options::threads`' rule). Silently dropping a reproducibility guarantee
  is exactly what the Accelerate-side throw already refuses.
- Every other Pardiso- or Accelerate-only knob (pivot perturbation, matching, scaling, pivot
  strategy, two-level, solve parallelism, mflops, zero tolerance) configures the **fallback session
  only**. Its existing per-backend validation is unchanged, and the dense arm documents that it
  ignores the knob.

Engine plumbing:
- **IPM:** `IpmLinearOptions::dense_factorization` → `KktFactorization::Options`.
- **SQP:** `SqpLinearOptions::dense_factorization` → `sqp_kkt_options(threads, policy)`, the single
  factory every `KktFactor` (walk K0, SSN) and `ws_independence` already use; and into the IPQP
  subproblem's `KktFactorization`.
- **`equality_basis.cpp` stays at `kNever` in DN-1.** Its two factors (Gram, certificate K) carry a
  rounding-error certificate built on Pardiso's perturbed-pivot evidence and its own pivot exponent
  (`equality_basis.cpp:205-234`, `:955-966`). Moving a certificate's evidence source is a separate
  argument, registered (brief change 5).
- Whether the IPM's option is accepted in DN-1 is owner Q2.

## 6. Policy when the dense factor is exactly singular (C259/C262 input 1)

**The cases.**
- DN-0: 25 of 5501 replayed matrices (0.45 %) are exactly singular under `dsytrf`, in 7 cells:
  - IPM: BT8 10/11, HS61 2, LEWISPOL 2, LSNNODOC 1;
  - SQP: BURKEHAN 6 (the 2×2 pattern n = 2, pos 2 / neg 0 after Pardiso's perturbation), HS92 3,
    HS88 1 [D3].
- Every one is Pardiso-perturbed, and every one is undecidable by the reference: genuinely singular
  to working precision [C2].

**The policy: per-factorization fallback to the platform session.** On `info > 0` the same
`factorize()` call takes the factorization through the platform session (fork or reuse per §5.1).
The returned `FactorizeOutcome` (status, backend code, `InertiaEvidence`, `FactorEvidence`) is that
session's, verbatim. `dense_fallback_count` increments. The next `factorize()` tries the dense arm
again.

Why this policy:
1. **The engines' behaviour on these matrices is exactly today's.** Pardiso perturbs and proceeds
   with `perturbed_pivots > 0`:
   - the walk reads kSuspect and solves, the suspect ladder answering as designed;
   - the IPM ladder reads the same perturbed counts it reads today;
   - the SQP's `factorize_checked()` never sees a new status (it throws on `kBackendError`, which a
     singular dense attempt would otherwise have to become).

   No fabricated evidence is needed: the evidence comes from the backend that produced the numerics.
2. **No contract change.** `FactorizeOutcome::Status` stays `{kOk, kBackendError}`. The alternative
   needs a third status ("completed, exactly singular, counts observed, not solvable"), and every
   consumer would then need a no-solve path. In the walk that path does not exist, and building it
   is a walk redesign (census, CEN-R3).
3. **Cheap.** 14 forks, 1.8 M instructions across the whole replay window [D3]. A sticky variant
   (stay sparse for the rest of the pattern) is simpler for identity but runs 122 later
   factorizations sparse, 111 of them dense-acceptable [D2]. Per-factorization fallback with §5.1's
   reuse rule keeps the identity contract without that loss.

**The IPM's inertia-correction loop.**
- A dense-taken factorization feeds `neigs()`/`peigs()` with the observed counts. They sum to n,
  `ppivs()` is 0, and the ladder's `Inertia()` test is unchanged.
- An exactly singular matrix goes to Pardiso, which perturbs it, so the ladder sees exactly today's
  counts.
- The rejected alternative for the IPM alone: report the exact zero class (dsytrf completes even at
  `info > 0`, so D is readable) and let `Singular()` trigger `delta_c`, as Ipopt does with MA57's
  singular flag. That is arguably better numerics. But it gives the two engines two singular
  policies, and it makes BT8 a like-for-unlike comparison for DN-3. It is registered (§15 R10), not
  built.

**The SQP walk's verdict and the perturbed-pivot evidence.**
- Fallback outcomes are byte-for-byte today's: kSuspect where Pardiso perturbed.
- Dense-taken outcomes carry no perturbation evidence, because none exists. Pardiso perturbs 9.2 %
  of SQP replayed matrices (259/2817); the dense arm takes 249 of them:
  - **245 are decidable**, and the dense count equals the reference on all 245 [C2c]. The walk's
    kSuspect there was a correct "I cannot tell" from a backend that fabricated a sign. The dense
    arm *can* tell, and the walk now reads kOk or a trusted kWrong;
  - **4 are undecidable** (HS90 1, SPANHYD 3). Here a trusted verdict rests on a sign that is noise.
    This is the residual risk (§15 R2): 4 of 2817 matrices (0.14 %).

**Fields the dense path cannot produce, stated:**
- `perturbed_pivots` (absent: no perturbation mechanism);
- `factor_nonzeros`, `factor_mflops`, `factor_size_bytes` (absent: Pardiso and Accelerate
  observables);
- `SolveInfo::refinement_steps` (absent: no refinement);
- partial-solve composability (`supports_partial_solve()` false; `solve_partial` refused).

Nothing is zero-filled.

## 7. Policy on inertia disagreement at selectable sizes (input 2)

**There is no threshold, no zero-class rule, and no agreement-driven fallback.** The dense arm
reports what Bunch-Kaufman observed. The evidence decides it:

1. **Against the reference, the dense arm is never the one that is wrong** on a decidable matrix:
   4779/4779 at every size, both engines [C2b]. The SQP size bins the ruling quotes (0.788 / 0.821 /
   0.598 above n = 32) measure Pardiso's unperturbed error, not the arm's. Cumulative Pardiso
   agreement with the reference on matrices it did not perturb (SQP) [C2b]:

   | n ≤ | 16 | 32 | 48 | 64 | 128 | all |
   |---|---|---|---|---|---|---|
   | agreement | 0.997 | 0.998 | 0.988 | 0.977 | 0.959 | 0.944 |

2. **A threshold zero class is rejected on numbers** [C3]. One way to imitate Pardiso's "perturbed"
   signal is to count D-block eigenvalues with |λ| ≤ 10^-k·max|a_ij| as zero, or to send such
   matrices to the fallback. At k = 8 (Pardiso's default exponent) that sends **46 % of IPM and
   38 % of SQP matrices** to the fallback (the SQP's 1e-8 regularization and the IPM's barrier
   scaling put legitimate pivots there), and the walk would see kSuspect on a third of its
   factorizations instead of 9 %. Even at k = 12 the SQP sends 4 %. It buys nothing in accuracy:
   on decidable matrices the arm already matches the reference at 100 % with no threshold at all.
3. **Where Pardiso is right and the dense arm wrong:** zero cases on decidable matrices. The only
   disagreements in the arm's disfavour are on undecidable matrices (666 IPM, 56 SQP), where neither
   arm's sign means anything.

So the policy is: trust the dense count, report it as evidence, and let the drivers keep their
verdicts. The behavioural consequence (fewer kSuspect reads in the SQP walk) is declared in the
proof (§14 S3), not hidden.

## 8. Workspace and allocation

| phase | work | allocation |
|---|---|---|
| `analyze()` (dense) | validate (the same boundary checks as today); capture the pattern hash; allocate n×n column-major `a_`, `ipiv_` (n), `work_` (lwork from a `dsytrf` workspace query, cached); build `scatter_` (one dense offset per stored CSR entry: row i, col j ≥ i → `j*n + i`, the upper triangle `uplo = 'U'` reads) | **here only** |
| `factorize()` | zero the stored upper triangle (n(n+1)/2 writes; dsytrf overwrote it last time); scatter the nnz values through `scatter_`; `LAPACKE_dsytrf_work` (MKL) or the shim's caller-workspace `dsytrf_` (Apple) with the cached `work_`; one allocation-free block walk producing (pos, neg) | none |
| `solve()` | copy RHS into X (contiguous) and `dsytrs` in place; a non-contiguous X goes through a scratch block held at analyze for one column, and multi-RHS beyond it allocates once, as `DenseSymmetricFactor::solve` does | none for contiguous X and single-RHS |
| fallback fork | Pardiso analyze + factorize | allocates (rare: 14 times on the window) |

- **The block walk.** `block_evidence()`'s 2×2 decode is reused through a shared inline detail helper
  only if `dense_symmetric_factor.o` stays byte-identical. Otherwise it is duplicated, with a test
  asserting both decode identical blocks to identical counts. Either way, `DenseSymmetricFactor`'s
  code path does not move.
- **Proven, not presumed.** A test-only global allocation counter (§13 T9) asserts zero heap
  allocations across `factorize()` + `solve()` on a warmed dense session.
- **Memory:** 8n² bytes plus lwork·8 (n = 406: 1.3 MB; n = 2000: 32 MB).
- **Outside DN-1's scope, registered:**
  - the consumers' own allocations, e.g. `solve_vec()` in `kkt_calls.h` (returns a fresh `Vec` per
    solve) and LAPACKE's per-call workspace in the Schur border stack;
  - they are part of the 0.09-0.12 residual [D1] (§15 R8).

## 9. Thread-safety

Unchanged contract (owner ruling 2026-09-11: solves are thread-safe, with no parallel-solve facility):
- Distinct engines on distinct threads share no mutable state. `DenseSession` holds no globals, and
  LAPACK is reentrant.
- `MklThreadScope` is thread-local and restores the caller's override.
- The session-id allocator is atomic.
- Solves on one engine and its co-owners are not internally synchronized. The caller serializes
  them, exactly as `symmetric_factor.h`'s THREAD SAFETY note says. The dense arm adds no lock and no
  parallel facility.
- The fallback session inherits the platform session's guarantees.

## 10. Accelerate (input 6)

- **What the arm uses on Apple.** Accelerate's own LAPACK (`dsytrf_`/`dsytrs_`, the classic Fortran
  ABI under `ACCELERATE_NEW_LAPACK`), reached through `hven/detail/linear/lapacke_shim.h`, as
  `DenseSymmetricFactor` does today. The shim gains a caller-workspace `dsytrf` (lwork query plus
  call), because today it allocates a `std::vector` per call. The fallback session is Accelerate's
  LDLᵀ (TPP): no `perturbed_pivots`, and a native zero class under `zeroTolerance`.
- **UNOBSERVED, every one of these:**
  - every Apple instruction count, `rho`, projected or measured saving, and timing;
  - the fallback rate;
  - whether Accelerate's `dsytrf` makes the same pivot choices as MKL's (on undecidable matrices,
    two BK implementations already differ on 23 [C1]);
  - inertia agreement on Apple;
  - whether Accelerate's LAPACK threads at small n, and what `set_threads` does (stored, applied to
    nothing).
- **What the macOS CI lane can observe:** the DN-1 test suite's pass/fail (§13), including status and
  objective on the engine fixtures. CI-observed values are recorded as such. Instruction counts are
  not available on that lane.
- DN-3 consequence (brief change 6): an auto-selection default on Apple needs Apple observation. Until
  then, DN-3's default is platform-scoped and stays `kNever` on Apple.

## 11. Licences (input 7)

- LAPACK `dsytrf`/`dsytrs` through MKL (already linked) and through Accelerate (a system
  framework).
  hven's backend-LA ruling is met: production linear algebra goes through `hven::linear`, on the
  vendor LAPACK.
- No new dependency, nothing vendored, no `notices/` entry. No GPL. No LGPL.
- `Eigen::LDLT` is not used: it has no Bunch-Kaufman pivoting for indefinite matrices
  (`schur_complement.h:21-28`), and Eigen factorizations are scaffolding only.
- Algorithmic references, papers only:
  - Bunch & Kaufman, *Math. Comp.* 31 (1977), the pivoting;
  - Ashcraft, Grimes & Lewis, *SIAM J. Matrix Anal. Appl.* 20 (1998), bounded BK (rook), the
    registered alternative R3.

  No solver source was read for this design.
- The analysis tools (numpy/scipy, BSD) ran in a scratch environment and are never linked.

## 12. The per-call overhead alternatives (input 8)

The DN-0 review concluded "dense arm first, by an order of magnitude". From [D1], as shares of SQP
solver instructions (median; HS / small-dense):

| lever | what it removes | saving |
|---|---|---|
| **dense arm** (init-excluded) | analyses; 62-71 % of each factorization; 86-90 % of each solve | **0.744 / 0.717** |
| F2 ceiling: solves cut to one per factorization | 27 of every ~28 solves | 0.787 / 0.760 |
| F2 at half the solves removed | half the solves | 0.408 / 0.393 |
| F2 at a third removed | a third of the solves | 0.272 / 0.262 |
| Pardiso call trimming, ceiling: every analysis after the first | repeated phase 11 | 0.023 / 0.039 |
| F2 (half) on top of the dense arm | half of the now-dense solves | 0.043 / 0.056 |

**Confirmed, against call trimming.** The dense arm is 18-32× larger. The per-call cost is inside
MKL (~32-38 k instructions for a solve at n = 2-16), and trimming can only remove calls, not that
cost. Solves are 90-95 % of SQP calls and every one is algorithmically required.

**Refuted, against F2, in its strong form.**
- **F2's ceiling equals the dense arm's.** Each Schur-border minor needs K0 solves: two per `solve`,
  one per `add_border` (`schur_complement.h:49-51`). One solve per factorization is not reachable.
- **A realistic F2 is 2-3× smaller than the dense arm.** It removes a third to a half of the solves:
  the partial-solve fast path is no help at small n, since each partial phase is its own Pardiso call
  with the same fixed overhead.

**Dense still comes first**, for two reasons:
1. It is the larger lever.
2. It divides F2's remaining value by ~7-10 (0.39-0.41 → 0.04-0.06), because it makes every solve
   ~7-10× cheaper. F2 re-measured after the arm lands is the right order: it may no longer repay its
   walk-touching census.

## 13. Test plan

Factor layer, `tests/linear/` (both platforms; Apple values CI-observed only):

| # | test |
|---|---|
| T1 | lifecycle under `kAlways`: analyze / factorize / solve (single and multi-RHS, zero columns, non-contiguous X) matches `kNever` to 1e-12 relative on a fixture set (diagonal mixed-sign, the `[[0,1],[1,0]]` 2×2-pivot case, a KKT system with a zero (2,2) block, HS071's KKT) |
| T2 | `InertiaEvidence` on dense-taken outcomes: `kObserved`, counts equal to a hand-derived inertia, `zero == 0`, `zero_is_derived == false`, `perturbed_pivots` absent; `FactorEvidence` with only `dense_factor_entries` present, equal to n(n+1)/2 |
| T3 | exactly singular → fallback: a real singular fixture (BURKEHAN's n = 2 matrix, BT8's first matrix from the DN-0 dumps) returns the platform session's outcome **field-for-field equal** to a `kNever` factor's on the same matrix; `dense_fallback_count` +1, `dense_factorize_count` unchanged |
| T4 | identity: epoch monotone across dense → fallback → fallback (reuse, no fork) → dense → fallback (fork); session ids distinct across arms; `analyze()` forks the dense session; outstanding handles keep solving |
| T5 | `share()`/`adopt()` on dense numerics: full reuse, epoch-mismatch refusal, refusal with `BackendError` after a co-owner's singular attempt, pattern mismatch at first factorize |
| T6 | `supports_partial_solve()` false on dense numerics; `solve_partial` throws `std::logic_error` |
| T7 | Options validation: `kAuto` without a threshold throws; a threshold with `kNever`/`kAlways` throws; CNR + dense throws; Pardiso-only knobs accepted and inert on dense-taken factorizations; the `kAuto` mechanics (n ≤ threshold → dense) |
| T8 | pattern guard under `kVerify` / `kAssumeAnalyzed`; `pattern_verify_count` as today |
| T9 | allocation-free hot path: a test-TU global `operator new` counter reads 0 across 100 `factorize()` + `solve()` calls on a warmed dense session |
| T10 | a fallback-session backend failure (the existing `HVEN_TESTING` factorize injector in `symmetric_factor_mkl.cpp`, docs/testing.md's convention) surfaces as `kBackendError` from a dense-selected engine. No new injection point; no change inside a derived file |
| T11 | the counter identity of §5.5 on a mixed run |

Engines:
- **T12:** IPM and SQP solve HS071 and a BT8-like singular fixture at `kAlways`, with status and
  objective equal to `kNever` within 1e-8.
- **T13:** an SQP walk fixture whose K0 Pardiso perturbs (kSuspect today) reads kOk under the dense
  arm. The verdict is pinned, so the behavioural change is a tested fact.
- **T14:** both `validate()`s accept `kNever`/`kAlways` (IPM per owner Q2) and still refuse `kAuto`
  and an engaged `max_dense_dimension`; the options dump and the AP-4 drift pin carry the field.

## 14. The proof plan, with pre-registerable criteria

All legs follow the M8 pattern: a declaration committed before the first leg; serial `perf stat`
for instruction counts; counters asserted; wall-clock informational.

**N. The sparse path is untouched** (brief; CLAUDE.md §5). The arm is compiled in, not selected:

| # | criterion |
|---|---|
| N1 | compile-flags diff: no existing TU's flags differ; only the new TU is added |
| N2 | 27-cell replay × three engines, byte-exact against the pre-DN-1 pin |
| N3 | 57-cell census against `2026-10-m7-close-corpus`, 0 mismatches. CEN-R3: walk-adjacent headers change |
| N4 | runtime gate: instructions within the ±0.5 % corpus bar |
| N5 | iparm byte-identical; `dense_symmetric_factor.o` byte-identical |
| N6 | suites Release and Debug, examples, install smoke on both front ends, export contract, golden rig |
| N7 | Accelerate pass A and the macOS CI lane green |

**A. Inertia agreement** (C262 input 3, with its declared alternative):

- **Instrument.** DN-0's 5501 dumped matrices, replayed through the DN-1 build's `SymmetricFactor`
  at `kAlways` (the real arm, fallback included) against:
  - DN-0's sparse columns;
  - a reference: MKL `dsyevd` in the instrument, decidable iff min|λ| > n·eps·max|λ|.
- **Population and sizes.** The full DN-0 TRAIN population at every size the arm is selected for.
  Under DN-1's `kAlways` that is every size, n ≤ 406. Every figure is also reported cumulatively by
  n ≤ N, for DN-3.

| # | criterion | DN-0 observed (DenseSymmetricFactor) |
|---|---|---|
| A1 | dense-arm negative count = reference on ≥ 99 % of reference-decidable matrices, pooled over both engines | 4779/4779 = 100 % |
| A2 | matrices where Pardiso (unperturbed) matches the reference and the arm does not: ≤ 0.1 % of decidable matrices | 0 |
| A3 | every matrix `dsytrf` finds exactly singular is taken by the fallback, with outcome and evidence field-for-field equal to the sparse replay's | 25/25 expected |
| A4 | the literal C262 form, pooled agreement with Pardiso on matrices Pardiso did not perturb: ≥ 99 % at n ≤ 32 (gating); reported, not gating, above | IPM 1.000, SQP 0.9975 at n ≤ 32; 0.9967 / 0.9453 at all sizes |
| A5 | implementation identity: the arm's negative count equals DN-0's dense column on 100 % of dense-taken matrices (same LAPACK routine, same triangle) | — |

Why A1 replaces the literal form above n = 32: on decidable matrices Pardiso's unperturbed count is
wrong on 2.3 % (n ≤ 64) to 5.6 % (all sizes) of SQP matrices, and the dense count is right on every
one of them (§1.2). A gate on agreement with Pardiso would fail the more accurate arm for being
accurate.

**S. Status and objective, arm forced** (brief), both engines (the IPM per owner Q2) on DN-0's
238 cells. The baseline is the same build at `kNever` (= the pre-DN-1 pin by N2):

| # | criterion |
|---|---|
| S1 | every baseline-success cell (optimal or acceptable) ends with the same status and objective within 1e-8·max(1, \|f\|) |
| S2 | every baseline non-success cell: the forced status is reported per cell. A move to success is reported with its objective and terminal KKT residuals. **Not gating; the settler rules** (brief change 2) |
| S3 | iterations, factorizations, `dense_factorize_count`, `dense_fallback_count` and the walk's kSuspect count reported per cell; every move declared |
| S4 | the 27-cell replay and the census with the arm forced: reported, not gated (a declared trajectory change, not a neutrality claim) |

**M. Measured savings, reported against the projection** (brief; informational, not gating):
- **Cells and baseline.** SQP median cells hs043 and HS84, plus population medians: serial
  `perf stat` instructions at `kAlways` against `kNever`.
- **Projections.** 0.769 / 0.735 raw and 0.744 / 0.717 init-excluded [D1].
- **Exclusion.** The process-once OpenMP init is excluded by an unwind-capable attribution (r1's gdb
  marker, or a CFI-built MKL-free leg), as C262 requires of DN-3. The raw reading is reported beside
  it.
- **IPM.** IPM numbers, if the IPM is wired, are reported and carry no weight.

Failure handling: an N or A failure stops DN-1 and goes to the settler. An S1 failure is reported
per cell with its kSuspect-conversion trace (S3), and the settler rules. One merged fix round, per
the brief.

## 15. Risks

| # | risk | evidence / mitigation |
|---|---|---|
| R1 | **the SQP walk moves.** ~9 % of SQP factorizations change from kSuspect to a trusted verdict (§6), and the suspect ladder, zero-multiplier probe and repair can fire differently | correct by the reference on 245/249; S1/S2/S3 make every move visible; the settler rules on S2 |
| R2 | **trusted noise.** 4 SQP matrices (0.14 %) are Pardiso-perturbed, reference-undecidable and dense-taken; there a trusted kWrong rests on a noise sign | registered follow-on: a dense-arm small-pivot *fact* (not a fabricated perturbation count) and a walk rule using it, built only if S1 attributes a failure to it |
| R3 | **BK element growth** (L unbounded in partial BK) | DN-0 residuals: dense max 2.4e-16 over 5476 matrices (§1.3); rook pivoting (`dsytrf_rook`, Ashcraft-Grimes-Lewis) registered as the fallback design |
| R4 | **`kAlways` at large n:** O(n²) memory, O(n³) time, no ceiling | explicit user choice, documented; `kAuto`'s threshold (DN-3) bounds the default path; on DN-0's replay the first matrix with `rho ≥ 1` is at n = 106 (SQP) and 168 (IPM) |
| R5 | **MKL threading inside dsytrf:** OpenMP init and region overhead | replay costs already include them; the init is excluded from the projection (§1.1) |
| R6 | **Apple** | UNOBSERVED, every value (§10) |
| R7 | **identity bookkeeping across two sessions** | §5.1's single rule; T4/T5 |
| R8 | **consumer-side allocation and the Schur border's LAPACKE workspace** keep part of the 0.09-0.12 residual | registered, outside DN-1 |
| R9 | **the replay covers the first 50 factorizations per cell**, so fallback and fork counts beyond that are unmeasured | S3 reports `dense_fallback_count` over whole solves |
| R10 | **the IPM's singular policy** is Pardiso's perturbation, not Ipopt-style singular evidence | registered alternative (§6); DN-3 or later |

## 16. Open questions for the owner

**Q1. Two new field names.** Approve two names the AP-1 record does not yet carry:
- a count of factorizations the dense path handed back to the sparse backend
  (`dense_fallback_count`);
- the dense factorization's size report (`dense_factor_entries`).

Also confirm the factor-layer size threshold is spelled `max_dense_dimension`, matching the public
option after the naming pass, rather than AP-1's `dense_max_dimension`. *Recommendation:* approve
all three.

**Q2. The dense option on the IPM in DN-1.** Should the IPM get the dense option now, default off and
proven alongside the SQP, or should DN-1 offer it on the SQP only and leave the IPM until DN-3 has
measured it properly? *Recommendation:* offer it on both. The seam is shared, and the only IPM change
is one reporting branch. DN-3 needs the IPM wired to measure it at all. The IPM's default stays
sparse either way.

## 17. What should change in the brief

1. **DN-1's agreement clause** (and C262 input 3): measure against an exact-eigenvalue reference
   (§14 A1-A2), with the Pardiso-agreement form gated at n ≤ 32 and reported above (A4). At larger
   SQP sizes Pardiso is the inaccurate arm.
2. **DN-1's status proof.** "Every cell keeps its status" should become S1 (success cells keep status
   and objective) plus S2 (non-success moves reported; the settler rules). The arm removes ~9 % of
   the SQP walk's kSuspect reads, so failing cells may legitimately move.
3. **C262's "≈ 5 % of one call"** reads ≈ 4-6 % of a fresh analyze+factorize+solve. Against an
   average engine call it is ≈ 17-27 %; the engine-weighted ratio (0.11-0.14) is the projecting one.
4. **r1 note 6 ("the SQP is unaffected")**: the SQP pays the same process-once OpenMP init through
   MKL's first call (median 10 % HS, 4 % small-dense of solver instructions). It does not change
   DN-1. DN-3's init-exclusion requirement should name both engines.
5. **The four consumers become three.** `equality_basis` stays sparse in DN-1 (§5.7). Its
   certificate's evidence source is a separate registered item.
6. **DN-3 on Apple:** the auto-selection default is platform-scoped and stays `kNever` on Accelerate
   until Apple values are observed.
7. **The 20 % threshold's reading:** DN-1's measured-savings report compares against the
   engine-weighted, init-excluded projection (§14 M), not against DN-0's isolated `rho`.

---

## Appendix A. Binding inputs → answers

| input (C259/C262) | answer | where |
|---|---|---|
| 1. exactly singular vs Pardiso perturbing; IPM ladder; SQP verdict; fields not produced | per-factorization fallback to the platform session, evidence verbatim; ladder and walk unchanged on those matrices; dense-taken outcomes trusted (correct on 245/249 perturbed SQP matrices); absent fields listed | §6, §5.3 |
| 2. inertia disagreement at selectable sizes | no threshold. Against the reference the arm is right on 100 % of decidable matrices; the large-n disagreement is Pardiso's error; a threshold would send 38-46 % to the fallback | §7, §1.2 |
| 3. the proof's agreement criterion | A1 (≥ 99 % vs reference; observed 100 %), A2, A3, A4 (the literal form, gating at n ≤ 32: 1.000 / 0.9975), A5 | §14 A |
| 4. the IPM carries no projected-saving weight | IPM numbers informational only; the SQP case is restated init-excluded (0.744 / 0.717); the SQP's own init found and excluded | §1.1, §14 M |
| 5. size gate; DN-1 vs DN-3 | `kNever`/`kAlways` in DN-1, decided per pattern at `analyze()`; `kAuto` mechanics at the factor layer, refused by the engines until DN-3 | §4 |
| 6. Accelerate | Accelerate LAPACK via the shim (plus a workspace entry); every Apple figure UNOBSERVED; DN-3 platform-scoped | §10 |
| 7. licences | MKL / Accelerate LAPACK only; nothing vendored; no GPL/LGPL | §11 |
| 8. per-call alternatives | order of magnitude confirmed against call trimming; refuted against F2 (2-3× realistic, equal ceiling); dense first because it is larger and shrinks F2 ~7-10× | §12 |
