# M7 W2 M5 — THE DEGENERACY ARMS: THE RULE, PRE-REGISTERED

**WRITTEN ON THE BUILT DESIGN, 2026-09-22.** This file describes code that
exists, at `02d8cd98`, gated green at `0c6709ce` (`gates/README.md`). Nothing
in it describes a design that was superseded; where a superseded number is kept
it is labelled as a FINDING about the old oracle, never as a rule.

**STATUS: CLEARED AND COMMITTED BEFORE THE FIRST LEG.** The draft of this text
went to the settler under §10, who CLEARED it in full — items C1–C7 of
`.superpowers/m7-w2-m5-armb-amendments.md`. Those clearings are not an appendix:
they are written into the rule itself, in §5.1, §6 and §7, and §10 now records
what was cleared rather than what is asked. **No leg had run when this file was
committed.** After the first leg the rule is FROZEN — a change is a successor
rule, never an edit.

**THE THREE CLEARINGS THAT ADD OR REFUSE SOMETHING, so a reader meets them
before the sections that carry them:**

* **C4 pre-registers `cond(R)` BESIDE EVERY `indep_leak_bound_exceeded`** — the
  R-diagonal ratio off the QR that is already built, free, printed with the
  passed-over constraint's `aᵀp` decomposition. §5.1 and §6.
* **C6 REFUSES the bounds-only amendment** §5.1 raised. The class-(b) tails
  re-add a BOUND and never an inequality row, so restricting the pass-over to
  rows would make b1 vacuous on its own population. §5.1 states the refusal with
  its reason.
* **C7 registers the ENGINE-CERTIFICATE finding** and requires the certificate
  site to be NAMED. It is named in §5.1 and repeated in the artifact README.

Read with `.superpowers/m7-w2-m5-brief.md`;
`.superpowers/m7-w2-m5-armb-rulings.md` (R1, R3, R4, R7 stand; R2, R5 and R6
are superseded); `.superpowers/m7-w2-m5-armb-amendments.md` (A1–A12, binding);
`.superpowers/m7-w2-m4bc-review-sol.md` §6 (the Codex review the amendments
answer); and the header comment of
`include/hven/detail/qp/qp_degeneracy_arm.h`, which carries the design in full.

Code under gate: `f68dbbe8` (arm A + the diagnostic half), `3899bad6` (arm b1
as first built, whose oracle this design replaces) and **`02d8cd98` (the arm b1
REDO on A1–A12)**. Declaration of the gates:
`gates/psym-derivation-m5.txt`, re-declared at `0c6709ce`; its first
declaration covered a superseded head, was never measured, and may not be
quoted. **THE GATES RAN AND ARE GREEN** — all six met their declared
expectations and the fold proofs under them are 46/0; the declared-vs-measured
reading, taken from the raw transcripts, is `gates/README.md`. The production
library is byte-identical to `f255bae9` in Release (42/42) and debug-sections
only in Debug (41/42, `qp_engine.cpp.o`), which is what licenses calling either
arm a test seam rather than a change.

Papers only. Gill, Murray & Wright, *Practical Optimization* (1981) ch. 5 and
§8.3; Nocedal & Wright, 2nd ed. §16.5; Fletcher, LAA **106** (1988) and
*Ann. OR* **46/47** (1993); Gill, Murray, Saunders & Wright, *Math. Prog.* **45**
(1989) §2; Gill & Wong, *Math. Prog. Comp.* **7** (2015) §4. No solver source of
any kind was read. SNOPT firewall absolute.

---

## §1 — THE ARMS, AND WHAT EACH ONE IS

| switch | what it changes | built |
|---|---|---|
| `kScaledDrop` (a) | `drop_worst`'s two absolute `viol > opt_tol` tests become `viol > opt_tol · scale`, with `scale` the quantity `free_block_stationarity` already computed for the pass, PUBLISHED and not re-derived | yes |
| `kIndependentAdd` (b1) | one shared independence predicate at every working-set mutation site, an entry repair that makes the seed set independent, and — at the ordinary add — the ratio test's candidates taken IN RATIO ORDER with the first INDEPENDENT one blocking | yes |
| b2, the exchange | release an eligible working-set member and admit the blocker in its place | **NO — registered (A4)** |
| `kBoth` (a+b1) | both of the above | yes |

Everything is `HVEN_TESTING`-only and OFF by default; with nothing armed the
walk runs its production statements verbatim. The gates in
`gates/psym-derivation-m5.txt` are what establishes that, on the production
library's own bytes.

