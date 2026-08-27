# E1 acquisition experiment — implementer report

Status: **DONE**. All 20 cells ran and produced a real row; none timed out,
none failed, no counter is estimated or fabricated.

Artifact: `/home/ghecht/Projects/hven-e1/artifact/`
Workspace: `/home/ghecht/Projects/hven-e1/` (disposable; nothing migrates)

**No verdict is written anywhere in this artifact.** The GO/NO-GO reading
against the pre-registered criteria is the settler's.

## What was built

- `bridge/` — the archived tycho_sqp `prototypes/piqp_bridge/` copied in whole
  (`build_piqp.sh`, `piqp_f7_driver.cpp`, `README.md`, `results/`), plus this
  experiment's own new sources. Nothing in either archive was edited; the
  workspace also holds read-only copies of `tycho_sqp/include`,
  `tycho_sqp/tests/support`, and tycho's vendored Eigen/fmt, so it is
  self-contained.
- `bridge/e1_dump.h` — the QP container and its on-disk text format: the
  archive's own `write_qp_dump` triplet format, extended additively with the
  E1 taxonomy tags and the constructed ground truth. Ground-truth `x*` and
  multipliers live in a separate `.sol` sidecar, so the QP a solver is handed
  never carries its own answer.
- `bridge/e1_generate.cpp` — the cell generator (KKT-inverse construction) and
  its verifier.
- `bridge/piqp_e1_driver.cpp` — the solve arm, derived from the archive's
  `piqp_f7_driver.cpp`: same solve path, same bound-sentinel mapping, same
  external residual recomputed in unscaled data, same referee `gate`. New here:
  the E1 CSV, the acquired-vs-constructed active-set comparison under two
  declared rules, the derived factorization count, and a per-row disagreement
  diagnostic.
- `bridge/run_verification.sh`, `bridge/run_sweep.sh`,
  `bridge/stamp_provenance.sh`, `bridge/build_e1.sh`.

PIQP v0.6.3 was already installed at `~/Software/piqp`; `build_piqp.sh` is
idempotent and skipped the clone/build. Its step 2 (compiling the *archive's*
driver from a repo-relative path) fails in this workspace, which is expected
and inert — `build_e1.sh` compiles this workspace's tools instead.

## Verification performed before the sweep (step 3)

`artifact/KKT-VERIFICATION.log`:

- **Referee gate**, all three tolerances, through this workspace's derived
  driver: `x_err_inf` = 1.68e-7 / 1.68e-9 / 1.68e-11 at eps_abs 1e-10 / 1e-12 /
  1e-13 — reproducing the archive's committed `gate_tolerance_sweep.txt` to
  three significant figures. PASS at the default 1e-12.
- **One small instance per cell class** (3 fractions x 3 margins at N = 200,
  n = 1000, mi = 200), each generated, written to disk, **read back**, and
  checked on the re-read data: stationarity (4e-16 against an O(1) scale — exact
  cancellation), equality feasibility (0), inequality feasibility, active rows
  holding with exact equality, inactive rows strictly feasible, strict
  complementarity (active multipliers > 0.5, inactive exactly 0), `|A|` equal to
  `round(fraction * mi)`, the 10th-percentile relative margin landing on the
  taxonomy's `m`, the box strictly inactive at `x*`, `H` positive definite
  (sparse Cholesky), and **LICQ: `[Ae; Ai_A]` full row rank**. All 9 classes
  pass.
- **Anchor reproduction** against the committed oracle artifact: both anchors
  reproduce `iter = 9`, `ext_stationarity` 2.0688919e-12 / 2.3507056e-12 to
  every printed digit, `max_ci` -0.275 / -0.210632, `0/20000` active.

`artifact/KKT-VERIFICATION-full-size.log`: the same KKT check re-run at each
swept cell's own size, all 18 constructed cells, all pass. (The LICQ rank test
is recorded as SKIPPED above n = 5000 — a sparse QR of the 60000 x 100000
staircase is not affordable, and the structural argument is size-independent.)

**One real defect was caught by this verification and fixed before any cell
ran**: the first draft of the generator sampled the active set from all `mi`
rows, and the LICQ check reported rank 619 of 620 — row 0. F7's path row `k`
reads node `k`'s state block only, and node 0's state block is pinned by the
three initial-condition equality rows, so row 0 lies in `Ae`'s row space and
activating it destroys LICQ. Row 0 is now structurally excluded from every
active set. This is exactly the failure mode the step-3 requirement exists to
catch.

## The 20 cells

