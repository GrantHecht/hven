# M7 W2 NC2 (THE FLAT FACE) — the implementer's hand-back, placed here by the settler.
# Copied VERBATIM from `.superpowers/m7-w2-nc2-report.md` (implementer commit `ce3963b0`, cherry-picked to `71f54c50`).
# Nothing below this line is the settler's text; the settler's own record of the round is `README.md` in this directory.

# M7 W2 NC2 — report

SIGNOFF: M7-W2-NC2-REPORT

Commit: **`ce3963b0`**, one squashed commit on `dc8665e3`, no attribution trailer, tree clean.

Implementer hand-back, 2026-09-26. Worktree `/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9`,
branch `worktree-agent-a69f46e11796560b9`, base `dc8665e3`. Continuation of the first
implementer's round (WIP snapshot `afd7651c`, squashed away). Brief:
`.superpowers/m7-w2-nc2-brief.md` (M1–M9). Every claim is labelled OBSERVED (a transcript on
this box) or INFERRED (reasoning from one).

## 0. What shipped, in one paragraph

**THE FLAT FACE shipped in full. THE EQUALITY DRIFT shipped as its checked, counted invariant
with NO re-projection. THE SADDLE EXIT did not ship.** The other two were implemented, measured,
and taken back out because each broke fixtures the engine's own correctness rests on: the drift
repair takes the feasible census from 120/120 to 116/120 and re-certifies a feasible elastic copy
as infeasible; the saddle exit moves eleven SQP-driver/HS fixtures, four of which have a QP
failure as their PREMISE and therefore need redesigning, not re-pinning. Both are registered in
§9 with their full measurements so the settler can commission them as their own rounds. The
landed round is small and clean: **corpus 12/51 → 15/51 kOptimal, seven movers all in the
flat-face family, Release suite 2862/2862 with the census at 120/120 and no HS re-pin.**

## 1. M1 — established first (no notes existed on disk; re-derived here)

Source: the predecessor's env-gated stderr probes in a **base-source** Release build (tag `base`),
transcripts at `.scratch/nc2/probes/base/*.trace`, classified with `.scratch/nc2/tools/analyze.py`.
OBSERVED that these are base-source traces: every `NC2 END` line's `status`/`minors` equals the
corresponding cell of the independently-run base corpus (`.scratch/nc2/corpus/base-border-*.txt`),
on all 15 dumps. Verdict codes are `0=kOk, 1=kSuspect, 2=kWrong`.

### 1a. `Cs_STEENBRB` qp1–qp6 — the flat face (all OBSERVED)

| dump | drops | gates | vetoes | undos | END |
|---|---|---|---|---|---|
| qp1 | 49 | 49 | 1 | 1 | num 219 |
| qp2 | 65 | 64 | 5 | 5 | num 251 |
| qp3 | 2620 | 2619 | 1312 | 1312 | cap 6600 |
| qp4 | 69 | 69 | 18 | 18 | num 250 |
| qp5 | 2185 | 2185 | 23 | 23 | cap 6600 |
| qp6 | 2455 | 2453 | 816 | 816 | cap 6600 |

- **Which gate fired:** section 4c's curvature gate, on EVERY post-drop pass, at a quotient of
  **exactly 0.0** against a **finite** `curvature_tol` — and here R2 §0's premise is WRONG:
  `hven::solvers::detail::hessian_scale` has floored at 1 since M3 (`git log -S "FLOORED AT 1"`
  → `72baafd4`), so on an LP `curvature_tol` is 1e-12, never 0. The floor M2 asks to "state and
  derive in the header" was already stated and derived there. Nothing about it needed changing.
- **Which refusal:** `0` of (i) (`nsok=1` on every gate), **`0` of (ii)-flat**, and **every one of
  the 2175 vetoes is (ii)-DISAGREE** — the measured slope and the predicted one have OPPOSITE
  SIGNS, so the descending sign of the measured slope re-enters the released constraint and the
  feasibility half vetoes. R2 §0's "`ride_sign` reads the slope as flat" is also wrong: the
  measured slope clears its tolerance by three to four orders of magnitude.
- **z_s, leaving rate, slope read, EQP step norm** at the last vetoed gate of each:
  - qp1 it=217: `zs=-8.0995e-08`, rate `dd=-2.0993e+01`, measured `slope=+2.9216e-05`,
    `slope_tol=6.6063e-10`, predicted `pred=-1.7003e-06`, `pnorm=4.7677e+01`. TERM at it=218 with
    `exempt_worst=8.0995e-08` → the zero-motion-exhaustion exit, status num.
  - qp3 it=6598: `zs=-7.7215e-08`, `dd=-2.9669e+00`, `slope=+9.8263e-08`, `slope_tol=2.6367e-10`,
    `pred=-2.2909e-07`, `pnorm=1.9029e+01`.
  - qp6 it=35: `zs=+4.6124e-08`, `dd=+2.0716e+01`, `slope=-2.6284e-06`, `slope_tol=2.0414e-09`,
    `pred=+9.5551e-07`, `pnorm=1.4732e+02`.
