# SQP linear-solver retarget design (M3 phase B)

> **SURFACE VERDICT (adjudicated by the M3 plan rev B §4; two additional
> gaps found by this note's own source verification, each with a proposed
> disposition awaiting the same review):** the SQP engine's consumed
> backend surface is the near-default seam the consumed-surface audit
> confirmed exactly ([`docs/consumed-surface-audit.md:59-75`](consumed-surface-audit.md)),
> and `hven::linear::SymmetricFactor::Options` covers all of it EXCEPT:
>
> 1. **`max_refinement_iters` and `pivot_perturb_exp` cannot express
>    don't-write** (RULED, plan §4.1, option (a)). The SQP seam never
>    writes `iparm[7]` or `iparm[9]`; hven currently writes both
>    unconditionally (`src/linear/pardiso_session.cpp:231-236`). On the
>    audited MKL the effective values the seam actually runs under are
>    `pardisoinit`'s own — refinement cap **2** (the audit's runtime shim
>    observed it: `consumed-surface-audit.md:145-151`) and pivot exponent
>    **8** (corrected from this note's original 13; see the §2.4
>    amendment) — so mapping `max_refinement_iters = 0` would CHANGE the
>    engine's effective refinement and break the 57-cell census. Both
>    options grow an explicit don't-write state (§2 below proposes the
>    mechanic), guarded by a `pardisoinit`-defaults canary in the same
>    commit. **This touches the Pardiso `iparm` surface and therefore
>    requires explicit human review before merging (CLAUDE.md §6), and
>    the `hven::linear` commit gets a scoped tycho-side review lane
>    (plan §4.1).**
> 2. **Accelerate zero tolerance (gap found here, not in the plan).** The
>    plan's §4.1 table says "everything else absent/default". That is
>    wrong for Accelerate: the SQP Accelerate seam writes
>    `zeroTolerance = 1e-4 * eps` on every numeric factorization
>    (`include/hven/detail/sqp/kkt_system_accelerate.h:393`), while hven at
>    its default `pivot_perturb_exp = 8` derives `1e-8 * eps` — four orders
>    of magnitude tighter, a divergence hven's own Accelerate session
>    documents against exactly this precedent
>    (`src/linear/accelerate_session.cpp:180-190,210-214`). Left
>    "absent/default", phase B would silently change the Accelerate
>    zero-pivot threshold — an unlicensed numerical delta under the §9
>    ledger. §2.3 proposes the disposition (fold Apple's documented
>    default into the new don't-write state's Accelerate semantics).
> 3. **`DenseSymmetricFactor` cannot host the Schur border as-is (gap
>    found here, not in the plan).** The border factors `C` with
>    `LAPACKE_dsytrf(..., 'L', ...)` — the LOWER triangle
>    (`include/hven/detail/sqp/schur_complement.h:307`; the solve side's
>    `dsytrs` uses `'L'` likewise, `schur_complement.h:174`) — while
>    `DenseSymmetricFactor` is upper-triangle-only
>    (`include/hven/linear/dense_symmetric_factor.h:24-35`); Bunch-Kaufman
>    'L' and 'U' eliminate in different orders, so switching uplo changes
>    floats, which §9 does not license. The border also needs a
>    NON-THROWING exact-singular outcome (`schur_complement.h:313-325`
>    treats `info > 0` as state; `dense_symmetric_factor.h:30-34` throws)
>    and the Bunch-Kaufman block-eigenvalue walk
>    (`schur_complement.h:330-359`), which reads the factor internals
>    `DenseSymmetricFactor` hides. §6 proposes the surface growth; the
>    dense header itself anticipates it ("no inertia surface, no counters
>    (added only when a consumer reads them)",
>    `dense_symmetric_factor.h:6-9`). Same review lane as item 1.
>
> Everything else maps exactly: ordering `kBackendDefault` is EXACT ACT
> parity on both backends (the seam never writes `iparm[1]`, and its
> Accelerate twin requests `SparseOrderDefault`,
> `kkt_system_accelerate.h:348`, which is precisely what hven's
> `kBackendDefault` passes); `weighted_matching = false` is don't-write =
> exact parity; the Accelerate pivot tolerance is the same fixed 0.01 on
> both sides (`kkt_system_accelerate.h:392`;
> `accelerate_session.cpp:203`); and the phase-33x force-zero-restore rule
> dissolves into `solve_partial()`'s per-call refinement-off contract
> (§5). None of these mappings is licensed as a golden-rig numerical
> delta; the licensed deltas are enumerated in §11 and nowhere else.

**Authority and scope.** This note expands the M3 plan rev B's §4 design
skeleton (the reviewer-side note
`docs/notes/2026-08-14-hven-m3-plan-revB.md` in the sibling archive,
§4 and §9) into the phase-B edit plan, mirroring the M2 precedent
([`docs/retarget-design.md`](retarget-design.md)) in shape: surface
verdict → configuration mapping → ownership/lifecycle → evidence
plumbing → solve mapping → call-site inventory → hazards →
behavioral-delta ledger. The plan's §4 rulings are SETTLED and are
implemented here, not reopened. The authoritative consumption inventory
is [`docs/consumed-surface-audit.md`](consumed-surface-audit.md) (SQP
seam: clean, confirmed exactly). The migration consequences of the
amended docket
[`docs/dockets/2026-08-09-sqp-inertia-before-factorization.md`](dockets/2026-08-09-sqp-inertia-before-factorization.md)
are discharged in §4. Every file:line below was verified against this
repository at phase-A HEAD (`0ba1b9f`); where a plan-quoted line number
has drifted, the drift is noted.

**Review status.** The execution review (2026-08-14; the reviewer-side
note `docs/notes/2026-08-14-m3-gate-a-and-phase-b-review.md`) returned
**APPROVED WITH CHANGES** on this note. Its three REQUIRED changes are
folded into §6.3, §4.1 and §7.2, its open-item rulings are recorded in
§12, and its consolidated gate-B checklist is merged into §11.1. Every
post-approval fold carries a bracketed provenance tag naming the review
section it came from, so a reader can tell approved text from folded
text.

**What dissolves.** `include/hven/detail/sqp/kkt_system.h` (the MKL
Pardiso `KktSystem`, with the platform dispatch at its lines 5-7),
`kkt_system_accelerate.h` (the Accelerate twin), and `lapacke_shim.h`
(the SQP copy of the Accelerate LAPACKE shim, global-scope, duplicating
`include/hven/detail/linear/lapacke_shim.h`'s namespaced twin — the
phase-A review carry). `KktSystem`'s consumers move onto
`hven::linear::SymmetricFactor`
([`include/hven/linear/symmetric_factor.h`](../include/hven/linear/symmetric_factor.h));
`schur_complement.h`'s dense LAPACKE border moves onto
`hven::linear::DenseSymmetricFactor`
([`include/hven/linear/dense_symmetric_factor.h`](../include/hven/linear/dense_symmetric_factor.h)).
`schur_complement.h` itself survives phase B (its move to `kkt/` is
phase-C restructure); only its linear-algebra internals change.

## 1. Configuration mapping — the near-default seam

`KktSystem` consumes NO engine option at all: it stores `QpOptions` and
never reads it (`kkt_system.h:115,127-130`; the Accelerate twin likewise,
`kkt_system_accelerate.h:153,201`). Its entire backend configuration is
`pardisoinit` plus one adapter-internal write, `iparm[34] = 1`
(zero-based CSR, `kkt_system.h:279`), which hven's session performs
identically (`pardiso_session.cpp:228-230`). The audit's SQP table
(`consumed-surface-audit.md:59-75`) is the proof nothing else is
touched: the seam never writes `iparm[1]`, `[9]`, `[10]`, `[12]`,
`[20]`, `[23]`, `[24]`, `[33]`.

