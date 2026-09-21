# M7 W2 M3b — the first-QP per-minor trace: what IS the active-set walk doing for 4 680 minors?

Pre-registered rule: [`RULE.md`](RULE.md), committed at `d35f70f8` **before any
leg ran, the identity legs included**. Provenance:
[`PROVENANCE.txt`](PROVENANCE.txt). Machine-written verdict:
[`verdict.txt`](verdict.txt). Non-vacuity transcript:
[`fold-walk.txt`](fold-walk.txt). Raw traces: [`traces/`](traces) — committed
whole, one record per minor iteration.

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
