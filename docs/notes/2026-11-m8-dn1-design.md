# M8 DN-1: the dense arm behind the factor layer (design)

Date: 2026-11-08. Author: the DN-1 design lane. Status: DRAFT r1, revised on the settler's ruling on
the design review (APPROVE WITH CHANGES: R1-R8, S1, S3, S4, S5 applied; the record is Appendix B).
Base: `m8` at `44af1c16d`. No code in this round.

Authority:
- the M8 brief, `docs/notes/2026-11-m8-brief.md`, §DN (`:218-300`);
- the AP-1 review record, `docs/notes/2026-11-m8-ap1-api-review.md` §4.4, as renamed by AP-2 B8
  (`docs/notes/2026-11-m8-ap2-b8-naming.md:445-447`);
- the settler's rulings C259 and C262. Their eight binding inputs are answered in §§2-12; the map
  is in Appendix A. C259's agreement input is amended by the settler on this review (§14 A, §17).

Evidence:
- DN-0, `docs/notes/data/2026-11-m8-dn0/`;
- DN-0 r1, `docs/notes/data/2026-11-m8-dn0-ipm-r1/`;
- this page's own analysis over DN-0's raw replay data, `docs/notes/data/2026-11-m8-dn1-design/`:
  `dn1_design_analysis.py`, `results.txt`, `matrices.csv.gz`, `PROVENANCE.txt`. Part E of
  `results.txt` is the review round's: MKL 2026.2's own `dsytrf` and `dsytrf_rook` (the version DN-0
  ran, `MKL_NUM_THREADS=1`) and a congruence-scaled eigenvalue reference.

A citation of the form `[A]`, `[C2b]`, `[D1]` or `[E3]` names a section of `results.txt`. Every
number is Linux x86-64 with MKL. Apple/Accelerate is UNOBSERVED throughout (§10).

---

## 0. Decisions at a glance

