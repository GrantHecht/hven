# M6 register — SQP lane's submission on IPQP and SQP refinements

Date: 2026-08-26. Carrier: cross-session message from the SQP lane
(tycho-sqp-73; its sandbox archived read-only, tycho_sqp main @
91c4ec1). Mirrored here by the hven settler as the record; all source
citations are tycho_sqp docs/notes paths. Status of everything below:
REGISTER INPUT for the M6 brief — nothing is scheduled by this note.

## IPQP — state: ruled direction + survey + one oracle verdict; NO spec, NO code

- **Algorithm of record** (ruled direction, Grant 2026-08-04): in-house
  IP-PMM — Pougkakiotis–Gondzio, "Interior Point–Proximal Method of
  Multipliers for convex QP" (COAP 78:307, 2021, arXiv:1904.10369) —
  infeasible primal-dual Mehrotra predictor-corrector wrapped in
  proximal primal+dual regularization (quasi-definite KKT; no
  LICQ/strict-convexity requirement), assembled on the existing sparse
  symmetric-indefinite KKT / Pardiso–Accelerate stack, reusing
  fraction-to-boundary, inertia, and adaptive-barrier machinery.
- **Two-level barrier framing** (Grant 2026-08-06, normative, levels
  compose): (i) the QP-subproblem tier — a QP-focused IPM built from
  the migrated interior/ elements, specialized to fixed Hessian +
  fixed sparsity (rev B §5 calls it interior/'s "first scheduled
  second consumer"); (ii) NLP-level acquisition/crossover — DELIVERED
  by M5's polish extension. Level (i) is the M6 item. Grant's
  non-binding lean was to wait until post-merge because migrated IPM
  elements may BE the tier — the deferral point is now.
- **Role**: not an SSN/walk replacement — a three-tier composition:
  IP-PMM for cold acquisition + degeneracy fallback; SSN for warm
  exploitation; existing exact stable-face refinement for the 1e-10
  finish selected in bulk. Mode selection per-family by runtime
  telemetry (bulk-flip counts, weak-activity fractions, constraint
  tags). Promote/demote triggers recorded in the survey
  (tycho_sqp docs/notes/research/fable_research_qp_sqp_scaling_survey_claude.md).
- **Warm-start is the honest weak point** and the case for owning the
  tier: PIQP has no warm-start API; a primal-only near-solution can be
  worse than neutral — the adapter must restore strict positivity and
  centrality (Gondzio–Grothey 2003; Skajaa–Andersen–Ye 2013;
  Chen–Goulart–Jones arXiv:2512.00693). IP-PMM also lacks a
  homogeneous infeasibility certificate — feasibility mode is its
  natural certified fallback.
- **Measured evidence** (tycho_sqp docs/notes/2026-08-06-piqp-oracle.md):
  neutral-cold first-QP at nx=1e5: 0.157 s / 9 iterations / residual
  2e-12, ~190× inside the gate — n=1, not a p95. LOAD-BEARING CAVEAT:
  every measured cell's first QP was effectively equality-constrained
  (zero active inequality rows) — this certifies IPM linear-algebra
  cost on F7's pattern, NOT barrier active-set acquisition, which is
  UNMEASURED and is the precondition experiment before any spec.
- **License discipline** (standing): PIQP sparse backend is
  LGPL-tainted — oracle only, never vendored; Cyqlone/QPALM/FATROP
  papers-only; HPIPM/Clarabel usable as comparators.
- **Gate linkage**: tycho_sqp P7 G1/G2 are unpassable by QP-engine
  changes alone (2/8 population cells die on the kFullWarm setup hop =
  cold wide-window solve); the acquisition tier is the named fix;
  Grant has ruled neither build-it nor re-register — P7 results §11.1
  calls it the phase's largest open decision.

## Other SQP refinements (what / state / where)

- **Feasibility mode** (Grant-COMMITTED for M6): no spec anywhere yet.
  Existing anchors: elastic tier/ladder + restoration in the driver;
  ladder early-exit opt-in (default OFF — degenerate-Hessian
  counter-example). Shape it WITH IPQP (certificate-fallback role).
- **Quasi-Newton / first-order-only**: explicit v1 non-goal in the SQP
  design of record; no design. Lit anchors on file: OpenSQP
  (arXiv:2512.05392), SNOPT's dense reduced-Hessian Θ(nS²) cost
  analysis (the beat-SNOPT lever), Uno's hessian_model surface as a
  comparator. Interaction to resolve at design time: SSN certification
  tiers assume an exact (possibly indefinite) Hessian with inertia
  evidence — a PSD approximation must define what "inertia" means
  (T4 saddle certification, R5).
- **Problem scaling**: documented feature gap, no spec (HS25 row;
  S=1e12 dual-scale fixture 60/60-major non-convergence). Landed
  already: activity_rel_tol relative to hand-off dual scale. R8 and
  row-wise μ are the SSN-side levers but need a dual-scale population
  first (the scale corpus is dual_scale=1.0 everywhere).
- **Multiplier handling**: R6 = O(m) sign sweep on face prices (kSsn
  adopted negative face prices: 14 rows / 2,212 multipliers / max
  3.5e-6; accepted-with-disclosure, R6 the ruled fix). Watch:
  Ding–Feng–Li extreme-point dual representative.
- **Globalization/SSN levers** (all opt-in, no default changed,
  ratified 2026-08-08): R5 merged certification (default flip
  preconditioned on a weak-active-indefinite fixture); R1
  kResidualArmed; R4 false-positive axis (1e-4 re-derivation ordered
  before default change); R2 rejected; R3 conditional FBstab
  prox-probe (arm off kBudget exits); R7 FFK identification radius;
  R9 Zhang–Hager nonmonotone; τ re-derivation on an ill-conditioned
  population. SOC unchanged.
- **Best-localized perf target from P7**: 95.5% of residual
  PSIOPT-envelope wall sits in the 2/9 steps where SSN escaped to the
  walk (escape census 11 kBudget / 2 kInfeasibleSuspect; 13/0 under R4).
- **Standing tycho_sqp carries that collide with M6 scheduling**:
  ws_algebra selection rule; N∈{800,1600} p=0.90 DNFs + N=850 DNF;
  Uno warm-chain latent (driver can adopt an unconverged iterate as
  warm seed — must fix before any Uno warm re-run); per-minor cost gap
  vs Uno 13.6–35.4×; self_check_kkt non-finite fix; TU-conformance of
  qp_engine.h / sqp_driver.h / ssn_engine.h (5.3k/4.1k/3.6k lines)
  under the source-parity ruling.

## Dependency ordering (the lane's reading, adopted as register input)

1. IPQP spec ← the unmeasured barrier active-set acquisition
   experiment at nx=1e5 on near-boundary cells (~a week with the PIQP
   oracle + corpus taxonomies). Measure before spec.
2. IPQP ← interior/ consumability as a QP-specialized IPM (fixed
   Hessian + fixed sparsity, value-only refactorization) — an hven
   interior/-side interface question; design needs the IPM lane in
   the room.
3. IPQP warm-restart repair defines itself against WarmStartData +
   ipm.polish.v1 (zL/zU + mu is exactly the state an IP-PMM restart
   consumes) — no new currency.
4. Feasibility mode scoped WITH IPQP (one brief, or feasibility first
   with the IPQP hook named).
5. R6 sign sweep before/alongside crossover-consuming work (negative
   face prices would leak through the currency into z = zL − zU).
6. Quasi-Newton after IPQP's mode-selection telemetry exists (a
   first-order-only mode needs a place to select into); lit review
   first stands.
7. TU-conformance of the three big headers precedes any IPQP engine
   addition to qp/ (else it lands as a fourth 4k-line header) —
   proposed as the SQP side's M6 opening task.
8. Scaling: independent, small, early — precondition for honest
   HS25-class rows and any dual-scale population.
