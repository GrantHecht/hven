# The consumed-surface audit

**Run 2026-08-09**, hven commit `ca2744c1ba41`, on `fedora-x86_64-linux`,
Intel oneAPI MKL 2026.1 (Product Build 20260612), clang 22.1.8.

Roots scanned:

| Seam | Root | State |
| --- | --- | --- |
| interior-point | `/home/ghecht/Projects/tycho/psiopt` | live checkout, at `060ad2138a373bacd9d1ba2daabd6b503a9a6fb1` (hven's recorded pin — `tests/golden_rig/CMakeLists.txt`'s `HVEN_RIG_PSIOPT_SEAM_COMMIT`), advisory-verified since this checkout is expected to move |
| SQP | `/home/ghecht/Projects/tycho_sqp` | tag `phase-7-close` = `4faa1df116da53c9dc68f36635c118f52d39d2b9`, tree state verified at configure |

The question this audit answers is not "what backend surface exists" but
**"what do the two engines being migrated actually consume, and does the
frozen `Options` set cover it"**. Anything found beyond the frozen set is
listed under "Findings" and re-enters the freeze gate. Nothing here
extends the surface on its own authority.

Raw static output: [`audit/static-scan-2026-08-09.csv`](audit/static-scan-2026-08-09.csv)
(226 rows including header). The scanner is
`tests/golden_rig/audit/static_scan.sh`; regenerate with

```bash
tests/golden_rig/audit/static_scan.sh <tycho>/psiopt <tycho_sqp>
```

## Method, and what each half can prove

**Static half.** One CSV row per backend touchpoint — parameter reads and
writes, phase-entry calls, Accelerate calls, dense LAPACK calls, thread
controls — matched by pattern over the source. Deliberately
over-inclusive: a touchpoint reported that turns out not to matter costs a
line in a review, one missed silently is a hole in the option set the
migration lands on. It reports what the source **mentions**, and it cannot
distinguish a parameter written on every call from one written on a path
no fixture reaches.

Of the 225 data rows, **165 are live code and 60 are commented-out**. The
tables below count live rows only; the commented ones are kept in the
artifact because a commented-out parameter write is a decision someone
made and may want back.

**Runtime half.** `hven_golden_rig_audit` interposes on the backend's
phase entry point via the linker's `--wrap`, recording every call — from
hven's own session and from either old seam alike — with the parameter
array as it stood at that moment. It forwards to the real symbol
unchanged. Linux-only, and mission-limited to Linux anyway: both old seams
build only against sibling Linux/MKL checkouts.

Its job is the half the static pass cannot do: report what a run
**executes**.

## Reconciliation against the frozen `Options` set

The frozen set is `kind`, `num_threads`, `pivot_perturb_exp` (iparm[9]),
`max_refinement_iters` (iparm[7]), `ordering` (iparm[1]), and
`weighted_matching` (iparm[12]), with no raw-parameter escape hatch.

### SQP seam — clean

Everything the SQP seam touches was already on the freeze review's list,
and the audit found nothing else:

| Touchpoint | Frozen coverage |
| --- | --- |
| writes `iparm[34] = 1` (zero-based CSR) | adapter-internal, named as such in the frozen spec |
| writes `iparm[7]` | `max_refinement_iters`, plus the phase-33x rule below |
| reads `iparm[7]` | the save half of the phase-33x rule |
| reads `iparm[13]` | `InertiaEvidence::perturbed_pivots` |
| reads `iparm[21]`, `iparm[22]` | `InertiaEvidence` counts |
| `pardisoinit`, `pardiso` phase calls | the lifecycle itself |
| `LAPACKE_dsytrf` / `dsytrs` | the dense border factor — `DenseSymmetricFactor` |

**Result: the SQP-side consumed-surface list in the frozen spec is
confirmed exactly, with no additions.**

### Interior-point seam — five knobs and one evidence pair beyond the set

The interior-point seam writes its whole parameter array in one
`set_params()` (`pardiso_interface.h:618-650`), so a raw count of writes
overstates its configurability: most entries are hardcoded constants
(reserved slots, "not in use", output slots pre-zeroed). What matters is
which entries are bound to a **user-facing setting**. Those are:

| Entry | Engine setting | Default | In the frozen set? |
| --- | --- | --- | --- |
| `iparm[1]` | `qp_ord_` (`QPOrderingModes`) | `METIS` | yes — `ordering` |
| `iparm[7]` | `qp_ref_steps_` | `0` | yes — `max_refinement_iters` |
| `iparm[9]` | `qp_pivot_perturb_` | `8` | yes — `pivot_perturb_exp` |
| `iparm[12]` | `qp_matching_` | `1` | yes — `weighted_matching` |
| `iparm[4]` | `store_perm_` | `2` | deliberately excluded — see below |
| `iparm[10]` | `qp_scaling_` | `0` | **NO** |
| `iparm[20]` | `qp_pivot_strategy_` (`QPPivotModes`) | `TwoByTwo` (1) | **NO** |
| `iparm[23]` | `qp_alg_` (`QPAlgModes`) | `Classic` (0) | **NO** |
| `iparm[24]` | `qp_par_solve_` | `0` | **NO** |
| `iparm[33]` | `qp_threads_`, only when `cnr_mode_` | unwritten (mode off) | **NO** |

Reads:

| Entry | Consumed as | In the frozen set? |
| --- | --- | --- |
| `iparm[13]` | `ppiv_` → perturbed pivots | yes |
| `iparm[21]`, `iparm[22]` | `peigs_` / `neigs_` | yes |
| `iparm[17]` | `mem_` → `result_.factor_mem_` | **NO** |
| `iparm[18]` | `flops_` → `result_.factor_flops_` | **NO** |
| `iparm[0]` | "was a factorization computed" predicate | yes, as lifecycle state |

Thread controls: `mkl_set_num_threads_local` (7 sites) — matching A.6's
per-instance, call-scoped mechanism exactly. Plus two Accelerate-side
controls discussed under finding 6.

### `iparm[4]` (store_perm) — the pre-registered dead-code check, CONFIRMED

