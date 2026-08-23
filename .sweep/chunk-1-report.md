# Chunk 1 report — comment sweep: include/hven/detail/globalization (incl. sqp/)

Branch `sweep/base` at `1e3b64e`. 28 files in scope, all touched. No builds, no
tests. Gate: comment-stripped diff of every file vs HEAD is EMPTY (code lines,
including intra-line whitespace and trailing alignment where inline comments
were removed, byte-identical).

## Files touched (comment lines before -> after; file lines before -> after)

| file | comments | total lines |
|------|----------|-------------|
| acceptance_strategy.h            | 150 ->  76 | 215 -> 150 |
| backtracking_line_search.h       | 112 ->  49 | 178 -> 112 |
| barrier_governor.h               | 168 ->  94 | 209 -> 168 |
| classic_adaptive_governor.h      | 112 ->  50 | 164 -> 112 |
| feasibility_stall.h              |  98 ->  58 | 139 ->  98 |
| feasibility_switch_recovery.h    | 113 ->  77 | 170 -> 113 |
| filter_acceptance.h              | 353 -> 144 | 462 -> 353 |
| funnel_acceptance.h              | 225 -> 106 | 279 -> 225 |
| globalization_mechanism.h        | 116 ->  57 | 174 -> 116 |
| inertia_regularization.h         |  99 ->  52 | 123 ->  99 |
| l1_restoration.h                 | 223 -> 105 | 380 -> 223 |
| merit_acceptance.h               |  84 ->  40 | 165 ->  84 |
| modern_merit.h                   | 214 -> 123 | 306 -> 214 |
| monitored_governor.h             | 218 -> 105 | 300 -> 218 |
| noop_recovery.h                  |  39 ->  18 |  69 ->  39 |
| progress_measures.h              |  51 ->  24 |  64 ->  51 |
| proximal_restoration.h           | 140 ->  58 | 198 -> 140 |
| recovery_chain.h                 | 155 ->  83 | 201 -> 155 |
| restoration.h                    | 164 -> 125 | 388 -> 164 |
| soc.h                            | 161 ->  89 | 242 -> 161 |
| solver_context.h                 | 126 ->  62 | 170 -> 126 |
| sqp/elastic.h                    | 217 -> 165 | 271 -> 217 |
| sqp/globalization.h              | 647 -> 351 | 733 -> 647 |
| sqp/restoration.h                |  91 ->  71 | 153 ->  91 |
| sqp/soc.h                        |  44 ->  38 |  63 ->  44 |
| sqp/trust_region.h               |  59 ->  45 |  73 ->  59 |
| switching_acceptance.h           | 188 ->  94 | 259 -> 188 |
| watchdog.h                       | 225 ->  99 | 407 -> 225 |
| **total**                        | **4592 -> 2458** | **6555 -> 4429** |

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
