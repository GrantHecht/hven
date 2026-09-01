# Roadmap draft — M6, M7, M8

Date: 2026-08-26. Author: the hven settler lane. Status: DRAFT for
owner review. Inputs: the accumulated M6 register, the SQP lane's
2026-08-26 submission (docs/notes/2026-08-26-m6-register-sqp-lane-submission.md,
its dependency ordering adopted), the M4-registered no-copy
claim-stream end-state, and the owner's 2026-08-26 directives:
consider everything from tycho_sqp (high priority); adaptive
mode-switching with smart heuristics (unforced); default parameter
tuning with labeled configs (unforced) and a published-comparable
benchmark suite (Knitro published results; SNOPT and IPOPT available
locally); solver-trace upgrades in the SNOPT/IPOPT concise-code style.

Sequencing constraint over everything: M5 closes both trees first
(tycho Task 14 → tycho M5 PR → owner merge). The M6 brief is written
after that close; this note is the raw material.

---

## 0. The end state — what "state of the art" means here

The goal is measurable, not aspirational:

- **Robustness**: success rate at fixed tolerance on the standard
  sets ≥ Ipopt's, with no problem class where hven is the outlier
  failure.
- **Speed**: within the leaders' band on shifted geometric mean over
  the Mittelmann-style sets (counters primary, §7-clean wall
  informational), and AHEAD of them on warm-started and sequenced
  workloads — continuation, mesh refinement, crossover chains —
  because that is where hven's structural advantages (the
  declaration-keyed currency, engine-peer composition, the polish
  handoff's measured constant-cost re-solve) have no published peer.
- **First-order-only**: beat SNOPT on most if not all problems when
  only first derivatives are available (owner target, on the record).
- **Presentation**: trace, diagnostics, and documentation at the
  quality bar of the commercial solvers — a stranger with an NLP can
  read a run.

The strategy the milestones implement: fix the known weak cells —
acquisition and degeneracy both remediated by the M6 tier (IP-PMM's
proximal regularization needs no LICQ and is the named fallback for
the frozen/degenerate cells where bulk SSN stalls), scaling by its
M6 opener — then compose the engines adaptively where measurement
supports it, then tune and prove it publicly. Lead with the sequenced-workload strength; reach parity on
one-shot cold solves.

## M6 — the acquisition tier and the certified core

The big milestone: IPQP + feasibility mode as one scope, on a
foundation cleaned first. Two decisions gate its middle: the
build-vs-re-register ruling on tycho_sqp P7 G1/G2, taken AFTER the
precondition experiment.
SCOPE VALVE (review finding, held for the brief): this is a heavy
milestone. The brief may split it — openers + experiment + API break
+ quality floor as M6a, the tier as M6b — without renegotiating this
roadmap; and benchmark suite v1 has NO dependency on the tier, so
its build may start as an M6-parallel lane in the harness repo even
though it is ratified as M7 scope.

**Openers (parallel, small, before the tier):**
- TU-conformance of the three big SQP headers (qp_engine.h 5.3k /
  sqp_driver.h 4.1k / ssn_engine.h 3.6k) — so IPQP does not land as a
  fourth 4k-line header. SQP-side opener by its own proposal.
- Problem scaling (documented gap; HS25 row, S=1e12 fixture) — small,
  independent, precondition for honest dual-scale populations later.
- R6 sign sweep on face prices — must precede crossover-consuming
  work (negative prices would leak through the currency).
- self_check_kkt non-finite fix; SSN tie-fixture pin relaxation
  (margin-based uncertain membership or the portable property).
- No-copy claim-stream path (model/provider side, hven lane — runs
  parallel to the SQP openers). Closes the Transcribe_16seg deviation,
  which stays OPEN until this lands (owner ruling).

**The precondition experiment (~1 week, before any IPQP spec):**
barrier active-set acquisition at nx=1e5 on near-boundary cells
(PIQP oracle + corpus taxonomies). The oracle's 190x margin certified
linear-algebra cost only — every measured cell was effectively
equality-constrained. The experiment's result feeds the owner's
build-vs-re-register decision on P7 G1/G2.
CONTINGENCY (so a NO-GO is a branch, not a stall): if barrier
acquisition at scale does NOT hold its margin on near-boundary
cells, the fallback scope is (a) re-register G1/G2 with the measured
evidence as the record, (b) keep feasibility mode on the existing
elastic/restoration machinery (it is committed independently of
IPQP), and (c) redirect the tier effort at the escape-path target
(M7) and warm-start repair, which pay off under either verdict. The
one-brief ruling covers both branches: the brief names the fork and
the evidence that decides it.

