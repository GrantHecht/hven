# M3 Gate C — review-request package

2026-08-21. Branch `m3`, tip `2a6a2ef` at packaging. Census-of-record
evidence is committed at `docs/notes/data/2026-08-21-m3-gate-c/`
(`walk_census_gateC.csv` with in-artifact protocol declaration,
`compare.txt`, `score_gates.txt`, `provenance_header.txt`). Review
reports, fix briefs/reports, and the parked-minors triage live in the
SDD workspace (`.superpowers/sdd/2026-08-14-hven-m3-plan-revB/`) — a
git-ignored working directory readable by the reviewing session; their
verdicts and dispositions are quoted in full in §5, so this note stands
alone if the workspace is ever swept (the gate-B lesson about dangling
evidence pointers, recorded rather than repeated).

## 1. What Gate C certifies

Phase C complete: the S-series restructures, the U0 flag-unification
event, the T-series TU splits (T1–T8 + T9 obligations), the H-series
hash re-key (H1 survey / H2 core entry point / H3 SQP re-key), S4b, the
maintenance batch, and the milestone-close review wave — every task
reviewed, every ruling folded, every proof leg green, and every
Critical/Important finding of the close-out reviews fixed on-branch and
re-reviewed to CLOSED (§5). Branch tip at packaging: `2a6a2ef`.

## 2. The census of record

The H3 post-re-key census (which is this gate's census of record) ran
at `fb45a00` — stamp `fb45a000c095`, launched 2026-08-20T15:12:49Z,
finished 21:11:35Z, parallel-tiered v1 (T1=44 6-wide pinned / T2=3
solo / T3=10 5-wide), `MKL_NUM_THREADS=1`, ALONE, protocol declared
in-artifact. **57/57 cells, zero mismatches on all 13 asserted columns
against BOTH arms:** the committed baseline
`bench/baselines/2026-08-16-u0-corpus/` (runner-wired compare,
serial-confirm list empty) and the preserved T678 pre-arm census run
at `8400424` (verified twice: independent column projection and the
comparison script). Bridge argument (owner-ruled 2026-08-20): the
commits between `8400424` and H3's base are H2 (digest preservation
proven — frozen literal + SQP objects byte-identical), the H2 fix
(no SQP object), S4b (P-SYM-accounted comments/strings), and a
scripts-only instrument fix, so no counter input changed across the
bridge; the two green compares confirm it. Score profile unchanged
(57 rows, 10 DNF worst-case-scored, 0 engine errors, 43 KKT-gated,
0 wrong-answer). Artifacts preserved at
`.superpowers/sdd/2026-08-14-hven-m3-plan-revB/preserved-census/build-h3-census/`.

## 3. Item 3a — the two split obligations (nine-set adjudication, 2026-08-19)

**(i-a) Reconstruction-authority ratification: DISCHARGED** at C0.4
(4/5/0 outcome, not-ratifiable vocabulary, provenance markings) — the
adjudication left every C0.4 disposition standing; only the "nine-trace"
title was withdrawn.

**(i-b) The tie-back-loss statement (owed by this package, statement
duty only):**

> The expected values for the nine reconstructed traces T1, T2, T3, T4,
> T4b, T8, P1, P4, and P6 (fixture producers `collocation_chain_kkt`,
> `hs76_kkt`, `barrier_chain_kkt`, `brutally_scaled_kkt`) were freshly
> derived from the old seams at M1 and are NOT cross-checked against the
> engines' own committed pins; the tie-back to those pinned numbers is
> absent for those nine traces, stated here at the gate as the M1 record
> requires rather than discovered here. T5(a), T6, T7, P2, P3, and P5
> are unaffected (coefficient-exact transcriptions or pure state
> assertions). Source of authority: the M1 Task-7 implementer report,
> spec-deviation (b) (tycho-side SDD workspace, M1 plan). Per the
> adjudication (reviewer-side `2026-08-19-nine-set-adjudication.md`,
> SIGNOFF NINE-SET-ADJUDICATION-FINAL), discharging this obligation
> means stating it; no new measurement obligation exists.

## 4. Rig state

Three-seam rig green (64/64 + P-series 6/6 at the maintenance batch);
psiopt arm pinned by consumed-tree hash `bfb7d30c…` per the rig-pin
ruling; Mac legs deferred with the owner (single future session, both
H3 arms capturable by checkout — pre `bed76b2`, post `fb45a00`; macOS
lane runs 32378993026 / 32384330753 both green as interim evidence;
Accelerate values UNOBSERVED).

## 5. Reviews — the milestone-close wave

**Dual review, plus an owner-directed extension.** Three review legs ran
over the branch (range `c6635e1..fb45a00`, 117 commits, reviewed as
current-state-plus-history-spot-checks):

- **Fable leg** (full report + triage:
  `final-review-fable-report.md` in the SDD workspace): verdict READY
  conditional on a fix batch. Dispositioned all 61 open parked Minors:
  3 MUST-FIX / 20 DEFER / 38 NO-ACTION.