**b2 IS NOT BUILT.** Amendment A4 drops it outright for M5 and fixes its
reference: the registered production design item is **Gill & Wong (2015) §4's
MULTIPLIER-AWARE exchange** — never removing a mandatory equality or fixed
bound, recording the dependency vector, the multiplier and sign conditions, the
removed member, feasibility after the exchange, and the proof that the new set
is independent — and NOT "release the largest dependency coefficient", which
the Codex review's item 5 showed is not theorem-backed. R3's implementability
condition also fails, for three reasons written out in the arm header's ARM B2
block (`refresh_shifts` can re-admit the released member and erase the swap
with no record; the release is not a drop, and `DropRecord`/the probe's
addition-sequence stamps are what make a release safe; releasing a pinned bound
changes `num_free` while the step about to be taken came out of the pre-swap
face). **Task B measures b1. No verdict in this round is about the exchange.**

## §2 — HOW A DEPENDENT CONSTRAINT CAN BLOCK AT ALL

The arm header's §B.0 by reference, in full: in exact arithmetic on an
unregularized KKT it cannot (`A_W p = 0` ⇒ `aᵀp = 0` for a dependent `a`, and
`kEngineDenomTol` screens it out). Three things break that premise here — the
**regularization leak** (`A_W p = μ λ_W`, so `aᵀp = μ·yᵀλ_W`), the **off-face
iterate** (`A_W x ≠ b_W`), and the **rounding floor** (`ε‖a‖‖p‖`).

**AND AMENDMENT A5's SWEEP HAS ALREADY REFUTED THE UNQUALIFIED "O(μ)" READING
ON THE FIXTURE.** Run at `dual_mu ∈ {1e-6, 1e-8, 1e-10}` on M4's period-2 QP,
the first passed-over candidate's `aᵀp` is **1.46e-06 / 9.3276692556e-08 /
9.3277051744e-08**: it tracks μ above `1e-8` and **SATURATES** below it,
agreeing to five figures across a hundredfold change in μ. The declared band on
the ratio `aᵀp/(μ‖λ_W‖∞)` — `[1e-3, 1e+3]`, fixed before the numbers — holds,
but the mechanism does not stay the same across it: at small μ the blocking is
§B.0(ii)/(iii), the iterate's own residual and the rounding floor, and μ does
not touch either.

**SO THE RULE MAKES NO ORDER CLAIM.** "The observed blocking is the leak" is a
comparison a reader makes, per cell, from the recorded columns —
`add_b1_passed_a_dot_p`, its normalized form, `add_b1_leak_bound` (`μ‖λ_W‖∞`),
`add_b1_max_retained_leak` (`max|A_W p|` over the retained rows) and
`add_b1_passed_coeff_residual` (`|aᵀp − vᵀ(Rp)|`, whether the candidate's
derivative is RECONSTRUCTED by the retained rows' own leaks through the
dependency coefficients). A blocker whose `aᵀp` sits below the leak bound is
CONSISTENT with the leak and not proved to be it.

## §3 — THE DEPENDENCY ORACLE, ITS COST, AND ITS TOLERANCE

**THE ORACLE IS A RANK-REVEALING QR OF THE UNREGULARIZED ACTIVE ROWS (A1).**
`R = [Ae ; A_W ; e_i for each pinned i]`, `Eigen::ColPivHouseholderQR` of `Rᵀ`,
built ONCE per pass. The statistic is the relative projection residual

        ‖a − Q Qᵀ a‖₂ / ‖a‖₂   >   dependency_tol   ⇒   INDEPENDENT

with `Q` the first `rank(R)` columns of the orthogonal factor. The dependency
coefficients `v` with `a = Rᵀv` come from the same decomposition and are what
A5's reconstruction column is computed from.

**WHY NOT THE REGULARIZED KKT FACTOR, MEASURED RATHER THAN ARGUED.** The first
build tested a candidate by back-solving through a δ/μ-regularized local KKT
factor. The Codex review's item 4 objected that the `−μI` block is exactly what
makes a dependent row factorizable. The implementer then measured it: with the
screen off, on M4's period-2 QP, 102 residuals came out **bimodal — 99 at
`1.0000000272292198e-08` and 3 at order one** — i.e. the dependent population
sat exactly ON `dual_mu = 1e-8`, which is also the pre-registered tolerance, so
the test classified all 99 as INDEPENDENT by a relative hair and converted
nothing. **That measurement is kept as the evidence for A1** (in the arm
header's §B.2 block, and in this round's artifact) and the tolerance amendment
the previous draft asked for is WITHDRAWN: the oracle changed instead of the
number.

**THE TOLERANCE, PRE-DECLARED: `dependency_tol = 1e-8`,** unchanged in value
and changed in meaning — it is now applied to a statistic with no `μ` floor
under it. The same number is the QR's own rank threshold (A2), so the rank and
the candidate verdict cannot be read at two different tolerances.

**THE INTEGER SCREEN IS A FREE PRE-CHECK AND NEVER A VERDICT (A8).** Under A2's
invariant the two cannot disagree: `me + |W| = n` makes the rows span ℝⁿ, so
every candidate projects with residual 0 and the QR says DEPENDENT too; below
`n` the screen is silent and the QR alone decides. **The arm never refuses on
the screen alone.** The screen is recorded because A8's three-mechanism split
needs it.

