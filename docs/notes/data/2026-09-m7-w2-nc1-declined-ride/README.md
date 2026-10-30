# M7 W2 NC1 — the declined ride

Brief: `.superpowers/m7-w2-nc1-brief.md` (settler rulings N1–N10). Implementer report:
`.superpowers/m7-w2-nc1-report.md`. Base `82303972` (the R1 docs record; its sources are E2 fix 6's
`a53dbb88`). Origin: the R1 diagnosis (`../2026-09-m7-w2-r1-restoration-landing/REPORT.md` §3).

When the QP walk drops a constraint onto a negative-curvature face and the ride along that face
declines, the engine used to fall through to the ordinary capped step, which re-entered the
released bound at step 0 and ended the solve in zero-motion exhaustion (the four STEENBRB
restoration QPs, `Cs_STEENBRB_qp3`–`qp6`). NC1 closes that fall-through per decline reason:
a DEPENDENT ride blocker (reason iii, observed on all four dumps) takes the add site's exchange
rule and the ride is re-taken on the exchanged set; a `ride_sign` veto (ii), or a null-space veto
(i) whose step does not leave the released bound, UNDOES the drop instead of stepping. The rule is
stated in `include/hven/detail/qp/qp_engine.h` §4c; the counter `dep_ride_declines` is renamed
`dep_ride_blockers`, with three new counters (`ride_exchanges`, `ride_declines_fallthrough_blocked`,
`ride_sign_vetoes`).

## Fix rounds

| Round | Commit | What landed | Fixtures | Declared movers | Gates |
|---|---|---|---|---|---|
| NC1 | `5732a1f3` (cherry-pick of `232853d9`) | the declined-ride rule (iii exchange + re-take; ii/i undo), `src/qp/qp_engine.cpp`, header §4c; QpCounters: 1 renamed + 3 new fields; `QpWalkRideOutcome::kDependentBlocker`, `QpWalkAction::kRideUndone` (seam trace) | `QpE2WorkingSet.ADeclinedRideIsExchangedOnSteenbrbRestorationQp{3,4,5,6}` (the four dumps, `tests/qp/fixtures/e2-d4/Cs_STEENBRB_qp{3..6}.txt`: N3 → kOptimal at the option-off objective, both algebras), `QpE2WorkingSet.ADependentRideBlockerIsExchangedAndTheRideRetaken`, `QpE2WorkingSet.ANullSpaceVetoedRideNeverStepsBackIntoTheReleasedBound` (synthetic), the walk seam's post-drop test extended | `HsSweepCorpus.Corpus` total minors 3371 → 3365 (HS15 warm arm: three null-space-vetoed drops now undone; statuses unchanged); corpus kOptimal 8 → 12 (the four new dumps); census 120/120 byte-identical; U0 EXACT | `wrappers-gates/` (declared before the chain; `psym-derivation-nc1.txt`, `preflight.txt`) |

Registered alongside (N9): no reduced-curvature re-read on the null-space veto (i); the "impossible"
sign disagreement (ii) fires on noise-level multipliers (ACOPR57 qp6, SAWPATH qp1; outcomes identical
to base under the undo); STEENBRB_def qp1 border now ends N3 on a dependent ride blocker with no
admissible member; SAWPATH_def qp1's capped iterate is worse.