| `SymmetricFactor::Options` member | Value for parity | Grounding and exact effect |
| --- | --- | --- |
| `kind` | `FactorKind::kLDLT` | `KktSystem` is mtype = -2 on MKL (`kkt_system.h:92`) and `SparseFactorizationLDLTTPP` on Accelerate (`kkt_system_accelerate.h:132`); hven's kLDLT is exactly this pair. |
| `num_threads` | `0` (backend default) | The seam has no per-instance thread control anywhere (audit, `consumed-surface-audit.md:59-75`: no thread rows for this seam); asserted runs pin threads process-wide via `MKL_NUM_THREADS=1` (the docket's own provenance stamp records `process-global=1`), and that test-side env-pin discipline continues unchanged. `0` leaves the backend default alone (`symmetric_factor.h:247-260`) — exact act parity. |
| `pivot_perturb_exp` | **the NEW don't-write state** (§2) | Never wrote `iparm[9]`; effective exponent is `pardisoinit`'s (8 on the audited build — canary, §2.4; corrected from this note's original 13, see the §2.4 amendment). hven today writes it unconditionally (`pardiso_session.cpp:234-236`), so the state must be grown first. |
| `max_refinement_iters` | **the NEW don't-write state** (§2) | Never wrote `iparm[7]` except the phase-33x save/zero/restore (`kkt_system.h:327-335`), which §5 dissolves; effective FULL-solve cap is `pardisoinit`'s (2 on the audited build, observed by the audit's runtime shim, `consumed-surface-audit.md:145-151`). The census pins depend on that effective cap — the refinement-default parity carry closes here. |
| `ordering` | `Ordering::kBackendDefault` | Never wrote `iparm[1]` — don't-write is EXACT ACT parity, unlike the IPM's explicit METIS. On Accelerate the twin requests `SparseOrderDefault` (`kkt_system_accelerate.h:348`), which is literally what hven passes at `kBackendDefault` (`symmetric_factor.h:277-282`) — exact parity there too, so the IPM's Accelerate-ordering migration hazard (`symmetric_factor.h:296-300`) does NOT apply to this seam. |
| `weighted_matching` | `false` | Never wrote `iparm[12]`; `false` writes nothing (`symmetric_factor.h:309-319`). Also keeps `supports_partial_solve()`'s matching conjunct inert (§5). |
| `matrix_scaling` | `false` | Never wrote `iparm[10]`; `false` writes nothing (`symmetric_factor.h:321-346`). |
| `pivot_strategy` | `PivotStrategy::kBackendDefault` | Never wrote `iparm[20]`; `kBackendDefault` writes nothing (`symmetric_factor.h:348-389`). |
| `factorization_algorithm` | `FactorizationAlgorithm::kBackendDefault` | Never wrote `iparm[23]` (`symmetric_factor.h:391-415`). |
| `solve_parallelism` | `SolveParallelism::kBackendDefault` | Never wrote `iparm[24]` (`symmetric_factor.h:417-465`). |
| `cnr_threads` | `0` | Never wrote `iparm[33]`; `0` writes nothing (`symmetric_factor.h:467-499`). |
| `collect_factor_mflops` | `false` | The seam never reads `iparm[18]`/`iparm[17]` (audit read rows: `iparm[7]` — the phase-33x save half — plus `iparm[13]/[21]/[22]`; no factor-size or cost row) and reports no factor size or cost anywhere. **`false` suppresses the `iparm[18]` (Mflop-cost) request and nothing else** — hven's analyze writes `iparm[17] = -1` (the nonzero-count request) **unconditionally**, on every analyze, gated by no `Options` field at all (`pardiso_session.cpp:361-370`; the flag's own gate is `pardiso_session.cpp:375`). That unconditional write is an old-vs-new difference in its own right and is carried as a named §11 ledger row, not by this row's `false`. [corrected at gate-B dual review] |
| `accelerate_zero_tolerance` | `std::nullopt`, **given §2.3's Accelerate semantics for the pivot-exponent don't-write state** | The twin writes Apple's own documented default `1e-4 * eps` (`kkt_system_accelerate.h:383-393`, whose comment says exactly that it restates the documented default). With §2.3, don't-write on Accelerate supplies precisely that value, so no override is needed and one platform-neutral factory serves both backends. If the reviewer rejects §2.3, the fallback is an explicit Accelerate-only `accelerate_zero_tolerance = 1e-4 * std::numeric_limits<double>::epsilon()` (the IPM's own mapping shape, `retarget-design.md` row 13) in a platform-split factory. |

The Accelerate pivot tolerance needs nothing: the twin writes the fixed
0.01 (`kkt_system_accelerate.h:392`) and hven's Accelerate session uses
the same fixed value with no option (`accelerate_session.cpp:176-178,203`).

The intended factory — ONE function, platform-neutral, replacing nothing
(the seam had no factory to replace):

```cpp
// hven::solvers::detail — the SQP engine's proven linear configuration.
inline hven::linear::SymmetricFactor::Options sqp_kkt_options() {
    hven::linear::SymmetricFactor::Options o;
    o.kind = hven::linear::FactorKind::kLDLT;
    // 0 = backend default, and the test-side process-wide MKL_NUM_THREADS
    // pin is what makes runs reproducible. That pair is the intended and
    // sufficient mechanism: no consumer moves the count at runtime, so no
    // per-instance thread control is exposed. [Comment required by the
    // execution review (2026-08-14), §2.4 addendum-(c) ruling.]
    o.num_threads = 0;
    o.pivot_perturb_exp = std::nullopt;    // NEW don't-write state, §2
    o.max_refinement_iters = std::nullopt; // NEW don't-write state, §2
    // Every remaining member keeps its default, and every default is
    // don't-write / absent -- the audit's SQP table is the proof this
    // covers the whole consumed surface.
    return o;
}
```

## 2. The two new don't-write states (RULED option (a); mechanic proposed here)

### 2.1 The mechanic: `std::optional<int>`, defaults unchanged

Both members become `std::optional<int>`:

```cpp
std::optional<int> pivot_perturb_exp = 8;
std::optional<int> max_refinement_iters = 0;
```

Arguments, in order of weight:

- **`0` is a meaningful written value for both entries** — the IPM engine
  writes `iparm[7] = 0` deliberately (`retarget-design.md`, mapping row
  4) — so a `0`-as-sentinel scheme (the `num_threads`/`cnr_threads`
  shape) is unavailable, and a `-1` sentinel would collide with
  `iparm[18]`-style negative request codes in readers' minds while
  standing for nothing Pardiso documents for these entries.
- **The surface already has the optional-means-don't-write precedent**:
  `accelerate_zero_tolerance` is `std::optional<double>` with `nullopt`
  = don't-write (`symmetric_factor.h:540-551`), and the session's own
  internal config already models every don't-write enum as an optional
  (`pardiso_session.cpp:237-262,283-309`: `cfg_.ordering.has_value()`
  etc.). The public mechanic converges with the internal one.
- **Defaults stay `8` and `0`, NOT `nullopt`.** Flipping the defaults to
  don't-write would change every existing default-constructed
  `Options` — the IPM mapping and the entire `tests/linear` pin suite
  (e.g. `test_symmetric_factor.cpp:383`'s refinement pins assume the
  written cap) — for zero benefit: the ruling requires the STATE to be
  expressible, not to be the default. Zero IPM-side change, exactly as
  the plan requires.

Per-backend semantics rows (the knobs discipline's mandatory rows):

| Member / state | MKL Pardiso | Apple Accelerate |
| --- | --- | --- |
| `pivot_perturb_exp`, present `k` | writes `iparm[9] = k` (today's unconditional write at `pardiso_session.cpp:234-236` becomes the present-value branch) | `zeroTolerance = 10^-k * eps`, the existing formula (`accelerate_session.cpp:210-214`), unchanged |
| `pivot_perturb_exp`, `nullopt` | `iparm[9]` untouched — `pardisoinit`'s own value survives (8 on the audited build; canary §2.4, corrected from 13) | `zeroTolerance = 1e-4 * eps` — Apple's OWN documented default (the formula at `k = 4`, which `accelerate_session.cpp:180-182` already identifies as Apple's default), passed explicitly because Accelerate takes an options struct and has no untouched-entry mechanism. "Don't-write" on this backend MEANS "the backend's documented default value", the closest expressible act — see §2.3. |
| `max_refinement_iters`, present `n` | writes `iparm[7] = n` (today's `pardiso_session.cpp:231-233` becomes the present-value branch). Partial solves still force `0` for the call and restore, regardless of the resting value (`pardiso_session.cpp:455-459`) — the restore target simply becomes whatever the entry held, written or inherited. | no refinement mechanism exists in the session at all (`accelerate_session.cpp` contains no refinement handling — verified by search); the value produces no backend act, and `SolveInfo::refinement_iters` stays absent. Same store-only treatment `num_threads` already has there. |
| `max_refinement_iters`, `nullopt` | `iparm[7]` untouched — `pardisoinit`'s cap survives (2 on the audited build; canary §2.4). Partial solves force `0`/restore exactly as above, which reproduces the dissolved seam's save/zero/restore acts (`kkt_system.h:327-335`) against the inherited cap — the same acts the audit's runtime shim verified (`consumed-surface-audit.md:123-156`). | as the row above — no mechanism, no act. |

### 2.2 Act evidence for the new states

The don't-write claim must be pinned as an ACT, not inferred from a
boundary read that cannot distinguish "left it alone" from "wrote the
same value" — the exact problem the sanctioned `PardisoIparmObserver`
deviation exists for (`pardiso_session.cpp:243-262`;
`docs/testing.md`'s deviation register). The two new conditional writes
get the same did-the-write-execute observer fields the `ordering` write
has (`pardiso_session.cpp:258-261`), in the same file, under the same
`HVEN_TESTING` guard. This extends the existing sanctioned deviation:
the byte-identical-object / no-symbol proof is re-run and re-recorded,
`docs/testing.md`'s deviation list is updated, and the `notices/` entry
for the session file records the change (CLAUDE.md §6's deviation terms,
all of them).

### 2.3 The Accelerate semantics of don't-write, argued

On MKL, don't-write has a literal referent: the `iparm` entry is not
assigned. Accelerate has no such thing — hven always populates
`SparseNumericFactorOptions`, so SOME value is always passed
(`accelerate_session.cpp:201-214`). The two candidate meanings for
`pivot_perturb_exp = nullopt` there:

- **(chosen) pass Apple's own documented default, `1e-4 * eps`.** This
  is what "leave the backend's default in force" honestly means on a
  backend configured by struct rather than by entries, it is the same
  value Apple documents in `SolveImplementation.h` (restated at
  `kkt_system_accelerate.h:383-387` and `accelerate_session.cpp:174-182`),
  and it makes the SQP factory platform-neutral (§1): the dissolved
  Accelerate twin wrote exactly this value, so parity is exact by
  construction. The value is DOCUMENTED by Apple, not fabricated — the
  never-fabricate rule governs observations, and citing Apple's own
  documented default is citing a document, not inventing a measurement.
- (rejected) keep deriving from some exponent anyway — there is no
  exponent present to derive from; any invented fallback exponent would
  be exactly the fabricated-value shape the surface forbids.

`max_refinement_iters = nullopt` on Accelerate needs no such choice: no
mechanism exists to configure (row above).

### 2.4 The rider: the `pardisoinit`-defaults canary (load-bearing)

Modeled directly on
`BackendDefaultPremise.MklPardisoinitLeavesScalingAndCnrAtZero`
(`tests/linear/test_fault_injection.cpp`; argued at
`docs/testing.md:478-503`): the same commit that grows the don't-write
states adds a canary asserting, from the post-`pardisoinit` capture
point (`pardiso_session.cpp:219-226` — the only point that answers
"what did `pardisoinit` default this to", before phase 11 can
overwrite), that on the linked MKL:

- `iparm[7]` (refinement cap) is **2**, and
- `iparm[9]` (pivot perturbation exponent) is **8**.

**[Amendment, 2026-08-14, B1 implementation — a declared correction, not a
silent one.]** This note originally predicted **13** for `iparm[9]`, and the
canary's own first run falsified it: the post-`pardisoinit` capture observes
**8** on the linked MKL (oneAPI 2026.1, mtype = -2), a standalone
`pardisoinit()` probe outside hven's session code confirms 8 (and reproduces
13 only for mtype = 11), Intel's oneMKL Developer Reference documents 13 as
the NONSYMMETRIC default and 8 for symmetric indefinite matrices. The repo's
own prior records (`docs/retarget-design.md`'s `pivot_perturb_exp` row;
`consumed-surface-audit.md:87-90`) already recorded 8 too, but for a
different quantity -- the engine's WRITTEN default for `qp_pivot_perturb_`,
not `pardisoinit`'s own value -- so that match is corroborative by
coincidence of value, not independent evidence of the `pardisoinit` default;
the live canary, the standalone probe, and Intel's mtype-specific
documentation carry the correction on their own. The original 13
was an Intel-doc misread (the mtype = 11 row). Parity consequence: benign —
the dissolved seam's effective exponent was 8, which coincides in value with
the 8 the IPM engine and hven's default both write, so the don't-write state
differs from the written default as an ACT but not in effective value on
this MKL; no census implication. The canary asserts 8; the two §1/§2.1
table mentions of 13 are corrected in place with pointers here. This is
exactly the canary doing the job this section assigned it — converting the
assumption into a test — one commit earlier than anticipated.

`PardisoIparmObserver`'s `post_pardisoinit_*` trio grows two fields for
this (`post_pardisoinit_refinement_cap_iparm`,
`post_pardisoinit_pivot_perturb_iparm`) — part of the same deviation
extension as §2.2. The canary is load-bearing, not decorative: the
57-cell walk census's byte-identity depends on the effective refinement
cap the don't-write state inherits, so an MKL default move must become a
failing test, never a silent census break. This is the exact
`iparm[10]/[33]` precedent (`docs/testing.md:107-115`) applied to the
two entries this seam's parity rests on.

## 3. Ownership and lifecycle

### 3.1 What each consumer owns after the retarget

`KktSystem` appears in exactly four ownership positions, all of which
become a `SymmetricFactor` (move-only, like the class it replaces —
`symmetric_factor.h:597-602`; `kkt_system.h:39-42`) constructed from
`sqp_kkt_options()`:

| Owner | Site | Notes |
| --- | --- | --- |
| `BorderState::kkt` (border mode's persistent K0 factor) | `qp_engine.h:2063` | By value, inside the non-copyable, non-movable `BorderState` (`qp_engine.h:2053-2058`) — the shared-ownership/Pardiso-handle-safety argument (`qp_engine.h:2093-2121`) transfers verbatim: `SymmetricFactor`'s destructor releases the backend session exactly once, `shared_ptr<BorderState>` still guarantees it runs after the last holder. |
| `SsnEngine::kkt_` (held across solves) | `ssn_engine.h:3560` | The "one symbolic analysis per structure" property (`ssn_engine.h:228-229`) is preserved by §3.2's explicit-analyze mechanic. |
| Per-run elimination-path locals | `qp_engine.h:2502,2944` | Constructed per `run()` / per tier-3 refine, reused across iterations within it. |
| Predictor per-call local | `predictor.h:961-963` | One analyze+factorize ("THE one factorization"), then a `SchurComplement` over it. |

There is NO separate assembly-buffer change (unlike the IPM retarget's
`kkt_matrix_`): every consumer already owns its matrix and passes it
explicitly (`border.k0.K`, `SsnEngine::k_`, `solve_eqp`'s `asm_.K`,
predictor's `k0.K`). Every one of those matrices IS `hven::SpMatRM`
(`Eigen::SparseMatrix<double, Eigen::RowMajor>`, defined once in
`include/hven/core/types.h:44`): the SQP layer used to spell the type
`hven::solvers::SpMatU`, M3 phase-C S1 made that name an alias OF
`hven::SpMatRM`, and phase-C S2b deleted the alias and moved every call
site onto the library's own name. What survives in
`include/hven/qp/qp_types.h` is the `static_assert` that matters at this
boundary — that `hven::SpMatRM::StorageIndex` stays 32-bit, which is what
the sparse backends are built against. Matrices therefore flow into
`analyze()`/`factorize()` with no conversion. The
structural-diagonal requirement `analyze()` validates
(`symmetric_factor.h:29-32,660-670`) is satisfied by construction:
every K this engine builds emits its diagonal unconditionally, value
zero or not (`kkt_assembly.h:188,204,221`; `ssn_engine.h:2927-2946`,
the `for_each_entry` walk).

### 3.2 Reproducing `KktSystem::factorize`'s auto-analyze, observably

Old contract: `factorize(K)` re-runs `analyze()` first iff K's pattern
differs from the last analyzed matrix (`kkt_system.h:291-298`, via
`pattern_matches`, `kkt_system.h:197-203`), and `QpCounters::
symbolic_analyses` is counted at the call sites by probing
`pattern_matches` BEFORE `factorize()` (`types.h:269-286`;
`qp_engine.h:4509-4516`; `ssn_engine.h:2154-2156,2512-2514`). hven's
contract is deliberately different: `factorize()` NEVER re-analyzes and
throws on a foreign pattern (`symmetric_factor.h:672-683`), and
`SymmetricFactor` exposes no `pattern_matches` probe.

The retarget therefore carries the decision to the consumer side, as a
small engine-owned lifecycle helper — proposed as a struct plus three
free functions in a new header `include/hven/detail/sqp/kkt_calls.h`
(name and placement are implementer-adjustable at execution review;
phase C's restructure will re-home it):

```cpp
struct KktFactor {
    hven::linear::SymmetricFactor factor;   // from sqp_kkt_options()
    std::uint64_t analyzed_pattern = 0;     // hven::pattern_hash of the analyzed K
    bool analyzed = false;
};
// True iff factorize_checked() would run an analysis for K --
// the direct replacement for the call sites' pattern_matches probe,
// consulted (as today) BEFORE the factorize for symbolic_analyses.
bool needs_analysis(const KktFactor &k, const SpMatRM &K);
// analyze-iff-needed, then factorize; throws std::runtime_error on a
// kBackendError outcome (§3.3) -- KktSystem::factorize()'s contract.
hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K);
// allocate-and-solve, matching KktSystem::solve's Vec-returning shape.
Vec solve_vec(const KktFactor &k, const Vec &rhs);
```

This is deliberately NOT a compatibility adapter in M2's sense: it
carries no evidence projection (§4 adopts `InertiaEvidence` directly),
no backend knowledge, and no configuration — it is call-shape
preservation for the lifecycle decision the dissolved class made
internally. `needs_analysis` compares `hven::pattern_hash(K)`
(`include/hven/core/pattern_hash.h:86`) against the recorded key. The
decision is equality-based, exactly as the dissolved
`hash_pattern`/`pattern_matches` pair's was (`kkt_system.h:176-203`), so
reuse DECISIONS are preserved even though hash VALUES differ
(`pattern_hash` additionally mixes `cols`; both hashes are
deterministic in the pattern, so equal patterns compare equal under
either). No pin asserts a hash value — that standing belief is §5 of
the plan's survey obligation, re-confirmed for these sites at execution.
Phase B does NOT re-key `detail::structural_hash`/`values_hash`
(`qp_engine.h:1991-2007`) or the SSN structure key
(`ssn_engine.h:2951-3019`) — those are phase-C §5 work and stay
byte-identical here.

> **AMENDMENT — M3 phase C, task B2 (2026-08-15).** The shape above grew
> **one struct and one overload**, additively: `AnalysisDecision` (the
> `needs_analysis` answer plus the `pattern_hash` it was taken on) and
> `factorize_checked(k, K, decision)`. `KktFactor`, `needs_analysis`,
> `factorize_checked(k, K)` and `solve_vec` are unchanged, and the
> `symbolic_analyses` call-site counting contract is unchanged — the
> three counting call sites (`ssn_engine.h` ×2, `qp_engine.h`'s
> `rebuild_k0`) still take the decision BEFORE the factorize and still
> count off it; they now hand it in instead of letting
> `factorize_checked` retake it. **Reason:** as designed above, steady
> state hashed the pattern THREE times per SSN major where the dissolved
> seam hashed it twice, and B1 priced the extra pass at 6.12–7.76 % of
> SSN-major wall-clock on the SSN-heavy families
> ([`notes/data/2026-08-15-m3-b1-hash-cost/`](notes/data/2026-08-15-m3-b1-hash-cost/report.md)),
> over the phase-C plan's 5 % gate-blocking bar. Steady state is now two:
> the decision's own, and `SymmetricFactor::factorize`'s pattern guard,
> which is `hven::linear`'s published contract and is the floor B2 does
> not touch. No decision, counter, or float moves; proven by P-SUITE
> (Release + Debug) and P-BENCH
> ([`notes/data/2026-08-15-m3-b2-hash-removal/`](notes/data/2026-08-15-m3-b2-hash-removal/report.md)).
> **P-CENSUS did not run for B2** and was not owed: the SQP execution reviewer's
> census-frequency ruling (2026-08-15) routes intermediate content-change
> censuses into one cumulative counter census after H3, with the full 57-cell
> bar unchanged at Gate C. That artifact's §4 records the disposition.

### 3.3 Error mapping — throws stay throws

Old `KktSystem` converts every nonzero Pardiso error to
`std::runtime_error` (`run_phase`, `kkt_system.h:255-257`), and its
consumers' control flow is built on that: `SsnEngine` catches around
`factorize` and routes `kNumericalError`/`kSingular`
(`ssn_engine.h:2153-2187,2511-2524`), border mode's fallback
discriminates singular-C throws from genuine backend throws
(`qp_engine.h:4020-4051`), and `rebuild_k0`'s throw path is what the
engine's invalidation policy is written against (`qp_engine.h:4489-4506`).
hven instead RETURNS a backend failure in `FactorizeOutcome`
(`symmetric_factor.h:177-195`). `factorize_checked` restores parity:
`Status::kBackendError` becomes a thrown `std::runtime_error` carrying
`backend_code`. The message TEXT necessarily changes (the old
`"pardiso phase 22 failed: error {}"` names a phase that no longer
exists at this layer); no test or census pins that text (verified: the
suite's `escape_detail` assertions match engine-authored substrings
only, e.g. `tests/sqp/test_ssn_engine.cpp:224,2065,2840`), and §11
records the residual exposure for the gate.

`analyze()` failures already throw on both sides
(`kkt_system.h:284` via `run_phase`; `symmetric_factor.h:660-670`) —
no mapping needed. The Accelerate twin's factorize-throws-on-singular
behavior (`kkt_system_accelerate.h:402-415`) maps onto the same
`kBackendError`-to-throw conversion; the status code text changes
identically.

### 3.4 Release and the stale-triple question

Old `release()` is PRIVATE (`kkt_system.h:113`), reachable only from
the destructor, `analyze()`'s top, and move-assignment
(`kkt_system.h:132,146-149,267-271`). hven has no `release()` to call
and none is needed: `analyze()` starting a fresh session subsumes the
pattern-change release, and destruction releases the session. The
release-path audit this closes is §4.4.

## 4. Evidence plumbing — M3 adopts `InertiaEvidence` directly

This is where M3 deliberately differs from M2: no compatibility cache,
no integer projection. The verdict consumers are rewritten to consume
`hven::linear::InertiaEvidence` (`symmetric_factor.h:100-123`), read
via `SymmetricFactor::inertia()` (`symmetric_factor.h:741-745`), and
the compiler enforces the classification the amended docket's
consequence 1 demands: once the type carries the state, no call site
can skip the question.

### 4.1 The two verdict helpers, rewritten

`detail::inertia_verdict` (`qp_engine.h:1679-1695`) and
`detail::ssn_inertia_verdict` (`ssn_engine.h:996-1007`) are the same
function restated (`ssn_engine.h:980-989` says so and why); both test
perturbed pivots FIRST. Rewritten signature:
`(const hven::linear::InertiaEvidence &e, Index expected_pos, Index expected_neg)`,
call sites passing `kkt.factor.inertia()`. Semantics:

```
1. e.state != kObserved                       -> kSuspect   (NEW explicit route)
2. e.perturbed_pivots present and != 0        -> kSuspect   (old rule, MKL)
3. e.n_pos + e.n_neg != expected_pos+neg      -> kSuspect   (old short-sum rule)
4. exact match -> kOk, else -> kWrong                        (old rule)
```

Rule 2 consumes the optional's ABSENCE by not fabricating anything:
absent perturbed-pivot evidence (Accelerate) simply does not trigger it.
This preserves every verdict on BOTH backends:

- **MKL, post-factorization:** state is `kObserved`, `perturbed_pivots`
  is present with Pardiso's own count — rules 2-4 are bit-for-bit the
  old helper. No delta.
- **MKL, pre-factorization / failed factorization:** the old helper read
  the constructor-zeroed array and returned a verdict computed from
  `(0, 0, ppiv 0)` — the docket's CONFIRMED defect (P5's `kObserved`
  zero triple). Rule 1 now routes `kUnavailable` to `kSuspect`
  explicitly. The preservation argument is **identical-verdict-in,
  identical-action-out**: the zero triple produced `kSuspect` under the
  old helper's short-sum rule at every reachable site (a real system has
  `expected_pos + expected_neg = dim > 0`), and produces `kSuspect`
  under rule 1, so every downstream consumer sees the SAME verdict
  stream and takes the same action. No DECISION moves — the delta is the
  evidence row, licensed in §11.

  [Folded per the execution review (2026-08-14), §2.2. This bullet
  previously argued that `kSuspect` "DOES NOT ACT anywhere in either
  engine", citing `qp_engine.h:4133-4137` and `ssn_engine.h:2215-2220`.
  **That claim is false as stated** and is withdrawn: those two sites are
  places kSuspect deliberately doesn't act, not the whole engine. The
  section-4b **suspect-stall gate DOES act on kSuspect at certification
  time** (archived tag, `qp_engine.h:3287-3345`): a kSuspect verdict
  triggers the free-block stationarity check, and on failure escalates
  `primal_delta` by `kSuspectDeltaFactor`, rebuilds K0, and counts
  `counters.suspect_escalations` — a PINNED counter with per-backend arms
  (the standing rule from the Accelerate audit: tests asserting
  `suspect_escalations == 0` on exactly-singular fixtures need
  per-backend arms). The conclusion never needed the inert-verdict
  premise; it rests on identical-verdict, as now written. Consequence
  carried into the gate: `suspect_escalations` is an explicit
  gate-blocking check at gate B (§11.1).]
- **Accelerate, zero-pivot factorization:** the old twin mapped Apple's
  ZERO-pivot count into `num_perturbed_pivots()`
  (`kkt_system_accelerate.h:109-123,521`) — a *differently-valued*
  reading A.5's degradation rule forbids, which is the A.5 rewrite
  obligation this section discharges. After the rewrite the same
  factorization lands on `kSuspect` through rule 3 instead: Accelerate
  reports all three counts natively (`zero_is_derived == false`), so a
  nonzero zero class makes `n_pos + n_neg` come up short by exactly
  that count. Same verdict, honest channel. (Note the plan's §9 table
  abbreviates this as "present-0 → absent"; the seam's Accelerate value
  was the zero-pivot count, not a hardcoded 0 — the plan's §4.2 body
  states it correctly, and this note follows the body.)
- **Accelerate, degraded reads:** the old twin's not-ready degradation
  returned `(pos 0, neg 0, ppiv n_)` (`kkt_system_accelerate.h:505-521`)
  — deliberately suspect-in-every-consumer. Rule 1 reproduces
  `kSuspect` from `kUnavailable`/`kQueryFailed` directly.

The one behavioral asymmetry, stated so the Mac leg can prove it: on
MKL, rule 3 fires on the DERIVED zero class exactly as the old short-sum
rule did (`zero_is_derived == true`, `symmetric_factor.h:108-115`,
docket consequence 3 — unaffected); on Accelerate it fires on the
MEASURED one. No rule consults `n_zero` directly, so no
`zero_is_derived` conditional is needed — the sum rule is the same
arithmetic the old helpers ran.

### 4.2 Every `num_pos_eigs()`/`num_neg_eigs()`/`num_perturbed_pivots()` call site, classified

Per docket consequence 1. "Assert-observed" means the site is
unreachable before a successful factorization and asserts
`state == kObserved`; "route-explicit" means the unavailable case is
handled as its own branch.

| Site | Today | Classification |
| --- | --- | --- |
| `qp_engine.h:1681,1684-1685` | verdict helper body | rewritten per §4.1 (route-explicit via rule 1) |
| `ssn_engine.h:998,1001-1002` | verdict helper body | rewritten per §4.1 |
| `qp_engine.h:3970` | `border.kkt.num_perturbed_pivots() != 0` gates K0 rebuild, deliberately reading the PREVIOUS solve's factorization (`qp_engine.h:383-385`) | short-circuit already guarantees a successful factorization exists (`border.schur.has_value()` implies `rebuild_k0` completed). Rewrite: read `border.kkt.factor.inertia()`; rebuild if `state != kObserved` (defensive, unreachable today) or perturbed present-and-nonzero or (natively-observed) `n_zero != 0` — the Accelerate twin's zero-pivot trust signal, preserved through the honest channel. Assert-observed in spirit, route-explicit in code. |
| `qp_engine.h:2506` (via `inertia_verdict` after `solve_eqp`) | reachable only after `solve_eqp`'s factorize returned | assert-observed (a throw upstream already aborted the path) |
| `qp_engine.h:3930-3931` (`eliminated_candidate`) | same shape | assert-observed |
| `qp_engine.h:4093-4094` (`border_inertia_verdict` tail) | reads `border.kkt` after `needs_refactorization()` screening | assert-observed (same guarantee as `qp_engine.h:3970`) |
| `ssn_engine.h:2222-2223` (in-loop gate), `2525` (deferred certification) | immediately after a successful `factorize` | assert-observed |
| `ssn_engine.h:2250-2251,2266,2536-2537` | diagnostic `escape_detail` prints of the counts | assert-observed; print `e.n_pos`/`e.n_neg` from the same evidence object the verdict consumed, so message and verdict can never describe different factorizations |
| `tests/sqp/test_kkt_inertia_probe.cpp` (20 reads), `test_kkt_system.cpp:14-27`, `test_qp_engine_indefinite.cpp` (13 reads), `test_qp_engine_border.cpp`, `test_ssn_engine.cpp`, `test_qp_engine.cpp` | fixture-level reads | retargeted mechanically onto `FactorizeOutcome`/`inertia()`; the inertia-probe pins keep pinning the findings-doc behavior (fabricated-looking counts under perturbation) through the evidence type |
| `tests/golden_rig/seam_sqp.cpp:225-229` | the OLD-SEAM adapter's unguarded reads | UNTOUCHED — it compiles against the pinned sibling tag, not this tree, and its refusal to smooth is load-bearing (docket, "the rig's adapter is deliberately not protecting anyone from this") |

### 4.3 Accelerate deltas are licensed, docket-named, Mac-proven

On MKL this adoption produces NO evidence-value change (present counts,
identical values — §4.1). On Accelerate, absence-vs-zero-count in the
rewritten verdicts is a licensed delta whose verdict-consumer half must
be PROVEN on the Mac leg — a gate-B dependency this note records but
cannot produce. The delta rides under a NEW docket entry for this seam
(the precedent is
[`2026-08-09-psiopt-accelerate-perturbed-pivots.md`](dockets/2026-08-09-psiopt-accelerate-perturbed-pivots.md);
the SQP twin's case is differently-valued, not present-0, and gets its
own record at gate B). Every Apple slot with no real-hardware
observation stays `UNOBSERVED` — never zero-filled, never interpolated
(CLAUDE.md §6; the checklist-(h) re-derivation the plan cites is the
input to the Mac proof, and its "degradation direction no longer safe"
flag is exactly why this rewrite is design work with a Mac proof, not a
mechanical substitution).

### 4.4 The release-path stale-triple audit — performed, recorded

Docket consequence 2, self-assigned at M1 close, discharged here by
hand-search of this tree:

- `release()` is PRIVATE (`kkt_system.h:113`) and never called by any
  consumer; its only callers are the destructor (`kkt_system.h:132`),
  move-assignment (`kkt_system.h:148`), and `analyze()`'s top
  (`kkt_system.h:270`). No consumer code can hold a post-`release()`
  object that is not also mid-`analyze()` or destroyed.
- The one real window is post-`analyze()`/pre-`factorize()` INSIDE
  `factorize()`'s auto-analyze (`kkt_system.h:291-298`): `analyze()`
  sets `active_ = true` at `kkt_system.h:285` before any numeric
  factorization has written the counters. No consumer code runs inside
  that window — no consumer calls `analyze()` directly (verified: zero
  `.analyze(` call sites outside the class and its tests), so no
  consumer can read the stale triple there either.
- Conclusion: **no call site relies on a stale read.** The window
  defect itself is closed structurally by the retarget: hven's
  `inertia()` reports `kUnavailable` until THIS engine's factorization
  exists (`symmetric_factor.h:741-745`), and the false-before-
  factorization contract is PINNED at hven
  (`tests/linear/test_symmetric_factor.cpp:470`,
  `SupportsPartialSolveIsFalseBeforeAFactorization` — analyze-then-read
  included). The pin, not any internal mechanism, is what this design
  relies on.

## 5. Solve mapping

| Old operation | hven operation |
| --- | --- |
| `KktSystem::solve(rhs)` — Pardiso phase 33 (`kkt_system.h:300-313`) / Accelerate `SparseSolve` (`kkt_system_accelerate.h:429-452`) | pre-size `x`, `factor.solve(rhs, x)` (`symmetric_factor.h:690-706`); `solve_vec` keeps the Vec-returning call shape |
| `solve_forward` — phase 331 / `SparseSubfactorL` | `solve_partial(SolvePhase::kForward, rhs, x)` |
| `solve_diagonal` — phase 332 / `SparseSubfactorD` | `solve_partial(SolvePhase::kDiagonal, rhs, x)` |
| `solve_backward` — phase 333 / `SparseSubfactorL` transposed | `solve_partial(SolvePhase::kBackward, rhs, x)` |

**The phase-33x force-zero-restore rule DISSOLVES.** The rule the
audit's runtime shim independently re-found
(`consumed-surface-audit.md:123-156`; source: `kkt_system.h:104-108,
327-335`) is subsumed by `solve_partial()`'s contract: refinement is
forced off for every partial solve regardless of the configured cap
(`symmetric_factor.h:708-720`; implementation
`pardiso_session.cpp:455-459` — the identical save/zero/call/restore,
now inside the session where no caller can get it wrong). Callers must
not mutate anything around `solve_partial()` — there is nothing left to
mutate.

**`supports_partial_solve()` mapping.** The seam's gate is
`active_ && num_perturbed_pivots() == 0` (`kkt_system.h:78`) on MKL and
hardwired `false` on Accelerate (`kkt_system_accelerate.h:100`,
measured divergence, not caution). hven's predicate
(`symmetric_factor.h:722-739`): false before a successful
factorization, false without usable numerics, false on any perturbed
pivot (MKL), false under active matching (inert here —
`weighted_matching = false`), false always on Accelerate (no
perturbation evidence exists, so composability is unverifiable). This
is strictly conservative in the same direction on MKL — it adds `false`
exactly in the docket's post-`analyze()`/pre-`factorize()` window and
after a failed factorize, where the old gate consulted a count no
factorization produced — and EXACTLY equal on Accelerate. The
false-before-factorization behavior is PINNED at hven
(`test_symmetric_factor.cpp:470`); the design relies on that pin, not
on hven's internals.

**Engine reality check, stated for the reviewer:** no ENGINE code path
calls the partial solves or the gate — `SchurComplement` deliberately
uses the two-full-solve variant and the partial-solve fast path was
never implemented (`schur_complement.h:52-64`); the callers are the
test suite (`tests/sqp/test_kkt_partial_solve.cpp`,
`test_accelerate_probes.cpp:118,146`) and the golden rig's old-seam
adapter. M2 recorded hven's partial-solve path as untested by the IPM
migration (vacuous mapping); the migrated SQP SUITE is its first real
consumer, which is why the composition pins migrate rather than
dissolve (§10).

## 6. The dense border: `schur_complement.h` onto `DenseSymmetricFactor`

### 6.1 What the border actually consumes

One `LAPACKE_dsytrf(LAPACK_COL_MAJOR, 'L', m, C, m, ipiv)` per border
change (`schur_complement.h:305-308`), one in-place
`LAPACKE_dsytrs(..., 'L', ...)` per solve (`schur_complement.h:172-179`),
`info > 0` (exact singularity) treated as STATE — `singular_ = true`,
factorization kept, `needs_refactorization()` reports it, `solve()` and
`expected_neg_eigs_delta()` throw on use (`schur_complement.h:313-325,
161-166,261-273`) — and a Bunch-Kaufman block walk over the factored
matrix and `ipiv` producing per-block `|eigenvalue|`s and the negative
count (`schur_complement.h:330-359`), which feed `cond_estimate()`,
`nearly_singular()`, and the border inertia gate
(`qp_engine.h:4057-4095`).

### 6.2 Why `DenseSymmetricFactor` cannot host this today (the gap)

Three mismatches, each independently blocking:

1. **Uplo.** `DenseSymmetricFactor` factors the UPPER triangle
   (`dense_symmetric_factor.h:24-28,3-4`); the border factors LOWER.
   Bunch-Kaufman 'U' eliminates from the last column, 'L' from the
   first — different pivot sequences, different rounding, different
   `y`, different `block_abs_eigs_`. A silent uplo switch is a float
   delta §11 does not license.
2. **Singularity contract.** `DenseSymmetricFactor::factorize` THROWS
   `std::runtime_error` on `info > 0` (`dense_symmetric_factor.h:30-34`);
   the border needs the completed-but-singular factorization as a
   reportable state (its callers read `needs_refactorization()` and
   degrade — `qp_engine.h:3987-4018`).
3. **Evidence.** The block walk reads the factored matrix and the pivot
   array — the state `DenseSymmetricFactor` holds as its private
   `factors_`/`ipiv_` members (`dense_symmetric_factor.h:59-60`); no
   accessor exists.

### 6.3 Proposed surface growth (hven::linear change — same review lane as §2)

`DenseSymmetricFactor` grows, in the minimal shape its own header
anticipates (`dense_symmetric_factor.h:6-9`):

1. **Triangle selection**: `enum class Triangle { kUpper, kLower }`,
   a `factorize(A, Triangle)` overload (existing single-argument call
   stays `kUpper` — zero change for the IPM-side and `tests/linear`
   consumers). Backend-neutral by construction: both backends are
   LAPACK `dsytrf`/`dsytrs` (MKL's LAPACKE on Linux/Windows; the
   namespaced Apple shim `include/hven/detail/linear/lapacke_shim.h`,
   whose entry points already take `uplo` through —
   `lapacke_shim.h:83,113`), so there is no per-backend semantics
   divergence to document beyond "identical on both".
2. **Non-throwing exact singularity, ADDITIVELY**
   [folded per the execution review (2026-08-14), §2.1 — this item
   previously proposed mutating `factorize`'s contract; that proposal is
   withdrawn]. The existing `factorize(A)` and the new
   `factorize(A, Triangle)` overload keep TODAY'S documented contract
   exactly: they throw on `info > 0`
   (`dense_symmetric_factor.h:30-34`), throw on `info < 0` (internal
   illegal-argument bug), and throw on validation failure. The
   outcome-returning behavior arrives as a SEPARATE entry point —
   working name `try_factorize(A, Triangle)`, final name the
   implementer's choice — declared `[[nodiscard]]` and returning
   `kOk` / `kExactlySingular`; `info < 0` and validation failures still
   throw from it. `SchurComplement` consumes THAT entry and sets its
   `singular_` flag from the returned outcome. `factorized()` stays
   false on `kExactlySingular`; `solve()` before a successful factorize
   keeps throwing (`dense_symmetric_factor.h:44-47`) — which is exactly
   the border's own solve-on-singular behavior.

   The reason it must be additive, recorded because it is the binding
   constraint: the current consumer set is
   `tests/linear/test_dense_symmetric_factor.cpp` and
   `tests/golden_rig/traces_sqp.cpp` ONLY (the IPM engine never adopted
   the dense factor), and both must be untouched. A mutated contract
   either silently changes an existing caller — a caller that ignores
   the new return proceeds where it previously got a throw, the exact
   silent-failure shape this project exists to forbid — or forces edits
   to the rig trace instrument mid-measurement (hazard 6's spirit).
   Additive costs every existing caller nothing, gives the border its
   state contract, and leaves the instrument alone.
3. **Block evidence**: a `BunchKaufmanBlockEvidence` (name at
   implementer's discretion) computed by the factor — it owns the
   uplo convention the walk depends on — carrying the per-block
   `|eigenvalue|` list and the negative-eigenvalue count, arithmetic
   migrated VERBATIM from `schur_complement.h:330-359` (the 2x2
   trace/det/disc formulas are float-load-bearing for
   `cond_estimate()`). Evidence, not verdict: singular ⇒ evidence
   absent, mirroring the border's own leave-empty discipline
   (`schur_complement.h:313-325`) and `InertiaEvidence`'s
   absent-never-zero rule. The judgment calls —
   `kSchurSingularEigFrac`, `needs_refactorization()`,
   `expected_neg_eigs_delta()`'s throw-on-singular — STAY in
   `SchurComplement`: they are border-stack policy, not dense-factor
   facts.

Rejected alternative, recorded: raw `factors()`/`ipiv()` accessors with
the walk left in `schur_complement.h`. It avoids growing an evidence
type but exports LAPACK internals as public surface and leaves two
copies of the walk possible. [Folded per the execution review
(2026-08-14), §2.1: **RULED — the evidence struct**, on the grounds that
it owns the uplo convention and exports no LAPACK internals. The
alternative stays recorded as history, not as a live option.]

With this, `schur_complement.h` drops both LAPACKE includes
(`schur_complement.h:81-85` — the `#ifdef USE_ACCELERATE_SPARSE` switch
and the MKL header) and the SQP `lapacke_shim.h` loses its last
in-tree engine consumer. Its OTHER includer is
`tests/sqp/test_accelerate_probes.cpp:29` (the phase-A carry's second
inventory item): the shim-consuming probes retarget onto
`hven::linear::detail`'s shim or retire with the twin they probe (§10),
and `include/hven/detail/sqp/lapacke_shim.h` is deleted. The global-scope
duplication with `include/hven/detail/linear/lapacke_shim.h` (which is
namespaced under `hven::linear::detail` and guard-hardened —
`lapacke_shim.h:41-43,59-64,66`) ends with it.

## 7. Hot handle identity — the session conjunct

### 7.1 What moves

The kHot reuse key today is conditions (a)-(d) — `structural_hash`,
exit working set, `values_hash`, effective `(delta, mu)` — plus (e),
the `BorderState::generation` stamp (`qp_engine.h:3006-3013`; fields
`qp_engine.h:2217-2230`; emission `qp_engine.h:2333-2341` — the plan
quotes this note-plus-function as `qp_engine.h:2307-2333`; at HEAD the
FIX ROUND 2 note spans 2307-2332 and `hot_state()` runs to 2341, a
9-line drift, content identical). `generation` is bumped once per
`rebuild_k0()` — the sole site that changes what K0 is
(`qp_engine.h:4477-4486,4506`) — and exists because (a)-(d) describe
the PROBLEM, never WHICH OBJECT `border_` names
(`qp_engine.h:2067-2080`).

hven names one set of numerics by the triple
`(pattern_hash, session_id, epoch)` (`symmetric_factor.h:222-233` — the
plan cites the fork-aliasing rationale "around line 222"; at HEAD the
IDENTITY paragraph begins at exactly 222 and runs to 233 — the
preceding 216-221 is the SHARING paragraph). `analyze()` FORKS: a fresh
session, epochs CONTINUING the old sequence, so two branches can reach
the same epoch on the same pattern and only `session_id` separates them
— exactly what a generation stamp cannot distinguish. Phase B wires the
triple through `HotState`:

- `HotState` gains `kkt_session_id` and `kkt_epoch`, captured in
  `hot_state()` from `border_->kkt.factor` alongside the committed
  fingerprints (`qp_engine.h:2337-2340`), and drops `generation`.
- Condition (e) becomes: `border_->kkt.factor.session_id() ==
  <committed session_id> && border_->kkt.factor.epoch() == <committed
  epoch>` — a live read off the shared object on every solve, exactly
  as today's live `border_->generation` read is
  (`qp_engine.h:2986-2998`). The pattern member of the triple is
  carried by condition (a)'s `structural_hash` (equivalent for this
  purpose: K0's pattern is a function of the raw patterns it hashes,
  `qp_engine.h:1986-1990`); adding a redundant third conjunct is not
  proposed.
- `BorderState::generation` and `border_generation_` are deleted;
  `rebuild_k0` no longer stamps anything — `factorize()` advancing the
  epoch (`symmetric_factor.h:769-780`) IS the stamp, and `analyze()`
  moving the session id covers the pattern-rebuild case.

### 7.2 The one semantic gap the mapping opens, and its closure

`generation` is bumped BEFORE the mutations that can throw —
bump-before-mutate is deliberate defense (`qp_engine.h:4489-4506`) — so
a FAILED rebuild on a shared object leaves every other holder's (e)
mismatching. The epoch, by contrast, does NOT advance on a failed
factorize (`symmetric_factor.h:771-773`): after another holder's failed
rebuild, a stale holder's `(session_id, epoch)` can still MATCH an
object whose numerics are invalidated (solves throw until a
factorization succeeds, `symmetric_factor.h:680-682`). Reuse would then
throw where today it detaches and rebuilds gracefully. Closure: the
reuse gate gains a usable-numerics conjunct —
`border_->kkt.factor.inertia().state == InertiaEvidence::State::kObserved`
— which is false precisely when the last factorize failed or none
happened. This is the same evidence read §4.2's rebuild gate already
performs, costs one struct copy, and restores detach-on-mismatch as the
failure mode.

**ORDERED PIN (blocking): the failed-factorize evidence pin.** The
conjunct rests on "`inertia()` reports non-`kObserved` after a FAILED
factorize". `symmetric_factor.h:741-745` states the not-factorized and
stale-adopt cases explicitly; the failed-factorize case follows from
"invalidates the current numerics" but has no dedicated pin today. **A
reuse gate may not rest on an unpinned contract.** The pin lands in
`tests/linear` in the SAME commit/review lane as §2's Options change —
it is an `hven::linear` behavior pin, and the scoped tycho-side lane
reviews it with the rest. If the pin cannot deterministically fail a
factorize without the fault injector, the fault-injection suite is the
sanctioned home. Either way it exists BEFORE the reuse gate's
usable-numerics conjunct ships; the conjunct may not land first.
[Folded per the execution review (2026-08-14), §2.3 — this was a flagged
open verification item and is now an ordered, gate-blocking pin.]

Session-fork aliasing itself stays unreachable in the current
single-session driver — `analyze()` on `border.kkt` happens only inside
`rebuild_k0` on the engine's own (possibly shared) object, and no
consumer holds a second `SymmetricFactor` on the same pattern — which
is exactly why the conjunct is cheap now and expensive after a second
session exists (plan §4.3). The forged-handle tests (the R3 probe
family around `hot_state()`, `qp_engine.h:2307-2341`;
`tests/sqp/test_qp_warm_start.cpp`) re-derive against the triple:
emission captures committed values, adoption copies them
(`qp_engine.h:2760-2766`), the live-vs-committed discrimination
argument transfers with epoch playing generation's role. **Declared
pin-review if any counter moves; none is expected** — every scenario
those tests stage maps one rebuild to one epoch advance.

## 8. Call-site inventory

Every live touch of the three dissolving files and the Schur border at
HEAD. Grouped where one replacement covers adjacent lines; every source
line named. "H" = the §3.2 helper (`KktFactor` /
`needs_analysis`/`factorize_checked`/`solve_vec`).

### 8.1 Includes of the dissolving files

| Site | Replacement |
| --- | --- |
| `eqp_solve.h:102`, `qp_engine.h:1502`, `predictor.h:358`, `ssn_engine.h:558` | `hven/linear/symmetric_factor.h` + the helper header |
| `schur_complement.h:87` | same |
| `schur_complement.h:81-85` (`lapacke_shim.h` / `mkl_lapacke.h` switch) | `hven/linear/dense_symmetric_factor.h`, unconditional — the platform `#if` dissolves |
| `kkt_system.h:5-7` (platform dispatch to the Accelerate twin) | file deleted |
| `tests/sqp/test_accelerate_probes.cpp:28-29` | see §10 (probe disposition) |
| `tests/sqp/test_kkt_system.cpp:2`, `test_kkt_partial_solve.cpp:2`, `test_kkt_inertia_probe.cpp:2`, `test_border_ops.cpp:9`, `test_qp_engine_border.cpp:9`, `test_qp_engine_indefinite.cpp:36` | `hven/linear/symmetric_factor.h` (+ helper) per §10 |
| `tests/sqp/test_schur.cpp:2` (via `schur_complement.h`), `test_eqp_solve.cpp:3` and `test_eqp_refine_ab.cpp:50` (via `eqp_solve.h`) | no direct include of a dissolving file — these reach `KktSystem` TRANSITIVELY, which is why an includes-only sweep misses them; their live constructions are inventoried at §8.6 |

### 8.2 `KktSystem` lifecycle and solve touches

| Site | Current touch | Replacement |
| --- | --- | --- |
| `qp_engine.h:2063` | `BorderState::kkt` member | `KktFactor kkt` (H), constructed from `sqp_kkt_options()` |
| `qp_engine.h:2502,2944` | elimination-path locals `KktSystem kkt{eff_opts}` | `KktFactor` locals; `eff_opts` no longer feeds the factor (it never did — §1) |
| `predictor.h:961-963` | local + `kkt.factorize(k0.K)` + `SchurComplement schur(kkt, ...)` | `KktFactor` + `factorize_checked` + `SchurComplement` over it |
| `eqp_solve.h:167` | `solve_eqp(..., KktSystem &kkt, ...)` signature | `KktFactor &` |
| `eqp_solve.h:211-212,241` | `kkt.factorize(asm_.K)`, `kkt.solve(rhs)`, `kkt.solve(residual_of(y))` | `factorize_checked`, `solve_vec` ×2 — preserve the assign-then-refine order exactly |
| `qp_engine.h:3890,3905,3941,3983,4108,4160,4338` | `KktSystem &kkt` parameters through the candidate/probe/repair chain | `KktFactor &` (mechanical) |
| `qp_engine.h:4509-4517` | `pattern_matches` probe for `symbolic_analyses`, then `border.kkt.factorize(border.k0.K)` | `needs_analysis` probe (same before-factorize position, `types.h:269-286` contract preserved), then `factorize_checked` |
| `qp_engine.h:4506` | `++border.generation` | deleted; epoch/session are the stamp (§7) |
| `ssn_engine.h:3560` | `KktSystem kkt_` member | `KktFactor kkt_` |
| `ssn_engine.h:2154-2160` | `pattern_matches` probe, `factorize`, `solve` | `needs_analysis`, `factorize_checked`, `solve_vec` — the surrounding `try/catch` keeps working via §3.3's throw parity |
| `ssn_engine.h:2512-2516` | same pair on the deferred-certification path | same |
| `schur_complement.h:114` | ctor takes `KktSystem &kkt` | `KktFactor &` (or `SymmetricFactor &` — it only solves; implementer's choice at execution review) |
| `schur_complement.h:120,157,186` | `kkt_.solve(v/rhs0/rhs0_adj)` | `solve_vec` — two-full-solve variant unchanged |

### 8.3 Inertia and gate reads

Classified in §4.2; the rows there are the inventory. Additional
comment-only references that must be re-aimed mechanically:
`qp_engine.h:16,119,128,505-522,645,1918,1929,2025-2037,2069,2094,2111,
3033-3060,3314,4027,4512`; `ssn_engine.h:183,228-229,509-518,982,
1564-1569,2886-2894,2963-2965`; `types.h:269-286`;
`sqp_types.h:1244-1255`; `warm_start.h:24-29,205,423-425`;
`kkt_assembly.h:18,45,62`; `border_ops.h:9-16,33-34,61,122,167`;
`bordered_eqp.h:5,18,142,293`; `predictor.h:34,282-289,581,663`;
`sqp_driver.h:6783-6799`. (Comments naming `KktSystem`/`kkt_system.h`
retarget to `SymmetricFactor`/the helper; none carries code.)

### 8.4 The dense border (LAPACKE touches)

| Site | Current touch | Replacement |
| --- | --- | --- |
| `schur_complement.h:305-308` | `LAPACKE_dsytrf(..., 'L', ...)` on rebuilt C | `DenseSymmetricFactor::try_factorize(C, Triangle::kLower)` — the §6.3 additive outcome-returning entry, NOT `factorize` (which keeps throwing); `singular_` set from `kExactlySingular` [folded per the execution review (2026-08-14), §2.1] |
| `schur_complement.h:309-312` | `info < 0` throw | retained inside the dense factor (illegal-argument stays a throw) |
| `schur_complement.h:313-325` | `info > 0` → `singular_` state | consumed from the outcome; leave-evidence-empty behavior unchanged |
| `schur_complement.h:330-359` | Bunch-Kaufman block walk over `factored_`/`ipiv_` | consume the factor's block evidence (§6.3), arithmetic migrated verbatim |
| `schur_complement.h:172-179` | in-place `LAPACKE_dsytrs` on `y` | `DenseSymmetricFactor::solve` (pre-sized destination; same factors, same `dsytrs`, float-identical) |
| `schur_complement.h:362-375` | members `factored_`, `ipiv_`, `singular_`, `block_abs_eigs_`, `neg_count_` | `DenseSymmetricFactor` member + `singular_` + the evidence copy; `block_abs_eigs_`/`neg_count_` read from evidence |
| `include/hven/detail/sqp/lapacke_shim.h` (whole file: shim at 44-83) | Apple-only LAPACKE source for the above | DELETED; `hven/detail/linear/lapacke_shim.h` serves the dense factor internally (`dense_symmetric_factor.h:10-14`) |

### 8.5 Hot-handle sites (§7)

| Site | Current touch | Replacement |
| --- | --- | --- |
| `qp_engine.h:2217-2230` | `HotState` fields incl. `generation` | `kkt_session_id`/`kkt_epoch` replace `generation` |
| `qp_engine.h:2333-2341` | `hot_state()` emits committed `border_generation_` | emits committed session/epoch (captured at the same exit-commit point that sets the fingerprints today) |
| `qp_engine.h:2760-2766` | adoption copies `hot->generation` | copies the pair |
| `qp_engine.h:3006-3013` | condition (e) live `border_->generation` read | live `session_id()`/`epoch()` reads + the usable-numerics conjunct (§7.2) |
| `qp_engine.h:4489-4506` | bump-before-mutate note + `++border.generation` | deleted; the §7.2 conjunct carries the defensive role |

### 8.6 Transitive test consumers (no direct include of a dissolving file)

Three test files construct `KktSystem`/`SchurComplement` live but reach
them only through `eqp_solve.h`/`schur_complement.h`, so §8.1's include
rows do not surface them. Every construction line, named:

| Site | Current touch | Replacement |
| --- | --- | --- |
| `tests/sqp/test_schur.cpp:20,46,70,101,122,177,225,247,270` | `KktSystem kkt(opts)` locals backing the `SchurComplement` unit suite | `KktFactor` locals; factorization through `factorize_checked` where the fixture factorizes K0 |
| `tests/sqp/test_schur.cpp:22,48,72,103,124,187,232,249,277,281` | `SchurComplement schur(kkt, opts)` constructions (including the `healthy`/`sick` pair at 277/281) | the ctor's retargeted reference (§8.2). Assertions unchanged — this suite becomes the FLOAT-PARITY WITNESS for §6.3's dense-factor growth: `cond_estimate()`, `expected_neg_eigs_delta()`, `nearly_singular()` and the exact-singular behavior must come back bit-identical through `DenseSymmetricFactor` |
| `tests/sqp/test_eqp_solve.cpp:73,84,119` | `KktSystem kkt{QpOptions{}}` locals fed to `solve_eqp` | `KktFactor` locals matching `eqp_solve.h:167`'s retargeted signature |
| `tests/sqp/test_eqp_refine_ab.cpp:540` | `KktSystem kkt{on}` inside the A/B probe lambda, fed to `solve_eqp` | same |

## 9. Migration hazards

1. **The refinement cap is inherited, not written.** Nothing may
   "helpfully" set `max_refinement_iters = 0` for tidiness: the
   census's byte-identity runs at the INHERITED cap (2 on the audited
   build). The canary (§2.4) is the tripwire; treat a canary failure as
   a design re-open with evidence, never a value to patch.
2. **Do not switch the dense border's triangle.** 'L' is
   float-load-bearing (§6.2). The `Triangle::kLower` request is part of
   parity, not a style choice.
3. **`factorize()` never auto-analyzes at hven.** Any call site that
   bypasses `factorize_checked` and calls `factor.factorize(K)` after a
   pattern change gets a THROWN `std::invalid_argument`
   (`symmetric_factor.h:675-679`) where the old seam silently
   re-analyzed. The helper is the contract; the compile break from the
   type change is what forces every site through it.
4. **`SymmetricFactor::Counters` are NOT the census counters.**
   `QpCounters::factorizations`/`symbolic_analyses` stay counted at the
   call sites (`types.h:269-286`) — moving them onto the factor's own
   counters (`symmetric_factor.h:568-591`) would change per-instance
   scoping (fresh `KktFactor` locals restart at zero) and is not
   attempted in phase B.
5. **Throw-type discrimination in border fallback.** The
   singular-C-vs-backend-throw discriminator
   (`qp_engine.h:4027-4034`) relies on `needs_refactorization()` state,
   not message text — unaffected by §3.3's message change. Do not
   "improve" it to match on strings.
6. **The golden rig's old-seam adapter is out of bounds.**
   `tests/golden_rig/seam_sqp.cpp` and `audit/test_audit_shim.cpp`
   compile against the pinned sibling tag (`seam_sqp.cpp:36`;
   `tests/golden_rig/CMakeLists.txt:262-265`) and are the COMPARISON
   arm; phase B must not touch them, and P5's fail-by-design control
   pair must keep failing/passing exactly as derived.
7. **Options construction throws are construction-time.** The factory
   (§1) is platform-neutral only under §2.3's ruling; if the reviewer
   picks the explicit-override fallback, the factory must be
   platform-split, because a present `accelerate_zero_tolerance` throws
   on MKL (`symmetric_factor.h:545-551`).

## 10. New and migrated tests phase B adds

Counted per the count-arithmetic discipline (N additions move the
registered count by exactly N; dissolved-with-declaration tests are
enumerated in the gate report):

1. **The composition test (migrated pin; hven's partial-solve path's
   first real consumer).** `test_kkt_partial_solve.cpp`'s pins retarget
   onto `SymmetricFactor`: partial∘partial∘partial == full on an
   unperturbed factorization; divergence + gate-false under a perturbed
   pivot (the O(1e8) regression pin, `test_kkt_partial_solve.cpp:37-64`);
   D14/D15-class per-backend arms — MKL composes, Accelerate pins the
   measured bare-subfactor divergence with the gate false
   (`test_kkt_partial_solve.cpp:12-26`'s `#ifdef` split, retargeted onto
   `solve_partial`/`supports_partial_solve`). These COMPOSE WITH, and do
   not duplicate, the backend's own refinement-off-per-call pin
   (`test_symmetric_factor.cpp:383`, comment block from 378) and the
   false-before-factorization pin (`:470`) — the suite-level pins assert
   the SQP configuration's behavior through the SQP options, not the
   backend's default options.
2. **The `pardisoinit`-defaults canary** (§2.4), in the same commit as
   the Options change, `BackendDefaultPremise` family.
3. **Don't-write act pins** for the two new states, via the §2.2
   observer fields (same shape as the existing ordering act pin).
4. **The failed-factorize evidence pin** (§7.2's ORDERED pin), in
   `tests/linear` — `inertia()` reports non-`kObserved` after a failed
   factorize — in the same commit/review lane as the §2 Options change,
   or in the fault-injection suite if a deterministic factorize failure
   needs the injector. It is ordered BEFORE §7.2's usable-numerics
   conjunct ships. [Folded per the execution review (2026-08-14), §2.3.]
5. **The constraint-side seam fixture** (plan §1.2, handoff item 4
   accepted into M3): ONE objective-only fixture type registered via
   `ConstraintUnsupported<T>`, with a compile-fail probe asserting the
   AUTHORED constraint-route message fires and the raw member-lookup
   diagnostic does not — modeled on the existing probe family
   (`tests/interior/CMakeLists.txt:64-131`,
   `tests/interior/compile_fail/`). ONE fixture, not a matrix:
   compile-fail probes cost ~90 s serial per ctest run and nested
   builds are unthrottled under `ctest -j` (the M2 t55f2-m1 cost note,
   carried by the plan). **DISCHARGED 2026-08-15 (phase-C C0.4):**
   `adapter_fixture::StubMixinObjectiveOnly` in
   `tests/interior/adapter_fixture_functions.h`, registered through
   `ConstraintUnsupported<T>`, with the sixth family probe
   `tests/interior/compile_fail/constraint_unsupported_mixin.cpp`
   inheriting the family's `FORBIDDEN_REGEX`, `EXPECT_SINGLE_ERROR` and
   shared `RESOURCE_LOCK` unchanged. It landed with the family rather
   than under `tests/sqp/` because forking the family would fork the
   resource lock the nested builds share. Count arithmetic: +1 ctest
   entry for the probe (`add_test` runs once per probe name) and +1 for
   the positive gtest case that exercises the direction the mixin does
   support, 1151 → 1153.
6. **Dissolved with declaration:** `test_kkt_system.cpp` (2 tests —
   both pinned behaviors are covered at
   `tests/linear/test_symmetric_factor.cpp` by
   `AnalyzeOnceFactorizeManyOnAFixedPattern`/`FactorizeRejectsAForeignPattern`
   and the inertia pins); `test_qp_engine_border.cpp:625`'s
   `KktSystemMoveKeepsFactorizationUsable` retargets onto
   `SymmetricFactor` move semantics or is declared covered by the
   `tests/linear` move/share pins. `test_kkt_inertia_probe.cpp`
   retargets onto evidence (§4.2). `test_accelerate_probes.cpp`: the
   probes that pinned the dissolved twin's own mappings (its
   `RecordProperty` instruments, `test_accelerate_probes.cpp:100-105`)
   retire with the twin; the composition probes fold into item 1's
   Accelerate arms; the shim probes retarget onto
   `hven::linear::detail`'s shim. Every disposition is enumerated in
   the gate report with the count arithmetic.
7. **Retargeted in place, count-neutral (the §8.6 transitive
   consumers):** `test_schur.cpp` (all 19 constructions),
   `test_eqp_solve.cpp` (locals at 73, 84, 119), and
   `test_eqp_refine_ab.cpp:540` retarget mechanically with no test
   registered or removed — the gate report carries them as explicit
   zero-delta rows so the count arithmetic accounts for every touched
   test file, not only the ones whose counts move. `test_schur.cpp`
   additionally serves as §6.3's float-parity witness (§8.6): a
   `cond_estimate`/`expected_neg_eigs_delta` value that moves there is
   a gate-blocking float delta, not a test to update.

## 11. Behavioral-delta ledger (mirrors plan §9)

Anything old-vs-new not on this list blocks gate B.

| Delta | License |
| --- | --- |
| Accelerate perturbed-pivots: the twin's zero-pivot-count reading (and degraded `n_`) → absent evidence, consumed by the rewritten verdicts through the native zero class (§4.1) | plan §9 row 1 (A.5 + checklist-(h) re-derivation); verdict-consumer half PROVEN on the Mac leg (gate-B dependency); new docket entry authored at gate B, precedent `psiopt-accelerate-perturbed-pivots` |
| Pre-factorization / post-release inertia: fabricated zero / stale triple → explicit `kUnavailable` routed as `kSuspect` (§4.1, §4.4) | amended docket `sqp-inertia-before-factorization`; the golden P5 rows keep recording the OLD seam's answer (`tests/golden_rig/expected/P5.csv`), and the rig adapter stays unsmoothed |
| kHot reuse key gains the session/epoch conjunct, drops `generation` (§7) | plan §4.3/§9; no counter expected to move; declared pin-review if any does |
| Namespace/include paths; backend-error MESSAGE TEXT (`"pardiso phase 22 failed: error N"` → hven-authored, backend code preserved) reaching `escape_detail` (§3.3) | mechanical; no pin or census row asserts the old text (verified §3.3) — if gate B finds one, it is a declared re-derivation, not a silent break |
| Dense-border can't-happen throw types: `std::invalid_argument` → `std::runtime_error` for dsytrs `info != 0` and dsytrf illegal-argument (`src/linear/dense_symmetric_factor.cpp`) | both throw paths are unreachable with valid state (LAPACK contract: `info != 0`/illegal-argument cannot fire on well-formed input); no test pins the old exception type; the predictor's degradation net catches `std::exception` by phase for exactly this reason, so the type change is not observable through that consumer [added at B4 fix round, per task review I-1] |
| `SymmetricFactor::analyze()` validates at the analyze boundary what the dissolved seam forwarded to the backend unvalidated. The full validation set is `validate_upper_csr` (`src/linear/symmetric_factor_mkl.cpp:253-292`): compressed, square, **non-empty (a 0x0 matrix is rejected)**, **no entry below the diagonal**, and a structural diagonal in every row | unreachable from engine assemblies, which are square, non-empty, upper-triangle-only and emit the diagonal unconditionally (§3.1); reachable only from hand-built matrices — five test fixtures needed an explicit zero diagonal added to stay valid; solver behavior on well-formed input is unchanged, no counter moves [added at B4 fix round, per task review I-1; the empty-matrix and below-diagonal siblings named explicitly at gate-B dual review] |
| The retargeted path writes **`iparm[17] = -1` (the factor-nonzero-count request) on every analyze**, where the dissolved seam never touched `iparm[17]` at all. Unconditional, gated by no `Options` field — `collect_factor_mflops` gates only `iparm[18]` (§1) | delta-free, not merely unreachable: `pardisoinit`'s own mtype = −2 initialization already sets this entry to −1 on every MKL version checked (`pardiso_session.cpp:361-370`), so the write changes nothing Pardiso computes — it only makes hven's request explicit rather than riding a backend default; the dissolved seam read neither `iparm[17]` nor `iparm[18]` (audit `consumed-surface-audit.md:104`), so no consumer can observe it. ~~Unlike `iparm[7]`/`iparm[9]` it carries **no `pardisoinit` canary**, so a future backend-default drift would be silent; extending `BackendDefaultPremise` to `iparm[17]` is a phase-C item, recorded in the gate note's addendum~~ **CLOSED at phase C, task C0.1:** the canary now exists — `BackendDefaultPremise.MklPardisoinitAlreadyRequestsTheFactorNonzeroCount` (`tests/linear/test_fault_injection.cpp`) asserts the −1 default from the post-`pardisoinit` capture point, so a backend-default drift fails this row loudly instead of silently. Value observed, not assumed: −1 on MKL 2026.0.1 (build 20260612), from hven's own session and from a standalone `pardisoinit()` probe outside hven's session code, mtype = −2 and mtype = 11 alike. The "every MKL version checked" phrasing above stands as written, now with a running check behind it [added at gate-B dual review; canary landed at phase-C C0.1] |
| Probe-side throw site moves: `needs_analysis(fresh_factor, uncompressed_K)` short-circuits on `!k.analyzed` and returns `true` where the old `pattern_matches` threw `std::invalid_argument("KktSystem: matrix must be compressed …")` from the probe itself (`src/sqp/kkt_calls.cpp:10`; old seam `kkt_system.h:170-174,197`) | unreachable from engine assemblies, which hand a compressed matrix at every call site; and delta-free even off that path, because the throw still fires one call later inside `analyze()` — no invalid matrix reaches a backend, no probe-then-factorize consumer can tell the difference, and no test asserts on the probe-side throw (searched at B3 review; none found). Only the throwing site and the message text change, the latter already licensed by §3.3 for `factorize` [added at gate-B dual review, per B3 task review M4] |

Explicitly NOT licensed: any float, counter, status, or census delta
not named above — including any refinement-cap effect change (§2's
don't-write states + canary exist to prevent exactly that), any
Accelerate ordering change (`kBackendDefault` = `SparseOrderDefault` is
exact parity for this seam, §1), any dense-border float change (§6's
lower-triangle requirement exists to prevent exactly that), and any SSN
rebuild-count change (phase B does not touch the SSN structure key —
§3.2).

### 11.1 What phase B's gate must show (consolidated)

[Folded per the execution review (2026-08-14), §2.6, which states these
are "no new demands": §10 and §11 already carry them, and they are
gathered here so the gate report can tick them in one pass. Merged, not
duplicated — each row points at the section that owns it.]

- **Count arithmetic with every disposition enumerated**, including
  §10 item 7's explicit zero-delta rows for the retargeted-in-place
  test files.
- **The 57-cell census byte-identical at the inherited refinement cap**,
  with the `pardisoinit`-defaults canary green (§2.4, hazard 1).
- **`test_schur.cpp`'s float-parity witness unmoved** — `cond_estimate`,
  `expected_neg_eigs_delta`, `nearly_singular`, and the exact-singular
  behavior come back bit-identical through `DenseSymmetricFactor`
  (§8.6, §10 item 7).
- **`suspect_escalations` pins unmoved on BOTH backends** — all pins;
  any movement is a gate-blocking unlicensed delta, and the Mac leg
  re-verifies the per-backend arms. This is an explicit gate-blocking
  item, not a note (§4.1's fold; review §2.2).
- **The §11 ledger's eight rows and nothing else** — any old-vs-new
  difference not on that list blocks the gate. [Four rows when this
  checklist was written; six at the B4 fix round (`6535566`); eight at
  the gate-B dual review, which added the unconditional `iparm[17]`
  write and the probe-side throw-site move and widened row 6 to name the
  whole analyze-boundary validation set. Each growth was reviewer-found
  and reviewer-ruled — see the gate note §5.]
- **Clean-configure provenance stamps** on every gate-B artifact, so the
  stamp names a real commit (gate A's census stamp read
  `cf65a03a77eb-dirty`; content unaffected, hygiene carried forward).
- **The ordered failed-factorize pin landed in the linear suite**
  (§7.2, §10 item 4), before §7.2's usable-numerics conjunct ships.

## 12. What this note does NOT decide

[Folded per the execution review (2026-08-14), §2.4: every item this
section opened for the reviewer has now been ruled. The ruled items are
marked **RESOLVED** in place with the ruling and its one-line ground;
the items that remain genuinely undecided are unmarked and stay open.]

- **The don't-write states' final spelling.** `std::optional<int>` with
  unchanged defaults is proposed and argued (§2.1).
  **RESOLVED — ENDORSED** [execution review (2026-08-14), §2.4]: the
  defaults-stay-written argument is right — the ruling requires the
  state to EXIST, not to be the default, so the IPM side changes
  nothing. Reminder unchanged and not substituted for by the verdict:
  the iparm-surface commit still requires **Grant's human review** plus
  the scoped tycho-side review lane.
- **§2.3's Accelerate don't-write semantics** (documented-Apple-default
  vs explicit override + platform-split factory). Proposed: the former.
  **RESOLVED — ADOPTED as proposed** [execution review (2026-08-14),
  §2.4]: citing Apple's own documented default (`1e-4 * eps`) is citing
  a document, not fabricating a measurement; the rejected alternative
  would have required a fabricated exponent. The platform-neutral
  factory stands and the explicit-override fallback is not needed
  (hazard 7's platform-split contingency does not fire).
- **The `DenseSymmetricFactor` growth shape** (§6.3's evidence struct
  vs raw accessors). Proposed: the evidence struct.
  **RESOLVED — the evidence struct** [execution review (2026-08-14),
  §2.1]: it owns the uplo convention and exports no LAPACK internals.
  Note that §6.3 item 2's singularity mechanic changed in the same
  ruling — additive `try_factorize`, not a mutated `factorize`.
- **The helper's home and name** (§3.2) — phase C re-homes it
  regardless. **RESOLVED — APPROVED** [execution review (2026-08-14),
  §2.4]: the `KktFactor` + three free functions shape, the
  deliberately-not-an-adapter framing, and the `needs_analysis` probe
  preserving `symbolic_analyses` call-site counting are all approved;
  name and home are settled at execution review as proposed.
- **Anything the IPM engine consumes.** Zero IPM-side change: the IPM
  keeps its explicit written values, its compatibility cache, and its
  postponed honesty-state adoption (`retarget-design.md`'s inertia
  section) — M3 changes only the SQP side's consumers.
- **Hash re-keying** (`structural_hash`/`values_hash`/SSN structure
  key/`mix_pattern`) — phase C §5, with its own survey, composite-key
  rule, and proof obligations. Phase B leaves every one byte-identical.
- **The default flip question for `pivot_perturb_exp`/
  `max_refinement_iters`** (should hven's OWN default someday become
  don't-write?) — a surface-policy question for the freeze process, not
  this retarget; §2.1 deliberately avoids forcing it.
- **The Mac leg's outcomes.** Every Accelerate claim above whose value
  is not sourced from Apple documentation or the pinned twin's own
  source is a gate-B observation to be made on real hardware; empty
  slots stay `UNOBSERVED` until then.
- **Whether `inertia()` after a FAILED factorize reports
  non-`kObserved`** was asserted by the header's contract but unpinned.
  **RESOLVED — ORDERED PIN** [execution review (2026-08-14), §2.3]: a
  reuse gate may not rest on an unpinned contract, so the pin is
  required in `tests/linear` (or the fault-injection suite) ahead of
  §7.2's usable-numerics conjunct. Full terms at §7.2; gate row at
  §11.1. If the execution finds the contract does not hold, the conjunct
  is re-derived and this note amended — loudly, not silently.

### 12.1 Rulings the review settled that this section did not open

[Folded per the execution review (2026-08-14), §2.4 — recorded so the
answers are on file where the questions would otherwise have no home.]

- **§4.2's rebuild-gate rewrite** (`qp_engine.h:3970`): **APPROVED** —
  the "natively-observed `n_zero != 0`" qualifier does the work; inert
  on MKL where the zero class is derived, and it preserves the
  Accelerate twin's zero-pivot trust signal through the honest channel.
- **§1's parity items**: `collect_factor_mflops = false`,
  `ordering = kBackendDefault`-is-exact-parity-on-both-backends, and the
  fixed-0.01 Accelerate pivot tolerance — **APPROVED**, verified against
  the audit's SQP table and the dissolved twin's source.
- **§2.2's act-evidence extension** of the sanctioned observer
  deviation: **ENDORSED**, on the same terms §2.2 already states
  (byte-identical-object proof re-run, `docs/testing.md` and `notices/`
  updated).

## Addendum (2026-08-14): M2.5 merge — live thread-count setter

**(a) What landed.** M2.5 (merged into this branch as `8d078ed`) added a
live thread-count setter to `SymmetricFactor` — the one field on
`Options` this note otherwise treats as frozen at construction (§1's
`num_threads` row, §9 item 7's construction-throws discussion) is no
longer the whole story:

```cpp
void set_num_threads(int num_threads);          // symmetric_factor.h:645
int num_threads() const noexcept { return opts_.num_threads; }
                                                  // symmetric_factor.h:656
```

Contract, as encoded by the new tests (`tests/linear/
test_symmetric_factor.cpp:616` `ANewThreadCountKeepsTheAnalysisTheSessionAndTheNumerics`,
`:663` `AThreadCountSetBeforeAnalyzeIsCarriedIntoTheSession`, `:682`
`ANegativeThreadCountIsRejectedAndChangesNothing`; `tests/interior/
test_kkt_factorization.cpp:150` `SettingTheSameThreadCountKeepsTheAnalysis`,
`:160` `ChangingTheThreadCountKeepsTheAnalysisToo`):

- **Kept, not invalidated:** the symbolic analysis (`analyze_count` does
  not move), the backend session (`session_id()` unchanged), and the
  current numerics/epoch (`epoch()` unchanged, and a subsequent
  `factorize()`/`refactorize()` needs no re-analysis). Nothing about the
  call is observable as a re-analysis or a new session — the test suite's
  proof is exactly that the counters and identity triple stay put while
  the count moves.
- **What actually moves:** the count takes effect from the NEXT backend
  call — MKL updates the live session's stored config in place
  (`session_->set_num_threads(...)`, `symmetric_factor_mkl.cpp`), so a
  call in flight or already issued is unaffected and the following one
  picks up the new value. On Accelerate the value is stored and applied
  to nothing (best-effort-absent, the same treatment
  `Options::num_threads` itself already has on that backend).
- **Before the first `analyze()`:** the setter still has somewhere to
  put the count — `opts_` — and the next `analyze()` builds its session
  from that stored value.
- **Rejection:** a negative count throws `std::invalid_argument` (the
  constructor's own rule, reused via a shared `validate_num_threads`
  helper), validated BEFORE anything is mutated, so a rejected call
  leaves the engine exactly as it was.
- **Shared sessions:** per the header's own SHARING/THREAD SAFETY notes,
  the count belongs to the session, not to one co-owning `SymmetricFactor`
  — a change governs every co-owner's subsequent calls on that session,
  and is the caller's to serialize against concurrent calls, the same
  rule that already covers solves across co-owners.

**(b) Substance-changed declarations found.** None of the citations
corrected in the refresh above point to a declaration whose contract
changed — the drift was line-number movement caused by the setter's
insertion, not a meaning change at any cited site. One declaration in
the M2.5 diff DID change in substance but is not cited anywhere in this
note by line number, so no citation needed rewriting: `adopt()`'s doc
comment (`symmetric_factor.h:815-823`) changed from "the adopting engine
inherits the emitting engine's Options" to "takes its Options from the
SESSION, not from the emitting engine" — because `num_threads` can now
move on a live session after a handle is emitted, an adopter receives
whatever count is in force NOW, which may differ from the count the
session was analyzed with or the count in force when `share()` was
called. Flagged here because `adopt()`/`share()` are part of the same
frozen-except-num_threads surface this note otherwise assumes throughout
§3 and §7.

**RULED — WEIGHED, INERT for phase B** [execution review (2026-08-14),
§2.4]: the retargeted design's co-ownership is `shared_ptr<BorderState>`
holding the `SymmetricFactor` BY VALUE, and there is no `share()` /
`adopt()` call site anywhere in the phase-B consumer set — so the changed
semantics cannot bite. Recorded so the question has an answer on file; if
M5's crossover ever adopts handles across engines, this line item is the
pointer to re-open it.

**(c) Implication to be ruled on.** §1's factory sets
`o.num_threads = 0` for exact parity with the dissolved seam (no
per-instance thread control existed there; reproducibility today comes
from the test-side `MKL_NUM_THREADS=1` process-wide pin, unaffected by
any of this). Now that a live setter exists on the surface `KktFactor`
(§3.2) wraps, should `KktFactor` or `sqp_kkt_options()` expose or pin
the thread count explicitly — e.g. a pass-through
`KktFactor::set_num_threads()`, or an explicit comment recording that
`0` plus the env-pin is the intended and sufficient mechanism — or is
silence (today's `o.num_threads = 0`, unchanged) still correct because
nothing in phase B's consumer set needs to move the count at runtime?

**RULED — SILENCE** [execution review (2026-08-14), §2.4]:
`o.num_threads = 0` stays, and phase B does **not** expose a
`KktFactor::set_num_threads` pass-through. No phase-B consumer moves the
count at runtime; parity is phase B's law; a live mutation channel with
zero consumers is surface without a user and would owe its own
reproducibility story against the census discipline. The one thing the
ruling DOES require is the explicit comment in `sqp_kkt_options()`
recording that `0` plus the process-env pin is the intended and
sufficient mechanism — folded into §1's factory sketch above. If a real
consumer emerges (tycho consumption, Jet-style per-worker pinning),
exposure is a reviewed change then, not a phase-B one.
