# E1 — barrier active-set acquisition experiment: artifact

Scratch workspace: `/home/ghecht/Projects/hven-e1/`. **Disposable.** Nothing in
this workspace migrates into `hven`, `tycho_sqp`, or `tycho`; the PIQP bridge it
carries is a copy of an archived prototype and PIQP itself is installed
externally and never vendored.

**This artifact deliberately has NO verdict section.** The GO/NO-GO reading is
written by the settler against the criteria the protocol pre-registered, which
are reproduced verbatim below. Everything here is measurement and method.

## Contents

| file | what it is |
|---|---|
| `e1_cells.csv` | the 20 per-cell rows, with the provenance stamp as a `#` comment block at the top |
| `PROVENANCE.txt` | the same stamp, standalone: toolchain, CPU, date, PIQP version, bridge origin + commit, and the declared measurement terms |
| `KKT-VERIFICATION.log` | step 3: the referee gate at three tolerances, one small instance per cell class checked by direct KKT residual evaluation (LICQ rank test included), and the anchor reproduction check against the committed oracle artifact |
| `KKT-VERIFICATION-full-size.log` | the same KKT check re-run at each swept cell's OWN size, for all 18 constructed cells |
| `sweep.log` | the sweep's own stdout: one block per cell, exit code, and elapsed-including-parse |
| `generator/` | every source and script that produced the above |
| `variant_contiguous.csv`, `variant_contiguous_layout.csv`, `VARIANT-CONTIGUOUS-VERIFICATION.log`, `variant_contiguous_sweep.log` | the **follow-on contiguous-window variant**, added after the first sweep — see its own section below |

`generator/` holds `e1_generate.cpp` (the cell generator), `e1_dump.h` (the QP
container and its on-disk format), `piqp_e1_driver.cpp` (the solve arm, derived
from the archived `piqp_f7_driver.cpp`), `build_e1.sh`, `run_verification.sh`,
`run_sweep.sh`, `stamp_provenance.sh`, and `build_piqp.sh.from-archive` (the
archived PIQP install script, copied unmodified).

Reproduce with:

```bash
bash bridge/build_piqp.sh        # idempotent; installs PIQP v0.6.3 externally
bash bridge/build_e1.sh
bash bridge/run_verification.sh  # step 3 -- a precondition of the sweep
bash bridge/run_sweep.sh         # step 4 -- 20 cells, sequential
```

Note on `build_piqp.sh`: it is the archive's file verbatim, so its step 2
(compiling the archive's own driver from a path relative to the tycho_sqp repo
root) fails in this workspace. That failure is expected and inert — step 1, the
PIQP install, is the only part this experiment uses, and `build_e1.sh` compiles
this workspace's own tools.

QP dumps are **not** kept: they are regenerable byte-identically from the
generator, the seeds recorded in the CSV's cell ids and in `run_sweep.sh`, and
the pinned `scale_problems.h`. Two independent full sweeps were compared and
every non-timing column, residuals at 17 digits included, was bit-identical.

---

## Declared budgets and thread settings

- **Threads.** `MKL_NUM_THREADS=1` and `OMP_NUM_THREADS=1` exported for every
  cell. Neither is read by PIQP: it links no MKL, and its OpenMP pragmas
  compile out because `BUILD_WITH_OPENMP` defaults OFF and the pinned install
  never enabled it. The load-bearing evidence is the `ldd` output in
  `PROVENANCE.txt` — no `libgomp`, no BLAS, no MKL. Single-threaded.
- **Per-cell budget.** 120 s hard timeout (`timeout -k 5 120`) on the whole
  `run` invocation, dump parse included. A timeout is a **recorded outcome**:
  the row is written with `status=timeout` and `-1` in every counter column,
  never dropped, never estimated. No cell hit it.
- **Retry policy.** One retry on cell *generation* failure only. A solve that
  fails is recorded as it failed and is never retried. No retry fired.
- **Scheduling.** Sequential, one process at a time; no cell ever co-ran with
  another. The box was not otherwise serialized, which is admissible only
  because **no wall-clock claim is made from this artifact** — `wall_info_s` is
  informational, per the protocol's own instrument clause.
- **Solver settings.** `eps_abs = 1e-10`, `eps_rel = 0`, duality-gap check OFF,
  `max_iter = 200`. These are the archived driver's operating point:
  `eps_rel = 0` so only the absolute tolerance can declare convergence, and
  `max_iter` deliberately generous so the "< 40 iterations" question is read
  off `info.iter` *after* the solve rather than enforced as a budget.
