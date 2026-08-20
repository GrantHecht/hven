> Ratified by the execution review 2026-08-15 (reviewer-side note
> `docs/notes/2026-08-15-m3-phase-c-ratification.md`, sentinel-signed).
> Amendments A1-A3 folded. Q1 ruled option (a), with Grant's rider-1
> sign-off pending at T0.
>
> **Amended 2026-08-15 by the flag-unification re-plan** (reviewer-side note
> `docs/notes/2026-08-15-m3-flag-unification-replan.md`, SIGNOFF
> FLAG-REPLAN-FINAL), which implements **Grant's owner ruling on Q1 rider 1**:
> uniform library flags. The ruling's owner half governs, as the reviewer's own
> rider said it would. **Q1's option-(a) regime-assignment rule is void; T0 is
> dissolved; O11's deferral is void; the answer to the `tests/sqp` CMake
> comment's deadline is now "yes, at U0."** Everything the re-plan does not name
> stands as ratified. Read §0 below before executing any phase-C task.

# M3 Phase C — execution plan (mechanism restructure + TU split + structure-hash re-key)

**Status:** RATIFIED — the phase-C ratification (2026-08-15) rules every §10
open item, folds three amendments (A1–A3), rules Q1/O4 (option (a), with a
stated regime-assignment rule and Grant's governance rider), and routes Q6b to
Grant with a codify recommendation. Prior state: REVISED after task review
(`.superpowers/sdd/2026-08-14-hven-m3-plan-revB/phase-c-plan-review.md`,
2026-08-15: NEEDS-REVISION, 0 Critical / 5 Important / 6 Minor, all text-level;
task set, tiering, and batch structure unchanged), then submitted for
ratification. **B1, the R-batches, and the T-series may proceed once the
amendments below are committed with the plan — no re-review of the folds is
required unless a fold proves impossible as specified, which is reported back
to the reviewer, not adapted around.**

**What waited for ratification, now closed.** The §10 open items have been
ruled by the execution reviewer — O4 in particular, ruled option (a), because
its answer changes the TU candidate set from eight to three and therefore
changes what "phase C complete" means. **C0.1–C0.3 never waited.** Their
authority is the gate-B verdict directly, not this plan: the verdict endorsed
the canary and named its timing, mandated B4 M-1's restoration, and routed the
§10 carries. None touches the restructure surface, and C0.1 is independently
gated on Grant's `IPARM-SURFACE` review regardless of this plan's state. **C0.4
is new** (phase-C ratification A2, §2 below) and follows the same C0
non-blocking logic — it is a plan-of-record §7 obligation, not a restructure
task.

**Phase-C base: `31e57b0`** (`m3` HEAD at review time). `54fce6c` landed the
verdict-2 M3-4 Accelerate-arm pins (test + register) and `31e57b0` landed the
three walk-baseline-consumer updates for the licensed amendment; the suite stands
at **1150 registered** there. Both are verdict-implementing commits phase C
builds on, so every count-arithmetic and A/B measurement anchors at `31e57b0`,
not at the gate-B close. (The baseline *amendment* itself is correctly cited at
`02bc6b7` — §9 row 13 — and that citation stays.)

**Authorities, in the order they bind:**

1. The plan of record, `2026-08-14-hven-m3-plan-revB.md` — §2 (target
   mechanism-dir map + collision watch), §3 (phase C + Gate C), §5 (structure-hash
   re-key: composite-key rule and its proof obligation), §6 (the TU split under
   the four-clause ruling), §8 (the M3 gate in full), §9 (the licensed-delta
   ledger).
2. `CLAUDE.md` §5 (what stays header/templated vs what moves to a TU; "never at
   measurable runtime cost" outranks "use separate TUs"), §7 (measurement
   discipline), §8 (process rules), §6 (governance: iparm review, the
   instrument-at-the-boundary preference).
3. `docs/retarget-design-sqp.md` §11 row 7 (the `iparm[17]` canary obligation),
   §12/§12.1 (everything the execution review already settled — reproduced in §9
   below as "do not reopen"), and **that note's own M2.5 addendum, items
   (a)/(b)/(c)**. Distinct from, and never to be confused with, **the gate-B
   *request note*'s §12 addendum, items 1/2/4** (`docs/notes/2026-08-14-m3-gate-b-review-request.md`)
   — the error-mapping disposition, the O(nnz) disclosure, and the two phase-C
   items. Two same-named addenda in two documents; every citation below names
   its document.
4. The gate-B verdict, `2026-08-15-m3-gate-b-review.md` — its gate-C-blocking
   carries: the O(nnz) hash-cost benchmark on SSN-heavy families; the `iparm[17]`
   `BackendDefaultPremise` canary riding "phase C's first `hven::linear`-adjacent
   commit"; B4 M-1's inertia re-read restoration; the stale-comment sweep folded
   into move-then-restructure commits; the §10 carries (including the
   **tycho-side rig-pin ruling**, tracked here at §7/O12) and the dangling D-note
   cleanup.
5. The tree as it stands on `m3` at **`31e57b0`**: 20 headers in
   `include/hven/detail/sqp/`, one TU in `src/sqp/`, the SQP suite compiling under
   sandbox-matched flags (`tests/sqp/CMakeLists.txt:10-25`), `bench/` likewise,
   the PCH's opt-in list and source-count tripwire (`src/CMakeLists.txt:100-200`),
   suite at 1150 registered.

**Phase C's internal law** (plan §6 clause 4, generalized from §3): every commit
changes ONE thing and proves invariance before the next starts. A relocation
commit contains no content change; a content change contains no relocation.

---

## 0. Flag unification re-plan (2026-08-15)

Folded from the flag-unification re-plan (2026-08-15), reviewer-side note
`docs/notes/2026-08-15-m3-flag-unification-replan.md`. It is the execution half
of **Grant's owner ruling on Q1 rider 1**; where it and the ratification
disagree, this section governs.

### 0.1 Grant's owner ruling

- **Uniform library flags.** The library is one flag regime. The
  non-uniform per-source/object-library regime option (a) proposed is not
  taken.
- **TUs split cleanly for build efficiency, without sacrificing performance.**
  Where to draw a TU boundary is decided on code quality and build efficiency.
- **Code quality is not limited to preserve census bit-identity.** Bit-identity
  against the *existing* census baseline is not a constraint on how the library
  is flagged or structured.
- **The runtime-neutrality bar is unchanged.** P-BENCH per boundary stands, and
  CLAUDE.md §5's "never at measurable runtime cost" outranking clause stands —
  Grant's own "without sacrificing performance" re-affirms it. The ruling
  removes bit-identity as a constraint on *flags*; it removes nothing from the
  performance bar.

**Superseded by this section:** Q1's option-(a) regime-assignment rule (void);
the gate-A ruling that harness TUs stay sandbox-matched indefinitely (void);
T0-as-ruling-task and its pilot (dissolved, §6); O11's deferral (void — §10).
**Not superseded:** the runtime-neutrality bar; the census-frequency ruling's
schedule and floor clause (they re-base onto the new baseline unchanged, §0.4);
every non-flag ruling in the ratification.

### 0.2 The structural insight: unify FIRST, in a commit that moves no code

The engine is header-mostly, and the harness (the `tests/sqp` and `bench/` TUs)
compiles nearly all engine code today — so **the pin-moving event is the harness
flag change itself, not the TU splits**. The moment `tests/sqp` and `bench/`
adopt `COMPILE_FLAGS`, every header-inlined FP path in the engine recompiles
under `-ffast-math`/`-march=native`, and every flag-sensitive pin moves AT ONCE,
regardless of how many TU splits have happened. Two consequences:

1. **"Re-derive once after all splits land" is the wrong shape.** Each
   FP-carrying split would move pins again relative to the previous state (more
   code crossing regimes each time), the suite would be partially red for the
   whole T-series, and the final delta would be confounded — flags × code
   motion × split boundaries.
2. **The right shape is a single unification commit — U0 — that changes FLAGS
   ONLY and moves ZERO code**, immediately followed by its declared mass
   re-derivation as one event. Old-vs-new is then purely flag-attributable (the
   cleanest available experimental design), and every subsequent T-split moves
   code between *same-flagged* homes — which **restores per-boundary
   bit-identity as the proof currency for the entire T-series**. Grant's ruling
   removes bit-identity as a constraint on flags; U0's placement gets it back as
   a proof tool for the splits, free.

### 0.3 Revised phase-C sequencing

**B2** (mid-flight at the ruling; completes under current flags — its evidence
stays valid and its semantic carries) → **R1–R6** → **S1–S4** (flag-invariant
restructures, proven against today's stable pins; S3's content-identity proof in
particular wants the old-flag world) → **U0 + the mass re-derivation event**
(one task, one declared event, §6) → **T1–T8** (ordinary splits under uniform
flags, per-boundary bit-identity restored, P-BENCH/P-BUILD per the ratified
plan) → **H1–H3** (per-backend no-op proven against the POST-U0 state) → **the
one counter census** → **Gate C full 57** — the last two against the NEW
baseline.

### 0.4 The census-frequency ruling, re-based

The census-frequency schedule (§10 O3) is **unchanged in structure** and
re-bases onto the new baseline: P-SUITE + P-SYM per relocation batch, P-CENSUS
at the relocation-group boundary and on every content change, one counter census
post-H3, Gate C's full 57 at the gate, floor clause standing. The only change is
the object of comparison: **the post-H3 census and Gate C's full 57 run against
the NEW baseline U0's event derives**, not against
`bench/baselines/2026-08-06-corpus/walk_baseline.csv`.

---

## 1. Proof vocabulary

Defined once here; every task below cites these by name rather than restating
them. All asserted runs are `MKL_NUM_THREADS=1`, serialized (never two
suites/sweeps concurrently, CLAUDE.md §7), from a clean-configure tree so the
provenance stamp names a real commit.

| Tag | What it is | Cost |
|---|---|---|
| **P-SUITE** | Full `ctest` — `hven_tests`, `hven_interior_tests`, `hven_sqp_tests`, `hven_fault_injection_tests` — in **Release AND Debug**, with the count arithmetic stated against the **1150-registered** phase-C entry baseline (N new tests move the count by exactly N) and against **both** configuration shapes (SNOPT-enabled = the pinned figure; no-SNOPT = recorded beside it, plan §8 item 2) | ~minutes |
| **P-SYM** | **Release**-config per-symbol disassembly identity, `CCACHE_DISABLE=1`, identical configure on the same host, against the immediately preceding build. **Object set:** `libhven.a`, every `hven_sqp_tests` object, **and the `bench/` objects the asserted artifacts come from** — `hven_sqp_corpus` (the census binary), `hven_sqp_bench`, `ssn_safeguard_probe`. Only licensed difference: `__FILE__`-derived string content (assert/throw messages, debug info). **Coverage limit:** the Accelerate `#ifdef` arms are not compiled on Linux at all, so P-SYM says nothing about them — acceptable because the census baseline is MKL-derived and the Accelerate proof lands at H3 and Gate C. Precedent for the comparison: the PCH table's "identical instructions per symbol" (`src/CMakeLists.txt:149-152`). Debug objects differ trivially and are not part of P-SYM; Debug correctness is P-SUITE's job. **T-task scoping (phase-C ratification A1, 2026-08-15):** for T-tasks (§6), P-SYM is asserted over all symbols OUTSIDE the task's declared blast radius — the moved bodies and their direct callers, enumerated in the commit message; inside the radius the proof is P-CENSUS + P-BENCH + P-SUITE, not P-SYM, because out-of-lining a function changes its callers' codegen by construction and an unscoped P-SYM would be an impossible obligation for a T-task | ~one build |
| **P-CENSUS** | 57-cell walk census byte-identical to the baseline of record, via `scripts/run_walk_census.sh` under the accepted parallel-tiered protocol (gate-B verdict §1.1). **The baseline of record is `bench/baselines/2026-08-06-corpus/walk_baseline.csv` as amended at `02bc6b7` up to and including S4, and the NEW dated baseline U0's re-derivation event produces from U0 onward** (§0.4, §6 U0) | ~6 h |
| **P-WSB** | Warm-start battery + `ScaleF7Slow` run explicitly + the four `--from-csv` artifact re-scores + every `kPins` row + `bench_scale --self-check` | ~1 h |
| **P-BENCH** | `hven_sqp_bench` (`bench_scale`), `hven_sqp_corpus`, `hven_sqp_ssn_safeguard_probe`, single-threaded, provenance-stamped, against the phase-C base measurement taken in **B1**. (These are the substitute vehicles for plan §6 clause 3's named psiopt-envelope sweep, which does not exist in this repository — see §12 Q6a) | ~1 h/arm |
| **P-BUILD** | Clean-build **parallel wall-clock** (`-j$(nproc)`) and **peak RSS** (`/usr/bin/time -v` on the build), `CCACHE_DISABLE=1`, three runs, median reported. Build-side benefit is judged on these two, never serial CPU-time (CLAUDE.md §5) | ~10 min |
| **P-PCH** | `scripts/check_pch_neutrality.sh` for any new TU + the `_hven_expected_source_count` tripwire updated in the same commit that adds the source | ~10 min |
| **P-RIG** | Golden rig, P- and T-series, old seam vs migrated engine; Mac leg at the gate only, and gated on the tycho-side rig-pin ruling (§7) | ~1 h Linux |
| **P-MAC** | The Accelerate arm of whichever of the above the task names. Requires a scheduled Grant-hardware session — see §7 | scheduled |

**Per-batch proof default (relocation commits): P-SUITE + P-SYM.** The
justification for not running P-CENSUS on every relocation batch is in §10 O3 and
is a reviewer decision, not a unilateral one.

---

## 2. C0 — the pre-work commits

These do not wait for ratification (see the status block). Two of the three are
gate-B verdict obligations that the verdict explicitly said should not wait for a
convenient moment.

### C0.1 — `iparm[17]` `BackendDefaultPremise` canary (Opus)

**Why first:** the gate-B verdict, Verdict 3: extending `BackendDefaultPremise`
to `iparm[17]` "should ride phase C's **first** `hven::linear`-adjacent commit
rather than waiting for a convenient moment." C0.1 is designed to *be* that
commit.

**Scope.** `iparm[17] = -1` is written unconditionally on every analyze
(`src/linear/pardiso_session.cpp:361-370`), gated by no `Options` field, and is
delta-free *only because* `pardisoinit`'s own mtype = −2 initialization already
sets it to −1. That premise carries no canary today
(`docs/retarget-design-sqp.md` §11 ledger row 7), so a future backend-default
drift would be silent. The `iparm[9]` correction found at gate B (predicted 13,
observed 8) is the proof this class of premise needs a test, not a belief.

**Exact edits:**

- `include/hven/detail/linear/fault_injection.h` — add
  `post_pardisoinit_factor_nnz_request_iparm` beside the five existing
  `post_pardisoinit_*` fields (`:154-159`, `:201-206`) and to `reset()`.
- `src/linear/pardiso_session.cpp` — capture it at the **existing**
  post-`pardisoinit` capture point (`:220-232`), *before* the `iparm_[17] = -1`
  write at `:370`. Timing matters for the same reason the `iparm[33]` comment
  records: a boundary-timed read would report this session's own write, not
  `pardisoinit`'s default.
- `tests/linear/test_fault_injection.cpp` — extend
  `BackendDefaultPremise.MklPardisoinitDefaultsTheRefinementCapAndPivotPerturbExponent`
  or add a sibling asserting `== -1`, with the failure message naming ledger row
  7's delta-free argument as the thing that stops holding.

**Governance.** This is an *extension of an already-sanctioned inside-the-session
deviation* (CLAUDE.md §6; `docs/retarget-design-sqp.md` §2.2's act-evidence
extension was ENDORSED at that note's §12.1 on stated terms). The terms carry:
byte-identical-object proof for the production build re-run, `notices/` and
`docs/testing.md` updated in the same commit. It **observes** the iparm surface
rather than changing it — but it touches `pardiso_session.cpp` and the iparm
array, so **flag it `IPARM-SURFACE: requires human review (Grant)` anyway**; the
cost of an unnecessary label is zero and the gate-B disposition on `ab8aeda` is
the record of what the opposite mistake costs. See §10 O9.

**Proof:** P-SUITE (count arithmetic: 1150 → 1151) + the byte-identical-object
re-run + P-SYM. **Alone** — it is a governance-flagged commit and must be
reviewable on its own.

### C0.2 — B4 M-1: restore the inertia re-read (Opus)

**Scope.** The dissolved `PatternHashDetectsChange` asserted `n_neg == 1` on a
re-read after a cross-pattern re-factorize; its successor
`KktFactor.NeedsAnalysisPreservesCallSiteCounting` asserts the outcome status
across the pattern change but not the inertia value. Restore the assertion.
Verdict's framing: "disclosed assertion-strength regressions should decay toward
zero, not accumulate."

**Exact edits:** `tests/sqp/test_kkt_calls.cpp` —
`KktFactor.NeedsAnalysisPreservesCallSiteCounting`, add the `n_neg == 1` re-read
after the second `factorize_checked`. No count change (assertion added to an
existing test), so the count arithmetic row reads +0 against 1150 and says so.

**Proof:** P-SUITE. **Batchable** with C0.3.

**[erratum, C0 execution]** The text above says the restored re-read follows
"the second `factorize_checked`." The correct placement — and the one
implemented in `5fd56d2` — is after the THIRD call, the cross-pattern
re-factorize: the second call is a value-only change, not the cross-pattern
re-factorize the assertion is meant to observe. Declared per the C0
implementer's adjudication.

### C0.3 — verdict small items that must not wait (Opus)

Three documentation/reference repairs that the restructure would otherwise churn
twice:

- **Dangling D-note references.** ~a dozen citations to `D14`–`D19`/`D22` in
  test comments point at `docs/notes/2026-07-*` / `2026-08-01` files that do not
  exist in this repository. Disposition per the gate-B request note §10:
  **repoint**, not migrate — the observations are quoted in full at the citing
  sites, and the origin notes are a citable archive that plan §2 explicitly does
  not migrate. Repoint every reference to
  `docs/notes/2026-08-14-accelerate-divergence-register.md` or, where the
  register has no row, to the quoting comment itself with an explicit "origin
  note not migrated, observation quoted here" marker.
- **`docs/testing.md` and `docs/ci.md` entries** owed for the per-backend arm
  convention, the divergence register's role and evidence bar, and the JUnit
  artifact the lanes produce (request note §10).
- **`docs/testing.md`'s missing entry for the migrated SQP suite** (gate-A carry)
  — write the entry that exists today, and mark the TU-structure half as owed to
  U0/T9 rather than pre-writing a structure that is not yet ruled.
- **The consumer-sweep protocol** (phase-C ratification, Notes for the record §4,
  2026-08-15): document that declared amendments of pinned artifacts include a
  same-commit consumer sweep — the line from the reviewer's post-gate-B reply,
  landing in this documentation pass because it has no other natural home in the
  task list.

**Proof:** P-SUITE (docs/comments only; **run P-SYM once here to calibrate its
`__FILE__`-class noise floor** before the relocation batches depend on it).
**Batchable** with C0.2.

### C0.4 — the plan-of-record §7 obligations (phase-C ratification A2, 2026-08-15) (Opus)

**Why.** Verified by the execution reviewer before ratifying: the gate-B package
contains **no mention** of the nine-trace reachability ratification, the T5(b)
fixture errata, or the constraint-side seam fixture; no `ConstraintUnsupported`
fixture exists under `tests/sqp/`; no count-arithmetic row in B1–B4 accounts for
one. These are plan-of-record obligations (rev B §1.2 and §7): the fixture was
committed to M3 explicitly, and the reachability ratification is "the one gate
addition M3 makes to the rig itself." The gate-B review consolidated §2.6 "with
no new demands" and thereby missed their absence — a miss the ratification
records as shared, not assigned, and this task is the repair.

**Scope, exactly as ratified:**

1. **The nine-trace reachability ratification.** Compare each reconstructed
   recipe's structure against the migrated fixture it names, upgrade the
   provenance strings ("reconstructed" → "verified against fixture") or document
   and adjudicate the mismatch. Runs any time after the base is stable; MUST be
   complete before Gate C item 3 is ticked. **Gate C item 3's "carried from gate
   B if already discharged there" hedge is resolved: it was not discharged at
   gate B — it happens in phase C.**
2. **The T5(b) errata.** Correct the trace authority's nonexistent-fixture
   reference; the mechanics (an errata note in hven's rig docs vs. an amendment
   to the archived authority note, which remains writable until M3 closes) are
   the implementer's choice, disposition recorded either way.
3. **The constraint-side seam fixture** (plan rev B §1.2, accepted from the M2
   handoff): ONE objective-only fixture type registered via
   `ConstraintUnsupported<T>`, ONE compile-fail probe asserting the authored
   constraint-route message and the absence of raw diagnostics, modeled on the
   existing probe family, with its count arithmetic stated. The M2 cost note
   (~90 s serial per probe) is the reason it stays ONE fixture.

**Proof:** P-SUITE (count arithmetic stated for the new fixture/probe) + the
reachability-comparison artifact. **Alone.**

**NOT in C0** — deliberately deferred, with reasons: the stale-comment sweep
(gate-B verdict routes it into the move-then-restructure commits "where the
inventory rule naturally permits it" → folded into R2/R5 and the S4 residue
commit); the `WallDeadline` in-child assertion (follow-up, not phase C); the
seam-executable direct pin for the `kBackendError` mapping (the gate-B request
note's §12 addendum item 1 calls it a *candidate*, not an obligation → §10 O10).

---

## 3. B1/B2 — the O(nnz) hash-cost obligation

The gate-B verdict makes this **gate-C blocking** and names the family: "on
SSN-heavy families specifically."

### B1 — price the extra pattern hash (Opus)

**Scope, measurement only, no source change.** The retarget's one disclosed
steady-state cost (gate-B **request note** §12 addendum item 2): steady state
with the call-site probe computes the pattern hash **three** times per SSN major
— `needs_analysis` at the call site, `factorize_checked`'s own `needs_analysis`,
and `factorize()`'s internal `hven::pattern_hash` — where the dissolved seam
computed **two**; on the analysis branch a **fourth** is computed for the record
where a local would have served.

**Method.** Measure at the **phase-C base `31e57b0`**, before any relocation, so
the number is attributable to the retarget and not to the restructure:

1. Instrument-free A/B: build `31e57b0` and the archived pre-retarget engine at
   the pinned tag, run `hven_sqp_ssn_safeguard_probe` and the SSN-heavy corpus
   cells (the `kSsn` arm of the gate battery, the SSN-heavy parametric families)
   under P-BENCH discipline.
2. Direct cost attribution: a throwaway (uncommitted) counter around
   `hven::pattern_hash` plus `nnz` per call, to report hash-bytes/major against
   factorization time/major. This is a measurement scaffold, not a shipped
   change.
3. Report as a committed artifact under `docs/notes/data/` with a provenance
   stamp naming `31e57b0`, and state the verdict in the language CLAUDE.md §7
   requires: counters are the asserted currency, wall-clock is informational — so
   the *claim* here is a wall-clock claim and is labelled as such.
4. **Record the hash-count-per-major end-state row** (phase-C ratification A3,
   2026-08-15): three (phase-C base `31e57b0`) → the post-B2 count (if B2's
   decision rule fires) → what H3's composite key implies for the steady-state
   hash count — one table row in the same artifact, so the B-series measurement
   and the H-series re-key cannot silently disagree about how many hashes steady
   state pays. No new run required.

**Decision rule, stated before the measurement (so it cannot be tuned to the
outcome):** if the extra hash is ≤ 1 % of SSN-major wall-clock on the SSN-heavy
families, it is declared noise, recorded, and B2 does not run. Above 1 %, B2
runs. Above 5 %, B2 is gate-blocking rather than optional.

**Disclosure:** B2 is the one plan-added task with no direct mandate — the
verdict requires the *measurement*, not the remediation. Its pre-stated decision
rule and conditional status are what keep it a disclosed addition rather than
scope creep.

**Proof:** P-BENCH (both arms) + the artifact. **Alone.**

**[B1 outcome, 2026-08-15 — `docs/notes/data/2026-08-15-m3-b1-hash-cost/`]**
Measured at `31e57b0` under both instruments the method names. The extra hash is
**6.12 %–7.76 % of SSN-major wall-clock** (median 7.34 %) on the SSN-heavy
families — equivalently ~20 % of one numeric KKT factorization — and removing it
cuts whole-solve wall by **5.58 % pooled** over the thirteen `qp_minors = 0`
cells, with **zero counter movement** across eight passes. The two percentages
have **different denominators** and only the first is the rule's metric; the range
is over 11 of the 13 cells under an exclusion rule stated in the artifact's §5,
and the excluded pair's evidence points the same way. **The 5 % threshold is
crossed: B2 fires, and it is GATE-BLOCKING, not optional.** Two method points
declared there rather than here: the "archived pre-retarget engine at the pinned
tag" does not exist in this repository (no tags), so arm B is `31e57b0` plus an
uncommitted patch reproducing the dissolved seam's two-hash steady state; and
the A3 end-state row reads **3 → 2 → 2**.

### B2 — (conditional on B1) remove the redundant hashes (Opus)

**Scope.** The redundancy is structural, not algorithmic: `factorize_checked`
recomputes what the call site's `needs_analysis` just computed, and the analysis
branch computes a fourth for the record. Both are removable without changing any
decision — thread the computed hash through rather than recomputing it, and reuse
the analysis-branch value for the record.

**Constraint that makes this delicate:** `needs_analysis`'s call-site-counting
contract (`kkt_calls.h:55-58`) exists so `symbolic_analyses` counts once per
logical change. Any threading change must preserve that contract exactly; the
B4-era test `KktFactor.NeedsAnalysisPreservesCallSiteCounting` (as strengthened
by C0.2) is the pin.

**Proof:** P-SUITE + P-CENSUS + P-BENCH (showing the cost gone). This is a
**content change on the solve path** — P-CENSUS is mandatory, not optional.
**Alone.**

**Ordering note.** B1/B2 run **before** the relocation batches so the
`kkt_calls` content is settled before `kkt_calls.h/.cpp` relocates in R2, and
before the H-series changes the hash function underneath the measurement.

---

## 4. R1–R6 — the mechanism-dir relocations

Six relocation-only batches. **Content-identical; include-path updates only.**
Each is one commit. The stale-comment sweep folds into R2 and R5 where the moved
file is the one carrying the stale comment (gate-B verdict, §10 fold); everything
else waits for S4.

**Placement convention adopted here** (the reviewer's to ratify — §10 O1):
consumer-facing headers go to `include/hven/<dir>/`; mechanism internals go to
`include/hven/detail/<dir>/`, following the IPM precedent exactly
(`drivers/interior_point_solver.h` and `model/nlp_problem.h` public;
`detail/interior/`, `detail/globalization/` internal).

**Forced include-churn, mapped in advance so it is not mistaken for content
drift** (confirmed against the intra-SQP include graph at review): `sqp_types.h`
(R1) includes `globalization.h` + `warm_start.h`, so R4/R5 re-edit
`drivers/sqp_types.h`; `bordered_eqp.h` (R2) includes three R3 headers, so R3
re-edits it; `continuation.h` (R4) includes `sqp_driver.h`, so R6 re-edits it.
Each is an include-line touch inside a later relocation commit's legitimate
inventory — no breakage, no hidden content change.

### R1 — leaf types, the model contract, the ledger (Opus)

| Old path | New path |
|---|---|
| `include/hven/detail/sqp/nlp_model.h` | `include/hven/model/nlp_model.h` |
| `include/hven/detail/sqp/types.h` | `include/hven/qp/qp_types.h` |
| `include/hven/detail/sqp/sqp_types.h` | `include/hven/drivers/sqp_types.h` |
| `include/hven/detail/sqp/ledger.h` | `include/hven/core/ledger.h` |

Rationale per file: `NlpModel` is the ABC users derive from → public `model/`
(plan §2 row 1; M4 rebases it, M3 does not). `types.h` holds `QpOptions`,
`SolveOverrides`, `QpCounters`, `BoundState`, `QpStatus`,
`WorkingSetLinearAlgebra` — QP-engine surface that `SqpOptions` embeds, so it is
consumer-reachable → public `qp/`, **not** `core/` as plan §2's row literally
says. That row cannot execute literally (filename collision with the existing
`include/hven/core/types.h`), and its own note names the intent: the *aliases*
are the `core/` part. Resolution ratified by the task review (§12 Q2); O7 is the
reviewer's ratification of the map row's amended letter. `sqp_types.h` is
options/status/counters/iterate/solution → public `drivers/` now, split at **S2**.
`ledger.h` is instrumentation, which CLAUDE.md §2 puts in `core/` — with the
layering consequence stated and fixed at S2 (see there).

Blast radius: `sqp_types.h` is included by nearly every SQP header, so R1 is the
widest include-path churn of the six. That is a reason to do it **first** (every
later batch's diff is then free of it), not to split it.

**Proof:** P-SUITE + P-SYM. **Alone** (widest churn; a reviewable diff needs to
be a pure `git mv` + include rewrite).

### R2 — the KKT tier (Opus)

| Old path | New path |
|---|---|
| `include/hven/detail/sqp/schur_complement.h` | `include/hven/detail/kkt/schur_complement.h` |
| `include/hven/detail/sqp/border_ops.h` | `include/hven/detail/kkt/border_ops.h` |
| `include/hven/detail/sqp/bordered_eqp.h` | `include/hven/detail/kkt/bordered_eqp.h` |
| `include/hven/detail/sqp/kkt_assembly.h` | `include/hven/detail/kkt/kkt_assembly.h` |
| `include/hven/detail/sqp/kkt_calls.h` | `include/hven/detail/kkt/kkt_calls.h` |
| `src/sqp/kkt_calls.cpp` | `src/kkt/kkt_calls.cpp` |

`kkt_assembly.h` and `kkt_calls.h` have **no row in plan §2's table** (both
post-date it: `kkt_calls` is a phase-B product, and `docs/retarget-design-sqp.md`
§12 says its home is "re-homed by C regardless"). `kkt/` is the obvious home for
both — flagged in §10 O6 as a map completion rather than a silent choice.

The `.cpp` move edits `src/CMakeLists.txt`'s source list (path only; the source
count is unchanged, so the PCH tripwire does not fire). The long FP-freedom
comment at `src/CMakeLists.txt:11-29` moves with it **verbatim** — it is the
phase-B flags declaration, **U0 is what discharges it** (U0 deletes the
flag-regime boundary declaration outright; §6 U0), and rewriting it here would
be a content change.

**Stale-comment fold:** `SchurComplement::solve`'s singular-throw message still
names `LAPACKE_dsytrf` (review M-4), and `kkt_calls.h` carries B3-era historical
references. Both files are in this batch's inventory, so the sweep is permitted
here — **but** a message-text edit is a content change. Do it as a **second
commit in the same batch**, not inside the relocation commit (§10 O2 asks the
reviewer to ratify this two-commit shape for R2 and R5).

**Proof:** P-SUITE + P-SYM for the relocation commit; P-SUITE + P-SYM + a search
showing no pin asserts the old message text, for the comment/message commit.

### R3 — the QP tier (Opus)

| Old path | New path |
|---|---|
| `include/hven/detail/sqp/qp_engine.h` | `include/hven/detail/qp/qp_engine.h` |
| `include/hven/detail/sqp/ssn_engine.h` | `include/hven/detail/qp/ssn_engine.h` |
| `include/hven/detail/sqp/qp_problem.h` | `include/hven/detail/qp/qp_problem.h` |
| `include/hven/detail/sqp/working_set.h` | `include/hven/detail/qp/working_set.h` |
| `include/hven/detail/sqp/eqp_solve.h` | `include/hven/detail/qp/eqp_solve.h` |

8 765 lines of the engine's two hottest headers. Nothing else in the batch.

**Proof:** P-SUITE + P-SYM. **Alone.** This is the batch where P-SYM earns its
keep: if a "relocation-only" commit moved a single line of `ssn_engine.h`, P-SYM
finds it in one build instead of six census hours.

### R4 — the warm-start tier (Opus)

| Old path | New path |
|---|---|
| `include/hven/detail/sqp/warm_start.h` | `include/hven/detail/warmstart/warm_start.h` |
| `include/hven/detail/sqp/mesh_transfer.h` | `include/hven/detail/warmstart/mesh_transfer.h` |
| `include/hven/detail/sqp/predictor.h` | `include/hven/detail/warmstart/predictor.h` |
| `include/hven/detail/sqp/continuation.h` | `include/hven/detail/warmstart/continuation.h` |

Note the directory is `warmstart/` (no underscore) per CLAUDE.md §2's map; the
file keeps its `warm_start.h` name.

**Proof:** P-SUITE + P-SYM. **Batchable with R5** if the reviewer prefers five
batches to six; recommended separate because the two have different collision
risk profiles.

### R5 — globalization, and the collision decision (Opus)

| Old path | New path |
|---|---|
| `include/hven/detail/sqp/globalization.h` | `include/hven/detail/globalization/sqp/globalization.h` |

**The collision watch (plan §2) resolved, with evidence.**
`include/hven/detail/globalization/` holds 23 IPM headers. I checked the actual
type names on both sides: **zero type-name collisions today** (IPM contributes
`FunnelAcceptance`, `SocRecovery`, `RestorationStrategy`, `GlobalizationMechanism`,
…; SQP contributes `GlobalizationStrategy`, `FunnelStrategy`, `StepContext`,
`StepVerdict`). The exposure is **file names**, and it materialises at **S3**,
not here: the carve-out will want `soc.h`, `restoration.h`, `funnel.h`, and
`detail/globalization/` already has `soc.h`, `restoration.h`,
`l1_restoration.h`, `funnel_acceptance.h`.

**Recommendation: an `sqp/` subdirectory** under `detail/globalization/`, not
`sqp_*` filename prefixes. It makes every future carve-out collision-free without
a naming convention anyone has to remember, it mirrors the sanctioned `detail::`
nesting mechanism at the directory level, and it keeps the IPM's 23 headers
untouched. Prefixes would work today and rot the first time someone forgets one.
(§10 O8 — the plan explicitly makes this "an execution-time choice the
implementer proposes and the execution review approves.")

**Stale-comment fold:** none in this file's inventory; the two uninventoried test
files (`tests/sqp/test_scale_smoke.cpp` ~L715-717, `tests/sqp/test_warm_start.cpp`
L934/L944) are not in R5's inventory either — they go to S4.

**Proof:** P-SUITE + P-SYM. **Batchable with R4.**

### R6 — the driver (Opus)

| Old path | New path |
|---|---|
| `include/hven/detail/sqp/sqp_driver.h` | `include/hven/drivers/sqp_driver.h` |

6 900 lines, public placement (users construct `SqpDriver`), and after this the
directory `include/hven/detail/sqp/` is **empty and deleted** — which is itself
the batch's most useful assertion: the relocation group is complete iff that
directory no longer exists.

**Proof:** P-SUITE + P-SYM + **P-CENSUS** (the one census in the relocation
group, run at the group boundary — see §10 O3) + P-WSB.

**Amended 2026-08-15 — the P-CENSUS term above is superseded.** The
census-frequency ruling (2026-08-15, reviewer-side
`docs/notes/2026-08-15-m3-census-scope-ruling.md`, Ruling 2 condition 1) fixes
the census schedule at one post-H3 counter census plus Gate C, replacing this
row's group-boundary P-CENSUS. Its operative sentence, quoted verbatim: "Gate C
(and every future gate) runs the full 57; the post-H3 counter census is the ONE
intermediate census; any further scope or frequency reduction is pre-refused —
a proposal to run Gate C itself reduced would break ratified gate currency and
does not need to be asked." R6 accordingly did not run P-CENSUS at the group
boundary. Supporting evidence: R6's byte-identity-chain check across all six
relocation batches' P-SYM verdicts
(`.superpowers/sdd/2026-08-14-hven-m3-plan-revB/task-c-r6-report.md` §6 table —
R1–R6 all PASS; R2's single noise-class object independently adjudicated to 0
instructions changed), showing the census binary's solve paths provably
unchanged since the last census-proven state. Note that the ruling document
itself is a reviewer-side artifact and is not present in this repository — see
the R6 report §6 and §9 item 1 for the full diagnosis.

---

## 5. S1–S4 — the restructures (content changes, each separately proven)

Move-then-restructure means these are separate commits from R1–R6, not that they
must be far away from them. Each S task follows its R batch's merge.

### S1 — reconcile the type aliases (Opus)

**Scope.** `hven::solvers::Index` is `Eigen::Index`; `hven::Index` is
`std::int64_t`. `hven::solvers::SpMatU` is
`Eigen::SparseMatrix<double, Eigen::RowMajor>`; `hven::SpMatRM` is the same type.
Plan §2: "reconcile, don't duplicate." Delete the SQP-side aliases from
`qp/qp_types.h` and use `hven/core/types.h`'s.

**Hazard, stated because it is the whole risk.** On LP64 Linux and both Apple
targets `Eigen::Index` (`std::ptrdiff_t`) and `std::int64_t` are the same type,
so this should be a no-op at codegen. "Should be" is not the bar. Two things can
bite: an overload set that resolves differently if the types ever differ (Windows
is LLP64 — `ptrdiff_t` and `int64_t` are both `long long`, still the same, but
this must be *checked*, not assumed), and `SpMatU`'s `StorageIndex` (`int`) being
unaffected either way. Verify with a `static_assert(std::is_same_v<...>)` landed
in the same commit, so the reconcile's premise is a compile-time pin rather than
a claim.

**Proof:** P-SUITE + P-SYM (expected: identical, no `__FILE__`-class difference
either) + P-CENSUS. **Alone.**

**Amended 2026-08-15, S1 executed — the hazard paragraph's Apple premise is
measured-FALSE, and the pin is what proved it.** On macOS arm64,
`std::int64_t` is `long long` while `std::ptrdiff_t`/`Eigen::Index` is `long`:
same width and signedness, distinct types. The mandatory `static_assert`
landed exactly as this section required (`58dac2a`) and failed the
`macos-clang-release` lane (CI run 31902660573) while Linux and Windows passed
the same commit — the check-don't-assume discipline this paragraph asked for
is the only instrument that could have caught it, since every Linux-side proof
(P-SUITE, P-SYM) was green on the failing commit. Outcome (`9e90595`,
execution-reviewer ruling `2026-08-15-m3-s1-ruling.md` reviewer-side, SIGNOFF
S1-RULING-FINAL, Ruling 1 confirmed by the owner): S1 closed at two-of-three —
`Vec`/`SpMatU` reconciled with identity pins; `Index` ruled UNRECONCILABLE,
staying `Eigen::Index` with the measured reason and CI citation inline in
`qp/qp_types.h` and its pin re-aimed at the width/signedness invariant the
SQP↔linear-algebra boundary actually needs. Redefining `hven::Index` as
`std::ptrdiff_t` (full unification at zero width change) is recorded as a
post-M3 `core/` public-surface question; nothing in phase C depends on it.
Teaching point for every later task touching a platform-dependent typedef:
land the compile-time pin in the same commit and treat the CI matrix, not the
local proof stack, as the verdict.

### S2 — split `sqp_types.h`, and close the `core/` layering inversion (Opus)

**Scope.** Plan §2: "options/diagnostics/counters split into `core/` per the
counter contract." Executing that row literally *also* fixes a layering problem
the R1 placement creates, which is why the two are one task.

**The inversion, stated.** `ledger.h` includes `sqp_types.h`; `sqp_types.h`
includes `globalization.h` and `warm_start.h`. After R1, `core/ledger.h` would
include `drivers/sqp_types.h` and transitively `detail/{globalization,warmstart}/`
— `core/`, the bottom tier of CLAUDE.md §2's map, depending upward on `drivers/`.
Splitting counters alone does not close it: `Ledger::level_histogram` and
`SqpCounters::start_level_used` both consume `StartLevel`, which lives in
`warm_start.h:291`, and `Ledger::SolveRecord` consumes `QpCounters` from
`qp/qp_types.h`.

**Disposition: eliminate the inversion, do not record it.** Three options were
available (move the types down, accept-and-record, or re-home the ledger); moving
the types down is the one plan §2's own row already asks for, so it costs no new
decision. The split:

| From | To | What |
|---|---|---|
| `drivers/sqp_types.h` | `core/solver_counters.h` | `SqpCounters`, `SsnCounters` |
| `qp/qp_types.h` | `core/solver_counters.h` | `QpCounters` — a counters type, so the counter contract's home applies to it too |
| `drivers/sqp_types.h` | `core/solver_status.h` | `SqpStatus` + its `to_string` declaration (diagnostics) |
| `qp/qp_types.h` | `core/solver_status.h` | `QpStatus` (diagnostics) |
| `warm_start.h` | `core/start_level.h` | `StartLevel` + its `to_string` declaration; `StartLevelHistogram` moves here from `ledger.h` |
| `drivers/sqp_types.h` | stays | `SqpOptions`, `SqpIterate`, `SqpSolution`, the `SsnSigmaRule`/`SsnHintRule`/`SsnInfeasibilityRule` enums, `QpMode`, the `kWarm*` constants |
| `qp/qp_types.h` | stays | `QpOptions`, `SolveOverrides`, `BoundState`, `WorkingSetLinearAlgebra` |

After this, `core/ledger.h` includes only `core/` headers, and the map's tier
order holds in the include graph rather than only on paper.

**Discipline:** header-split only. No declaration changes, and **no reordering of
aggregate members** — a `SqpCounters` layout change would be observable through
the census's counter columns. The four `to_string` declarations that move here
are the same ones T1 later TU-izes; moving them is S2's job, out-of-lining them
is T1's.

**Proof:** P-SUITE + P-SYM + **P-CENSUS** (this is a wider content change than
the draft's version of S2 and touches the counters' own headers, so the census is
mandatory, not optional). **Alone.**

### S2b — vocabulary rename onto the library's aliases (owner-directed; Opus)

**Added 2026-08-15 by owner override.** The execution reviewer's S1 Ruling 2
("the re-exports stand as landed, permanently") was overruled by the owner:
"Rename in M3. The same aliases should be used consistently throughout hven."
The reviewer's ruling stands on its own record; this row is the override's
execution shape, communicated to the reviewer the same day.

**Scope.** Move every call site onto the library vocabulary and delete the two
reconcilable solvers-side aliases: `hven::solvers::Vec` → `hven::Vec`,
`hven::solvers::SpMatU` → `hven::SpMatRM` (~237 occurrences across ~40 files,
plus restoring `Vec` resolution in the ~30 test/bench TUs that reach it via
`using namespace hven::solvers;`). The upper-triangle mnemonic the `U` carried
is recorded as a comment at the KKT assembly fill site instead of a type name.
**`hven::solvers::Index` SURVIVES** — per the S1 amendment above it is a
genuinely distinct type on Apple, not a redundant alias, and cannot move onto
`hven::Index` unless the deferred post-M3 redefinition happens.

**Hazards, recorded at S2 so they are not rediscovered:** (1) deleting the
`Vec`/`SpMatU` aliases must NOT delete S1's `Index` width/signedness
`static_assert` or the `SpMatU`-adjacent pins' surviving analogues; (2)
`hven::solvers::Index` has two edit sites (`qp/qp_types.h` normative,
`core/start_level.h` re-declaration) that must move together if it ever moves.

**Sequencing:** after S2 (+ its review), before S3 — so the S3 carve of
`sqp_driver.h` happens in the final vocabulary and its content-identity proof
is not followed by another text pass over the carved files.

**Proof:** P-SUITE both configs + P-SYM byte-identity (a rename of an
alias-of-an-alias is codegen-neutral; any P-SYM difference means the change
was not mechanical — stop and report, do not adjudicate). No census
(schedule ruling unchanged). **Alone.**

**Executed 2026-08-15 (`f390d40`), review clean.** One proof-vocabulary
correction from execution, binding on later mechanical tasks: an
insertion-bearing mechanical change's P-SYM bar is "byte-identical OR
class-(a) with 1:1 insertion accounting, and library objects strictly
byte-identical" — S2b's mandated using-declarations shifted `__LINE__` in 23
test objects (each audited to its file's exact inserted-line count; all
library and bench objects byte-identical), so a flat "expect 60/60" prediction
mis-sets the alarm for tasks whose own scope inserts lines.

### S2c — redefine `hven::Index` onto Eigen's index; retire the last alias (owner-directed; Opus)

**Added 2026-08-15 by owner order** (extending the S1 ruling's recorded-not-
ordered rider into an M3 task; the execution reviewer's hazard scan,
reviewer-side `2026-08-15-m3-s2c-hazards.md` SIGNOFF S2C-HAZARDS-FINAL, found
no blocking hazard and endorsed shape and sequencing). Full index
unification: one index type on every platform, no vocabulary exceptions.

**Commit 1 — the type event alone.** `core/types.h`: `Index` =
`std::int64_t` → `Eigen::Index` (`std::ptrdiff_t`), contract rewritten from
fixed-width to pointer-width-64-on-all-supported-targets, with same-commit
pins (`sizeof==8` + signedness + `is_same_v<Index, Eigen::Index>`). Per the
hazard scan's Flag 1, `Fnv1a::feed_index`'s parameter re-types to literal
`std::int64_t` and `docs/pattern-hash.md`'s wording drops its alias
dependence, same commit — no hash value moves on any supported target, but
the hash's width contract must not ride the alias. Per Flag 2, the
pre-landing scan covers committed artifacts (no expected table/baseline CSV
carries a hash value column — negative result recorded in the commit
message) plus code reliance on literal `int64_t` identity. On Linux (LP64)
and Windows (LLP64) the redefinition is provably no-type-change; macOS arm64
is the real type change, evidenced by same-commit pins plus the macOS lane's
full suite. ZERO numeric churn is the expectation: any non-`__FILE__` Linux
P-SYM difference fires the stop-and-report rule — U0's re-derivation is a
safety net, not a license.

**Commit 2 — the alias retires.** `hven::solvers::Index` (now the same type
everywhere) deletes from both edit sites together (`qp/qp_types.h`,
`core/start_level.h` — S2's recorded hazard); S1's Apple-finding comments
become historical records; S1's width/signedness pin is replaced by the
identity pin S1 originally specified, now true on every target.

**Proof:** P-SUITE both configs; P-SYM at both commits (Linux byte-identity
expected at each, class-(a) licensed only with 1:1 insertion accounting per
the S2b correction above); CI all three lanes with the macOS lane as the
Apple evidence. No census. **Alone.**

**Executed 2026-08-15 (`bcdf287` type event / `9875d69` retirement /
`9d80c23` declared fix), review clean.** Three commits, not two: the
retirement broke the macOS lane on `tests/sqp/test_accelerate_probes.cpp`,
whose whole body is `#ifdef`-gated — a Linux-error enumeration of call sites
is structurally blind to platform-gated TUs, and an amend of the
already-pushed commit was (correctly) refused, so the one-line fix landed
declared. Zero numeric churn held at every step; both scans negative; macOS
1065/1065 on the real type change (run 31918684985). Method carry-forwards:
(1) any future rename/retirement task must enumerate call sites in
platform-gated TUs by grep, not by compile error; (2)
`scripts/check_accelerate_syntax_linux.sh` does not cover
`test_accelerate_probes.cpp` — extending it is a candidate improvement, not
yet done.

### S3 — carve the globalization logic out of `sqp_driver.h` (**FABLE**)

**Scope.** Plan §2 row 6 + §6 clause 2: "the funnel/TR/SOC/elastic/restoration
logic inside `sqp_driver.h`" moves to `globalization/`, and this is the
**restructure half** — its TU-ization is a separate step (T-series).

Concretely, from `include/hven/drivers/sqp_driver.h` into
`include/hven/detail/globalization/sqp/`:

| Source region | New file |
|---|---|
| `kTrGrowThreshold`/`kTrGrowFactor`/`kTrShrinkFactor`/`kZeroStepScale` (`:2149-2194`) + the TR update logic inside `SqpDriver` | `trust_region.h` |
| `build_soc_subproblem` (`:2685`) | `soc.h` |
| `kElastic*` constants, `ElasticQp` (`:2761`), `build_elastic_subproblem`, `set_elastic_penalty`, `elastic_seed`, `elastic_project` (`:2737`–`:3106`) | `elastic.h` |
| `kRestoreRadiusFactor`, `kRestorationSlackBound`, `RestorationModel` (`:3108`–`:3362`) | `restoration.h` |
| (already there) `globalization.h`'s `GlobalizationStrategy`/`FunnelStrategy` | `globalization.h` |

**Why this is the Fable-hard one.** It is the only task in phase C that carves a
6 900-line header into pieces while asserting the result is content-identical.
Four specific hazards, each of which can produce a *silent* behavioral change
that reads as a compile-fix:

1. **Definition-order dependence.** `inline constexpr` constants consumed by
   functions defined earlier in the file will not compile after a naive split,
   and "fix the compile error by moving one more thing" is exactly how a
   restructure becomes a content change.
2. **ODR and inline linkage.** Everything here is `inline` in a header today; it
   must stay `inline` in the new headers, or the TU-ization step (T-series)
   inherits a link error it will be tempted to fix by changing semantics.
3. **Circularity.** `RestorationModel` derives from `NlpModel` and is constructed
   by the driver; `SqpDriver` calls the restoration path. The carve-out must not
   create a cycle between `drivers/sqp_driver.h` and
   `detail/globalization/sqp/restoration.h` — forward declaration or a
   driver-side hook, decided at execution and stated in the commit message.
4. **Floating-point identity across the move.** Moving `kFunnelDelta * h * h`
   from one header to another changes nothing *if* the flags are the same. The
   flags **are** the same for this task (headers, compiled by `hven_sqp_tests`
   under sandbox flags) — which is precisely why S3 is a restructure and the
   T-splits are a separate step.

**Ordering (re-plan condition 4, §0):** **S3 lands BEFORE U0.** Its
content-identity proof wants the old-flag world and today's stable pins; running
it after the unification would confound a carve-out proof with a flag change.
The same logic puts the H-series and the Mac session's pre-re-key capture
*after* U0.

**Proof:** P-SUITE + P-SYM + **P-CENSUS** + P-WSB. **Alone.**

**Executed 2026-08-15 (`dc98d70`), review clean, ruling closed.** P-SYM 60/60
byte-identical with zero noise; content identity mechanical both directions;
`sqp_driver.h` byte-exactly reconstructed from base + three declared edits.
The carve table's "+ the TR update logic inside `SqpDriver`" clause was NOT
taken and the execution reviewer ruled it **INAPPLICABLE-AS-WRITTEN**
(reviewer-side `2026-08-15-m3-s3-ruling.md`, SIGNOFF S3-RULING-FINAL): the
update logic is loop orchestration reading `opts_` — driver code by the same
orchestration-vs-mechanism line that shaped the map; it travels with the loop
when T5 TU-izes it. Recorded as a **scope clarification for Gate C's
behavioral ledger, not a delta**; the debt is killed — any future extraction
is a NEW designed content change, explicitly not an S3 leftover. The ruling's
second rider is a **hard requirement on U0** (see the U0 section). Fourth
L-1 flake occurrence on this task's run, recorded in the register.

### S4 — the stale-comment sweep residue (Opus)

**Scope.** What the R-batch inventories did not permit:
`tests/sqp/test_scale_smoke.cpp` (~L715-717) and `tests/sqp/test_warm_start.cpp`
(L934, L944) still name the dissolved KKT seam. Plus any comment the R/S commits
newly staled (an inventory of "comments naming `detail/sqp/`" after R6).
Added from S2b execution (2026-08-15), DISCHARGED by S2c the same day: the
`qp/qp_types.h` dangling-phrase/heading pair was rewritten out of existence by
S2c commit 2's historical rewrite (verified in the S2c review). S4 owes only
a confirming glance, no edit.

**Proof:** P-SUITE + P-SYM. **Batchable** — this is the one task that can ride
alongside another commit if the reviewer prefers fewer commits, since it is
comment-only.

**Executed 2026-08-15 (`9ddb2f2` + fix round `85240bc`), review closed.**
P-SYM 60/60 byte-identical on both commits. All 16 inventory items
dispositioned; the fix round re-derived the exclusion classification over the
whole population and re-aimed eleven moved-referent sites (closure grep now
empty). Two of the fixes were correctness-bearing (the bench probes'
shadow-include recipes — a stale path there silently measures the unpatched
engine); both verified end-to-end in review. Deferred to S4b with counts
re-derived: **40 rename-class `types.h` references across 15 files** + **19
phase-A self-name banners** — and a recorded hazard: **three of the 40
(`qp_engine.h:2646,:2652,:2657`) are inside throw-message string literals**,
so S4b cannot hold a byte-identical P-SYM bar for those (`.rodata` and length
immediates move; message-text changes are licensed by §10 item 11's R2
precedent). `src/CMakeLists.txt:11,25,28`'s stale `src/sqp/` naming was
deliberately left — U0's first act deletes that block.

### S4b — the deferred reference sweep (post-U0; Opus)

**Added 2026-08-15 from S4's execution record.** Scope: the 40 rename-class
`types.h` references (15 files), the 19 phase-A self-name banners, and any
stragglers S4's closure grep class definitions surface. Sequenced **after
U0**: the three throw-message string-literal sites are a content change under
pre-U0 pins but ride ordinarily under the post-U0 evidence world with the
message-text license above; the rest is comment-only. Proof: P-SUITE +
P-SYM with the string-literal sites' objects accounted explicitly (not
byte-identical for those TUs, by declared necessity). **Batchable** with a
T-series commit if the reviewer prefers.

---

## 6. U0 and T1–T9 — flag unification and the TU split

### U0 — the flag-unification commit and its mass re-derivation event (**FABLE**)

> **EXECUTED AND ACCEPTED (folded 2026-08-17).** The unit is
> `bf8e897..62bcab2` (six commits, pushed, two consecutive fully-green
> three-lane CI runs 31988455020 / 31989138134). Result: 74 solved cells
> (57-cell census + 17-cell bench set) re-derived twice each — **zero
> status flips, zero counter movement, zero verdict movement**; every
> old-vs-new delta is a `kkt_residual` last-digit change. Two census runs
> byte-agreed on all schema columns except `wall_s`; no serial-confirm
> owed. P-BENCH pooled median −0.97 % (range −2.07 %…+1.21 %),
> counter-identical across arms. New baseline of record:
> `bench/baselines/2026-08-16-u0-corpus/` (hven-native header,
> eight-consumer sweep green in the repoint commit); old baselines frozen.
> Delta report: `docs/notes/data/2026-08-16-m3-u0-rederivation/`. The CI
> hard requirement is DISCHARGED with measured outcome (HVEN_SIMD_ARCH
> lane pinning; L-1 4 failures → 1 explained MKL-dispatch residue → green
> lanes; register updated). Debug arithmetic proven unchanged; six
> config-split pins keep every pre-U0 Debug value. Acceptance:
> reviewer verdict ACCEPTED
> (reviewer-side `2026-08-16-m3-u0-review.md`, SIGNOFF
> U0-REVIEW-FINAL — items 1–3, 5, 6 closed there, incl. M3-6 accepted
> with a named M5 re-open trigger now folded into the Accelerate
> register); owner acknowledgment relayed 2026-08-17. One item routed to
> the owner and tracked outside this task: the `nlp_model.h`
> "bit-identical" `eval_values` contract sentence (reviewer recommends
> re-wording to "identical under value-preserving compilation; agreement
> to reassociation residue under the library's flag regime"). Execution
> was split across two implementers by owner budget directive (Fable
> first half, Opus second half); state file
> `.superpowers/sdd/2026-08-14-hven-m3-plan-revB/task-c-u0-report.md`.
> **The re-derived pins are now the bar; the T-series proceeds against
> them with per-boundary bit-identity as its proof currency.**

Folded from the flag-unification re-plan (2026-08-15). **This is now the phase's
measurement-critical task** — the Fable-tier assignment T0 held moves here, to
the re-derivation event.

**Why it is one task and not two.** Per §0.2, the harness flag change *is* the
pin-moving event; the TU splits are not. Unifying first, in a commit that moves
no code, makes the old-vs-new delta purely flag-attributable and hands the
T-series back per-boundary bit-identity as its proof currency.

#### (a) The commit: flags only, zero code motion

Engine, `tests/sqp`, and `bench/` all move onto `COMPILE_FLAGS`. In the same
commit, and nothing else:

- Delete the flag-regime boundary declaration at `src/CMakeLists.txt:11-29` —
  the phase-B comment forbidding FP-carrying SQP code in `src/sqp/` "before
  phase C's per-boundary bit-identity proofs settle the
  library-flags-vs-engine-TUs question." The question is settled; the
  declaration goes.
- Delete the `tests/sqp/CMakeLists.txt:10-25` flags exception comment — the one
  that defers the decision to "no later than the phase-C TU split." Its deadline
  is answered here, "yes."
- `kkt_calls.cpp`'s bespoke FP-freedom argument becomes historical. Harmless
  either way: it may stay as a comment or go, implementer's choice, stated in
  the commit message.

**The design note T0 was to have written collapses into this task** (§6 T0
below): one page — the unified flag set, the re-derivation protocol, the
delta-report bar.

**HARD REQUIREMENT added 2026-08-15 (S3 ruling rider, S3-RULING-FINAL): the
design note must decide the CI flag posture explicitly — "unaddressed" is not
an option.** Four L-1 occurrences (same four tests, same sites, thread pin in
place, byte-identical objects) make runner-microarchitecture/MKL-dispatch
variance the working explanation, and `-march=native` on a heterogeneous CI
fleet means per-runner codegen: counter and status pins are asserted on CI,
and the CI-vs-local divergence widens by construction under `native`. The
options (a fixed `-march` baseline for CI lanes vs `native` locally; or
runner-class pinning) are U0's to design; re-derived pins must be assertable
on the lanes that actually run them. Related: S4 confirmed
`src/CMakeLists.txt:11,25,28` still name `src/sqp/` — deleted by this
commit's first act, no separate fix owed.

#### (b) The same-event mass re-derivation and its evidence package

The re-derivation is U0's **same-event companion**, not a follow-up task.

1. **Two independent reproductions of every re-derived artifact, before it
   becomes the bar** — the licensing precedent applied at mass scale. The new
   census baseline comes from **two full-57 runs** under the ratified
   parallel-tiered protocol; battery artifacts and suite pins are likewise
   two-run byte-agreed. Gate C is then a **third** witness, not the second.
2. **The old-vs-new delta CHARACTERIZED, not merely declared** — a committed
   delta report carrying:
   - per-cell counter deltas;
   - **status flips enumerated, with statuses-must-not-regress as a HARD BAR**:
     a solved cell that stops solving is a finding that **halts acceptance**,
     not flag noise. DNF→Optimal boundary flips get physics-cell-style
     adjudication;
   - an old-vs-new **KKT-residual distribution comparison** — systematic
     degradation beyond flag-plausible bounds is a finding;
   - battery counter deltas;
   - **U0's P-BENCH old-vs-new**: Grant's "without sacrificing performance"
     measured at the event itself. Uniform flags should be neutral-or-better,
     and what they buy is worth recording.
3. **A NEW baseline file, not an in-place rewrite.** The gate-A and gate-B
   records cite the current `walk_baseline.csv` *by content*; a 57-row in-place
   rewrite would muddy those citations. So: a **new dated baseline directory**,
   the CMake define **repointed** to it, and the **old file frozen as historical
   evidence** — which also confines Q6b's origin-string question to the old
   file, since the new header is hven-native.
4. **The consumer sweep** (the protocol line from the C0.4 round, §2) run for
   every amended or repointed artifact, **in the same commit**.
5. **The frozen-artifact live-comparison tests dispositioned.** The
   frozen-resweep cross-comparisons (`TheReSweptWalkArmIsCounterIdentical…` and
   the D0-repair twin) assert continuity between origin-era artifacts and the
   live engine — a claim the flag unification **ends**. They either **retire
   with declaration** (their continuity purpose was discharged on the gate-B
   record) or **convert to frozen-vs-frozen**; the single-cell
   adjudicated-exception mechanism does not stretch to 57 cells. The four
   `--from-csv` re-scores of frozen artifacts **survive** — the reader re-scores
   frozen bytes and its arithmetic is flag-insensitive — but that is
   **verified at the event, not assumed**.
6. **Acceptance gate.** The delta report is **reviewed by the execution reviewer
   and summarized to Grant as owner BEFORE the new pins become the bar.** A mass
   re-derivation must not self-accept.

#### (c) Suite float pins: mechanical for most, four classes need design attention

Mechanical run-and-pin (two reproductions) for the bulk, EXCEPT:

1. **Cross-config pins** — tables and tests asserting the same float in Release
   AND Debug. Verify which of the unified flags are config-invariant; where
   Release-only flags move values, **pins split per config** (the context-pin
   machinery already supports build-config pinning — use it, **never widen a
   tolerance**).
2. **Knife-edge counter and zero-pins** — `suspect_escalations == 0` fixtures,
   escape counts, refusal counts, the battery's exact minors. Re-derived values
   get a **per-pin plausibility eyeball in the delta report**: a zero-pin going
   nonzero, or an escape appearing where none existed, is a **reviewed finding,
   not an auto-pin**. **Status and verdict pins must not move at all**; any that
   do **halt the event**.
3. **Documented-verdict constants** (the gate evaluator) — recomputed from the
   new baseline. If any **G-gate VERDICT** (not figure) flips under the new
   flags, **surface it loudly**: that is Phase-8-relevant evidence, not
   bookkeeping.
4. **Accelerate-side values** — M3-4 arm pins, divergence-register entries,
   report-only rows. Re-observed on Mac CI under the unified flag set's Apple
   form, two-run bar for counters, register updated; folded into the
   already-scheduled Mac session (§7, O12) where possible.

#### Binding conditions

1. **U0 moves zero code** — flags and the two deleted boundary declarations
   only. **Any code motion found in U0's diff makes the delta unattributable and
   fails the commit.**
2. The mass re-derivation is U0's same-event companion; **the suite is never
   left red across a task boundary** — U0 + re-derivation land as **one proven
   unit**.
3. The (b) package **in full**, including the statuses-must-not-regress halt
   condition and the acceptance gate (reviewer review + owner summary before the
   new pins are the bar).
4. **S3 lands before U0** (content-identity proof against stable pins); the
   **H-series after** (no-op proof against the post-U0 world); the **Mac
   session's pre-re-key capture is post-U0** by the same logic.
5. **Everything not named here stands as ratified.**

**Proof:** the U0 design note + the (b) evidence package in full — two-run
P-CENSUS (new baseline), two-run P-WSB, P-SUITE in Release AND Debug, P-BENCH
old-vs-new, the committed delta report, the consumer sweep — plus the
acceptance gate. **Alone.**

### T0 — DISSOLVED (flag-unification re-plan, 2026-08-15)

**No admission test, no regime machinery, no pilot.** Grant's owner ruling
removed the question T0 existed to answer; the re-plan dissolves the task. Its
**deliverable collapses into U0's design note** — one page: the flag set, the
re-derivation protocol, the delta-report bar — and its **Fable-tier assignment
moves to U0's re-derivation event**, which is now the phase's
measurement-critical task.

Two consequences that were T0's and are now nobody's: the regime-assignment rule
(void — there is one regime) and the pilot's per-symbol P-SYM bar for
`FunnelStrategy::accept` (moot — there is no pilot). §1's T-task P-SYM scoping
rule (phase-C ratification A1) is **unaffected** and remains the general T-task
bar.

### T1–T9 — the candidate set

**Fifteen candidates enumerated from the actual headers; eight recommended (all
eight now un-gated — the T0 gate is dissolved and T4–T8 are ordinary splits
under uniform flags); seven rejected** — all 20 headers
dispositioned. The whole SQP tree is effectively non-template (three `template`
declarations across 24 957 lines, in `warm_start.h`, `ssn_engine.h`,
`mesh_transfer.h`), so CLAUDE.md §5's "stays templated" clause does almost no
work here; the deciding question is **call frequency and inlining sensitivity**,
and LTO is **opt-in and off by default** (`cmake/hven_compile_options.cmake:37`),
so a cross-TU call really does lose inlining.

#### Recommended (FP-arithmetic-free — flag-indifferent, land whenever convenient)

| # | Candidate | Sites | Frequency | Why it is safe |
|---|---|---|---|---|
| **T1** | Printing: `to_string(SqpStatus)`, `to_string(StepVerdict)`, `to_string(StartLevel)`, `to_string(PredictorOutcome)`, `format_iteration_table(SqpSolution)` | `sqp_types.h:408`, `sqp_driver.h:6828`, `warm_start.h:298`, `predictor.h:489`, `sqp_driver.h:6871` (pre-S2/S3 coordinates) | O(1) per solve / per report row | Switch-to-string and `fmt` formatting. No FP arithmetic — `fmt` *reads* doubles, it does not compute with them. CLAUDE.md §5 names printing explicitly. New TU: `src/drivers/sqp_print.cpp` (IPM precedent: `interior_point_solver_print.cpp`) |
| **T2** | `Ledger::level_histogram`, `Ledger::summary_table`, `Ledger::sqp_summary_table` | `core/ledger.h:180-278` | Once per report | Integer counters + string formatting only. CLAUDE.md §5 names ledger code explicitly. Keep `record()`/accessors inline (one `push_back` per solve; moving them buys nothing). New TU: `src/core/ledger.cpp` |
| **T3** | `SqpDriver`'s constructor option validation, `sqp_driver.h:3986-4030` | | Once per driver construction | Comparisons + `fmt`-formatted throws. **The NaN-branch pin stays exactly as planned, on an updated premise (flag-unification re-plan, 2026-08-15):** the `!(x > 0.0)` NaN-catching idiom depends on `-fno-finite-math-only`, and **the unified flag set includes it** — that is now the premise the disassembly verification pins, in the same commit. New TU: `src/drivers/sqp_options.cpp` |

**T1 and T2 are flag-indifferent** (FP-free) and may land before or after U0.
T3's pin is written against the unified set, so it lands **after U0**.

#### Recommended, un-gated (FP-carrying orchestration — plan §6 clause 2's actual content)

**Un-gated by the flag-unification re-plan (2026-08-15).** T0's gate is
dissolved. T4–T8 run as **ordinary splits under uniform flags** with the
ratified proof set, and with **per-boundary bit-identity RESTORED** — same flags
on both sides of every boundary, per §0.2 — and P-SYM's A1 blast-radius scoping
intact.

| # | Candidate | Sites | Frequency | Note |
|---|---|---|---|---|
| **T4** | `FunnelStrategy` (the globalization state machine) | `detail/globalization/sqp/globalization.h:428-806` post-S3 | Once per major | Already virtual-dispatched through `GlobalizationStrategy`, so out-of-lining costs nothing the vtable was not already costing. The FP is `h_new <= beta*width_`, `pred_df >= delta*h_old*h_old` — reassociation/contraction exposure was T0's reason to gate it; post-U0 both sides of the boundary carry the same flags, so bit-identity carries the proof |
| **T5** | `SqpDriver::solve`'s major loop — `sqp_driver.h:4031-6828`, i.e. the class body **excluding** the constructor block T3 carves out | | Once per solve, loops per major | The biggest build-time win in the tree and the one plan §6 most wants. Highest bench risk: it currently inlines `eval_nlp`, `evaluate_kkt`, `build_subproblem` |
| **T6** | Elastic/SOC/restoration builders: `build_soc_subproblem`, `ElasticQp` + its four functions, `RestorationModel` | post-S3 `soc.h`, `elastic.h`, `restoration.h` | Per major, on the escape paths only | `RestorationModel` is already virtual (derives `NlpModel`). The elastic builders allocate — call overhead is already dominated |
| **T7** | Warm-start ingest + `StartLevel` resolution | `detail/warmstart/warm_start.h` | Once per solve | Ingest is orchestration by CLAUDE.md §5's own list |
| **T8** | Continuation driver | `detail/warmstart/continuation.h` | Once per continuation step | Same |

#### Rejected — stays header/inline, with the reason

| Candidate | Why it stays |
|---|---|
| `qp_engine.h`'s walk (5 148 lines) | Per-minor and per-element; the reuse-gate hashes and the walk's inner loops are exactly the "per-element hot path that depends on inlining" CLAUDE.md §5 exempts |
| `ssn_engine.h` (3 617 lines) | Same, per SSN minor; also carries one of the three templates |
| `kkt_assembly.h` | Per-major triplet emission, per-element |
| `border_ops.h`, `schur_complement.h`, `bordered_eqp.h`, `eqp_solve.h` | Per-minor dense/sparse kernels |
| `predictor.h`'s algebra core | Per-predictor-call linear algebra; only its `to_string` moves (T1) |
| `mesh_transfer.h` | Carries a template; the non-template half is per-element transfer |
| `working_set.h`, `qp_problem.h`, `qp_types.h`, `nlp_model.h` | Data types and an ABC — nothing to move |

#### T9 — PCH membership and the source-count tripwire (Opus)

Not a separate phase; **an obligation attached to every T-commit that adds a
source.** `src/CMakeLists.txt:193-200` sets `_hven_expected_source_count 19` and
postcondition 3 refuses to configure when a source is added without revisiting
the PCH decision. So each T-task's commit must:

1. bump `_hven_expected_source_count` by exactly the sources it adds;
2. run `scripts/check_pch_neutrality.sh` for the new TU and record the two bars
   (faster **and** byte-identical object) in the measurement table comment;
3. add to `_hven_pch_sources` only if both bars clear — the measured table shows
   the "faster but not byte-identical" group is large, and byte-identity is the
   bar for engine code.

**Prediction, to be falsified by measurement, not asserted:** `sqp_print.cpp` and
`sqp_options.cpp` will resemble the IPM's `interior_point_solver_print.cpp`/
`_settings.cpp` (opt-in list, ~−1.5 s, identical), so they are likely PCH
members; `ledger.cpp` is small and may land in the "slower with the PCH" group
like `solver_init.cpp`.

**Per-T-task proof (all of T1–T8):** P-SUITE + P-SYM + P-CENSUS + **P-BENCH**
(the runtime-neutrality benchmark) + **P-BUILD** (parallel wall-clock + peak RSS,
`CCACHE_DISABLE=1`) + P-PCH. This is plan §6 clause 3 in full: bit-identity AND
bench, per boundary. **A boundary that costs measurable runtime is reverted or
redrawn** — CLAUDE.md §5's clause outranks the split. P-SYM here is scoped per
§1's T-task rule (phase-C ratification A1): asserted outside the task's
declared blast radius only; inside the radius, P-CENSUS + P-BENCH + P-SUITE
carry the proof.

> **OWNER RULING (2026-08-17, at T1+T2's close) — the T-series
> runtime-neutrality standard ("layout-band").** How CLAUDE.md §5's "a
> boundary that costs measurable runtime is reverted or redrawn" is judged
> for every T-task: **counters must be bit-identical across arms; wall-clock
> is judged on the pooled median and the engine-heavy (multi-second) cells;
> the sub-second cells carry an explicit ±1.5 % layout-noise band.** A move
> beyond the band, or any engine-heavy-cell regression, still triggers the
> revert clause. Basis: T1+T2's escalation evidence — the moved functions run
> O(1) per solve (lost inlining arithmetically excluded); a semantically-null
> include-reorder control at the base commit moves the same sub-second cells
> −0.34 %; a same-binary cross-session floor measures pooled +0.03 %; and in
> `bench_corpus.cpp.o` only 2 of 478 text symbols changed size (`main`,
> `write_outcome` — the engine is size-identical) while 96.1 % of the linked
> binary's symbols moved address, isolating the mechanism as emission-order/
> layout, not codegen. U0's accepted ±1.2 % on the same cells is the
> precedent. The reviewer's caveat is recorded with the ruling: the null
> control is a single draw, so the band is calibrated conservatively (1.5 %,
> above every observed layout excursion) rather than fitted.
>
> **AMENDED (owner, 2026-08-18, at T3's close — direction and threshold,
> closing the two gaps the T3 review surfaced):** (a) the sub-second band is
> a **cost gate, one-sided**: a cell fails only beyond **+1.5 % slower**;
> favourable moves of any size pass and are recorded, not adjudicated.
> (b) the engine-heavy limb's threshold is explicit: a **reproducible
> slowdown beyond +0.5 %** on an engine-heavy cell (≈3× the largest
> excursion observed across three arms, 15× the measured +0.03 % pooled
> cross-session floor) triggers §5's revert clause. Counters bit-identical
> remains the hard unconditional bar. Under the amended text T3's P-BENCH
> is discharged as measured. Evidence basis: T1+T2 → T3 shows the
> sub-second class oscillating (+1.20 % then −1.50 % on the same ten
> cells), not accumulating — two independent draws supporting the layout
> mechanism the original ruling adopted.

**Batching:** T1+T2 may share a commit (both FP-free printing/ledger, one new TU
each — but then P-BUILD attributes the build win to the pair, which is
acceptable). T3 alone (the NaN-branch verification is its own argument).
T4–T8 **each alone** — "per-boundary proof" means per boundary.

> **OWNER AMENDMENT (2026-08-19, at T4's close) — the census leg only.**
> Five consecutive full censuses since U0 (U0×2, T1+T2, T3, T4) found zero
> counter movement; each confirmed what P-SYM + P-SUITE + P-BENCH had
> already shown. The cost/evidence ratio no longer supports a ~6 h census
> per boundary. Amended: **T6–T8 keep every per-commit leg (P-SYM, P-SUITE,
> P-BENCH, P-BUILD, P-PCH) but share ONE P-CENSUS at the combined head**,
> T1+T2-pattern; Gate C's full census against the baseline of record
> backstops the series close. T5 retains its own census (highest-risk
> boundary). "Per-boundary proof" is unchanged for every other leg — the
> census's attribution role is what the amendment trades, and a counter
> regression surfacing at the shared run is attributed by bisection over
> the (at most three) candidate commits. **Post-M3 direction (owner, same
> ruling, to be formalized in the close-out carry doc):** the census
> becomes a gate/event/on-demand instrument — milestone gates, flag and
> toolchain events, releases, and whenever the cheap legs disagree — not a
> per-change requirement; per-change proof is the ladder (byte-identity
> for mechanical changes; suite + bench counters for behavior-preserving
> work; declared re-derivation for intentional numeric change).

---

## 7. H1–H3 — the structure-hash re-key (plan §5)

**Ordering (re-plan condition 4, §0):** the **whole H-series runs after U0**, and
H3's per-backend no-op is proven **against the post-U0 state** — both the
pre-re-key and post-re-key captures are taken in the unified-flag world, or the
comparison would confound a re-key with a flag change. The Mac session's
pre-re-key capture (O12) is post-U0 for the same reason, and its Accelerate
values are re-observed under the unified set's Apple form at U0 (§6 U0 (c) class
4) before it serves as H3's baseline.

### H1 — the survey (Opus)

**Scope.** Plan §5 item 1. Enumerate every hash site, its ingredient list, and
every consumer of its value. Starting inventory (the survey confirms and
completes it):

| Site | Ingredients | Consumers |
|---|---|---|
| `qp_engine.h:2015` `detail::structural_hash` | `mix_pattern` over H/Ae/Ai: rows, cols, nnz, then **raw `outerIndexPtr`/`innerIndexPtr` bytes** at native `StorageIndex` width | `HotState::structural_hash` (`:2251`), the reuse gate, `WarmStart::structure_hash` |
| `qp_engine.h:2025` `detail::values_hash` | `mix_values`: rows, cols, nnz, then raw value bytes | The reuse gate. **NOT a pattern hash — out of scope for the re-key**, and saying so explicitly is part of H1's job |
| `ssn_engine.h:2961` `structure_hash` | n, me, mi, mb, then **`InnerIterator`-based** pattern feed over H/Ae/Ai, then the bound-row `(var, sign)` list | The SSN rebuild gate |
| `hven::pattern_hash` (`src/core/pattern_hash.cpp`) | rows, cols, nnz, `outer[0..rows]`, `inner[0..nnz)`, each through `feed_index` (64-bit widened) | `kkt_calls.cpp`'s `needs_analysis`; `SymmetricFactor` internally |

**Premise correction H1 must record** (ratified §12 Q4): plan §5's "three
mutually incompatible structure hashes … one omitting `cols`" is stale.
`kkt_system.h::hash_pattern` and its Accelerate twin no longer exist — phase B
dissolved both — and **neither surviving site omits `cols`**; both mix rows,
cols, nnz before the pattern data. The re-key's scope is therefore **two sites
plus one new core entry point**, and the "removes the cols-omitting variant"
benefit was already collected by phase B.

**The obligation that makes H1 gate-relevant:** confirm **no pin asserts a hash
VALUE**. If any does, it is a declared re-derivation, never silent (plan §5 item
1; plan §9 row 3's license is conditional on exactly this).

**Design constraint H1 hands to H2** (ratified §12 Q3): `hven::pattern_hash`
**throws** on an uncompressed matrix (`pattern_hash.cpp:10-15`), while the SSN
site deliberately hashes through `InnerIterator` *because* `QpProblem`'s matrices
are caller-supplied and may be uncompressed (`ssn_engine.h:2973-2985`), and
`qp_engine.h`'s `mix_pattern` pays for a compressed copy only when needed
(`:1952-1975`). A naive re-key would either throw on legal input or force an
O(nnz) compressed copy per SSN major — the exact cost class B1 measures.

**Proof:** the survey artifact. **Alone.**

### H2 — the hven core surface: the multi-matrix continuation entry point (Opus, *with T5-style escalation*)

**Scope.** Plan §5 item 2 and its folded riders:

- A new multi-matrix entry point using **append-style continued accumulation**
  (`Fnv1a` threaded across matrices), **not a fold over per-matrix digests** —
  this settles `docs/pattern-hash.md`'s deliberately-open combined-key question
  (`pattern-hash.md:34-68`) in the same change, as the rider requires.
- Fed **element-wise through `feed_index`** (64-bit widened, byte-order-fixed),
  which removes the build-dependent-width variable the current raw-byte feeds
  carry.
- **A cross-width stability test:** 32-bit and 64-bit index matrices feeding
  equal hashes for equal structures.
- An **iteration-based (uncompressed-tolerant) path** per H1's finding,
  contractually producing the same digest as the compressed path — the outer
  offsets are derivable while iterating, so equal structures hash equal in both
  storage states — with a test pinning that equality.
- `docs/pattern-hash.md` updated: the open question closed, the recipe stated.

**Tier note.** Tiered Opus, but this is the enlarged obligation Q3 created:
reproducing `pattern_hash`'s exact feed order from `InnerIterator` iteration,
cross-width, **as a contract** is closer to S3's difficulty class than to T1's.
**Escalate to Fable if the compressed/uncompressed equality pin does not fall out
of the first design** — the failure mode is a digest that agrees on the fixtures
and disagrees on a gapped matrix, which is a wrong-reuse class, not a test
failure.

**Governance:** a `core/` public-surface change → the M2-style review (plan §5
item 2's rider) and a scoped review lane. Not an iparm change, so CLAUDE.md §6's
iparm clause does not fire.

**Proof:** P-SUITE (new tests: cross-width stability, compressed/uncompressed
equality, continuation-vs-fold distinctness) + P-SYM (the SQP side is untouched by
H2, so its objects must be identical). **Alone.**

### H3 — re-key the SQP sites onto the composite key (**FABLE**)

**Scope.** Plan §5 items 3 and 4.

- `qp_engine.h::structural_hash` → H2's continuation over H/Ae/Ai. Pure
  structural; no conjunct needed.
- `ssn_engine.h::structure_hash` → **composite key**: H2's continuation for the
  structural part (n, me, mi, mb and the three matrices) **plus the bound-row
  `(var, sign)` data as a separate conjunct** (hash or direct comparison —
  implementer's choice per plan §5 item 3).
- `values_hash` untouched.

**Cost constraint carried from B1 (artifact §7, `docs/notes/data/2026-08-15-m3-b1-hash-cost/`).**
The steady-state per-SSN-major `hven::pattern_hash` count is **2 post-B2, and H3
must not move it** — H3 re-keys the two SQP-side hashes, not the KKT-level one, so
it changes the digest and not the count. B1 priced the unit: **one** added O(nnz)
pass per major costs ≈ 7.3 % of SSN-major wall on the SSN-heavy families, so H1's
Q3 constraint (a compressed copy or an extra iteration pass per major) is a
measured cost, not a theoretical one. B1 also records a second, separately-counted
O(nnz) hash H3 should price against: `SsnEngine::structure_hash` itself, computed
unconditionally in `sync_matrix` (`ssn_engine.h:3034`) from both `:1688` and
`:3259` — ≈ 0.15 calls per major on this corpus (≈ 1 % of SSN-major wall), so
worth inventorying at H1 but not a hot path here.

**The two ingredients have different statuses and the composite must preserve
both for different reasons** — verified against source at `ssn_engine.h:3004-3025`:

- **`br.var` is correctness-critical.** `mb` alone does not cover it: a different
  *assignment* of the same *number* of bound rows to variables puts the bound
  block's off-diagonals in different columns of K, and reusing a pattern across
  that would write B's values into A's slots. The source records that a mutation
  dropping this loop **survived the first sweep**. Fixture:
  `PatternKeySeparatesBoundLayoutsOfEqualSize`.
- **`br.sign` is conservative-by-record.** Dropping it would still be *correct*
  (a sign flip moves no slot; the refresh path re-emits it); it is kept because
  the over-conservative key costs one avoidable rebuild and buys never
  re-deriving the argument. The source calls it a knowingly-unkillable line.
  **Preserving it is a behavior-preservation obligation, not a correctness one** —
  and it is what makes plan §9's "no SSN rebuild-count change" achievable.

**Why Fable-hard:** the correctness half is a silent-data-corruption class
(wrong-slot writes), the behavior half must move zero counters, the digest is
changing value *and* feed mechanics at once, and the proof is a byte-identity
argument over reuse *decisions* rather than over the hash values themselves.

**Proof (plan §5 item 4, under the ratified reading — §12 Q5): a per-backend
no-op.** Each backend's post-re-key suite, census, and warm-start battery are
byte-identical to **that backend's own pre-re-key run**, and the MKL arm is
additionally byte-identical to the committed baseline. A *cross-backend*
byte-identical census is impossible by the M3 record's own rulings (verdict 2
pins `ssn_bulk_flips == 4` on Accelerate against MKL's 1 and rules end-state tie
membership backend-dependent by nature), and no re-key could change that; the
per-backend reading is what plan §5 item 4's own parenthetical argues — "reuse
*decisions* are equality comparisons; every decision input is preserved" — and it
is achievable. Concretely: P-SUITE + P-CENSUS + P-WSB on Linux, and P-MAC over
all three on Accelerate, **captured pre-re-key and compared post-re-key** (the
two-pass structure O12 owns). Plus an explicit commit statement that hash
*values* change (licensed by plan §9 row 3, conditional on H1's finding) while
every reuse *decision* is preserved, citing the two fixtures above as the pins.
**Alone.**

**Mac-session prerequisites, tracked (not assumed).** H3's Accelerate arm and
Gate C item 3's rig Mac leg share one scheduled Grant-hardware session, and that
session has **two** dependencies that must be closed before it is booked:

1. **The tycho-side rig-pin ruling** (gate-B request note §10's first carry;
   verdict §1.4's "the three-seam rig's Mac legs behind the tycho pin ruling").
   The old-seam arm's pin keeps drifting and the last runs used the sanctioned
   `HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM=ON` escape; the options on the tycho lane
   are a pinned worktree, re-pointing at HEAD, or relaxing to a tree-hash
   comparison. **Without the ruling the rig's Mac legs run under the escape or
   not at all**, so Gate C item 3 cannot be ticked honestly.
2. **The pre-re-key Accelerate capture** for H3 (above) — it must be taken in the
   same session, before H3 lands, or the post-re-key run has nothing to compare
   against.

Both are §10 O12.

---

## 8. Gate C

Per plan §3's Gate C definition plus plan §8's full M3 gate (phase C HEAD is M3
HEAD) plus the verdict's carries. The gate package is a review-request note in
`docs/notes/` mirroring the gate-A/gate-B instruments, with committed evidence.

**Checklist:**

1. **Per-boundary bit-identity + bench proof, per TU-ruling clause 3** — the
   collected P-SYM/P-CENSUS/P-BENCH/P-BUILD records from every T-task, presented
   per boundary, with parallel wall-clock and peak RSS measured and **never
   regressed at measurable cost**.
2. **Suite + census + rig re-run at phase HEAD** (not just per-task): P-SUITE,
   P-CENSUS, P-RIG.
3. **Golden rig old-vs-new, P- and T-series, with the Mac leg** — gated on the
   tycho-side rig-pin ruling (§7).
   - **3a. The three plan-of-record §7 obligations** (C0.4; phase-C
     ratification A2, 2026-08-15): (i) the nine-trace reachability
     ratification, complete before this item is ticked — the "carried from
     gate B if already discharged there" hedge is **resolved: not discharged
     at gate B, discharged here in phase C**; (ii) the T5(b) fixture errata,
     corrected; (iii) the constraint-side seam fixture
     (`ConstraintUnsupported<T>` + its one compile-fail probe), landed with
     count arithmetic stated.
4. **Full SQP suite at the restated case count, configuration-pinned**: the
   SNOPT-enabled figure is the gate; the no-SNOPT figure recorded beside it;
   both shapes matched in **Release AND Debug**, ± the tests this plan adds
   against the **1150** entry baseline (C0.1 +1, H2's new tests, T3's NaN-branch
   pin, any T-task pin, and **U0's retirements/conversions of the
   frozen-artifact live-comparison tests**, §6 U0 (b) item 5), each enumerated
   with its count arithmetic.
5. **57-cell census byte-identical; warm-start battery; `bench_scale
   --self-check`; every `kPins` row.** **Against the NEW baseline** U0's
   re-derivation event produced (§0.4), with the old baseline file cited as
   frozen history.
6. **Bench parity** — specifically the **B1 O(nnz) hash-cost measurement
   discharged** (the verdict's named gate-C obligation, on SSN-heavy families)
   with B2's remediation if B1's decision rule fired. The gate package states
   explicitly that P-BENCH's SQP vehicles substitute for plan §6 clause 3's named
   psiopt-envelope sweep, which is an IPM-side instrument that does not exist here
   (§12 Q6a).
7. **The `iparm[17]` canary landed** (C0.1) and green.
8. **B4 M-1 restored** (C0.2).
9. **The §10 carries dispositioned**: stale-comment sweep complete (R2/R5/S4),
   dangling D-notes repointed (C0.3), `docs/testing.md`/`docs/ci.md` entries
   written, the tycho-side rig-pin ruling closed (§7), the `WallDeadline`
   margin-not-enforcement carry explicitly re-routed as a follow-up (not phase-C
   work), the error-mapping seam pin either landed or explicitly deferred with the
   reviewer's assent (§10 O10).
10. **The microarch-flake watch rule honoured** (verdict note 2): if a **third**
    GitHub Linux microarch flake occurs during phase C, capture the four test
    names and their observed values into the divergence register's evidence style
    before they scroll out of CI retention. Conditional — nothing owed if it does
    not fire. **FIRED AND DISCHARGED 2026-08-15**: the third occurrence hit S2's
    `e7f89a7` (run 31907664085 attempt 1) — with `MKL_NUM_THREADS=1` already
    pinned, falsifying R5's root cause as sufficient. Captured with full
    per-cell values in `docs/notes/2026-08-15-linux-runner-divergence-register.md`
    (entry L-1, class corrected to "last-bits arithmetic, two of four cells
    surfacing as discrete-state divergences"). The runner-pin/-march question it
    opens is routed to U0's design note.
11. **A behavioral-delta ledger for phase C**, in plan §9's shape. Expected rows,
    named in advance: hash values change under the H-series re-key (licensed by
    plan §9 row 3, conditional on H1); include paths and header names (mechanical);
    backend-error/throw message text where R2's sweep touched it (already licensed
    at `docs/retarget-design-sqp.md` §11 row 4's precedent); and, **re-registered
    under the flag-unification re-plan (2026-08-15), replacing the A3 row that
    was conditional on T0 landing option (a)**: "the library, its tests, and its
    bench are unified onto one flag regime at U0; the flag-sensitive pins,
    battery artifacts, and the 57-cell census baseline are re-derived in that
    same declared event, against a new dated baseline, with the old baseline
    frozen as history" — the flag outcome is a delta in project shape and belongs
    on the ledger where the next phase reads it, not only in U0's note.
    **Nothing else beyond these four rows.**
    Explicitly not licensed: any SSN rebuild-count change (the composite key
    exists to prevent exactly that); any counter, status, float, or census delta
    outside U0's declared re-derivation event — **U0's deltas are licensed by
    that event and its (b) evidence package, and by nothing else**, and its
    statuses-must-not-regress bar means status flips of the halting kind are
    never licensed at all.
12. **Both configs green** (Release + Debug) per the standing phase-merge rule.

**Reviews at the gate:**

- **The sol + Fable dual review** of the branch, as at gate B.
- **The SQP-instance execution review** (mandatory, plan §11) — this plan's §10
  open items are the first thing it should rule on, and it should rule on them
  **before** execution starts, not at the gate.
- **Grant's items**, which no verdict substitutes for: the `IPARM-SURFACE` review
  of C0.1; **the owner summary of U0's delta report before the new pins become
  the bar** (§6 U0 (b) item 6 — this is the item that replaced T0's governance
  question, which Grant's ruling on Q1 rider 1 already answered); and the
  standing gate-B conditions still open at his end. **Q6b is closed** — Grant
  ruled codify, and CLAUDE.md §1 carries the codification.

---

## 9. Settled — do not reopen

Everything below was ruled by `docs/retarget-design-sqp.md` §12/§12.1, the gate-B
verdict, or the plan of record. Phase C executes against them; it does not
relitigate them. Cited so a future reader can find the ruling, not the argument.

| # | Ruling | Authority |
|---|---|---|
| 1 | Don't-write states are `std::optional<int>` with **defaults unchanged** — the ruling required the state to exist, not to be the default | retarget-design-sqp §12, ENDORSED |
| 2 | Accelerate don't-write semantics cite Apple's documented default; platform-neutral factory stands; no explicit-override fallback | retarget-design-sqp §12, ADOPTED |
| 3 | `DenseSymmetricFactor` grows an **evidence struct** (not raw accessors) and an **additive `try_factorize`** (not a mutated `factorize`) | retarget-design-sqp §12 |
| 4 | The `KktFactor` + free-function shape, its name, and the `needs_analysis` probe preserving `symbolic_analyses` call-site counting — approved. **Only its directory is re-homed by C.** **[housekeeping per B2 concern 3]** The B3-era "three free functions" description is amended to the surface B2 shipped: `KktFactor` + `AnalysisDecision` + `analysis_decision()`, `needs_analysis()`, `solve_vec()`, and **two** `factorize_checked()` overloads — the second taking a caller-held `AnalysisDecision`. The addition is **purely additive**: the original three-function surface is unchanged and still supported, and every property this row protects (the shape, the name, `needs_analysis`'s call-site counting contract) is unchanged — `needs_analysis(k, K)` is now `analysis_decision(k, K).needed` and cannot disagree with it. The ruling is not reopened; only its description of the surface is brought current | retarget-design-sqp §12; surface amended at B2 (`ff461ad`) |
| 5 | **Zero IPM-side change.** The IPM keeps its explicit written values, its compatibility cache, and its postponed honesty-state adoption | retarget-design-sqp §12 |
| 6 | `o.num_threads = 0` + the process-env pin is the whole mechanism; **no `KktFactor::set_num_threads` pass-through** | retarget-design-sqp's **M2.5 addendum (c)**, RULED — SILENCE |
| 7 | §4.2's rebuild-gate rewrite (the natively-observed `n_zero != 0` qualifier) | retarget-design-sqp §12.1 |
| 8 | `collect_factor_mflops = false`; `ordering = kBackendDefault` is exact parity on both backends; the fixed-0.01 Accelerate pivot tolerance | retarget-design-sqp §12.1 |
| 9 | §2.2's act-evidence extension of the sanctioned observer deviation, on its stated terms (byte-identical object, `docs/testing.md` + `notices/`) | retarget-design-sqp §12.1, ENDORSED |
| 10 | **Namespace:** `hven::solvers`, no per-mechanism namespaces; collisions resolved by file/dir disambiguation or `detail::` nesting | plan §2, RULED |
| 11 | **M3-4:** per-backend arms pinned (coin directions, end-state `ineq_uncertain[1] == false`, `ssn_bulk_flips == 4` on Accelerate); MKL assertions not relaxed; no algorithmic change; one named re-open trigger (M5 crossover evidence) | verdict 2; landed at `54fce6c` |
| 12 | The **parallel-tiered census protocol** is the protocol of record for counter-asserting replays; no serial re-run | verdict §1.1 |
| 13 | The **baseline amendment at `02bc6b7`** is licensed and is the baseline P-CENSUS compares against | verdict §1.2 |
| 14 | The **eight-row phase-B ledger** is the accepted phase-B delta record | verdict 3 |
| 15 | Hash re-key rules: **continuation, not fold**; the SSN site takes a **composite** key with the bound-row `(var, sign)` conjunct; hash values may change **iff** no pin asserts a value | plan §5 |
| 16 | **Move-then-restructure**; per-boundary proof = bit-identity AND bench; "never at measurable runtime cost" outranks "use separate TUs" | plan §6, CLAUDE.md §5 |
| 17 | Gate count pinned to the **SNOPT-enabled** figure, no-SNOPT recorded beside it, both shapes matched in Release and Debug | plan §8 item 2 |
| 18 | The error-mapping coverage disposition (consumer-level only today) is **accepted as recorded** — the seam pin is a candidate, not a debt | verdict, notes |

## 10. Open for the reviewer — RULED (phase-C ratification, 2026-08-15)

Genuine choices this plan made. Every item below is now **RULED** by the
execution reviewer's ratification (2026-08-15); three of them (O4, O7, O8)
change task content, and **O3's tripwire and O7's `static_assert` pin are part
of the ratified rule, not advice.** Q2–Q5 and Q6a from the draft's
open-questions file were already **resolved** (§12) before ratification.

**Amended 2026-08-15 by the flag-unification re-plan (§0):** **O4's and O11's
rulings are SUPERSEDED by Grant's owner ruling** — the rows below carry both the
ratification's text and what replaced it. **Q6b is RESOLVED-CODIFIED** (Grant
ruled codify; CLAUDE.md §1 carries it). Every other row stands.

| # | Question | This plan's recommendation | Ruling (phase-C ratification, 2026-08-15) |
|---|---|---|---|
| **O1** | **Public vs `detail/` placement** per mechanism dir. Plan §2's map and CLAUDE.md §2 describe public dirs; the IPM precedent puts mechanism internals under `detail/` and only the driver/model contract at the top level | Consumer-facing public (`model/nlp_model.h`, `qp/qp_types.h`, `drivers/sqp_types.h`, `drivers/sqp_driver.h`, `core/ledger.h`, and S2's `core/solver_counters.h` / `core/solver_status.h` / `core/start_level.h`); mechanism internals under `detail/{kkt,qp,warmstart,globalization/sqp}/` | **RULED — as proposed.** Public: `model/nlp_model.h`, `qp/qp_types.h`, `drivers/sqp_types.h`, `drivers/sqp_driver.h`, `core/ledger.h` + S2's three `core/` homes; internals under `detail/{kkt,qp,warmstart,globalization/sqp}/`. Matches the IPM precedent exactly |
| **O2** | **Batch boundaries** — six relocation batches (R1 leaves/model/ledger, R2 kkt, R3 qp, R4 warmstart, R5 globalization, R6 driver), with R2/R5 carrying a **second** commit for their stale-comment fold | As proposed; R4+R5 may merge into five batches if the reviewer prefers | **RULED — as proposed** — six batches, R2/R5 carrying their stale-comment fold as a SECOND commit (content never inside a relocation commit; the two-commit shape is ratified). Keep R4/R5 separate: the collision-profile argument is right and the saving from merging is one ctest run |
| **O3** | **Census frequency per batch.** P-CENSUS is ~6 h. Running it on all six relocation batches costs ~36 h for commits that, by construction, cannot change codegen | **P-SUITE + P-SYM per batch; P-CENSUS once at the relocation-group boundary (R6), then on every content change (B2, S1, S2, S3, all T-tasks, H3) and at Gate C.** The argument: over the object set P-SYM compares — which now includes the census binary `hven_sqp_corpus` and the bench binaries, closing the draft's coverage hole — per-symbol disassembly identity is **strictly stronger than a census for the Linux/MKL build**: a census can only move if codegen moves, and P-SYM proves it did not, in one build instead of six hours. Two bounded limits, stated rather than implied: the Accelerate `#ifdef` arms are not compiled on Linux so P-SYM says nothing about them (acceptable — the census baseline is MKL and the Accelerate proof lands at H3/Gate C), and P-SYM is Release-only (Debug correctness is P-SUITE's). Residual risk is bounded by design: R6's group-boundary P-CENSUS still catches anything that slipped, at the cost of attribution, not detection. If P-SYM shows any non-`__FILE__` difference, the batch is not relocation-only and P-CENSUS becomes mandatory for it | **RULED.** P-SUITE + P-SYM per relocation batch; P-CENSUS at the group boundary (R6) and on every content change. The strictly-stronger argument holds for the Linux/MKL build because the object set includes the census and bench binaries' own objects — that inclusion is what makes it sound, and the stated **tripwire (any non-`__FILE__` P-SYM difference ⇒ the batch is not relocation-only and P-CENSUS becomes mandatory for it) is part of the ratified rule, not advice.** C0.3's noise-floor calibration before the R-batches depend on P-SYM is required, as planned. **Amended 2026-08-15 — the R6-group-boundary census term is superseded.** The census-frequency ruling (2026-08-15, reviewer-side `docs/notes/2026-08-15-m3-census-scope-ruling.md`, Ruling 2 condition 1) fixes the census schedule at one post-H3 counter census plus Gate C, replacing this row's "P-CENSUS once at the relocation-group boundary (R6)" clause. Its operative sentence, quoted verbatim: "Gate C (and every future gate) runs the full 57; the post-H3 counter census is the ONE intermediate census; any further scope or frequency reduction is pre-refused — a proposal to run Gate C itself reduced would break ratified gate currency and does not need to be asked." R6 accordingly ran no P-CENSUS at the group boundary; supporting evidence is R6's byte-identity-chain check across all six relocation batches' P-SYM verdicts (`.superpowers/sdd/2026-08-14-hven-m3-plan-revB/task-c-r6-report.md` §6 table — R1–R6 all PASS, R2's single noise-class object adjudicated to 0 instructions changed). The ruling document is a reviewer-side artifact not present in this repository — see the R6 report §6 and §9 item 1 for the diagnosis |
| **O4** | **The flag-regime ruling (T0)** — option (a) per-source sandbox-matched flags for SQP library TUs, (b) TU-ize only FP-free code, or (c) unify and re-derive every pin | **(a)**, with (b) as the fallback; (c) rejected for M3. Ordering endorsed by the task review. **This decides whether plan §6 clause 2 executes at all**, and it collapses the candidate set 8 → 3 under (b) — so it must be ruled **before the R-batches**. See open-questions Q1 | **SUPERSEDED (flag-unification re-plan, 2026-08-15).** The ratification ruled option (a) via Verdict 2, with a regime-assignment rule and Grant's governance rider. **Grant ruled on that rider: uniform flags — effectively option (c), executed as U0.** The regime-assignment rule is void, T0 is dissolved, and the re-derivation option (c) priced as prohibitive is taken with its full evidence package. See §0 and §6 U0; the ratification's text is kept here as the record of what was superseded |
| **O5** | **The TU candidate set** — 15 analyzed, 8 recommended (T1–T3 unconditional, T4–T8 gated on O4), 7 rejected | As tabled in §6. The reviewer should specifically confirm T5 (the major loop) is wanted given it is the highest bench risk, and that the rejected list's per-element rationale is accepted | **RULED — the 15/8/7 disposition is ratified, and T5 is confirmed wanted** — with the revert rule standing in full force: T5 is the boundary most likely to be REVERTED under CLAUDE.md §5's outranking clause, and a revert there is a finding, not a failure. The inlining loss at the TU boundary applies to calls INTO the TU (once per solve); `eval_nlp`/`evaluate_kkt`/`build_subproblem` remain inlinable *within* the TU since their headers come along — P-BENCH arbitrates, the prediction does not. The rejected-seven's per-element rationale is accepted |
| **O6** | **Map gaps:** `kkt_assembly.h` and `kkt_calls.h` have no row in plan §2's table (both post-date it) | Both to `detail/kkt/`; `src/sqp/kkt_calls.cpp` to `src/kkt/` | **RULED — as proposed** — `kkt_assembly.h`, `kkt_calls.h` → `detail/kkt/`; `src/sqp/kkt_calls.cpp` → `src/kkt/`. Recorded as map completion |
| **O7** | **`types.h` → `core/`** (plan §2's literal row) vs its actual contents, and the same row's governance of S2's counters/diagnostics consolidation | Ratify the amended letter: **aliases** reconcile into `core/types.h` at S1 (+`static_assert` pin); **QP options/enums** to public `qp/qp_types.h` at R1; **counters and status/level enums** to `core/` at S2, which is what closes the `core/ledger.h` layering inversion. Resolution ratified at §12 Q2; O7 is the reviewer's ratification of the map row's letter | **RULED — the amended map letter is ratified**: aliases → `core/types.h` at S1 (with the `static_assert` pin — **REQUIRED, not optional**); QP surface → public `qp/qp_types.h` at R1; counters/status/`StartLevel` → `core/` at S2. The original row was the reviewer's and its literal reading was impossible; this amendment preserves its intent |
| **O8** | **The globalization collision mechanic** — plan §2 offers `funnel_*.h` prefixes or an `sqp_` subdirectory, "an execution-time choice the implementer proposes and the execution review approves" | **`include/hven/detail/globalization/sqp/`.** Evidence: zero *type-name* collisions between the 23 IPM headers and the SQP set today, but S3's carve-out wants `soc.h`/`restoration.h`/`funnel.h` and `detail/globalization/` already has `soc.h`, `restoration.h`, `l1_restoration.h`, `funnel_acceptance.h` | **RULED — the `sqp/` subdirectory** under `detail/globalization/`, as proposed. The evidence (zero type-name collisions today; four file-name collisions waiting at S3) decides it; prefixes rot |
| **O9** | **Does C0.1 need the `IPARM-SURFACE` label?** It observes the iparm array rather than changing it | **Label it anyway.** Zero cost; the gate-B disposition on `ab8aeda` records what the opposite mistake costs | **RULED — label C0.1 `IPARM-SURFACE`.** Ratified with the plan's own reasoning |
| **O10** | **The seam-executable direct pin** for `kkt_calls.cpp`'s `kBackendError` → `std::runtime_error` mapping (the gate-B request note's §12 addendum item 1 calls the seam executable "the phase-C home" and standing it up "a phase-C candidate") | Defer past M3 unless the reviewer wants it. It is a *candidate*, the consumers are pinned, and phase C's task list is already the longest of the three phases | **RULED — defer the seam-executable direct pin past M3** — with the reviewer's assent recorded, which is what the gate-B disposition asked for. The consumers are pinned; a candidate stays a candidate |
| **O11** | **Does the SQP suite move onto unified `COMPILE_FLAGS`?** `tests/sqp/CMakeLists.txt:21-24` defers this decision to "no later than the phase-C TU split" | **No, not in M3.** It is option (c) of O4 and would re-derive a gate-B-accepted baseline. Record the deferral explicitly so the deadline the CMake comment sets is answered rather than passed | **DEFERRAL VOID; ANSWERED "YES, AT U0" (flag-unification re-plan, 2026-08-15).** The ratification ruled the suite does NOT move in M3, restating the gate-A "harness TUs stay sandbox-matched indefinitely" ruling. **Grant's owner ruling voids both.** The `tests/sqp` suite and `bench/` move onto `COMPILE_FLAGS` at U0 — and per §0.2 that move *is* the pin-moving event, not a rider on it. **The CMake comment's deadline is answered, not deferred: the comment is deleted at U0** along with `src/CMakeLists.txt:11-29`'s flag-regime boundary declaration |
| **O12** | **The Mac session, with its two tracked prerequisites.** H3's proof needs an Accelerate arm; Gate C item 3 needs the rig's Mac leg | One scheduled Grant-hardware session serving both, sequenced before Gate C is declared, and **booked only after** (i) the **tycho-side rig-pin ruling** closes — otherwise the rig's Mac legs run under the sanctioned unpinned escape or not at all — and structured as **two passes in one session**: the pre-re-key Accelerate capture (H3's parent commit) and the post-re-key comparison (H3 HEAD), because the per-backend no-op proof (§12 Q5) needs both | **RULED** — one Grant-hardware session, two passes (pre-re-key capture at H3's parent, post-re-key comparison at H3 HEAD), booked only after the tycho-side rig-pin ruling closes. Both prerequisites tracked, neither assumed |

---

## 11. Task list, ordered, with tiers

| Task | What | Tier | Alone / batchable |
|---|---|---|---|
| C0.1 | `iparm[17]` `BackendDefaultPremise` canary (+ `IPARM-SURFACE` label) | Opus | Alone |
| C0.2 | B4 M-1: restore the `n_neg == 1` inertia re-read | Opus | With C0.3 |
| C0.3 | Dangling D-note repoint; `docs/testing.md`/`docs/ci.md` entries; P-SYM noise-floor calibration | Opus | With C0.2 |
| C0.4 | Nine-trace reachability ratification; T5(b) errata; `ConstraintUnsupported<T>` seam fixture (phase-C ratification A2) | Opus | Alone |
| B1 | O(nnz) hash-cost benchmark on SSN-heavy families at `31e57b0` (measure only) | Opus | Alone |
| B2 | (conditional) remove the redundant per-major hashes | Opus | Alone |
| R1 | Relocate `nlp_model.h`, `types.h`, `sqp_types.h`, `ledger.h` | Opus | Alone |
| R2 | Relocate the KKT tier (5 headers + 1 TU) + stale-comment commit | Opus | Alone (2 commits) |
| R3 | Relocate the QP tier (5 headers) | Opus | Alone |
| R4 | Relocate the warm-start tier (4 headers) | Opus | With R5 |
| R5 | Relocate `globalization.h` into `detail/globalization/sqp/` | Opus | With R4 |
| R6 | Relocate `sqp_driver.h`; `detail/sqp/` deleted | Opus | Alone |
| S1 | Reconcile `Index`/`Vec`/`SpMatU` onto `core/types.h` | Opus | Alone |
| S2 | Split `sqp_types.h`; counters/status/`StartLevel` → `core/`; close the ledger layering inversion | Opus | Alone |
| S3 | Carve funnel/TR/SOC/elastic/restoration out of `sqp_driver.h` | **Fable** | Alone |
| S4 | Stale-comment sweep residue | Opus | Batchable |
| ~~T0~~ | ~~The flag-regime ruling + pilot~~ — **DISSOLVED** (flag-unification re-plan, 2026-08-15); deliverable collapses into U0's design note | — | — |
| **U0** | **Flag unification (flags only, zero code motion) + the mass re-derivation event** — one task, one declared event | **Fable** | Alone (one proven unit) |
| T1 | Printing TU (`src/drivers/sqp_print.cpp`) | Opus | With T2; flag-indifferent |
| T2 | Ledger TU (`src/core/ledger.cpp`) | Opus | With T1; flag-indifferent |
| T3 | Options/validation TU (`src/drivers/sqp_options.cpp`) | Opus | Alone (post-U0: its NaN-branch pin is written against the unified set) |
| T4 | `FunnelStrategy` TU | Opus | Alone (post-U0) |
| T5 | `SqpDriver::solve` major-loop TU | Opus *(borderline Fable — see below)* | Alone (post-U0) |
| T6 | Elastic/SOC/restoration TUs | Opus | Alone (post-U0) |
| T7 | Warm-start ingest TU | Opus | Alone (post-U0) |
| T8 | Continuation TU | Opus | Alone (post-U0) |
| T9 | PCH membership + source-count tripwire | Opus | Folded into each T commit |
| H1 | Hash survey (incl. the plan §5 premise correction) | Opus | Alone |
| H2 | hven `core/` multi-matrix continuation entry point + tests + docs | Opus *(escalate to Fable — see §7)* | Alone |
| H3 | SQP composite-key re-key (both sites) | **Fable** | Alone |
| G1 | Gate C package, dual review, SQP-instance execution review | Opus | Alone |

**Fable-hard, with why (3 firm, 2 borderline):**

- **S3** — carving a 6 900-line header into five while asserting content
  identity. Four independent silent-failure modes (definition order, inline
  linkage, circularity between the driver and restoration, FP identity across
  the move), and the failure mode is a behavior change that reads as a
  compile-fix.
- **U0** — the flag unification and its mass re-derivation event, which
  **inherits T0's Fable tier** (flag-unification re-plan, 2026-08-15). It is the
  phase's measurement-critical task: one commit that must move zero code while
  moving every flag-sensitive pin at once, then characterize a 57-cell
  old-vs-new delta well enough that a status regression is caught rather than
  absorbed as flag noise, across four classes of pin that are not mechanical.
  The failure mode is a mass re-derivation that quietly launders a real
  regression into the new bar.
- **H3** — the composite key. Correctness half is a wrong-slot-write class that
  a mutation already survived once; behavior half must move zero counters; the
  digest changes value and feed mechanics simultaneously; the proof is over reuse
  *decisions*, not hash values, on both backends.
- **T5 (borderline)** — the major-loop TU is mechanically simple but carries the
  highest bench risk in the tree (it currently inlines `eval_nlp`,
  `evaluate_kkt`, `build_subproblem`) and is the boundary most likely to be
  *reverted* under CLAUDE.md §5's outranking clause. Assign Opus; escalate if the
  first P-BENCH shows a regression, because "redraw the boundary" is the hard
  part, not "move the code."
- **H2 (borderline)** — Q3 enlarged it from "a new entry point" to "reproduce an
  exact feed order from iteration, cross-width, as a contract." Assign Opus;
  escalate if the compressed/uncompressed equality pin does not fall out of the
  first design.

**Everything else: Opus.** The relocation batches in particular are mechanical by
construction — that is the point of relocation-only commits, and P-SYM is what
makes "mechanical" a proof rather than a claim.

---

## 12. Conflicts raised at draft, resolved by the task review

Recorded here so the resolutions live in the plan rather than in a side file.
Full statements and evidence are in the review
(`.superpowers/sdd/2026-08-14-hven-m3-plan-revB/phase-c-plan-review.md` §2 and
§7). **Q1 and Q6b are now both closed** — Q1 by Grant's owner ruling as folded
here (§0, §6 U0), Q6b as RESOLVED-CODIFIED (CLAUDE.md §1). Their final chapters
are recorded in `2026-08-15-m3-phase-c-open-questions.md`.

| Q | Conflict | Resolution (task review, 2026-08-15) | Where it lands |
|---|---|---|---|
| **Q2** | Plan §2's `types.h → core/` row collides with the existing `core/types.h` and mis-homes the QP surface | Real but answerable; the row's own note pins the intent. Aliases → `core/types.h` at S1 with a `static_assert`; QP surface → `qp/qp_types.h` at R1. Reviewer **ratifies the letter**, does not re-derive | R1, S1, O7 |
| **Q3** | `hven::pattern_hash` throws on uncompressed input; the SSN key iterates *because* `QpProblem` imposes no compression | Real but answerable; the plan's resolution is "correct and complete". Iteration-based entry point (or sibling) with a contractually identical digest + an equality pin. A design constraint on H2, not a ruling | H1's finding, H2's scope, H2 tier note |
| **Q4** | Plan §5's "three hashes, one omitting `cols`" is stale | Real but answerable; factual correction. Two surviving sites, both mixing `cols`; the dissolved pair took the `cols`-omitting variant with it | H1 records the premise correction |
| **Q5** | "Census byte-identical on both backends" is impossible under the M3 record's own per-backend pins | Real but answerable; the per-backend no-op reading is "the only coherent one and it satisfies §5's proof obligation" — it is what §5 item 4's own parenthetical argues. Requires a pre-re-key Accelerate capture | H3's proof, O12's two-pass session |
| **Q6a** | Plan §6 clause 3 names a psiopt-envelope sweep vehicle that does not exist here | Real but answerable; it is an IPM-side instrument. `bench/`'s complete inventory is `bench_scale`, `bench_corpus`, `bench_f7_cold`, `bench_snopt_f7`, `ssn_safeguard_probe`, `tau_bar_sweep_probe` — no envelope sweep, and `bench_scale.cpp:485-493` marks the IPM envelope row as owed. Substitute the SQP vehicles that exist and say so in the gate package | P-BENCH's definition, Gate C item 6 |