| topic | decision | section |
|---|---|---|
| seam | inside `SymmetricFactor`: a runtime dense arm beside the platform session, chosen at `analyze()` | §2 |
| TU placement | new Apache-2.0 files `include/hven/detail/linear/dense_session.h` + `src/linear/dense_session.cpp`; a few lines of dispatch in each platform adapter (`symmetric_factor_mkl.cpp`, `symmetric_factor_accelerate.cpp`); the MPL-derived session files are untouched | §2 |
| factor-layer fields | `SymmetricFactor::Options::dense_factorization` (`DensePolicy`, default `kNever`) and `SymmetricFactor::Options::max_dense_dimension` (`std::optional<Index>`, `kAuto` only) | §3 |
| new names (owner Q1) | `Counters::dense_fallback_count`; `FactorEvidence::dense_factor_entries` (a size report); `FactorizeOutcome::arm` (`FactorArm`); `QpCounters::inertia_suspect_verdicts` and `inertia_wrong_verdicts` | §3 |
| selection | DN-1: `kNever` (default) and `kAlways`. `kAuto` and its threshold stay refused by both engines' `validate()` until DN-3. `kAlways` documents its 8n² bytes and validates n·n against overflow | §4 |
| exactly singular | per-factorization **fallback to the platform session**: `dsytrf` `info > 0`, a 2×2 block whose computed determinant is exactly 0.0, or any non-finite value in D. The outcome and evidence are that backend's, verbatim | §6 |
| block walk | 2×2 signs from the **determinant and trace**, not from `block_evidence()`'s `(trace ± disc)/2`. That form miscounts 5 decidable IPM matrices by 6 negatives each | §5.3, §8 |
| near-singular | no threshold. The dense arm reports Bunch-Kaufman inertia as observed, with `perturbed_pivots` ABSENT. A threshold zero class would send 38-46 % of matrices to the fallback | §6, §7 |
| agreement proof | gated **per engine** against an exact eigenvalue reference **and** a congruence-scaled one (their decidable union), expected value 100 %. Literal agreement with an unperturbed Pardiso is **reported** at every size cut, not gated (settler's amendment of C259) | §7, §14 A |
| routine | `dsytrf` (Bunch-Kaufman) kept. `dsytrf_rook` gives the same inertia on every decidable matrix; it bounds element growth. Its column is pre-registered in the A-leg | §7, §14 A6 |
| allocation | every buffer is sized at `analyze()`; `factorize()`/`solve()` allocate nothing (LAPACK `_work` entry points) | §8 |
| Accelerate | Accelerate's LAPACK through `lapacke_shim.h`, extended with a caller-workspace `dsytrf`. Every Apple figure UNOBSERVED | §10 |
| licences | LAPACK through MKL / Accelerate only. No new dependency, nothing vendored, no `notices/` entry | §11 |
| per-call alternatives | dense first, confirmed. Against Pardiso call trimming the gap is an order of magnitude (0.72-0.74 vs ≤ 0.04). Against F2 the "order of magnitude" is **refuted**: a realistic F2 saves 0.26-0.41 (2-3x less), its ceiling equals the dense arm's, and after the dense arm it is worth 0.04-0.06 | §12 |
| measured saving | **gates**: ≥ 0.20 of solver instructions, init-excluded, on hs043 and HS84, serial `perf stat`, at the default thread setting and at `MKL_NUM_THREADS=1` | §14 M |
| forced-arm replay and census (S4) | **no cell qualifies**: every census and replay cell is F7 with n_kkt ≥ 2,250, above the declared n_kkt cap of 2,000 | §14 S4 |

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
init-excluded figure. The projection is informational; the measured saving is what gates (§14 M).

**The IPM** carries no projected-saving weight (C262 input 4). For the record only: on r1's
init-excluded reading the same projection gives 0.339 (HS) and 0.243 (small-dense) [D1]. These
numbers are informational and are not an input to any decision here.

### 1.2 Inertia

The ruling's agreement table is the SQP small-dense population. This analysis reproduces it to
the third digit:
- agreement 0.984 / 0.982 / 1.000 / 0.788 / 0.821 / 0.598;
- `rho` 0.050 / 0.080 / 0.126 / 0.181 / 0.368 / 1.066 [A'].

Two references decide what those numbers mean:
- **The unscaled reference:** `eigvalsh` (LAPACK `?syevd`) on the densified matrix A.
- **The congruence-scaled reference (review R4):** `eigvalsh(D·A·D)` with D = |a_ii|^-1/2, rounded
  to a power of two so every product is exact, and D = 1 on a zero diagonal. Inertia is invariant
  under congruence, so inertia(DAD) = inertia(A) with no rounding at all. No scaled entry left the
  normal range on any of the 5501 matrices [E1].
- **Decidable** means min |λ| > s·n·eps·max |λ| under either reference, with safety factor s = 1,
  10 or 100. Where both references decide, they agree on all 5501 matrices [E1].

| decidable matrices [E2] | s = 1 | s = 10 | s = 100 |
|---|---|---|---|
| IPM (2684): unscaled / scaled / **union** / newly decidable | 2018 / 2574 / **2575** / 557 | 1900 / 2555 / **2556** / 656 | 1807 / 2528 / **2531** / 724 |
| SQP (2817): unscaled / scaled / **union** / newly decidable | 2761 / 2695 / **2764** / 3 | 2727 / 2650 / **2741** / 14 | 2673 / 2582 / **2692** / 19 |

Scaling makes the barrier-scaled IPM matrices decidable: undecidable IPM matrices fall from 666 to
109 at s = 1.

On the union, at every safety factor [E3]:
- **The dense arm (MKL `dsytrf`, signs of each 2×2 block from its determinant and trace)** matches
  the reference's negative count on **100 %** of decidable matrices, per engine: IPM 2575/2575
  (s = 1), 2556/2556, 2531/2531; SQP 2764/2764, 2741/2741, 2692/2692. On the newly decidable sets:
  IPM 557/557, 656/656, 724/724; SQP 3/3, 14/14, 19/19.
- **A finding the first round missed: DN-0's dense column is wrong on 5 decidable IPM matrices.**
  DN-0 read `dsytrf`'s D through `block_evidence()`'s 2×2 decode, e1,2 = (trace ± disc)/2. When a 2×2
  block has det < 0 and |det| ≪ trace², `trace − disc` cancels to zero or the wrong sign, and the
  negative eigenvalue goes uncounted. That decode and the det/trace decode differ on 14 matrices,
  all LAKES (n = 168), each by 6 negatives [E3d]. 5 of them (mat_0011-0015) are decidable under the
  scaled reference; there the det/trace count equals the reference and Pardiso's, and the e1/e2
  count is 6 short. The other 9 are undecidable. `dsytrf` itself is right on all 14. The new
  session uses the det/trace decode (§5.3), and DN-0's `block_evidence()` decode stays where it is,
  registered (§15 R11).
- **Pardiso** matches on 2557/2575 (IPM) and 2602/2764 (SQP); on the matrices it did not perturb,
  2404/2405 and 2379/2519 [E3].
- **Every one of the 140 SQP unperturbed disagreements** (all at n ≥ 9, 135 of them at n ≥ 33) is a
  case where the dense count equals the reference and Pardiso's does not [C2]. Example: MESH, n =
  77, Pardiso 42 negatives, dense and reference 40, min |λ| = 5e-9·max|a|. The eigenvalues near the
  SQP's 1e-8 regularization are where Pardiso's restricted (supernodal) pivoting misreads signs.
- **No matrix anywhere** has Pardiso (unperturbed) right and the dense arm wrong: A2's count is 0 at
  every safety factor, both engines [E3].
- **Undecidable under both references** at s = 1: 109 IPM and 53 SQP. Neither arm's sign decision
  there means anything.

Who perturbs, and what the dense arm says there [C2c, E6]:

| | Pardiso perturbed | dense exactly singular (fallback) | dense-taken, undecidable (union, s = 1) |
|---|---|---|---|
| IPM | 264 | 15 | 79 |
| SQP | 259 | 10 | 4 |

**What is new in a trusted read (review S5).** The walk reads a trusted verdict today on every
matrix Pardiso does not perturb, decidable or not. Of the 46 SQP matrices that are undecidable
under the unscaled reference and that the dense arm takes, **42 are already read trusted today**
through an unperturbed Pardiso. Only **4** (HS90 1, SPANHYD 3; 0.14 % of 2817) are new trusted
reads, made on a sign that is noise. Under the union it is 39 of 43, and the same 4 [E6]. On the
IPM, the dense arm takes 94 union-undecidable matrices, 79 of them Pardiso-perturbed (2.9 % of
2684). The IPM is default-off in DN-1, and DN-3 measures it.

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
above max(1e-10, 100× sparse). The dense residual holds although Bunch-Kaufman's L is unbounded on
these matrices: max |L| reaches 3.6e12 (IPM) and 9e5 (SQP), q99 2.7e8 and 7.4e5 [E7].

### 1.4 How small the dense pivots get (review R6)

min |D-block eigenvalue| / max |a_ij| over every dense-taken factorization [E5]:

| `dsytrf` | taken | q01 | q05 | q25 | median | ≤ 1e-8 | ≤ 1e-12 | ≤ 1e-14 |
|---|---|---|---|---|---|---|---|---|
| SQP, all | 2807 | 1.8e-14 | 3.2e-12 | 4.4e-10 | 6.6e-6 | 1054 | 112 | 3 |
| SQP, decidable (union) | 2764 | 2.8e-14 | 3.3e-12 | 4.4e-10 | 9.9e-6 | 1011 | 69 | 2 |
| SQP, undecidable | 43 | 5.6e-35 | 1.6e-14 | 1.8e-14 | 1.8e-14 | 43 | 43 | 1 |
| IPM, all | 2669 | 2.8e-37 | 7.7e-25 | 3.1e-14 | 3.9e-8 | 1228 | 837 | 611 |
| IPM, decidable (union) | 2575 | 9.1e-31 | 9.0e-22 | 8.0e-14 | 7.0e-8 | 1134 | 743 | 517 |
| IPM, undecidable | 94 | 2.3e-44 | 2.6e-43 | 4.0e-38 | 2.8e-26 | 94 | 94 | 94 |

Reading:
- **SQP.** Every undecidable matrix has its smallest pivot ≤ 1e-12·max|a|, and so do only 69 of
  2764 decidable ones. A small-pivot *fact* at that cut would flag all of R2's residual risk at the
  price of 69 false alarms. It is the evidence for R2's registered follow-on, not a DN-1 rule.
- **IPM.** The measure is not scale-invariant, and barrier scaling defeats it: 517 decidable IPM
  matrices have a pivot ≤ 1e-14·max|a|. Decidability for the IPM is a scaled-reference question.
- `dsytrf_rook`'s distribution is close to these: its counts at each cut are 0-8 % higher [E5].

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
3. **The consumers stay unchanged** except for configuration (§5.7) and the IPM's evidence
   projection (§5.4). The walk's `inertia_verdict`, the IPM's inertia ladder and `ws_independence`
   read `InertiaEvidence` exactly as today.
4. **Precedent:** the contract already carries backend-varying capabilities
   (`supports_partial_solve()`, false on Accelerate).
5. **Neutrality is easy to state.** With `kNever` the arm pointer is null and every public entry point
   takes one predictable branch. The proof's ±0.5 % instruction bar (§14 N4) has ~4 orders of
   magnitude of headroom: a branch per call against ~39 k instructions per call.

**TU placement** (CLAUDE.md §5, §6):

| file | licence | content |
|---|---|---|
| `include/hven/detail/linear/dense_session.h` (new) | Apache-2.0 | `detail::DenseSession`: column-major n×n workspace, `ipiv`, the LAPACK workspace, the CSR→dense scatter map, session id, epoch, inertia counts, numerics-valid flag. Internal: no public header includes it except through the incomplete type |
| `src/linear/dense_session.cpp` (new) | Apache-2.0 | `analyze` (size validation, map, workspace query), `factorize` (zero triangle, scatter, `dsytrf_work`, allocation-free det/trace block walk), `solve` (`dsytrs`), platform `#if` for MKL LAPACKE vs `lapacke_shim.h` exactly as `dense_symmetric_factor.cpp` does |
| `src/linear/symmetric_factor_mkl.cpp`, `symmetric_factor_accelerate.cpp` (edited) | Apache-2.0 adapters | the dispatch: selection at `analyze()`, dense-then-fallback at `factorize()`, routing of solves, evidence, the arm tag, counters, identity |
| `include/hven/linear/symmetric_factor.h` (edited) | Apache-2.0 | two `Options` fields, one new `Counters` field, `FactorEvidence::dense_factor_entries`, `FactorizeOutcome::arm` and its enum, `class DenseSession;` forward declaration, `std::shared_ptr<detail::DenseSession>` members on `SymmetricFactor` and `Factorization` |
| `include/hven/detail/linear/lapacke_shim.h` (edited) | Apache-2.0 | a caller-workspace `dsytrf` (lwork query + call) beside the existing allocating one |
| `include/hven/core/solver_counters.h` (edited) | Apache-2.0 | the two verdict counters (§3, owner Q1), on `QpCounters` and summed into the SQP's counters as `suspect_escalations` is |
| `pardiso_session.*`, `accelerate_session.*` | MPL-derived | **untouched** (brief §0, CLAUDE.md §6) |
| `dense_symmetric_factor.*` | Apache-2.0 | **untouched**. The Schur border stack's path stays byte-identical. See §8 |

The dense session is a separate class and does not reuse `DenseSymmetricFactor`, for four reasons:
- that class copies its input into its own buffer (a second n² copy after densification);
- LAPACKE allocates its workspace on every `dsytrf` call;
- `block_evidence()` allocates a vector;
- `block_evidence()`'s 2×2 decode miscounts (§1.2), and the dense arm needs the det/trace form.

Fixing those in place would change the Schur border's code path, which is walk-touching (CEN-R3).
DN-1 keeps that path byte-identical. The border's per-call allocation and its decode are registered
as follow-ons (§15 R8, R11).

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
| factor-layer threshold | `SymmetricFactor::Options::max_dense_dimension` | `std::optional<Index>`, empty | AP-1 named it `dense_max_dimension`; this page applies B8's `max_` rule so the two layers spell it alike (**owner Q1**) |
| counter | `SymmetricFactor::Counters::dense_factorize_count` | `Index` | AP-1 |
| counter | `SymmetricFactor::Counters::dense_fallback_count` | `Index` | **new, owner Q1** |
| evidence | `FactorEvidence::dense_factor_entries` | `std::optional<Index>` | **new, owner Q1** |
| arm tag | `FactorizeOutcome::arm`, of `enum class hven::linear::FactorArm { kSparse, kDense }` | `FactorArm`, `kSparse` | **new, owner Q1** (review S1) |
| walk verdicts | `QpCounters::inertia_suspect_verdicts`, `QpCounters::inertia_wrong_verdicts` | `Index` | **new, owner Q1** (review R3) |

The public name is the invariant (AP-1 §4.4). The seam is inside the factor, so the factor-layer
names sit on `SymmetricFactor::Options` and `SymmetricFactor::Counters`, as AP-1 anticipated.

Why each new name exists:
- **`dense_fallback_count`** counts `factorize()` calls the dense arm attempted, found exactly
  singular, and handed to the platform session. Without it, "the arm took every factorization" and
  "the arm handed back k of them" are indistinguishable from the counters. That is the only
  observation that makes the fallback assertable (§13, §14 A3).
- **`FactorizeOutcome::arm`** states which session produced the numerics, the evidence and the status
  of this outcome: `kDense` for a dense-taken factorization, `kSparse` for the platform session
  (Pardiso or Accelerate), including every fallback and every `kNever` factorization. A3 and A5
  attribute each matrix by it, and `KktFactorization::record()` branches on it (§5.4). Identifying
  the arm by which evidence fields happen to be present would make attribution depend on a
  side effect.
- **`dense_factor_entries`** is a **size report**, not a measurement: n(n+1)/2, the number of entries
  of L and D the dense session stores, computed from n. It is exact by construction, and it is all
  a dense factorization has to say about its size. It cannot go into `factor_nonzeros`, because
  `FactorEvidence`'s rule is that no field carries more than one backend's meaning. Its doc comment
  says "computed from the dimension, not reported by a backend".
  - The alternative was to make `KktFactorization::factor_mem_` absent-capable
    (`src/interior/kkt_factorization.cpp:171-174`). It is rejected because the value surfaces as
    `IpmResult::factor_memory`, a public `Index` (`include/hven/drivers/ipm_solver_types.h:154`), and through
    `analysis.factor_mem`. Making it optional is a public type change after the AP-2 break closed,
    for a quantity the dense arm can state exactly. `IpmResult::factor_memory` ("memory reported by
    the last factorization") reports the dense size; its unit is already per-backend (entries on
    MKL, bytes on Accelerate).
- **`inertia_suspect_verdicts` / `inertia_wrong_verdicts`** count the walk's main per-pass inertia
  verdict (the one produced at `qp_engine.cpp:835-844` and the `HVEN_TESTING` observer records as
  `inertia_verdict`) when it is kSuspect or kWrong. kOk is the remainder. The zero-multiplier probe's
  hypothetical re-entries and the `reread_wrong_inertia` reads are not counted here
  (`dep_inertia_rereads` and `dep_inertia_reread_flips` count those). Today this verdict is visible
  only under `HVEN_TESTING`, so S3 could not report it from the measured binary (review R3).
  - The alternative is to run S3's verdict histogram on an `HVEN_TESTING` build of the bench cell: a
    second binary, a second build, and the observer's per-pass record over 238 whole solves.
  - The counters cost one increment per pass. `census_compare.py` reads the first 13 columns and the
    replay CSV's schema does not carry `QpCounters`, so N2/N3 do not move. Any dump that enumerates
    `QpCounters` gains two declared fields.

No other public name is added.

## 4. Selection and the size gate

**DN-1 ships two policies.** C262 input 5 says to follow the brief's split ("Selection: an explicit
option, default OFF until DN-3"; DN-3 chooses `(n_kkt, nnz/n_kkt²)`), and this does:

| policy | DN-1 behaviour |
|---|---|
| `kNever` (default) | sparse, byte-identical to today (§14 N) |
| `kAlways` | dense for every analyzed pattern, at any n the platform can address. The user's explicit choice: memory 8n² bytes per analyzed pattern, time O(n³/3) per factorization (§15 R4) |
| `kAuto` | **refused** by both engines' `validate()` until DN-3, as today, and `max_dense_dimension` stays refused unless empty |

**`kAlways` boundary validation (review R8).** At `analyze()` on a dense-selected pattern, before
anything is allocated:
- n must fit LAPACK's integer (`lapack_int`, 32-bit under hven's LP64 MKL and on Accelerate);
- n·n·sizeof(double) must not overflow `Index` or `std::size_t`. The check is a division
  (n ≤ max / n / 8), never a multiplication that could itself overflow.

Either failure throws `std::invalid_argument` naming n and the byte figure it would need. The
allocation itself is not wrapped: `std::bad_alloc` surfaces unchanged as the boundary error. On Linux
with overcommit a large request can succeed and be killed later by the OOM killer instead. The
dense session therefore zeroes its buffer at `analyze()`, so any such failure lands there and not
mid-solve. The option's doc comment carries the figure: "`kAlways` allocates 8·n_kkt² bytes for each
analyzed pattern, and again for each session an outstanding handle keeps alive (n_kkt = 2,000:
32 MB; 10,000: 800 MB; 40,000: 12.8 GB)".

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
- **DN-3 reads this page's cumulative tables** (`results.txt` [B], [C2b], [E3c]) for its threshold
  rule. The brief's DN-3 rule (the largest n at which every replayed matrix of that size or smaller
  has `rho < 1`) gives n = 105 for the SQP and 167 for the IPM on DN-0's replay. The first matrix with
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