- **Codex sol leg, part 1** (trimmed to the five correctness-critical
  surfaces after a first over-scoped attempt stalled): 0 Critical,
  3 Important — all three exception-safety holes in the border stack,
  all three CONFIRMED by Fable-side adversarial adjudication (which also
  retracted its own over-broad "every exception path detaches" claim).
- **Codex sol leg, part 2** (owner-directed, the complement surfaces:
  model/, warmstart+drivers, kkt assembly, core, test spot-checks):
  1 Critical + 5 Important — ALL SIX CONFIRMED by the same adversarial
  adjudication, with per-finding counter-neutrality arguments. The
  Critical (unvalidated model-callback returns at the primary user-facing
  boundary — the exact §4 "Eigen asserts must never be the only guard"
  class) is the one true Critical of the review cycle. Coverage
  statement: sol part 2 skipped its assigned surface 5 (`qp/` +
  `globalization/`) for depth — those surfaces carry per-task review
  coverage from phases B/C but no dedicated milestone-close deep read;
  recorded here as a stated gap, not silently.

**The fix wave (all findings above Minor, fixed on-branch):** 17 items
across two batches plus one re-review round —
`cf17c61` (FX-1..FX-8: border-stack exception safety — commit-last
`rebuild_k0`, reserve/rollback `add_border`, ledger reserve
`sync_borders`, the `evidence_usable()` guard — plus the CI artifact
condition, a stale comment, the `num_threads()` read-through contract
fix, and the register path-list sentence), `9e54c88` (doc follow-on),
`099cdda` (per-backend fixture split for the NaN-border test + register
entry M3-7: MKL LAPACKE NaN-screens where Accelerate demotes to
kExactlySingular), `4965fb6` (S-1..S-7: the callback-validation
Critical, fixed-at-infinity rejection, warm-start finiteness gating on
both routes, continuation counter invariant + endpoint honesty, assembly
box checks, and the drop-side rollback the FX implementer itself
surfaced), `2a6a2ef` (round 2: the S-5 arc-length revert restoring
success-path bit-identity, S-8 closing the same callback class at the
`upgrade_to_full` boundary, one scope revert). Every fix
throw-path/invalid-input-only; per-change ladder (P-SUITE both configs —
final count 1169/1169 — and per-commit P-SYM with pre-declared radii,
zero objects outside radius) in the two fix reports. **No census re-run
needed or performed: the census of record at `fb45a00` §2 stands, and
every subsequent commit's counter-neutrality is argued and proven at the
per-change ladder.**

**Scoped re-reviews:** round 1 (15 items) returned 14 PASS + 3 open
(S-5 neutrality, the S-8 gap it confirmed as its rider, one scope
violation); round 2 returned **CLOSED** with zero open items and zero
out-of-scope diff content.

**Parked-minors disposition:** the 3 MUST-FIX rows landed in the fix
wave; the 20 DEFER rows transfer to the carry doc; the 38 NO-ACTION
rows are closed with their reasons in `final-review-fable-report.md`.

[PENDING SLOT: tycho-lane LINEAR-BATCH-REVIEW-FINAL verdict — package sent
(rig pin `2c7564f` + B1 linear surface + H2 entry point), verdict
pending; the FX-4 `num_threads()` adapter change (`cf17c61`) is FLAGGED
to ride with that review — it moves the hven::linear surface and is the
one fix-wave item awaiting an external leg.]

**Lane-health record for this wave:** L-1 occurrences 8–10 folded into
the Linux-runner divergence register (`cd33962` + the gate-close fold),
including the tally's first two double-fail-then-green events — both on
test/docs-only commits, both topologically exonerated — and a mechanism
observation (exact counter pin on a documented backend-dependent tie)
routed to the post-M3 rate-quantification task.

## 6. Standing items handed past M3

Carry doc (`carry-doc-draft.md` → committed note at close): census as
gate instrument + per-change proof ladder; L-1 per-push rate
quantification task (now carrying occurrences 8–10, the two double-fails,
and the tie-pin fixture mechanism observation); NlpEval restructure
candidate; M3-6 re-open trigger at M5; M6 rig-arm horizon; seam-adapter
deletion schedule; the 20 DEFER rows from the parked-minors triage
(including the Fable leg's F-3 stale-iparm[6]-readback canary candidate,
F-4 backend validation divergence, F-5/F-7 comment/residual records);
sol part 2's surface-5 coverage gap (`qp/` + `globalization/` deep read);
the `stableNorm`-for-np>1 note from the S-5 round; and one item for the
OWNER: the Fable leg's F-6 flagged that CLAUDE.md §1's two-exception
wording does not name the register's disclosed pre-2026-08-14
origin-citation class — folding that class into §1's exception list is
the owner's call, flagged here rather than edited.

SIGNOFF SLOT: [GATE-C-REVIEW-FINAL by the SQP reviewer]
