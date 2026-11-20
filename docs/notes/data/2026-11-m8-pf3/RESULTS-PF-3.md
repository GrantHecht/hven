# RESULTS-PF-3 -- suite-wide instruction profile with per-component attribution, and the routes census

Pin hven `48e1eeae2ced`; harness hven-bench `e03cbad` (detached worktree); lane binary sha256 `961bcb4a...3853e`; clang 22.1.8; AMD Ryzen 7 5800X3D.
Date 2026-11-19/20. Protocol and every amendment: `DECLARATION-PF-3.md` (+ addenda 1, 2). Instrument: `perf stat` + `perf record -e instructions:u`
with DWARF user callchains, innermost-owner attribution through `tools/components.py`. Co-run: 6 wide, one process per pinned physical core
(cpus 0-5), `MKL_NUM_THREADS=1`, inside one `flock` acquisition per leg. **Wall-clock is not measured here**; the solo wall leg is pending (`SOLO-LEG.md`).

## 0. What was measured

- 284 cells: HS TRAIN (22 problems), CUTEst `cutest-nlp-35` TRAIN (23), CUTEst `cutest-small-dense` TRAIN (97), both lanes, default config.
  272 `full` cells run to their own end (instruction totals asserted); 12 deadline-class cells (dnf at the 60 s ceiling in the baseline: ipm
  MSS2/MSS3/READING8/TAX213322; sqp ACOPR118/BRITGAS/MSS2/MSS3/NET3/READING8/TAX213322/TAX53322) are sampled for 30 s and give shares only.
- Pass-B sample estimate within 1 % of the pass-A total on every cell with >= 200 samples; pass-B status/iterations/factorizations equal pass A on all cells.
- Cross-run determinism (`out/determinism.txt`): five complete runs of the 75-cell nlp35+HS population: status, iterations, factorizations identical in
  every run; instruction totals agree to <= 0.03 % on every cell above 1e9 instructions (max spread 1.2 % on one 8.7M-instruction HS cell).
- All 134 full hven-sqp rows carry zero escape counters (section 5).
- Corpus facts: 3.674e12 instructions over the 272 full cells (hven-sqp 3.162e12, hven-ipm 0.512e12). Ten cells are 83 % of it (hven-ipm TAX53322 12.5 %, then sqp
  SPANHYD 11.8 %, CRESC100 10.3 %, STEENBRC 9.3 %, STEENBRG 8.8 %, STEENBRE 7.0 %, STEENBRD 6.6 %, ACOPP118 5.6 %, YORKNET 5.5 %, BIGBANK 5.3 %). **Corpus shares below are
  therefore the profile of those cells; the per-set rows and `out/attrib-percell.csv` carry the other populations.**

## 1. Attribution table (solver-only: process loader/problem-load excluded; corpus shares of instructions)

Truncated BLAS-kernel chains (7.3 % of all samples; no unwind through MKL assembly) are redistributed per cell by BLAS family (declaration, addendum 1); 0.1 % stays unresolved.
Owner categories map as: factorization = the 'FACTOR' columns; QP logic = 'QP'; evaluation = 'EVAL'; setup = 'SETUP'; instrumentation = 'INSTR'.