**The block walk (review R5).** After `dsytrf` returns `info == 0`, the session walks D:
- a 1×1 block d: positive if d > 0, negative if d < 0 (exactly 0.0 cannot occur: `info` would be
  > 0);
- a 2×2 block [a b; b c]: det = a·c − b·b, trace = a + c. det < 0 gives one positive and one
  negative; det > 0 gives two of trace's sign;
- **exact singularity, and the fallback, on any of:** a 2×2 det that is exactly 0.0; any non-finite
  value among D's entries, a det, or a trace. The factorization is then treated exactly like
  `info > 0` (§6).

So "zero = 0, counted" is literally true: a dense-taken factorization has walked every block and
found none of these. On DN-0's matrices neither case occurs: no 2×2 det is exactly 0.0 and no value
is non-finite in any of 5476 dense-taken factorizations [E4]. The rule exists so that the claim
holds by construction, not by observation.

The det/trace form replaces `block_evidence()`'s (trace ± disc)/2 form, which miscounts when
|det| ≪ trace² (14 LAKES matrices, 5 of them decidable, §1.2 [E3d]). Signs need no eigenvalues.

On a factorization **taken by the dense arm** (`arm == kDense`):

| field | value | why |
|---|---|---|
| `state` | `kObserved` | the counts are read off the completed factorization |
| `positive`, `negative` | counted from D's blocks by the walk above (Sylvester's law: inertia(A) = inertia(D)) | the dense arm's real observation |
| `zero` | `0`, **counted, not derived** | the walk found no zero 1×1 pivot (`info == 0`), no 2×2 det exactly 0.0 and no non-finite value; any of these goes to the fallback instead |
| `zero_is_derived` | `false` | all three classes are counted from D (the MKL sparse path derives zero as dim − p − n) |
| `perturbed_pivots` | **ABSENT** (`std::nullopt`) | Bunch-Kaufman never perturbs a pivot. Zero would claim "counted and found none", which is a statement about a mechanism that does not exist here |

