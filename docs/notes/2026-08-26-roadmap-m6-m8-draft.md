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

## M6 — the acquisition tier and the certified core

The big milestone: IPQP + feasibility mode as one scope, on a
foundation cleaned first. Two decisions gate its middle: the
build-vs-re-register ruling on tycho_sqp P7 G1/G2, taken AFTER the
precondition experiment.

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

**The tier (on a GO):** IP-PMM (Pougkakiotis–Gondzio) on the existing
KKT stack, specialized to fixed Hessian + fixed sparsity —
interior/'s first scheduled second consumer, so the IPM lane is in
the design room. Feasibility mode (owner-COMMITTED) scoped WITH it as
the certified fallback IP-PMM lacks. Warm restart defined against
WarmStartData + ipm.polish.v1 — resolving the reserved mu adoption
ruling — never a new currency. Mode-selection TELEMETRY lands here
(per-family counters: bulk-flip counts, weak-activity fractions,
constraint tags) as instrumentation only — the heuristics that act on
it are M7.

**API break window (committed M6 items):** OptimizationProblemBase
retirement/fold (consumer-free confirmed by tycho Task 8) + callback
const-argument redesign — one declared break, together.

**Quality floor:** drivers coverage 67.8% → parity with the library;
the five 0% inline headers; census comparator into scripts/ with the
1e-5 gate; LTO-on build exercised once.

**Also riding M6 (tycho_sqp standing carries, adjudicated in the
brief):** ws_algebra selection rule; the N∈{800,1600} p=0.90 and
N=850 DNF cells; the Uno warm-chain latent fix (must land before any
Uno warm re-run); the per-minor cost gap vs Uno (13.6–35.4x) as a
named investigation.

## M7 — adaptivity and the first-order mode

- **Adaptive mode composition** (owner directive, unforced): cold
  IPQP acquisition → SSN/walk exploitation once the active set
  settles → stable-face finish, driven by heuristics developed on
  M6's telemetry. The survey's promote/demote triggers are the
  starting criteria; "unforced" is the rule — if measurement says a
  static per-family choice beats switching, the heuristics ship as
  configuration guidance, not machinery.
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
  through against the M7 benchmark instrument (below) rather than
  one-off fixtures.
- **Benchmark suite v1** (instrument, built here to serve M7 and M8):
  the standard sets this solver class is judged on — Hock–Schittkowski
  (in-suite already), CUTEst/AMPL-NLP (the Mittelmann/Plato
  comparability goal), harnessed for counters + §7-clean timings, with
  local SNOPT and IPOPT lanes wired in from the start.

## M8 — tuning, comparison, and presentation

- **Default parameter tuning, both engines**, on the M7 suite;
  labeled configs where the data supports them (unforced — named
  profiles like "trajectory", "degenerate", "first-order-only" only
  if clusters actually separate).
- **The published comparison**: hven-vs-SNOPT/IPOPT locally,
  Knitro via published Mittelmann results, on the suite's standard
  cells. The first-order-only target (beat SNOPT with first
  derivatives only) is measured here.
- **Solver-trace upgrade**: concise per-iteration trace in the
  SNOPT/IPOPT tradition — mode/tier column (acquisition vs SSN vs
  walk vs restoration vs elastic), condensed event codes, a legend
  block, machine-parseable. Specified early in M8 so tuning runs
  read well; the trace is also the natural surface for the
  mode-switch heuristics' decisions to be visible.

---

## Open owner decisions carried into the M6 brief

1. Build vs re-register on tycho_sqp P7 G1/G2 — after the
   precondition experiment reports.
2. Whether feasibility mode + IPQP are one brief (recommended) or
   feasibility-first with the IPQP hook named.
3. Scope extension for the pre-M5 header-prose cleanup pass (rides
   the TU-conformance opener naturally if granted).
4. Where the benchmark suite lives (hven bench/ vs a sibling
   harness repo) — affects M7 planning only.
