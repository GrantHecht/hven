# M7 W0 T3 — the reading

Provenance: `PROVENANCE.txt`. Counters asserted, wall and instructions informational.
Raw: `raw/both-states-3reps.log` (three replicates, byte-identical counters),
`perf/both-states-perf.log` (one process per cell), `raw/step2a-debug.log` (Debug).

## 1. The table

`rung facts` is the ELASTIC LADDER's own factorizations, attributed by an identity that is
validated where its answer is known (see §4). `rung minors` is the ladder's own minor
iterations, summed off the `qp.mode` line each rung emits at site `elastic_rung`.

| engine | N | n | rungs | rung minors | rung facts | declined rungs | rung B | wall s | instructions |
|---|---|---|---|---|---|---|---|---|---|
| walk | 100 | 500 | 6 | 312 | **184** | 0 | 0 | 0.536 | 4.09e9 |
| walk | 200 | 1000 | 6 | 612 | **484** | 0 | 0 | 2.011 | 1.60e10 |
| walk | 400 | 2000 | 6 | 1212 | **1084** | 0 | 0 | 7.781 | 6.57e10 |
| walk | 800 | 4000 | 6 | 2409 | **2281** | 0 | 0 | 31.382 | 2.74e11 |
| tier | 100 | 500 | 5 | 71 | **73** | 3 | 1 | 0.045 | 5.23e8 |
| tier | 200 | 1000 | 5 | 69 | **71** | 3 | 1 | 0.089 | 1.03e9 |
| tier | 400 | 2000 | 5 | 71 | **73** | 3 | 1 | 0.181 | 2.09e9 |
| tier | 800 | 4000 | 5 | 73 | **75** | 3 | 1 | 0.375 | 4.32e9 |

## 2. Does the tier rung's cost stay flat in n? YES.

The tier rung's factorizations are **73, 71, 73, 75** across a 16× range of `n` — flat, with
no trend. The walk's are **184, 484, 1084, 2281** — linear in `n` (the ratio to `n` rising
0.37 → 0.48 → 0.54 → 0.57). At N = 800 the tier pays **30× fewer factorizations**.

The mechanism, visible in `rung_max_minors` (step 2a): the walk's ladder spends ~0.6·n minors
on its FIRST rung and ~2 on each of the other five, so the hot chain works exactly as the M6
W2 plan said and what does not scale is the COLD FIRST RUNG — an active-set solve from cold,
whose iteration count cannot be sublinear in the active set. An interior-point rung has no
such scaling, and the measurement says so.

**In absolute terms** at each size, tier vs walk: 73 vs 184 facts (N = 100, 2.5×), 71 vs 484
(6.8×), 73 vs 1084 (14.8×), 75 vs 2281 (**30.4×**). The tier already wins at the smallest size
measured, and the margin grows linearly. Instructions: 63× fewer at N = 800, and the tier's
own instruction count grows LINEARLY in N (2× per doubling) against the walk's quadratic
(4× per doubling) — each tier factorization is on a larger matrix, which is the expected O(n)
sparse cost, while the walk pays O(n) factorizations of O(n) cost apiece.

## 3. THE FINDING THAT CUTS THE OTHER WAY: the tier declines, and it is a false positive

At EVERY size, **3 of the tier's 5 rungs exit non-optimal** and **one fallback entry falls
through to rung B** — the cold walk, W1's own refusal path. The verdict survives (the
four-arm entry partition absorbs it exactly as designed, and the certificate is still
synthesized by the DRIVER from the ladder's slack l1, never by the tier), but the rung the
toggle exists to provide is declined more often than it is taken.

**Why.** Diagnosed on a throwaway build that attached the trace sink to the elastic engine
(two lines; reverted — see §5): every one of those declines is
`IpqpEscape::kInfeasibleSuspect`. The elastic QP is **feasible by construction**, so each is
by definition a FALSE POSITIVE of the §6.3 detector. Raising `ipqp_max_iter` to 200 and to
600 changes not one counter, so this is not a budget effect; each declining rung spends ~20
iterations and escapes.

This is exactly the failure mode the design page's §(iii) predicted in advance: the detector
fires "in exactly the regime — a rank-deficient curvature block wanting a large `rho_dem` —
that it was hardest to settle". The elastic QP's `H` is structurally empty on its `ns` slack
columns, and the proximal `rho` plus the condensed bound curvature are what keep the KKT
non-singular there; the detector reads that regime as suspicion.

**The consequence is bounded and is a robustness question, not a correctness one.** No wrong
certificate is possible: the tier never produces `kInfeasible` (spec §6.3), an unusable exit
maps to `kMaxIter`, and the driver's own exhaustion test owns the verdict. What is lost is a
rung — and on the two-variable W2 T7 fixture, where all rungs decline, the solve reaches
`kInfeasible` through rung B but WITHOUT the infeasibility certificate the walk arm produces
(`SqpDriverCertifiedFallback.T3TheTierSolvedRungReachesTheSameVerdictOnTheSameLinearization`
records both).

## 4. The attribution, and why it can be trusted

Walk rungs: the Ledger's QP-level `SolveRecord`s are one per walk-engine solve; the `qp.mode`
stream also carries lines for solves that are not the walk's, so the walk-mode SUBSEQUENCE is
aligned against the ledger and CHECKED per index (each record's `minor_iters` must equal its
line's `iters`).

Tier rungs: the elastic engine emits no ledger record, so its factorizations are read by
difference — `SqpCounters::factorizations` less the PRIMARY tier's `ipqp_factorizations` less
every ledger record's. The identity is **validated on the walk arm**, where the elastic tier
never runs and the residual must therefore be exactly 0; the test asserts that, so a
factorization reaching `SqpCounters` by a path the subtraction does not know about fails the
measurement rather than silently biasing it.

## 5. What is NOT observable from the boundary, and is registered

The elastic engine's escape REASON. The driver deliberately does not attach the trace sink to
it: the `ipqp.*` schema's `solve` id is per-instance, so two engines emitting into one stream
would make that id ambiguous. The contract-level fact — that a rung declined — IS observable
(the `qp.mode` line's outcome, counted as `declined rungs` above and asserted in the suite).
The REASON required a two-line diagnostic build:

    // in SqpSolver::elastic_ipqp_engine(), after construction:
    elastic_ipqp_engine_->attach_trace(ipqp_trace_);

with a sink recording `on_ipqp_escape`. Under `max_iter = 1` the first escape event is the
primary tier's and the rest are the elastic engine's. Reproducing the §3 finding is that
patch plus one run. **Registered**: an escape census for the elastic engine that does not
collide with the primary tier's stream is an M7 W2 item, not this task's.

## 6. The disposition

**`kWalk` stays the default**, on two independent grounds:

1. **The M7 exit criterion.** The win is measured on a CONSTRUCTED family, not on the suite's
   TRAIN split — and the populations that would decide it are either empty (F7 never fires the
   rung) or embargoed (HS, until W1 H2). A heuristic that cannot show a win on TRAIN ships
   disabled. W2 re-reads this default on TRAIN, as the M7 brief's §1 T3 says.
2. **§3's false positives.** A rung that is declined 3 times in 5 on the very relaxation it
   was built to solve is not yet the robustness improvement the owner's sentence asks for. The
   cost case is proven and large; the reliability case is not, and shipping the toggle ON would
   trade a measured 30× cost win for an unmeasured reliability regression on populations that
   do not exist in the tree yet.

The toggle ships, validated, OFF, and measured — which is what M7 asked for. The two things
that would move it: a §6.3 detector that does not misfire on a feasible-by-construction
relaxation, and a TRAIN population that fires the rung at scale.