| group | cells | total instr | FACTOR sparse factorize | FACTOR sparse solve | FACTOR sparse analyze | FACTOR dense factorize | FACTOR dense solve | FACTOR other/scaling | QP schur/border | QP walk+dep+refine+other | EVAL callbacks | EVAL hven assembly | KKT assembly+glue | OUTER loop | SETUP library init | SETUP init/teardown | INSTR | unattrib. |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| SUITE (full cells) | 272 | 3.67e+12 | 13.2% | 61.3% | 10.9% | 0.8% | 1.0% | 0.7% | 3.3% | 3.6% | 2.5% | 0.3% | 1.4% | 0.5% | 0.4% | 0.0% | 0.0% | 0.1% |
| ipm (full) | 138 | 5.1e+11 | 59.7% | 18.3% | 0.2% | 0.2% | 0.0% | 0.0% | 0.0% | 0.0% | 17.6% | 2.0% | 0.0% | 1.7% | 0.1% | 0.0% | 0.0% | 0.1% |
| ipm / hs | 22 | 4.75e+07 | 0.0% | 0.0% | 0.0% | 1.1% | 2.1% | 0.0% | 0.0% | 0.0% | 2.1% | 2.1% | 1.1% | 5.3% | 82.1% | 0.0% | 0.0% | 4.2% |
| ipm / nlp35 | 19 | 5.06e+11 | 60.0% | 18.4% | 0.2% | 0.0% | 0.0% | 0.0% | 0.0% | 0.0% | 17.5% | 2.0% | 0.0% | 1.7% | 0.1% | 0.0% | 0.0% | 0.1% |
| ipm / sd | 97 | 3.57e+09 | 20.9% | 5.3% | 0.4% | 28.5% | 2.2% | 1.2% | 0.0% | 0.0% | 29.1% | 2.5% | 0.0% | 3.4% | 5.0% | 0.0% | 0.0% | 1.3% |
| ipm / deadline-class (shares only) | 4 | 1.05e+12 | 89.8% | 2.4% | 2.2% | 0.0% | 0.0% | 0.0% | 0.0% | 0.0% | 2.7% | 0.8% | 0.0% | 1.4% | 0.5% | 0.0% | 0.0% | 0.1% |
| sqp (full) | 134 | 3.16e+12 | 5.7% | 68.2% | 12.7% | 0.8% | 1.1% | 0.9% | 3.8% | 4.2% | 0.0% | 0.0% | 1.6% | 0.4% | 0.4% | 0.0% | 0.0% | 0.1% |
| sqp / hs | 22 | 7.55e+07 | 0.0% | 0.0% | 0.0% | 3.3% | 15.2% | 0.0% | 6.6% | 21.9% | 0.0% | 0.7% | 3.3% | 1.3% | 40.4% | 1.3% | 0.0% | 6.0% |
| sqp / nlp35 | 15 | 2.02e+12 | 7.8% | 62.7% | 17.4% | 0.2% | 0.1% | 1.0% | 3.0% | 5.0% | 0.0% | 0.0% | 2.1% | 0.2% | 0.3% | 0.0% | 0.0% | 0.1% |
| sqp / sd | 97 | 1.14e+12 | 2.1% | 78.0% | 4.4% | 1.9% | 3.0% | 0.5% | 5.1% | 2.8% | 0.1% | 0.0% | 0.8% | 0.6% | 0.5% | 0.0% | 0.0% | 0.1% |
| sqp / deadline-class (shares only) | 8 | 2.05e+12 | 20.8% | 50.6% | 12.2% | 0.0% | 0.1% | 0.5% | 1.0% | 11.5% | 0.3% | 0.1% | 1.1% | 0.9% | 0.3% | 0.0% | 0.0% | 0.3% |

Reading (corpus rows; each is a statement about the cells that dominate the corpus):

- **Factorization backend (MKL Pardiso + dense) is 87.9 % of all solver instructions** (sqp 89.4 %, ipm 78.5 %). Inside it the *solves* dominate the SQP engine
  (sparse solve 68.2 % of sqp instructions) and the numeric *factorization* dominates the IPM engine (59.7 % of ipm). Dense arm (dense factorize + solve): 1.7 % of the suite, 18.5 % of the sqp/HS profile.
- **QP subproblem logic (excluding the backend) is 8.0 % of sqp** (Schur/border bookkeeping 3.8 %, walk + dependency + refine/polish 4.2 %); **8-29 % of the small dense cells** (sqp/small-dense 7.9 %, sqp/HS 28.5 %), where the backend is cheap.
- **Function/derivative evaluation:** 17.6 % of ipm CUTEst (callbacks inside `libcutest`/`libpb`: Hessian assembly 5.5 %, Jacobian 4.3 %, gradients 2.3 % of ipm); 29 % of ipm small-dense; hven's own
  assembly layer is 2.0 % of ipm. On sqp evaluation is 0.0-0.1 %: the SQP engine evaluates once per major iteration (6 to 30 evaluations on the cells profiled in 2.1).