**THE COST, DECLARED (A1, A11).** One dense rank-revealing QR of `Rᵀ` per pass
— `O(n·(me+|W|)²)`, with an `n × (me+|W|)` matrix materialized — plus one
orthogonal projection per candidate examined and one least-squares solve per
candidate called dependent. A cached decomposition is reused only for the EXACT
working set AND the EXACT solve it was taken on, so A11's "reuse the pass's QR
where the mutation went through the predicate, rebuild where it did not" is
decided by comparison, never by an assumption about call order. The per-pass
build count is `add_b1_qr_builds` and the per-solve total is `indep_qr_builds`.
**Wall-clock is informational (CLAUDE.md §7); a deadline-censored cell is
TRUNCATED and reported as such, never as a verdict.** SAWPATH and BRITGAS are
the cells A1 named as likely censored.

**THE EMPTY WORKING SET IS ANSWERED WITHOUT A DECOMPOSITION.** It spans
nothing, so every nonzero candidate is independent of it by definition. No
build is counted, because none is taken.

## §3.1 — WHAT THE THREE MECHANISMS ARE, AND WHY THEY ARE COUNTED APART (A8)

1. **ENTRY REMOVALS** — seed members the entry repair dropped
   (`indep_entry_members_removed`).
2. **SCREEN-AND-QR PASS-OVERS** — the screen fired and the QR agreed
   (`indep_screen_and_qr_dependent`, `add_b1_screen_qr_passed`).
3. **QR-ONLY PASS-OVERS** — the screen was silent and the QR alone called it
   dependent (`indep_qr_only_dependent`, `add_b1_qr_only_passed`).

**MEASURED ON THE FIXTURE, AND IT CONTRADICTS A8's EXPECTATION.** A8 predicted
the period-2 conversion would MOVE from the pass-over to the entry repair, on
the reasoning that the first build's screen-positive candidates at residual
0.667–1.0 proved a rank-deficient incoming set. **It did not move.** The redo
converts in 5 minors with `entry = 0`, `screen-and-QR = 3`, `QR-only = 0`, and
`indep_rank_deficit_max = 0` over the whole walk — so no incoming set was ever
deficient, and the first build's disagreement was an artifact of the
REGULARIZED oracle alone. Under the QR the screen and the oracle agree, which
is what A8 argued must hold under the invariant. **This is reported as a
finding, not smoothed over.**

## §4 — ARM A's RULE AND READ-OUT

The rule and its one declared approximation (the scale is published at the
pass's STARTING iterate while `drop_worst` runs after the KKT-point snap;
bounded by `‖H‖·step_tol`) are the arm header's, by reference.

**THE PREDICTION IS SPLIT IN TWO (A6), AND ONLY ONE HALF IS A PREDICTION ABOUT
THE DYNAMIC RUN.**

* **OFFLINE, COMPUTABLE NOW, NO SOLVER.** Replayed over the committed YORKNET
  traces, ALL 2,538 recorded core drops fail the scaled test. This is a
  pre-registered check run in Task B's read-out from the committed traces, and
  it is falsifiable without running anything.
* **DYNAMIC.** The altered run cannot promise a zero core count, because the
  trajectory diverges after the first refused drop. **REFUTATION means the SAME
  candidate identity is still dropped under the arm, or the SAME matched
  recurrence (tail signature set) persists.** A DIFFERENT tail surviving is NOT
  a refutation; it is reported as **"diagnosis untested by this trajectory"**.

**WHAT THE FIXTURE DOES AND DOES NOT ESTABLISH (R1).** ACCEPTED as a MECHANISM
fixture asserting `kNumericalError`: the refused multiplier there is a REAL
multiplier made small only relative to an inflated scale, so the drop it
refuses is one the walk needed; YORKNET's is CANCELLATION RESIDUE, where
refusing is the point. **The "terminates optimal under a" claim is made on
YORKNET in leg 2 or NOT AT ALL.** A residue-producing fixture was sought and
not found; the README records "residue fixture not found: three constructions
tried" as a known gap and no further search is run.

**READ-OUT:** `scaled_drops_made`, `scaled_candidates_refused`,
`scaled_drops_prevented` (the arm's actual claim — an emptied candidate set the
absolute test would have left non-empty), `scaled_max_refused_ratio`, and
`scale_unavailable_passes`. **`scale_unavailable_passes` must be 0 on every
run; a nonzero value INVALIDATES that run's arm-A read-out and is reported, not
absorbed.**

## §5 — ARM B1's RULE, ITS INVARIANT, ITS SITE COVERAGE, AND ITS HAZARD

