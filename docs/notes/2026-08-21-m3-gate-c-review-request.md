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

**Stronger than the claim above, per the execution reviewer's own
independent full-width comparison** (reviewer-side
`2026-08-20-m3-gate-c-review.md`, SIGNOFF GATE-C-REVIEW-FINAL): against
BOTH arms the ONLY differing column is `wall_s` (47 cells, unasserted by
design) — **all 36 non-wall columns are byte-identical, `kkt_residual`
included** — so the U0 residual watch holds unweakened through the
H-series, and the fix wave's no-census-re-run posture rests on a census
that was clean at full width, not merely on the 13-column projection this
package asserted.

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

**The external leg — LANDED (slot filled 2026-08-21).** The tycho-lane
pass on the package sent 2026-08-20 (rig pin `2c7564f` + B1 linear surface
+ H2 entry point) returned **APPROVED WITH ONE CHANGE**, one Important
(R1) and one Minor (M1), no re-review of the folds owed (reviewer-side
`2026-08-20-hven-linear-batch-review.md`, SIGNOFF
LINEAR-BATCH-REVIEW-FINAL). R1 — the rig's escape-hatch provenance must
report the tree it OBSERVED rather than the tree it was pinned to —
landed at `2508b6a` with the falsehood demonstrated live against a scratch
drift clone before the fix and gone after; M1 (three overstated hash
sentences) rode along in the same commit, `pattern_hash.cpp.o`
byte-identical. The FX-4 `num_threads()` adapter change (`cf17c61`) was
flagged to ride with that review as the one fix-wave item awaiting an
external leg; it got its own scoped verdict — **APPROVED, no findings**
(same reviewer-side file, addendum of 2026-08-20, SIGNOFF
LINEAR-BATCH-ADDENDUM-FX4-FINAL), which closes the C2 slot for that
surface.

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

## 7. Package amendment per GATE-C-REVIEW-FINAL C1

Added 2026-08-21, docs-only, after the execution reviewer's verdict
(reviewer-side `2026-08-20-m3-gate-c-review.md`, SIGNOFF
GATE-C-REVIEW-FINAL: **CONDITIONAL PASS**, conditions C1 and C2). C1 is
five things the plan's §8 checklist requires the *package* to state and
this package did not; none is a substance gap — the evidence existed for
all five and is cited here. Nothing below is new work, and nothing below
changes a verdict, a count, or an artifact. C2 is discharged by the
external verdict now folded into §5.

### 7.1 The phase-C behavioral-delta ledger (checklist item 11)

In the plan of record's **§9 shape** — the shape
`docs/retarget-design-sqp.md` §11 mirrors for phase B — at **four rows**:
the four pre-registered in `docs/notes/2026-08-15-m3-phase-c-plan.md` §8
item 11, and **nothing else**. Phase B's ledger grew 4 → 6 → 8 under
review; phase C's did not grow. No task review and no gate review raised
an old-vs-new difference outside these four, and the one candidate that
might have — S4b's three throw strings — was declared in the plan before
the task ran and belongs to row 3's class.