- **Start.** Neutral cold on every cell — and not by choice: PIQP's public API
  has no primal/dual/slack seeding surface (`solve()` takes no arguments), so
  every row runs its own from-scratch barrier initialization.

## How a cell is built, and why its active set is known

`e1_generate.cpp` carries the full derivation in its banner. In brief:

1. **Structure is F7's own, not an imitation.** `H`, `Ae`, `Ai`, and the box
   are taken verbatim from `F7CollocationChain`'s neutral-cold first QP — the
   same linearization the oracle measured — at `N` nodes, 3 states, 2 controls:
   `n = 5N`, `me = 3N`, `mi = N`. Those blocks are independent of the family's
   parameter `p` (`start_point()` is, and so are the Jacobians/Hessian at a
   fixed `x`), so a constructed cell is window-independent and is recorded as
   `window=constructed` rather than claiming a bound/path identity it does not
   have.
2. **Values are constructed by KKT inversion.** Choose `x*`, an active set `A`,
   and multipliers (`lambda_e` free, `lambda_i > 0` strictly on `A` and exactly
   `0` off it); set `be = Ae x*`, `bi = Ai x* + s` with `s = 0` on `A` and
   `s > 0` off it, and `g = -(H x* + Ae' lambda_e + Ai' lambda_i)`. `H` is
   positive definite (checked), so `x*` is the **unique global minimizer** and
   `A` is exactly its active set.
3. **The box is inactive at `x*` by construction** (control coordinates drawn
   in `[-0.4, 0.4]` inside the `[-1, 1]` control box), so the bound multiplier
   is zero and the only activity in a cell is the inequality activity the
   taxonomy is about. `min_box_slack` is reported on every row.
4. **Margins.** Per inactive row `j`, the relative margin is the slack divided
   by `row_scale(j) = sum_k |Ai(j,k)| * |x*(k)|`. With `M` inactive rows and
   quantile position `q`, the assigned relative margin is
   `m * 10^(3*(q - 0.1))` — a three-decade log-uniform spread whose **10th
   percentile is exactly `m`** — and the quantile positions are randomly
   permuted across rows so the tight rows are not clustered in the collocation
   index.
5. **Row 0 is structurally excluded from every active set.** F7's path row `k`
   reads node `k`'s state block only, and node 0's state block is pinned
   outright by the three initial-condition equality rows. Activating row 0
   therefore breaks LICQ. This was caught by the generator's own rank check,
   which reported rank 619 of 620 on the first draft — exactly one row short,
   exactly row 0. See the deviations section below.

## The acquired-active-set rule (stated, not implied)

An interior-point solution never puts a slack exactly at zero, so counting
"rows PIQP reports active" is a thresholding question. Both rules are computed
and reported on every row:

