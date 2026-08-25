# M4 Task 1 design note — the naming checkpoint, settled

**Date:** 2026-08-21. **Reviewer:** the hven lane, per the ratified
plan's ruling (c): the Task 1 design note is reviewed and settled
BEFORE the first Task 1 implementation commit. **Subject:** the
drafter-side Task 1 design note (Level 2 contract surface — names,
shapes, the joint design element), reviewed against hven m4
`8846607`. Ground checks performed in-tree: the `hven::solvers`
namespace claim (verified — `nlp_model.h:88`), the member-naming
convention (verified — the extracted engine's own `VariableBoundStage`
carries trailing underscores, so the note's struct style matches the
surface it wraps), and the engine's evaluation entry points (verified
— eight, `non_linear_program.h:822–849`, with genuinely distinct
output shapes).

## VERDICT: SETTLED. Task 1 implementation may start.

The name set is approved as proposed, all six §8 recommendations are
adopted (three with riders below), and the reviewer-raised question 7
is ruled with one design change. Everything below is binding on the
Task 1 implementation.

## Names — approved as proposed

`NlpAggregate` (beside `NlpModel`, never over it), `AggregateDeclaration`,
`VariableBound`, the `AggregatePiece` concept, `KktClaimSpace` /
`KktLocationTable` / `KktScatterView` and their `Rhs` twins,
`ClaimDomain` / `ClaimDomainSet`, `KktStorage`, `ModelStructureKey` +
the `model_structure_key()` free function, `StructureEpoch` /
`StructureEpochCounter`, `IdentityProbe` / `probe_identity`,
`AggregateCapability::{kNone, kDirectScatter, kValuesFastPath}`,
`evaluate_candidate_values` / `evaluate_candidate_first_order`,
`negotiate_partition_count`. Specifically endorsed: the
distinct-type rationale for `ModelStructureKey` (never a bare u64, and
`model_structure_key` not `structure_key` — the engine-side private
member of that name is exactly the collision the prefix prevents); the
deliberate non-collision of `evaluate_candidate_values` with the
Level-1 `eval_values` and its pinned wording; and the epoch semantics
of §4 in full — aggregate-owned counter, non-public bump,
release/acquire ordering, and the failure-restore rule (this is MF-4's
plan obligation, correctly carried into API semantics).

## The six open questions — ruled

1. **Claim-method renames:** adopted as recommended — rename at carve
   time, AFTER the as-is move is verified, as its own reviewable diff
   (this is a 2c-class step under the plan's Task 2 decomposition and
   carries its own proof).