| # | Row | Status, and the evidence that discharges it |
| --- | --- | --- |
| 1 | **The two SQP-side structure-hash VALUES change under the H-series re-key.** `detail::structural_hash(qp)` becomes `hven::combined_pattern_hash(H, Ae, Ai)`; `SsnEngine::structure_hash` becomes a composite key = (structural digest, bound-row conjunct) | IMPLEMENTED at `fb45a00` (H3), on H2's entry point (`b617a9d` + `7ec864c`). **Licensed** by the plan of record's §9 row 3 (conditional on H1), carried into the phase-C plan's §9 row 15 as "hash values may change **iff** no pin asserts a value", and by H1's ruling (reviewer-side `2026-08-19-m3-h1-ruling.md`, SIGNOFF H1-RULING-FINAL), which adopted the survey's reading that the one value pin found — the single-matrix digest literal at `test_pattern_hash.cpp:130` — is a **constraint H2 satisfies, not a re-derivation**. That literal stands unmodified. **Nothing outside the two digests moved:** `values_hash` untouched; the per-SSN-major `hven::pattern_hash` count unmoved (hash-count probe identical cell-for-cell on the four pure-SSN cells: 81/34, 91/39, 89/38, 71/29); census 57/57 clean; P-WSB counter-identical (§7.4). **Reuse DECISIONS are preserved**, which is what the row actually owes, and each ingredient has its own fixture: `SsnEngineLocal.PatternKeySeparatesBoundLayoutsOfEqualSize` pins the correctness-critical `br.var` half, and `SsnEngineLocal.SignFlipAtConstantBoundLayoutForcesRebuild` — new at H3 — pins the conservative-by-record `br.sign` half, which the old in-hash design recorded as knowingly unkillable |
| 2 | **Include paths and header names** (mechanical) | IMPLEMENTED across the R-series relocations (`69adf40` R1, `debe50b` R2, `7942a39` R3, `e0ac68f` R4, `e001528` R5, `f4e1f2a` R6 — leaf types/model/ledger to their public homes, the KKT tier to `detail/kkt/`, the QP tier, the warm-start tier, globalization, the driver, and `detail/sqp` retired) and the T-series TU splits (`eb6c011` T1+T2, `8c7d5d1` + `530f154` T3, `e5a3ed8` T4, `de97f5d` T5, `0a76fe0` T6, `a4db5f0` T7, `8400424` T8), each with its own per-boundary P-SYM/P-CENSUS/P-BENCH/P-BUILD record. No counter, status, float or census cell moves on this row anywhere |
| 3 | **Backend-error / throw MESSAGE TEXT where the licensed sweeps touched it** (precedent: `docs/retarget-design-sqp.md` §11 row 4) | IMPLEMENTED, two sites of origin, both proven unpinned. **R2's fold (`bdd7b83`)** rewrote the two `std::runtime_error` messages in `schur_complement.h` (`solve()` and `expected_neg_eigs_delta()`) off `LAPACKE_dsytrf` vocabulary and onto `DenseSymmetricFactor`'s own published outcome (`kExactlySingular`) — a grep of `tests/` and `bench/` returns nothing for either message, and P-SYM resolved every affected object into exactly two immediates of +10 each, character-counted against the edited literals (140→150, 171→181). **S4b (`2f9185f`)** carried three throw-message strings in `qp_engine.h` through the deferred reference sweep: each names the header its sentinel convention is documented in, and the R-series rename moved that header (`types.h` → `qp_types.h`), so the message text moved with it. That is the class the plan declared non-byte-identical **in advance**, when it added the S4b row; S4b re-verified at base that no test in the tree matches on any of the three fragments, and P-SYM accounted the twelve differing objects as exactly the licensed strings plus one format-length immediate (122 → 125, re-derived as 77+45 → 77+48). Same row, same license, stated here rather than promoted to a fifth row |
| 4 | **The flag regime** — *re-registered under the flag-unification re-plan (2026-08-15), replacing the A3 row that was conditional on T0 landing option (a)*: "the library, its tests, and its bench are unified onto one flag regime at U0; the flag-sensitive pins, battery artifacts, and the 57-cell census baseline are re-derived in that same declared event, against a new dated baseline, with the old baseline frozen as history" | IMPLEMENTED as one declared event: `bf8e897` (flags, no code motion) + `39a27c9` (the re-derived baseline of record, `bench/baselines/2026-08-16-u0-corpus/`), design note `docs/notes/2026-08-16-m3-u0-design.md`. Two full 57-cell reproductions, byte-agreed on every asserted column; the pre-U0 baseline stays frozen at `bench/baselines/2026-08-06-corpus/` and is still read by the frozen-vs-frozen continuity test (§7.2, `39a27c9` row). One `kPins` row moved (F3stress warm+pred), inside the event and in the delta report. The owner summary preceded the new pins becoming the bar, per plan §6 U0 (b) item 6 |