| id | status | iters | active found | active true |
|---|---|---|---|---|
| `e1_f7_n4000_af01_m1e-2` | solved | 15 | 40 | 40 |
| `e1_f7_n4000_af01_m1e-4` | solved | 14 | 40 | 40 |
| `e1_f7_n4000_af01_m1e-6` | solved | 16 | 40 | 40 |
| `e1_f7_n4000_af10_m1e-2` | solved | 12 | 400 | 400 |
| `e1_f7_n4000_af10_m1e-4` | solved | 11 | 400 | 400 |
| `e1_f7_n4000_af10_m1e-6` | solved | 14 | 400 | 400 |
| `e1_f7_n4000_af30_m1e-2` | solved | 11 | 1200 | 1200 |
| `e1_f7_n4000_af30_m1e-4` | solved | 10 | 1200 | 1200 |
| `e1_f7_n4000_af30_m1e-6` | solved | 13 | 1200 | 1200 |
| `e1_f7_n20000_af01_m1e-2` | solved | 18 | 199 | 200 |
| `e1_f7_n20000_af01_m1e-4` | solved | 15 | 200 | 200 |
| `e1_f7_n20000_af01_m1e-6` | solved | 17 | 200 | 200 |
| `e1_f7_n20000_af10_m1e-2` | solved | 12 | 2000 | 2000 |
| `e1_f7_n20000_af10_m1e-4` | solved | 12 | 2000 | 2000 |
| `e1_f7_n20000_af10_m1e-6` | solved | 14 | 2000 | 2000 |
| `e1_f7_n20000_af30_m1e-2` | solved | 11 | 6000 | 6000 |
| `e1_f7_n20000_af30_m1e-4` | solved | 9 | 6000 | 6000 |
| `e1_f7_n20000_af30_m1e-6` | solved | 13 | 6000 | 6000 |
| `e1_anchor_f7_n20000_bound_neutral` | solved | 9 | 0 | 0 |
| `e1_anchor_f7_n20000_path_neutral` | solved | 9 | 0 | 0 |

`active found` is Rule A (slack rule, threshold `1e-8 * row_scale`, declared
before the sweep). Iterations range 9–18; every cell reported `solved` with
`res_primal <= 3.6e-12` and `res_dual <= 9.4e-11`. Full residuals, dual-rule
counts, set-agreement columns, `x_err_inf`, `min_box_slack`, `max_ci`,
`max_abs_z_u` and wall are in `artifact/e1_cells.csv`.

**Determinism.** Two independent full sweeps were compared: every non-timing
column, residuals at 17 digits included, was bit-identical.


## Follow-on: contiguous activity window (9 cells, added after the first sweep)

Run at the settler's request to address fidelity concern 1 below. Same
construction, same budgets, same counting rules, same driver binary; the only
difference is that the active set is one contiguous block of rows instead of a
uniform random subset. `N = 20000` only, crossed with the same three fractions
and three margins. Written to `variant_contiguous.csv` — **not merged into the
pre-registered `e1_cells.csv`**.

| id | status | iters | active found | active true | block rows |
|---|---|---|---|---|---|
| `e1blk_f7_n20000_af01_m1e-2` | solved | 12 | 200 | 200 | 6571–6770 |
| `e1blk_f7_n20000_af01_m1e-4` | solved | 15 | 200 | 200 | 4393–4592 |
| `e1blk_f7_n20000_af01_m1e-6` | solved | 17 | 200 | 200 | 2196–2395 |
| `e1blk_f7_n20000_af10_m1e-2` | solved | 13 | 2000 | 2000 | 7088–9087 |
| `e1blk_f7_n20000_af10_m1e-4` | solved | 15 | 2000 | 2000 | 8010–10009 |
| `e1blk_f7_n20000_af10_m1e-6` | solved | 18 | 2000 | 2000 | 12414–14413 |
| `e1blk_f7_n20000_af30_m1e-2` | solved | 12 | 6000 | 6000 | 9176–15175 |
| `e1blk_f7_n20000_af30_m1e-4` | solved | 15 | 6000 | 6000 | 1974–7973 |
| `e1blk_f7_n20000_af30_m1e-6` | solved | 17 | 6000 | 6000 | 12102–18101 |

All 9 solved, iterations 12–18, `res_primal <= 4.1e-13`, `res_dual <= 8.6e-11`,
and the active set recovered **exactly** on every cell — zero missed, zero false
positives under the primary slack rule. (For contrast, the scattered cells at
the same size ran 9–18 iterations with one row missed by the slack-rule
threshold; the settler will want to read those two bands side by side, which is
why the variant CSV carries the identical schema.) Two independent full runs of
the variant were bit-identical on every non-timing column.