- INFERRED, and the round's central finding: `|measured − predicted|` is up to **3 orders above
  `slope_tol`**, so the measured slope is not merely noisy, it is DOMINATED by the point's own
  stationarity residual ρ (`gᵀp = z_s·a_sᵀp + ρᵀp` exactly, every retained row being orthogonal to
  p). At these multiplier magnitudes (1e-7…1e-8, R2's ω‖∇f‖ scale) ρᵀp is the bigger term, and
  the sign it produces is not information about f's behaviour along the ray.

### 1b. `A0_YORKNET` qp3/qp4 — the equality drift (all OBSERVED)

The pass at which `|Ae x − be|` first exceeds 10·feas_tol having been below it, and what the walk
did there (`.scratch/nc2/tools/drift.py`):

- **qp3, pass 177.** `NC2 EXCH it=177 out=pin 290 in=pin 95 alpha=4.396e-04` then
  `NC2 STEP it=177 kind=2 idx=95 alpha=4.396e-04 dx=1.034e-02 eq=3.125e-01` — the residual goes
  from `3.755e-14` to `3.125e-01` in one step. It is **not** a clamped re-add: the PRESTEP line
  reports `|Ae·sdir| = |Ae·eqp.x − be|`, and `eq_after ≈ alpha × that`, i.e. the step moved α of
  the way toward an EQP answer that missed its own equality rows by ≈7.1e2. The exchange site then
  cycles (period 5/7) and `eq` oscillates between `2.544e-04` and `3.128e-01` for the remaining
  7500 passes.
- **qp4, pass 193.** `EXCH out=pin 290 in=pin 212 alpha=2.734e-03`, `STEP kind=2 idx=212
  dx=2.067e-03 eq=6.250e-02` (from `3.689e-14`); later profile alternates `4.676e-14` / `5.172e+00`.
- The two gates immediately before are the 1/δ-sized regularized steps R2 described: qp3 it=176
  `curv=-3.9e-51 pnorm=1.43e+08 slope=-1.03e+08 zs=-4.37e+01 pred=-1.03e+08`; qp4 it=190/192
  `curv=-2.84e-02 pnorm=2.75e+02 slope=+2.14e+03 pred=+2.14e+03`. Here measured and predicted
  AGREE to all printed digits — the multipliers are O(10), not O(1e-8).

### 1c. `Hj_STEENBRD` qp1/qp4/qp9 — the saddle exit (all OBSERVED)

- qp1: 97 drops, 23 gates (20 stepped on `curv<0`, 2 on `curv>0`, 1 dependent blocker), 0 undos,
  54 exchanges; **TERM it=616 `verdict=2` (kWrong) with `exempt_worst=0`** → no drop candidate, the
  terminal trusted-kWrong exit, status num 617. The face was reached by an ADD.
- qp9: 55 drops, 21 gates (14 stepped, 7 dependent blockers), 152 exchanges; **TERM it=305
  `verdict=2` (kWrong), `exempt_worst=0`**, num 306.
- qp4: 82 drops, 19 gates, 61 exchanges; three consecutive gates it=207/208/209 with the SAME
  `curv=-1.2290e-06`, `slope=+5.5675e+01`, `pred=+5.5675e+01`, each a **dependent blocker**
  (outcome 3) — the exchange cycle; **TERM it=282 `verdict=0` (kOk) with `exempt_worst=4.5000e-01`**,
  i.e. the released pin re-entered drop-exempt and the zero-motion rule ended the solve (num 283).
  So qp4 is NOT a trusted-kWrong terminal case; M4's reading of it ("a drop onto a face the inertia
  test reads kSuspect") is not what the base walk records at its exit.

## 2. THE FLAT FACE, as landed

`src/qp/qp_engine.cpp`, `QpEngine::ride_sign`, and `include/hven/detail/qp/qp_engine.h` §4c. The
header text, verbatim:

```
//    THE FLAT FACE (independent_working_set). On an LP (H == 0) every post-drop
//    quotient is exactly 0 -- hessian_scale floors at 1, so curvature_tol is
//    1e-12 and not 0, and the gate fires on every drop: the direction is a RAY
//    and the ride is the LP's own answer. What stopped it was ride_sign. At a
//    KKT point of an INDEPENDENT working set the slope along p is known exactly
//    from the drop's own multiplier,
//        s = violation * dropped_directional(p),
//    which is g'p in exact arithmetic (the Dantzig violation is -z_s in the
//    engine's sign convention, so the product is z_s a_s'p, and every retained
//    row is orthogonal to p). The MEASURED g'p is that plus rho'p, rho being the
//    point's own stationarity residual, and on the R2 restoration LPs rho'p is
//    the larger term: measured slopes of 2.0e-7 against predicted -5.2e-8, the
//    two DISAGREEING in sign, which is what (ii)'s feasibility half then vetoed
//    -- 1312 undone drops on one 6600-minor solve.
//    RULE: when |s| resolves above ride_sign's slope tolerance, s decides the
//    sign (ride_predicted_signs); only when it does not does the measured slope
//    decide, and a flat s leaves (ii)'s veto and its undo exactly as they were.
//    The predicted sign is the LEAVING sign by construction (s < 0 iff p leaves
//    the released constraint), so (ii)'s feasibility half cannot veto it.
//    A drop undone on ride_sign's veto is marked kPtNoise: its multiplier is
//    below the resolution of the slope it was supposed to predict, so it does
//    not count toward zero-motion exhaustion at this point.
//
//    NOT the literal slope FLOOR: "take s when the measured g'p is below
//    tolerance" cannot fire in double precision. The tolerance is
//    kEngineDenomTol * ||g|| * ||p|| = 1e-12 ||g|| ||p||, about 4500 times the
//    cancellation error of the dot product, so whenever |s| clears it the
//    measured slope clears it too (it would take ||g||_1 / ||g||_2 > 1e4, i.e.
//    n > 1e8, for both to hold). Measured: 0 firings over 51 dumps. Precedence
//    is the same intent made reachable.
```

**DECLARED DEVIATION from M2.** M2 words the rule as a floor: "`ride_sign` uses the predicted
slope's sign WHEN |gᵀp| is below tolerance and |s| is above it". Landed as PRECEDENCE: whenever
|s| resolves, it decides. Reasons, both measured: (a) the literal floor fires **0 times on 51
dumps** — the drafted form was in the WIP and `ride_predicted_signs` was 0 on every cell — and
INFERRED it cannot fire at all in double precision, by the ‖·‖₁/‖·‖₂ argument in the header;
(b) M1 shows the veto that actually stops the LP walk is the sign-DISAGREEMENT one, and the
disagreeing term is exactly the residual the predicted slope does not carry. M2's own sentence
"the drop passed `opt_tol` so |s| is NOT noise even when the measured gᵀp is below the current
slope tolerance" is the intent; precedence is that intent where it is reachable. The flat-s veto
and its undo (NC1's (ii)) are untouched, so a genuinely noise-level multiplier still undoes its
drop and is still counted.

Also landed, as M2 requires: the ray is ridden to its first blocker by NC1's existing machinery
(independent ⇒ step, dependent ⇒ exchange along the ray, none ⇒ `kUnbounded`), and the
1/δ-sized EQP saddle step is never taken on a flat face (unchanged from NC1). `kPtNoise` keeps an
undone noise drop out of `exempt_worst`, so a point whose only remaining candidates are
noise-level no longer exits through zero-motion exhaustion.

Counters added: `ride_predicted_signs`, `eq_drift_violations` (`include/hven/core/solver_counters.h`).

Two properties worth stating because they are what keep the rule narrow. A ZERO-MULTIPLIER PROBE
drop fills a fresh `DropRecord` (`src/qp/qp_engine.cpp` ~:2948), so its `violation` is 0, the
predicted slope does not resolve, and the branch cannot fire there — which is right, since that
drop's multiplier IS zero and §4c is documented vacuous for it. And with
`independent_working_set` off, `s_pred` is hard-zero, so the function is the base one token for
token.

## 3. THE EQUALITY DRIFT, as landed — and the decision

Landed as the CHECKED, COUNTED invariant at the head of every pass, with **no re-projection**.
Header text, verbatim:

```
//    THE EQUALITY DRIFT (independent_working_set). Measured cause: a regularized
//    EQP answer can miss its OWN equality rows by 1/delta-sized amounts, and a
//    ratio-test step of alpha toward it leaves the iterate alpha * that far off
//    rows it had already met (alpha = 4.4e-4 toward an answer 7.1e2 off the rows
//    put YORKNET's restoration QP 3.1e-1 off them at pass 177, and the exchange
//    cycle that followed never came back). The iterate's invariant -- no equality
//    row carries a STRUCTURAL violation, the terminal classifier's own test
//    (violation_is_structural, row by row, on this pass's multipliers) -- is
//    therefore CHECKED at every pass once the walk has met the rows (a start
//    point need not), and a miss is COUNTED (eq_drift_violations). Stated in the
//    classifier's currency and not one digit tighter: a residual section 5 would
//    not call a violation is not a drift either. NOTHING re-projects the iterate:
//    see the registered items in the NC2 report for the correction that was
//    implemented and measured (it closes the rows through the pass's own factor,
//    converts one more restoration QP, and takes the feasible census from 120 to
//    116 because the corrections the regularization dominates are 1/delta-sized).
```

**THE DECISION, and why (this is the item my dispatch asked to settle).** The dispatch said: keep
the WIP's checked-invariant form only if the measured drift on YORKNET qp3/qp4 vanishes under the
flat-face rule; otherwise implement M3's re-projection. The drift does NOT vanish. I therefore
implemented the re-projection — three successive forms, each measured on the full 51-dump corpus
and on the Release suite:

| form | corpus kOptimal | what it cost |
|---|---|---|
| (1) correction through the pass's factor, closed-or-nothing, pass re-taken | 16/51 | `ACOPR57_def` qp0/qp1/qp4 optimal → capped (f 432.66 → 1310.8 with a 5.2e-1 wrong-signed multiplier; −18.395 → +67.7); YORKNET qp3 repaired at 6997 of 7680 passes |
| (2) + a size bound (‖dx‖∞ ≤ the motion since the rows last held) | 15/51 | same three ACOPR57 cells, unchanged |
| (3) + the invariant restated in the classifier's currency (the landed threshold) | 18/51 | ACOPR57 restored to base exactly; **census 120 → 116**; `T6bAFeasibleElasticCopyIsNoLongerCertifiedInfeasible` fails (a feasible elastic copy certified infeasible, worst_resid 0.65); `E2Fix4TheTerminalMuPointIsRefinedBeforeAnInfeasibleVerdict` fails; `Continuation.ProbeBudgetBoundsAFailingProposal`, `SqpDriverDisprovedEntry`×3, `ScaleF7.WideWindowWalkIsChurnNotDegeneracy` move; `STEENBRB_def/qp3` ends with f = NaN |

Form (3) is the best of them and it still buys **one** extra conversion (`A0_YORKNET/qp1`, with 2
adopted repairs) at the price of the feasible census and a no-false-infeasible fixture. A rule
that re-certifies a feasible problem as infeasible is not shippable, so the re-projection does not
ship. OBSERVED, with the bisect that attributes it: `.scratch/nc2/logs/bisect-*.log` — with all
three NC2 rules off, the 19 tests that failed under the full draft pass 20/20; with only the drift
repair off, 11 fail (all of them the saddle exit's); with only the predicted sign off, 19 fail
(the flat face causes none of them).

INFERRED diagnosis to hand on: the corrections that damage a cell are the ones the regularization
dominates — on a face where `Z'HZ` is singular along the rows, `K⁻¹[0; r]` is 1/δ-sized, closes
the rows, and lands somewhere unrelated; the size bound catches some but not all of those, because
the walk's own motion since the rows last held can be large too.

## 4. THE SADDLE EXIT — measured, not shipped

Implemented (the WIP's draft, kept and corrected): a Lanczos search on the face's inverse reduced
Hessian through the pass's own factor (`detail::face_solve`, `detail::face_negative_curvature`),
accepted only VERIFIED (`d'Hd ≤ -curvature_tol·d'd`, `A_W d ≈ 0`), oriented by descent, ridden
uncapped by NC1's blocker rules (`QpEngine::ride_saddle`). Papers only, as M8 requires: Forsgren,
Gill & Murray 1991; Gill, Murray, Saunders & Wright 1991; Gill & Wong 2015. No solver source read.

**What it buys (OBSERVED, corpus tag `w6`/`w8`):** `Hj_STEENBRD` qp1/qp4/qp9 all convert
(num → opt, at first-order points whose final face reads kOk: f = −18.8865098462 border /
−19.6365098462 refactorize on qp1, −9.21780323474 on qp4, −7640.46819091 on qp9 — two different
local minimizers on qp1, no global oracle), and with the drift repair `A0_YORKNET/qp5` too. That
is +3 to +4 conversions.

**What it costs (OBSERVED, `.scratch/nc2/logs/bisect-nosaddle.log`):** eleven Release tests —
`HsBattery.BorderMode`, `HsBattery.RefactorizeMode`,
`HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions`,
`HsSweepCorpus.Corpus` (total minors 3371 → 3379),
`IpqpDispatch.KIpmAndKWalkAgreeOnTheAnswerAcrossTheHsBattery`,
`QpEngineIndefinite.PostProbeRestartIsOneShotPerSolve` (the walk no longer stops at the fixture's
saddle: it rides 1.5 off it and returns kOptimal — arguably the rule working, but M6 says that
file stays unchanged), `SqpDriverQpFailure`×3, `SqpDriverSoc.SocIsSkippedOnRoutedFailures`,
`SqpDriverSsnMode.EveryHsProblemTheWalkCertifiesIsCertifiedUnderKSsn`.

INFERRED: four of those (the three `SqpDriverQpFailure` fixtures and `SqpDriverSoc`) have a QP
`kNumericalError` as their PREMISE — the saddle exit removes the failure they provoke, so they need
new provoking problems, not new pinned numbers. That is a fixture-redesign round with its own
brief, not something to land inside this one. Registered in §9.

Also drafted by the first implementer and taken out for being outside M4: a saddle exit on EVERY
pass whose verdict is kWrong and whose EQP step would not strictly descend. It converts
`A0_YORKNET/qp5` and moves six more E2/NC1 fixtures' premises (`ACOPR57`, STEENBRB elastic,
`Cs_STEENBRB` qp3/qp4). Registered.

The same applies to the WIP's **FACE CHECK** (extra refinement of every EQP answer's constraint
rows, in `eqp_solve.h` and `bordered_eqp.h`): measured alone (corpus tag `face`), it converts
`A0_YORKNET` qp1/qp3/qp4 and it moves CONVEX faces, which M5 forbids — `ACOPP57_def` qp0–qp3 lose
1.1e-3 of relative objective and `ACOPR57_def/qp4` goes from optimal at −18.395 to capped at
+34.04. Reverted to base; registered.

## 5. M5 — what did not change (checked against the diff)

- The option-OFF path: `ride_sign`'s predicted slope is computed only under
  `opts_.independent_working_set` and is 0 otherwise, so with the option off the function is
  token-for-token the base one; the drift invariant's block is guarded by the same flag. OBSERVED:
  the corpus's option-off legs were not re-run this round (the predecessor's `baseoff` legs are the
  only off transcripts on disk), but the code path is unreachable — see §10.
- Convex faces: nothing in the diff touches the ordinary step, the ratio test, the drop selection,
  or the add site. The only convex-face-visible change in the whole diff is `ride_sign`'s choice of
  SIGN, which is reached only on a post-drop pass whose quotient is at or below `curvature_tol` —
  i.e. never on a positive-curvature face. OBSERVED: the 36-dump corpus moves in exactly one cell
  outside the flat-face family (`ACOPR57_def/qp6`, already capped, f in the 9th significant digit).
- `kStepped`/`kUnbounded` outcomes, rule (iii), and the undo for a true noise-level drop: unchanged
  (the undo now also marks `kPtNoise`, which only affects `exempt_worst`).

## 6. Fixtures

`tests/qp/test_qp_flat_face_saddle.cpp` (new, in `hven_qp_walk_seam_tests`; `tests/CMakeLists.txt`
one line). 8 tests, all OBSERVED green in Release AND Debug:

| test | what it pins |
|---|---|
| `QpNc2FlatFace.AWrongSignedDropOnAnLpFaceRidesTheRayToItsBlocker` | synthetic 3-var LP, wrong-signed drop on a flat face: `kOptimal` at f = 0, x₂ = 1, `curvature == 0.0` against `curvature_tol == 1e-12`, the ray ridden to its blocker, `ride_predicted_signs ≥ 1`, 0 undos, 0 vetoes, both algebras |
| `QpNc2FlatFace.ANoiseLevelDropIsUndoneAndCountedNotRidden` | synthetic 3-var LP, z₁ = −2e-9 against ‖g‖ = 1e4: neither slope resolves, so the drop is undone and counted (`ride_sign_vetoes == 1`, `ride_declines_fallthrough_blocked == 1`, `ride_predicted_signs == 0`, `zero_motion_exhausted == 0`, the vertex kept); at −2e-8 the same QP rides the ray to x₁ = 1 |
| `…TheSteenbrbRestorationLpQp{2,3,4}TakesTheMultipliersSign{Refactorize,Border}` | the three R2 restoration LPs, one test per (dump, algebra): the rule FIRES (`ride_predicted_signs ≥ 1`), the base failure mode is GONE (`zero_motion_exhausted == 0`), and an answer the walk calls `kOptimal` passes the full E2-D4 KKT battery and sits within 2e-7 relative of the Clarabel optimum |

**Why the dump fixtures do not pin the conversion.** It is configuration-dependent, and that is
OBSERVED, not feared. Release, both algebras identical (`.scratch/nc2/logs/seam-rel-v2.log`):
qp2 `kOptimal` 248 minors f = −192.000440284, qp3 `kOptimal` 294 minors f = −384.000285512, qp4
`kOptimal` 238 minors f = −192.000148701, every one with `predicted_signs` 55–80 and
`vetoes=0 undos=0 zmexh=0`. Debug, both algebras identical (`.scratch/nc2/logs/seam-dbg-v2.log`):
qp4 converges (256 minors, f = −192.000152406, 3.2e-8 of the oracle) while qp2 and qp3 run to the
6600-minor cap — but NOT because the walk goes wrong: Debug qp2 ends at f = −192.000439386, **2.9e-8
relative from the oracle, CLOSER than the Release answer**, with `predicted_signs = 1640`,
`vetoes = 2`, `undos = 2`; Debug qp3 at f = −384.000293757, 5.7e-8 of the oracle, with
`predicted_signs = 2182` and no veto at all. The walk makes the same progress in both; it is the
certification that does not arrive inside the budget. These LPs
decide on multipliers of 1e-7…1e-8 and the two configurations round differently (-O0 against -O3
under SAFER_FAST), so which side of `opt_tol` a given pass lands on is not a cross-configuration
property. The conversion is this report's Release measurement; the fixture pins what holds in both.

Fixture dumps added: `tests/qp/fixtures/e2-d4/R2_Cs_STEENBRB_qp{2,3,4}.txt`, copied byte-for-byte
from `docs/notes/data/2026-09-m7-w2-r2-composite-landing/dumps/Cs_STEENBRB/`.

**Not fixtured, and why.** M6 asks for `Cs_STEENBRB` qp1–qp6, `A0_YORKNET` qp1–qp6 and
`Hj_STEENBRD` qp1/qp4/qp9. Under the landed rule set only Cs qp2/qp3/qp4 reach kOptimal: Cs
qp5/qp6 still cap (now with **0 undos and 0 vetoes** — they make monotone progress and run out of
budget, f within 1e-8 of the capped base value), Cs qp1 caps (§7), and the YORKNET and Hj families
need the two unshipped rules. Their dumps are not left in `tests/qp/fixtures` as unused files; they
remain in the R2 evidence directory where they were.

Debug cost (M6's < 600 s per fixture): OBSERVED in the full Debug ctest — refactorize 0.9 s
(converging) to 22 s (capped), border 304 s (capped). Splitting one test per (dump, algebra) is
what keeps the border cases inside the limit.

`tests/sqp/test_qp_engine_indefinite.cpp` is UNCHANGED, and so is every other existing test file —
no correctness fixture was re-pinned.

## 7. Corpus, census, U0, HS, suites

**Corpus** (`.scratch/nc2/corpus`, `dump_replay` built against the installed Release library,
`MKL_NUM_THREADS=1`, `taskset -c 2`, one solver at a time under `flock /tmp/box-build.lock`; the
36-dump E2-D4 corpus + the 15 R2 dumps, E2 on, both algebras):

- **kOptimal 12/51 → 15/51**, identical under `border` and `refactorize`. NC1's reference was
  12/36+15; the three conversions are `Cs_STEENBRB/qp2`, `Cs_STEENBRB/qp3`, `Cs_STEENBRB/qp4`.
- Every mover, both algebras (`.scratch/nc2/tools/fcmp.py base final`):

| dump | base → final | cause |
|---|---|---|
| `Cs_STEENBRB/qp2` | num 251 → **opt 248**, f −192.000443022 → −192.000440284 | the rule |
| `Cs_STEENBRB/qp3` | cap 6600 → **opt 294**, f −384.000271723 → −384.000285512 | the rule |
| `Cs_STEENBRB/qp4` | num 250 → **opt 238**, f −192.000150665 → −192.000148701 | the rule |
| `Cs_STEENBRB/qp5` | cap 6600 → cap 6600, f −192.000031467 → −192.000029848 | the rule (undos 23 → 0, `predsign=2178`) |
| `Cs_STEENBRB/qp6` | cap 6600 → cap 6600, f −192.000007685 → −192.000009553 | the rule (undos 816 → 0, `predsign=2181`) |
| `Cs_STEENBRB/qp1` | **num 219 → cap 6600**, f −1378.22297564 → −1378.22294737 | THE FLAT FACE: base ended through zero-motion exhaustion on an exempt 8.1e-8 multiplier left by its one veto; under the rule that veto does not happen (OBSERVED `undo=0 signv=0 zmexh=0 predsign=2176`), so the exhaustion exit is never reached and the walk rides to the budget. Neither outcome is kOptimal; f is unchanged to 8 digits (2.1e-8 relative) and the stationarity residual improves 1.0 → 6.7e-7. Its Clarabel gap is 2.2e-4 absolute, the largest of the six, so this cell needs more than the sign rule |
| `ACOPR57_def/qp6` | cap 7100 → cap 7100, f 2.40026120591e-09 → −3.37349905115e-09 | the rule on a capped cell; 9th-digit f (5.8e-9 relative), sign residual stays 0 |

  Nothing else in the 51 moved — no status, no objective, no counter.
- **Census: 120/120.** OBSERVED: `QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet`
  passes in the full Release run (it fails at 116/120 with the drift repair, §3).
- **HS: no pin moved.** OBSERVED: `HsSweepCorpus.Corpus`, `HsBattery.BorderMode`,
  `HsBattery.RefactorizeMode` and `HsBattery.CrashBasisIs…` all pass unmodified. No re-pin was
  needed anywhere in this round, and none was made.
- **Suites.** Release `ctest -j4` (under `setsid`, `MKL_NUM_THREADS=1`, one solver at a time under
  the box lock): **2862/2862 passed, 0 failed** (`.scratch/nc2/logs/ctest-rel-final.log`). Debug
  `ctest -j4` without the long test: **2857/2861 passed**, the four failures being the qp2/qp3 dump
  fixtures' then-still-absolute status assertion — the fixture was corrected to assert the
  mechanism (§6) and both configurations are green on the final file
  (`.scratch/nc2/logs/seam-rel-v2.log` 90/90, `.scratch/nc2/logs/seam-dbg-v2.log`). The one
  > 600 s Debug test, `Continuation.ProbeBudgetBoundsAFailingProposal`, ran ALONE and detached:
  **Passed, 563.74 s** (`.scratch/nc2/logs/ctest-dbg-long.log`).
  By name: **8 ADDED, 0 REMOVED** — Release 2862 − 8 = 2854 = NC1's count (INFERRED from NC1's
  report; `dc8665e3` adds no tests over `5732a1f3`).
- **U0 EXACT** (27 F7 replay cells × 75 columns × 3 engines, `bench/hven_sqp_corpus` against the
  M6-close pin at `docs/notes/data/2026-09-m6-close-gate/replay`, gate 1e-5, runner
  `.scratch/nc2/g4.sh` with every path derived from this worktree, `MKL_NUM_THREADS=1`,
  `taskset -c 2`, under the box lock). OBSERVED, `.scratch/nc2/u0/comparison.txt`:
  `differences: 0` on **walk, ssn and ipm**; within-gate 7 on walk (the same four
  `f7_n1000_path_warm` and three `f7_n800_path_warm` residual-class digits NC1 and E2 fix6 both
  reported), 0 on ssn, 0 on ipm. **No U0 status, objective or counter moved, so there is no walk
  mover to declare.**

## 7b. The RE-TAKE 8 prediction, per cell, and what the corpus supports

The settler's prior was "YORKNET may convert — its restoration QPs are the fixtures; STEENBR
stays". **Under the rule set that actually shipped, I predict 0 TRAIN conversions, YORKNET
included**, and here is why, cell class by cell class:

- **YORKNET: no conversion.** Its restoration sub-QPs (`A0_YORKNET` qp1–qp6) convert only under the
  FACE CHECK (qp1/qp3/qp4) or the drift repair (qp1) or the ascending-step saddle exit (qp5) —
  none of which shipped. Under the landed tree all six still cap or fail, byte-identical to base
  (OBSERVED, corpus `final` vs `base`: not a single YORKNET row moved).
- **The two `f7` families and every HS cell: EXACT.** U0 0 differences on 27 × 75 × 3, and the whole
  HS battery/sweep/IPQP-dispatch set passes unmodified. INFERRED: those harness cells cannot move.
- **STEENBRB / STEENBRD / SAWPATH / DALLASM / READING6 / ACOPP57 / ZAMB2-8 / SMBANK: no conversion,
  counters almost certainly unchanged.** OBSERVED: their 36-corpus rows are byte-identical to base
  except `ACOPR57_def/qp6` (a capped cell, f in the 9th digit). The one place a shipped change can
  reach them is a post-drop pass on a flat face; the dumps show none outside the Cs family.
- **`ACOPR57`: counters may move by a digit.** `ACOPR57_def/qp6`'s capped iterate differs at 5.8e-9
  relative, so a late major of that cell could take a different step. No status change is supported.
- **The Cs_STEENBRB family — the cells the rule was built for — is NOT on the shipped path.** Those
  sub-QPs come from R2's COMPOSITE restoration arm; R2 states the shipped landing is still
  violation-only at f ≈ 1e12. So the three conversions this round buys are conversions of QPs the
  harness does not currently ask for. INFERRED, and worth the settler's attention: the flat-face
  rule's value is banked for whichever restoration arm ships, and it is what makes the composite
  arm's sub-QPs solvable — R2's recommendation ("QP engine first, then a Cc-based landing") is one
  step further along, not finished.
- The five cells already optimal hold (U0 exact, census identical, HS unchanged); not measured on
  the harness itself.

## 8. Re-pins

**None.** No existing fixture's pinned value was changed, no existing test was renamed or removed,
and `notices/` was not touched. The one status mover in the corpus (`Cs_STEENBRB/qp1`, num → cap)
is not a pinned fixture; it is declared in §7 with its cause.

## 8b. The whole trade space, measured (all corpus figures OBSERVED, 51 dumps, E2 on, identical on both algebras)

| tree | rules on | kOptimal | Release suite |
|---|---|---|---|
| `base` (= `dc8665e3`) | none | 12/51 | 2854/2854 |
| **`final` (shipped)** | flat face + the drift invariant | **15/51** | **2862/2862** |
| `w6` | + saddle exit | 17/51 | 11 failures (§4) |
| `w5` | + saddle exit + drift repair | 18/51 | 19 failures (§3, §4) |
| `w8` | + saddle exit + drift repair + flat face | 19/51 | 19 failures |
| `w3` | + saddle exit + drift repair (feas_tol-scaled) | 16/51 | ACOPR57 qp0/qp1/qp4 optimal → capped |
| `face` | the FACE CHECK alone | 16/51 | ACOPR57 qp4 optimal → capped, ACOPP57 f worse by 1.1e-3 |

The shipped tree is the only row in that table with a green suite. Everything above 15/51 costs
either the feasible census, a no-false-infeasible fixture, or a set of driver fixtures whose
premise is a QP failure.

## 9. Registered (not this round)

1. **The equality-drift re-projection.** Implemented and measured in three forms (§3). Form (3) is
   the one to start from: it is in this report's history, not in the tree. It needs the census and
   `T6b`/`E2Fix4` understood before it can ship — the open question is why a correction that closes
   the rows lands the elastic copy on a structurally violated point.
2. **The saddle exit** (§4), including the eleven fixtures it moves and the four that need
   redesigning rather than re-pinning. Worth its own round: it is the only mechanism measured to
   convert the `Hj_STEENBRD` family.
3. **The ascending-step saddle exit** (a ride on any kWrong pass whose EQP step does not descend):
   +1 conversion, 6 fixture premises moved, outside M4.
4. **The FACE CHECK** (refining every EQP answer onto its face): converts three YORKNET restoration
   QPs, violates M5 on convex faces (§4).
5. **R2 §0's two wrong premises**, for the ledger: `curvature_tol` is not 0 on an LP (the floor has
   been in `hessian_scale` since M3), and `ride_sign` does not read those slopes as flat (it reads
   them as disagreeing).