**Explicitly NOT licensed, and none occurred:** any SSN rebuild-count
change (the composite key exists to prevent exactly that — the probe above
is its falsifier); any counter, status, float or census delta outside U0's
declared event; any halting-kind status flip, at all. The fix wave's every
change is throw-path/invalid-input-only, argued and measured per commit
(§5).

### 7.2 Item 4 — the count arithmetic, enumerated

**Two units, and they must not be mixed.** *Registered* is what `ctest -N`
lists. The `out of N` in ctest's summary is *registered − the two
permanently `DISABLED_` cases* (`EqpRefinementAb.FootprintRuleProbe`,
`EqpRefinementAb.FullBattery`); two further cases
(`FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas`,
`JetMklGuard.RestoresThreadDefaultOnExit`) are **Skipped by design** and
sit inside that N as not-failed. The gap is a constant **2** across the
whole phase. Gate B's entry baseline **1150 is the registered figure**
(= 1148 as a ctest denominator); the **1169/1169** quoted in §5 is the
denominator figure (= 1171 registered). Same net movement either way:
**+21**.

**The gating figure is the SNOPT-enabled one** (phase-C plan §9 row 17,
from plan §8 item 2), matched
in **Release AND Debug** at every step below.

| Commit | What it adds or retires | Δ | Registered after |
| --- | --- | --- | --- |
| `6535566` | **gate-B entry baseline** (1118 +7 +5 +18 +4 −2, gate-B note §2.2) | — | **1150** |
| `0802fc4` | C0.1 — the `iparm[17]` `BackendDefaultPremise` canary | +1 | 1151 |
| `8628f04` | C0.4 — the constraint-side seam fixture: **one** gtest case plus **one** compile-fail probe (`constraint_unsupported_mixin`, registered by `tests/interior/CMakeLists.txt`'s adapter-probe `add_test` loop, which is why the delta is 2 and only one `TEST` appears in the diff) | +2 | 1153 |
| `6419d06` | S2 fix round 1 — the `core/` layering regression net, two cases | +2 | 1155 |
| `39a27c9` | **U0's retirement/conversion**: the frozen-artifact live-comparison test is retired *as a live comparison* and converted to a frozen-vs-frozen continuity test (`CorpusBaseline.TheReSweptWalkArmIsCounterIdenticalToTheCommittedBaseline` → `…ToTheFrozenPreU0Baseline`, repointed at the new baseline with the pre-U0 path kept behind its own define) | −1 +1 = **0** | 1155 |
| R1–R6, S1–S4b, T1–T8, `2c7564f`, `33adcfa`, `b68324e` | relocations, TU splits, sweeps, maintenance. **T3's NaN-branch pin (`8c7d5d1`) is on this line**: it pins existing driver assertions against a build-flag-dependent branch by two-compile differential, and registers no new case | 0 | 1155 |
| `b617a9d` | H2 — the multi-matrix continuation entry point, six `PatternHash` cases | +6 | 1161 |
| `7ec864c` | H2 fix round 1 — the gapped-structure independent-reference pin | +1 | 1162 |
| `fb45a00` | H3 — `SsnEngineLocal.SignFlipAtConstantBoundLayoutForcesRebuild` | +1 | 1163 |
| `cf17c61` | fix wave, FX batch — the non-finite-border state pin and the adopted-engine thread-count pin | +2 | 1165 |
| `099cdda` | FX-1/FX-7b — the non-finite-border fixture split **per backend**: one case renamed, its two backend arms `#if defined(__APPLE__)` inside one body, so it is one registration on either platform | −1 +1 = **0** | 1165 |
| `4965fb6` | fix wave, S-1..S-7 — **six findings get regression coverage; five of them as new cases** (see the reconciliation below) | +5 | 1170 |
| `2a6a2ef` | fix wave round 2 — S-5's case renamed by the arc-length revert (−1 +1), S-8 adds `SqpDriverContract.MisSizedUpgradeReturnsAreRejectedByName` | +1 | **1171** |

`1150 + 1 + 2 + 2 + 0 + 0 + 6 + 1 + 1 + 2 + 0 + 5 + 1 = 1171` registered,
**= 1169 as ctest's denominator, both configs.** Exact, no unexplained
test. Every delta above was re-derived from the commits themselves, and a
sweep of every commit in `6535566..2a6a2ef` finds no other commit that
adds or removes a registration.

**The one place a stated test-add count and the recorded total disagree,
reconciled.** `4965fb6`'s commit body and report both say "six new
regression tests, one per finding except S-7", and the registered count
moves +5. Both are true: **S-2's coverage landed as new assertions plus
two controls INSIDE the existing case
`NLPAdapterCoreTest.RejectsBadSizesAndStructures`** — the same case that
has rejected the constraint-row twin of that shape since it was written —
so it registers nothing. Six findings covered, five registrations. (The
other two apparent gaps are the rename/convert rows above: a converted
test and two renamed ones move a name, not a count.)

**The no-SNOPT figure, recorded beside the gating one: 1167 registered /
1165 run.** The whole difference is the four `SnoptBridgeGate` cases —
first recorded as an absolute during S1 (1153 / 1149, with
`ctest -N | grep -c SnoptBridgeGate` = 4 in the SNOPT tree and 0 in the
no-SNOPT one) and unchanged since. Independently witnessed three times by
the `linux-clang-release` CI lane, which configures without SNOPT: 1149 at
S2b against a local 1153, `1 of 1159` at `099cdda` against a local 1163,
and **1164/1165 at `2508b6a`** against 1169 — the most recent (run
32438752595, attempt 1), its single failure being L-1 occurrence 11
(`docs/notes/2026-08-15-linux-runner-divergence-register.md`; single-fail
then green, exonerated on the diff). No test registration moves between
`2a6a2ef` and `2508b6a`.

### 7.3 Item 6 — the two statements this package owed

**(a) B1's O(nnz) hash-cost measurement is DISCHARGED, and its decision
rule FIRED.** Evidence: `docs/notes/data/2026-08-15-m3-b1-hash-cost/`
(report, the eight corpus CSVs, the A/B analysis, the compile-flags diff,
the measurement patches), landed at `1f9868d` + `26e5a24`. **One added
O(nnz) pattern-hash pass costs 6.12 – 7.76 % of SSN-major wall-clock,
median 7.34 %**, on the SSN-heavy families — `bench_corpus` F3/F7,
n = 750…20000, ~424 s per pass, 4 reps per arm, plus
`ssn_safeguard_probe census 20000` (517 301 majors), 7 reps per arm.
Against the plan's pre-stated **5 %** bar the rule fired and B2 became
**gate-blocking**. B2 discharged it: `ff461ad` (engine) + `ad8acc1`
(evidence, `docs/notes/data/2026-08-15-m3-b2-hash-removal/`) + `5c72df8`
(review fix) thread the analysis decision through `factorize_checked`,
taking the per-SSN-major `hven::pattern_hash` calls **3/3/5 → 2/3/3**,
scaffold readback 3.00 → 2.10–2.17 per factorization, **wall recovery
median 7.45 %** against B1's predicted 7.34 %, with the production engine
reproducing the measurement-patch arm to within **0.23 %** and no counter
moving anywhere the change is observable. H3 then held that count: the
hash-count probe is identical cell-for-cell to its pre-arm (row 1 above).
Per CLAUDE.md §7, every wall figure quoted here is single-threaded
(`MKL_NUM_THREADS=1`), strictly one solve at a time, machine otherwise
idle, as both artifacts' provenance blocks state.

**(b) The Q6a substitution.** Plan §6 clause 3 names an IPM-envelope sweep
as a P-BENCH vehicle. That vehicle is an IPM-side instrument and **does
not exist in this repository**: `bench/`'s complete inventory is
`bench_scale`, `bench_corpus`, `bench_f7_cold`, `bench_snopt_f7`,
`ssn_safeguard_probe` and `tau_bar_sweep_probe`, and `bench_scale.cpp`
marks the IPM envelope row as owed. **P-BENCH's SQP vehicles substitute
for it throughout phase C**, and this sentence is the package saying so,
as the phase-C plan's §12 Q6a resolution directs.

### 7.4 Item 5 — the non-census legs, cited at the gate

**P-WSB counter-identity at H3.** Captured on both sides of the re-key and
identical. Pre-arm at H3's base `bed76b2`, Release, serialized,
`MKL_NUM_THREADS=1`: `WarmStartBattery` 3/3, `ScaleF7Slow` 6/6,
`bench_scale --self-check` PASSED, and six committed baseline CSVs
re-scored `--from-csv --score-gates`, all rc 0 (the 2026-08-06 walk
baseline, the 2026-08-08 walk-reswept and SSN battery, the 2026-08-09 SSN
and walk resweeps, and the 2026-08-16 U0 walk baseline). Post-arm from the
clean census build — **stamp `fb45a000c095`, the very build that produced
the census of record in §2**: `WarmStartBattery` 3/3, `ScaleF7Slow` 6/6,
six re-scores rc 0. This is the reuse-decision-level no-op evidence row 1
of the ledger rests on.

**`bench_scale --self-check`.** Its most recent gate-relevant run is that
same post-arm run from the `fb45a000c095` census build, where it **PASSED
with counter output identical to the pre-arm's**. What it covers: it
ignores every other flag and runs one fixed, hard-coded scenario — F3,
n = 1000, warm arm, full-step ON — and checks its counters against
`tests/sqp/test_warm_start_battery.cpp`'s `kPins["F3n1000"].arm[kArmWarm]`
row. It is the cross-check that the bench vehicle and the suite agree on
the same cell, which is why it is run beside every battery capture (U0
ran it twice at the repointed tree, byte-identical to the old-arm run).

**The `kPins` rows are inside the 1169 both-config count.** `kPins` in
`tests/sqp/test_warm_start_battery.cpp` is **six families × five arms = 30
pinned rows** (F1, F2, F2far, F3n50, F3n1000, F3stress × cold, warm,
warm+pred, hot, cold@pred), each row pinning twelve counters. They are
asserted by `check_pinned_corpus_totals()` from the registered gtest case
`WarmStartBattery.Corpus`, with the F7 scale cells asserted by
`ScaleF7Slow` — ordinary ctest-registered cases, run in Release and Debug
alike, and therefore already counted inside the 1169/1169 of §5 rather
than being a separate leg anyone has to run by hand.

### 7.5 The governance change inside the review range

**`24aef78`** — `docs(governance): retire the blocking iparm human-review
gate (owner ruling, reiterated 2026-08-20)` — sits inside the reviewed
range and edits CLAUDE.md §6, so it alters a standing rule this package is
otherwise measured against. Named here so the package does not silently
span it. It is the owner's third statement of the same ruling; the policy
it leaves standing is label (`IPARM-SURFACE`) plus validation evidence
plus the tycho-lane review when one is in scope, with owner spot-checks at
their discretion and no merge gate. Every `iparm`-surface change in this
milestone carries that label and its evidence.

SIGNOFF SLOT — **FILLED**: **GATE-C-REVIEW-FINAL**, by the SQP reviewer,
**CONDITIONAL PASS (the gate-B shape)**, dated 2026-08-20 and relayed by
the owner (reviewer-side
`2026-08-20-m3-gate-c-review.md`). Condition **C1** — this five-item
package amendment, docs-only, verified by inspection with no re-review
loop — is discharged by §7 above. Condition **C2** — the external linear
lane — is discharged by the verdict and its FX-4 addendum folded into §5.