- **Setup:** one-time MKL/OpenMP bring-up (`setup.library_init`) is 0.35 % of the suite but 40 % (sqp) and 82 % (ipm) of the solver-only profile of the HS cells; the process loader of the 116 MB statically linked harness binary adds
  ~4M instructions to every cell (harness artifact, reported apart as `proc.*`, not counted above).
- **Instrumentation and trace:** 0.0 % by symbol. Counter increments are inline in their owning frames; `CountingNlp` and the ledger/trace sinks do not appear above 0.05 %. This is a limit of the symbol method (inline increments are not separable), not a measured zero cost.
- **Overlays (non-additive, `out/attrib.md`):** allocator leaves 1.7 % (sqp 1.9 %, sqp/HS 6.7 %); OpenMP-runtime leaves (libiomp5) 3.0 % (ipm 3.7 %, sqp 2.9 %).

The full tables (suite, per engine, per set, solver-only and all-in; median per-cell shares; dominance lists; top leaf symbols; backend drivers) are in `out/attrib.md`; per-cell numbers in `out/attrib-percell.csv`; per-cell JSON in `out/cells`, `out/cells-sd`.

## 2. The four known inputs

**2.1 Schur-border solves per factorization (CONFIRMED, wider than ~35).** callgrind call counts (`out/callgrind2`, exact): solves per factorization hven-sqp DALLASM **41.5** (18 725 solves / 451 factorizations),
hs033 (dense) **33.3**, SMBANK **105.7**, hs071 11.8, ACOPP57 9.5; hven-ipm ACOPP57 0.7. Per QP minor: DALLASM 24.7 solves, SMBANK 10.4, ACOPP57 11.2. Every `SchurComplement::add_border` is called from `QpEngine::sync_borders` (100 % of the calls in all five SQP cells). By sampling,
`add_border` is **43.2 % of all hven-sqp instructions** inclusive (57.6 % of the sparse-solve component; dominant on SPANHYD 86 %, MESH 70 %, DNIEPER 69 %, CRESC50 66 %, READING6 66 %, CRESC100 61 %).
The batch size (add_border calls per sync_borders call) is not yet read: open item 7. Measured average add_border calls per QP: DALLASM 154 (7 529 / 49), SMBANK 5.5, ACOPP57 38.

**2.2 Per-minor cost on minor-heavy cells.** `out/extra.md` (cells with >= 100 minors). On the sparse minor-heavy cells one minor costs 1.0e7 to 2.4e7 instructions (STEENBRB 1.0e7, MSS1 1.0e7, STEENBRD 1.1e7, STEENBRC 1.2e7, STEENBRG 1.4e7, YORKNET 1.5e7, CRESC100 1.6e7, ACOPP118 2.0e7, SPANHYD 2.4e7; BIGBANK 6.1e7 at 3 196 minors, large n).
Factorizations run at 1.0-1.4 per minor on these (SPANHYD 2.0). A minor on STEENBRC (1.2e7 instructions) splits: sparse solves 57 % (6.9M), analyses 17 % (2.1M), numeric factorization 7.9 % (1.0M), QP logic 9.2 % (1.1M, of which Schur border 2.9 %), KKT assembly and glue the rest. Backend analysis (METIS) is 15-24 % of the cost of a minor on STEENBR*, YORKNET, ACOPP118, BIGBANK, MSS1.
Dense-arm minors cost 1e5-6e5 instructions (HS/small-dense sqp) with QP logic 30-40 % of them.

**2.3 Fixed overhead on small problems after the dense arm (`out/extra.md`).** Median per-cell instructions: HS ipm 6.8M, HS sqp 8.5M, small-dense ipm 8.0M, small-dense sqp 12M. Of the HS ipm cell: loader 4.0M (harness binary), MKL/OpenMP bring-up 2.0M, net solver
**0.79M** (11.6 %); HS sqp net solver 1.5M (19 %); small-dense sqp 5.5M (45 %). So for a one-solve process on a small problem, 80 % of the instructions are not the solve: the process loader (harness-side) and `mkl_get_max_threads()`'s threading-layer bring-up (`ensure_solver_initialized`, once per process). Within the net solver
cost of the HS sqp cells, the dense arm (solve 15 %, factorize 3 %) and QP logic (22 %) are the largest parts; library-call overhead (LAPACKE wrappers, allocator leaves 6.7 %) is visible.

