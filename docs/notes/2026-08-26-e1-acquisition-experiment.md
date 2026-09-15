# E1 — barrier active-set acquisition experiment (protocol)

Date: 2026-08-26. Status: protocol DRAFT; pre-registered before any
cell runs. Purpose: close the load-bearing caveat in the PIQP oracle
verdict (tycho_sqp docs/notes/2026-08-06-piqp-oracle.md §5(c)/§8) —
every cell measured there had an effectively EQUALITY-constrained
first QP, so IPM linear-algebra cost is certified but barrier
ACTIVE-SET ACQUISITION at scale is not. This experiment measures
exactly that gap. Its artifact feeds the owner's build-vs-re-register
decision on tycho_sqp P7 G1/G2 (the M6 brief's E1 gate).

## Question (pre-registered)

Does an interior-point QP solve acquire a NONTRIVIAL active set at
nx ≈ 1e5 on F7-pattern cells within the ruled gate — < 40 IPM
iterations and ≲ 30 s per solve — and does the iteration count stay
bounded as the active fraction and near-activity tightness grow?

## GO / NO-GO criteria (pre-registered, before any cell runs)

- GO: every cell class converges with iterations < 40 (counter,
  asserted) and no monotone iteration blow-up across the active-
  fraction sweep; failures confined to cells the taxonomy marks
  pathological.
- NO-GO: systematic failure or iteration blow-up on the near-active
  classes at nx = 1e5.
- Anything between (isolated failures, borderline growth) is
  reported as measured — the owner's decision weighs it; the
  criteria bound the automatic verdicts only.

## Instrument

- PIQP as the numerical ORACLE, un-vendored (LGPL-tainted sparse
  backend — oracle only, never vendored; CLAUDE.md §6 math-only
  discipline). The bridge is copied from the archived tycho_sqp
  prototypes/piqp_bridge/ (read-only source) into a scratch
  workspace OUTSIDE the hven tree; it remains disposable and does
  not migrate.
- Counters asserted: PIQP iteration count, factorization count,
  termination status, and the solution's active-set size. Residuals
  reported. Wall informational (single-threaded, stated); no wall
  claim is made, so cells may run without whole-box serialization —
  but never concurrently with a tycho-d7 wall-asserting gate (§7
  courtesy, coordinated by the settler).
- Per-cell budget: 120 s hard timeout, one retry on setup failure;
  a timeout is a recorded outcome, never an unbounded cell (§8).

## Cell taxonomy

F7-pattern QP structure (the pattern the oracle already certified
for linear algebra), sizes nx ∈ {2e4, 1e5}, crossed with:

1. **Active fraction at the solution**: ~1%, ~10%, ~30% of
  inequality rows active — constructed by KKT-inverse cell design
  (choose x*, active set, and consistent multipliers; derive the
  linear term), so the true active set is known by construction and
  "acquired" is checkable exactly.
2. **Near-activity margin**: the inactive rows' slacks drawn so the
  nearest decile sits at margins {1e-2, 1e-4, 1e-6} of row scale —
  the tie-pressure axis the oracle's cells lacked (nearest row was
  ≥ 0.21 from activity).
3. **Start**: neutral cold start only (the acquisition question);
  warm-start repair is W3's business, not E1's.

3 fractions × 3 margins × 2 sizes = 18 cells, plus the 2 original
oracle-class equality-only cells re-run as anchors = 20 cells.
One process per cell, sequential.

## Artifact

docs/notes/data/2026-08-m6-e1-acquisition/ — per-cell CSV (id,
n, mi, active_fraction, margin_class, status, iters, factorizations,
active_set_size_true, active_set_size_found, residuals, wall_info),
provenance stamp (toolchain, hardware, date, oracle version, bridge
commit of origin), the generator script, and a README carrying this
protocol verbatim plus the declared budgets. The verdict section is
written LAST, against the pre-registered criteria above.

## What this experiment is NOT

Not a p95 (single run per cell; the decision question is feasibility
of acquisition, not tail latency); not a warm-start measurement; not
an IP-PMM measurement (PIQP is a proxy for barrier-method
acquisition behavior — the verdict transfers as evidence about the
method class, and the brief says so wherever it is cited).