- **Rule A (primary; the CSV's `active_set_size_found`)** — row `j` is active
  iff `bi(j) - (Ai x)(j) <= 1e-8 * row_scale(j)`, using the generator's own
  per-row scale (carried in the dump) so activity is read in the same units the
  margins were drawn in.
- **Rule B (corroborating; `active_set_size_found_dual`)** — row `j` is active
  iff `z_u(j) >= 1e-6 * max(1, ||z_u||_inf)`.

`active_true_recovered`, `active_false_positive`, and `active_missed` compare
Rule A's set against the constructed one row by row, so the CSV records *which*
rows were acquired, not just how many.

## `factorizations` is a DERIVED counter

PIQP's public `Info` exposes no cumulative factorization count — `factor_retires`
is a retry counter reset to 0 after each successful factorization
(`solver.tpp:462`, `:705`). Its control flow is unambiguous: exactly one
`update_scalings_and_factor` site in initialization (`solver.tpp:442`) and
exactly one inside the main iteration loop (`solver.tpp:684`). So

    factorizations = 1 + info.iter

counting successful numeric factorizations and excluding regularization retries,
which the public API cannot report. `factor_retires_final` is carried beside it.
This is a derivation from the pinned v0.6.3 source, labelled as such — not a
counter PIQP reports.

## Anchors

The two anchor cells are the oracle's own equality-only cells at `nx = 1e5`,
re-run: `f7_n20000_bound_neutral` and `f7_n20000_path_neutral`. They are
rebuilt directly from `F7CollocationChain` through the same neutral-cold
first-QP path `corpus_cells.h::first_qp_for_cell` takes, which avoids needing a
full tycho_sqp/MKL build of the corpus runner. The reproduction was checked
against the committed oracle artifact and agrees: `iter = 9` on both,
`ext_stationarity` `2.0688919e-12` / `2.3507056e-12` matching to every printed
digit, `max_ci` `-0.275` / `-0.210632`, and `0/20000` rows active. Their
`active_set_size_true` is recorded as `0` — **measured, not constructed**, which
is why they are anchors and not taxonomy cells.

## Rows as measured

A transcription of the CSV's counter columns. No interpretation is offered here;
the verdict belongs to the settler.

| id | n | mi | active_fraction | margin_class | status | iters | factorizations | active_set_size_true | active_set_size_found | res_primal | res_dual | wall_info_s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| e1_f7_n4000_af01_m1e-2 | 20000 | 4000 | 0.01 | 0.01 | solved | 15 | 16 | 40 | 40 | 2.89e-15 | 6.70e-12 | 0.048 |
| e1_f7_n4000_af01_m1e-4 | 20000 | 4000 | 0.01 | 0.0001 | solved | 14 | 15 | 40 | 40 | 5.36e-14 | 2.35e-11 | 0.046 |
| e1_f7_n4000_af01_m1e-6 | 20000 | 4000 | 0.01 | 1e-06 | solved | 16 | 17 | 40 | 40 | 6.22e-15 | 4.10e-11 | 0.051 |
| e1_f7_n4000_af10_m1e-2 | 20000 | 4000 | 0.1 | 0.01 | solved | 12 | 13 | 400 | 400 | 1.53e-14 | 3.10e-11 | 0.040 |
| e1_f7_n4000_af10_m1e-4 | 20000 | 4000 | 0.1 | 0.0001 | solved | 11 | 12 | 400 | 400 | 1.60e-14 | 4.83e-12 | 0.037 |
| e1_f7_n4000_af10_m1e-6 | 20000 | 4000 | 0.1 | 1e-06 | solved | 14 | 15 | 400 | 400 | 6.22e-15 | 7.00e-11 | 0.046 |
| e1_f7_n4000_af30_m1e-2 | 20000 | 4000 | 0.3 | 0.01 | solved | 11 | 12 | 1200 | 1200 | 1.89e-15 | 2.32e-12 | 0.037 |
| e1_f7_n4000_af30_m1e-4 | 20000 | 4000 | 0.3 | 0.0001 | solved | 10 | 11 | 1200 | 1200 | 2.73e-14 | 2.17e-12 | 0.034 |
| e1_f7_n4000_af30_m1e-6 | 20000 | 4000 | 0.3 | 1e-06 | solved | 13 | 14 | 1200 | 1200 | 1.25e-14 | 6.54e-11 | 0.043 |
| e1_f7_n20000_af01_m1e-2 | 100000 | 20000 | 0.01 | 0.01 | solved | 18 | 19 | 200 | 199 | 1.91e-14 | 6.43e-12 | 0.299 |
| e1_f7_n20000_af01_m1e-4 | 100000 | 20000 | 0.01 | 0.0001 | solved | 15 | 16 | 200 | 200 | 1.08e-13 | 7.53e-11 | 0.250 |
| e1_f7_n20000_af01_m1e-6 | 100000 | 20000 | 0.01 | 1e-06 | solved | 17 | 18 | 200 | 200 | 4.66e-14 | 1.47e-11 | 0.280 |
| e1_f7_n20000_af10_m1e-2 | 100000 | 20000 | 0.1 | 0.01 | solved | 12 | 13 | 2000 | 2000 | 3.29e-14 | 1.97e-11 | 0.208 |
| e1_f7_n20000_af10_m1e-4 | 100000 | 20000 | 0.1 | 0.0001 | solved | 12 | 13 | 2000 | 2000 | 3.61e-14 | 2.60e-12 | 0.205 |
| e1_f7_n20000_af10_m1e-6 | 100000 | 20000 | 0.1 | 1e-06 | solved | 14 | 15 | 2000 | 2000 | 2.99e-14 | 3.15e-11 | 0.234 |
| e1_f7_n20000_af30_m1e-2 | 100000 | 20000 | 0.3 | 0.01 | solved | 11 | 12 | 6000 | 6000 | 4.49e-14 | 1.44e-11 | 0.197 |
| e1_f7_n20000_af30_m1e-4 | 100000 | 20000 | 0.3 | 0.0001 | solved | 9 | 10 | 6000 | 6000 | 3.52e-12 | 9.31e-11 | 0.164 |
| e1_f7_n20000_af30_m1e-6 | 100000 | 20000 | 0.3 | 1e-06 | solved | 13 | 14 | 6000 | 6000 | 3.00e-14 | 8.44e-12 | 0.224 |
| e1_anchor_f7_n20000_bound_neutral | 100000 | 20000 | 0 | n/a | solved | 9 | 10 | 0 | 0 | 1.06e-14 | 2.07e-12 | 0.160 |
| e1_anchor_f7_n20000_path_neutral | 100000 | 20000 | 0 | n/a | solved | 9 | 10 | 0 | 0 | 1.84e-14 | 2.35e-12 | 0.158 |

Full-precision residuals, the dual-rule active counts, the set-agreement
columns, `x_err_inf`, `min_box_slack`, `max_ci`, `max_abs_z_u`, and
`setup_info_s` are all in `e1_cells.csv`.

## Deviations from the protocol, and one construction caveat

1. **Row 0 is excluded from every constructed active set.** The protocol asks
   for active fractions of inequality rows; it does not ask for a
   constraint-qualification degeneracy. F7's row 0 is in the row space of the
   initial-condition equality rows, so activating it makes `[Ae; Ai_A]` rank
   deficient and the multipliers at `x*` non-unique — the cell would then be
   measuring LICQ failure, not acquisition. `|A|` is unchanged
   (`round(fraction * mi)`); row 0 simply joins the inactive rows and draws a
   margin like any other. Caught by the generator's own LICQ check, not
   assumed.
2. **Anchors are rebuilt from the model rather than dumped by the corpus
   runner.** `tycho_sqp_corpus --dump-qp` requires a full MKL build of the
   tycho_sqp library, which this out-of-tree workspace deliberately does not
   have. The neutral-cold branch of `first_qp_for_cell` is three lines
   (`set_parameters`, `start_point`, `build_subproblem` at zero multipliers) and
   is reproduced exactly; the agreement with the committed oracle row is the
   evidence that the reproduction is the same cell.
3. **The anchors are both at `nx = 1e5`, not one per size.** The protocol says
   "the 2 original oracle-class equality-only cells"; the oracle's node grid is
   `(1000, 2000, 5000, 10000, 20000)`, which contains no `N = 4000`
   (`nx = 2e4`) cell, so re-running *original* cells forces both anchors to
   `N = 20000`. The bound and path windows are the two chosen.
4. **`x_err_inf` is reported but is not a tight quantity on this family.**
   A residual near `1e-11` maps to an `x_err_inf` of order `1e-5` here. That is
   the conditioning constant of F7's KKT system, already documented in the
   archived driver's gate (a `1e-10` residual maps to `1.7e-7` at `N = 30`, and
   the ratio is approximately linear across three tolerances), not a defect in
   the cell or the solve. The counters and the KKT residuals are the asserted
   quantities; `x_err_inf` is context.
