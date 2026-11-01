# M7 W2 M3b — RULE.md — the pre-registered classifier for the first-QP per-minor trace

Committed **BEFORE any trace leg, identity legs included**. Nothing below is
relaxed, widened, re-labelled or re-read in the light of what a leg says. If a
predicate turns out to have been badly designed, that is declared in the report
as a defect of this rule — exactly as M7 W2 M3a's settler declared the 0.5 bar —
and the verdict this rule returns still stands.

Task brief: `.superpowers/m7-w2-m3b-brief.md`. Instrument: Task A's
`include/hven/detail/qp/qp_walk_trace.h`, landed at `01a956a7`, gated at
`e891041e` (evidence in `gates/`). Prior artifact this one continues:
`../2026-09-m7-w2-m3a-request-probe/` — its `verdict-F.txt` returned **MIXED** on
both cells and did **not** show cycling proper.

## 0. What this rule may and may not do

* **No verdict below licenses a behaviour change, a default change, or a code
  change of any kind.** This is a DIAGNOSTIC measurement. Section 8's remedy
  mapping is a **recommendation to the settler and nothing more**; funding any of
  it is the settler's call, on its own task, with its own rule.
* No file under `src/` or `include/` is touched by the measurement. The
  instrument already landed and is already gated; a leg adds nothing to it.
* The two cells are `STEENBRB` and `YORKNET` at `tr_init = inf`, default options
  otherwise, **first QP only** (the solve the SQP driver builds at major 0).
  STEENBRB first, then YORKNET.
* HOLDOUT is never run. Wall-clock is informational only; the trace and the
  counters are the currency.

## 1. The object the rule is applied to

One `QpWalkObserver` capture of **one** `QpEngine::run` — the first subproblem of
the driver's solve — recorded with `capture_solve = 0`. Its members:

* per-solve: `warm`, `n`, `me`, `mi`, `max_iter`, `incoming_ws_signature`,
  `incoming_ws_size`, `exit_status`, `truncated`;
* per minor: the `QpWalkMinorRecord` fields, whose semantics are fixed by
  `qp_walk_trace.h` and by the hook sites in `src/qp/qp_engine.cpp`.

### 1.1 Derived sets, defined once and used by every predicate below

Let `P` be the recorded passes, in minor order, and `N = |P|`.

| name | definition |
|---|---|
| **DROP pass** | `action ∈ {kDropDantzig, kDropZeroProbe}` |
| **ADD pass** | `action ∈ {kAdd, kRideStepped}` **and** `block != kNone` |
| **RIDE-ADD pass** | `action == kRideStepped` (a ride that stepped always admits a blocker) |
| **identity** of a pass | the triple `(block, block_index, block_side)`; `block_side` is `kFree` for a row |
| `N_drop`, `N_add` | the counts of those two sets |

These are exhaustive of neither `P` nor each other's complement: `kStartRepair`,
`kPostProbeRestart`, `kSuspectEscalation`, `kTerminal`, `kMoveOnly`,
`kRideUnbounded` are passes that are neither, and `kUnset` is an instrumentation
bug the rule treats as an ABORT (section 2).

### 1.2 What the trace can and cannot decide, stated before it is read

The record carries `x_inf` and `x_l1` — two norms of the iterate the pass began
at — and **not** `x` itself. That was a deliberate choice in Task A (a per-minor
copy of a 468-vector for 4 680 minors is a different instrument). The
consequence, stated here rather than discovered later:

> A match on `(ws_sig_in, x_inf, x_l1)` is a **necessary** condition for "the
> same working set at the same point", not a sufficient one.

Section 3.1 therefore does not rest on the norms alone. It adds the condition
that **no pass in the recurrence window moved the iterate** — which turns the
match into a bound on the real displacement, `‖x_j − x_i‖∞ ≤ Σ_{m∈[i,j)} dx_inf
≤ (j − i) · max step_tol` — and it reports that bound with the period, so the
settler can see how tight the claim is. A recurrence over a long period with a
loose bound is reported as exactly that.

