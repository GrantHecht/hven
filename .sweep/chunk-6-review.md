# Chunk 6 comment-sweep review (hven-sweep @ b8ebc54)

Scope reviewed: all 17 files in `git diff 1d02fee b8ebc54` (largest deletions first:
`warmstart/continuation.cpp`, `linear/symmetric_factor_accelerate.cpp`,
`linear/accelerate_session.cpp`, `interior/kkt_factorization.cpp`,
`linear/pardiso_session.cpp`, `globalization/sqp/soc_elastic_restoration.cpp`,
`globalization/sqp/funnel.cpp`, `hven_pch.h`, then the small ones). Every surviving or
edited sentence was checked at the code it sits on; every cross-reference was resolved in
the tree at b8ebc54.

**Verdict: APPROVE WITH REQUIRED FIXES.** 2 blocking, 5 minor, 3 nits. Nothing needs the
chunk reverted; the two blocking items are single-sentence corrections.

---

## Blocking

### 1. `src/interior/kkt_factorization.cpp:179-182` — wrong actor, and self-contradictory (severity: medium/blocking)

Written:

> `// On this backend a failed factorization RESETS the inertia counts and`
> `// both factor metrics to zero -- DEFINED values the engine goes on to`
> `// read, so they are reproduced here rather than replaced by the linear`
> `// layer's honest invalid counts.`

Nothing on "this backend" resets anything. The zeros are written by **this function**, at
:183-187 (`n_pos_ = 0; n_neg_ = 0; perturbed_pivots_ = 0; factor_mem_ = 0; factor_flops_ = 0;`),
precisely because the engine expects defined values there. The sentence also contradicts
itself: if a failed factorization already reset them, there would be nothing to "reproduce
here". The parent text named the actor correctly ("The engine's previous Accelerate
interface reset its inertia counts to zero … so they are reproduced here").

Correct text (keeps the fact, drops the history):

```
// This projection ZEROES the inertia counts and both factor metrics on a
// failed factorization: the engine reads them as DEFINED values, so the zeros
// are written here rather than passing through the linear layer's honest
// invalid counts.
```

### 2. `src/linear/accelerate_session.cpp:171-172` — other-codebase attribution turned into an hven history claim (severity: medium/blocking)

Written:

> `// hven's own k = 8 default (~2.2e-24) is FOUR ORDERS OF MAGNITUDE TIGHTER`
> `// than the k = 4 real-hardware precedent it replaced.`