**THE RULE AT THE ORDINARY ADD (R3/R4), verbatim.** Candidates in ratio order;
the first INDEPENDENT one is the blocker; each DEPENDENT candidate is passed
over, recording index, residual, `aᵀp`, the leak columns and the tie-set size;
one oracle test per candidate examined, in ratio order, until an independent
one is found, with the count recorded per pass. If no independent blocker
exists, take the full step and record every passed-over constraint's post-step
violation. A candidate whose test could not be taken (`verdict = −1`) is
ADMITTED, never refused.

**THE INVARIANT IS RANK, NOT ROW COUNT (A2).**
`rank(R) == me + |W| ≤ n`, read off the same decomposition at the same
tolerance. The row count `me + |W| − n ≤ 0` is NECESSARY BUT NOT SUFFICIENT
(the Codex review's item 2) and survives only as the free integer witness it
is, recorded as `add_b1_rows_after` and `indep_rows_minus_n_max`.

**A VIOLATION AWAY FROM ENTRY DISARMS THE SOLVE AND CONTINUES (A9).** It is
recorded (`indep_rank_violations`, `add_b1_disarmed`, with the SITE), the arm
stops for the rest of that solve, and the solve runs on UNARMED. **That cell's
b1 verdict is UNSUPPORTED — counters reported, no CONVERTED and no HARM.** If
MORE THAN HALF the cells disarm, **the arm's result is UNSUPPORTED as a
whole.** It is not a global stop, and it is not a reason to abandon the legs.

**EVERY MUTATION SITE IS COVERED, AND THE TWO THAT ARE NOT ARE NAMED (A3).**
One shared predicate, `admit_if_independent`:

| # | site | function | what the arm does |
|---|---|---|---|
| 1 | ordinary blocker add | `QpEngine::run` | REFUSE ⇒ take the next candidate in ratio order |
| 2 | ride add | `ride_negative_curvature` | REFUSE ⇒ the ride DECLINES, before it mutates `x` or the set |
| 3 | seed ingestion | `ingest_seed_working_set` | covered by the ENTRY REPAIR below |
| 4 | shift additions | `refresh_shifts` | REFUSE ⇒ the row is not added; the shift stays positive and the row is offered again next refresh |
| 5 | zero-probe restoration | `probe_zero_multiplier_drops` | **CHECK-ONLY**: the verdict is counted, the restoration always proceeds |
| 6 | repair pins | `pin_at_best_bound` | REFUSE ⇒ `false`, which the caller already handles as "nothing to pin against" |

Site 5 is check-only DELIBERATELY: it puts BACK a member the probe removed for
a trial, so refusing it would change the probe's contract rather than an add
rule, and the walk would silently lose a constraint it never decided to drop.
**`refine_on_face`'s mutation is ruled OUT OF SCOPE explicitly** — it
transcribes a CALLER-SUPPLIED face for one exact solve, does not run the walk
and reaches no add rule. **Sites 5 and 6 are declared UNCOVERED-BY-FIXTURE**
(both sit inside repair paths that fire on inertia verdicts a constructed QP
does not produce on demand); what stands behind them is A2's rank check at the
next predicate call, which disarms under A9 if either left the set deficient.

**THE ENTRY REPAIR (A3).** At solve entry — after the seed ingest and the
seeding `refresh_shifts`, and BEFORE the observer snapshots the starting set —
the arm decomposes the seed working set. If it is independent, nothing happens
(one decomposition). If it is not, the arm rebuilds it: the MANDATORY rows
first (equality rows and FIXED bounds, which it may never remove), then every
non-mandatory member in canonical order, each admitted only if the same
predicate calls it independent of what has been accepted. Each removal is
counted. **"Reverse ingestion order" is honoured by a DECLARED SURROGATE:**
`WorkingSet` retains no ingestion order (`active_ineq()` is kept sorted,
`bound_state()` is indexed by variable), so the canonical order — equality
rows, then working rows ascending, then pinned bounds ascending — is used and
the EARLIER member of a dependent pair is kept, which is removal in reverse
canonical order. **`x` is not moved**: a removed pin leaves the variable where
it sits.

**MANDATORY ROWS DEPENDENT ⇒ THE ARM REFUSES THAT SOLVE**
(`indep_refused_mandatory_dependent`), disarms, and the solve proceeds UNARMED.
That cell's row says so and its b1 verdict is UNSUPPORTED.

**THE SELF-CHECK.** The arm re-enumerates the ratio test's candidates in
production's scan order and its FIRST candidate must be the blocker
production's `ratio_test` just returned. A mismatch REFUSES the pass and is
counted (`indep_enumeration_mismatches`). **This counter must be 0 for a run's
b1 read-out to mean anything; a nonzero value invalidates that cell.**

**NEUTRALITY IS STRUCTURAL.** When nothing is passed over the caller changes no
variable at all, so an armed run on a walk with nothing dependent IS
production, bit for bit, by construction rather than by arithmetic.

**PRE-REGISTERED PREDICTIONS.** The core drop→same-bound-re-add unit count
falls to ~0 on the class-(b) tails (ACOPP118, ACOPR57, DALLASM, STEENBRB,
STEENBRC, STEENBRG, SAWPATH, and ACOPP57's b-half). **If it does not, b1 is
REFUTED as the cause on that cell.** `indep_blocker_switched > 0` on each; the
leak-magnitude distribution is reported, not predicted. The three-mechanism
split (§3.1) is reported per cell and is NOT predicted — the fixture's split is
a fixture's.

### §5.1 — THE HAZARD THE RULE CARRIES, AND A10's BOUND ON IT

**b1 CAN LEAVE THE ITERATE OUTSIDE ITS BOX, AND THE ENGINE CERTIFIES IT
`kOptimal` ANYWAY.** The no-independent-blocker branch takes the full step; a
passed-over BOUND is a bound the walk steps through, and nothing downstream
puts it back (`refresh_shifts` is about inequality ROWS; the certification
branch reads the working set's own residual). Measured on the scale-carrier
fixture: `kOptimal` returned at a point **5.59e-03 outside its box**, recorded
only by the arm's own `add_b1_passed_violation` column and
`indep_max_passed_violation` counter. Asserted as a fixture so it cannot
quietly stop being true.

**AMENDMENT A10's PRE-REGISTERED BOUND.** After a pass-over,

        violation  ≤  (μ‖λ_W‖∞ + ε‖a‖₂‖p‖₂) · α · 10

with the factor of 10 declared in advance. A larger violation means the
candidate was NOT merely leaking ⇒ **FINDING**, and **that solve's b1 verdict
is UNSUPPORTED**. Counted as `indep_leak_bound_exceeded` and
`add_b1_leak_bound_exceeded`.

**AND BECAUSE THE BOUND IS KNOWN TO BE INSUFFICIENT AS FORMULATED, C4
PRE-REGISTERS WHAT IS PRINTED BESIDE EVERY EXCEEDANCE — NOW, BEFORE ANY LEG.**
The settler's clearing accepts the finding below (`A_W p = μλ` bounds `A_W p`,
not what `p` reaches THROUGH an ill-conditioned `A_W`) and does not weaken the
bound to hide it. Instead, beside every `indep_leak_bound_exceeded` the
read-out prints, per exceedance:

1. **`cond(R)`, from the QR's own R-diagonal ratio** —
   `|R(0,0)| / |R(rank−1, rank−1)|` on the decomposition the pass ALREADY
   built, so it costs nothing and is available at exactly the passes where the
   bound is tested. Recorded per exceedance and as the per-solve high-water
   mark.
2. **The passed-over constraint's `aᵀp` decomposition** — the A5 columns for
   that candidate: `aᵀp`, its normalized form, `μ‖λ_W‖∞`, `max|A_W p|` over the
   retained rows, the dependency coefficients `v`, and the reconstruction
   residual `|aᵀp − vᵀ(Rp)|`. Together these say whether the excess came
   through the retained rows' own leaks amplified by the dependency
   coefficients, or from somewhere the model does not describe.

**WHAT AN EXCEEDANCE DOES IS UNCHANGED:** that solve's b1 verdict is
UNSUPPORTED, and it is an INFEASIBLE-EXIT cell if the returned point violates
its own tolerances. `cond(R)` is a DIAGNOSTIC, never an excuse — a large
`cond(R)` does not restore the verdict. **The tables report exceedances against
`cond(R)`** so a reader can see for themselves whether every exceedance is an
ill-conditioning case or whether some are not; that comparison is a reading the
tables support, not a prediction this rule makes.

**AND ON THE FIXTURE THE BOUND FAILS — NOT FOR A10's REASON.** A10 expected the
5.59e-03 excursion to be the rank-deficient-incoming case. It is not:
`indep_rank_violations = 0` and `indep_rank_deficit_max = 0`, so the set was
independent, the candidate was genuinely dependent on it (screen and QR
agreeing), and the direction still leaked through it by far more than
`μ‖λ_W‖∞` can account for — because that fixture's working set is
ILL-CONDITIONED by construction, and `A_W p = μλ` bounds `A_W p`, not what `p`
itself reaches through a badly conditioned `A_W`. **Reported as a finding
about the RULE, before the legs.**

**CONSEQUENCE FOR THE LEGS, PRE-REGISTERED.** A b1 or a+b1 cell whose returned
point violates its own bounds or rows by more than the cell's feasibility
tolerance is **NOT a CONVERTED cell**, whatever its status says. The per-cell
read-out carries the returned point's own primal infeasibility, recomputed from
the problem, and a cell failing it is reported as **INFEASIBLE-EXIT** — a
category this round adds to M4 §7's list and declares here, in full, before the
first leg.

**REGISTERED SEPARATELY AS AN ENGINE FINDING (A10, cleared as C7), NOT AS THIS
TASK'S WORK.** The engine certified `kOptimal` with a bound violated by
5.59e-03: **the optimality certificate does not check the returned iterate's
primal BOUND feasibility.**

**THE CERTIFICATE SITE, NAMED as C7 requires.** `src/qp/qp_engine.cpp`, the
would-be-optimal exit of the drop loop: the branch tests
`detail::free_block_is_stationary(free_stat, stat_scale, eff_opts)` (~:956) and,
on success, sets `status = QpStatus::kOptimal` (~:990) with no reading of the
returned `x` against its own bounds. Everything the branch consults is DUAL —
the free block's stationarity residual against its scale — and the primal side
it does consult is the WORKING SET's residual, not the box. A variable that the
walk stepped THROUGH a passed-over bound is outside its box while the free
block is perfectly stationary, so the two conditions do not contradict each
other and nothing downstream notices.

**IT IS REGISTERED, NOT FIXED HERE.** The arm is a test seam; touching the
certificate would be a production change, outside this task's scope and outside
its gates. The finding is repeated in the artifact README with this same site,
and it is NOT contingent on the arm: an engine that certifies a point it has not
checked is a property of the engine, which the arm merely made visible. It is
carried as an ENGINE item for the register.

**A SECOND INTERACTION, PRE-REGISTERED AS A RISK, NOT A PREDICTION.**
`refresh_shifts` puts every inequality row carrying a positive shift INTO the
working set — and under the arm that site now REFUSES a dependent row, so the
row can be offered and refused repeatedly. The read-out is
`counters.shift_adds` and `indep_shift_refusals` against the OFF run's,
reported per cell. No direction is predicted.

**THE BOUNDS-ONLY AMENDMENT WAS RAISED AND IS REFUSED (C6), WITH ITS REASON.**
The draft asked whether restricting the pass-over to inequality ROWS, never to
BOUNDS, should be adopted before the first leg — it would remove the hazard
above at the cost of changing R3's rule. **The settler refused it, and the
reason is a measurement, not a preference:** M3c's and M4c's traces show the
class-(b) tails re-add a **BOUND**, never an inequality row — a share of **1.0
on 16 of 21 traced solves**. A pass-over restricted to rows would therefore
never fire on the population b1 exists to address, and the arm would be
VACUOUS on its own cells while appearing to run. **The pass-over stays on
bounds.** The hazard is not removed; it is BOUNDED and REPORTED, by three
things already in this rule: A10's bound with C4's `cond(R)` read-out beside
every exceedance, the UNSUPPORTED verdict an exceedance forces, and the
INFEASIBLE-EXIT category that outranks CONVERTED. This is the last point at
which the question could be raised, and it was raised and answered here.

## §6 — READ-OUTS

M4 `RULE.md` §6's predicates **by reference**, unchanged, plus:

* the core drop→same-bound-re-add unit count from M4c's `m4c_tails.py`, COPIED
  into `wrappers/` and run, never re-derived by hand;
* arm A's five counters (§4) and b1's counters (§5), per cell;
* **A8's three-mechanism split** (§3.1), per cell;
* `rank(R)`, `me + |W|` and their difference, per pass
  (`add_b1_rank`, `add_b1_rank_rows`) and as the per-solve high-water marks
  (`indep_rank_deficit_max`, `indep_rows_minus_n_max`);
* **A11's cost**: `indep_qr_builds` per solve and `add_b1_qr_builds` per pass;
* the projection-residual DISTRIBUTION per cell, with the screen-positive /
  screen-negative split — printed regardless of the verdict;
* **A5's leak columns** per add (§2), and the returned point's own primal
  infeasibility (§5.1);
* `indep_ride_refusals`, `indep_shift_refusals`, `indep_repair_pin_refusals`
  and `indep_probe_restore_dependent`, so the table says WHICH SITE acted;
* `indep_rank_violations`, `indep_refused_mandatory_dependent` and
  `indep_leak_bound_exceeded` — the three counters that make a cell
  UNSUPPORTED;
* **C4's exceedance read-out, pre-registered here and printed BESIDE EVERY
  `indep_leak_bound_exceeded`**: `cond(R)` from the QR's R-diagonal ratio (per
  exceedance and as the per-solve high-water mark) and the passed-over
  constraint's full `aᵀp` decomposition (§5.1). The per-arm table carries an
  exceedances-against-`cond(R)` column so the ill-conditioning reading is one a
  reader takes from the numbers rather than one this rule asserts;
* `counters.shift_adds` against OFF (§5.1);
* per drop pass, under its own switch, the post-drop inertia through the local
  factor — ONE extra assemble+factorize per drop pass, declared.

## §7 — VERDICTS, CONSENSUS, RANK ORDER, AND STOPPING RULES

M4 `RULE.md` §§7–10 **by reference**: CONVERTED / improved-unverified / HARM /
none / TRUNCATED, the consensus rule (M3c's), the rank order, and "what a
result licenses". **THE DEVIATIONS, WRITTEN IN FULL AND IN ADVANCE:**

1. **INFEASIBLE-EXIT** is added as a category (§5.1). It outranks CONVERTED: a
   cell that exits infeasible is never CONVERTED, whatever its status.
2. **UNSUPPORTED** replaces R5's separation rule and covers three cases, all
   from the amendments: a cell that DISARMED (A9); a cell whose arm refused the
   solve on mandatory-dependent rows (A3); and a cell with a nonzero
   `indep_leak_bound_exceeded` (A10). An UNSUPPORTED cell reports its counters
   and no CONVERTED/HARM verdict.
3. **IF MORE THAN HALF THE CELLS DISARM, the arm's result is UNSUPPORTED AS A
   WHOLE** (A9).
4. **A NONZERO `indep_enumeration_mismatches` OR `scale_unavailable_passes`
   INVALIDATES that cell's read-out** for the arm concerned (§4, §5).
5. **The rank invariant does NOT stop the legs.** R6's "a positive value STOPS
   the arm" is superseded by A9: record, disarm, continue, report UNSUPPORTED.
6. **Wall-clock is informational.** A deadline-censored cell is TRUNCATED. The
   QR's `O(n·m²)` per pass makes censoring more likely than in M4, and a
   censored cell is reported as TRUNCATED and not as a verdict.

## §8 — THE LEGS

In the brief's order, one lock hold each, `taskset -c 2`,
`MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, 600 s deadline (300 s SAWPATH),
`timeout 900`, censored rows removed, chain detached with `setsid`:

1. **OFF controls** — M4's committed ten OFF rows by sha (never re-run) and
   M4's three neutrality OFF rows; the smoke proves the new binary reproduces
   M3c's ACOPP57 OFF row on EVERY field.
2. **Arm a on YORKNET alone** — A6's OFFLINE check from the committed traces
   first, then the dynamic leg with A6's refutation condition.
3. **Arm b1 on the seven class-(b) cells plus ACOPP57**, then **a+b1 on the
   same eight**, then **a+b1 on BRITGAS and YORKNET** (§5's predictions, the
   rank invariant, §5.1's read-outs).
4. **Neutrality, DESCRIPTIVE AND EXTENDED (A7).** READING6, SMBANK, ZAMB2-8
   under a, b1, a+b1 — status preserved, `f` under M3c's consensus rule, minors
   ratio printed, **plus FEASIBILITY and STATIONARITY against each leg's own
   tolerances**. A change of status on any of the three is HARM and stops that
   arm's verdicts being quoted. **No "production-ready" wording appears
   anywhere in the artifact** (A7): three cells cannot license it, and the
   phrase is not used even where the result is clean.

The probe is built by REPLAYING the install's compile commands with the
compiler pinned (M4's pattern). `trace_out` is a label. The classifier and the
runner DERIVE; they never carry.

## §9 — WHAT A GREEN RESULT WOULD NOT LICENSE

* Not that either arm ships. Both are experiments on a test seam; the
  production forms (a cheaper dependency test than a per-pass dense QR, and the
  Gill & Wong exchange) are registered design items and neither is built.
* Not that the drop/re-add recurrence has ONE cause. M4c classified four
  populations; these arms address two of them.
* Not a timing claim of any kind.
* Not, for arm A, "terminates optimal" from a fixture — only from YORKNET, and
  only if leg 2 shows it (R1) — and not a zero dynamic core count (A6).
* Not, for b1, a claim about the exchange, which is not built.
* Not an "O(μ)" claim about the leak: §2's sweep already refutes it below
  `dual_mu = 1e-8` on the fixture.
* Not "production-ready", in those words or any paraphrase (A7).

## §10 — WHAT THE SETTLER CLEARED, C1–C7, WRITTEN INTO THIS RULE

The draft of this file asked the settler six questions under this heading. All
six were answered, and a seventh item was added. **The answers are not recorded
here as an appendix — each one is written into the rule section that carries
it**, and this section says WHERE, so that the rule a leg is judged by is one
text and not a text plus a correspondence.

**C1 — CLEARED: the INFEASIBLE-EXIT category and the UNSUPPORTED rules, with a
rank violation DISARMING rather than stopping.** §5.1 defines INFEASIBLE-EXIT
and §7's deviations 1–3 carry it: a cell that exits infeasible is never
CONVERTED whatever its status; a rank violation away from entry records, disarms
that solve, continues, and makes that cell's verdict UNSUPPORTED; and **if more
than half the cells disarm, the arm's result is UNSUPPORTED AS A WHOLE**. It is
not a global stop and it is not a reason to abandon the legs.

**C2 — CLEARED: the canonical-order surrogate for the entry repair.** §5's entry
repair admits mandatory rows first, then non-mandatory members in the canonical
order (equality rows, then working rows ascending, then pinned bounds
ascending), keeping the EARLIER member of a dependent pair. **The rule states
the reason plainly: `WorkingSet` retains no ingestion order** — `active_ineq()`
is kept sorted and `bound_state()` is indexed by variable — so A3's literal
"reverse ingestion order" cannot be executed, and the surrogate is declared as a
surrogate rather than passed off as the instruction.

**C3 — CLEARED: sites 5 and 6 declared UNCOVERED-BY-FIXTURE, and site 5
CHECK-ONLY with its reason.** §5's site table carries both. Site 5 (zero-probe
restoration) is CHECK-ONLY because it puts BACK a member the probe removed for a
trial: refusing there would change the probe's contract rather than an add rule,
and the walk would silently lose a constraint it never decided to drop. **What
stands behind both is A2/A9's rank check at the next predicate call**, which
disarms the solve if either site left the set deficient — and §5 says so, rather
than leaving the gap implicit.

**C4 — the three findings are reported AS FINDINGS, each headed with the
amendment it contradicts, and one of them adds a pre-registered read-out.** They
are in §2, §3.1 and §5.1, and the artifact reports them under the same headings:

* **Contradicts amendment A8's expectation.** §3.1. A8 predicted the fixture's
  period-2 conversion would MOVE to the ENTRY REPAIR, reasoning that the first
  build's screen-positive candidates at residual 0.667–1.0 proved a
  rank-deficient incoming set. **It did not move: `entry = 0`, `screen-and-QR =
  3`, `QR-only = 0`, `indep_rank_deficit_max = 0`** over the whole walk. No
  incoming set was ever deficient, so those residuals were an ARTIFACT OF THE
  REGULARIZED ORACLE alone — and under the QR the screen and the oracle agree,
  which is exactly what A8 argued must hold under the invariant. **A8's
  diagnosis is REFUTED on the fixture; A8's reasoning about the screen is
  confirmed.**
* **Contradicts amendment A10's expectation.** §5.1. A10's bound is
  INSUFFICIENT AS FORMULATED: `A_W p = μλ` bounds `A_W p`, not what `p` reaches
  through an ill-conditioned `A_W`. The 5.59e-03 excursion is flagged
  (`exceeded = 1`) with `rank_violations = 0` and `rank_deficit_max = 0`, so it
  is NOT the rank-deficient-incoming case A10 expected. **The response is a
  pre-registration, made before any leg: `cond(R)` and the `aᵀp` decomposition
  beside every exceedance (§5.1, §6).** An exceedance still makes that solve
  UNSUPPORTED; `cond(R)` explains, it does not excuse.
* **Contradicts amendment A5's expectation.** §2. The μ sweep at
  `dual_mu ∈ {1e-6, 1e-8, 1e-10}` gives `aᵀp` = **1.46e-06 / 9.3276692556e-08 /
  9.3277051744e-08**: **the leak tracks μ above 1e-8 and SATURATES below it**,
  agreeing to five figures across a hundredfold change. The declared band on the
  ratio holds, but the MECHANISM changes across it. **The rule's leak model
  states both regimes and makes no unqualified "O(μ)" claim anywhere** — below
  the floor the blocking is §B.0(ii)/(iii), the iterate's own residual and the
  rounding floor, and μ touches neither.

**C5 — CLEARED: b2 stays REGISTERED AND UNBUILT.** §1 carries it, on A4's
reference — **Gill & Wong, *Math. Prog. Comp.* 7 (2015) §4's multiplier-aware
exchange**, not "release the largest dependency coefficient" — together with the
three reasons R3's implementability condition fails here. **No verdict in this
round is about the exchange.**

**C6 — REFUSED: there is NO bounds-only amendment.** §5.1 states the refusal and
its reason: the class-(b) tails re-add a BOUND and never an inequality row
(share 1.0 on 16 of 21 traced solves), so restricting the pass-over to rows
would make b1 vacuous on its own population. **The pass-over stays on bounds**,
and the hazard is bounded by A10 + C4's read-out + UNSUPPORTED +
INFEASIBLE-EXIT. This was the last point at which the question could be raised.

**C7 — REGISTERED: the engine-certificate finding, with its site named.** §5.1
names it — `src/qp/qp_engine.cpp`, the would-be-optimal exit that tests
`free_block_is_stationary` (~:956) and sets `QpStatus::kOptimal` (~:990) without
reading the returned `x` against its own bounds — and the artifact README
repeats it. **It is an ENGINE item for the register, not this task's work**, and
it is not contingent on the arm: the arm made a pre-existing property visible.

**NOTHING IS OUTSTANDING.** No leg had run when this file was committed, and no
question remains open that a leg's result could be made to answer after the
fact.