## 2. ABORT conditions — checked before any predicate is evaluated

If any of these holds, the leg's verdict is **ABORTED**, no outcome is assigned,
and the report says which condition fired and what was measured.

**2.1 The identity gate.** The traced solve's first QP-ledger record must equal
M7 W2 M3a's committed `rows/f/<cell>@F-trinf.txt` record 0 (`f_qp_0`) **on all 20
`QpCounters` fields**, plus the record's status. The 20 fields and their required
values, transcribed here from the committed M3a rows (`first_*` keys inside
`LEDGER_FIRST_BEGIN … LEDGER_FIRST_END`) so that the comparison is against a
number written down before the leg ran:

| field | STEENBRB@F-trinf | YORKNET@F-trinf |
|---|---|---|
| `factorizations` | 5 | 5106 |
| `schur_updates` | 4849 | 144684 |
| `minor_iters` | 4680 | 2560 |
| `eqp_refine_steps` | 0 | 0 |
| `border_refine_steps` | 18904 | 9 |
| `verdict_refine_steps` | 0 | 0 |
| `suspect_escalations` | 0 | 0 |
| `k0_reused` | 0 | 0 |
| `symbolic_analyses` | 1 | 1 |
| `ws_adds` | 2344 | 1287 |
| `ws_drops` | 2323 | 1232 |
| `shift_adds` | 0 | 0 |
| `degenerate_steps` | 2319 | 1257 |
| `degenerate_run_max` | 2310 | 1191 |
| `ws_adds_bound` | 2344 | 1287 |
| `ws_drops_bound` | 2323 | 1232 |
| `distinct_ineq_added` | 0 | 0 |
| `distinct_bound_added` | 23 | 95 |
| `drop_ties` | 0 | 12 |
| `ratio_ties` | 0 | 52 |
| (status) `record_status` | `max_iter` | `max_iter` |

> NOTE, so that a transcription slip cannot silently widen the gate: the
> authoritative values are the bytes of
> `../2026-09-m7-w2-m3a-request-probe/rows/f/<cell>@F-trinf.txt`, and the
> comparison is made **against that file**, key by key, by the checker — not
> against this table. This table is the human-readable declaration of what the
> checker will find; where it and the file disagree, **the file wins, and the
> discrepancy is reported as a transcription defect in this rule** — never
> resolved in the table's favour, and never by editing this file after a leg.

The checker must be **structurally non-vacuous**, on M3a fix1's hardened
template: it refuses (a) a missing or unparseable M3a row file, (b) a missing or
unparseable traced record, (c) a comparison covering fewer than 20 fields, and
(d) a field list that does not match the declared one. A checker that compares
zero fields and exits 0 is the failure mode this clause exists to forbid.

**2.2 Trace integrity.**
* `QpWalkObserver::truncated` must be `false`.
* `N` must equal the record's `minor_iters` (4680 / 2560). The scope emits one
  record per pass of the loop that increments `minor_iters`, so any other value
  means a pass went unrecorded.
* `exit_status` must be `QpStatus::kMaxIter`.
* No record may carry `action == kUnset`.
* `solve` must be 0 on every record, and `warm` must be `false` — this is the
  cold first subproblem.

**2.3 Neutrality.** The traced run's 20 counter fields are also what 2.1
compares, so neutrality and identity are the same check here. It is stated twice
on purpose: if the observer had moved a counter, 2.1 would fail, and the rule
would report an INSTRUMENT FAULT rather than a verdict.

## 3. The outcomes and their exact predicates

Seven outcomes. They are made **mutually exclusive by the precedence order in
section 4**; the predicates themselves may overlap, and every quantity below is
reported for every cell **whatever outcome wins**, so precedence decides the
label and never what the settler is shown.

