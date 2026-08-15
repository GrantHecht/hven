# Phase C — open escalations

**Revised 2026-08-15** after task review
(`.superpowers/sdd/2026-08-14-hven-m3-plan-revB/phase-c-plan-review.md`). The
draft raised six source conflicts. The review confirmed all six as real and
ruled four and a half of them answerable; those resolutions are now folded into
`phase-c-plan.md` §12 (with the tasks that carry them) and are **not** repeated
here.

**Status (2026-08-15): BOTH QUESTIONS CLOSED.** Q1 is CLOSED by Grant's owner
ruling on rider 1 (uniform flags), folded into the plan by the flag-unification
re-plan (2026-08-15). Q6b is **RESOLVED-CODIFIED** — Grant ruled codify, and
CLAUDE.md §1 now carries the second bounded exception. This file is now a
record, not a queue.

| # | Owner | Decision needed | Blocks |
|---|---|---|---|
| **Q1** | **CLOSED** (Grant's owner ruling, 2026-08-15) — uniform flags; the ratification's option-(a) regime rule is void and T0 is dissolved | None — the final chapter is recorded below | Nothing. The critical path is now U0 (plan §0, §6 U0) |
| **Q6b** | **RESOLVED-CODIFIED** (Grant, 2026-08-15) — codify | None — CLAUDE.md §1 carries it | Nothing |

---

## Q1 — **CLOSED (Grant's owner ruling, 2026-08-15).** Plan §6 clause 2 cannot execute under the flag regimes phase B declared

### Final chapter: Grant's owner ruling, and what it dissolved

The ratification's option-(a) ruling carried **rider 1**: Grant's governance
sign-off was required before T0's outcome became final, because a non-uniform
library flag regime is a project-shape call and the execution reviewer's ruling
was only the execution-review half. **Grant ruled on that rider, and ruled the
other way:**

- **Uniform library flags.** One regime for engine, tests, and bench.
- **TUs split cleanly for build efficiency, without sacrificing performance** —
  boundaries are drawn on code quality and build efficiency.
- **Code quality is not limited to preserve census bit-identity** against the
  existing baseline.
- **The runtime-neutrality bar is unchanged** — P-BENCH per boundary, and
  CLAUDE.md §5's outranking clause, both stand.

The execution half is the **flag-unification re-plan (2026-08-15)**,
reviewer-side note `docs/notes/2026-08-15-m3-flag-unification-replan.md`
(SIGNOFF FLAG-REPLAN-FINAL), folded into
`2026-08-15-m3-phase-c-plan.md` §0 (the ruling, the unify-first insight, the
revised sequencing, the census re-basing) and §6 U0 (the task: a flags-only
zero-code-motion commit plus its same-event mass re-derivation, with the full
evidence package, the four float-pin classes needing design attention, and the
five binding conditions).

**What this dissolved:** option (a)'s regime-assignment rule (void — there is
one regime); the gate-A ruling that harness TUs stay sandbox-matched
indefinitely (void); **T0 as a task, together with its pilot** — its deliverable
collapses into U0's design note and its Fable tier moves to U0's re-derivation
event; and O11's deferral (void — the `tests/sqp` CMake comment's deadline is
answered "yes, at U0," and the comment is deleted there).

**What survived unchanged:** the runtime-neutrality bar; the census-frequency
ruling's schedule and floor clause, which re-base onto the new baseline; and
every non-flag ruling in the ratification.

---

**Historical record below: the ratification's ruling, now superseded on its
flag half by the owner ruling above.**

**Ruling: option (a), with a stated regime-assignment rule; (b) stands as the
fallback (fires as a ruled deferral if Grant rejects (a)); (c) rejected for
M3.** Recorded here in full because this file is Q1's authoritative home; the
plan's §10 O4 row and T0 section point back here rather than repeating it.