5. **One cell's Rule-A count is one short of its constructed set**
   (`e1_f7_n20000_af01_m1e-2`: 199 found, 200 true, 1 missed). The threshold was
   pre-declared and was **not** moved afterwards. The diagnostic the driver
   printed is in `sweep.log`: the missed row 557 has slack `1.524e-9` against a
   threshold of `1.142e-9` — a relative slack of `1.33e-8`, a factor 1.33 the
   wrong side of Rule A's `1e-8` — while its dual `z_u = 0.529` is
   unambiguously that of an active row, and Rule B counts it (dual count 200).
   Recorded as a thresholding-boundary artifact, with the numbers, rather than
   smoothed away.

**Construction-fidelity caveat, stated plainly.** These cells share F7's exact
sparsity and matrix values but their `g`/`be`/`bi` are manufactured, so `x*` is
a random-ish point rather than a trajectory the family would actually visit,
and the active set is a uniform random subset of rows rather than the
contiguous junction window F7's own geometry produces. That is the price of
knowing the active set exactly, and it is the design the protocol asked for
("constructed by KKT-inverse cell design ... so the true active set is known by
construction"). What it means for the reading: an active set scattered across
the collocation index is *not* obviously the same difficulty as one
concentrated in a junction window, and this artifact does not measure the
difference. A reader who wants the contiguous-window variant should ask for it
as a follow-on, not infer it from these rows.

---

## Follow-on variant: contiguous activity window (added AFTER the first sweep)

**Not part of the pre-registered taxonomy.** These 9 cells were generated and
run *after* the 20-cell sweep above had already completed, at the settler's
request, to address the first fidelity concern the implementer raised: the
taxonomy's active sets are a uniform random subset of inequality rows, while
F7's own geometry produces a **contiguous junction window**, and whether the two
layouts are the same difficulty for a barrier method was not measured.

They live in their own files and are **not merged into `e1_cells.csv`**:

| file | what it is |
|---|---|
| `variant_contiguous.csv` | the 9 variant rows — schema identical to `e1_cells.csv`, so the two are directly comparable |
| `variant_contiguous_layout.csv` | each variant cell's block offset, first/last active row, and block size |
| `VARIANT-CONTIGUOUS-VERIFICATION.log` | the variant's own three-tier KKT + LICQ verification, run before its sweep |
| `variant_contiguous_sweep.log` | the variant sweep's stdout |
| `generator/run_variant_contiguous.sh` | the script that produced all of it |

**Design.** Identical construction to a taxonomy cell in every respect but one:
the active set is a single contiguous block `[o, o+k)` of inequality rows
instead of a uniform random subset. `N = 20000` (`nx = 1e5`) only, crossed with
the same three active fractions and the same three margin classes. Row 0 stays
excluded for the same structural reason, so the block is drawn inside
`[1, mi-1]` with the offset `o` uniform on `[1, mi-k]`. Same budgets, same
120 s timeout, same slack/dual counting rules, same driver binary.

**The offset shift rule.** A contiguous block sits differently against the
collocation staircase than a scattered one, so LICQ is *re-tested at the drawn
offset* rather than inherited. If the LICQ certificate fails at `o`, the offset
shifts by +1 (wrapping within `[1, mi-k]`) and is re-tested, across every
admissible offset; if none admits LICQ the cell is recorded as
`infeasible_by_construction` and no row is fabricated for it. **The rule never
fired**: every drawn offset passed on the first try, and no cell was infeasible.
The offsets actually used are in `variant_contiguous_layout.csv`.

**LICQ verification, in three tiers**, because the exact test does not scale.
The exact test is a sparse QR of `[Ae; Ai_A]^T`; measured end-to-end on this
staircase it is instant at 660 rows, 11–16 s at 6020–6600 rows (`N = 2000`),
171 s at 16500 rows (`N = 5000`), and past a 300 s budget at 33000 rows
(`N = 10000`) — it cannot be paid at the swept size's 60200–66000 rows. So:

1. **Exact rank at `N = 200`, all 9 classes** — full row rank on every one.
2. **Exact rank at `N = 2000`, all three fractions** at one margin class. One
   margin covers that axis for *this* test: the margin sets the **inactive**
   rows' slacks only, and no inactive row enters `[Ae; Ai_A]`.
3. **Exact rank at `N = 5000`, 30% active (16500 rows)** — the largest exact
   check this experiment can afford, at the most LICQ-stressing fraction, a
   quarter of the swept size. Full row rank.
4. **A cheap `LDL^T` certificate of `J J^T` at the full `N = 20000`**, on every
   swept cell, inside the generator, where it also gates the offset shift rule.
   Its caveat is stated rather than buried: forming `J J^T` squares the
   condition number, so it is a numerical certificate, not an exact rank. What
   makes it credible here is that **wherever both tests ran they agreed on
   every cell**, and its margin at full size is real (`min|D|` ~ 1e-9 against
   `max|D|` = 2.0, over 66000 rows).

The full KKT check (stationarity, feasibility, exact-equality active rows,
strict complementarity, margin decile, box inactivity, `H` positive definite)
was re-run at each variant cell's own size as well; all pass.

### Variant rows as measured

Again a transcription, with no interpretation offered.

| id | n | mi | active_fraction | margin_class | block_first_row | block_last_row | status | iters | factorizations | active_set_size_true | active_set_size_found | res_primal | res_dual | wall_info_s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| e1blk_f7_n20000_af01_m1e-2 | 100000 | 20000 | 0.01 | 0.01 | 6571 | 6770 | solved | 12 | 13 | 200 | 200 | 3.55e-13 | 5.32e-11 | 0.205 |
| e1blk_f7_n20000_af01_m1e-4 | 100000 | 20000 | 0.01 | 0.0001 | 4393 | 4592 | solved | 15 | 16 | 200 | 200 | 1.64e-13 | 2.88e-11 | 0.249 |
| e1blk_f7_n20000_af01_m1e-6 | 100000 | 20000 | 0.01 | 1e-06 | 2196 | 2395 | solved | 17 | 18 | 200 | 200 | 1.23e-13 | 2.09e-11 | 0.279 |
| e1blk_f7_n20000_af10_m1e-2 | 100000 | 20000 | 0.1 | 0.01 | 7088 | 9087 | solved | 13 | 14 | 2000 | 2000 | 1.01e-13 | 1.89e-12 | 0.220 |
| e1blk_f7_n20000_af10_m1e-4 | 100000 | 20000 | 0.1 | 0.0001 | 8010 | 10009 | solved | 15 | 16 | 2000 | 2000 | 2.21e-13 | 5.13e-11 | 0.250 |
| e1blk_f7_n20000_af10_m1e-6 | 100000 | 20000 | 0.1 | 1e-06 | 12414 | 14413 | solved | 18 | 19 | 2000 | 2000 | 5.32e-14 | 1.98e-11 | 0.296 |
| e1blk_f7_n20000_af30_m1e-2 | 100000 | 20000 | 0.3 | 0.01 | 9176 | 15175 | solved | 12 | 13 | 6000 | 6000 | 4.04e-13 | 8.53e-11 | 0.211 |
| e1blk_f7_n20000_af30_m1e-4 | 100000 | 20000 | 0.3 | 0.0001 | 1974 | 7973 | solved | 15 | 16 | 6000 | 6000 | 1.14e-13 | 3.14e-11 | 0.253 |
| e1blk_f7_n20000_af30_m1e-6 | 100000 | 20000 | 0.3 | 1e-06 | 12102 | 18101 | solved | 17 | 18 | 6000 | 6000 | 2.05e-13 | 4.10e-11 | 0.280 |

Two independent full runs of the variant were compared: every non-timing
column, residuals at 17 digits included, was bit-identical.

### One housekeeping note on the pre-registered CSV

Adding the variant required touching the generator (a new layout mode) and the
dump format (two additive metadata tags). The original 20-cell sweep was
therefore **re-run under the final build** and its output compared against the
copy produced before those changes: every non-timing column, residuals at 17
digits included, was bit-identical. `e1_cells.csv` as it stands is that re-run;
no pre-registered number moved.

---

## The protocol, verbatim

Copied without alteration from
`hven/docs/notes/2026-08-26-e1-acquisition-experiment.md`
(sha256 in `PROVENANCE.txt`).

> # E1 — barrier active-set acquisition experiment (protocol)
>
> Date: 2026-08-26. Status: protocol DRAFT; pre-registered before any
> cell runs. Purpose: close the load-bearing caveat in the PIQP oracle
> verdict (tycho_sqp docs/notes/2026-08-06-piqp-oracle.md §5(c)/§8) —
> every cell measured there had an effectively EQUALITY-constrained
> first QP, so IPM linear-algebra cost is certified but barrier
> ACTIVE-SET ACQUISITION at scale is not. This experiment measures
> exactly that gap. Its artifact feeds the owner's build-vs-re-register
> decision on tycho_sqp P7 G1/G2 (the M6 brief's E1 gate).
>
> ## Question (pre-registered)
>
> Does an interior-point QP solve acquire a NONTRIVIAL active set at
> nx ≈ 1e5 on F7-pattern cells within the ruled gate — < 40 IPM
> iterations and ≲ 30 s per solve — and does the iteration count stay
> bounded as the active fraction and near-activity tightness grow?
>
> ## GO / NO-GO criteria (pre-registered, before any cell runs)
>
> - GO: every cell class converges with iterations < 40 (counter,
>   asserted) and no monotone iteration blow-up across the active-
>   fraction sweep; failures confined to cells the taxonomy marks
>   pathological.
> - NO-GO: systematic failure or iteration blow-up on the near-active
>   classes at nx = 1e5.
> - Anything between (isolated failures, borderline growth) is
>   reported as measured — the owner's decision weighs it; the
>   criteria bound the automatic verdicts only.
>
> ## Instrument
>
> - PIQP as the numerical ORACLE, un-vendored (LGPL-tainted sparse
>   backend — oracle only, never vendored; CLAUDE.md §6 math-only
>   discipline). The bridge is copied from the archived tycho_sqp
>   prototypes/piqp_bridge/ (read-only source) into a scratch
>   workspace OUTSIDE the hven tree; it remains disposable and does
>   not migrate.
> - Counters asserted: PIQP iteration count, factorization count,
>   termination status, and the solution's active-set size. Residuals
>   reported. Wall informational (single-threaded, stated); no wall
>   claim is made, so cells may run without whole-box serialization —
>   but never concurrently with a tycho-d7 wall-asserting gate (§7
>   courtesy, coordinated by the settler).
> - Per-cell budget: 120 s hard timeout, one retry on setup failure;
>   a timeout is a recorded outcome, never an unbounded cell (§8).
>
> ## Cell taxonomy
>
> F7-pattern QP structure (the pattern the oracle already certified
> for linear algebra), sizes nx ∈ {2e4, 1e5}, crossed with:
>
> 1. **Active fraction at the solution**: ~1%, ~10%, ~30% of
>   inequality rows active — constructed by KKT-inverse cell design
>   (choose x*, active set, and consistent multipliers; derive the
>   linear term), so the true active set is known by construction and
>   "acquired" is checkable exactly.
> 2. **Near-activity margin**: the inactive rows' slacks drawn so the
>   nearest decile sits at margins {1e-2, 1e-4, 1e-6} of row scale —
>   the tie-pressure axis the oracle's cells lacked (nearest row was
>   ≥ 0.21 from activity).
> 3. **Start**: neutral cold start only (the acquisition question);
>   warm-start repair is W3's business, not E1's.
>
> 3 fractions × 3 margins × 2 sizes = 18 cells, plus the 2 original
> oracle-class equality-only cells re-run as anchors = 20 cells.
> One process per cell, sequential.
>
> ## Artifact
>
> docs/notes/data/2026-08-m6-e1-acquisition/ — per-cell CSV (id,
> n, mi, active_fraction, margin_class, status, iters, factorizations,
> active_set_size_true, active_set_size_found, residuals, wall_info),
> provenance stamp (toolchain, hardware, date, oracle version, bridge
> commit of origin), the generator script, and a README carrying this
> protocol verbatim plus the declared budgets. The verdict section is
> written LAST, against the pre-registered criteria above.
>
> ## What this experiment is NOT
>
> Not a p95 (single run per cell; the decision question is feasibility
> of acquisition, not tail latency); not a warm-start measurement; not
> an IP-PMM measurement (PIQP is a proxy for barrier-method
> acquisition behavior — the verdict transfers as evidence about the
> method class, and the brief says so wherever it is cited).

---

## VERDICT (settler, 2026-08-26 — written against the pre-registered criteria, after both sweeps)

**GO.** Both automatic criteria are met, with margin:

- Every cell of the pre-registered 20-cell taxonomy converged, 9–18
  IPM iterations against the < 40 gate (worst cell 18 = 2.2× margin);
  zero timeouts, zero failures, no pathological class.
- No iteration blow-up across the active-fraction sweep — iterations
  are FLAT to slightly falling as the active fraction rises 1% → 30%
  at both sizes. The only mild upward axis is margin tightness
  (1e-2 → 1e-6 adds ~2–5 iterations), bounded well inside the gate.
- The post-hoc contiguous-window variant (9 cells, added at the
  settler's request to address the scattered-activity fidelity
  concern) reproduces the result: 12–18 iterations, exact active-set
  recovery on every cell. Layout is not the differentiator.

The oracle's load-bearing caveat is CLOSED: barrier active-set
acquisition at nx = 1e5 on F7-pattern cells with known-by-
construction active sets, under tie pressure to 1e-6 margins, is
demonstrated — not just the linear-algebra cost the 2026-08-06
verdict certified.

Residual risks, carried to the W1 spec rather than blocking the GO:
(1) block LOCATION and the two-junction structure of a real
bound-arc hop are not exercised (the variant fixes layout, not
placement); (2) x* is manufactured — conditioning of a real hop's
QP may differ; (3) no cell has simultaneous bound activity;
(4) PIQP is a proxy for the method class, not a measurement of the
in-house IP-PMM. None of these is an acquisition question of the
kind the criteria pre-registered; all four become W1 acceptance
cells.

Decision this feeds: the owner's build-vs-re-register ruling on
tycho_sqp P7 G1/G2. The evidence supports BUILD.
