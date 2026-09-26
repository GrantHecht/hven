# M7 W2 NC2 FIX ROUND 1 — the implementer's hand-back, placed here by the settler.
# Copied VERBATIM from `.superpowers/m7-w2-nc2-fix1-report.md` (implementer commit `7b36a01e`, cherry-picked to `0fa59cdc`).
# Nothing below this line is the settler's text; the round's own record is `README.md` in this directory.

# M7 W2 NC2 FIX ROUND 1 — report

Implementer hand-back. Worktree `agent-a69f46e11796560b9`, branch `nc2-fix1`, one squashed commit on
`71f54c50`. Builds `.scratch/nc2-build-{debug,release}` (clang, `HVEN_FP_MODE=SAFER_FAST`).
Evidence: `.scratch/nc2-fix1/` (this round; `.scratch/nc2/` untouched and unread-from except to COPY
the base/final corpus rows out for comparison). Every leg under `flock /tmp/box-build.lock` with the
`PGREP_EMPTY` / `FOREGROUND_START` / `FOREGROUND_END` markers, `set -o pipefail`,
`MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2`, one solver at a time.


## 0. WHAT THIS ROUND CONCLUDED, IN ONE PARAGRAPH

`rho` — the difference between the drop's predicted slope `s` and the measured `g'p` — is the PRIMAL
regularization's own term, `delta` over the free block as the EQP's single unregularized refinement
step leaves it, and NOT the dual term. M21(a)'s prescription (re-price regularization-free with the
pass's own factor, one extra solve) is ALREADY what `solve_eqp_eliminated` does, and on a flat face
it removes at most 56% of `rho`, so (a) is refused on evidence and registered to the EQP-side round
M12 opened; (b) ships. (b) makes the measured slope decide the sign whenever it resolves, which
delivers the **DESCENT INVARIANT** — `sign * g'p <= kEngineDenomTol*||g||*||p||` on every ride,
0 ascending rides on 12 Release cells against NC2's 1064 / 1072 / 1061 on three of them — and which
also reduces NC2's PRECEDENCE to M2's literal FLOOR, measured at 0 firings on 51 dumps. The
conversions that survive (`Cs_STEENBRB/qp2` and `qp4`, base's `kNumericalError`-by-exhaustion cells)
come from the corrected `kPtNoise` test, not from a ride; `qp3` reverts to base's cap. The
zero-length ride is a zero-motion event now, and that is the cause of four declared corpus movers,
the largest of which takes `A0_YORKNET/qp2` from 2.5e-03 off its equality rows to 5.7e-14. Corpus
**12/51 -> 14/51** both algebras, census 120/120, U0 0 differences on all three engines, option-off
byte-identical to base on all 51, HS unmoved, **0 re-pins**.

One squashed commit: **`7b36a01e`** on `71f54c50`, four files, no attribution trailer of any kind,
and neither `tests/qp/test_qp_nc2fix1_measure.cpp` nor its `tests/CMakeLists.txt` line in the diff.

## A. rho ESTABLISHED FIRST (M21's precondition)

### A.0 What rho is, exactly (the identity the measurement uses)

`price` forms `r = Hx + g + Ae'le + Ai'li` and sets `z = r` on the pinned components, so
`rho := (Hx + g) - A_W'(-lambda)` restricted to the components FREE under the drop pass's working
set is exactly `r` over that free block — which is the quantity
`detail::free_block_stationarity` computes and the walk seam already records as
`stat_free_inf` (its inf-norm). With `p` the next pass's EQP direction (`p = eqp.x - x`, and `x` is
NOT moved between the drop pass and the ride pass):

    g'p  =  s + rho'p + (the retained-nullspace defect terms)
    s    =  violation * dropped_directional(p)

so `rho'p` is recoverable EXACTLY, with no extra instrumentation, as `g'p - s`: the seam records
`ride_slope` (= `g'p`) and `released_directional` on the ride pass and `decision_multiplier`
(= the drop's own `violation`) on the drop pass, which is the record immediately before it.
That is how the table below was taken — no header was touched, no production code was
instrumented, and no solve was added.

Harness: the scratch gtest `tests/qp/test_qp_nc2fix1_measure.cpp` (the first implementer's, extended
with the rho columns and a descent-invariant count; **it does not ship** — the copy under
`.scratch/nc2-fix1/` is the record). Raw per-post-drop-pass CSVs:
`.scratch/nc2-fix1/probes/rho/<qp>_<alg>[_<override>].rho.csv`, transcripts `*.out`.

### A.1 The three cells M21 named, at the cycling drop (OBSERVED; sources UNCHANGED at `71f54c50`)

Dumps `docs/notes/data/2026-09-m7-w2-r2-composite-landing/dumps/Cs_STEENBRB/qp{1,3,5}.txt`
(`dims 660 96 0`, H structurally present with 3276 entries and **every value 0** — LPs, so
`g(x) = g` and `g'p` is the EXACT directional derivative, not an estimate). Summaries were
**byte-identical under `border` and `refactorize`** in every cell. `delta = mu = 1e-8` on every one
of these passes (the record's own `primal_delta` / `dual_mu`; the suspect ladder did not escalate).
Tail = the last 400 passes, which is inside the exact cycle the first implementer established
(period 6 on qp1/qp3/qp5).

| cell | tail post-drop passes | both slopes resolve | DISAGREE | median &#124;s&#124; | median &#124;g'p&#124; | median &#124;rho'p&#124; | ‖rho‖inf | ‖x‖inf | delta·‖x‖inf | ‖rho‖inf / (delta·‖x‖inf) | mu footprint (active_res_inf) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Release qp1 | 133 | 133 | **66** | 8.31e-05 | 8.20e-05 | 1.07e-06 | 5.62e-07 | 128 | 1.28e-06 | **0.439** | 4.26e-14 |
| Release qp5 | 133 | 133 | **66** | 3.60e-06 | 7.22e-07 | 2.87e-06 | 7.54e-08 | 16 | 1.60e-07 | **0.471** | 7.11e-15 |
| Debug qp3 | 133 | 133 | **66** | 1.73e-07 | 1.26e-07 | 4.78e-08 | 9.00e-08 | 32 | 3.20e-07 | **0.281** | 1.07e-14 |

The cycle, cell by cell, is two rides that alternate. Release qp1's period-6 tail, verbatim from
`probes/rho/qp1_border.rho.csv` (every row of the last 400 passes repeats these two to the last
digit):

| minor | drop's violation | dropped_directional(p) | s | g'p | rho'p | sign | alpha | df = alpha·sign·g'p |
|---|---|---|---|---|---|---|---|---|
| 6562 | 8.8925e-07 | -93.412 | **-8.3067e-05** | **-8.1994e-05** | +1.0724e-06 | +1 | 1.3036 | **-1.069e-04** (descends) |
| 6565 | 3.3518e-08 | -20.993 | **-7.0362e-07** | **+9.4482e-07** | +1.6484e-06 | +1 | 5.8849 | **+5.561e-06** (ASCENDS) |

**M21's first confirmation, OBSERVED:** on the ascending ride (minor 6565 and every sixth pass
after it) the measured `g'p = +9.4482e-07` has the OPPOSITE sign to `s = -7.0362e-07`, the ride
takes `sign = +1` from `s`, and `f` rises by `alpha * sign * g'p = +5.561e-06`. On an LP
`f(x + alpha*d) - f(x) = alpha * g'd` holds in exact arithmetic (H is identically zero, so the
quadratic term is absent — a proof, not an estimate), so that product IS the rise. It reconciles
with the first implementer's independently computed `df_up` sum (`+6.03e-03` over 1064 ascending
rides on this cell).

