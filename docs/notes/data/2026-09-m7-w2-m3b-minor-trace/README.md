# M7 W2 M3b — the first-QP per-minor trace: what IS the active-set walk doing for 4 680 minors?

Pre-registered rule: [`RULE.md`](RULE.md), committed at `d35f70f8` **before any
leg ran, the identity legs included**. Provenance:
[`PROVENANCE.txt`](PROVENANCE.txt). Machine-written verdict:
[`verdict.txt`](verdict.txt). Non-vacuity transcript:
[`fold-walk.txt`](fold-walk.txt). Raw traces: [`traces/`](traces) — committed
whole, one record per minor iteration.

> **FIX ROUND 1 (2026-09-21) — READ THE LAST TWO SECTIONS WITH THIS FILE.**
> A review found four things wrong or missing in the text below. The
> corrections, and every withdrawal, are listed in
> [**Corrections (fix1)**](#corrections-fix1); the measurement that settles the
> question this file could only speculate about is in
> [**Re-trace (G)**](#re-trace-g), under the separately pre-registered
> [`RULE-G.md`](RULE-G.md) with its own verdict
> [`verdict-G.txt`](verdict-G.txt). **`RULE.md` and its `CYCLING_PROPER` verdict
> stand unchanged** — the re-trace answers WHY, not WHAT. Passages the fix round
> withdrew or narrowed are marked in place.

---

## The result, in plain language

**The walk is cycling, and it is not subtle.** On both cells the first QP
subproblem stops making progress after a few dozen minors and then repeats
**one drop and one re-add of the same bound variable, at zero step, for the
rest of its iteration budget** — 4 621 of STEENBRB's 4 680 minors (98.7 %) and
2 383 of YORKNET's 2 560 (93.1 %). Inside that stretch **every recorded field
of every record repeats with period 2**: the same working set, the same
iterate, the same direction, the same multiplier, the same 64-bit working-set
signatures alternating between the same two values. It is a bit-exact
two-cycle, not a slow crawl.
[*NARROWED (fix1): the exact claim is "every RECORDED field repeats in an exact
zero-motion working-set period-2 tail" — the trace holds `‖p‖∞` and norms of
`x`, not the vectors themselves. See Corrections (fix1) C4.*]

Concretely, on STEENBRB, from minor 59 to minor 4 679:

1. At a KKT point of a 22-member working set, variable **189**'s multiplier at
   its **lower** bound has the **wrong sign**, magnitude `1.1748e-3` against a
   drop tolerance of `1e-9`. The Dantzig rule releases it. Working set → 21.
2. The next minor solves the EQP on the 21-member set and gets a direction of
   norm `‖p‖∞ = 1.4569e4` — sixteen times the iterate itself (`‖x‖∞ = 913.6`).
   That direction points **back through the bound just released**, so the ratio
   test's minimum is `-0.0` on variable 189 itself, uniquely (`ratio_tied = 1`).
   The step is `-0`, the iterate does not move, and 189 is re-pinned at its
   lower bound. Working set → 22, **identical to step 1**.
3. Go to 1.

YORKNET does the same thing on variable **291** at its lower bound, from minor
177 to minor 2 559, with `|multiplier| = 1.4895e7` and `‖p‖∞ = 44.42`.

This answers the question M7 W2 M3a could not. Its verdict was `MIXED` and it
explicitly did **not** show cycling proper, because six different mechanisms
produce the same per-solve counters. Five of those six are now ruled out on
these cells by direct observation, and the sixth — an immediate drop and re-add
of the bound just admitted — is what the walk is made of.

**Nothing here licenses a change.** No default was changed, no file under
`src/` or `include/` was touched by the measurement, and `RULE.md` §0 said
before any leg that no outcome could license one. §8's remedy mapping, restated
at the end of this file, is a recommendation to the settler and nothing more.

---

## The verdict, as the rule returns it

| cell | outcome | `R` | first recurrence | period | drift bound on it | also holds |
|---|---|---|---|---|---|---|
| **STEENBRB** | **`CYCLING_PROPER`** | 0.9889 | `i = 22`, `j = 24` | 2 | 1.83e-06 | `IMMEDIATE_READD` (`P_imm` = 0.9940) |
| **YORKNET** | **`CYCLING_PROPER`** | 0.9301 | `i = 177`, `j = 179` | 2 | 3.02e-07 | `IMMEDIATE_READD` (`P_imm` = 0.9659) |

`CYCLING_PROPER` (§3.1) needs `R ≥ 0.20` **and** a recurring pair whose whole
window moved the iterate by nothing at the walk's own resolution. Both cells
clear the first by more than four times and satisfy the second on **every**
recurring pair — 4 628 of them on STEENBRB, 2 381 on YORKNET. Precedence (§4)
hands the label to `CYCLING_PROPER` over `IMMEDIATE_READD`, which also holds;
both are reported, as §3 requires, and neither is relabelled.

The other four predicates are **false on both cells**, and their numbers are
worth having because they are what rules the competing mechanisms out:

| predicate | bar | STEENBRB | YORKNET |
|---|---|---|---|
| `SIDE_PING_PONG` | `P_flip ≥ 0.40`, `C_top ≥ 0.20` | `P_flip = 0`, 0 side flips in 2 344 bound admissions | `P_flip = 0`, 0 in 1 287 |
| `ZERO_PROBE_CHURN` | `P_probe ≥ 0.30` | 0 — **all 2 323 drops are Dantzig drops** | 0 — all 1 232 |
| `NEGATIVE_CURVATURE_BLOCKERS` | `P_ride ≥ 0.30` | 0.00085 — 2 ride-adds in 2 344 | 0 — none |
| `DEGENERATE_CRAWL` | `G ≥ 0.70` and both halves | `G = 34/4680 = 0.0073`; `D₂ = 2` | `G = 138/2560 = 0.054`; `D₂ = 2` |

`D₂ = 2` on both cells is the crawl hypothesis dying: the **entire second half
of each walk visits exactly two distinct working sets**.

### The rule was proven able to fail before it was pointed at a real trace

[`fold-walk.txt`](fold-walk.txt) is the transcript of `RULE.md` §6's nine
pre-registered synthetic traces, run as twelve cases (§6's last two rows each
name several conditions and each is exercised separately): **12 folds, 0
failed, 8 distinct classes returned.** A classifier that returned `MIXED` for
all of them would be vacuous and its output would not be a verdict. Separately,
the ABORT checker `RULE.md` §2 delegates to was fold-proven **before the legs
ran**: [`gates-trace/identity-folds.txt`](gates-trace/identity-folds.txt), 15
folds, 0 failed, of which 13 are refusals.

### The gate the legs had to pass first

Both trace legs reproduce M3a's **committed** `rows/f/<cell>@F-trinf.txt`
record 0 on **all 20 `QpCounters` fields plus the record status**, DIFFERING 0
— compared against the bytes of that file, key by key, not against `RULE.md`'s
transcription of it ([`gates-trace/`](gates-trace)). Trace integrity is green
on both: one record per pass, nothing truncated, the cold first subproblem,
exited on the cap. Every internal cross-check the rule demanded **agrees**:
`N_drop` against `ws_drops`, `N_add` against `ws_adds`, the distinct-identity
count against `distinct_bound_added` (23 / 95), the admission count against
`ws_adds_bound` (2 344 / 1 287), and the tie count against `ratio_ties`
(0 / 52).

---

## OBSERVED — what is in the trace

Everything in this section is a recorded field of
[`traces/`](traces), or arithmetic over those fields. Full tables:
[`tables/`](tables).

**Progress stops exactly where the cycle starts.** The last minor that moved
the iterate by more than the walk's own `step_tol` is **58** on STEENBRB
(`m_stop/N = 0.0124`) and **176** on YORKNET (`0.0688`); the final cycle begins
at minor **59** and **177**. The sum of `dx_inf` over every pass after `m_stop`
is **exactly 0** on both cells — not "small", zero. Of 4 680 passes, 38 moved
the iterate; of 2 560, 70 did.

**One variable dominates; there is no succession of cycles.** Scanning for
maximal runs that alternate a drop and an add of a single identity, there is
**exactly one such run of length ≥ 4 in each trace**, and it is the final one.

| cell | admissions | distinct identities | top identity | its share | identities admitted exactly once |
|---|---|---|---|---|---|
| STEENBRB | 2 344 | 23 | `bound189` @ lower, **2 314** | 0.9872 | 16 |
| YORKNET | 1 287 | 95 | `bound291` @ lower, **1 193** | 0.9270 | 94 |

So M3a's "23 distinct bound variables admitted 2 344 times" was true and
misleading: 22 of the 23 account for 30 admissions between them, and one
accounts for 2 314. On YORKNET it is starker — 94 of the 95 variables are
admitted **once each**, and the 95th is admitted 1 193 times.

Before the final cycle locks on, the walk does pass through the same *shape* on
other variables — on STEENBRB, minors 22–58 drop and re-add `bound83`,
`bound264`, `bound189`, `bound217`, `bound205`, `bound158` at zero step, with a
`move_only` pass between each pair that still moves `x` by ~0.1 (minors 21, 24,
27, 30, 33, 37, 40, 43, 47, 50, 52, 55, 58 — and then never again). Those are
2-pass events separated by real motion, not cycles; §3.1's first recurrence,
`(22, 24)`, is one of them. The walk escapes each of them and then fails to
escape the last one.

**The cycling pair, field by field** (STEENBRB, minors 59 and 60; the same two
records repeat 2 310 more times):

| | minor 59 | minor 60 |
|---|---|---|
| action | `drop_dantzig` | `add` |
| identity | `bound 189`, side `lower` | `bound 189`, side `lower` |
| drop route | `dantzig` (**not** the zero-multiplier probe) | — |
| `decision_multiplier` / `decision_tol` | `1.1748e-3` / `1e-9` | — |
| `best_multiplier`, `drop_candidates`, `drop_tied` | `1.1748e-3`, 10, 1 | — |
| `alpha` | — | **`-0.0`** |
| `p_inf` | `0.0` | **`14568.88`** |
| `dx_inf` | — | **`-0.0`** |
| `raw_ratio`, `ratio_tied` | — | **`-0.0`**, **1** |
| `degenerate`, `ride` | false, false | **true**, **false** |
| `bound_gap` | `inf` | `inf` |
| `ws_size_in` → `ws_size_out` | 22 → 21 | 21 → 22 |
| `ws_sig_in` | `14864884868504189174` | `223057516508574445` |
| `x_inf`, `x_l1` | `913.560915499257`, `103080.58777493805` | identical |

**The engine never reacts to the stall.** Over the whole of both walks the
trace contains **0** `suspect_escalation` passes, **0** `post_probe_restart`
passes, **0** `drop_zero_probe` passes and **0** `terminal` passes. YORKNET has
one `start_repair`, at minor 0, and STEENBRB none; STEENBRB has two
`ride_stepped` passes, at minors 35 and 45 — both before the cycle. Not one
`move_only` pass occurs after the cycle begins on either cell (the last are
minors 58 and 176). The walk simply runs out of minors.

**The re-admitted variables are NOT fixed or near-fixed.** `bound_gap` is
`+inf` on all 2 344 STEENBRB admissions (these are one-sided bounds) and
`9.34` on YORKNET's cycling variable. At each pass's own `step_tol` and again
at `opt_tol = 1e-9`, every admission classifies `ORDINARY`. The fixed-variable
mechanism is not what is happening.

**The drop is not a marginal tolerance call.** `|multiplier| / opt_tol` over
all drops: median `1.17e6` on STEENBRB, `1.49e16` on YORKNET; minimum
`7.18e4` and `1.32e15`. And `|multiplier| / best_multiplier` is exactly `1` at
every drop on both cells — the rule always picks the largest violation, and
there is never a near-tie it could have broken differently (`drop_tied` max 1
on STEENBRB, 5 on YORKNET).

---

## INFERRED — what the code says about why, read against the trace

Everything in this section is **code reading**, labelled as such. It is not
recorded by the instrument and is not part of the verdict.

The four candidate explanations the settler named, judged against
`src/qp/qp_engine.cpp` and `include/hven/detail/qp/qp_engine.h`:

**(i) An indefinite reduced Hessian on the post-drop working set — CONSISTENT,
and the leading candidate.** hven uses the exact Lagrangian Hessian and
STEENBRB is nonconvex, so the EQP solved on the 21-member set need not have a
minimizer: its stationary point can be a saddle, and the direction to a saddle
has no obligation to descend or to respect the sign of the multiplier that
justified the drop. Two observations point that way. The recorded `‖p‖∞` is
`1.46e4` against `‖x‖∞ = 913` — the signature of a reduced KKT with a tiny or
negative eigenvalue, whose Newton step blows up along the near-null direction.
And the direction points back through the released bound, which a *convex* EQP
could not do: `drop_worst` releases 189 because `z(189) < 0` says the objective
falls as `x(189)` rises off its lower bound, and a positive-definite reduced
Hessian would return `p(189) > 0`.
[*NARROWED (fix1): true only for the EXACT, UNREGULARIZED, FULL-RANK KKT
identity `z_i p_i = −pᵀHp`. hven solves a regularized system and no rank
evidence exists for the released bound. See Corrections (fix1) C2, and the
identity checked against both cells as read-out (e) of
`tables-fix1/<cell>-outside-the-rule.txt`.*]

*Is the inertia gate evaluated on these passes at all?* **It is computed on
every pass and consulted on none of them.** `eqp_candidate` fills a
`detail::InertiaVerdict` on every iteration, but the loop acts on it in exactly
three places: the `iter == 0` temporary-vertex start repair; the terminal
branch (reached only when `drop_worst` finds nothing to drop); and the
suspect-stall gate, which sits *inside* that same terminal branch. On the
cycle's drop pass `drop_worst` succeeds, so the terminal branch is never
reached; on the cycle's add pass the verdict is not read at all. The header
says as much — "a wrong inertia LATER is section 4c's ride to handle". **The
trace does not record the verdict's value, so what it actually says on these
passes is UNOBSERVED.**
[*fix1: the consulted-at-three-places reading is CONFIRMED and stands as a
VERIFIED DESIGN FACT, not a proved defect (Corrections C3). The verdict's value
is no longer unobserved — the re-trace records it: `kWrong` on every tail pass
of STEENBRB, `kOk` on every tail pass of YORKNET.*]

*And §4c's ride cannot handle it.* The ride is armed on exactly this pass
(`dropped.active` is true after a drop) and computes
`curvature = pᵀHp / pᵀp` against `curvature_tol`. The recorded action is `add`,
so either the curvature gate did not fire or the ride **declined** — the trace
cannot separate those. But `ride_sign` declines **by construction** in this
configuration: its feasibility veto is "the descending sign must not step back
into the constraint the drop rule just released", and stepping back into the
constraint just released is precisely what `p` does here. So even if the
curvature gate had fired, the ride would have declined and the loop would have
fallen through to the same ordinary step. **hven's only anti-saddle machinery
on this path is structurally unable to act on this pattern**, and when it
declines it discards the fact that it declined.

> **WITHDRAWN (fix1) — this paragraph's "by construction" claim is WRONG.**
> `ride_sign` (`src/qp/qp_engine.cpp`, the `slope` / `leaves` block) can and
> does select `−p`: it picks the sign by DESCENT first, and a `p` that is an
> ASCENT direction gives sign `−1`, which then also moves AWAY from the released
> constraint and passes the feasibility veto. The trace shows it happening twice
> on STEENBRB — minors 35 and 45, both `ride_stepped`, both with `gᵀp > 0`,
> moving the iterate by 862 and 1127. The veto is a real refusal on a real pass,
> not a structural impossibility. What the paragraph gets right is its last
> clause: the engine did discard the fact that it declined, and that is what
> fix1's `ride_outcome` field now records. An ordinary zero-step add proves only
> that the ride did not step, never why. See Corrections (fix1) C1 and
> Re-trace (G).



**(ii) The delta/mu regularization making the multiplier's sign and the primal
direction mutually inconsistent — POSSIBLE, NOT SHOWN, and partly
disfavoured.** `primal_delta` and `dual_mu` default to `1e-8`; the drop
multiplier is `1.1748e-3`, five orders above that footprint, so the *drop
decision* is not a regularization artifact. Whether the *direction* is one
cannot be decided from this trace: it records neither the effective delta nor
any KKT residual. The header's own degeneracy note — "the delta/mu
regularization keeps the KKT matrix factorizable, and the dependent rows split
the multiplier between them" — makes this a live possibility at a degenerate
vertex, and nothing here refutes it.

**(iii) A multiplier computed on one working set and a direction on another
("one working set stale") — NOT CONSISTENT as the cause.** The header's
staleness note is about the **exported** `z` on a `kMaxIter` exit, not about the
walk's decisions. Inside the loop, `price()` runs at the top of each pass on
the current set and `drop_worst` consumes those multipliers in the same pass;
the next pass rebuilds the EQP on the new set. The trace corroborates it: every
drop pass's `ws_sig_in` is the previous add pass's `ws_sig_out`, with no gap.
The multiplier and the direction are each consistent with their own set; that
they belong to *different* sets is the design of an active-set method, not a
bug.

**(iv) Tolerance asymmetry between the drop test and the blocking test — NOT
the cause, and the trace is decisive.** Neither side of the cycle is a
tolerance call. The drop clears `opt_tol` by a factor of `1.17e6` (STEENBRB)
and `1.49e16` (YORKNET). The block is not a comparison against a tolerance at
all: `ratio_test` computes `min(0, lower(i) − x(i)) / p(i)`, and the add branch
sets `x(i) = lower(i)` **exactly**, so the numerator is exactly zero and the
ratio is `-0.0` for any `p(i) < -1e-12`. Widening either bar does not touch it.

### The one quantity that would discriminate — and the observer does not have it

The **inertia verdict on the post-drop working set**, recorded per pass. It is
already computed on every one of these passes by `eqp_candidate`; recording it
is one `int` per record and zero extra arithmetic, and it separates (i) from
(ii) directly: a `kWrong` verdict on the 21-member set says the reduced Hessian
is indefinite there, and a `kOk` one says the saddle story is wrong and the
regularization/conditioning story is what is left. The curvature `pᵀHp / pᵀp`
together with the `RideOutcome` is the near-equal second choice — also already
computed on exactly these passes, and it would additionally say *why* the ride
declined. **The current observer records neither.**
[*SUPERSEDED (fix1): fix round 1 added all of them — the verdict, its triple,
the expectation, `pᵀHp`, `pᵀp`, the granular ride outcome and forty more fields
— and re-traced the same two first QPs under the identity gate. The answer is in
Re-trace (G): STEENBRB `kWrong` with 22 excess negative eigenvalues, YORKNET
`kOk` with the inertia exactly as expected. The two cells do NOT have the same
cause.*]

One thing worth being clear about: **the sign of `p` on the released variable
is already determined by the existing trace.** On the add pass the blocker's
`block_index` equals the just-dropped index, `block_side` is `lower`, and
`raw_ratio` is `-0.0`; per `ratio_test` those three together require
`p(189) < -1e-12`. That was the settler's candidate for the missing quantity,
and it is not the gap.

---

## What this trace does NOT show

* **It does not show the inertia of the post-drop KKT**, the curvature along
  `p`, or why the ride declined. Those are the (i)-vs-(ii) discriminators and
  they are UNOBSERVED here.
* **It does not carry `x` itself**, only `‖x‖∞` and `‖x‖₁` — a deliberate Task A
  choice, and `RULE.md` §1.2 declared the consequence in advance: a norm match
  is *necessary* for "the same point", not sufficient. The rule handles it by
  also requiring that nothing in the recurrence window moved the iterate, and
  reporting the resulting displacement bound (1.83e-06 and 3.02e-07 on the
  first recurrences). For the **final** cycle the point is moot in practice:
  every recorded field repeats byte-for-byte, so the two alternating states are
  not merely close in norm.
* **It is ONE QP per cell** — the first subproblem the driver builds at major 0,
  cold (`warm = false`, incoming signature 0, incoming size 0). `RULE.md` §5.6's
  comparison against later majors' warm hand-off is DECLINED BY SCOPE, not a
  gap; M3a already showed YORKNET's later majors replaying byte-identical
  counter records, and testing that hand-off needs its own task.
* **It is two cells, both TRAIN.** Nothing here generalizes to the suite, and
  HOLDOUT was not run.
* **It says nothing about wall-clock.** No timing in this artifact supports any
  claim (CLAUDE.md §7).
* **It does not reopen M3a.** `RULE.md` §7: the `MIXED` verdict on H-DEG stands
  exactly as committed. What this task answers is a different hypothesis,
  H-MECH — *which* of the six mechanisms the minors are made of.

---

## Two honest notes about the rule itself

**The first recurrence is weaker evidence than the final cycle, and the rule
reports the weaker one.** `RULE.md` §3.1 defines the first recurrence as the
least `j` with a matching earlier `i`; on STEENBRB that is `(22, 24)`, a pair
whose window moved nothing but which the walk then *escaped* — the `move_only`
pass at minor 24 moves `x` by 0.092. The predicate is satisfied honestly (a
state did recur, with no motion in between) and 4 628 of the recurring pairs
satisfy it, but the strong claim — that the walk is trapped — rests on the
final segment, where 4 621 consecutive passes repeat every recorded field. That
segment is an **outside-the-rule** read-out (`tables/outside-the-rule.txt`),
not a predicate. The rule's answer and the stronger fact point the same way;
they are not the same statement, and this file does not merge them.

**Where `RULE.md` left a choice open, the classifier made one and says so.**
When several earlier passes match, it takes the **most recent** as the
recurrence's partner (shortest period, and the partner most likely to satisfy
the no-move conjunct, so checking it is sufficient for that existential). An
absent `step_tol` is read as zero, the strictest reading; the count of such
passes is reported and is 0 on STEENBRB, 1 on YORKNET. Both choices are stated
in `wrappers/classify_walk_m3b.py` at the point they are made.

---

## `RULE.md` §8's remedy mapping — A RECOMMENDATION ONLY

Restated from the rule, binding on nothing. Each is a candidate for a
**separate task** with its own pre-registered rule and its own gates; funding
any of them is the settler's call. Algorithmic references are open-literature
**papers only** (CLAUDE.md §6).

The rule maps `CYCLING_PROPER` to **a lexicographic or symbolic-perturbation
anti-cycling rule**. Three candidates are worth separating, because they act on
different parts of what the trace shows:

1. **Anti-cycling by perturbation (the rule's own mapping).** A lexicographic
   or symbolic perturbation of the bounds breaks exact ties in the ratio test
   and gives a finite-termination guarantee. It would certainly break this
   two-cycle.

2. **EXPAND-style working-tolerance expansion** (Gill, Murray, Saunders &
   Wright 1989 — **the paper only**). `RULE.md` §8 maps this to *long zero-step
   sequences without exact recurrence*, which is **not** what these cells
   returned, and it is worth saying plainly how well it would fit anyway:
   EXPAND lets a small feasibility tolerance grow so the blocking ratio becomes
   `(0 + δ)/|p_i| > 0` and every iteration takes a strictly positive step,
   which does break an exact two-cycle. But it does not touch **why** the
   direction points back through the bound just released, and with
   `‖p‖∞ ≈ 1.5e4` the step it buys is `~δ/1.5e4`. The likely effect on these
   cells is to convert a bit-exact 2-cycle into a very slow drift that still
   exhausts the cap. **It is a termination guarantee, not a remedy for the
   curvature** — a reader who takes EXPAND as the answer here should know that
   is what they are buying.

3. **Forbidding the immediate re-add at zero step**, unless the direction is
   recomputed: if the ratio test's blocker IS the constraint the previous pass
   released and `α ≈ 0`, the drop achieved nothing and the pair should not be
   repeated. This is the narrowest rule that matches what the trace shows, and
   `RULE.md` §8 already lists it under `IMMEDIATE_READD` as **a diagnostic arm,
   not a production rule** — which is the right status for it: it suppresses
   the symptom without deciding what the walk should do instead.

4. **Curvature handling — the candidate that addresses the cause.** The engine
   already computes everything the diagnosis needs on exactly these passes: the
   inertia verdict, `pᵀHp`, and a ride whose feasibility veto fires precisely
   here and whose decline is then discarded. Making "the ride declined because
   the descending direction re-enters the constraint just released" an
   actionable fact — rather than a fall-through to the ordinary step — is a
   change to §4c, not to the ratio test. **A cheap first step that is not a
   behaviour change at all: record the inertia verdict and the ride outcome in
   the observer and re-run these two legs.** That settles (i) versus (ii)
   before anyone designs a fix.

**No verdict in this artifact licenses any of the four.** `RULE.md` §0.

---

## Corrections (fix1)

A review of this artifact (2026-09-21, Codex sol) returned one critical and
three important findings. All four were accepted by the settler. This section
records every correction, **including every withdrawal**, against the text
above. `RULE.md` is **not edited**, the committed rows and traces are **not
edited**, and the verdict `CYCLING_PROPER` on both cells **stands**.

### C1 — WITHDRAWN: "§4c's ride cannot act by construction"

The claim was **wrong**. `ride_sign` (`src/qp/qp_engine.cpp`) picks the sign by
**descent first**: `slope < −slope_tol` gives `+1`, `slope > +slope_tol` gives
`−1`. Only then does the feasibility veto ask whether that sign steps back into
the constraint just released. So when the EQP direction `p` is an **ascent**
direction the ride selects `−p`, and `−p` both descends and moves away from the
released bound — the veto does not fire and the ride steps.

That is not hypothetical. STEENBRB's own trace does it twice, at minors 35 and
45: `ride_outcome = stepped`, `gᵀp = +1.68e-04` and `+2.23e-04`, sign `−1`,
`α = 0.0771` and `0.1096`, moving the iterate by `862` and `1127`. The ride is
hven's anti-saddle machinery working as designed, on this very cell.

What the original paragraph got right, and what stands: when the ride declines,
the engine discards *why*. `QpEngine::RideOutcome` folds all three refusals into
`kDeclined`. That is exactly the gap fix round 1 closed.

**The general form of the error, which matters more than the instance:** an
ordinary zero-step add proves only that the ride **did not step**. It never
proves **why**. The M3b text inferred a mechanism from the absence of a record,
and there was no record to infer from.

### C2 — NARROWED: "a convex EQP could not return `p_i < 0`"

True, but only of the **exact, unregularized, full-rank** KKT identity

```
z_i · (a_iᵀ p)  =  −pᵀ H p
```

which for a bound is `z_i p_i = −pᵀHp`. hven does not solve that system. It
solves one regularized by `primal_delta` and `dual_mu` (both `1e-8` effective on
every tail pass of both cells), in finite precision, on a working set whose row
rank is **not measured anywhere in this artifact** — `RULE-G.md` §0 marks rank
evidence for the released constraint ABSENT and refuses to supply it.

The identity is nevertheless worth checking, and the re-trace makes it
checkable. Read-out (e) of `tables-fix1/<cell>-outside-the-rule.txt` does so,
with every premise stated:

| | STEENBRB | YORKNET |
|---|---|---|
| `z_i` (bound at lower, so `z_i = −violation`) | `−1.1748e-03` | `−1.4895e+07` |
| `a_iᵀp` = `p_released` | `−1.4569e+04` | `−44.418` |
| LHS `z_i · a_iᵀp` | `+17.116` | `+6.6161e+08` |
| RHS `−pᵀHp` | `−2.0257e-05` | `−3.7929` |
| LHS / RHS | `−8.45e+05` | `−1.74e+08` |

Both **disagree in sign** and by five and eight orders of magnitude. On
STEENBRB the premise of positive-definiteness is refuted directly by the
recorded inertia, so the identity has no claim on the result. On YORKNET the
inertia is exactly as expected and the identity still misses by `1.7e8`, which
leaves the other two premises — exactness (regularization, precision) and the
requirement that `x` be an exact KKT point of the pre-drop set. The second is
measurable here and the measurement is unfavourable: YORKNET's iterate sits
`1.92e-02` off the constraints its own EQP holds at equality.

### C3 — STANDS, as a VERIFIED DESIGN FACT: the inertia verdict is computed
every pass and consulted at three places only

Re-verified against `src/qp/qp_engine.cpp`: the loop acts on the verdict at
`iter == 0` (the temporary-vertex start repair), in the terminal branch, and in
the suspect-stall gate inside that branch. On a tail drop pass `drop_worst`
succeeds, so the terminal branch is never reached; on a tail add pass the
verdict is not read.

This is stated as a **design fact**, not as a proved defect. Whether a walk
should keep taking Newton-type EQP steps on a working set its own gate has just
judged `kWrong` is an engineering question for the settler, and the re-trace now
supplies the fact the question needs: on STEENBRB the verdict is `kWrong` on
**every** pass of the walk, with 21–22 excess negative eigenvalues.

### C4 — NARROWED: "the same direction, a bit-exact two-cycle"

The trace records `‖p‖∞` and norms of `x`, not the vectors. The defensible
claim is: **every RECORDED field repeats in an exact zero-motion working-set
period-2 tail.** The working-set signatures are exact 64-bit values and the
zero-motion conjunct is separately recorded, so "the same working set, at the
same point, for 4 621 passes" is sound; "the same direction" is sound only as
"the same `‖p‖∞`, to the last bit."

### C5 — ADDED: four candidate causes the M3b text missed

1. **Dantzig selection by largest violation, with no regard for post-drop
   usability.** The drop rule picks the largest sign violation and nothing
   else. The tail's drop pass has **10** candidates on STEENBRB and **19** on
   YORKNET; the chosen one is rank 1 by violation on both (checked, not
   assumed). Whether any of the others would have given a usable post-drop
   subspace is **UNMEASURED** — it needs one extra factorization per candidate
   and `RULE-G.md` §0 declines to compute it. On STEENBRB the choice is
   nearly arbitrary: rank 1 beats rank 2 by a **relative `6.67e-08`**
   (`1.1748355559e-03` against `1.1748354776e-03`), a margin far below the
   pass's own relative stationarity residual of `6.71e-06`.
2. **Border/Schur solve accuracy.** **This first QP's own
   `border_refine_steps` is 18 904**, not the 843 016 the review quoted — that
   figure belongs to a different leg, and the correction is made here rather
   than carried. 18 904 over 4 680 minors is ~4 per pass, and the tail's two
   states spend exactly 3 (state A) and 5 (state D) in the main call, well
   under the compiled cap of 10.
3. **Rank dependence of the released bound.** Unmeasured, and deliberately so.
4. **Regularization.** The effective `primal_delta` and `dual_mu` are recorded
   now and are `1e-8` on every tail pass of both cells — §4b's suspect ladder
   never escalated (`suspect_escalations = 0`).

### C6 — `RULE.md`'s own defect: `P_imm ≥ 0.60`'s null derivation is INVALID

The derivation assumed an **independent / random Dantzig pick** and used
`n − me` as the droppable-candidate count. Neither holds: the Dantzig rule is
deterministic, and the candidate count is the sign-violating subset, which the
re-trace shows to be 10 and 19 — a candidate set of one would make the null
exactly 1 and the bar vacuous.

**It changes no outcome.** `CYCLING_PROPER` holds independently, has precedence
over `IMMEDIATE_READD`, and is what `verdict.txt` returned. **`RULE.md` is not
edited** — a pre-registered rule's bytes are the pin, and a defect in it is
reported, not repaired after the fact. `RULE-G.md` §5 states explicitly that it
does not repeat the mistake, and its own share null is stated as "no majority"
rather than as an equiprobable draw for exactly this reason.

### C7 — Minors

* **Record-per-pass.** "One record per minor iteration" holds for a
  **non-truncated** capture whose record allocations succeeded. The qualified
  wording is now in `include/hven/detail/qp/qp_walk_trace.h` and
  `docs/testing.md`, and `RULE-G.md` §2.2 aborts a leg on either
  `truncated` or `allocation_failed`.
* **The process deviations recorded in the M3b ledger stay deviations, not
  precedent.**
* **The 64-bit signature collision figure** (`4680² / 2⁶⁵ ≈ 5.94e-13`, in
  `RULE.md` §3.1's derivation) is a **uniform-hash heuristic, not a bound**. It
  assumes the additive SplitMix64 signature behaves like a uniform random 64-bit
  hash. It is not deterministic, and no outcome should be rested on it. The
  figure is left in `RULE.md` unedited, with this qualification recorded here.

### C8 — The process note this round earned

**A declaration is derived by diffing the task's OWN `base..head`, and the
derivation is stated in the runner.** It is never inherited from a previous
round's measured shape. Fix round 1's gate runner declared `DEBUG_ONLY=3` from
M3b's measured numbers and measured 1 — because M3b's base predates the seam
`#include` this round's base already carries. The substance was better than
declared; the declaration was still wrong, and a declaration that is wrong for a
good reason is still a failed declaration.

**Every path a runner hands a sibling script is a path that runner is
responsible for creating.** The same round's chain passed `$S/dbg` to a script
that builds `$S/gates-dbg`; the leg found no configured tree and the fold
fail-closed on the skip.

---

## Re-trace (G)

> ### READ THIS BEFORE ANY OF IT (added fix2)
>
> **Fixing this `tr_init = inf` first-QP behaviour addresses LITTLE of the
> shipped default-option failures directly.** Everything below is a trace of the
> first QP at an INFINITE trust region, which is the configuration that makes
> the walk's own pathology visible. It is not the configuration the solver
> ships. At the default radius the QPs are DIFFERENT finite-box subproblems —
> every bound, every blocker and the whole trajectory change — and they
> terminate `infeasible` rather than exhausting the iteration cap. What happens
> next is what the shipped failures actually are: STEENBRB fails through an
> objective-blind restoration excursion, and YORKNET's restoration sub-solve
> returns `infeasible` without moving (M2 / M3a). Better QP robustness may help
> shared code paths eventually, but **nothing in this re-trace shows that
> repairing the walk repairs either default failure, and "remove the window" is
> not a sufficient basis for a remedy.**

Pre-registered rule: [`RULE-G.md`](RULE-G.md), committed at `f916e82d` **before
any leg of the re-trace ran, the identity legs included**. Machine-written
verdict: [`verdict-G.txt`](verdict-G.txt). Non-vacuity transcript:
[`fold-cause-G.txt`](fold-cause-G.txt) — 30 folds, 0 failed, 9 distinct
landings. Rows, gates, traces and log:
[`rows-fix1/`](rows-fix1), [`gates-trace-fix1/`](gates-trace-fix1),
[`traces-fix1/`](traces-fix1), `logs/chain-trace-fix1.log`. Read-outs:
[`tables-fix1/`](tables-fix1).

### The result, in plain language

**The two cells do not have the same cause, and the difference is not subtle.**

**STEENBRB is an indefiniteness story, and the machinery that exists to handle
it ran and refused.** On every one of the 2 310 add passes of the tail, the
engine's own inertia test says the system carries **22 more negative
eigenvalues than it should**. The curvature gate **fires** — `pᵀHp / pᵀp` is
`6.67e-14` against a tolerance of `1.04e-07`. §4c's negative-curvature ride is
therefore offered the direction, and it declines, for one named reason on all
2 310 passes: `gᵀp = −2.09e-03` against a slope tolerance of `5.20e-06` makes
`+p` the only descending sign, and `+p` moves `−14 568.88` **through** the bound
just released. Descent and release-feasibility disagree. The rule returns
**`RIDE_VETOED`**.

**YORKNET is not an indefiniteness story at all.** Its inertia is **exactly**
what it should be — `(256, 256)` against an expectation of `(256, 256)` — on
both tail states. The curvature gate stays **silent**, correctly: `pᵀHp = 3.79`
is comfortably positive, so the direction is not a negative-curvature one and
the ride is never attempted. What is wrong is the direction's provenance: the
iterate sits `1.92e-02` off the constraints its own EQP holds at equality,
which is **1.27e+05 times** the pass's own step tolerance. The rule returns
**`NUMERICAL_DIRECTION`**, sub-reason `RESIDUAL`.

### The verdict, as the rule returns it

| cell | outcome | sub-reason / named veto | form | state-A passes |
|---|---|---|---|---|
| **STEENBRB** | **`RIDE_VETOED`** | `sign_disagreement_veto` | STATE | 2 310 / 2 310 |
| **YORKNET** | **`NUMERICAL_DIRECTION`** | `RESIDUAL` (both arms) | STATE | 1 191 / 1 191 |

Both legs passed §2.1 (identity gate, 21 fields, DIFFERING 0), §2.2 (integrity,
`truncated=0`, `allocation_failed=0`, `records == minor_iters`), §2.3
(neutrality: five named fixtures `Passed` in both configurations) and §2.4. Both
tails are §3's two-state shape (`t₀` 59 and 177; `|T|/N` 0.9874 and 0.9309), and
§5's homogeneity **holds on all 16 fields in both states of both cells** — so §4
ran in its STATE form and each outcome is a single repeated value reported with
its repetition count, never dressed as a proportion.

§7.0, reported either way: **44 of 45** added fields repeat with period 2 over
STEENBRB's tail — the exception is `pass_schur_updates`, `1` on 4 620 passes and
`0` on one, at the tail's own boundary — and **45 of 45** over YORKNET's.

### OBSERVED, on the tail's two states

| | STEENBRB A | STEENBRB D | YORKNET A | YORKNET D |
|---|---|---|---|---|
| `inertia_verdict` | `2` (`kWrong`) | `2` | `0` (`kOk`) | `0` |
| observed / expected | `(441,135)` / `(463,113)` | `(441,135)` / `(462,114)` | `(256,256)` / `(256,256)` | `(255,256)` / `(255,256)` |
| excess negatives | **22** | **21** | 0 | 0 |
| `eqp_path` | `border_schur` | `border_schur` | `eliminated` | `eliminated` |
| `eqp_call_refine_steps` | 3 | 5 | 0 | 0 |
| `curvature` / `curvature_tol` | `6.67e-14` / `1.04e-07` | — | `1.63e-04` / `1.49e-10` | — |
| `curvature_fired` | **true** | false | **false** | false |
| `ride_outcome` | `sign_disagreement_veto` | `not_attempted` | `not_attempted` | `not_attempted` |
| `gᵀp` / `slope_tol` | `−2.09e-03` / `5.20e-06` | — | `+2.31e+03` / ABSENT | — |
| `p[released]` | `−1.4569e+04` | — | `−44.418` | — |
| `stat_free_inf / stat_scale` | `6.71e-06` | `6.71e-06` | **`4.08e-01`** | `3.74e-16` |
| `active_res_inf / x_inf` | `1.10e-08` | `1.10e-08` | **`1.27e-04`** | `1.27e-04` |
| `primal_delta`, `dual_mu` | `1e-8`, `1e-8` | `1e-8`, `1e-8` | `1e-8`, `1e-8` | `1e-8`, `1e-8` |
| drop candidates | — | **10** (tied 1) | — | **19** (tied 1) |

`inertia_zero` is `0` on every row above and is **DERIVED** (`dim − pos − neg`,
which is what MKL forces), so it is *not* an observation that there are no zero
eigenvalues. `RULE-G.md` §4.0 says so and the classifier honours it.

### A DEFECT OF `RULE-G.md`, declared; the outcome stands

`RULE-G.md` §4.4's `RESIDUAL` arm reads `stat_free_inf / max(stat_scale, 1)` on
**state A**, which is a **post-drop** pass. That quantity is large by
construction there, and the rule did not notice: `stat_free_inf` is the
stationarity residual over the **free block**, and the variable the previous
pass released is free, so its own dual — the very multiplier that justified the
drop — enters the residual. The trace measures the confound directly. On
YORKNET's `eliminated` path the relative stationarity residual is `3.74e-16` on
the 1 322 non-post-drop passes and `4.08e-01` on the 1 231 post-drop ones; the
released multiplier is `1.49e+07`.

Per `RULE-G.md`'s own preamble a badly-posed predicate is **reported as a defect
of the rule, with the outcome it produced left standing.** So:

* The outcome `NUMERICAL_DIRECTION` on YORKNET **stands**.
* The **stationarity arm is confounded** on a post-drop pass and should not be
  read as a statement about solve accuracy.
* The **active-row arm is not confounded** and is what carries information
  here. It measures how far the iterate sits off the constraints the EQP holds
  at equality; it is bit-identical on both tail states (the tail does not move
  the iterate); and it separates the two cells cleanly — `1.10e-08` relative on
  STEENBRB, **`1.27e-04` on YORKNET**, four orders apart, with only YORKNET's
  above the `1e-06` bar. STEENBRB's stationarity arm would also have crossed the
  bar (`6.71e-06`) had the cell reached §4.4, and its released multiplier is far
  too small to be the cause of that — so the unconfounded reading is that
  STEENBRB's own solve residual is itself four orders above the mechanism's
  `1e-10`.
* A future rule should read the stationarity residual on the **pre-drop**
  state, or restrict it to the variables still pinned.

### OUTSIDE THE RULE — six read-outs, none of them a verdict

`tables-fix1/<cell>-outside-the-rule.txt`, in full. The six, in brief:

**(a) Was the first working set already indefinite, or did the drops create
it?** **Already indefinite, on STEENBRB, before a single constraint was
added.** At minor 0 the working set is **empty** (`ws_size_in = 0`,
`border_dim = 0`), the expectation is exactly `(n, m_e) = (468, 108)`, and the
backend reports `(441, 135)` — **27 excess negative eigenvalues in the cold
equality-constrained KKT matrix itself**. The first six bound additions absorb
six of them and the excess then sits at 21–22 for the remaining 4 674 passes;
the observed triple `(441, 135)` is **constant over all 4 680 passes**. The
cycling did not manufacture the indefiniteness. YORKNET's excess is 1 at
minor 0 and 0 on 2 549 of its 2 560 passes.

What the engine compares on the border path is K0's backend triple against a
structural expectation the Schur complement's Haynsworth contribution has been
backed out of (`border_inertia_verdict`). The excess above is therefore the
engine's own indefiniteness signal read exactly as the engine reads it — not an
independent measurement of the current reduced Hessian, which the observer does
not take.

**(b) Did the same veto occur on the pre-tail pairs that escaped, and what
differed?** Yes, three times (minors 29, 39, 57), identical in mechanism. Over
the **whole** STEENBRB walk there are 2 322 post-drop passes:

| ride outcome | sign of `gᵀp` | count |
|---|---|---|
| `sign_disagreement_veto` | `gᵀp < 0` | **2 313** |
| `nullspace_veto` | either | 7 |
| `stepped` | `gᵀp > 0` | **2** |

**The sign of `gᵀp` is the whole difference.** Every pass that rode had `p` as
an **ascent** direction, so `ride_sign` chose `−1` and `−p` both descended and
left the bound. Every pass that was vetoed had `p` descending, so the only
admissible descent sign was `+1`, which re-enters the bound. There is no pass in
the trace where a descending `p` was ridden.

The pre-tail pairs escaped by a *different* route: after each zero-step re-add
came a `move_only` pass that moved the iterate by `0.06`–`0.15`, after which the
Dantzig rule picked a different bound. There are **13** such passes in the whole
walk and the last is at minor **58** — one before `t₀`. The 2-cycle closes
exactly when the intervening `move_only` disappears.

**(c) YORKNET's residuals over the walk.** By path and post-drop status:

| `eqp_path` | post-drop | n | `stat_rel` min / med / max | `act_rel` min / med / max |
|---|---|---|---|---|
| `border_schur` | no | 7 | `1.00` / `1.00` / `1.00` | `12.3` / `12.3` / `12.3` |
| `eliminated` | no | 1 322 | `1.78e-18` / `3.74e-16` / `1.57e-05` | `4.34e-05` / `1.27e-04` / `12.3` |
| `eliminated` | yes | 1 231 | `1.78e-18` / **`4.08e-01`** / `4.08e-01` | `4.34e-05` / `1.27e-04` / `1.62e-01` |

**Does `p[released] < 0` ever coincide with a small residual?** In this trace,
no: all 1 191 passes with `p[released] < 0` carry `stat_rel = 4.08e-01`, while
37 of the 40 with `p[released] ≥ 0` carry `stat_rel ≤ 1e-06`. **That association
is confounded** — every `p[released] < 0` pass is a tail pass, and the residual
is high on post-drop passes for the structural reason above. It is reported
because it was asked for, not because it separates anything.

**(d) The drop-candidate set on the tail's drop pass.** STEENBRB: 10 candidates,
8 recorded (the cap), chosen rank **1**, tie count 1, margin over rank 2
`7.83e-11` absolute and `6.67e-08` relative. YORKNET: 19 candidates, 8 recorded,
chosen rank **1**, margin `1.69e+06` absolute and `11.4 %` relative. **The same
candidate set recurs exactly**: one distinct candidate string across all 2 311
(STEENBRB) and all 1 192 (YORKNET) tail drop passes; 13 and 41 distinct strings
across the whole walk.

**(e) The exact-arithmetic drop identity**, with every premise stated — the
table in C2 above.

**(f) §4b's iter-0 temporary-vertex start repair.** hven already has the
machinery an inertia-controlling method needs: at `iter == 0`, on a `kWrong`
verdict, `repair_temporary_vertex` pins free variables at bounds in order of
most negative Hessian diagonal, probing the inertia after each, until the
verdict is `kOk` — **and reverts entirely if it never is**.

* **YORKNET**: it RAN and SUCCEEDED. Minor 0's recorded action is
  `start_repair`, the excess falls from 1 to 0 at minor 1, and the walk proceeds
  from a certified working set.
* **STEENBRB**: OBSERVED — minor 0's verdict is `kWrong` and the action is
  `add`, not `start_repair`, and that pass spent **4 factorizations** (4 of the
  solve's 5). INFERRED, from the guard being a conjunction — the repair **was
  called and returned false**: it could not certify a `kOk` working set by
  pinning variables at bounds, so it undid its pins, and the walk began from a
  working set the engine had just judged wrong by 27 negative eigenvalues.

No `start_repair`, `post_probe_restart` or `suspect_escalation` occurs anywhere
else in STEENBRB's 4 680 passes.

### `RULE-G.md` §8's remedy mapping — A RECOMMENDATION ONLY, per cell

Restated from the rule and binding on nothing. **No outcome here licenses a
behaviour change, a default change, or a line of code.** Each item is a
candidate for a separate, separately-briefed task with its own pre-registered
rule. Algorithmic references are **open-literature papers only** (CLAUDE.md §6).
Wall-clock is informational only; the cost notes below are counter-based.

#### For STEENBRB — `RIDE_VETOED`, on a working set with 22 excess negatives

*What the verdict licenses: nothing. What it establishes: the gate fired, the
ride was offered the direction, and it refused for one named reason on all
2 310 tail passes. What it does NOT establish: that repairing the ride would
fix the cell, or that the direction is trustworthy.*

1. **Inertia-controlling working-set methods** (Gill, Murray, Saunders &
   Wright; Gill & Wong — **papers only**) are the cause-level candidate, and
   the honest thing to say about them here is what they **require**: they
   maintain second-order consistency of the working set, which means they must
   **start** from a second-order-consistent one. Read-out (a) says STEENBRB's
   cold start is 27 excess negatives away from that with an empty working set,
   and read-out (f) says hven's existing repair for exactly this — pinning
   variables at bounds until the inertia is right — **ran and could not get
   there**. So adopting an inertia-controlling method is not a drop-in change
   to the walk: it needs a start that works. A vertex-type start (pin enough
   variables that the reduced Hessian is positive definite, then release them
   one at a time, never losing the property) is the classical answer, and
   **why `repair_temporary_vertex` fails on this cell is the first thing to
   find out** — it is cheap, it is already instrumented, and it gates
   everything else in this row.
2. **Repairing the ride's sign logic** is the targeted candidate, and it should
   be entered with eyes open. The sign test is **not** broken — C1 withdrew
   that claim, and the ride demonstrably works when `p` ascends. The real
   question is whether a ride makes sense **at all** with 22 negative
   directions present: the ride rides the **EQP direction**, which here is a
   near-zero-curvature direction of norm `1.46e+04` (`pᵀHp = 2.03e-05`,
   `pᵀp = 3.04e+08`) that happens to point back through the released bound. It
   is not *a* negative-curvature direction chosen for the purpose. The work
   that would actually help is **constructing or projecting a feasible
   negative-curvature direction** — one in the retained working set's nullspace
   whose sign satisfies descent and release-feasibility together — rather than
   accepting or rejecting whatever the EQP returns (Fletcher; Gould — papers
   only). With 22 negative directions available, that such a direction exists is
   plausible; that this one is it, is not.
3. **SQP-level convexification or a modified Hessian** changes the exact-Hessian
   model and is a **separately registered policy question for the owner**, not a
   QP fix. It is the one remedy that would make the cold start
   second-order-consistent by construction, which is precisely why it is a
   policy question and not a tactic: it changes what problem is being solved.
4. **Stopgaps** — taboo, lexicographic or EXPAND-style anti-cycling — would stop
   this 2-cycle and repair nothing. Read-out (d)'s STEENBRB margin
   (`6.67e-08` relative between the top two candidates) means a taboo rule would
   likely just hand the drop to bound 205 and produce a different cycle.

**Cost, which is part of the recommendation and no part of the verdict.** hven
pays dense Schur/border maintenance per add and per drop. An inertia method that
adds temporary constraints needs incremental updates or batched rebuilds. Set
against that: one valid ride replacing 2 310 drop/re-add pairs would be an
overwhelming net reduction.

#### For YORKNET — `NUMERICAL_DIRECTION`, sub-reason `RESIDUAL`

*What the verdict licenses: nothing. What it establishes: the inertia is as
expected, the curvature gate is correctly silent, and a residual the rule reads
is above its bar. What it does NOT establish: that the linear algebra is at
fault — the stationarity arm is confounded (see the declared defect above), and
`REFINEMENT_SATURATED` explicitly did **not** fire.*

The unconfounded facts, and what each points at:

1. **The iterate is `1.92e-02` off its own active set**, on both tail states,
   against a step tolerance of `1.51e-07` — a factor of `1.27e+05`. An EQP whose
   "active" constraints are satisfied to `1e-02` is not solving the problem the
   drop rule priced. **Where the drift comes from is the first question**:
   accumulated `x` updates, the homotopy's shifts, or the eliminated path's
   pinned-variable assignment.
2. **The `eliminated` path does no iterative refinement, and its recorded
   `eqp_call_refine_steps = 0` is a constant, not a convergence report.**
   `solve_eqp` takes one factorize, one solve and one refinement step, then sets
   `refine_steps = 0` by definition (`eqp_solve.h`: "no iterated loop on this
   path"). So YORKNET's `0` must **not** be read as "refinement converged" — it
   is "this path has no refinement loop to report". That asymmetry with the
   bordered path is worth a look on its own; `RULE-G.md`'s
   `REFINEMENT_SATURATED` predicate can structurally never fire on this path.
3. **The border machinery was latched off and never came back.** On every tail
   pass `border_attempted = true`, `border_latched = true`,
   `border_needs_refactorization = true`, and the path is `eliminated` — a
   fall-back, not the elimination mode chosen on its merits. Each pass pays
   **2 factorizations and 56–57 Schur updates**; the solve's totals are 5 106
   factorizations and 144 684 Schur updates. That is where YORKNET's cost is.
4. **Nothing checks the EQP solve's residual before the step is taken.** The
   loop accepts the direction the solve returns. If the direction is to be
   called untrustworthy, the check that would say so does not exist yet, and
   adding one is a *diagnostic*, not a remedy.

The remedies in rows 1 and 2 of STEENBRB's list are **not** indicated for
YORKNET: its inertia is right and its ride is correctly not attempted. Applying
an inertia-controlling method here would be work spent on a condition this cell
does not have.

---

## Corrections (fix2)

The scoped re-review of fix round 1 (`.superpowers/m7-w2-m3b-fix1-review-sol.md`,
FIX-ROUND, 0 critical) raised three Important findings and three minors. The
settler accepted all of them. **Both formal verdicts STAND as returned** —
`CYCLING_PROPER` ×2 from `RULE.md`, and `RULE-G.md`'s outputs — and neither
`RULE.md` nor `RULE-G.md` is edited: a pre-registered rule's bytes are the pin,
and a defect in one is recorded, never repaired after the fact.

### C9 — `NUMERICAL_DIRECTION` is the RULE'S OUTPUT for YORKNET, not a finding about its CAUSE

`RULE-G.md` returns `NUMERICAL_DIRECTION` on YORKNET and that output is retained
as the pre-registered rule's formal result. **It must not be carried into the
ledger as an established cause**, because the arm that produced it does not
carry the meaning the fix1 report gave it. Four separate reasons, each
independently sufficient:

1. **It is measured at the wrong point.** `active_res_inf` is taken at the
   pass's STARTING `x` (`qp_engine.cpp` ~:704), not at the EQP solution. A
   quantity evaluated before the direction exists says nothing about the
   accuracy of that direction.
2. **Its normalization is not a backward-error normalization.** Dividing by
   `max(x_inf, 1)` omits row and right-hand-side scale entirely.
3. **The equations themselves carry `−dual_mu·I`** (`eqp_solve.h`), so a
   non-zero residual against the UNREGULARIZED rows is the regularization's own
   footprint as much as it is error.
4. **The `1.27e-4` level holds throughout the walk**, not just in the tail, so
   it cannot explain why bound 291 is immediately re-entered. It is persistent
   primal drift, and it is not peculiar to the passes under study.

**WITHDRAWN: `RULE-G.md`'s `1e-6` bar for the ACTIVE-ROW arm has an unsound
derivation.** `n²ε` with "modest growth" is a heuristic, not a bound, and
`active_res_inf / max(x_inf, 1)` is not the quantity such a bound would apply
to. The file itself is unedited; the withdrawal is recorded here and in the
report. The STATIONARITY arm's confound stands as fix1 described it, but the
wording is made precise: the released variable becomes FREE and its former bound
multiplier is therefore omitted, so the old bound-stationarity imbalance
reappears inside the free-block residual evaluated at the old iterate. The
released multiplier does not literally enter `free_block_stationarity` — `z` is
not one of its arguments.

**WHAT YORKNET'S ROWS DO SUPPORT**, and it is a short list:

* the regularized KKT's inertia is **exactly as expected** — this cell has no
  inertia problem and generic inertia-correction work would be spent on a
  condition it does not have;
* `pᵀHp = 3.7929 > 0` on the computed direction;
* the exact identity is violated **in sign**: `z_i·p_i = +6.616e8` where
  `−pᵀHp = −3.7929`;
* and that identity's **full-row-rank premise is FALSE BY DIMENSION**: `n = 312`,
  `me = 256`, 57 working constraints ⇒ **313 rows on 312 columns**. Global full
  row rank is impossible for the pre-drop set. Only `dual_mu` keeps the system
  nonsingular, which is also why correct inertia does not restore the missing
  premise.

**REGISTERED — the single next observation, as its own task.** A
regularization-aware, RANK-REVEALING audit of ONE tail post-drop system: form
the retained active Jacobian, report its numerical rank and smallest singular
value, and report `C_A·x_eqp − d − dual_mu·λ_A`. That one audit separates the
live alternatives — regularization-and-multiplier-scale, eliminated-solve
accuracy, and a drop that releases no numerically meaningful degree of freedom —
and no anti-cycling or Dantzig-policy change should be adopted before it.

### C10 — STEENBRB: the CONTROL-FLOW label is right, the MATHEMATICAL frame was not

`RIDE_VETOED` with `sign_disagreement_veto` as the dominant reason is the
correct label under `RULE-G.md`, and it stands. But **"a negative-curvature ride
was vetoed" is the wrong frame** for what the numbers say. The computed
direction has tiny **POSITIVE** curvature: `pᵀHp = 2.0257e-05`,
`pᵀp = 3.0360e+08`, quotient `6.672e-14`, against a curvature tolerance of
`1.0367e-07`. The gate therefore treats **"not sufficiently positive"** as
ride-worthy. The evidence says: the inertia test reports negative directions
somewhere in the system, the EQP direction that is actually computed is almost
flat but positively curved, and the ride heuristic offers that unsuitable
direction before feasibility vetoes it. **The veto may be protecting the
solver**, and changing the ride's sign logic is not indicated. What is indicated
is recovering or projecting an actual feasible negative-curvature direction.

### C11 — the constant `(441, 135)` triple is K0's, and the "excess" is an algebraic verdict

Not stale, and not a fresh per-working-set measurement either. On the border
path the printed observed triple is the **retained sparse `K0` factor's**
inertia; the dense Schur complement's current Haynsworth contribution is backed
out of the EXPECTED counts instead (`qp_engine.cpp` ~:1768), so the shared
helper compares K0's own numbers. Two consequences the fix1 text did not draw:

* at **minor 0**, where the border dimension is zero, `(441, 135)` against
  `(468, 108)` is a direct statement about the **cold equality-constrained
  KKT**: 27 excess negatives before any bordering exists;
* the later **"21–22 excess"** is the engine's verdict about the full bordered
  system, reached **algebraically**, not by factorizing each working set afresh.

### C12 — the iter-0 repair: called, returned false, and the reason was NOT recorded

The inference stands and is restated as what it is — **a code-reading
inference**, not an observation. At `qp_engine.cpp` ~:660, `iter == 0` and
`verdict == kWrong` both held; a repair that succeeded would have recorded
`kStartRepair` and continued, and the pass instead records `add`. So
`repair_temporary_vertex` was called and returned false. **False can mean only
two things**: it exhausted usable pins while still wrong, or it met a
non-`kWrong`, non-`kOk` verdict — principally `kSuspect` — and reverted
(~:2038). **There is no repair pin budget and no 128-pin repair cap**; the "128"
in the fix1 text belongs to a different mechanism. One-sided bounds are usable
(~:1967) and are not themselves a failure. Since all 468 STEENBRB variables
count as genuinely bounded under the same `1e20` convention, an early `kSuspect`
is the more plausible of the two — but **the stop reason was not recorded, and
nothing in this artifact establishes it.** Instrumenting and repairing
`repair_temporary_vertex` — every attempted pin, verdict, evidence state,
Schur/latch state and the exact stop reason — is the cause-level next step for
this cell.

### C13 — NARROWED: "the sign of `gᵀp` is the whole difference"

It separates the **2 313 sign-disagreement vetoes from the 2 rides that
stepped**, and that much is exact in
[`tables-fix1/steenbrb-outside-the-rule.txt`](tables-fix1/steenbrb-outside-the-rule.txt).
It is **not** the whole difference across all outcomes: the **7 nullspace
vetoes** occur BEFORE sign selection is reached at all.

### C14 — the minors

1. **NARROWED — "the seam's neutrality claim holds even when the allocator
   fails."** True of the RECORD STORE and of nothing wider. The scope's
   destructor catches its own `push_back`, which is the only throwing operation
   in the destructor; other observer-only work allocates too (the residual
   hooks' temporaries, the drop rule's copied-and-sorted candidate vector) and a
   throw there propagates like any other allocation failure in the engine.
   Corrected at the site, in `qp_walk_trace.h` and in `docs/testing.md`.
2. **QUALIFIED — "mutually exclusive by construction."** The cause
   classifier's outcomes are mutually exclusive **under the producer's
   invariants**, not as literally disjoint predicates over every CSV shape a
   reader could hand it. The classifier's own fold deliberately constructs a
   trace on which 4.1 and 4.2 overlap, and it REFUSES that trace with
   `MULTIPLE` — so the committed evidence is protected by a refusal, which is
   the right design, but the unqualified phrase overstated it.
3. **FIXED — the ELF section digest now REFUSES two malformed shapes it used to
   guess at.** An unsupported `EI_DATA` was treated as big-endian (so every
   header field would then be unpacked from a guessed encoding), and a
   section-name offset that is out of range or unterminated produced an empty
   name or the name table's whole tail, either of which would then have been
   COMPARED. Both are refusals in `wrappers/section_digest_fix2.py`, with folds
   in `wrappers/fold_proofs_psym_fix2.sh` PART 6 — including the control that
   the fix1 and fix2 digests are byte-identical on a well-formed object, so the
   refusals are the only change. Neither shape occurs in the compiler objects
   this check is run on, which is why the independent `objcopy` arm made it
   immaterial to the measured evidence.
4. **FIXED — the padding-mask comment contradicted the fixture.** It said an
   empty mask "degrades to the strict byte-for-byte form"; the non-vacuity
   fixture requires `padded > 0` and would FAIL on such a toolchain. Refusing is
   deliberate, and the comment now says so.

### C15 — the seam reads its inertia evidence ONCE (and no re-trace is owed)

Both verdict sites used to call `factor.inertia()` twice on a pass — once for
the production verdict, once for the trace. On MKL that is a reread of cached
session fields, but Accelerate's accessor makes a genuine, independently
fallible `SparseGetInertia` call, so a record-only seam was adding a BACKEND
QUERY, and in principle the trace could record evidence other than the evidence
the gate compared. Both sites now read once into a local that both consumers
share, inside the `#ifdef HVEN_TESTING` arm, with the `#else` arm carrying the
production statement verbatim.

**THE COMMITTED TRACES ARE NOT RE-TAKEN, and here is why that is sound.** They
were taken on **MKL**, whose `inertia()` rereads cached session fields rather
than querying the backend, so the second call returned exactly what the first
did and **their inertia columns equal the evidence each walk's own verdict was
computed from**. That premise is now a measured fixture rather than an assertion
(`QpWalkSeam.TheBackendsInertiaAccessorAnswersTheSameThingTwiceOnThisPlatform`).
The COUNT of backend inertia queries is not observable from any existing seam in
this tree — the inertia-query fault injector substitutes a return code, carries
no counter, and is Accelerate-only — so the single-read property is asserted BY
CONSTRUCTION, stated as such, rather than by inventing a counting seam.

### C16 — the concurrency fixtures now force the overlap they are about

Fix round 1's two concurrent-solve fixtures only CREATED the threads close
together, and a valid scheduler could run them one after the other, letting both
"must read 1" assertions pass on the broken observer. They now block at a
two-party start gate until both threads have built their problem and constructed
their engine, and the problems are 96 and 48 variables rather than 24 and 2, so
the solve they are released into lasts milliseconds in both configurations and
the two `QpEngine::run` calls really are in flight together. **The honest limit
is stated in the fixture and in `docs/testing.md`**: the pre-fix defect is a
DATA RACE, redness under a data race is probabilistic without a sanitizer, and
this tree has no TSan preset. `...ArmingOnOneThreadIsInvisibleOnAnother` remains
the deterministic arm. **REGISTERED for the milestone close: add a TSan preset
and run this seam's suite and the concurrent-solve suites under it.**

---

**No verdict in this artifact licenses any of the above.** `RULE.md` §0 and
`RULE-G.md` §0.