Each threshold is followed by the value **the pure mechanism would produce**, and
by the value the **null** (the competing mechanism) would produce. Bars are
placed strictly inside that interval, never on either end. This is the settler's
M7 W2 M3a lesson, applied: the 0.5 bar in `RULE-F.md` sat on the value an
add/drop alternation converges to, and so decided nothing.

### 3.1 CYCLING_PROPER

Define a pass's **state** as `(ws_sig_in, bucket(x_inf), bucket(x_l1))`, where two
passes `i, j` share a bucket iff

* `|x_inf[i] − x_inf[j]| ≤ T[i,j]` with `T[i,j] = max(step_tol[i], step_tol[j])`, and
* `|x_l1[i] − x_l1[j]| ≤ n · T[i,j]`.

Let `R` = (number of passes whose state was already seen at a strictly earlier
pass) / `N`. Let the **first recurrence** be the least `j` with a matching
earlier `i`, and the **period** `j − i`.

> **CYCLING_PROPER** iff `R ≥ 0.20`
> **and** there is a recurring pair `(i, j)` for which **every** pass in `[i, j)`
> has `dx_inf` absent or `dx_inf ≤ step_tol` (no pass moved the iterate).

**Derivation of the 0.20 bar.**
* Pure form. An exact cycle of period `k` visits `k` distinct states and repeats
  them for the rest of the walk, so `R = 1 − k/N`. For the periods this
  mechanism is interesting at (`k ≤ 100`) and the traces measured
  (`N ∈ {2560, 4680}`), `R ≥ 0.96`.
* Null. A crawl through distinct sets produces `R = 0` **exactly**, up to
  signature collision. The signature is a 64-bit sum of SplitMix64-mixed member
  tokens; the birthday probability of even one spurious collision over
  `N = 4680` is `N²/2^65 ≈ 6 × 10⁻¹³`, and a colliding pair would additionally
  have to match both norms.
* So the attainable interval is `[~0, 0.96]` and **0.20 sits well inside it** —
  above any plausible accident, an order of magnitude below the pure value, and
  reachable by a cycle of period up to `0.8 N` (3744 on STEENBRB), so it is not
  a bar only a short cycle can clear.

**The `T` tolerance.** The walk's own resolution: the loop declares a move of at
most `step_tol = feas_tol · max(1, ‖x‖∞)` to be no move, in both the KKT-point
test and the degenerate-step classification. Using the pass's own value rather
than a constant is what keeps the test scale-free. The `n ·` factor on the `l1`
comparison is the worst case of `n` components each within `T` — deriving it,
rather than reusing `T`, is what stops the `l1` check from being strictly
stronger than the componentwise statement it stands in for.

**Reported with the outcome, always:** `R`, the first recurrence `(i, j)`, the
period, the number of distinct states, the multiset of periods over all recurring
pairs, and the drift bound `(j − i) · max step_tol` on the first recurrence.

### 3.2 SIDE_PING_PONG

Over ADD passes with `block == kBound`: a **side flip** is an admission of
variable `v` at side `s ∈ {kAtLower, kAtUpper}` whose most recent earlier
admission in the trace was at the other side. Let
`P_flip = side_flips / bound_ADD_passes`, and let `C_top` be the share of all
side flips held by the single variable with the most of them.

> **SIDE_PING_PONG** iff `P_flip ≥ 0.40` **and** `C_top ≥ 0.20`.

**Derivation of 0.40.**
* Pure form. One variable strictly alternating lower/upper flips on every
  admission after its first: `P_flip → 1` as the number of admissions grows.
* Null. A one-sided two-cycle — the same variable pinned at the same bound,
  dropped and re-added — produces `P_flip = 0` **exactly**: there is no side
  change to count. A crawl that admits many different variables, each at the
  side that blocks its own step, likewise has no mechanism forcing a flip; its
  expected `P_flip` is the fraction of variables whose blocking side changes
  between visits, which is 0 for a monotone ride toward one bound.
* Interval `[0, 1]`, bar at 0.40 — clear of the null by a wide margin and clear
  of the pure value by a wider one.