On a factorization **taken by the fallback** (`arm == kSparse`, §6): the platform session's
`InertiaEvidence` verbatim, including `perturbed_pivots` present on MKL and absent on Accelerate.

Consequences, read against each consumer:
- **SQP walk, `inertia_verdict` (`qp_engine.h:1107`).** A dense-taken outcome never trips the
  perturbed rule or the short-sum rule, so it reads kOk or a *trusted* kWrong. A fallback outcome
  reads exactly as today (kSuspect on perturbed pivots). §6 and §7 argue why this is correct and
  what it changes.
- **The walk's `reread_wrong_inertia` at μ = 0 (`qp_engine.cpp:4186-4216`).** It re-reads a kWrong
  through a fresh local factor at μ = 0. Under R-C' a kSuspect at μ = 0 replaces the kWrong only if a
  second read at the pass's own μ is kOk. On Pardiso a near-singular face at μ = 0 reads kSuspect
  and goes through that confirmation. On the dense arm the same face reads a trusted kOk or kWrong,
  and **a kOk at μ = 0 flips the kWrong with no confirming read**. That is a behaviour change on the
  path R-C' was written to guard. T13 pins it, and S3 reports `dep_inertia_reread_flips` per cell.
  The local factor is built by `KktFactor(threads_)`, so it takes the engine's policy through the
  same factory (§5.7).
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
| `dense_factor_entries` (new) | n(n+1)/2, a size report (§3) |

`KktFactorization::record()` gains a branch on `outcome.arm == FactorArm::kDense`:
`factor_mem_ = *dense_factor_entries`, `factor_flops_ = 0` under the existing `> 0` print guard, and
the absence of `dense_factor_entries` on a dense outcome is a `std::logic_error` like the existing
per-backend ones. `factor_mem_` widens from `int` to `Index` (a detail member): n(n+1)/2 passes
2³¹ at n = 65,536, which `kAlways` permits. `IpmResult::factor_memory` is already `Index`. This is one
of the two consumer-side code changes; the other is the options plumbing (§5.7).

### 5.5 `Counters`