**The tier (on a GO):** IP-PMM (Pougkakiotis–Gondzio) on the existing
KKT stack, specialized to fixed Hessian + fixed sparsity —
interior/'s first scheduled second consumer, so the IPM lane is in
the design room. Feasibility mode (owner-COMMITTED) scoped WITH it as
the certified fallback IP-PMM lacks. Warm restart defined against
WarmStartData + ipm.polish.v1 — resolving the reserved mu adoption
ruling — never a new currency; this IS the repair-based warm-start
work (strict-positivity/centrality restoration per
Gondzio–Grothey / Skajaa–Andersen–Ye), the same item the NO-GO
contingency redirects to, so it survives either verdict. Mode-selection TELEMETRY lands here
(per-family counters: bulk-flip counts, weak-activity fractions,
constraint tags) as instrumentation only — the heuristics that act on
it are M7. The machine-trace SCHEMA (the M8 trace item's JSON-lines
event stream) is versioned v0 here and carried by M7's telemetry, so
the heuristics never grow a second bookkeeping path for M8's trace
to replace.

**API break window (committed M6 items):** one declared break
carrying everything at once: OptimizationProblemBase retirement/fold
(consumer-free confirmed by tycho Task 8); callback const-argument
redesign; the model-layer rename, Scheme 1 as owner-decided
2026-08-21 (NLPProblem → NlpTripletModel, NlpAggregate → NlpAssembly
with the Assembly* cascade — one proven mechanical sweep,
P-SYM-class verification, never incremental; HARD CONSTRAINT: the
"free" deadline is M7-eve, before Python bindings exist); and the
by-value eval_* deprecation in favour of the in-place virtuals (its
registered M6 neighbour). Nothing renamed or deprecated
anticipatorily before the window.

**Quality floor:** drivers coverage 67.8% → parity with the library;
the five 0% inline headers; census comparator into scripts/ with the
1e-5 gate; LTO-on build exercised once.

**Also riding M6 (tycho_sqp standing carries, adjudicated in the
brief):** ws_algebra selection rule; the N∈{800,1600} p=0.90 and
N=850 DNF cells; the Uno warm-chain latent fix (must land before any
Uno warm re-run); the per-minor cost gap vs Uno (13.6–35.4x) as a
named investigation.

**M6 exit criteria:** the acquisition decision is TAKEN (either
branch) and its evidence artifact is on the record; feasibility mode
ships with its refusal/fallback contract pinned; ALL FIVE openers
are closed (three headers TU-conformant and prose-cleaned; scaling
landed; R6 sign sweep landed; self_check_kkt and the SSN tie-fixture
pin fixed; no-copy path landed and the Transcribe deviation closed);
the API break is one declared event with both trees consuming; the
quality floor is met (drivers coverage at parity, comparator in
scripts/, LTO exercised); the four tycho_sqp riders are each either
closed or explicitly adjudicated non-gating in the brief; the usual
close gate (replay, smoke, rig, suite) is green.

## M7 — adaptivity and the first-order mode

Internal ordering: the suite comes FIRST — the heuristics, the
ratification ladder, and the first-order design are all judged on
it, so it cannot arrive last.

- **Benchmark suite v1** (the instrument; sibling private repo per
  ruling): the standard sets this solver class is judged on —
  Hock–Schittkowski (in-suite already), CUTEst and the AMPL-NLP
  Mittelmann/Plato sets — harnessed for counters + §7-clean timings,
  with local SNOPT and IPOPT lanes wired in from the start. Direct
  comparability requires speaking the sets' native format: an
  AMPL .nl reader (ASL-based) bridging to the hven model contract is
  part of v1 and lives IN THE PRIVATE HARNESS REPO, full stop — hven
  itself ships no .nl code. If it is ever to move into hven as a
  public frontend, that is a separate decision that opens the ASL
  license/notices question first; registered on the watchlist. Suite discipline from
  day one: problem-set pins, per-cell budgets, and a train/holdout
  SPLIT so M8's tuning cannot overfit the set it reports on.