**Derivation of 0.20 for `C_top`.** The brief's mechanism is "the **same**
variable alternating lower/upper". If flips were spread uniformly over the `V`
distinct bound variables the walk admits, `C_top ≈ 1/V`; the M3a rows give
`V = 23` on STEENBRB (`C_top ≈ 0.043`) and `V = 95` on YORKNET (`C_top ≈ 0.011`).
One ping-ponging variable gives `C_top = 1`; three, evenly, give `0.33`. A bar
at 0.20 admits up to five co-ping-ponging variables and excludes the uniform
spread by a factor of at least 4.6 on the smaller cell.

**Reported always:** `P_flip`, `C_top`, the per-variable flip counts (top 10),
and the flipping variables' `bound_gap`.

### 3.3 IMMEDIATE_READD

For each DROP pass `j` with a non-`kNone` identity `d`, let `a(j)` be the most
recent ADD pass before `j`. Say `j` is an **immediate** drop iff `a(j)` exists,
`a(j) = j − 1`, and the identity added at `a(j)` equals `d` **on
`(block, block_index)`** (the side is not required to match — a same-variable
re-pin at the other side is section 3.2's mechanism, and section 4's precedence
hands it there). Say an immediate drop is **re-admitted** iff some ADD pass in
`(j, j + k]` carries `(block, block_index)` equal to `d`.

Let `P_imm` = (immediate drops that are re-admitted) / `N_drop`.

> **IMMEDIATE_READD** iff `P_imm ≥ 0.60`, with **`k = 2`**.

**Derivation of 0.60.**
* Pure form. The two-cycle the mechanism names — add `A`, drop `A`, add `A`, … —
  makes **every** drop an immediate drop and re-admits it on the very next add
  pass: `P_imm = 1.00` at `k = 1`.
* Null. `drop_worst` picks by the Dantzig violation over the whole candidate set,
  a rule that has no reason to prefer the constraint just admitted. If the pick
  were independent of it, the rate would be `1/|candidates|`. The candidate set
  is the working set's droppable members; on these cells `mi = 0`, so it is the
  pinned variables, and the M3a rows put the walk at a vertex-like point of a
  problem with `n − me` free directions — 360 on STEENBRB, 56 on YORKNET. That
  gives nulls of `0.003` and `0.018`. Even the **worst imaginable** null — a
  candidate set of size 2, the smallest that leaves the rule any choice at all —
  is `0.50`.
* Interval `[0.003, 1.00]` in practice and `[0.50, 1.00]` in the adversarial
  worst case. **0.60 is above even the worst-case null** and 0.4 below the pure
  value.

**Derivation of `k = 2`.** One admission happens per ADD pass, so a pure
two-cycle re-admits at `k = 1`. `k = 2` allows exactly one intervening pass that
is neither an add nor a drop, which the M3a rows show to be rare: on
STEENBRB@F-trinf, `ws_adds + ws_drops = 4667` against `minor_iters = 4680` — 13
passes in 4 680 were neither, i.e. 0.28%. A larger `k` would start admitting
short orbits into an outcome whose whole content is "it came straight back", and
the settler-facing remedy (examine the drop rule) differs from the remedy for an
orbit (perturbation). `k = 2` is the smallest value that is robust to the
measured rate of neither-passes.

**Reported always:** `P_imm` at `k = 1, 2, 3, 5` (the neighbouring values, so the
sensitivity of the chosen `k` is visible), and the count of immediate drops that
were never re-admitted.

### 3.4 ZERO_PROBE_CHURN

`P_probe = |{DROP passes with drop_route == kZeroProbe}| / N_drop`.

> **ZERO_PROBE_CHURN** iff `P_probe ≥ 0.30`.

**Derivation of 0.30.**
* Pure form. `1.00`. Every drop arrives through §4b's zero-multiplier probe.
* Null. `0.00` **exactly, by construction**: the probe is reached only on the
  branch where `drop_worst` returned false, so a walk whose KKT-point passes
  always find a Dantzig drop can never produce a `kZeroProbe` record. This null
  is structural, not statistical.