**The second confirmation, and it is an identity rather than a measurement:** whenever both slopes
resolve and DISAGREE in sign, `|s| < |rho'p|` necessarily, because `g'p = s + rho'p`. It is visible
in the row above (7.04e-07 against 1.65e-06) and it is what makes M21's noise test self-evidently
satisfied on every disagreement.

### A.2 WHICH TERM rho IS: the primal regularization's own, not the dual's

**It is NOT the dual term (mu·lambda). OBSERVED.** `active_res_inf` — the seam's own measure of how
far the iterate sits off the constraints the EQP holds at equality, which is exactly the
`dual_mu * |lambda|` footprint — is 7.1e-15 … 4.3e-14 at these drops, i.e. **eight decades below**
`|rho'p|` (1.6e-6). Changing `dual_mu` from 1e-8 to 1e-12 (qp1 border, delta left at 1e-8) left
`‖rho‖inf` at 3.05e-07, the same order as the 5.62e-07 of the unmodified run
(`probes/rho/qp1_border_NC2FIX1_MU_1e-12.rho.csv`).

**It IS the primal term (delta over the free block).** Three strands:

1. *The algebra of the assembly* (INFERRED from `include/hven/detail/qp/eqp_solve.h` and
   `assemble_kkt`): the regularized system's free-block row is
   `(H_ff + delta*I) x_f + H_fp x_p + Ae_f' le + Ai_f' li = -g_f`, so at its exact solution
   `r` over the free block is `-delta * x_f` — i.e. `rho = -delta * x_free` EXACTLY, before
   refinement.
2. *The magnitude tracks `delta*‖x‖inf` across cells* (OBSERVED, table A.1): with `delta` fixed at
   1e-8, `‖x‖inf` is 128 / 16 / 32 across qp1 / qp5 / qp3 and `‖rho‖inf` is
   5.62e-07 / 7.54e-08 / 9.00e-08 — an 8x spread in `‖x‖inf` answered by a 7.5x spread in
   `‖rho‖inf`, and the ratio `‖rho‖inf / (delta*‖x‖inf)` stays inside **0.28 … 0.47** on all five
   delta=1e-8 measurements. Solve error would scale with `‖g‖` and the face's condition number,
   which have no reason to track `‖x‖inf` by a factor of 7.5 across two different dumps.
3. *Reducing `delta` removes it, and removes the ascending rides with it* (OBSERVED, with the
   confounder declared): at `delta = 1e-9` the same qp1 walk has median `|rho'p| = 1.4e-17`,
   `‖rho‖inf = 1.0e-20`, **0 ascending rides of 22 moving**, and reaches `kOptimal`-class progress
   in 46 post-drop passes instead of 2176; at 1e-10 likewise. CONFOUNDER, stated: the walk is a
   DIFFERENT walk at a different `delta` (median `|s|` goes from 8.3e-05 to 1.587 — the drops it
   makes are real violations, not noise), so this is evidence about the regime, not a per-pass
   scaling law.

**The refinement is why the residual survives, and it is the reason M21(a) is unreachable with the
pass's own factor.** `eqp_solve.h` (:205-222) ALREADY performs exactly the step M21(a) prescribes:
one step of iterative refinement against the UNREGULARIZED system, solved with the same regularized
factor. So the `rho` measured above is a POST-refinement residual. Since `rho` before refinement is
`-delta * x_f`, that one step reduced `‖rho‖inf` from `delta*‖x_free‖inf` to 0.28-0.47 of
`delta*‖x‖inf` — a contraction of at best ~2x-3.5x. On these faces the regularized factor is a poor
preconditioner for the unregularized system (the post-drop free block of an LP is `delta*I` in the
ridden direction, so `‖K_delta^{-1} * Reg‖` is O(1) there, not O(delta)), which is exactly the
flat-face condition. See §B for what that does to M21(a).

**Answer to M21's question, stated plainly: rho IS the regularization's own term — the PRIMAL one,
`-delta * x` over the free block, as the EQP's single unregularized refinement step leaves it. It is
not the dual term.**

## B. THE DECISION — (b), with (a) MEASURED AND REFUSED

### B.1 M21(a) is already in the code, and on these faces it does not reach

M21(a) prescribes: *"re-price the multipliers regularization-free at the pass that decides the drop
(precedent: `refine_terminal_mu_free`; use the pass's own factor -- one extra solve at most,
counted), so that s and g'p agree and the predicted sign is exact by construction."*

**That step is already performed, on every EQP solve, and the `rho` of §A is what survives it**
(OBSERVED in the source, `include/hven/detail/qp/eqp_solve.h`, the lines that end
`solve_eqp_eliminated`):

```
    // One step of iterative refinement against the UNREGULARIZED system:
    // r = rhs - K0*y = (rhs - K*y) + diag(reg)*y, solved with the same
    // (already-factorized) regularized system.
    ...
    y = y + detail::solve_vec(kkt, residual_of(y));
```

`reg` there is `+primal_delta` on the free block and `-dual_mu` on the constraint block, and
`residual_of` is the residual of the UNREGULARIZED system — i.e. exactly the construction
`refine_terminal_mu_free` uses, with the pass's own factor, at one solve. So "one extra solve with
the pass's own factor" is not an available improvement: it is the status quo, and the measurement in
§A is a post-refinement measurement.

How much that step removes, quantified: before refinement `rho = -delta * x_free` exactly (the
assembly's own algebra), so `‖rho‖inf` starts at `delta * ‖x_free‖inf <= delta * ‖x‖inf`; after it,
`‖rho‖inf / (delta * ‖x‖inf)` is **0.28 … 0.47** (five measurements, §A.1). **One refinement step
removed at most 56% of the residual, and at worst 53%.** The reason is structural, not incidental:
on a flat face the post-drop free block of the regularized system is `delta*I` along the ridden
direction, so the regularized factor's error operator `K_delta^{-1} * Reg` has norm O(1) there
rather than O(delta) — the regularized factor is a poor preconditioner for the unregularized system
exactly where THE FLAT FACE lives. Iterating it k times would give at best `0.44^k`, and "s and g'p
agree by construction" is not a statement that survives a factor of 0.44 per step: it would need a
FRESH UNREGULARIZED FACTORIZATION of the face, which is what `refine_terminal_mu_free` actually
does (`eliminated_candidate` into a `local` factor) and what makes it affordable only at the
terminal check. Doing it at every drop pass would be one extra assemble+factorize per pass on every
E2-on solve — a per-pass cost on the whole corpus, for a rule whose own ruling (M12) has already
placed this repair elsewhere:

> M12: *"the repair belongs where the leak is made (the regularized KKT solve / the dual
> regularization on the equality block), which is the round to open before any walk-side
> re-projection is retried."*