- **Adaptive mode composition** (owner directive, WIDENED
  2026-08-26, unforced): not only the cold-start case — the question
  is whether the solver can DETECT characteristics of the problem at
  the current point (active-set stability / bulk-flip rate,
  weak-activity fraction, inertia and conditioning evidence the
  factorizations already produce, progress stagnation, degeneracy
  indicators) and swap to the better-suited algorithm MID-SOLVE.
  hven's structural advantage: the M5 warm-start currency IS the swap
  mechanism — an engine switch is export → stage on declared-space
  values, a first-class composition rather than an internal hack.
  The survey's promote/demote triggers are the starting criteria;
  the M7 lit review covers the switching literature explicitly
  (Knitro's multi-algorithm selection and IP→active-set crossover
  being the closest production precedent). "Unforced" is the rule —
  if measurement says static per-family selection beats switching,
  the heuristics ship as configuration guidance, not machinery.
  DESIGN CONSTRAINT (owner, 2026-08-26): adaptive switching must not
  step on the polish/crossover feature. The explicit user-driven
  composition — the caller exports, stages, and sequences engines
  per call, full control, the M5 surface — stays first-class and
  untouched; automatic switching is a separate opt-in layer that
  CONSUMES the same currency mechanism rather than rewiring it, so
  a switch the solver makes and a switch the user makes are the same
  operation with a different trigger. More user control is good or
  bad depending on the user; hven offers both and forces neither.
  SETTINGS CONTRACT (owner, 2026-08-26): every switching mode and
  every heuristic is individually enable/disable-able through the
  settings surface — nothing adaptive is reachable only as a bundle —
  with enabled-defaults chosen intelligently from measurement (the
  M7/M8 suite data, not intuition), so the default user gets the
  measured-best behavior and a power user can pin any subset for a
  specific problem. This composes with the labeled-configs item
  (M8): a labeled config is a curated setting of these same toggles,
  never a separate mechanism.
- **Escape-path performance target**: 95.5% of residual
  PSIOPT-envelope wall sits in the 2/9 steps where SSN escaped to the
  walk — the best-localized target P7 produced, and the natural first
  beneficiary of the acquisition tier.
- **First-order-only mode (quasi-Newton)**: lit review first (the
  registered Hessian-approximation note governs), then design. Needs
  mode-selection to exist (a first-order mode is a mode) and must
  answer what inertia evidence means under a PSD approximation (T4
  certification, R5). The beat-SNOPT lever: SNOPT's dense
  reduced-Hessian Θ(nS²) cost is exactly what a sparse partitioned
  approximation avoids.
- **Globalization ratification ladder** (all opt-in today): R5
  default-flip fixture, R4 1e-4 re-derivation, R3 arming rule, R7,
  R9, τ re-derivation on an ill-conditioned population — worked
  through against the M7 benchmark instrument (above) rather than
  one-off fixtures, on the TRAIN split only, like every default
  chosen before M8 reports.

**M7 exit criteria:** suite v1 runs all lanes end-to-end with
pinned sets and budgets; every shipped heuristic has a measured
win over its own OFF state on the suite's TRAIN split (a heuristic
that cannot show one ships disabled; the holdout stays untouched
until M8); the first-order design is approved with
its inertia-under-approximation question answered; the escape-path
share of residual wall is measurably reduced or the finding is
re-adjudicated with evidence.

## M8 — tuning, comparison, and presentation

- **Default parameter tuning, both engines**, on the M7 suite's
  TRAIN split, reported on the holdout. Methodology candidates
  (unforced, chosen by pilot): automated algorithm configuration in
  the SMAC/irace tradition vs structured manual sweeps — automated
  configuration is the field's standard for this and composes with
  the per-toggle settings contract, but it earns its complexity only
  if the pilot shows headroom manual sweeps miss. Labeled configs
  where the data supports them (unforced — named profiles like
  "trajectory", "degenerate", "first-order-only" only if clusters
  actually separate).
- **First-order-only mode: implementation** (design approved in
  M7): the quasi-Newton mode is BUILT and certified here — pins for
  its inertia-evidence semantics, suite lanes, and its own toggle in
  the settings surface — so the beat-SNOPT measurement below runs
  against shipped code, not a prototype.