* Interval `[0, 1]` with a hard zero at the null end. A bar at 0.30 means a
  handful of probe drops in an otherwise Dantzig walk does not trip the outcome
  (on STEENBRB it demands at least 697 of 2 323 drops), while a walk genuinely
  churning on the probe clears it by a factor of three.

**Reported always:** the full drop-route histogram (`kDantzig`, `kZeroProbe`,
`kNone`), and `N_drop` against the record's `ws_drops` — the two must agree,
because `ws_drops` is exactly the merge of the two routes, and a disagreement is
reported as an instrument finding.

### 3.5 NEGATIVE_CURVATURE_BLOCKERS

`P_ride = |RIDE-ADD passes| / N_add`.

> **NEGATIVE_CURVATURE_BLOCKERS** iff `P_ride ≥ 0.30`.

**Derivation of 0.30.**
* Pure form. `1.00`, and — checked against the loop rather than assumed — it is
  **attainable**: the ride is armed only on the pass following a drop
  (`dropped.active`), and in a drop/ride alternation every add pass is a ride, so
  nothing structural caps `P_ride` below 1. (This is the check `RULE-F.md`'s 0.5
  bar did not make.)
* Null. `0.00` exactly: with no negative curvature the ride never fires and every
  addition is an ordinary `kAdd`.
* Interval `[0, 1]`, bar at 0.30, clear of both ends.

**Reported always:** the action histogram over all eleven `QpWalkAction` values,
including `kRideUnbounded`, and `N_add` against the record's `ws_adds` — which
must agree, because `ws_adds` folds the ride's blocker in with the ordinary
addition.

### 3.6 DEGENERATE_CRAWL

Let `D` = the number of distinct `ws_sig_in` values over `P`, and `G = D / N`.
Let `D₁`, `D₂` be the distinct-signature counts of the first and second halves of
the trace.

> **DEGENERATE_CRAWL** iff CYCLING_PROPER's predicate is **false**
> **and** `G ≥ 0.70`
> **and** `D₁ ≥ 0.60 · (N/2)` **and** `D₂ ≥ 0.60 · (N/2)`.

**Derivation of 0.70 and of the two-half condition.**
* Pure form. A crawl that never revisits a set has `G = 1.00` and
  `D₁ = D₂ = N/2` exactly.
* Null. A cycle of period `k` has `G = k/N` — at most `0.04` for `k ≤ 100` on
  these traces, and `G ≥ 0.70` would require a period above `0.7 N`, which is
  not a cycle in any useful sense.
* The bar at 0.70 allows up to 30% of passes to repeat a set (a crawl with local
  back-and-forth) while still excluding anything cyclic.
* The **two-half condition is what makes "grows ~linearly" a test rather than a
  word.** A walk that explores for its first half and then settles into a cycle
  has a global `G` of about 0.5 and a second-half `D₂` near zero; the global bar
  alone would call that MIXED via `G < 0.70`, but a walk that explored 80% and
  cycled 20% would pass a global-only bar while plainly not crawling throughout.
  The pure crawl's own half-counts are `N/2`, so a floor at `0.60 · (N/2)` sits
  40% below the pure value and far above the `≈ k/2` a cycling half yields.

**Reported always:** `G`, `D`, `D₁`, `D₂`, and `D` measured at each decile of the
trace (the growth curve itself).

### 3.7 MIXED

Anything the precedence order below leaves unassigned. **Reported with all of the
numbers**, exactly as `verdict-F.txt` reported its own MIXED — a MIXED verdict is
a result, not a failure, and nothing is relabelled to reach a cleaner word.

## 4. Precedence — how the seven are made mutually exclusive

The **first** predicate that holds, in this order, is the cell's outcome:

1. `CYCLING_PROPER`
2. `SIDE_PING_PONG`
3. `IMMEDIATE_READD`
4. `ZERO_PROBE_CHURN`
5. `NEGATIVE_CURVATURE_BLOCKERS`
6. `DEGENERATE_CRAWL`
7. `MIXED`

**Why this order.** The ranking is by how tightly the evidence constrains the
remedy, strongest first.

* `CYCLING_PROPER` is a statement about the walk's **trajectory** — a state
  provably revisited — and it is the only outcome that a true cycle cannot fail
  to trip. A true cycle also trips 3 (its drops are immediate re-adds) and can
  trip 2, so it must rank above both or it would be reported as a weaker thing
  than it is.
* `SIDE_PING_PONG` ranks above `IMMEDIATE_READD` because a side-flipping re-pin
  **is** a re-add plus one further fact, and the extra fact changes the remedy
  (fixed-variable handling, not the drop rule). Reporting it as
  `IMMEDIATE_READD` would discard the distinguishing observation.
* Both structural outcomes rank above the two **route** outcomes (4, 5). A route
  fraction describes which branch of one function the walk took; a structural
  finding describes what the walk did. Where both hold, the structure is the more
  informative label — and since section 3 reports every fraction for every cell
  regardless, nothing is lost by the route outcome losing the label.
* `DEGENERATE_CRAWL` ranks last of the six because it is defined partly by an
  absence (no recurrence) and is the weakest positive claim.

**Both cells are classified independently.** There is no combined verdict; if the
two cells return different outcomes, both are reported as they fall.

## 5. Descriptive read-outs — pre-registered, and NOT verdicts

Registered here so that they can be reported without becoming a verdict, on the
precedent of `RULE-F.md` §4.1. None of them can change the outcome section 4
returns, and none licenses anything.

1. **The first minor at which progress stops.** `m_stop` = the largest minor with
   `dx_inf > step_tol` (i.e. the last pass that moved the iterate at the walk's
   own resolution), reported as an index and as `m_stop / N`. Also: the largest
   minor with `dx_inf > 0`, and the total `Σ dx_inf` after `m_stop`.
2. **The re-admission count distribution per variable.** Over ADD passes, a
   histogram of admissions per `(block, block_index)`: number of distinct
   identities, mean, median, max, and the top 10 by count. Re-admissions are that
   count minus one. Cross-checked against the record's `distinct_bound_added`
   (23 / 95) and `ws_adds_bound` (2344 / 1287) — a disagreement is reported as an
   instrument finding.
3. **Whether the re-admitted variables are fixed or near-fixed.** For every
   admitted bound, `bound_gap` classified at the pass's own resolution:
   `FIXED` iff `bound_gap == 0`; `NEAR_FIXED` iff `0 < bound_gap ≤ step_tol`;
   `ORDINARY` otherwise. Reported again against `opt_tol` (the trace's
   `decision_tol`, default `1e-9`) as a second cut, and as the raw `bound_gap`
   distribution (min, deciles, max) so neither cut is the only view.
4. **Multiplier magnitude at drop vs the drop tolerance.** Over DROP passes,
   `decision_multiplier` and the ratio `decision_multiplier / decision_tol`:
   min, deciles, max, split by `drop_route`. Also `best_multiplier` and
   `decision_multiplier / best_multiplier` (how far from the best the pick was),
   and `drop_candidates` / `drop_tied`.
5. **Ratio-test tie multiplicity.** Over ADD passes, the distribution of
   `ratio_tied`: the histogram of multiplicities, the share of passes with
   `ratio_tied > 1`, and the max. Cross-checked against the record's `ratio_ties`
   (0 / 52).
6. **The incoming warm signature.** `incoming_ws_signature` and
   `incoming_ws_size` for the traced solve, with `warm` (which must be `false` —
   §2.2). **This task traces ONE QP per cell**, so the brief's "vs later majors'"
   comparison is **NOT performed**; it is recorded here as declined-by-scope, not
   as a gap, and the one signature is reported so a later task can compare
   against it.