**2.4 The three factorization pathologies (SPANHYD, CRESC100, MSS1; small-dense TRAIN; reported, not fixed).** They are all **hven-sqp** and all the same shape: **1.1 to 2.0 factorizations per QP minor and 9 500-23 800 minors per solve** (40 majors or fewer).
- SPANHYD: `numerical_error`, 32 majors, 18 208 minors, **36 497 factorizations**, 19 312 dense, 32 symbolic analyses, 2 932 wrong-inertia verdicts; 4.35e11 instructions (11.8 % of the suite); sparse solve 82.9 %, Schur border 3.3 %; add_border inclusive 86 %.
- CRESC100: `optimal`, 40 majors, 23 791 minors, **26 023 factorizations**, 6 600 dense, 5 704 symbolic analyses, 1 703 wrong-inertia verdicts; 3.78e11 (10.3 %); sparse solve 74.1 %, Schur border 7.5 %, analyze 5.4 %.
- MSS1: `numerical_error`, 11 majors, 9 525 minors, **10 119 factorizations**, 3 400 wrong-inertia verdicts; 9.9e10 (2.7 %); sparse solve 59.7 %, analyze 19.8 %.
- On the ipm lane the same three cells cost 4.0e8, 3.9e8 and 8.6e7 instructions (SPANHYD optimal in 110 iterations; MSS1 `numerical_error` at iteration 2). Together the three hven-sqp cells are **24.8 % of the suite's instructions** and 28.8 % of hven-sqp's.

## 3. Ranked behaviour-neutral opportunities

Instruction shares are of the stated population's solver instructions; "dominates" lists cells where the item is >= 50 % of the cell. *Neutral* means counters and statuses identical by construction; *numerics-changing* means they can move.
Savings are not estimated beyond what the data give: no candidate has been built or run (measurement-only task).