The frozen spec excludes `iparm[4]` on the grounds that "its only consumer
is dead code, confirmed by the consumed-surface audit before the migration
relies on it". **Confirmed.** The engine writes `iparm[4] = store_perm_ =
2` (ask Pardiso to return the permutation), and `perm_` is passed to every
`pardiso()` call as an out-parameter and `setZero`'d before use — but it
is never read anywhere in the tree. `grep` for `perm_` outside
`pardiso_interface.h` finds only the unrelated Accelerate-side
`permutation_`. The permutation is computed, stored, and discarded.

## The pre-registered coverage test — PASSED

The freeze review pre-registered a specific check on the audit's *method*:
the phase-33x `iparm[7]` force-zero-restore rule
(`tycho_sqp/include/tycho_sqp/kkt_system.h:327-335`) is a correctness rule
rather than an option, so a scan of an option surface would not produce
it. The audit had to find it independently through the runtime shim, or
the audit's method is broken and gets fixed before its output is trusted.

**`AuditRuntimeShim.FindsTheRefinementCapRuleInTheOldSeam` passes against
the real SQP seam.** It drives a full solve, a forward phase-split solve,
and another full solve, and asserts:

- the refinement cap is `0` on the phase-split call, and
- the cap was among the entries the detector reports as having *varied
  between calls*, and
- the cap after the phase-split solve is restored to what it was before.

The instrument is not told which entry to watch — it reports which
parameter indices changed value between recorded calls, and the test
asserts the refinement cap is among them. That makes it a test of the
technique rather than of a hardcoded expectation.

Both halves of the rule were genuinely exercised on this build. The test
carries a `GTEST_SKIP` for the case where the linked library's initializer
leaves the cap at zero — the restore half would then have nothing to
restore — and that branch did **not** fire: the initializer's cap is `2`
here, so the restore assertion ran. All four `AuditRuntimeShim` tests
passed, none skipped.

The static half found the rule too — three rows in `kkt_system.h` — because
the scanner greps parameter *subscripts* rather than option names. That is
a bonus, not the deliverable; the runtime finding is what was
pre-registered and it is what passed.

## Findings — surface beyond the frozen spec

Each of these is **flagged, not adopted.** They re-enter the freeze gate.

### 1. `iparm[10]` — MPS scaling (`qp_scaling_`)

A documented, user-facing knob with a written performance analysis behind
it (`docs/dev/analysis/2026-07-pr9-pardiso-options.md`): enabling it
measured −16% wall on PolarLT-class collocation and dropped perturbed
pivots from 95/120 to ~0, but degraded convergence elsewhere, so it ships
off with an opt-in. hven has no way to express it. A user who has opted in
today loses the ability to after the migration. Default `0` coincides with
`pardisoinit`'s `0` on this build, so the *default* path is unaffected.

**Disposition:** `SymmetricFactor::Options::matrix_scaling`
(bool, don't-write-by-default, iparm[10]). Pardiso-only: throws on
Accelerate for `true` — see the option's own doc comment for why this is
the same judgment as `weighted_matching`, not a separate one.

### 2. `iparm[20]` — pivoting strategy (`qp_pivot_strategy_`)

A six-valued user-facing enum (`QPPivotModes{OneByOne, TwoByTwo, E4, E6,
E8, E13}`), with no hven equivalent. Its default is `TwoByTwo = 1`, and
**the engine writes that value explicitly** while hven never touches
`iparm[20]` at all. `pardisoinit` leaves `1` there on this build, so the
values coincide — but the *acts* differ, which is precisely the
distinction the ordering/weighted-matching amendment was written around:
"writing the same value explicitly would be a different act with the same
effect". A library-default move on an MKL bump changes hven's pivoting
strategy and does not change the engine's.

**Disposition:** `SymmetricFactor::Options::pivot_strategy`
(`PivotStrategy` enum: `kBackendDefault`, `kOneByOne`, `kTwoByTwo`,
`kOneByOneNoAutoRefine`, `kTwoByTwoNoAutoRefine` — iparm[20]'s own FOUR
documented codes, 0/1/2/3 per Intel's oneMKL Developer Reference
("pardiso iparm Parameter"). `QPPivotModes`' `E4`/`E6`/`E8`/`E13` names
(4/6/8/13) are NOT among Intel's documented iparm[20] values and are not
carried over here — `psiopt` writing one of those codes writes an
undocumented iparm[20] value, which is a fact about `psiopt`'s own
surface, not one this option reproduces. `kTwoByTwo` (1) is `psiopt`'s
own default and stays expressible.), don't-write-by-default. Pardiso-only:
any non-default value throws on Accelerate, which has no pivoting-strategy
selector.

### 3. `iparm[23]` and `iparm[24]` — two-level algorithm, parallel solve

`qp_alg_` (`QPAlgModes{Classic, TwoLevel}`) and `qp_par_solve_`. Both
user-facing, both defaulting to `0`, both coinciding with `pardisoinit`'s
`0` here. Same shape as finding 2 at lower stakes: expressible today, not
expressible after.

**Disposition:**
`SymmetricFactor::Options::factorization_algorithm` (`FactorizationAlgorithm`
enum: `kBackendDefault`, `kClassic`, `kTwoLevel` — iparm[23]) and
`SymmetricFactor::Options::solve_parallelism` (`SolveParallelism` enum:
`kBackendDefault`, `kAdaptivePartitioning`, `kSequential`,
`kMatrixPartitionParallel` — iparm[24]'s own three documented codes, 0/1/2
per Intel's oneMKL Developer Reference; iparm[24] = 1 is SEQUENTIAL, not
parallel — an earlier revision of this option was a bare bool that wrote 1
for `true`, backwards from what the code means, corrected to this named
enum), both don't-write-by-default. Pardiso-only: either throws on
Accelerate for a non-default value, which has no two-level-algorithm
concept and no per-instance thread control to parallelize a solve with.
`factorization_algorithm == kTwoLevel` additionally requires
`ordering ∈ {kNestedDissection, kParallelNestedDissection}` and
`matrix_scaling == weighted_matching == false`, both validated at
construction (Intel documents both as requirements of the two-level
algorithm, not merely as recommendations).

### 4. `iparm[33]` — conditional numerical reproducibility

Written only when `cnr_mode_` is on (off by default), from `qp_threads_`.
This was already flagged during the rig build as *not* a thread count, and
it is worth restating why it matters more than its default suggests:
**CNR mode is the engine's existing answer to run-to-run reproducibility**,
which is the same property the golden-rig derivation had to hold rows back
over. A migration that drops it drops a reproducibility control while the
tables it is being validated against were themselves gated on
reproducibility. Deserves a decision, not a default.

**Disposition:** `SymmetricFactor::Options::cnr_threads` (int,
0 = off/don't-write, iparm[33]). Pardiso-only: a positive value throws on
Accelerate, which has no CNR concept — a silent no-op would misrepresent a
reproducibility guarantee as still holding. A positive value ALSO requires
`ordering == kNestedDissection` exactly, validated at construction: Intel
documents CNR as reproducible only under "the non-parallel version of the
nested dissection algorithm," and this MKL's own `kBackendDefault` floats
to the documented-incompatible parallel variant (iparm[1] = 3) — so
`kBackendDefault` throws here too, not only the values obviously
unrelated to nested dissection.

### 5. `iparm[17]` / `iparm[18]` — factor size and Mflops are consumed evidence

Not options — **evidence**, and the frozen evidence types have no home for
them. The engine reads both after every factorization
(`pardiso_interface.h:254-255` and three sibling sites), stores them as
`result_.factor_mem_` / `result_.factor_flops_`
(`psiopt.cpp:3148-3149`), and **prints `factor_mem_` in its own solver
output** (`psiopt.cpp:3155`). `hven::linear::SolveInfo` carries only
`refinement_iters`; `InertiaEvidence` carries counts and perturbed pivots.
After the migration the engine cannot report its factor size. That is a
user-visible regression in the engine's own output, not merely an internal
gap.

Note the same pair exists on the Accelerate side with **different meaning**
— the Accelerate interface's own comment records that its `mem_` is the
factor size in BYTES where Pardiso's is a nonzero COUNT, and both surface
as `result_.factor_mem_`. If this evidence is added to the frozen surface,
that is a per-backend semantics row to write, not a field to copy.

**Disposition:** a new `FactorEvidence` struct joins
`FactorizeOutcome::factor` (`hven/linear/symmetric_factor.h`) — not
`SolveInfo`: this evidence is read at the same point in the lifecycle as
`InertiaEvidence` (right after a successful numeric factorization), and a
solve does not refresh it — with the mandatory per-backend semantics row
this finding calls for. The two entries this finding names split
differently, per Intel's own documented cost: iparm[17] (nonzero count)
carries no documented request cost and pardisoinit's own sample
initialization already requests it, so `FactorEvidence::factor_nonzeros`
is collected UNCONDITIONALLY on MKL, no Options field at all; iparm[18]
(Mflop estimate) is documented to increase factorization time when
requested, so `FactorEvidence::factor_mflops` stays gated by
`SymmetricFactor::Options::collect_factor_mflops` (bool,
don't-write-by-default) — Pardiso-only, throws on Accelerate for `true`,
same shape as the options above. Accelerate's `factor_size_bytes` (the
symbolic factorization's own byte size) is likewise collected
UNCONDITIONALLY, no Options field: Accelerate computes it regardless.
No field is ever populated with the other backend's meaning.

### 6. Accelerate has a per-thread control, contradicting A.6's premise

A.6 documents Accelerate threading as "best-effort-absent (no public
thread control; we do not fabricate a control that does not exist)".

The audit finds the interior-point seam already using two:
`VECLIB_MAXIMUM_THREADS` at init (read exactly once, at the first BLAS
call — a `setenv` after that point is a no-op, which the seam documents),
and **`BLASSetThreading`, which the seam's own notes describe as
"per-thread dynamic control via thread-local storage" on macOS 15+ and
"the only working dynamic control"**. It is a binary toggle
(single-threaded vs the cached maximum), not an arbitrary count.

What this does **not** settle: whether either control reaches Accelerate's
*sparse* solver as opposed to its BLAS/LAPACK. The static pass cannot
answer that, the runtime half is Linux-only, and no Apple three-seam run
exists. So the finding is precise about its own limit: **a per-thread
control demonstrably exists and is already consumed; whether it governs
the surface hven wraps is unobserved.** A.6's blanket "no public thread
control" is too strong as written either way.

### 7. Accelerate ordering — a frozen claim the audit contradicts

The `ordering` amendment states, of `ordering` and `weighted_matching`:
"these are Pardiso concepts; a non-default value on the Accelerate backend
THROWS `std::invalid_argument`... **The IPM engine's Mac path never set
either, so its migration is unaffected.**"

The second half is contradicted by the source. `set_qp_params()`'s
Accelerate branch (`psiopt.cpp:88-110`) switches on `qp_ord_` and calls
`kkt_sol_.set_order(...)` on **every** invocation, mapping
`MINDEG → SparseOrderAMD`, `METIS → SparseOrderMetis`, `PARMETIS →
SparseOrderMTMetis` (with a runtime downgrade to serial METIS below macOS
26). `qp_ord_` defaults to `METIS`. So the Mac path sets ordering
unconditionally, on a real Accelerate ordering surface.

The first half is also only half right: **weighted matching is genuinely a
Pardiso concept with no Accelerate counterpart, but ordering is not** —
`SparseOrder_t` is Accelerate's own. A throw on a non-default `ordering`
under Accelerate would therefore refuse a request that backend can
actually satisfy, and would break the IPM engine's Mac path at the
migration rather than leaving it unaffected.

This is the sharpest item in this report: it is not a missing option, it
is a frozen statement that the consumed surface falsifies.

### 8. Accelerate pivot and zero tolerances

`set_pivot_tolerance(accel_pivot_tolerance_ = 0.01)` and
`set_zero_tolerance(accel_zero_tolerance_ = 1e-4 * eps)`, both set on
every `set_qp_params()`. The frozen `pivot_perturb_exp` names an
Accelerate mapping ("Accelerate: pivot tolerance mapping documented per
backend"), so the first is arguably covered pending that mapping being
written down. **The zero tolerance is not covered by anything.**

**Disposition:** `SymmetricFactor::Options::accelerate_zero_tolerance`
(`std::optional<double>`, don't-write-by-default — `std::nullopt` keeps
the existing `pivot_perturb_exp`-derived formula). Accelerate-only: a
present value throws on MKL, which has no zeroTolerance concept of its
own for it to override. The pivot tolerance half of this finding stays
covered by `pivot_perturb_exp`'s existing Accelerate mapping, which this
option does not change.

## What was NOT found

Stated because an audit's negative results are part of its output:

- **No raw-parameter escape hatch is needed by either seam.** Every
  touchpoint is either a named knob, an adapter-internal constant, or
  evidence. The frozen spec's "no escape hatch" decision survives the
  audit.
- **No additional trace is required.** B.4 allows the audit to demand one
  ("the coverage claim is observed, not assumed"). The traces already
  cover every touchpoint class the audit found; findings 1–8 are surface
  questions for the freeze gate, not gaps in what the rig exercises. The
  Accelerate ordering mapping identified by finding 7 now has native-arm
  coverage in the macOS CI lane.
- **The 64-bit Pardiso entry point is reachable in principle but not
  consumed.** `pardiso_64` appears exactly once, in the
  `pardiso_run_selector<long long int>` specialization
  (`pardiso_interface.h:60`) — a template body that is instantiated only
  if the seam is used with a 64-bit index type. This build links MKL LP64,
  so the `int` specialization is what runs. Recorded rather than dismissed:
  hven wraps one entry point, and a build that selected the other would
  slip past the interposer. hven's own `static_assert` on the index width
  fires first if that is ever attempted, which is what keeps the
  single-entry-point assumption honest rather than merely convenient.
- **No CHOLMOD, PaStiX, SuperLU or SparseLU consumption** by either
  engine. Those appear only in vendored Eigen, outside both scan roots.