2. **Candidate-evaluation re-entrancy:** adopted — non-`const`,
   concurrent use with an in-flight solve is a documented caller
   error. Riders: the posture is stated in the contract header itself
   (the linear surface's concurrency notes are the precedent), and the
   Task 9 gate's model-surface scorer holds its OWN aggregate rather
   than sharing the solve's.
3. **Digest over the declared claim stream in claim order:** adopted —
   it answers a layout-time question at layout time and keeps the
   model key structurally independent of the engine digest. Rider,
   stated so nobody "simplifies" it later: the ordered claim stream
   already varies with partition count, but the explicit
   partition-count conjunct of `ModelStructureKey` STAYS — the plan
   requires it as a named conjunct, and the redundancy is deliberate
   (an explicit field is inspectable where an order-sensitivity is
   not).
4. **Strong `StructureEpoch` type:** adopted. The linear layer's bare
   numeric epoch beside it is exactly why.
5. **Capabilities per aggregate, as the weakest claim over pieces:**
   adopted.
6. **Abstract base class for `NlpAggregate`:** adopted — the virtuals
   are per fan-out, never per element, matching the repository's
   TU/dispatch philosophy; the type-erasure seam stays one level down.

## Question 7 — the hot-path entry vs the eight legacy shapes: RULED

The finding is confirmed in-tree: the engine fans out through eight
entries (`eval_rhs`, `eval_ogc`, `eval_occ`, `eval_obj`, `eval_kkt`,
`eval_kkt_no`, `eval_soe`, `eval_aug`) whose output shapes genuinely
differ — bare objective value, value+gradient combinations, KKT
variants. A single `assemble(point, kkt, rhs)` cannot express them.

Ruling — one entry, but with a DISTINCT request type, not
`ClaimDomainSet`:

- `ClaimDomain` names what is CLAIMED at layout time; the hot-path
  selector names what is EVALUATED per call. These are different
  vocabularies (an objective VALUE is evaluable but claims no matrix
  structure), and conflating them would muddy exactly the R2.1
  statement `ClaimDomain` exists to make checkable. The entry is
  therefore `assemble(point, <request>, kkt, rhs)` with a separate
  request-flag enum (spelling left to implementation — `EvalRequest`
  or similar) whose members select outputs.
- The request enum must map the eight legacy shapes BIJECTIVELY: every
  legacy entry expressible as exactly one request set, and no request
  may compute anything its legacy counterpart did not — the Task 3
  retarget's counter-identity proof depends on call-for-call
  equivalence, and an entry that silently over-evaluates would move
  counters even when the numerics agree. The mapping table (legacy
  entry → request set) is a named deliverable of the Task 1
  implementation, and Task 3 proves against it.
- Empty/omitted scatter views are legal for arenas a request does not
  touch; scalar outputs (the bare objective value) get an explicit out
  slot rather than a repurposed view. No output is written that the
  request did not name.

## The SQP lane's design-review finding — adjudicated, adopted with a correction

The SQP lane's review (relayed with the drafting lane's concurrence)
found §2.4's "the serial-piece bridge declares NEITHER" wrong for
`kValuesFastPath`: blanket never-declare would make acceptance
criterion 4 unsatisfiable through the bridge and price the
probe-on-every-free-exit at a full evaluation, regressing against
today's direct model consumption. **CONFIRMED — but the proposed
remedy (declare conditionally, iff the model overrides `eval_values`)
rests on a wrong premise, and the correct fix is stronger and
simpler.** Verified in-tree at the opt-in's own contract text
(`nlp_model.h:130–160`): the Level-1 `eval_values` DEFAULT is not a
degradation — it computes f/cE/cI via the model's own value evaluators
"at EXACTLY the cost those three calls already were", touching no
derivative; an override exists ONLY to beat that baseline. So the
bridge's values path skips derivative work for EVERY `NlpModel`,
default or override, and:

- **The bridge declares `kValuesFastPath` UNCONDITIONALLY**,
  implemented over the Level-1 `eval_values` surface. Conditionality
  is unimplementable anyway — a virtual's override is not detectable,
  and adding a detection query to `NlpModel` would be a new
  implementer obligation, which §2 forbids.
- **`kDirectScatter` stays never-declared** by the bridge (the
  model-owned intermediate is intrinsic) — §2.4 was right about that
  half, and the "declared degradation" language now attaches to that
  flag alone.
- The bridge's `evaluate_candidate_values` MUST route through
  `eval_values` (never through a full evaluation), or the declaration
  above becomes false — state that in the bridge's contract text at
  Task 4.

## Question 7, extended — the SQP lane's two invariants, adopted as binding

Folded into the Q7 ruling verbatim: (a) claim LAYOUT is a function of
the declaration + the adopted partition count alone — evaluation-time
request masking never alters layout, digest, or epoch; (b) every legal
request subset carries the full path's determinism guarantee. Their
own consumption stays two-mode (full assemble + values-only) and the
entry-point set is not generalized on their account — the single-entry
ruling above already satisfies this.

## Boundary and order

§6 (validation ownership) satisfies the plan's §3.5 as written —
throws naming the piece with both numbers is the house rule. §7's
extraction-first order is a binding constraint this settlement
inherits, not just a preference: every type in the note is a bundle of
existing parameters, a published view, or new surface, and the
implementation must keep it that way — any change that would require
deleting or renaming an engine member IN the move step is a design
error to bring back here, not to absorb.

SIGNOFF: M4-TASK1-DESIGN-SETTLED