| # | opportunity | instruction share | dominates in | fix idea | neutrality |
|---|---|---|---|---|---|
| 1 | **Batch the Schur border stack's solves** | add_border inclusive 43.2 % of sqp (37 % of suite); 57.6 % of the sparse-solve component = 39 % of sqp | SPANHYD 86 %, MESH 70 %, DNIEPER 69 %, CRESC50 66 %, READING6 66 %, CRESC100 61 %; > 40 % on all sparse minor-heavy sqp cells | `sync_borders` adds borders one `add_border` (one K0 solve each) at a time; add all pending borders of a sync as one `SymmetricFactor::solve` with k right-hand sides (BLAS-3 kernels in the supernodal solve, one refinement setup) | **Intended neutral**: the border algebra is identical. Not guaranteed bit-identical: Pardiso's multi-RHS solve may round differently from k single solves, so counters/statuses must be proved by the 44-cell replay and the near-ulp gate before it is called neutral |
| 2 | **Pardiso's own iterative-refinement machinery inside every solve** | `mkl_spblas dcsr1nsunf mvout_par` 18.4 % + `find_norm` 4.3 % of la.sparse_solve = 15.5 % of sqp (13.9 % of suite) | all sparse sqp cells at 10-20 % | hven already refines (`qp.refine_polish`, `border_refine_steps`) and runs `solve_partial` with refinement forced to 0; drop (iparm[7] cap) the Pardiso refinement for solves hven re-verifies | **Numerics-changing** (refined solutions move), IPARM-SURFACE change: needs the label, validation evidence and the tycho-lane review (CLAUDE.md section 6) |
| 3 | **Reuse the symbolic analysis for a repeated KKT pattern** | la.sparse_analyze 12.7 % of sqp (17.4 % of nlp35-sqp; 24 % BIGBANK, 19-20 % MSS1/YORKNET/ACOPP118, 15-17 % STEENBR*); 95 % driven from `QpEngine::run` | BIGBANK (24 %), MSS1, YORKNET, ACOPP118, STEENBR* (15-19 %); callgrind: 0.50 analyses per factorization on ACOPP57, 0.66 on DALLASM | cache the `AnalysisDecision` (METIS permutation) by the existing pattern hash within a QP and across the QPs of a solve | **Neutral by construction if keyed on the identical pattern** (same pattern, same deterministic ordering). The repeat rate is NOT measured here (`side.symbolic_analyses` reports 1-3 where callgrind counts 50-296 backend analyses on ACOPP57/DALLASM: the public counter does not see them); measure the repeat rate first |
| 4 | **Sequential MKL threading layer** (drop the OpenMP runtime) | libiomp5 leaves 3.0 % of suite (ipm 3.7 %, mostly `__kmpc_flush` in factorization 3.6 % of ipm; sqp 2.9 %) + one-time bring-up 1.5-2.0M instructions per process (40 % / 82 % of the solver-only profile of the HS cells) | HS and small-dense cells (bring-up), ipm factorization cells | link or select `mkl_sequential` (or `mkl_set_threading_layer`) when the configured thread count is 1; the engines are single-thread by default | **Expected neutral at one thread** (same kernels), to be shown by replay; it removes multi-threaded MKL, and it touches MKL integration (CLAUDE.md section 6: flag for manual review) |
| 5 | **Dense border bookkeeping and workspace churn** | `qp.schur_border` 3.8 % of sqp (5.1 % of sqp/small-dense, 6.6 % of sqp/HS); allocator leaves 1.9 % of sqp; `SchurComplement::append_border` leaf 2.7 %, `memset` leaf 0.56 % (the `EIGEN_INITIALIZE_MATRICES_BY_ZERO` build define zero-fills every temporary) | the dense-arm cells (QP logic 30-40 %) | reserve capacity and grow the border matrices in place; keep the factor-C workspace across borders; avoid zero-initialising temporaries that are immediately overwritten | **Neutral** (storage only; no arithmetic change) |
| 6 | **Evaluation callbacks (harness/CUTEst side)** | 17.6 % of ipm (29 % ipm small-dense): `cutest_assemble_hessian` 5.5 %, `ccfsg` 4.3 %, `form_gradients` 2.3 % of ipm | hven-ipm ACOPP118 57 %, ACOPP57 53 %, ACOPR118 51 %, DECONVC 65 %, HS88-HS91 88-93 % | not hven code: the CUTEst bridge re-evaluates the full Hessian/Jacobian through the Fortran interface every iteration; cache the bridge's pattern work or evaluate by element; hven's own assembly layer is only 2.0 % of ipm | **Neutral** (identical values), but outside the library: informational |
| 7 | **Dense-arm call overhead on tiny cells** | `la.dense_solve` 15 % and `la.dense_factorize` 3 % of the sqp/HS solver-only profile (1.8 % of the suite) | HS sqp cells | call `dsytrs`/`dsytrf` directly on the column-major factor rather than through the LAPACKE work wrappers; fold the repeated small solves of the border path into one call (item 1 applies to the dense arm too: 11.2 % of dense-solve instructions sit under `add_border`) | **Neutral** for the wrapper change; bitwise only if the kernel is the same |

Not an opportunity (stated so they are not re-asked): the IPM's numeric factorization (59.7 % of ipm) is Pardiso doing real flops (`blkl_ll`, `upd_real_block`, `dgemm`/`dger` kernels); reducing it means fewer factorizations, which is behaviour. The three SQP pathologies of section 2.4 are behaviour (QP minor count) and are PF-1's subject, not an instruction fix.

## 4. Counter and method observations

- `side.symbolic_analyses` undercounts backend analyses (DALLASM: counter 3, callgrind 296 `FactorSession::analyze` calls; ACOPP57: 1 vs 50). A cost the public counters cannot see.
- Instruction totals are scheduling-invariant to < 0.03 % on every cell above 1e9 instructions across five runs; the 8-17 MI HS cells vary 0.2-1.2 % run to run (loader/allocator address effects), so no HS-scale claim below 1 % is made.
- The first classifier matched demangled signatures including parameter types (mis-charging `ratio_test(..., QpCounters&)` to instrumentation); it was corrected and both legs re-taken (declaration addendum 2; superseded tables kept in `out/superseded-signature-match`).
- Unwinding stops inside MKL assembly BLAS kernels; 7.3 % of samples needed the declared family redistribution. The raw (unredistributed) numbers are in each cell JSON.
- Deadline-class cells (12) give shares only; their sqp profile: sparse solve 50.6 %, factorization 20.8 %, analysis 12.2 %, QP logic 11.5 %; ipm: factorization 89.8 %.

