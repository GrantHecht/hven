# M4 plan — hven-surface gate review verdict

**Date:** 2026-08-21. **Reviewer:** the hven lane (hven-surface gate,
per the M4 ownership ruling: the drafting lane drafts, the hven lane
reviews as a gate before implementation).
**Subject:** the M4 plan draft (drafter-side
`2026-08-21-m4-two-level-model-contract.md`), reviewed at its CURRENT
state — post the SQP plan-gate's A1/A2 amendments and riders (SQP
verdict: reviewer-side `91c4ec1`, SIGNOFF M4-PLAN-GATE-SQP-FINAL).
**Method:** dual-pass — the orchestrator's own review (base-state
spot-checks, constraint-slate audit, the three flagged judgment
points) plus an independent dispatched reviewer working read-only
against hven `72baafd`, whose findings were adversarially adjudicated
before anything below was adopted. The independent pass fact-checked
all 20 of the plan's §1 base-state claims against the tree
(20/20 verified, including the A1/A2 corrections re-verified at
source) and mapped all eleven hven constraints and the full SQP
R-series (reviewer-side `beff317`, SIGNOFF M4-SQP-REQUIREMENTS-FINAL)
into the plan text: **no constraint and no R-item is dropped,
weakened, or contradicted.**

## VERDICT: APPROVED WITH REQUIRED AMENDMENTS — the gate-C shape.

One docs-only amendment commit to the plan (four required items, six
minor folds); no task-structure change is required and no re-review
loop is owed — the hven lane verifies the amendment by inspection.
Upon that amendment landing, the hven-surface gate is PASSED and, with
the SQP plan-gate already passed, execution may begin at Task 0.

## Required amendments (all four; each is plan text, not code)

**RA-1 — own the `dep/hven` gitlink across the milestone, and apply
the M3 squash lesson to M4's own pin.** Tasks 6–8 build the consumer
project against Level 2, which exists only on hven's unmerged `m4`
branch until Task 9 — so mid-milestone gitlink bumps to m4-branch
commits are unavoidable and currently unowned, and a Task 9 squash
merge "mirroring M3" would orphan exactly the commits those bumps
point at (the M3 precedent the plan's own §1 records, repaired by the
`m3-branch-final` tag). Required: (a) mid-milestone bumps to m4-branch
commits ride consumer-project FEATURE branches only, never its main;
(b) §7 gains standing policy that hven pushes a preservation tag
(`m4-branch-final`) at the m4 tip before the close merge; (c) Task 9
states the close ORDER: hven PR merges → `dep/hven` re-points to the
hven main merge commit → the consumer-project PR merges. No fresh
clone may ever see a gitlink unreachable from hven main.

**RA-2 — scope R2.3 and name the builder of the split-claim scatter.**
As drafted, §3.2's "direct scatter into engine storage … no
intermediate copy on the per-minor hot path" is stated flat, but
(a) nothing at `72baafd` scatters split H/Ae/Ai claims into QP-engine
storage (the existing location tables target the assembled KKT value
array), Task 2b disclaims redesign, and no task owns BUILDING the new
machinery; (b) for the Level-1 bridge the flat property is unattainable
by construction — `NlpModel`'s evaluators return matrices by value,
and §2 (correctly) forbids new implementer obligations, so the bridge
intrinsically holds one model-owned intermediate. Required: state
R2.3's scope — it binds native Level 2 providers' fill path; the
bridge's model-return intermediate is a DECLARED property of the
bridge (mirror R2.4's declared-degradation shape) — and name the
owner: the scatter capability is defined at Task 1, the engine-side
binding is built at Task 5 under that task's design custody, and the
SQP lane's verification rider then applies to what Task 5 built.

**RA-3 — give acceptance criterion 4's layout-determinism pin (R2.2)
an owning test.** "Pinned across thread counts" implies a test; no
task's deliverable list names one. Required: add to Task 1's test
list a contract-level determinism pin — same model structural key ⇒
identical claim-space layout and assembly order across runs and across
evaluation-thread counts.

**RA-4 — close R5.1's divergence hole.** An implementer can satisfy
the letter of "W1–W5 scoreable at the model surface" by scoring the
bridge's underlying Level-1 model — which passes every M4 test while
never covering native Level 2 providers, the exact case R5.1 exists
for. Required: one sentence in Task 1 stating the mechanism — Level 2
exposes candidate-point aggregate evaluation into caller-owned split
storage (equivalently: admits an independent consumer) — and that the
model-surface KKT scorer consumes THAT surface, never the bridge's
underlying model. Note this facility and RA-2's scoping are two faces
of one design element; design them together.

## The three flagged judgment points — RULED

**(a) Task 2a's proof class:** P-SYM stands, and no downgrade path is
licensed. P-SYM's own definition is per-symbol INSTRUCTION identity
with a mechanically-classified noise class (`__FILE__`/`__LINE__`
movement) — it was built for relocations, so "if byte-identity is
unattainable" does not arise: the tool never demanded raw object-byte
identity. Evidence gathered at review: the moved TU contains zero
`__FILE__`/`assert` occurrences, and consumers' include-line retargets
leave their preprocessed token streams identical. Any 2a difference
outside P-SYM's licensed class is a FAILED MOVE to be redone, never a
proof-class downgrade. Two riders: (1) re-word 2a from "no edits" to
"no content edits — include-path/CMake retargeting only, enumerated in
the commit message" (the literal wording is unexecutable: the source
lists and four consumer include lines must change); (2) the M4 P-SYM
runs DECLARE their object set before compare, extended beyond the
standing SQP-centric set to cover the moved code's consumers
(`libhven.a` already covers the movers; the interior-side test objects
do not sit in the standing set and must be added or their exclusion
justified in the task report).