7. **The action histogram** over all eleven `QpWalkAction` values, and the
   **drop-route histogram**.
8. **The working-set size trajectory**: `ws_size_in` at the first and last pass,
   min, max, and its value at each decile; plus the count of passes where
   `ws_size_out != ws_size_in` and where `shift_adds > 0`.
9. **`raw_ratio`**: over ADD passes, how often the uncapped minimum ratio was
   `> 1` (the full step was interior and nothing joined) and its distribution
   over the passes where it was not.

## 6. Non-vacuity of the analysis itself

The fold that applies this rule must be shown to be able to FAIL, on M3a fix1's
hardened template. Before the verdict is written, the classifier is run against
**synthetic traces** and must return the stated outcome for each:

| synthetic trace | must return |
|---|---|
| a strict two-cycle (add `A`, drop `A`, ×1000, `x` constant) | `CYCLING_PROPER` |
| a strict lower/upper alternation on one variable, no set repeat | `SIDE_PING_PONG` |
| immediate drop/re-add of the just-added bound, with the set drifting so no state repeats | `IMMEDIATE_READD` |
| every drop routed `kZeroProbe`, no recurrence, no immediate re-add | `ZERO_PROBE_CHURN` |
| every add a `kRideStepped`, no recurrence | `NEGATIVE_CURVATURE_BLOCKERS` |
| a monotone crawl, every set distinct | `DEGENERATE_CRAWL` |
| a trace with none of the above at their bars | `MIXED` |
| an empty trace / a trace with a `kUnset` action / `truncated = true` | `ABORTED` |
| an identity-gate file with 19 of 20 fields, or a field that differs | `ABORTED` |

A classifier that returns `MIXED` for all nine is vacuous and its output is not a
verdict.

## 7. What is NOT re-tested here

`RULE.md`, `RULE-E.md` and `RULE-F.md` of M7 W2 M3a stand exactly as committed,
and `verdict.txt`, `verdict-E.txt` and `verdict-F.txt` are not reopened. In
particular the **MIXED** verdict on H-DEG stands: nothing this task measures
upgrades, downgrades or relabels it. The hypothesis under test here is a
different one —

> **H-MECH** Which of the six mechanisms M3a's §4 could not separate is the
> first QP's 4 680 / 2 560 minors actually made of?

— and section 3's seven outcomes are its answer space.

## 8. The settler-facing remedy mapping — a RECOMMENDATION ONLY

Restated from the brief, and binding on nothing. No outcome below licenses a
behaviour change, a default change, or a line of code; each is a candidate for a
**separate task** with its own rule and its own gates.

| outcome | candidate remedy |
|---|---|
| `CYCLING_PROPER` | a lexicographic or symbolic-perturbation anti-cycling rule |
| long zero-step sequences without exact recurrence (i.e. `DEGENERATE_CRAWL`, or a `MIXED` whose read-outs show `m_stop ≪ N`) | an EXPAND-style working-tolerance expansion — Gill, Murray, Saunders & Wright (1989), **the PAPER only** |
| `IMMEDIATE_READD` | examine the drop rule; a taboo list as a **diagnostic arm**, not a production rule |
| `SIDE_PING_PONG` | fixed-variable handling |
| `ZERO_PROBE_CHURN` | the probe's exemption logic (`src/qp/qp_engine.cpp` ~:1664) |
| `NEGATIVE_CURVATURE_BLOCKERS` | not a degenerate stall at all — the ride's blocker policy |
| `MIXED` | reported with the numbers; the settler chooses, or funds nothing |

**CLAUDE.md §6's firewall applies to every line of this section**: algorithmic
references are open-literature papers only. No SNOPT source, no GPL/LGPL solver
source.

---

*Committed before any leg of M7 W2 M3b ran. Base: `e891041e` (Task A's gated
head). Cells: STEENBRB, YORKNET, `tr_init = inf`, first QP only.*