- **The published comparison**: hven-vs-SNOPT/IPOPT locally,
  Knitro via published Mittelmann results, on the suite's standard
  cells — reported in the field's own conventions so the numbers are
  citable: Dolan–Moré performance profiles, shifted geometric means,
  and success rates at the published tolerance, with hven's counters
  alongside. The comparison declares its instrument per §7
  (serialized wall-asserting runs), and cross-machine comparability
  against Knitro's published numbers is CALIBRATED, not assumed: a
  common reference solver (Ipopt, run both locally and in the
  published tables) anchors the ratio, and claims against Knitro are
  stated relative to that anchor, never as raw cross-hardware
  seconds. It separates the two claims: the
  one-shot cold-solve standing, and the sequenced-workload standing
  (continuation/mesh/crossover chains) where the currency should
  show clear water. The first-order-only target (beat SNOPT with
  first derivatives only) is measured here.
- **Solver-trace upgrade**: two surfaces, one event stream. The
  human trace: concise per-iteration lines in the SNOPT/IPOPT
  tradition — mode/tier column (acquisition vs SSN vs walk vs
  restoration vs elastic), condensed event codes, a legend block,
  and a solve-summary footer (exit condition, counters, KKT set).
  The machine trace: the same events as a structured stream (JSON
  lines), which the harness, the tuning runs, and the heuristics'
  own telemetry consume — no second bookkeeping path. Specified
  early in M8 so tuning runs read well; the trace is also where the
  mode-switch heuristics' decisions become visible and auditable.
- **User-facing documentation**: a solver a stranger can adopt —
  options reference generated from the settings surface (one source
  of truth with the toggles), the trace legend, a getting-started
  path, and the labeled configs' guidance. SOTA is partly
  presentation; this is the presentation milestone.

**M8 exit criteria:** tuned defaults adopted with the train/holdout
evidence on the record. HARD GATES (no gap-registration): the
comparison artifact published in the conventions above; the
first-order mode shipped and its beat-SNOPT standing measured
(whatever the result says); the presentation bullet (trace both
surfaces + documentation) delivered. The remaining §0 bullets
(robustness and speed standings) exit either met or carrying a
named, registered gap with its owning follow-on — those standings
depend on where the field is, not only on us.

---

## Beyond M8 — SOTA watchlist (registered, unscheduled)

Kept visible so the end state stays honest; none is forced into
M6–M8:

- **NLP presolve** (fixed/implied bounds, redundant rows, variable
  fixing): every commercial peer presolves; our declaration layer is
  the natural host.
- **Iterative refinement / mixed-precision factorization**: the
  modern lever on huge KKT systems; interacts with the residual
  near-ulp discipline.
- **Parallelism story beyond MKL's internal threading** (declared
  thread modes exist since M4; a deliberate multi-solve/multi-core
  strategy does not).
- **Parametric sensitivities** (post-solve dS/dp): what tycho-class
  consumers ask for next; the currency is the natural carrier.
- **MPCC/complementarity handling**: only if the benchmark evidence
  says the problem class matters to our consumers.
- **Python bindings** (bindings/ component exists in the layout;
  unscheduled): the adoption surface for the suite and for outside
  users.
- **Public .nl / external-format frontend in hven proper** — opens
  the ASL license/notices question; until then all .nl code stays in
  the private harness.

---

## Owner decisions (ruled 2026-08-26, direct)

1. Build vs re-register on tycho_sqp P7 G1/G2 — OPEN by design:
   taken after the precondition experiment reports.
2. Feasibility mode + IPQP: ONE BRIEF.
3. Header-prose cleanup: scope EXTENDED — the pre-M5 settlement
   prose rides the M6 TU-conformance opener (each big header touched
   once).
4. Benchmark suite: SIBLING HARNESS REPO, PRIVATE for now — problem
   sets, local SNOPT/IPOPT lanes, and published-result tables stay
   out of Apache-2.0 hven; hven stays consumable-clean.

## Amendment (2026-08-31, owner-approved; tycho-sqp lane reviewed): first-order mode moves to M9

Ruling (Grant, direct, after settler proposal and tycho-sqp review): the
first-order (Hessian-approximation) mode leaves M7 and becomes its own
milestone, split into a reading/design gate and an implementation:

- **M7 = adaptivity heuristics + benchmark suite v1.** First-order mode
  REMOVED. Two constraints carried so M9 does not have to reopen M7's work:
  (1) *Don't foreclose* — no heuristic reads the model Hessian outside the
  provider path; nothing assumes H's sparsity is fixed by the model; the KKT
  consumer takes H from an engine-owned "current Hessian" handle of which the
  provider-laid arena is ONE source, and heuristics/telemetry read the handle,
  never the arena. (2) *Cost model* — any heuristic pricing factorizations
  against evaluations carries curvature acquisition as its own cost line and
  never reads `eval_hess` (zero under a first-order mode); telemetry keyed on
  the bridge-lay `+1 eval_hess` identity is tagged exact-Hessian-specific;
  every curvature-reading heuristic (ladder counters, rho demanded, the item-4
  read, SSN certification tiers) is labelled with the curvature source it
  assumes — no decision, a label. Benchmark suite v1 carries a
  `hessian_provenance` column (exact / family-tag / none) from its first
  schema.
- **M8 = tuning + labelled configs + SNOPT/IPOPT/Knitro comparison + trace
  upgrade**, with the comparison EXPLICITLY scoped "second derivatives
  available" (SNOPT cannot accept a Hessian, so the like-for-like claim is not
  attempted in M8). The accuracy-matching rule is defined ONCE here under the
  owner's standing ruling (natural tolerance is the bar, matched accuracy is
  context) so M9b inherits it; the calibration grid's absent cell (no external
  solver on a non-F7 problem) is filled in M8, and M8's comparison set is chosen
  so M9b's like-for-like set is a SUBSET of it.
- **M9a = first-order literature review + design + owner ruling** on the
  approximation family and on what "beat SNOPT" concretely means and on which
  set — reusing the ratified two-clause band (coverage within fixed budgets +
  speed at K) as its template, with the clause-(b) vacuity lesson written in.
- **M9b = first-order implementation + the like-for-like SNOPT pass.**

Rationale: first-order mode is the least de-risked roadmap item; M8 must
measure the exact-Hessian engines in isolation (one variable, not two); M8's
results tell M9a where hven loses to SNOPT so the design is targeted; the
M9a/M9b gate is the precondition-experiment pattern M6 used. Two settler
proposals were RETRACTED under owner challenge and confirmed retracted by the
tycho-sqp lane: a Hessian-provider seam reserved in M7 (a seam now would guess
M9a's answer; the tagged-extension currency already carries quasi-Newton
history; per-mode tuning is M8's labelled-config mechanism) and an M7 scoping
read (M9a covers it). tycho-side: no timeline pressure — tycho's design premise
is cheap exact Hessians through the partitioned engine; first-order is external
positioning.


---

## M7 items registered at M6 W1 close (2026-09-01)

Accumulated from W1's task closes (arguments in the M6 ledger and the W1 spec
§12); listed here so M7's planning window starts from the full set:

- **Stopping-mu vs bound geometry** (C1's accuracy/stopping coupling; the
  gate-8 Sigma ~ 250 fixture and T10's `e1_f7_n20000_af30_m1e-6` — an
  active-set margin equal to the tier's contracted accuracy floor — are its
  two motivating cases).
- **`ipqp_converge_slack` as an adaptivity lever** (measured price of slack 1:
  +47 iterations over 29 cells; the 1e2 hand-off to tier 3 is the ruled
  composition).
- **Complementarity-derived `mu_0`** after the starting heuristic
  (Mehrotra/PIQP style), with the fixed measured default as fallback;
  per-family defaults (HS prefers 1e-3, everything else 1e-2 — T10's sweep).
- **Sticky trial** (NOT ADOPTED in W1: freeze class); **IC constants
  measurement**; **barrier endgame at the guard**; **`kIpqpLadderSkipAfter`**
  (T9 lever).
- **Tier-solved elastic rung** (owner: robustness over architectural purity —
  deferred, not declined).
- **Restoration seed generalization** beyond `clamp(x + p_elastic, box)`.
- **Dropped warm carry on a disproval** (T7/T9: carry DECLINED at 0-26%
  measured saving with the /3-descent hazard; revisit only with new evidence).
- **Structured per-piece convexification** (needs a second seam view —
  sized as such, spec §2.2).
- **A certification fixture class that survives barrier-default moves**
  (T10's gate-8 lesson: fixtures constructed from `Sigma = 2 mu_stop / s^2`,
  not tuned to where the barrier happens to stop).
- **A real tier envelope population**: cells the walk finds hard at
  `nx = 1e5` (the current corpus has none — T10's A13 substitute measures a
  one-factorization-walk population).