**LICQ was re-verified rather than inherited**, since a contiguous block sits
differently against the collocation staircase. The exact sparse-QR rank test is
superlinear here — instant at 660 rows, 11–16 s at 6600, 171 s at 16500, past a
300 s budget at 33000 — so it cannot be paid at the swept size's 66000 rows.
Four tiers instead: exact rank at `N = 200` (all 9 classes), exact rank at
`N = 2000` (all three fractions; the margin class cannot affect LICQ because it
only sets inactive rows' slacks, which never enter `[Ae; Ai_A]`), exact rank at
`N = 5000` with 30% active (16500 rows — the largest affordable, at the most
stressing fraction), and a cheap `LDL^T`-of-`J J^T` certificate at the full
`N = 20000` on every swept cell. All pass. The cheap test's caveat is stated in
the artifact: forming `J J^T` squares the condition number, so it is a numerical
certificate rather than an exact rank; what makes it usable is that the two
tests agreed on every cell where both ran, and its full-size margin is real
(`min|D|` ~ 1e-9 against `max|D|` = 2.0 over 66000 rows).

**The offset shift rule was implemented and never fired.** Every drawn offset
passed the LICQ certificate on the first try; no cell was
`infeasible_by_construction`. The rule and its recording path exist in
`run_variant_contiguous.sh` and `e1_generate.cpp` regardless, and the offsets
actually used are in `variant_contiguous_layout.csv`.

**Housekeeping.** Adding the variant required a new layout mode in the generator
and two additive metadata tags in the dump format. The original 20-cell sweep
was therefore re-run under the final build and compared against the copy
produced before those changes: **zero non-timing differences**, residuals at 17
digits included. No pre-registered number moved.

## Deviations from the protocol

1. **Row 0 excluded from every constructed active set** — structural, to avoid
   turning an acquisition cell into an LICQ-degeneracy cell. `|A|` unchanged.
   (Detail above; also in `artifact/README.md`.)
2. **Anchors rebuilt from `F7CollocationChain` rather than dumped by
   `tycho_sqp_corpus --dump-qp`** — the corpus runner needs a full MKL build of
   the tycho_sqp library, which this out-of-tree workspace deliberately lacks.
   The neutral-cold branch of `first_qp_for_cell` is three lines and is
   reproduced exactly; agreement with the committed oracle row to every printed
   digit is the evidence.
3. **Both anchors at nx = 1e5.** The oracle's node grid contains no N = 4000
   cell, so re-running *original* oracle cells forces both anchors to N = 20000
   (bound and path windows).
4. **`factorizations` is derived, not reported.** PIQP's public `Info` has no
   cumulative factorization counter (`factor_retires` is reset after each
   success). From the pinned v0.6.3 control flow — one factorization site in
   initialization, one per main-loop iteration — `factorizations = 1 + iter`,
   excluding unobservable regularization retries. Labelled as a derivation
   everywhere it appears; `factor_retires_final` carried beside it (0 on every
   row).
5. **One Rule-A count is one short of its constructed set.** The threshold was
   pre-declared and was NOT moved afterwards. `e1_f7_n20000_af01_m1e-2`: row 557
   has slack 1.524e-9 against a 1.142e-9 threshold — relative slack 1.33e-8,
   a factor 1.33 the wrong side of the `1e-8` rule — while its dual
   `z_u = 0.529` is unambiguously that of an active row, and Rule B counts it
   (dual count 200/200). Recorded with its numbers in `sweep.log`, not smoothed
   away.

## Concerns about cell-construction fidelity

Stated so the settler can weigh them rather than discover them.

1. **The active set is a uniform random subset of rows; F7's own geometry
   produces a contiguous junction window.** — **ADDRESSED** by the contiguous
   variant above, which measures the same 9 fraction x margin classes at
   `nx = 1e5` with the activity laid out as one solid block. What remains
   unmeasured is narrower than the original concern: the variant fixes the
   *layout*, not the *location* — its block offset is drawn uniformly rather
   than placed where F7's own junctions would put it, and the block is a single
   window rather than the two-junction structure a real bound-arc hop can
   present.
2. **`x*` is a random-ish point, not a trajectory the family would visit.**
   Manufacturing `g`/`be`/`bi` is what makes the true active set knowable, and
   the protocol asked for exactly that; the cost is that the cell's *data* is no
   longer a physically meaningful linearization even though its *structure* is.
   In particular the QP's conditioning need not match a real F7 hop's.
3. **The box is inactive by construction on every constructed cell**
   (`min_box_slack` ~ 0.6 everywhere). The taxonomy's axis is inequality-row
   activity, so this isolates the variable under study — but it means no cell
   here exercises simultaneous bound activity, which a real F7 bound-arc hop
   would have.
4. **`x_err_inf` is loose on this family** (a ~1e-11 residual maps to ~1e-5 in
   `x_err_inf`). That is F7's KKT conditioning constant, already visible in the
   archived gate's own three-tolerance table, not a construction fault. The
   asserted quantities are the counters and the KKT residuals.
5. **PIQP is a proxy.** The protocol says so, and it bears repeating in the
   report that feeds the decision: this measures a primal-dual interior-point
   implementation's acquisition behavior on these cells, which transfers as
   evidence about the method class, not as a measurement of an in-house IP-PMM.

## License and firewall compliance

PIQP was built and installed **externally** (`~/Software/piqp`) and referenced
by path. No PIQP source, header, or CMake package file was copied into this
workspace or into any repository. Nothing here migrates. No SNOPT material was
read or touched. `hven`, `tycho_sqp`, and `tycho` were read only; not one file
in any of them was created, modified, or deleted by this work. Access to
`tycho` was a single `cp -r` of `dep/eigen` and `dep/fmt` into the workspace,
which cannot mutate the source.

One observation to record rather than leave implicit: at the end of this work
`hven` and `tycho_sqp` had clean working trees, while `tycho`'s working tree
showed 82 uncommitted entries. Those are not from this experiment — nothing
here writes to `tycho` — and are presumably another session's in-flight work;
noted only so a later reader does not attribute them here. (`hven` also moved
from `6d6288e` to `08045fc` during the session, likewise not this work.)