hven never shipped k = 4. `Options::pivot_perturb_exp` is `= 8` at
`include/hven/linear/symmetric_factor.h:297` and always has been; k = 4 is (a) Apple's own
documented default, still what the don't-write branch passes at this file's :195-200, and
(b) the value hardcoded in the *other* codebase's `kkt_system_accelerate.h`, which is what
the parent sentence attributed it to ("the only real-hardware precedent … part of the
audited KktSystem precedent … therefore diverges from that precedent"). "it replaced"
invents an hven history and contradicts the comment three lines above it, which cites
k = 4 as Apple's default.

Correct text:

```
// hven's own k = 8 default (~2.2e-24) is FOUR ORDERS OF MAGNITUDE TIGHTER
// than k = 4 -- Apple's own documented default, and the only value with
// real-hardware precedent when this was written.
```

---

## Minor

### 3. `src/interior/kkt_factorization.cpp:22-24` — categorization attributed to the backend (severity: low/medium)

> `// Reporting-only status categories: which warnings the ladder prints depends`
> `// on them, so each branch transcribes its backend's established outcome`
> `// semantics verbatim.`

The branch is hven's own *mapping* of backend status codes onto
`Eigen::ComputationInfo` — not the backend's semantics. The parent correctly said both
branches reproduce the categorization the engine's previous backend interface performed.
Suggest: `…so each branch reproduces, verbatim, the categorization the engine has always
recorded for that backend's outcome codes.`

### 4. `src/globalization/sqp/funnel.cpp:35-39` — NaN argument generalized to all non-finite values (severity: low)

> `// Wrong-answer guard, not a robustness nicety: every comparison against a`
> `// NaN is false, so an unguarded non-finite value would be classified by`
> `// accident of how each test is written.`

The "every comparison is false" reasoning holds for NaN only; comparisons against ±inf are
well-defined, so an infinity is not "classified by accident". The parent scoped it to NaN
(`an unguarded NaN h_new would pass Eq. (8) …`). Suggest: `… so an unguarded NaN would be
classified by accident of how each test is written (the guard rejects infinities too).`

### 5. `src/globalization/sqp/funnel.cpp:85-86` — counterfactual that is not true (severity: low)

> `// Eq. (13), written in the paper's own term order -- reordering would`
> `// change the arithmetic the tests hand-derive.`

IEEE addition is commutative, so swapping the two addends of
`(1-κ)*h_new + κ*width_` changes no bit; and `FunnelHType.KappaRoleIsUnobservableAtOneHalf`
(`tests/sqp/test_funnel.cpp:253`) asserts that even the *role-swapped* form is bit-equal at
κ = 0.5. The parent made the true claim ("Written in the paper's own term order so the
arithmetic is the arithmetic the tests hand-derive"); restore that phrasing.

### 6. `src/warmstart/continuation.cpp:139-141` — invariant scoped to one case (severity: low)

> `// ContinuationResult's own invariant -- the two`
> `// counters sum to the number of failed attempts -- holds on the one`
> `// case where `steps` is entirely failures.`

Reads as though the invariant only holds there. `continuation.h:357-359` states it for
every failed attempt in `steps`; the parent's point was that the *old* code broke it on
this path. Suggest: `…so ContinuationResult's own invariant -- the two counters sum to the
number of failed attempts -- holds here too, the one case where `steps` is entirely
failures.`

### 7. `src/linear/symmetric_factor_mkl.cpp:137-139` — cross-reference to something the header does not carry (severity: low)

> `// doc comment in symmetric_factor.h for the citation and the naming`
> `// history it corrects.`

`symmetric_factor.h:472-511` carries the iparm[24] citation and the enum-rather-than-bool
rationale ("a naive true-means-parallel spelling would point at the wrong code, since 1 is
the SEQUENTIAL value"), but no naming *history* — the history was the sentence deleted here.
Suggest: `… for the citation and for why this is an enum rather than a bool.`

---

## Nits

### 8. `src/interior/utils/get_core_count.cpp:90-92` — floating `///` block

The Boost attribution is a `///` block inside a lambda body, attached to no declaration
("no floating `///` blocks", comment-rules). Use `//`. Provenance itself is fine: the Boost
Software Licence header at :1-9 is intact and the attribution names
`boost::thread::physical_concurrency` / `boostorg/thread, pthread/thread.cpp`; restoring the
upstream URL is optional (the report already offers it).

### 9. `src/globalization/sqp/funnel.cpp:46-47` — stale section title

`// see the class's THE FULL-STEP MODE note` — an earlier chunk renamed that section to
`FULL-STEP MODE — a SECOND, OPT-IN STATE OF THIS CLASS`
(`include/hven/detail/globalization/sqp/globalization.h:345`). Drop the leading "THE".

### 10. `.sweep/chunk-6-report.md` — comment-line counts disagree with the gate

The report's table totals 1746 → 1298; the mechanical gate reported 1598 → 1150. The delta
(448) matches, so this is a counting-rule difference (licence/provenance headers, most
likely) rather than a miscount — but state the rule so the chunk totals are comparable.

---

## Checked and correct (no action)

Recorded so the next reviewer does not redo it:

- **Report's own "written under uncertainty" list** — all six verified:
  `thread_pool.cpp` ctor-reads-stash (`thread_pool.h:486-502`);
  `kkt_factorization.cpp:67` "the factor validates first" (both adapters call
  `validate_num_threads` before mutating); `soc_elastic_restoration.cpp:191` "SqpDriver never
  reads QpSolution::z"; the slack-ceiling / `is_runaway` claim; `continuation.cpp`'s
  x0-ignored-on-warm-resolve, driver-owned `probe_budget_stops`, and predict()-reads-the-model.
- **`probe_budget_stops` is written in exactly one place** (`src/drivers/sqp_driver.cpp:1190`),
  so "set by, and only by, the driver's own budget exit" is true. The pointer says
  "(sqp_driver.h)" while the write now lives in the .cpp — pre-existing, and the header
  (:3138, :3170) is where the contract is documented, so it still resolves. Informational only.
- **New sentence added in `symmetric_factor_accelerate.cpp:540-543`** ("committing the new
  session only after the backend succeeds means a failed analysis leaves this engine exactly
  as it was") is TRUE: `session->analyze(A)` at :557 precedes `session_ = std::move(session)`
  at :559.
- **`accelerate_session.cpp:157` "the same invariant every backend session guarantees"** is
  TRUE — `pardiso_session.cpp:434` sets `has_numerics_ = false` up front. The narrowed
  `pardiso_session.cpp:429-433` write list is also TRUE (`release()` :181, `analyze()` :396,
  ctor seed :139).
- **Deleted callouts that a reader might miss are all carried by the owning header**, so the
  deletions are legitimate de-duplication, not loss: the MKL `cnr_threads`/`kBackendDefault`
  "floats with the linked MKL … is 3" rationale (`symmetric_factor.h:539-544`);
  `set_num_threads is startup-only` (`thread_pool.h:473`); the `ip_activity_threshold`
  stay-in-the-header rationale (`warm_start.h:437-439`); `hven_pch.h`'s "byte-identity
  property described above" still resolves at :22-32.
- **Cross-references resolve**: `THE SHRINK IS APPLIED TO THE STEP THAT ACTUALLY FAILED`
  (`continuation.h:51`), `BUDGET EXHAUSTION` (:73), `kProbeBudgetFloor = 200` (:420),
  `ContinuationOptions::probe_budget` (:268), `suspend_growth_after_failure` (:281),
  `src/CMakeLists.txt:21-29` (the two-adapter split),
  `elastic.h:169` (`set_elastic_penalty`), the funnel five-conjunct class note
  (`globalization.h:310+`), both funnel test names, `CMakeLists.txt:449`
  (`HVEN_VERSION_STRING`).
- **No residual labels** (M-numbers, phase-C, T-series, Task N, S-/W-numbers, fix rounds,
  review-record/census/bit-identity language) survive in any of the 17 files.
- **No `///` docstring on a definition duplicates its header's** — the only `///` blocks in
  the touched set are `thread_pool.cpp:12-22` (TU-local statics, unchanged) and the
  `get_core_count.cpp` attribution in nit 8.
- **Provenance/licence blocks untouched**: Boost (`get_core_count.cpp:1-9`), Eigen MPL-2.0
  (`accelerate_session.cpp:1-11`, `pardiso_session.cpp`).
