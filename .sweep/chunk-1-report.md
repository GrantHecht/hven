# Chunk 1 report — comment sweep: include/hven/detail/globalization (incl. sqp/)

Branch `sweep/base` at `1e3b64e`. 28 files in scope, all touched. No builds, no
tests. Gate: comment-stripped diff of every file vs HEAD is EMPTY (code lines,
including intra-line whitespace and trailing alignment where inline comments
were removed, byte-identical).

## Files touched (comment lines before -> after; file lines before -> after)

| file | comments | total lines |
|------|----------|-------------|
| acceptance_strategy.h            | 150 ->  76 | 215 -> 140 |
| backtracking_line_search.h       | 112 ->  49 | 178 -> 113 |
| barrier_governor.h               | 168 ->  97 | 209 -> 137 |
| classic_adaptive_governor.h      | 112 ->  50 | 164 -> 101 |
| feasibility_stall.h              |  98 ->  58 | 139 ->  98 |
| feasibility_switch_recovery.h    | 113 ->  77 | 170 -> 133 |
| filter_acceptance.h              | 353 -> 152 | 462 -> 269 |
| funnel_acceptance.h              | 225 -> 113 | 279 -> 172 |
| globalization_mechanism.h        | 116 ->  57 | 174 -> 114 |
| inertia_regularization.h         |  99 ->  52 | 123 ->  76 |
| l1_restoration.h                 | 223 -> 111 | 380 -> 267 |
| merit_acceptance.h               |  84 ->  40 | 165 -> 122 |
| modern_merit.h                   | 214 -> 128 | 306 -> 222 |
| monitored_governor.h             | 218 -> 112 | 300 -> 195 |
| noop_recovery.h                  |  39 ->  18 |  69 ->  47 |
| progress_measures.h              |  51 ->  24 |  64 ->  36 |
| proximal_restoration.h           | 140 ->  58 | 198 -> 115 |
| recovery_chain.h                 | 155 ->  84 | 201 -> 129 |
| restoration.h                    | 164 -> 125 | 388 -> 349 |
| soc.h                            | 161 ->  89 | 242 -> 170 |
| solver_context.h                 | 126 ->  62 | 170 -> 106 |
| sqp/elastic.h                    | 217 -> 166 | 271 -> 219 |
| sqp/globalization.h              | 647 -> 362 | 733 -> 452 |
| sqp/restoration.h                |  91 ->  71 | 153 -> 132 |
| sqp/soc.h                        |  44 ->  38 |  63 ->  57 |
| sqp/trust_region.h               |  59 ->  45 |  73 ->  58 |
| switching_acceptance.h           | 188 ->  94 | 259 -> 169 |
| watchdog.h                       | 225 ->  99 | 407 -> 281 |
| **total**                        | **4592 -> 2507** | **6555 -> 4479** |