**So (a) is refused on evidence and registered to the EQP-side round M12 already opened** (§H), and
(b) ships. This is a declared deviation from M21(a), of the same kind M11 and M12 are: the ruling's
premise ("rho IS the regularization's own term") is CONFIRMED, and its prescription turns out to be
the code's existing behaviour.

### B.2 What (b) is, and why it reduces to M2's literal floor

M21(b): *"when the measured slope resolves (|g'p| > slope_tol) and DISAGREES in sign with s, the
MEASURED slope decides descent."* Landed in `ride_sign` as the branch order reversed:

```cpp
    int sign = 0;
    if (slope < -slope_tol) {
        sign = 1;
    } else if (slope > slope_tol) {
        sign = -1;
    } else if (s_pred < -slope_tol || s_pred > slope_tol) {
        sign = s_pred < 0.0 ? 1 : -1;
        if (predicted != nullptr) { *predicted = true; }
    }
```

Three things follow, and the first is the one to say out loud.

**(i) This IS M2's literal FLOOR.** When both slopes resolve and AGREE, taking the measured sign and
taking the predicted sign give the same answer; when they DISAGREE, (b) says take the measured one;
when the measured one does not resolve, s decides. Those three cases collapse to "the measured slope
decides whenever it resolves, else s" — which is M2 as worded and what M11 recorded as the deviation
NC2 took (precedence instead of the floor). **Fix round 1 therefore reverts NC2's precedence to the
floor.** M11's measurement stands: the floor fires 0 times on 51 dumps, so
`ride_predicted_signs` is 0 on the corpus and the counter is a floor counter, retained because the
floor is the rule M2 asked for and is sound, not because it fires.

**(ii) THE DESCENT INVARIANT then holds by construction, in five cases.** Writing `S` for the
chosen sign and `t` for `slope_tol`:
1. both resolve, agree -> `S*slope < -t`;
2. both resolve, disagree -> `S` is the measured descending sign, `S*slope < -t`;
3. only s resolves -> `|slope| <= t`, so `S*slope <= t`;
4. only the measured resolves -> `S*slope < -t`;
5. neither resolves -> `sign == 0`, no ride.
In every case `S * g'p <= slope_tol`, hence `alpha * S * g'p <= alpha * slope_tol` for the
`alpha >= 0` the ratio test returns. (ii)'s feasibility veto only removes rides, so it cannot
break it; the (iii)-exchange re-arm calls `ride_sign` again with a FRESH `p`, so the case analysis
is re-run there and the stale `violation` cannot break it either — which also closes, for this
invariant, the registered (iii)-exchange-with-motion concern of M20(d).