6. **`Cs_STEENBRB` qp5/qp6 cap with 0 undos**: the flat-face rule removed the cycling but not the
   budget. The remaining cost is one ride per drop over a 600-pin working set; a cheaper
   re-pricing, or a larger budget, would likely finish them (f is already within 1e-8).
7. **`Cs_STEENBRB/qp1` num → cap** (§7). A `kPtNoise` member that is never re-examined leaves the
   walk without the exhaustion exit it used to take. A rule that retires a `kPtNoise` mark after a
   real move would give it back.
8. **`STEENBRB_def/qp3` ends with f = NaN under the drift repair** (not in the shipped tree): the
   base walk already ends 8.1e2 off its equality rows with |f| ≈ 9e11 on that cell (the E2-D4 leak
   class), and the repair turns the garbage into a NaN at 864 minors instead of 1185. Both are
   kNumericalError.

## 10. What I could not determine

- **The option-off corpus was not re-run** on the final tree. The rules are guarded by
  `independent_working_set` and the off path is byte-identical by inspection, but I did not produce
  an `off` transcript for the landed tree; the predecessor's `baseoff` legs are base-source.
- **Accelerate: UNOBSERVED.** Nothing in this round ran on Mac hardware.
- **The saddle exit's HS-battery diff is unclassified.** I know which eleven tests move, not whether
  the HS battery's per-problem outcomes improve or degrade.
- **The first implementer's own reasoning** is not on disk; §1 re-derives M1 from the transcripts
  its probes left, and §3/§4 re-measure every draft rather than trusting it.
- **Why the drift repair breaks `T6b`** (the elastic copy) is diagnosed only as far as §3's
  INFERRED paragraph.
- **The walk seam still reports only the MEASURED slope** (`ride_slope`, `ride_slope_tol`). The new
  decision is observable through the `ride_predicted_signs` counter, which is what the fixtures
  assert; a seam field for the predicted slope would make the DISAGREE case readable per-pass and
  is a cheap follow-up I did not take (it would have invalidated the verification runs).