| counter | dense-arm meaning |
|---|---|
| `analyze_count` | +1 per completed `analyze()`, either arm. It counts the lifecycle call. A fallback fork's internal phase 11 is **not** counted here: it is implied one-for-one by `dense_fallback_count` on the factorizations that forked |
| `factorize_count` | +1 per `factorize()` that reached a backend, either arm. A fallback is one call |
| `dense_factorize_count` | +1 per `factorize()` the dense arm took (`arm == kDense`) |
| `dense_fallback_count` | +1 per `factorize()` the dense arm found exactly singular (§5.3's three conditions) and handed to the platform session |
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
  At the default (`threads = 0`, MKL's own default) MKL may fork OpenMP threads inside `dsytrf` at
  n ≈ 100-400. Whether that pays is measured, not assumed (§14 M). On Apple the count is stored and
  applied to nothing (UNOBSERVED, §10).

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
- At `analyze()`: §4's size validation.

Engine plumbing:
- **IPM:** `IpmLinearOptions::dense_factorization` → `KktFactorization::Options`.
- **SQP:** `SqpLinearOptions::dense_factorization` → `sqp_kkt_options(threads, policy)`, the single
  factory every `KktFactor` and `ws_independence` already use; and into the IPQP subproblem's
  `KktFactorization`. `KktFactor`'s constructor takes the policy beside `threads`. Its eight
  construction sites in `qp_engine.cpp` (two `kkt`, `fresh`, three `local` factors including
  `reread_wrong_inertia`'s, `own`, `shifted`), the engine's member K0 and the SSN's `kkt_` pass the
  engine's policy, so no factor is left behind on the old default.
- **The warm-start predictor's factor stays at `kNever` in DN-1** (`predictor.h`, its one
  `KktFactor`, built with an explicit `kNever`): the policy reaches the walk, SSN and IPQP factors
  only. `KktFactor` and `sqp_kkt_options()` take the policy with no default, so every construction
  site states it (review C270 finding 4).
- **`equality_basis.cpp` stays at `kNever` in DN-1.** Its two factors (Gram, certificate K) carry a
  rounding-error certificate built on Pardiso's perturbed-pivot evidence and its own pivot exponent
  (`equality_basis.cpp:205-234`, `:955-966`). Moving a certificate's evidence source is a separate
  argument, registered (brief change 5).
- The IPM is wired in DN-1, default off (owner Q3).

## 6. Policy when the dense factor is exactly singular (C259/C262 input 1)

**The cases.**
- DN-0: 25 of 5501 replayed matrices (0.45 %) are exactly singular under `dsytrf`, in 7 cells:
  - IPM: BT8 10/11, HS61 2, LEWISPOL 2, LSNNODOC 1;
  - SQP: BURKEHAN 6 (the 2×2 pattern n = 2, pos 2 / neg 0 after Pardiso's perturbation), HS92 3,
    HS88 1 [D3].
- Every one is Pardiso-perturbed, and every one is undecidable by both references: genuinely
  singular to working precision [C2, E3].
- The block walk's two added conditions (a 2×2 det exactly 0.0; a non-finite value) add no case on
  DN-0's matrices [E4].

**The policy: per-factorization fallback to the platform session.** On `info > 0`, or either of the
walk's two conditions (§5.3), the same `factorize()` call takes the factorization through the
platform session (fork or reuse per §5.1). The returned `FactorizeOutcome` (status, backend code,
`InertiaEvidence`, `FactorEvidence`) is that session's, verbatim, with `arm == kSparse`.
`dense_fallback_count` increments. The next `factorize()` tries the dense arm again.

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
    This is the residual risk (§15 R2): 4 of 2817 matrices (0.14 %). The other 42 undecidable
    matrices the arm takes are already read trusted today (§1.2).

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
   IPM 2575/2575 and SQP 2764/2764 on the union at s = 1, 100 % at s = 10 and 100 too [E3]. The SQP
   size bins the ruling quotes (0.788 / 0.821 / 0.598 above n = 32) measure Pardiso's unperturbed
   error, not the arm's. Cumulative Pardiso agreement with the reference on decidable matrices it
   did not perturb (SQP, union, s = 1) [E3c]:

   | n ≤ | 8 | 16 | 32 | 48 | 64 | 128 | all |
   |---|---|---|---|---|---|---|---|
   | agreement | 1.000 | 0.997 | 0.997 | 0.988 | 0.977 | 0.959 | 0.944 |

2. **A threshold zero class is rejected on numbers** [C3]. One way to imitate Pardiso's "perturbed"
   signal is to count D-block eigenvalues with |λ| ≤ 10^-k·max|a_ij| as zero, or to send such
   matrices to the fallback. At k = 8 (Pardiso's default exponent) that sends **46 % of IPM and
   38 % of SQP matrices** to the fallback (the SQP's 1e-8 regularization and the IPM's barrier
   scaling put legitimate pivots there; §1.4), and the walk would see kSuspect on a third of its
   factorizations instead of 9 %. Even at k = 12 the SQP sends 4 %. It buys nothing in accuracy:
   on decidable matrices the arm already matches the reference at 100 % with no threshold at all.
3. **Where Pardiso is right and the dense arm wrong:** zero cases on decidable matrices, at every
   safety factor, both engines (A2's count, [E3]). The only disagreements in the arm's disfavour
   are on undecidable matrices (109 IPM, 53 SQP under the union), where neither arm's sign means
   anything.
4. **Rook pivoting does not change the inertia** (review S3) [E3, E7]. MKL's `dsytrf_rook` gives the
   same negative count as `dsytrf` on every decidable matrix, both engines, at every safety factor.
   They differ on 10 IPM matrices, all undecidable. Rook finds 13 IPM matrices exactly singular
   where `dsytrf` finds 15 (SQP: 10 each), so it would save 2 fallbacks. What rook does change is
   element growth: max |L| ≤ 1.88 on every matrix, against `dsytrf`'s 3.6e12 (IPM) and 9e5 (SQP).
   DN-0's dense residuals are at roundoff despite that growth (§1.3). **Recommendation: keep
   `dsytrf` for DN-1.** Rook is not materially better on inertia, and its cost is unmeasured:
   - it needs `dsytrs_rook` for solves;
   - its pivot search may scan many columns per elimination step where `dsytrf` scans at most two,
     so the worst case adds O(n³) comparisons to the O(n³/3) flops;
   - whether Accelerate's LAPACK exports it is UNOBSERVED.

   The A-leg carries a rook column (A6), so a later switch can rest on measured instructions and
   residuals.

So the policy is: trust the dense count, report it as evidence, and let the drivers keep their
verdicts. The behavioural consequence (fewer kSuspect reads in the SQP walk) is declared in the
proof (§14 S3), not hidden.

## 8. Workspace and allocation

| phase | work | allocation |
|---|---|---|
| `analyze()` (dense) | validate (the same boundary checks as today, plus §4's size checks); capture the pattern hash; allocate and zero the n×n column-major `a_`, `ipiv_` (n), `work_` (lwork from a `dsytrf` workspace query, cached); build `scatter_` (one dense offset per stored CSR entry: row i, col j ≥ i → `j*n + i`, the upper triangle `uplo = 'U'` reads) | **here only** |
| `factorize()` | zero the stored upper triangle (n(n+1)/2 writes; dsytrf overwrote it last time); scatter the nnz values through `scatter_`; `LAPACKE_dsytrf_work` (MKL) or the shim's caller-workspace `dsytrf_` (Apple) with the cached `work_`; one allocation-free det/trace block walk producing (pos, neg) or the singular verdict | none |
| `solve()` | copy RHS into X (contiguous) and `dsytrs` in place; a non-contiguous X goes through a scratch block held at analyze for one column, and multi-RHS beyond it allocates once, as `DenseSymmetricFactor::solve` does | none for contiguous X and single-RHS |
| fallback fork | Pardiso analyze + factorize | allocates (rare: 14 times on the window) |

- **The block walk** is the session's own, in `dense_session.cpp` (§5.3). It does not share
  `block_evidence()`'s decode, which miscounts (§1.2), and `DenseSymmetricFactor`'s code path does not
  move. The walk is a free function in the detail namespace so T2b can drive it on synthetic blocks.
- **Proven, not presumed.** A test-only global allocation counter (§13 T9) asserts zero heap
  allocations across `factorize()` + `solve()` on a warmed dense session.
- **Memory:** 8n² bytes plus lwork·8 (n = 406: 1.3 MB; n = 2000: 32 MB), per analyzed pattern and
  per session an outstanding handle keeps alive.
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
  - whether Accelerate's LAPACK exports `dsytrf_rook`;
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
    pre-registered A6 column and registered alternative R3.

  No solver source was read for this design.
- The analysis tools ran in a scratch environment and are never linked: numpy/scipy (BSD), and
  MKL's own LAPACK reached through `ctypes` for part E.

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
| T2 | `InertiaEvidence` on dense-taken outcomes: `arm == kDense`, `kObserved`, counts equal to a hand-derived inertia, `zero == 0`, `zero_is_derived == false`, `perturbed_pivots` absent; `FactorEvidence` with only `dense_factor_entries` present, equal to n(n+1)/2. Includes LAKES mat_0013 from the DN-0 dumps (decidable, 78 negatives), where `block_evidence()`'s decode reads 72 |
| T2b | the block walk on synthetic D blocks: a 2×2 block with det < 0 and \|det\| ≪ trace² counts one negative; det exactly 0.0 → singular; NaN or ±Inf in an entry, a det or a trace → singular |
| T3 | exactly singular → fallback: a real singular fixture (BURKEHAN's n = 2 matrix, BT8's first matrix from the DN-0 dumps) returns the platform session's outcome **field-for-field equal** to a `kNever` factor's on the same matrix, with `arm == kSparse`; `dense_fallback_count` +1, `dense_factorize_count` unchanged |
| T4 | identity: epoch monotone across dense → fallback → fallback (reuse, no fork) → dense → fallback (fork); session ids distinct across arms; `analyze()` forks the dense session; outstanding handles keep solving |
| T5 | `share()`/`adopt()` on dense numerics: full reuse, epoch-mismatch refusal, refusal with `BackendError` after a co-owner's singular attempt, pattern mismatch at first factorize |
| T6 | `supports_partial_solve()` false on dense numerics; `solve_partial` throws `std::logic_error` |
| T7 | Options validation: `kAuto` without a threshold throws; a threshold with `kNever`/`kAlways` throws; CNR + dense throws; Pardiso-only knobs accepted and inert on dense-taken factorizations; the `kAuto` mechanics (n ≤ threshold → dense); §4's size check, driven through its detail helper at n beyond `lapack_int` and at n where n·n·8 overflows (a real pattern that size cannot be built in a test) |
| T8 | pattern guard under `kVerify` / `kAssumeAnalyzed`; `pattern_verify_count` as today |
| T9 | allocation-free hot path: a test-TU global `operator new` counter reads 0 across 100 `factorize()` + `solve()` calls on a warmed dense session |
| T10 | a fallback-session backend failure (the existing `HVEN_TESTING` factorize injector in `symmetric_factor_mkl.cpp`, docs/testing.md's convention) surfaces as `kBackendError` with `arm == kSparse` from a dense-selected engine. No new injection point; no change inside a derived file |
| T11 | the counter identity of §5.5 on a mixed run |

Engines:
- **T12:** IPM and SQP solve HS071 and a BT8-like singular fixture at `kAlways`, with status and
  objective equal to `kNever` within 1e-8.
- **T13:** two SQP walk fixtures, verdicts pinned so the behavioural change is a tested fact:
  - a K0 Pardiso perturbs (kSuspect today) reads kOk under the dense arm, and
    `inertia_suspect_verdicts` moves accordingly;
  - **the `reread_wrong_inertia` μ = 0 path** (`qp_engine.cpp:4186-4216`): a kWrong whose μ = 0
    eliminated face is near-singular, so Pardiso reads kSuspect and R-C' asks for the confirming read,
    while the dense arm reads a trusted verdict directly. The test pins both arms' outcome and
    `dep_inertia_reread_flips`.
- **T14:** both `validate()`s accept `kNever`/`kAlways` and still refuse `kAuto` and an engaged
  `max_dense_dimension`; the options dump and the AP-4 drift pin carry the field; the verdict
  counters are summed into the SQP's counters.

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

**A. Inertia agreement** (C262 input 3, as amended by the settler on this review):

- **Instrument.** DN-0's 5501 dumped matrices, replayed through the DN-1 build's `SymmetricFactor`
  at `kAlways` (the real arm, fallback included) at `MKL_NUM_THREADS=1`, against:
  - DN-0's sparse columns;
  - the unscaled reference: MKL `dsyevd` on A;
  - the congruence-scaled reference: MKL `dsyevd` on D·A·D, D = |a_ii|^-1/2 rounded to a power of
    two, D = 1 on a zero diagonal (§1.2).

  A matrix is decidable if either reference finds min |λ| > s·n·eps·max |λ|. The gate uses s = 1;
  s = 10 and s = 100 are reported beside it.
- **Population and sizes.** The full DN-0 TRAIN population at every size the arm is selected for.
  Under DN-1's `kAlways` that is every size, n ≤ 406. Every figure is also reported cumulatively by
  n ≤ N, for DN-3.
- **The arm's inertia at the default thread setting** is reported beside the gated run: the
  negative counts must be identical, or the difference is listed.
- **Known outcomes, said plainly.** A1 and A2 on DN-0's own population are known before the leg
  runs. The arm runs the same routine on the same matrices (A5), and this page has measured both
  [E3]. On the real build they are a regression check. The new information is in A3 (the fallback,
  field for field, on the real factor) and A5 (implementation identity, including the decode).

| # | criterion | gate | expected (this page's analysis) |
|---|---|---|---|
| A1-IPM | dense-arm negative count = reference on the IPM's decidable union (s = 1), dense-taken matrices | = 100 % | 2575/2575 = **100 %** [E3] |
| A1-SQP | the same, SQP | = 100 % | 2764/2764 = **100 %** [E3] |
| A2-IPM | matrices where Pardiso (unperturbed) matches the reference and the arm does not, IPM union | = 0 | **0** [E3] |
| A2-SQP | the same, SQP | = 0 | **0** [E3] |
| A3 | every matrix the arm finds exactly singular (`info > 0`, a 2×2 det of 0.0, or a non-finite value) is taken by the fallback, `arm == kSparse`, with outcome and evidence field-for-field equal to the sparse replay's | all | 25/25 [D3, E4] |
| A4 | literal agreement with Pardiso on matrices Pardiso did not perturb, per engine, at every size cut n ≤ 8, 16, 24, 32, 48, 64, 96, 128, all | **reported, not gated** | IPM 1.000 up to n ≤ 32, 0.9983 at all sizes; SQP 1.000 / 0.9973 / 0.9975 at n ≤ 8 / 16 / 32, 0.9774 at 64, 0.9600 at 128, 0.9453 at all sizes [E3c] |
| A5 | implementation identity: on every dense-taken matrix the arm's negative count equals MKL `dsytrf` read through the det/trace walk. Against DN-0's dense column (the e1/e2 decode) it differs on exactly the 14 LAKES matrices of [E3d], by +6 each, and nowhere else | all | 5462 equal + 14 declared [E1, E3d] |
| A6 | `dsytrf_rook` column: negative count against the reference on the union, exactly-singular count, max \|L\|, replay residual, and isolated instructions per factorization and solve | reported, not gated | same inertia as `dsytrf` on every decidable matrix; singular 13 IPM / 10 SQP; max \|L\| ≤ 1.88 [E7] |
| A7 | min \|D-block eigenvalue\| / max \|a_ij\| over dense-taken factorizations, per engine, split decidable / undecidable (§1.4) | reported, not gated | [E5] |

Why the gates moved from the literal form (the settler's amendment of the C259 input): on decidable
matrices Pardiso's unperturbed count is wrong on 2.3 % (n ≤ 64) to 5.6 % (all sizes) of SQP
matrices, and all 140 SQP unperturbed mismatches are Pardiso's errors (§1.2). A gate on agreement
with Pardiso measures Pardiso's error, and would fail the more accurate arm for being accurate. The
gates are per engine, so the IPM's figures cannot hide behind the SQP's or the reverse.

An A1 failure means a decidable matrix where the dense arm is wrong. It stops DN-1 and goes to the
settler with the matrix, both references and the D blocks.

**S. Status and objective, arm forced** (brief), both engines on DN-0's 238 cells. The baseline is
the same build at `kNever` (= the pre-DN-1 pin by N2). A cell whose `kNever` run reaches an n_kkt
above the S4 cap (2,000) is excluded and named in the declaration; none is expected (DN-0's replay
window peaks at 406).

| # | criterion |
|---|---|
| S1 | every baseline-success cell (optimal or acceptable) ends with the same status and objective within 1e-8·max(1, \|f\|) |
| S2 | every baseline non-success cell: the forced status is reported per cell. A move to success is reported with its objective and terminal KKT residuals. **A move from one non-success status to a different non-success status is reported in the same detail as a move to success.** Not gating. **Pending the owner** (brief change 2, a loosening of the brief's "every cell keeps its status") |
| S3 | reported per cell, both arms: iterations, factorizations, `dense_factorize_count`, `dense_fallback_count`; the walk's verdict histogram from `inertia_suspect_verdicts` and `inertia_wrong_verdicts` (or, if the owner declines the counters, from an `HVEN_TESTING` build of the bench cell, declared as a second binary); and the production QpCounters `suspect_escalations`, `eqp_residual_refusals`, `eqp_row_drift_refusals`, `border_uncertified_fallbacks`, `dep_inertia_rereads`, `dep_inertia_reread_flips` and `zero_motion_exhausted`; plus A7's small-pivot distribution over each cell's dense-taken factorizations. Every move declared |
| S4 | the forced-arm 27-cell replay and census, **scoped to cells whose largest n_kkt is at most 2,000** (8n² = 32 MB, ≈ 2.7 GFlop per factorization). **No cell qualifies**, so S4 is not run in DN-1. Every census and replay cell is F7, with n = 5·n_nodes variables and m_e = 3·n_nodes equality rows (`tests/sqp/support/scale_problems.h:576-578`). Every engine's KKT carries the equality rows, so n_kkt ≥ 3·n_nodes ≥ 2,250 on the smallest cell (n750); the full-space order is about 8·n_nodes. Excluded, all 57 census cells, which include every one of the 27 replay cells: `f7_n{1000,2000,5000,10000,20000}_{bound,path}_{neutral,physics,corrupted,activity,warm}` (50 cells), `f7_n800_path_{neutral,physics,corrupted,activity,warm}` (5), `f7_n750_path_neutral_control`, `f7_n825_path_neutral_control`. Above the cap the arm is a DN-3 question: DN-3's threshold keeps those cells sparse and byte-identical |

**M. Measured savings** (brief; **gating**, review R2):
- **Gate.** The dense arm saves **≥ 0.20 of solver instructions**, process-once OpenMP init excluded,
  on **both** SQP median cells, **hs043 and HS84**. The saving is (kNever − kAlways) / kNever, solver
  instructions, measured serially with `perf stat` under CLAUDE.md §7's serial rule: one solve at a
  time, alone on the box.
- **Thread settings.** Both cells are measured at two settings, each against `kNever` at the same
  setting:
  - the engines' default (`threads = 0`: MKL's own default, under which `dsytrf` may fork OpenMP
    threads at n ≈ 100-400);
  - `MKL_NUM_THREADS=1`.

  The gate must hold at both. If it holds at one only, that is a thread-policy finding for the
  settler (for example, pin the dense arm to one thread at small n), not a pass.
- **Exclusion.** The process-once OpenMP init is excluded by an unwind-capable attribution (r1's gdb
  marker, or a CFI-built MKL-free leg), as C262 requires of DN-3. The raw reading is reported beside
  it.
- **Informational, not gating:** the comparison with the projection (0.769 / 0.735 raw, 0.744 /
  0.717 init-excluded [D1]); population medians; IPM numbers.

Failure handling: an N, A1-A3, A5 or M failure stops DN-1 and goes to the settler. An S1 failure is
reported per cell with its kSuspect-conversion trace (S3), and the settler rules. One merged fix
round, per the brief.

## 15. Risks

| # | risk | evidence / mitigation |
|---|---|---|
| R1 | **the SQP walk moves.** ~9 % of SQP factorizations change from kSuspect to a trusted verdict (§6), and the suspect ladder, zero-multiplier probe, the μ = 0 reread and repair can fire differently | correct by the reference on 245/249; S1/S2/S3 make every move visible; T13 pins the reread path; the owner rules on S2 |
| R2 | **trusted noise.** 4 SQP matrices (0.14 %) are Pardiso-perturbed, undecidable under both references and dense-taken; there a trusted kWrong rests on a noise sign. The other 42 undecidable SQP matrices are read trusted today. IPM: 79 such matrices (2.9 %), IPM default-off | registered follow-on: a dense-arm small-pivot *fact* (not a fabricated perturbation count) and a walk rule using it, built only if S1 attributes a failure to it. §1.4: at 1e-12·max\|a\| it flags all 43 SQP undecidable matrices and 69 of 2764 decidable ones |
| R3 | **BK element growth** (L unbounded in partial BK): max \|L\| to 3.6e12 (IPM) and 9e5 (SQP) [E7] | DN-0 residuals: dense max 2.4e-16 over 5476 matrices (§1.3); rook (`dsytrf_rook`, max \|L\| ≤ 1.88, same inertia) is the A6 column and the registered alternative |
| R4 | **`kAlways` at large n:** O(n²) memory, O(n³) time, no ceiling | explicit user choice; 8n² documented on the option; n·n validated against overflow, `bad_alloc` surfaces (§4); `kAuto`'s threshold (DN-3) bounds the default path; on DN-0's replay the first matrix with `rho ≥ 1` is at n = 106 (SQP) and 168 (IPM) |
| R5 | **MKL threading inside dsytrf:** OpenMP init and region overhead | the init is excluded from the projection (§1.1); M measures both thread settings and gates on both |
| R6 | **Apple** | UNOBSERVED, every value (§10) |
| R7 | **identity bookkeeping across two sessions** | §5.1's single rule; T4/T5 |
| R8 | **consumer-side allocation and the Schur border's LAPACKE workspace** keep part of the 0.09-0.12 residual | registered, outside DN-1 |
| R9 | **the replay covers the first 50 factorizations per cell**, so fallback and fork counts beyond that are unmeasured | S3 reports `dense_fallback_count` over whole solves |
| R10 | **the IPM's singular policy** is Pardiso's perturbation, not Ipopt-style singular evidence | registered alternative (§6); DN-3 or later |
| R11 | **`DenseSymmetricFactor::block_evidence()`'s 2×2 decode miscounts negatives** when \|det\| ≪ trace² (14 LAKES matrices, 5 decidable, [E3d]). The Schur border stack reads it through `expected_neg_eigs_delta()` (`schur_complement.h:419-431, 625`), so the walk's border path may carry the same miscount today | outside DN-1 (the border path is walk-touching and stays byte-identical, N5); registered as a follow-on with a census. The dense session does not use it (§5.3). **CLOSED** by c2ce3ed32 (det/trace decode, landed before DN-1); DN-1 shares that `decode_2x2` through `detail/linear/block_decode.h` |

## 16. Open questions for the owner

**Q1. New names.** Approve these names, which the AP-1 record does not yet carry:
- a count of factorizations the dense path handed back to the sparse backend
  (`dense_fallback_count`);
- the dense factorization's size, computed from its dimension rather than reported by a backend
  (`dense_factor_entries`);
- the factor-layer size threshold spelled `max_dense_dimension`, matching the public option after
  the naming pass, rather than AP-1's `dense_max_dimension`;
- a tag on each factorization result saying whether the dense or the sparse path produced it
  (`FactorizeOutcome::arm`, of type `FactorArm`, values `kSparse` and `kDense`);
- two counters for the SQP's inner QP walk, counting how often its inertia check reads
  "untrustworthy" and how often it reads "wrong" (`inertia_suspect_verdicts`,
  `inertia_wrong_verdicts`). Without them, the proof can report those readings only from a special
  test build.

*Recommendation:* approve all five.

**Q2. Loosening the status rule in the proof.** The brief says every cell keeps its status with the
dense path forced. The proposal: cells that solve today must still solve, with the same objective.
Cells that fail today may change outcome, and every change is reported in full detail, including a
change from one kind of failure to another. The dense path removes about 9 % of the QP walk's
"untrustworthy" readings, so a failing cell may legitimately move. Do you accept the loosening?
*Recommendation:* accept.

**Q3. The dense option on the IPM in DN-1.** The IPM gets the dense option now, default off, proven
alongside the SQP. The settler accepted this on condition of the scaled-reference check. That
check passed: with the corrected block reading, the dense path matches the exact reference on every
decidable IPM matrix (2575 of 2575). The old reading, the one DN-0 used, was wrong on 5 of them.
DN-3 needs the IPM wired to measure it at all, and the IPM's default stays sparse. Do you agree?
*Recommendation:* agree.

## 17. What should change in the brief

1. **DN-1's agreement clause** (and C262 input 3), as amended by the settler on this review:
   agreement is measured against an exact eigenvalue reference and a congruence-scaled one, gated per
   engine on their decidable union (§14 A1-A2, expected 100 %). Agreement with Pardiso is reported
   at every size cut and not gated (A4). At larger SQP sizes Pardiso is the inaccurate arm.
2. **DN-1's status proof** (**pending the owner**, Q2: a loosening). "Every cell keeps its status"
   becomes S1 (success cells keep status and objective) plus S2 (non-success cells may move; a move
   to success and a move to a different non-success status are reported in the same detail). The
   arm removes ~9 % of the SQP walk's kSuspect reads, so failing cells may legitimately move.
3. **C262's "≈ 5 % of one call"** reads ≈ 4-6 % of a fresh analyze+factorize+solve. Against an
   average engine call it is ≈ 17-27 %; the engine-weighted ratio (0.11-0.14) is the projecting one.
4. **r1 note 6 ("the SQP is unaffected")**: the SQP pays the same process-once OpenMP init through
   MKL's first call (median 10 % HS, 4 % small-dense of solver instructions). It does not change
   DN-1. DN-3's init-exclusion requirement should name both engines.
5. **The four consumers become three.** `equality_basis` stays sparse in DN-1 (§5.7). Its
   certificate's evidence source is a separate registered item.
6. **DN-3 on Apple:** the auto-selection default is platform-scoped and stays `kNever` on Accelerate
   until Apple values are observed.
7. **The measured saving gates** (§14 M): ≥ 0.20 of solver instructions, init-excluded, on hs043 and
   HS84, at both thread settings. The comparison with the engine-weighted projection is
   informational, not a gate.
8. **"The 27-cell replay and census with the arm forced"** has no cell under a workable n_kkt cap
   (§14 S4). The forced-arm proof is DN-0's population (S1-S3); the corpus is a DN-3 question.

---

## 18. DN-1d: dense singular handling for the SQP — report, don't replace

Ledger C276–C280 (design study C278, ruling C280). Replaces DN-1c's condition fallback.

**Factor layer.** `Options::dense_singular_policy` (`DenseSingularPolicy`, `dense_policy.h`):
`kSparseFallback` (default; DN-1: Bunch-Kaufman, exact singularity retaken by the platform session;
IPM, tycho, tests) or `kReport` (refused under `kNever`). Under `kReport` the dense session factors
with `dsytrf_rook`/`dsytrs_rook` and runs a two-stage detector: the rook block walk's D-ratio
`min|λ(D)|/max|λ(D)|` (1×1 pivot `|d|`, 2×2 block from `decode_2x2`'s two magnitudes) below
`kDenseSingularScreen = 1e-15` buys a `dsycon_rook` estimate (‖A‖₁ from the CSR, shift included);
rcond `< 2^-53` or NaN flags. Flagged: numerics kept, `kOk`, `InertiaEvidence::numerically_singular`.
Exactly singular (info > 0, a zero 2×2 determinant, non-finite D): `kBackendError`,
`backend_code = kDenseSingular`, arm `kDense`, the bit set, no numerics. Nothing is retaken.
Two responses: `factorize_shifted(A, n_primal, primal, dual)` (dense, `A + diag(+primal on the first
n_primal rows, −dual on the rest)`, detector re-run; a taken outcome reports
`perturbed_pivots = dim`, so every inertia verdict reads it suspect) and `retake_on_platform(A)`
(the `kSparseFallback` path, counted). Counters: `dense_singular_count`, `dense_flagged_count`,
`dense_condition_estimate_count` (dsycon calls), `dense_shift_refactor_count`,
`dense_retake_count`; DN-1c's `dense_condition_fallback_count` is gone; `factorize_count =
dense_factorize_count + dense_fallback_count + dense_singular_count + sparse-selected`.
Accelerate: the shim gains `LAPACKE_dsytrf_rook_work`, `LAPACKE_dsytrs_rook_work`,
`LAPACKE_dsycon_rook_work` (UNOBSERVED; macOS CI compiles them); MKL's LAPACKE has no
`dsycon_rook`, so `dense_session.cpp` wraps the Fortran entry.

**Bands (DN-1c review F3).** Pardiso perturbs pivots below ~1e-8 relative (the walk reads that
suspect); the dense flag is at rcond 2^-53. A dense factorization between the two is taken and
trusted where Pardiso's would have been suspect. Documented on the option; not a defect.

**SQP response (`detail::factorize_checked(k, K, decision, DenseShiftSite{n_primal, δ, μ})`).**
Flagged or exactly singular → `factorize_shifted` with `τp = max(δ, 2^-27·max|K|)`,
`τd = max(μ, 2^-27·max|K|)` (on top of the assembled δ, μ, as E0 measured); still flagged →
once more at 100×; still flagged → `retake_on_platform` (the last resort; criterion 0 on the
roster); a failed outcome → `BackendError` as before. Returns `CheckedFactorization{outcome, δ, μ,
shifted, retaken}`, δ/μ being the factored matrix's regularization; `KktFactor::dense_shifted`
marks shifted numerics. A shifted solve is refined against K itself (`refine_shifted_solve`, at
most `kEqpShiftRefineSteps = 10` steps, each kept only while ‖rhs − K y‖∞ falls): that restores
every direction whose curvature dwarfs τ and cannot move the near-null ones. The polish
(`polish_eqp_core`) forms its residual from the assembled K and the options' (δ, μ), so it needs
nothing from the factor and stays a keep-while-falling refinement.
Per consumer: `solve_eqp` (site `{n_free, δ, μ}`; the one unconditional proximal step becomes
keep-while-falling on a shifted factor); the reread probe (`refine_eliminated_face_for_verdict`,
same site, same step rule); μ-free polish / terminal / saddle (through `solve_eqp`, μ = 0);
`rebuild_k0` (site `{n, δ, μ}`; the Schur border stack solves through K0 unchanged); SSN (both
sites, `{n, eff_delta, eff_mu}`); IPQP (`kReport` at reconfigure; a flagged or exactly singular read
is `kPerturbed`, the tier's own shift remedy; never shifted or retaken); the rank repair's Gram
factor `kSparseFallback` (no detector); equality basis and predictor stay `kNever`. The IPM keeps
`kSparseFallback` and DN-1b's guard (IPM `kReport` registered, R10).

**Counting.** `QpCounters`/`SqpCounters`: `dense_condition_fallbacks` (now the last-resort retakes),
`dense_flagged_factorizations` (flagged + exact, shifted refactors included),
`dense_singular_refactors`; folded wherever `dense_condition_fallbacks` was; fed by
`DenseTallyScope`, which adds the factor-counter deltas on scope exit, a throw included (DN-1c
review F5). A retake that forks a fresh platform session pays a symbolic analysis
`symbolic_analyses` does not count (F7, documented).

**Known limit (measured in development, before any declaration).** The shift is uniform in
max|K|. On a badly scaled K it damps legitimate directions whose curvature is below τ, and no
keep-while-falling refinement restores them: `NearSingularHessianProblem` at 2^20 (τ = 8192)
stalls to max_iter where kNever converges in one major (the shipped test uses 2^14), and SQP
ALLINITC — whose flagged μ-free systems reach max|K| = 2.7e9, so τd reaches ~20 against a dual
Schur curvature ~1e-9 — rejects every μ-free terminal polish and ends numerical_error, the same
trajectory as DN-1c's platform retake. With the detector reporting and no response to finite
flags (exact ones shifted), ALLINITC is optimal, bit-for-bit DN-1b's objective.

**Riders.** `kIllConditioned` → `kNumericallySingular` (`DenseFactorizeResult`); DN-1b F8 (the IPM
guard's failed re-run sets `exhausted`; defensive — `factor_impl` already reports it — pinned by
`IpmDenseGuardSeam` on the new `DenseFactorizeFaultInjector`); stale shim prose.

## Appendix A. Binding inputs → answers

| input (C259/C262) | answer | where |
|---|---|---|
| 1. exactly singular vs Pardiso perturbing; IPM ladder; SQP verdict; fields not produced | per-factorization fallback to the platform session, evidence verbatim, `arm == kSparse`; singular = `info > 0`, a 2×2 det of 0.0, or a non-finite value; ladder and walk unchanged on those matrices; dense-taken outcomes trusted (correct on 245/249 perturbed SQP matrices); absent fields listed | §6, §5.3 |
| 2. inertia disagreement at selectable sizes | no threshold. Against both references the arm is right on 100 % of decidable matrices, per engine; the large-n disagreement is Pardiso's error; a threshold would send 38-46 % to the fallback; rook gives the same inertia | §7, §1.2 |
| 3. the proof's agreement criterion (amended by the settler on this review) | A1/A2 per engine on the decidable union of the exact and congruence-scaled references, expected 100 % / 0; A3; A4 (literal Pardiso agreement) reported at every size cut, not gated; A5 with the 14 declared decode differences; A6 rook; A7 small pivots | §14 A |
| 4. the IPM carries no projected-saving weight | IPM numbers informational only; the SQP case is restated init-excluded (0.744 / 0.717); the SQP's own init found and excluded; the measured SQP saving gates at ≥ 0.20 | §1.1, §14 M |
| 5. size gate; DN-1 vs DN-3 | `kNever`/`kAlways` in DN-1, decided per pattern at `analyze()`; `kAuto` mechanics at the factor layer, refused by the engines until DN-3 | §4 |
| 6. Accelerate | Accelerate LAPACK via the shim (plus a workspace entry); every Apple figure UNOBSERVED; DN-3 platform-scoped | §10 |
| 7. licences | MKL / Accelerate LAPACK only; nothing vendored; no GPL/LGPL | §11 |
| 8. per-call alternatives | order of magnitude confirmed against call trimming; refuted against F2 (2-3× realistic, equal ceiling); dense first because it is larger and shrinks F2 ~7-10× | §12 |

## Appendix B. The design review, round r1

The settler's ruling on the design review: APPROVE WITH CHANGES.

| item | change | where |
|---|---|---|
| R1 | S4 scoped to n_kkt ≤ 2,000; every census and replay cell is above it and named; S4 is not run in DN-1 | §14 S4 |
| R2 | the measured saving gates: ≥ 0.20, init-excluded, hs043 and HS84, serial `perf stat`, default threads and `MKL_NUM_THREADS=1`; the projection comparison is informational | §14 M |
| R3 | S3's counters named; two verdict counters proposed (owner Q1), with the `HVEN_TESTING` build as the alternative | §3, §14 S3 |
| R4 | a congruence-scaled reference (exact power-of-two Jacobi), run on all 5501 matrices at s = 1, 10, 100; A1/A2 gated per engine on the union, expected values stated. Result: the det/trace-read arm is right on 100 % per engine; DN-0's e1/e2-read column is wrong on 5 scaled-decidable IPM matrices | §1.2, §14 A |
| R5 | a 2×2 det of exactly 0.0, or a non-finite value, is exact singularity and falls back; 0 such cases on DN-0's matrices | §5.3, §6 |
| R6 | the small-pivot distribution computed (§1.4) and reported by the A-leg (A7) and S3; T13 covers the μ = 0 reread; S5's note in §1.2 | §1.4, §13, §14 |
| R7 | A4 reported at every size cut, not gated; the settler's amendment of C259 recorded; brief change 1 reworded; brief change 2 pending the owner, with the reviewer's non-success-to-non-success clause | §14 A, S2, §17 |
| R8 | `kAlways` validates n against `lapack_int` and n·n·8 against `Index`/`size_t` overflow; `bad_alloc` surfaces; the 8n² figure is on the option's doc comment | §4 |
| S1 | `FactorizeOutcome::arm` (`FactorArm`), name for the owner | §3, §5.3 |
| S3 | `dsytrf_rook` computed on all 5501 matrices: same inertia on every decidable matrix, bounded growth; `dsytrf` kept; A6 column pre-registered | §7, §14 A6 |
| S4 | `dense_factor_entries` kept as a size report; making `factor_mem_` absent-capable rejected (public `IpmResult` type change) | §3, §5.4 |
| S5 | 42 of 46 undecidable SQP matrices are read trusted today; 4 (0.14 %) are new | §1.2 |
| S2 | not applied: the review text was not available to this lane | — |