**The regime-assignment rule (T0's design note codifies this):** a TU carrying
FP arithmetic on any asserted-value path compiles under the
**census-derivation regime** (sandbox-matched — the object-library or
per-source mechanism is the implementer's engineering choice); an
FP-arithmetic-free TU compiles under **library flags** with its FP-freedom
argument recorded at the CMake site — the `kkt_calls.cpp` precedent
generalized into the admission test, exactly as option (b)'s mechanism
describes. This keeps the non-uniform set minimal (only the FP-carrying TUs
deviate), keeps `kkt_calls.cpp` where it already is under its existing
argument, and gives every future TU a one-question classification with a
written answer. T1/T2 are FP-free → library flags; T3's NaN-branch caveat
(`-fno-finite-math-only` dependence) is exactly why its disassembly
verification is in its own commit — under library flags that verification is
**REQUIRED** (the library regime sets the flag, so the branch survives, but
"sets it today" is a premise, and the same commit must pin it); T4–T8 are
FP-carrying → census-derivation regime.

**Two riders:**

1. **Grant's governance sign-off is required before T0's outcome is final** — a
   non-uniform library flag regime is a project-shape call, already on the
   plan's Gate C reviews list; the execution reviewer's ruling is the
   execution-review half, not the owner half. **If Grant rejects (a), the
   fallback (b) fires as the ruled deferral.**
2. **The long-term path is stated so (c)'s rejection is not read as forever**:
   unification of the SQP engine onto library flags remains available post-M3
   as a *declared re-derivation* of every pin and baseline (the M6-or-Phase-8
   window, when a re-derivation buys something — e.g. `-march=native` on the
   engine — and can be priced). Rejected for M3 because it re-derives a
   just-accepted baseline for zero M3 benefit.

The pilot design (falsifiable, with the fallback firing automatically on
failure) is ratified, with one bar correction: the pilot's per-symbol P-SYM
identity is achievable for `FunnelStrategy::accept` specifically because it is
virtual-dispatched (already out-of-line); that is the pilot's bar, not the
general T-task bar (P-SYM scoped outside the declared blast radius — see plan
§1 and §6 T0/T9). State this in T0's note or T3 reads as a pilot failure.

---

**Historical record below (the conflict as escalated, now resolved by the
ruling above).**

**Owner: the execution reviewer, at ratification — before B1 and the R-batches
start.** Not at the gate: option (b) collapses the TU candidate set from eight to
three, so the answer changes what "phase C complete" means.

**Status:** the task review verified all three texts verbatim and confirmed the
conflict is "REAL AND BLOCKING", that "no source determines the resolution" (the
plan of record predates the flag declaration and contemplates *runtime* cost, not
*bit-identity* cost), that escalating to T0 is correct, and that deciding before
the R-batches is correct. It also endorsed the recommendation ordering below as
sound.

### The three authorities, in conflict

1. **Plan rev B §6 clause 2:** "driver-level orchestration (funnel/TR/SOC/
   elastic/restoration state machines, ingest, continuation) **moves to TUs**."
2. **`src/CMakeLists.txt:23-29`** — landed in phase B, declared to the gate-B
   reviewer, accepted at verdict §1.3 as making the boundary "reviewable at phase
   C rather than discovered": "**No SQP code that DOES carry floating-point
   arithmetic** (inertia comparisons, dense border/Schur assembly, any assembly
   helper, etc.) **may move into `src/sqp/` before phase C's per-boundary
   bit-identity proofs settle the library-flags-vs-engine-TUs question.**"
3. **`tests/sqp/CMakeLists.txt:10-25`** (and `bench/` likewise): the suite
   compiles under **plain per-config defaults**, deliberately, "because every
   pinned value the M3 gate asserts — the measurement pins in these tests, the
   57-cell walk census baseline, the two gate-battery artifacts — was derived
   under those flags," and whether it moves onto hven's unified `COMPILE_FLAGS`
   is "revisited **no later than the phase-C TU split, which compiles engine TUs
   into the library under library flags.**"

### The mechanics

The library regime is `-O3 -march=native -mtune=native -ffast-math
-fno-finite-math-only` plus an inline-threshold override
(`cmake/hven_compile_options.cmake`); the SQP test and bench regime is plain
`-O3 -DNDEBUG`. A function carrying FP arithmetic — `FunnelStrategy`'s
`pred_df >= kFunnelDelta * h_old * h_old`, the TR update, the elastic penalty
ladder, the restoration model — produces different arithmetic under `-ffast-math`
(reassociation, FMA contraction) than under plain `-O3`. Moving it from a header
compiled by `hven_sqp_tests` into a library TU compiled by `hven` **changes the
numbers the census asserts, by construction**. Plan §9 licenses no such delta;
plan §5 and §8 both require census byte-identity.

This is not only a phase-C problem: it is why `kkt_calls.cpp` needed a bespoke
FP-freedom argument to be the first SQP TU at all, and that comment explicitly
hands the general question to phase C.

### The options, and the endorsed resolution

| Option | Mechanism | Cost |
|---|---|---|
| **(a) Sandbox-matched flags for the SQP library TUs** | per-source `COMPILE_OPTIONS`, or an object library with its own regime linked into `hven` | The library is no longer uniform-flagged — a project-shape/governance call. Needs a stated rule for which regime a file is in and why, plus an install/export story. **Preserves bit-identity by construction** |
| **(b) TU-ize only FP-arithmetic-free code** | no CMake change; `kkt_calls.cpp`'s FP-freedom argument becomes the general admission test | Safe and cheap, delivers T1–T3 only. **Plan §6 clause 2 goes unexecuted** and must be explicitly deferred to M4/M5 with the reviewer's sign-off |
| **(c) Unify: the SQP suite moves to `COMPILE_FLAGS`, every pin re-derived** | delete the `tests/sqp` flags exception | Re-derives the census baseline, the four battery artifacts, and every measurement pin — a baseline the gate-B verdict accepted days ago |

**Recommendation, endorsed by the task review: (a), with (b) as the fallback;
(c) rejected for M3.** (a) is the only option that lets plan §6 clause 2 execute
as written while keeping census byte-identity a *proof* rather than a hope, and
its non-uniformity is already half-precedented — `tests/sqp` and `bench/` are
deliberately non-uniform today for exactly this reason.

**How the decision is discharged:** task T0 (Fable-tiered) writes the design note
and runs a falsifiable pilot — TU-ize `FunnelStrategy::accept`'s body under the
chosen regime and show P-SYM per-symbol identity against the header build plus
P-CENSUS unchanged. If the pilot cannot show identity, (a) is falsified and the
fallback fires.

---

## Q6b — **RESOLVED-CODIFIED (Grant, 2026-08-15).** Recording a second origin-naming exception in CLAUDE.md §1

**Ruling: codify.** Grant ruled with the reviewer's recommendation. **CLAUDE.md
§1 now states two bounded exceptions**: the two OLD-SEAM rig adapters (temporary,
deleted when the migrations close) and, permanent by construction, that frozen
baseline and evidence CSVs **retain their origin-naming provenance headers
verbatim** as pinned artifacts. The stated reason is the failure mode this
question named: a rule reading "without exception everywhere else" invites a
future cleanup pass to mutate a pinned artifact for cosmetic reasons, which is
exactly the silent pin break §7 exists to prevent.

Behaviour is unchanged either way — the frozen header was never touched, and no
phase-C task proposed touching it. What changed is that the next agent who greps
for origin strings finds a sanctioned answer instead of an apparent violation.
U0's re-derivation narrows the exception's future scope further: the new dated
baseline it produces carries an hven-native header, so the exception applies to
the frozen historical file and does not grow (plan §6 U0 (b) item 3).

**Historical record below (the question as escalated).**

**Owner: Grant.** Governance hygiene; blocks nothing. **The phase-C
ratification (2026-08-15) reiterates the recommendation below** (Verdict 3:
"my recommendation, for Grant's consideration: codify") while confirming the
decision remains Grant's and the frozen header stays untouched either way.

### Half of this is already answered and is *not* being asked

`bench/baselines/2026-08-06-corpus/walk_baseline.csv` line 1 is the frozen
provenance banner naming the origin project's corpus binary, and the rest of
the frozen header carries that project's binary and invocation strings.

**The header stays untouched.** That half is settled by precedent, not open:
gate A and gate B both relied on the frozen header (verdict §1.2 — "the file's
frozen provenance header … untouched. This is how a pin should change"), and
CLAUDE.md §7 forbids silent pin mutation. Mutating a pinned artifact to satisfy a
cosmetic naming rule is not on the table, and no phase-C task proposes it.

### What is genuinely Grant's

CLAUDE.md §1 grants **exactly one** bounded origin-naming exception (the two
OLD-SEAM rig adapters, deleted when the two engine migrations close) and states
that "the rule holds without exception everywhere else." A frozen baseline CSV
whose provenance header names the origin project is a **second** de facto
exception — narrower and more durable than the first, since the artifact is
immutable by pin discipline rather than temporary by migration schedule.

**The question:** codify it, or leave it uncodified?

- **Codify** — extend §1's bounded-exception text with a second clause: frozen
  provenance headers on pinned measurement artifacts record what ran, and are
  immutable by §7's pin discipline. Cost: one paragraph. Benefit: the next agent
  who greps for origin strings finds a sanctioned answer instead of what looks
  like a rule violation, and does not "fix" a pin.
- **Leave uncodified** — accept that the CSV header is a historical record rather
  than a statement about this repository's identity, and rely on the pin
  discipline alone to protect it.

**Recommendation: codify.** The failure mode of leaving it uncodified is
specific and plausible — a future cleanup pass mutating a pinned artifact for
cosmetic reasons is exactly the kind of silent pin break CLAUDE.md §7 exists to
prevent, and a rule that says "without exception everywhere else" invites it.
Either way, the phase-C plan's behaviour is identical: the header is not touched.