## 5. The routes census and its disposition (brief PF-3, pre-registered rule)

- **Census taken once** with the existing binary (`hven_sqp_ssn_safeguard_probe routes`, default 20 000 QPs, default seed, built from this tree at the pin, sha256 `1861ed37...`), `out/routes-census.txt`:
  `infeasible QPs=20000  diagnosed=11375  STANDING route=1410 (12.4%)  EXHAUSTION route=9965  not diagnosed=8625`.
  The population is **infeasible by construction**, so every QP is an escape candidate; 11 375 end at `kInfeasibleSuspect`, 8 625 take another exit.
- **Escape counters over every measured row at the pin:** 0. All 134 full hven-sqp cells (HS 22, nlp35 15, small-dense 97): `ssn_escapes`, `ipqp_escapes`, `elastic_from_ipqp_escape` and every `*_escape_*` reason are zero; the committed pin-48e1 HS TRAIN rows (22) are zero.
  The 8 deadline-class hven-sqp cells report no counters when cut by the clock; bounded runs (`--budget-iters 5`, 120 s ceiling, `out/escape-bounded`): ACOPR118, BRITGAS and NET3 return with all escape counters 0; MSS2, MSS3, READING8 did not return inside 120 s; TAX213322, TAX53322 were not run. So 5 of the 8 deadline-class cells have **no escape observation**; the 44-cell close census had the same gap (-1 sentinel).
- **Disposition.** The literal pre-registered condition ("any census, TRAIN or suite-v1 row shows escapes > 0") is met only by the routes census, whose population is constructed to escape; it is not an observation of escapes in solves. On every row where escape counters exist (134 + 22 + 3 bounded) the count is zero. My reading: **the per-escape factorization attribution column has nothing to attribute on the solver populations (every share is 0 of 0), and the escape-path target is retired on that evidence, with the next observed escape reopening it** -- but because the literal clause fires on the synthetic census, this is a settler's call and I did not decide it silently.
  To make either reading cheap, the same instrument was run on the routes population (`out/routes-attrib.json`; the probe's 20 000 constructed QPs, 2.18e10 instructions): **the factorization backend is 80 % of the escape population's instructions** (sparse solve 40.6 %, numeric factorization 20.8 %, analysis 14.0 %, other 4.5 %), SSN logic 12.5 %. That is an upper bound on any per-escape factorization column (here every QP escapes, so escape and non-escape steps cannot be separated); it is not a measurement of escapes on the suite.

## 6. Open items

7. The border batch size per `sync_borders` call (the number of right-hand sides a batched solve would carry) was not read: the callgrind re-run that counts `sync_borders` waited more than 45 minutes for the box lock behind other agents' gates and was cancelled (my own queued process, no foreign process touched). `tools/pf3_callgrind.py` already counts it; re-running it on ACOPP57, SMBANK and DALLASM (about 80 s under callgrind) gives add_border / sync_borders per cell.

1. **Solo wall leg is pending** (`SOLO-LEG.md`: exact command; box must be quiet). Nothing here is a timing.
2. Opportunity 3's repeat rate (how often `FactorSession::analyze` sees an already-analysed pattern) needs a probe or counter; not available at this pin.
3. Opportunity 1's saving is unmeasured: a microbenchmark of nrhs = k against k single solves on Pardiso at the observed batch sizes should precede any build.
4. 5 of 8 deadline-class hven-sqp cells have no escape observation; the escape disposition (section 5) is for the settler.
5. `instr.*` is 0.0 % by symbol; a real instrumentation cost needs a with/without-ledger instruction diff (a build-flag experiment, not done here).
6. The pass-A totals of the superseded and repeated runs (`out/run1-cells`, `run2-cells`, `run3-cells`, `superseded-signature-match`) are retained as the determinism evidence; their attribution columns are not valid (sample loss or the signature bug).