(Comment counts include `///` docstring lines; regenerated programmatically at the
fix round — the originally published table's after-values were wrong.)

## Rationale candidates for docs/notes (deleted from files per the rules)

- globalization_mechanism.h:4-10 — a trust-region mechanism was investigated as
  an alternative implementation of the interface and cut unbuilt; decision and
  reversal condition live at docs/dev/analysis/2026-07-e2-g7-tr-decision.md.
- restoration.h:8-19 — the third feasibility-restoration trio member (an
  elastic/penalty relaxation, LOQO/Knitro `bar_relaxcons` lineage) was
  deliberately cut after the nested-l1 evidence showed no remaining gap;
  decision + reversal condition at
  docs/dev/analysis/2026-07-e2-g6-implicit-tr-regularization.md §3.
- feasibility_stall.h:26-58 — constant-sizing corpus evidence: plateau-based
  dispatch A/B outcomes (episodes into quietly succeeding stages steered one
  corpus problem off an acceptable exit), the motivating stalled trace's
  numbers (equality-residual ∞-norm 1.106 → 2.113 across a 500-iteration burn;
  L1 read 2.602 at the earlier predicate's dispatch point), and why ceding that
  trace's behavior is the right trade.
- monitored_governor.h:14-45 — citation discipline note (sources fetched+read,
  pinned to Ipopt releases/3.14.19 = 2695946f..., per-citation line numbers)
  and the resolution record "an earlier planning memo cited 0.9998; the source
  ships 0.9999, which is used".
- modern_merit.h:85-98 — full paper-location bibliography block ("for the
  review tier"): WMNO Math. Program. 107:391-408 (2006) §3.1 equation map;
  Curtis & Nocedal IMA JNA 28(4):749-769 (2008) equation map.
- proximal_restoration.h / l1_restoration.h — pinned-source citation apparatus
  ([Uno] cvanaret/Uno 7481abe; [Ipopt] coin-or/Ipopt 72a29c9 with per-file
  pointers), replaced by option-name citations at the constants.
- sqp/elastic.h:166-171 — history of a dropped claim ("bounded below on a
  bounded box" removed in fix round 1 because tr_radius=+inf breaks it) and the
  TU-carve measurement provenance for set_elastic_penalty staying inline
  (phase-C T6/T4 precedent references).
- sqp/soc.h, sqp/restoration.h, sqp/trust_region.h — carve provenance
  ("phase-C S3 restructure", T6 definition-location notes, banner-measurement
  pointer); navigation retained in condensed form, labels dropped.
- sqp/globalization.h:128-196 (pre-sweep) — THE SIX-DIVERGENCE BLOCK ("DIVERGENCES
  FROM [TYC], EACH RESOLVED TOWARD [KLV]"): (1) width update Eq. (13) vs Uno's
  strategy-1 Eq. (14); (2) switching condition KLV Eq. (10) vs WB Eq. (19);
  (3) Armijo on f with sigma = 1e-4 vs WB Eq. (20) on the barrier objective;
  (4) theta_min/theta_max machinery not ported; (5) KLV Table 1 constants vs
  Uno defaults (beta contraction consequence); (6) no template-method
  scaffolding. Items (4)/(5) were subsequently restored into the file as
  callouts at the fix round; the full six-item text lives here.
- sqp/globalization.h:271-277,305-309 — kRestoreMinRejections re-derivation
  history ("re-derived in Task 9, value moved 3 -> 4"; fix-round-1 correction
  crediting argument (II) alone). The derivation itself ((I)/(II)/intersection)
  IS KEPT at the constant.
- watchdog.h:99-111 — provenance of the WatchdogState policy/plumbing split
  (mirrors soc.h's split rationale).

## Looked like a defect, reported not fixed

None found in this chunk. (The nearest thing: sqp/globalization.h's include set
is deliberately unpruned — <algorithm>/<cmath>/<fmt/format.h> now serve only
out-of-line definitions — but that is a disclosed bit-identity decision,
documented at the class, not a defect.)

## Docstrings written under uncertainty (reviewer should check these first)

Condensed from longer originals without (intended) change of stated behavior:

1. barrier_governor.h — update_barrier_monotone()'s Fiacco–McCormick gate
   wording and the provides_restoration_barrier_safeguard() scope-of-guarantee
   paragraph.
2. watchdog.h — ExtendedBacktrackRecovery's rung-carrying mechanics (seed with
   compute_step's returned alpha; returned-alpha × scale carried forward).
3. feasibility_switch_recovery.h — the soft-pre-stage notification-timing
   consequence (un-augmented filter during pre-stage; asymmetry direction vs
   Ipopt).


## Fix round (per .sweep/chunk-1-fix-brief.md; review at .sweep/chunk-1-review.md)

| finding | disposition |
|---------|-------------|
| 1 (blocking) filter ceiling formula | APPLIED — basval clamp restated exactly (1.0 unless \|phi_ref\| > 10) |
| 2 (blocking) governor re-entry "no tiny step" | APPLIED — condition removed from the engine claim; reference attribution made explicit |
| 3 should-fix "tagged below" + lost divergences | APPLIED — clause dropped; items (4)+(5) restored as condensed callouts in the SOURCES paragraph |
| 4 report: six-divergence block missing | APPLIED — added to rationale candidates |
| 5 report: table after-values wrong | APPLIED — table regenerated programmatically (comment counts now include /// lines) |
| 6 double @brief on GlobalizationStrategy | APPLIED — generalization note demoted to `//`; single @brief remains |
| 7 floating /// narration | APPLIED — sqp/globalization.h algorithm/parameters blocks and inertia_regularization.h caveats block demoted to `//` |
| 8 BarrierDecision.mu qualifier | APPLIED — `///<` restored with the free-mode caveat |
| 9 elastic contamination premise | APPLIED — "with z_j = 0 when s_j is off its bound" restored |
| 10 ownership sentences (filter/funnel/l1) | APPLIED — one ownership sentence each restored to the class docstrings |
| 11 trailing whitespace (20 lines) | NOT APPLIED here — byte-preserving gate artifact; belongs to a follow-up formatting pass (still code-token-neutral), as the review itself suggests |
| 12 missing @throws | APPLIED — filter_acceptance (both hooks), funnel_acceptance (both hooks), modern_merit (ctor + both hooks), sqp/globalization judge() |
| 13 unnamed @param current | APPLIED — folded into the descriptions |
| 14 l1_restoration docstring on wrong declaration | APPLIED — swapped; l1_elastic_slack_init has the @brief |
| 15 kKappaResto citation | APPLIED — Ipopt option name + default restored |
| 16 grouped constant docstrings mis-attached | APPLIED — monitored_governor options block and recovery_chain depths block demoted to `//` prose with explicit per-constant mapping |
| 17 update_barrier_monotone barr_obj undocumented | APPLIED — @param barr_obj added |
| 18 RejectionCause per-value mapping | APPLIED — one `///<` per enumerator |

Gate re-run after the fix round: comment-stripped diff vs HEAD is EMPTY on all
12 touched files.