**(iii) A disagreement is ALWAYS a noise drop, so the corrected `kPtNoise` test is provably
equivalent to "mark on every sign veto".** `g'p = s + rho'p`, so `sign(g'p) != sign(s)` with both
resolving forces `|s| < |rho'p|`. A sign veto under (b) arises only when (case 5) neither slope
resolves — then `|s| <= slope_tol` — or when the measured slope decided and (ii) vetoed it, which
needs `S` to be the RE-ENTERING sign, i.e. the measured sign to disagree with the leaving sign,
i.e. with `s`'s sign — again `|s| < |rho'p|`. And the converse: if `|s| > max(slope_tol, |rho'p|)`
then `s` and `g'p` share a sign, the measured sign IS the leaving sign, and (ii) cannot veto. So
under (b) the mark's test and "a sign veto happened" coincide. The test is nevertheless written out
and used, because the coincidence is a property of THIS rule and the mark's premise should be
checked rather than assumed — and because it is what answers the review's item (3): a resolving
multiplier on a `p` nearly parallel to the released face is no longer vetoed by `ride_sign` at all,
so it can no longer be marked noise.

### B.3 The landed test (M21's "or the equivalent test you can compute cheaply — state it")

M21's test is `|z_s| * |a_s'p| <= |rho'p|`, i.e. `|s| <= |g'p - s|`. Landed as

    |s|  <=  max( kEngineDenomTol * ||g|| * ||p|| ,  |g'p - s| )

in `QpEngine::ride_drop_is_noise` (`src/qp/qp_engine.cpp`), with the arithmetic floor folded in so
that case 5 (neither slope resolves, which IS a noise multiplier by the header's own premise) is
covered too. `|g'p - s|` is the FULL first-order residual along `p` — the primal-delta term of §A
plus whatever the retained rows' nullspace defect contributes — which is the right currency: it is
what the sign comparison actually competes against, and it is free (`g'p` and `s` are the two
numbers `ride_sign` already formed). No solve, no new state, one mat-vec at the undo site only.

## C. EACH ITEM AS LANDED

Diff against `71f54c50` (the WIP snapshot `3338bbe0` is replaced by `git reset --soft`):
`include/hven/core/solver_counters.h`, `include/hven/detail/qp/qp_engine.h`,
`src/qp/qp_engine.cpp`, `tests/qp/test_qp_flat_face_saddle.cpp`. **No file added, none removed.**
`tests/qp/test_qp_nc2fix1_measure.cpp` and its `tests/CMakeLists.txt` line are NOT in the diff
(M21(f)); the scratch copy lives in `.scratch/nc2-fix1/`.

### C.1 THE RULE — `ride_sign`, the measured slope decides when it resolves (M21(b))

`src/qp/qp_engine.cpp`, `QpEngine::ride_sign`. The branch order is reversed and the comment says
which slope is the exact one; nothing else in the function changed. With
`independent_working_set` OFF, `s_pred` is hard-zero and the two `s_pred` comparisons are
`0 < -tol` / `0 > tol`, both false, so the OFF path is base's token for token — as it was before,
and by the same argument.

### C.2 `kPtNoise` on the multiplier's own resolution (M21(b), replacing M18's test)

New `QpEngine::ride_drop_is_noise` (declared next to `ride_sign`), called at the undo site in place
of the unconditional mark:

```cpp
    const Vec gx = qp.H.selfadjointView<Eigen::Upper>() * x + qp.g;
    const double slope = gx.dot(p);
    const double s_pred = dropped.violation * dropped_directional(qp, dropped, p);
    const double resolution =
        std::max(detail::kEngineDenomTol * gx.norm() * p.norm(), std::abs(slope - s_pred));
    return std::abs(s_pred) <= resolution;
```

Cost: one `H*x`, one dot, two norms, at the undo site only (E2-on, and only when `ride_sign`
vetoed). No solve, no new counter, no new state. §B.2(iii) proves it is equivalent to the mark NC2
made, under (b)'s rule — the point of writing it out is that the premise is now CHECKED.

### C.3 THE ZERO-LENGTH RIDE (M17(b) / M21, ships regardless)

`RideBlocker` gains `double dx_inf` and its comment now says what the struct carries on `kStepped`
(the motion, not the blocker to decide). `ride_negative_curvature` fills
`blocked.{kind,idx,state,alpha,dx_inf}` on the stepped path — `dir` deliberately not copied, the
ride has taken it. The caller's `kStepped` branch:

```cpp
                    // A RIDE THAT DID NOT MOVE IS A ZERO-MOTION EVENT (4c,
                    // FINITENESS), on the ordinary add path's own test: the point
                    // memory survives it and the blocker is admitted through it,
                    // so the release/re-entry cap bounds a drop -> zero-length
                    // ride -> drop cycle.
                    if (ride.dx_inf > step_tol) {
                        pt_clear(seen);
                    }
                    (void)pt_admit(seen, counters, ride.kind == BlockKind::kIneq, ride.idx);
```

`step_tol` is the loop's own `feas_tol * max(1, ||x||inf)` and `dx_inf` is
`alpha * ||dir||inf` — the ordinary add path's own two quantities (`:1889`), and `pt_admit` mirrors
`:1952`. **OPTION-OFF HARD ZERO, by inspection:** with `independent_working_set` off, `seen.pt_ineq`
and `seen.pt_bound` are both constructed EMPTY (`src/qp/qp_engine.cpp` ~:503), `pt_admit`
early-returns `false` without touching a counter, and `pt_clear` walks an always-empty
`pt_touched` with `pt_events` always 0 — so guarding or not guarding the `pt_clear` is the same
program off. The unguarded form is kept because it is the smaller diff against what was there.

Beyond the cap, this also makes the ride's blocker `kPtEntered`/`kPtExempt` at this point exactly as
the ordinary path's blocker is, which is what keeps `select_exchange` from exchanging out a member
that entered here on a step that did not move. That is the mirror M17(b) asked for; it is declared
rather than incidental.

### C.4 HEADER section 4c (M19)

`include/hven/detail/qp/qp_engine.h`. THE FLAT FACE is rewritten to state the rule only; the
measured numbers (1312 undos, 2.0e-7 against -5.2e-8, "0 firings over 51 dumps", the YORKNET
alphas, the census 120 -> 116) are gone; the `.superpowers/` pointer is replaced by
`docs/notes/data/2026-09-m7-w2-nc2-flat-face/REPORT.md` in both places it occurred (THE FLAT FACE
and THE EQUALITY DRIFT). Specifically:

- the per-member-kind sign statement is written out ("`-lambda_s` on a row, `-z_s` at a LOWER bound
  and `+z_s` at an UPPER one, `dropped_directional` carries the matching sign"), which is the loose
  "-z_s" phrasing the review's item (1) flagged;
- the FLOOR argument is corrected: it is inert because `rho'p` DOMINATES the measured slope on these
  faces, so `|g'p|` is far ABOVE the tolerance, not below it — and a cancellation to within
  tolerance is a coincidence of measure ~tol/|s|, **not** something double precision forbids;
- the DESCENT INVARIANT is stated as the rule's own guarantee
  (`sign * g'p <= kEngineDenomTol * ||g|| * ||p||` on every ride taken);
- `kPtNoise`'s test is stated with its premise and with why an absolute floor on `|s|` would be
  wrong (the cos-theta reading the review's item (3) raised);
- the coincidence of a sign veto and a noise drop under this rule is stated, with its one-line proof;
- the "No anti-cycling rule is needed here: a ride's objective decrease is strict whenever
  alpha > 0" sentence becomes a FINITENESS paragraph with the **alpha = 0 clause** (a zero-length
  ride is a zero-motion event; the point memory survives it; the cap bounds the cycle).

`include/hven/core/solver_counters.h`: `eq_drift_violations`'s doc gains the line M19 asked for —
the "has met them" flag never clears, so this counts PASSES off the rows, not drift EVENTS — and
`ride_predicted_signs`'s doc is corrected to say when the branch it counts is taken (the measured
slope below its own resolution), since that is no longer "rather than the measured g'p".

## D. FIXTURES (M21(e))

`tests/qp/test_qp_flat_face_saddle.cpp`, the same 8 tests in the same target
(`hven_qp_walk_seam_tests`): **0 ADDED and 0 REMOVED by count**, all eight strengthened, and **6
RENAMED BY NAME** — which a by-name suite census reads as 6 REMOVED + 6 ADDED, so it is declared
here rather than left for the gate to discover:

    QpNc2FlatFace.TheSteenbrbRestorationLpQp{2,3,4}TakesTheMultipliersSign{Refactorize,Border}
 -> QpNc2FlatFace.TheSteenbrbRestorationLpQp{2,3,4}NeverAscendsTheSlope{Refactorize,Border}

The rename is the point of the round: those tests pinned "takes the multiplier's sign", which is the
behaviour this fix removes, and they now pin the descent invariant. The two synthetic tests keep
their names. `tests/CMakeLists.txt` is NOT in the diff.

New shared machinery in the file: `ride_tally` walks the seam's records and counts rides,
zero-length rides (`!(dx_inf > step_tol)` — the ordinary add path's own test), and rides whose
`ride_sign_chosen * ride_slope > ride_slope_tol`; `expect_descent_invariant` asserts that last count
is 0 and prints the worst ascent and the ride count when it is not. Every one of the eight tests
calls it.

Both synthetic tests and all six dump tests are GREEN in Release, and seven of eight in Debug (the
eighth is the declared budget skip). Their measured values are in §D.2 / §D.3.

### D.1 The two synthetic fixtures

- `AWrongSignedDropOnAnLpFaceRidesTheRayToItsBlocker` — unchanged QP and seed. Still `kOptimal` at
  `f = 0`, `x2 = 1`, 0 undos, 0 vetoes, the gate firing on `curvature == 0.0` against
  `curvature_tol == 1e-12`, the ray ridden to its blocker. THREE CHANGES: the descent invariant;
  **the IDENTITY the whole rule rests on is now pinned** —
  `|g'p - violation * dropped_directional(p)| <= ride_slope_tol` at this vertex, with the violation
  read off the DROP pass's record and the directional off the RIDE pass's; and
  `ride_predicted_signs` is asserted `== 0` with the reason in a comment (this vertex IS a
  stationary point of its working set and the 3x3 system is conditioned well enough that the EQP's
  own refinement drives `rho` to rounding, so the measured slope resolves and decides — the
  predicted branch is the FLOOR, and only an iterate whose residual dominates its multiplier reaches
  it). That last line is the honest statement of what §B.2(i) means for this fixture: it used to
  pin `ride_predicted_signs >= 1` under precedence.
- `ANoiseLevelDropIsUndoneAndCountedNotRidden` — unchanged QP, seed and counters
  (`ride_sign_vetoes == 1`, `ride_declines_fallthrough_blocked == 1`,
  `ride_predicted_signs == 0`, `zero_motion_exhausted == 0`, the vertex kept, one
  `kSlopeFlatVeto`, and at `-2e-8` the ray ridden to `x1 = 1`). ADDED: the descent invariant on both
  solves, and **the kPtNoise mark's own test spelled out on the flat-veto pass** —
  `|s| <= max(ride_slope_tol, |g'p - s|)` — which is why zero-motion exhaustion is not the exit.

### D.2 The three R2 dump fixtures, per configuration

`expect_cs` now runs the dump WITH the walk observer armed (it did not before) so the descent
invariant is asserted on the real faces, prints
`status / f / minors / rides / zero_length / predicted_signs / vetoes / undos / zmexh / zmcap`, and
asserts:

- in BOTH configurations and both algebras: the DESCENT INVARIANT, and the base failure mode
  (`zero_motion_exhausted == 0`);
- the per-configuration STATUS that this round MEASURED, selected by `kDebugBuild`
  (`#ifdef NDEBUG`) — stated in the file as what it is, a rounding-sensitive outcome on LPs that
  decide on 1e-7…1e-8 multipliers;
- where the status is `kOptimal`, M6's full statement: the E2-D4 KKT battery
  (`eq <= 1e-5`, `ineq <= 1e-5`, `box <= 1e-9`, `stat <= 1e-6`, `sign <= 1e-9`, `comp <= 1e-6`) and
  the Clarabel oracle within `2e-7` relative.

#### The DEBUG measurement (OBSERVED, `.scratch/nc2-fix1/logs/`; the run that produced it used the
#### fixture file as it stood at `71f54c50`, so every assertion but `ride_predicted_signs >= 1`
#### was checked by that run too and every one of them held)

| dump | algebra | status | minors | f | vetoes | undos | zmexh | predicted_signs | Debug wall |
|---|---|---|---|---|---|---|---|---|---|
| qp2 | refactorize | **kOptimal** | 248 | -192.000436939 | 7 | 7 | 0 | 0 | 0.9 s |
| qp2 | border | **kOptimal** | 248 | -192.000436939 | 7 | 7 | 0 | 0 | **745 s** |
| qp3 | refactorize | kMaxIter | 6600 | -384.000279251 | 809 | 809 | 0 | 0 | 22.9 s |
| qp3 | border | kMaxIter | 6600 | -384.000279251 | 809 | 809 | 0 | 0 | 316 s |
| qp4 | refactorize | kMaxIter | 6600 | -192.000148966 | 1288 | 1288 | 0 | 0 | 21.8 s |
| qp4 | border | kMaxIter | 6600 | -192.000148966 | 1288 | 1288 | 0 | 0 | 307 s |

The two algebras agree in EVERY printed column on all three dumps. Against NC2's Debug: **qp2
converts (cap 6600 -> kOptimal 248) and qp4 reverts (kOptimal 256 -> cap 6600)**; qp3 still caps.
Debug qp2's `f` is 7.99e-6 absolute from the Clarabel optimum, i.e. 4.2e-8 relative — inside M6's
2e-7 gate, which the run asserted.

**THE ONE BUDGET PROBLEM, and what was done about it (M6's "< 600 s per Debug fixture").** Debug
`qp2` BORDER costs **745 s**, over the limit; the other five are 0.9 … 316 s. The cause is not the
fix and not the observer (this run had no observer on the dumps): it is border mode's own Debug cost
on a walk that ADDS — qp2 converges in 248 minors with ~254 factorizations, and border/refactorize
is 833x here against 14x on qp3 and qp4, which both cycle without adding. REMEDY, declared: the
Debug configuration runs `qp2` under `refactorize` only and SKIPS the `border` variant with
`GTEST_SKIP` naming the measured cost; Release asserts both algebras on the same dump, and the table
above is the round's record of what the skipped cell does. The coverage lost is the border LINEAR
ALGEBRA on one dump in one configuration, on a walk the two algebras were just measured to agree on
column for column. REGISTERED: the 833x border/refactorize Debug asymmetry on an add-heavy walk at
`schur_cap = 128` is a performance observation worth a look (Debug-only, so informational under
CLAUDE.md section 7 and never a quotable timing).

#### The RELEASE measurement (OBSERVED, `.scratch/nc2-fix1/logs/seam-rel-flatface.log` for the
#### fixtures and `rho6-rel.log` for the whole Cs family)

The three fixture dumps, with the fixture's own print (both algebras identical in every column):

| dump | algebra | status | minors | f | rides | zero-length | vetoes | undos | zmexh | zmcap | Release wall |
|---|---|---|---|---|---|---|---|---|---|---|---|
| qp2 | refactorize | **kOptimal** | 252 | -192.000437989 | 60 | 28 | 3 | 3 | 0 | 0 | 0.14 s |
| qp2 | border | **kOptimal** | 252 | -192.000437989 | 60 | 28 | 3 | 3 | 0 | 0 | 11.4 s |
| qp3 | refactorize | kMaxIter | 6600 | -384.000271723 | 1307 | 13 | 1312 | 1312 | 0 | 0 | 3.4 s |
| qp3 | border | kMaxIter | 6600 | -384.000271723 | 1307 | 13 | 1312 | 1312 | 0 | 0 | 7.6 s |
| qp4 | refactorize | **kOptimal** | 246 | -192.000146263 | 51 | 18 | 15 | 15 | 0 | 0 | 0.13 s |
| qp4 | border | **kOptimal** | 246 | -192.000146263 | 51 | 18 | 15 | 15 | 0 | 0 | 10.4 s |

`ride_predicted_signs` is **0 in every cell** — the floor, as §B.2(i) says. Oracle distances:
qp2 6.9e-6 absolute = 3.6e-8 relative; qp4 1.2e-5 = 6.4e-8 relative (both inside M6's 2e-7 gate,
asserted); qp3 4.4e-5 = 1.1e-7 relative at the cap (asserted at 1e-6).

**THE DESCENT INVARIANT, on the whole Cs_STEENBRB family and not only the fixtures** (12 cells =
qp1…qp6 x both algebras, Release, walk seam):
`ascending_rides = 0` in every one, and the sum of `alpha * sign * g'p` over all rides is strictly
NEGATIVE in every one, with `df_up = 0` — i.e. **f is monotone non-increasing across rides in all
twelve, where NC2 had 1064 / 1072 / 1061 ascending rides on qp1 / qp5 / qp3 and non-monotone f in
five of five capped cells.** That is the round's result.

| dump | status | minors | f | rides | zero-length | uphill | sum of df | vetoes = undos |
|---|---|---|---|---|---|---|---|---|
| qp1 | kMaxIter | 6600 | -1378.22272466 | 2175 | 22 | **0** | -4.505e-01 | 1 |
| qp2 | kOptimal | 252 | -192.000437989 | 60 | 28 | **0** | -1.937e-04 | 3 |
| qp3 | kMaxIter | 6600 | -384.000271723 | 1307 | 13 | **0** | -1.836e-03 | 1312 |
| qp4 | kOptimal | 246 | -192.000146263 | 51 | 18 | **0** | -9.593e-05 | 15 |
| qp5 | kMaxIter | 6600 | -192.000031467 | 2162 | 17 | **0** | -4.557e-04 | 23 |
| qp6 | kMaxIter | 6600 | -192.000007685 | 1637 | 3 | **0** | -6.762e-04 | 816 |

**WHERE THE TWO SURVIVING CONVERSIONS COME FROM, and it is not the ride.** qp3, qp5 and qp6 return
to BASE exactly — same status, same `f` to all printed digits, same `undos`/`signv` (1312 / 23 /
816) — which is what §B.2(i) predicts, since the floor never fires and the vetoes are base's. qp2
and qp4 were base's `kNumericalError` at 251 / 250 minors through ZERO-MOTION EXHAUSTION
(`zmexh = 1`), and under the corrected `kPtNoise` test their sign-veto members no longer count
toward exhaustion, so that exit is gone and the walk certifies at 252 / 246 minors. **The
conversions are the kPtNoise correction's, measured.** qp1 is the same story without the conversion:
base's single veto fed its exhaustion exit at 219 minors, that exit is now unreachable, and the walk
rides to the budget (f -1378.22272466 against base's -1378.22297564, 1.8e-7 relative; §E declares
it).

## E. CORPUS (M7): vs the BASE rows AND vs the NC2 rows

51 dumps (the 36-dump E2-D4 corpus + the 15 R2 dumps), E2 on, both algebras, `dump_replay` built
against the installed Release library with `-DNC2_COUNTERS=1`, `MKL_NUM_THREADS=1`, `taskset -c 2`,
one solver at a time under the box lock. Rows in `.scratch/nc2-fix1/corpus/fix1-*`; the base and NC2
rows were COPIED out of `.scratch/nc2/corpus/` (`base-*`, `final-*`) and not regenerated.

**The comparison tool is new and stricter than NC2's.** `.scratch/nc2-fix1/tools/rowcmp.py` diffs
EVERY column of every row; NC2's `fcmp.py` compared only `status`, `minors` and `f`.
**SUCCESSOR NOTE ON THE NC2 REPORT (§7), which is not edited:** its "Nothing else in the 51 moved —
no status, no objective, no counter" is contradicted by its own corpus files. Re-running `rowcmp.py`
on `base` -> `final` finds an EIGHTH row NC2's table does not list:
`A0_YORKNET/qp2` [cap -> cap], `exch: 354 -> 68; stall_admits: 160 -> 17; undo: 3016 -> 0;
signv: 2585 -> 0` — four counters, no status/objective change, which is exactly what `fcmp.py`
cannot see. NC2's seven declared movers are otherwise reproduced exactly.

### E.1 kOptimal count

**base 12/51 -> fix1 14/51**, identical under `border` and `refactorize`. NC2 was 15/51. The
difference from NC2 is `Cs_STEENBRB/qp3`, which reverts to base's cap — §B.2(i)'s prediction, since
the floor never fires and qp3's exit was not an exhaustion exit.

### E.2 Every mover vs BASE, with its cause (7 rows, both algebras unless noted)

| dump | base -> fix1 | cause |
|---|---|---|
| `Cs_STEENBRB/qp2` | num 251 -> **opt 252**, f -192.000443022 -> -192.000437989, `zmexh 1 -> 0`, undo/signv 5 -> 3 | **THE kPtNoise TEST.** Base exited by zero-motion exhaustion on the exempt noise multiplier its sign veto left; the mark's corrected test keeps such a member out of `exempt_worst`, that exit is unreachable, and the walk certifies. |
| `Cs_STEENBRB/qp4` | num 250 -> **opt 246**, f -192.000150665 -> -192.000146263, `zmexh 1 -> 0`, undo/signv 18 -> 15 | the same |
| `Cs_STEENBRB/qp1` | num 219 -> **cap 6600**, f -1378.22297564 -> -1378.22272466 (1.8e-7 rel), `zmexh 1 -> 0`, stat 1.00 -> 3.9e-07, comp 2.9e-02 -> 8.7e-05 | the same mechanism WITHOUT the conversion: the exhaustion exit is gone and nothing else finishes inside the budget. Neither outcome is kOptimal; the residuals improve by 7 and 3 decades and `f` is 1.8e-7 relative off base's. NC2 declared this same move (M14(a)); it is not new. |
| `A0_YORKNET/qp2` | cap 7680 -> cap 7680, f -10830.1674936 -> -10828.4159537, **eq 2.51e-03 -> 5.68e-14**, **stat 1.98e-02 -> 1.05e-16**, comp 1.71e-02 -> 6.55e-03, exch 354 -> 2937, stall_admits 160 -> 1447, rideblk 1 -> 2, rexch 1 -> 2, undo 3016 -> 0, signv 2585 -> 0, `eqviol = 2875` | **THE ZERO-LENGTH RIDE (C.3).** The point memory now survives a zero-length ride and the ride's blocker is admitted through it, so the exemptions and the exchange path change and the walk diverges from base early. The base answer was INFEASIBLE by 2.51e-03 on the equality rows and 1.98e-02 in stationarity, so its lower `f` was not a feasible point; the fix1 answer is on the rows to 5.7e-14 and stationary to 1.1e-16. A strictly better iterate at the same status. |
| `Hj_STEENBRD/qp9` | num 306 -> num 308 (facts 309 -> 311 border / 308 -> 310 refactorize), f UNCHANGED to all 12 digits | the same cause, in the small: two extra passes before the same terminal verdict at the same objective. |
| `STEENBRB_def/qp1` | **border**: num 422 -> num 1034, f -991055300802 -> -991055299847 (9.6e-10 rel), eq 2.27e-06 -> 7.83e-09, box 3.03e-10 -> 1.34e-12, comp 1.32e-06 -> 6.60e-07, `zmexh 0 -> 1`, rideblk 108 -> 329, rexch 107 -> 323, passov 14 -> 51. **refactorize**: cap 4680 -> **num 1129**, f -991044861675 -> -991046643677 (1.8e-9 rel), stat 2.75e-06 -> 2.44e-05, rideblk 684 -> 286, rexch 684 -> 284 | the same cause, on the round's most sensitive cell: the tripled (border) / halved (refactorize) `rideblk`/`rexch` counts are the (iii)-declined-ride path re-routed by the new point memory. Status moves in refactorize only (cap -> num), on a restoration elastic objective of ~1e12 where neither outcome is a solution; three of four residual columns improve in border and `f` agrees with base to 9 significant figures in both. |
| `STEENBRB_def/qp2` | cap 4680 -> cap 4680, f -990729855749 -> -990591855583 (1.4e-4 rel), eq 1.67e-06 -> 6.55e-07, box 3.57e-13 -> 4.87e-14, stat 1.02e-07 -> 1.65e-05, rideblk 308 -> 100, rexch 308 -> 99 | the same cause. A capped cell in both; `f` is the elastic restoration objective at ~1e12 and is not a claim at a cap. `stat` degrades by two decades, `eq` and `box` improve; declared, not defended. |

**`Cs_STEENBRB/qp3`, `qp5`, `qp6` and `ACOPR57_def/qp6` are NOT movers vs base** — they return to
base's row exactly, `f` to all printed digits and `undo`/`signv` to the unit (1312 / 23 / 816 /
694). That is the floor's inertness made visible.

### E.3 Every mover vs the NC2 rows (26 rows, border; the same set under refactorize)

19 of the 26 differ ONLY in `predsign` (NC2's count of predicted-sign rides) going to 0 — the
floor's reversion, on `ACOPR57_def/qp7` (1174 -> 0), `SAWPATH_def/qp1` (3931 -> 0),
`STEENBRB_def/qp0` (113 -> 0), `Hj_STEENBRD/qp{1,4}`, `R1_Cs_STEENBRB_qp{3,4,5,6}` and the
`Cs_STEENBRB` family. Every other column on those rows is identical, which is the strongest available
statement that the floor decided nothing on this corpus. The remaining 7 are §E.2's rows plus
`Cs_STEENBRB/qp3` (NC2's `opt 294` -> base's `cap 6600`) and `ACOPR57_def/qp6` (NC2's move
reverting to base).

## F. CENSUS, U0, HS

**U0: EXACT.** 27 F7 replay cells x 75 columns x 3 engines, `bench/hven_sqp_corpus` from the final
Release build against the M6-close pin
(`docs/notes/data/2026-09-m6-close-gate/replay`), gate 1e-5, runner `.scratch/nc2-fix1/g4.sh` with
every path derived from this worktree, `MKL_NUM_THREADS=1`, `taskset -c 2`, under the box lock.
OBSERVED, `.scratch/nc2-fix1/u0/comparison.txt`:

    === M6 CLOSE HEAD-walk vs ... (GATED 1e-5) === cells compared: 27  columns compared: 75  differences: 0  within gate: 7
    === M6 CLOSE HEAD-ssn  vs ... (GATED 1e-5) === cells compared: 27  columns compared: 75  differences: 0  within gate: 0
    === M6 CLOSE HEAD-ipm  vs ... (GATED 1e-5) === cells compared: 27  columns compared: 75  differences: 0  within gate: 0

**differences: 0 on all three engines.** The 7 within-gate cells are the same residual-class digits
on `f7_n1000_path_warm` (4) and `f7_n800_path_warm` (3) that NC1, NC2 and E2 fix6 each reported;
`kkt_residual` / `kkt_stationarity` / `kkt_primal` / `kkt_complementarity` agree to 9-10 significant
figures. **No U0 status, objective or counter moved, so there is no walk mover to declare.**

**CENSUS: 120/120.** OBSERVED, the final Release suite:
`QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet` **Passed** (0.05 s).

**HS: no pin moved.** OBSERVED, the final Release suite: `HsSweepCorpus.Corpus` and all eleven
`HsBattery.*` tests Passed, unmodified — `BorderMode`, `RefactorizeMode`,
`CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions`,
`NoBatteryProblemEverCertifiesInfeasibility`, `ModeStatusDivergenceIsEmpty`,
`AggregateCostIsRecordedAndBounded`, `AdaptiveMuNeverSharesAMajorWithSocOrElastic`,
`SocIsRareAndUnprofitableAcrossTheBattery`, `EveryElasticActivationRunsTheRhoLadderToItsCeiling`,
`FeasibleStartsEndFeasibleDespiteTheLooseFunnelWidth`, `RefinementStepCountersOnBorderRepro`,
`BorderModeFalseInfeasible`, `TableCoversEveryShippedProblem`.

## F2. SUITES

Both under `setsid`, `ctest -j4`, `MKL_NUM_THREADS=1`, inside the box lock.

**Release** (`.scratch/nc2-fix1/logs/ctest-rel.log`): **100% tests passed, 0 tests failed out of
2862**, 2864 registered (1 pre-existing Skipped: `FailByDesignControl.ControlsArePresentFor...`;
2 pre-existing Disabled: `EqpRefinementAb.FootprintRuleProbe` / `.FullBattery`). Total 102 s. The
registered count 2864 is UNCHANGED from NC2 (its report's "2862/2862 passed" is the same
disabled-excluded figure), consistent with 0 tests added and 0 removed by count.

**Debug** (`.scratch/nc2-fix1/logs/ctest-dbg.log`), `Continuation.ProbeBudgetBoundsAFailingProposal`
excluded: **100% tests passed, 0 tests failed out of 2861**, 2863 selected. Total 863 s. Two skipped:
the pre-existing `FailByDesignControl` one and **the new
`QpNc2FlatFace.TheSteenbrbRestorationLpQp2NeverAscendsTheSlopeBorder`** (§D.2's declared Debug
budget skip). Two pre-existing Disabled.

Per-fixture Debug cost, M6's < 600 s (OBSERVED from the ctest log): `qp3` border **358 s**, `qp4`
border **348 s**, `qp3` refactorize 24 s, `qp2`/`qp4` refactorize and the two synthetics under 1 s,
`qp2` border SKIPPED (745 s measured). The longest Debug test in the whole suite other than the
excluded one is `QpE2WorkingSet.ADeclinedRideIsExchangedOnSteenbrbRestorationQp3` at 193 s, which is
pre-existing.

**The one > 600 s Debug test, ALONE and detached** (its own lock leg, `ctest -j1 -R` on that name
only, `.scratch/nc2-fix1/logs/ctest-dbg-long.log`):

    1/1 Test #1587: Continuation.ProbeBudgetBoundsAFailingProposal ...   Passed  562.04 sec
    100% tests passed, 0 tests failed out of 1


**RE-VERIFICATION AFTER A WHITESPACE EDIT (declared).** One blank line was added to
`tests/qp/test_qp_flat_face_saddle.cpp` (between the closing `} // namespace` and the first R2
`TEST`, restoring the file's own spacing) AFTER the two suite builds. Only
`hven_qp_walk_seam_tests` recompiles from that file, so no other test binary can be affected; the
target was rebuilt in BOTH configurations and its 8 flat-face tests re-run:
**Release 8 PASSED, Debug 7 PASSED + 1 SKIPPED** (the declared budget skip), every `[ nc2 ]` value
identical to the suite runs (`.scratch/nc2-fix1/logs/reverify-{release,debug}.log`). The Debug
figures the fixtures print, for the record:

| dump | algebra | status | minors | f | rides | zero-length | vetoes = undos | zmexh | zmcap |
|---|---|---|---|---|---|---|---|---|---|
| qp2 | refactorize | kOptimal | 248 | -192.000436939 | 52 | 21 | 7 | 0 | 0 |
| qp2 | border | (SKIPPED in Debug; 745 s) | | | | | | | |
| qp3 | refactorize | kMaxIter | 6600 | -384.000279251 | 1642 | 12 | 809 | 0 | 0 |
| qp3 | border | kMaxIter | 6600 | -384.000279251 | 1642 | 12 | 809 | 0 | 0 |
| qp4 | refactorize | kMaxIter | 6600 | -192.000148966 | 1320 | 16 | 1288 | 0 | 0 |
| qp4 | border | kMaxIter | 6600 | -192.000148966 | 1320 | 16 | 1288 | 0 | 0 |

`ascending_rides == 0` on every one of them (the fixture's own assertion), so **the descent invariant
is asserted on all three dumps in BOTH configurations and, except for the one skipped cell, both
algebras.**

## G. OPTION-OFF

Run once on the final tree: the 51 dumps with `independent_working_set` FALSE, `border`,
`.scratch/nc2-fix1/corpus/fix1off-border-{new15,old36}.txt`, against
`.scratch/nc2/corpus/baseoff-border-*` (base's own off rows, copied out).

**BYTE-IDENTICAL on all 51 rows, both lists**, modulo the two counter columns the base tool did not
print (`predsign` and `eqviol`, which the base rows lack entirely and which are **0 on every off
row**). Transcript:

    off new15: IDENTICAL
    off old36: IDENTICAL
    (diff of the rows with ` predsign=N eqviol=N` stripped; empty on both lists)

That is the measured half of C.3's inspection argument: with the option off, `s_pred` is hard-zero in
`ride_sign`, `ride_drop_is_noise` is never called (its call site is inside the E2-only branch), and
the point-memory vectors are constructed EMPTY so the new `pt_admit` and the guarded `pt_clear` are
both no-ops.

## G2. RE-PINS

**None.** No correctness fixture was re-pinned to hide a break; the three dump fixtures' status
assertions are RE-DERIVED from this round's measurement and the derivation is in §D.2, with the
mechanism assertions (descent invariant, no zero-motion exhaustion) stated in both configurations so
that a future rounding difference cannot quietly weaken them. HS pins: §F.

## H. REGISTERED (not this round)

1. **THE REGULARIZATION-FREE RE-PRICE, to the EQP-side round M12 opened.** §B.1: the one
   unregularized refinement step the pass's own factor can afford is already performed inside
   `solve_eqp_eliminated`, and on a flat face it removes at most 56% of `rho`. Making `s` exact by
   construction needs the face's UNREGULARIZED factor (what `refine_terminal_mu_free` builds), i.e.
   one extra assemble+factorize per drop pass. The candidate list for that round therefore gains a
   third item beside M12's own two (`tools/eqp.py` reg-vs-refined on `yk3_it177.state`, and the
   cheap walk-side `Ae*sdir` guard): **a better preconditioner or more refinement for the
   post-drop EQP on a flat face**, whose pay-off is measured here as "delta one decade smaller turns
   2176 noise drops into 46 real ones and 0 ascending rides".
2. **`ride_predicted_signs` is a FLOOR counter with 0 firings on the corpus.** M11's measurement is
   re-confirmed by this round's own reversion to the floor. It is kept because the floor is M2's
   rule and is sound; a future round that finds it still 0 after the EQP-side repair may propose
   retiring it, which is a counter-schema event and not a silent deletion.
3. **The (iii)-exchange-with-motion re-arm** (M20(d)) stays registered as a rule-exactness question,
   but it can no longer break the descent invariant: `ride_sign` is re-called with a fresh `p` on the
   re-armed pass, so §B.2(ii)'s case analysis is re-run (stated there).
4. **`curvature_tol` on an LP.** M2 asked for the floor's derivation in the header. It is stated
   ("hessian_scale floors at 1, so curvature_tol is a positive floor and not 0") without the
   numeric 1e-12, which is a `kCurvatureTolFactor` value and not a rule. The synthetic fixture
   still pins the number (`curvature_tol == 1e-12`), which is where a number belongs.

## I. WHAT I COULD NOT DETERMINE

1. **Whether `rho` on the cycling face is EXACTLY `c * delta * x_free` with a face-dependent `c`, or
   partly the solve's own error.** The three strands in §A.2 make the primal-regularization reading
   the only one consistent with all of them, and they rule the dual term out by eight decades, but
   `‖rho‖inf` is an inf-norm: separating `-delta * x_f` from a same-sized backward error would need
   the vector itself, which no recorded field carries and which I declined to add (it would have
   meant editing `qp_walk_trace.h` — a header included by everything — for a measurement, then
   reverting it). The distinction does not change the decision: either way `rho` is what the
   regularized solve leaves behind and neither reading is removable by one more refinement step with
   the pass's own factor (§B.1).
2. **The exact contraction factor of the refinement step on the cycling face.** Bounded above as
   `‖rho‖inf / (delta * ‖x‖inf)` in 0.28 … 0.47 (that is an upper bound on the surviving fraction
   only if `‖x_free‖inf == ‖x‖inf`; if the largest free component is smaller, the step removed even
   less). Measuring it directly needs the pre-refinement residual, which `solve_eqp_eliminated`
   exposes only through `unrefined_out` and which nothing records.
3. **Whether the EQP-side repair of item H.1 would restore NC2's three conversions.** The
   `delta = 1e-9` run says the regime is the cause and that a smaller residual converts, but that
   run is a different walk (§A.2 strand 3) and `delta` is not the knob a repair would turn.

## I2. HOW A REVIEWER CAN RE-DERIVE EACH CLAIM

| claim | where |
|---|---|
| rho's magnitudes and the per-pass rho columns | `.scratch/nc2-fix1/probes/rho/*.rho.csv` and `*.out`; the harness is `.scratch/nc2-fix1/test_qp_nc2fix1_measure.cpp` (NOT shipped), run through `.scratch/nc2-fix1/rho.sh` / `rho_all.sh` / `rho6.sh` |
| the descent invariant on the whole Cs family, Release | `.scratch/nc2-fix1/logs/rho6-rel.log` (`DESCENT_INVARIANT ascending_rides=0`, 12 cells) |
| the fixtures' own values | `.scratch/nc2-fix1/logs/seam-rel-flatface.log`, `reverify-{release,debug}.log`, and the `[ nc2 ]` lines inside `ctest-{rel,dbg}.log` |
| corpus | `.scratch/nc2-fix1/corpus/fix1-*`, `fix1off-border-*`; comparator `.scratch/nc2-fix1/tools/rowcmp.py BASE TAG [--alg A] [--skip cols]` |
| U0 | `.scratch/nc2-fix1/u0/comparison.txt`, `head-{walk,ssn,ipm}.csv`, runner `.scratch/nc2-fix1/g4.sh` |
| suites | `.scratch/nc2-fix1/logs/ctest-rel.log`, `ctest-dbg.log`, `ctest-dbg-long.log` |
| the box protocol on every leg | the `PGREP_EMPTY` / `FOREGROUND_START` / `FOREGROUND_END rc=` markers at the head and tail of `.scratch/nc2-fix1/logs/phase-{rel,final,reverify,long}.log`, emitted by `.scratch/nc2-fix1/leg.sh` INSIDE `flock /tmp/box-build.lock` |

## J. LEDGER NOTE (for the settler)

NC2 fix round 1 — rho ESTABLISHED as the PRIMAL regularization's own term (delta over the free
block, `||rho||inf / (delta*||x||inf)` 0.28-0.47 on five cells, the mu footprint eight decades
below); M21(a) refused on evidence (the one unregularized refinement step it prescribes is already
inside `solve_eqp_eliminated` and removes at most 56% of rho on a flat face) and registered to the
EQP-side round M12 opened; M21(b) landed, which reduces NC2's predicted-sign PRECEDENCE to M2's
literal FLOOR (0 firings on 51 dumps) and delivers the DESCENT INVARIANT
`sign * g'p <= kEngineDenomTol*||g||*||p||` on every ride, asserted on the walk seam in both
configurations and both algebras; `kPtNoise` now marked only on `|s| <= max(slope floor, |g'p - s|)`
and that correction is where the two surviving conversions come from (Cs qp2 / qp4, base's
zero-motion-exhaustion exits); M17(b)'s zero-length ride landed (no `pt_clear`, `pt_admit` the
blocker) and is the cause of four declared corpus movers, one of which (A0_YORKNET/qp2) takes the
iterate from 2.5e-03 off its equality rows to 5.7e-14. Corpus 12/51 -> 14/51 both algebras (NC2 was
15/51; Cs qp3 reverts to base's cap); option-off byte-identical to base on all 51; U0 0 differences
on all three engines; 0 re-pins. SUCCESSOR NOTE on the NC2 report §7: an eighth base->NC2 mover
(`A0_YORKNET/qp2`, four counters) exists that `fcmp.py` could not see.

SIGNOFF: M7-W2-NC2-FIX1-REPORT