**(b) The §7 tree-token convention:** APPROVED with four amendments.
(1) Every grant and release is mirrored as one line in the M4
execution ledger regardless of channel health (the channel-failure
rider stays as the escalated form). (2) The owner preempts all tokens:
any hold is void while the owner is working in that tree, and a lane
receiving a grant checks `git status` before assuming a clean base.
(3) A session restart voids in-flight grants (peer identities change
on restart) — re-request, never resume a pre-restart hold. (4) Only
the current holder pushes to that repo's origin.

**(c) Deferred Level 2 naming:** ACCEPTED, with the implicit condition
made explicit: the Task 1 design note carrying the concrete names is
reviewed and settled by the hven lane BEFORE the first Task 1
implementation commit — a named checkpoint, not something folded
silently into the commit review.

## Minor folds (fold with the amendment commit; none blocks)

- **MF-1:** state the layout-band standard's numbers at Task 2
  (counters bit-identical unconditional; +0.5 % pooled / +1.5 %
  sub-second one-sided wall bands) rather than leaving them to §8's
  blanket incorporation.
- **MF-2:** Task 2a moves a TU listed in the PCH membership table;
  cite the membership bar (faster AND byte-identical, measured never
  predicted) at Task 2a/2c — re-measure or explicitly leave membership
  unchanged.
- **MF-3:** name the owner of converting the plan's hven-governing
  portion to hven's naming/citation convention before its `docs/`
  commit (the draft is saturated with identifiers hven's added lines
  may not carry).
- **MF-4:** Task 1's epoch spec/test enumerates the failure-restore
  path: a rejected reconfiguration that re-lays structures is a
  structural event and bumps the epoch under the same ordering
  guarantee (the in-tree `bounds_revision_` pair's failure handling is
  precedent).
- **MF-5:** give acceptance criterion 1 a named check (an
  include-graph or symbol-use sweep recorded in the gate note) so
  "consume Level 2 and only Level 2" is inspectable, not asserted.
- **MF-6:** the census legs name their baseline of record per artifact
  (the U0 corpus baseline and the gate-C census artifact are the
  standing ones).

## What was verified beyond the findings

The §2 architecture mapping satisfies hven constraint 1 exactly as
demanded (existing model surface extended, nothing displaced, the
mapping stated explicitly); the eval_values ruling is carried verbatim
at its point of force; the A1 rider's HotState posture (existing
handle and ingest pins preserved; mint-epoch wiring an SQP-custody
option; further evolution M5) is internally consistent with §6 and
with the code at `qp_engine.h:2247`; Task 3's renegotiation-posture
rider matches the code (IPM factorization reuse keyed at factorization
scope, no cross-structural-event hot state); the epoch-ordering
guarantee is implementable inside the current synchronous
renegotiation paths; task ordering 0 → {1,2} → {3,4} → 5 → 6 → 7 →
8 → 9 is sound with RA-1 the only dependency gap.

**Upon the amendment commit landing (verified by inspection, no
re-review loop), the hven-surface gate is PASSED.** Execution then
begins at Task 0, with the amended §7 conventions in force from the
first tree-token request.

SIGNOFF: M4-PLAN-GATE-HVEN-FINAL

---

## Addendum (2026-08-21, same day) — fold inspection: GATE PASSED

The drafting lane's amendment fold was inspected against the current
plan text, item by item: RA-1 (the §7 gitlink-ownership bullet with
feature-branch-only bumps and the standing `m4-branch-final` tag
policy; Task 9's explicit close order with the no-unreachable-gitlink
invariant and the owner's merge authority on both closing PRs), RA-2
(the §3.2 scope block — native providers bound, the bridge's
intermediate DECLARED mirroring R2.4, capability defined at Task 1,
engine-side binding built at Task 5, one-design-element note), RA-3
(the R2.2 layout-determinism pin in Task 1's test list,
cross-referenced as acceptance criterion 4's owning test), RA-4 (the
candidate-point aggregate-evaluation surface in Task 1 with the
scorer bound to it), rulings (a) — Task 2a re-worded with the
failed-move-not-downgrade language and the declared, extended P-SYM
object set — (b) — all four token-convention amendments in §7, ledger
at `docs/notes/2026-08-m4-ledger.md` (path accepted) — and (c) — the
design-note checkpoint before the first Task 1 implementation commit —
and MF-1 through MF-6 at their named locations. All folded faithfully;
no residue.

**The hven-surface gate is PASSED.** With the SQP plan-gate already
passed, the plan is ratified; execution begins at Task 0. The
hven-governing portion's naming-converted `docs/` commit follows under
MF-3's ownership (drafter converts, hven lane verifies at commit
inspection), exercising the §7 tree-token convention on its first real
use.

SIGNOFF: M4-PLAN-GATE-HVEN-PASSED
