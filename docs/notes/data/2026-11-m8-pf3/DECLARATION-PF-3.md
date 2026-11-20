# DECLARATION-PF-3 -- suite-wide performance profile with per-component attribution, and the routes census

Committed before the first leg. Measurement only: no library code changes. HOLDOUT is never run.

## 1. Pin and binaries

- hven pin: `48e1eeae2ced0c4d47c5d1fa154973433bff1da3` (branch m8; this worktree, branch pf3, is at that commit).
- Harness: hven-bench main `e03cbad456d0c93fa543f5575bead45977101277`, checked out as a detached worktree
  (`/home/ghecht/Projects/hven-bench-pf3`), pin built by `scripts/build_hven_pin.sh` into `.hven/install-48e1eeae2ced`
  (Release, clang 22.1.8, LTO off, `libhven.a` sha256 `58662787b3f872d283fbee7fb2e595937d02c23f2a70c64bb2b3fd4acdb56776`).
- Lane binary `build/lanes/hven/hven_bench_cell` sha256 `961bcb4a0fec6cd1b958645c2a5607d264e5a9e33af798a8efd2c8520493853e`,
  byte-identical to the binary recorded by the harness's own pin-48e1 equivalence run (`runs/2026-11-m8-pin-48e1/binary.sha256`).
- MKL is statically linked into the lane binary (symbols visible to perf); CUTEst problems load as `libpb.so` +
  `libcutest_double.so`. Toolchain: clang 22.1.8; hardware AMD Ryzen 7 5800X3D (8 physical cores, SMT 2); Linux 7.2.12.
- Date 2026-11-19.

## 2. Cells

`cells.csv` (sha256 `85fde8b5ca934559...` prefix recorded in the output headers): the TRAIN rosters of both sets, both hven lanes,
options label `default`, `--budget-iters 0` (the engine's own cap):

- HS TRAIN: 22 problems x {hven-ipm, hven-sqp} = 44 cells.
- CUTEst TRAIN (`cutest-nlp-35 v1`): 23 problems x 2 lanes = 46 cells.

Total 90 cells. The argv is the serial runner's (`--lane --problem --options default --budget-iters 0 --options-dump --assert-reference`).
The roster is the harness's TRAIN split (`schema/split.py`); HOLDOUT is not named anywhere.

**Two cell modes**, assigned from the frozen baseline (`runs/2026-11-m8-corun-cutest/nlp35/serial-A`, 60 s ceiling) before any leg:

- `full` (78 cells): the cell runs to its own end. Its instruction total is deterministic and ASSERTED.
- `trunc30` (12 cells, deadline-class: they end `dnf_wall` at the 60 s ceiling in the baseline): hven-ipm {MSS2, MSS3, READING8,
  TAX213322}, hven-sqp {ACOPR118, BRITGAS, MSS2, MSS3, NET3, READING8, TAX213322, TAX53322}. One perf pass cut at 30 s of wall by
  SIGINT. Their component SHARES are reported as informational; their instruction totals are deadline-dependent (CLAUDE.md section 7)
  and are never asserted, never summed into a "corpus total" without being labelled, and a separate table carries them. (An
  iteration-budget truncation was tried and rejected: `--budget-iters 10` changes behaviour on these cells -- BRITGAS hven-sqp
  ends `numerical_error` at iteration 1 -- so it does not sample the default-config run.)

## 3. Instruments

- **Instruction attribution (the asserted leg).** Per `full` cell: pass A `perf stat -e instructions:u` (exact total); pass B
  `perf record -e instructions:u -c P --user-callchains --call-graph dwarf,16384`, `P = max(2000, total / 12000)`, i.e. about 12 000
  samples per cell. `perf_event_paranoid = 2` on this box (environmental, not worked around): user-space events only (`:u`),
  user-only callchains. Pass-B sample estimate over the pass-A total is reported per cell (`est_over_total`) and must be within 1 %.
  The pass-B counters (status, iterations, factorizations) must equal pass A's, else the cell is flagged.
- **Counters** come from the cell's own stdout (`side.*` fields, the harness's `dump_counters`), pass A.
- **Symbol -> component attribution** by the table in section 4, rule "innermost owner".
- **Overlays** (non-additive, report-only): `alloc_leaf` (leaf in malloc/free/new/delete and MKL's allocator), `iomp_leaf`
  (leaf in libiomp5), `fortran_leaf`.
- **Exact call counts** (solves per factorization): `valgrind --tool=callgrind` on a declared subset (section 6), because sampling
  cannot count calls.
- **Escape census**: `hven_sqp_ssn_safeguard_probe routes` (section 7) and the escape counters of every row (section 7).
- **Wall leg (NOT RUN in this task).** Sampled wall time (`perf record -e cpu-clock -F 499`, same callchains and same table) is a
  separate leg, `tools/pf3_corun.py ... width 1` over `tools/cells_solo.csv`, to be scheduled by the settler when the box is quiet:
  serial, solo, nothing else running. The exact command is in `SOLO-LEG.md` next to this file. Nothing in this task's tables is a
  quotable timing; wall values captured as a by-product are labelled informational.

## 4. Symbol -> component table (`tools/components.py`, sha256 stamped in every output JSON)

**Rule of attribution (innermost owner).** Each sample's callchain is walked from the leaf outward; the first frame matched by any
rule decides the component (rules are tried in table order within one frame). Frames matching no rule are GENERIC (libc, libstdc++,
libm, libgfortran, out-of-line Eigen instantiations, MKL BLAS/LAPACK/spblas, libiomp5, `FactorSession::run_phase`) and the cost is
charged to the nearest owner above them. Two **chain-level overrides** (O1, O2) fire first: if ANY frame matches, the whole sample is
harness work whatever library it runs. A chain matching nothing is `unattributed`; one with at most 3 frames is
`unattributed.truncated_unwind` (DWARF unwinding stops inside MKL assembly kernels that carry no CFI), else `unattributed.other`.
Both are reported, never redistributed.

The owner's five categories map onto the components as: **factorization** = `la.*` (sparse analyze/factorize/solve, dense
factorize/solve, matching/scaling) reported split into factorize versus solve; **QP subproblem logic** = `qp.*` (excluding backend
time, which sits in `la.*`); **function/derivative evaluation** = `eval.callback` (CUTEst and HS models, the harness bridge) +
`eval.assembly` (hven's partitioned evaluation engine); **setup and allocation** = `setup.*` + the `alloc_leaf` overlay + (reported
apart as non-solver) `proc.problem_load`/`proc.loader_startup`; **instrumentation and trace** = `instr.*` (+ `proc.harness_report`
reported apart). `kkt.*` (assembly, factor glue) and `outer.*` (globalization, IPM loop, quasi-Newton, scaling) are reported as
their own categories. The table:

| # | component | owner category | symbol regex | dso regex |
|---|---|---|---|---|
| O1 | `proc.problem_load` | PROC (chain-level: ANY frame) | `CutestModel::CutestModel\|CutestSource::\|CUTEST_load_routines\|HsSource::\|small_dense_(problems\|sources)\|cutest_problems\|hven_bench::roster\|dlopen\|_dl_open` | - |
| O2 | `proc.harness_report` | PROC (chain-level: ANY frame) | `hven::dump_counters\|hven::dump_options\|add_counters_dump\|\(anonymous namespace\)::(add_side\|spell\|fill_shared\|kv_\|CellReport)\|compute_declared_diagnostics\|DeclaredRows` | - |
| 1 | `eval.callback` | EVAL | `hven_bench::cutest::CutestModel::\|Hs\d+Model\|SplitModelNlp` | - |
| 2 | `eval.callback` | EVAL | - | `libpb\.so\|libcutest_double\.so` |
| 3 | `proc.harness_other` | PROC | `hven_bench::` | - |
| 4 | `proc.loader_startup` | PROC | `^_dl_\|^__libc_start\|^_init$\|^__do_global\|^_GLOBAL__sub_I\|^dl_main$\|^_start$` | - |
| 5 | `proc.loader_startup` | PROC | - | `ld-linux` |
| 6 | `setup.library_init` | SETUP | `MKL_Read_ThreadENV\|_Init_MKL_Loader\|mkl_serv_(cpu\|get_max\|init\|set_num\|mpi\|domain\|verbose)\|get_core_count\|mkl_pds_lp64_pds_get_num_threads\|__kmp_(env\|init\|register\|runtime\|middle_initialize\|do_serial\|hardware)\|mkl_cbwr\|mkl_serv_load` | - |
| 7 | `instr.counting_wrapper` | INSTR | `CountingNlp` | - |
| 8 | `instr.ledger_trace` | INSTR | `Ledger\|TraceSink\|JsonLinesTraceSink\|ConsoleTraceSink\|on_(iter\|solve\|ipqp\|ipm\|qp\|message\|fallback)\|QpCounters\|stop_poll\|trace_names` | - |
| 9 | `la.sparse_analyze` | LA | `FactorSession::analyze` | - |
| 10 | `la.sparse_factorize` | LA | `FactorSession::factorize` | - |
| 11 | `la.sparse_solve` | LA | `FactorSession::solve` | - |
| 12 | `la.dense_solve` | LA | `DenseSession::solve\|run_dense_solve\|DenseSymmetricFactor::solve` | - |
| 13 | `la.dense_factorize` | LA | `DenseSession::\|DenseSymmetricFactor::\|hven::linear::detail::dense_\|validate_dense` | - |
| 14 | `la.matching_scaling` | LA | `matching_scaling\|ruiz_symmetric_scale\|max_product_matching` | - |
| 15 | `la.other` | LA | `hven::linear::(?!detail::FactorSession::run_phase)\|FactorSession::(?!run_phase)` | - |
| 16 | `kkt.assembly` | KKT | `assemble_kkt\|build_bordered_system\|KktAssembly\|IpqpKktLayout\|kkt_assembly\|KktVector` | - |
| 17 | `kkt.factor_glue` | KKT | `KktFactor\|factorize_checked\|solve_vec\|inertia_regularization\|dense_selection\|DenseShiftSite` | - |
| 18 | `qp.schur_border` | QP | `SchurComplement\|BorderOps\|BorderState\|sync_borders\|append_border\|add_border` | - |
| 19 | `qp.refine_polish` | QP | `refine_bordered\|polish_\|bordered_(regularized_)?residual\|eqp_refine` | - |
| 20 | `qp.eqp_dependency` | QP | `solve_eqp\|solve_bordered_eqp\|certificate_coefficients\|detect_dependency\|dependency_test\|DepSystem\|verify_farkas\|test_candidate` | - |
| 21 | `qp.ssn` | QP | `SsnEngine` | - |
| 22 | `qp.ipqp` | QP | `Ipqp` | - |
| 23 | `qp.elastic` | QP | `run_elastic_ladder\|ElasticQp` | - |
| 24 | `qp.walk` | QP | `QpEngine\|WorkingSet\|build_subproblem` | - |
| 25 | `outer.scaling_basis` | OUTER | `ProblemScaling\|ColumnScaledModel\|EqualityBasis\|EqualityRowSubsetModel\|recompute_equality_basis` | - |
| 26 | `outer.quasi_newton` | OUTER | `Qn\|Lbfgs\|DenseDampedBfgs\|MixedLift\|MaskedQn` | - |
| 27 | `outer.restoration` | OUTER | `Restoration\|SocRecovery` | - |
| 28 | `outer.acceptance_linesearch` | OUTER | `Acceptance\|Funnel\|LineSearch\|BacktrackingLineSearch` | - |
| 29 | `eval.assembly` | EVAL | `NlpAssembly\|assemble_impl\|full_kkt_pass\|objective_constraints_pass\|NlpProblemModel\|NlpAdapterCore\|NlpConstraintPiece\|NlpObjectivePiece\|ConstraintModel\|ObjectiveModel\|AssemblyEvalSeam\|NlpModelAssembly\|refresh_(g\|jac)\|eval_.*_in_place\|evaluate_kkt` | - |
| 30 | `setup.init_teardown` | SETUP | `solver_init\|init_impl\|prepare_solve\|SolverImpl::(Sqp\|Ipm)SolverImpl\|(Ipm\|Sqp)Solver::(Ipm\|Sqp)Solver\|(Ipm\|Sqp)Solver::~\|Problem::Problem\|Problem::~\|ProblemState\|make_nlp_program\|NonLinearProgram::(make_nlp\|analyze_partitioning)\|AssemblyDeclaration\|problem_from\|ipm_program\|ThreadPool\|thread_pool\|validate::\|ProblemAccess\|SolveResult\|restate_claim` | - |
| 31 | `outer.ipm_nlp_core` | OUTER | `IpmSolverImpl\|Governor\|NonLinearProgram\|barrier\|IpmSolver::` | - |
| 32 | `outer.sqp_loop` | OUTER | `SqpSolverImpl\|SqpSolver::\|AssemblyEvalSeam\|continuation` | - |
| 33 | `proc.main_other` | PROC | `^main$\|^__libc_start_call_main$\|^_start$` | - |

## 5. Co-run protocol (declared per CLAUDE.md section 7)

- The instruction legs assert ONLY deterministic columns (instruction totals, counters, statuses) at `MKL_NUM_THREADS=1`
  (`OMP_NUM_THREADS=1`), so they co-run.
- **Tier / width / pinning:** `tools/pf3_corun.py`, width 6, one process per pinned PHYSICAL core: logical cpus 0,1,2,3,4,5 (physical
  cores 0-5), their SMT siblings 8-13 left idle, two physical cores (6, 7) left free; each cell process is `taskset`-pinned to its
  worker's cpu. Order: longest-first by the baseline wall in `cells.csv`, fixed before the run. `trunc30` cells run in the same pool
  (their shares are informational and cut by the clock; they are not asserted).
- The whole leg runs inside one `flock /tmp/box-build.lock` acquisition (`setsid -w`), so no other agent's solver runs beside it.
- Wall-clock from the co-run is informational; it is never quoted and never compared with a serial timing. Instruction counts of a
  `full` cell are scheduling-invariant, so no cell is re-run unless a status or counter disagrees between pass A and pass B; any such
  disagreement is a candidate, re-run alone before it is believed.

## 6. Exact call counts (callgrind subset)

Declared subset (small enough for callgrind's slowdown, covering both engines and both arms): hven-sqp {hs071, hs033, ACOPP57,
SMBANK, DALLASM}, hven-ipm {hs071, ACOPP57}. `callgrind --toggle-collect` is not used; whole-process call counts of
`FactorSession::{analyze,factorize,solve,solve_partial}`, `DenseSession::{factorize,solve}`, `QpEngine::run` and
`SchurComplement::{add_border,solve}` are read from the callgrind out file. Solves per factorization = (sparse + dense solve calls)
/ (sparse + dense factorize calls), also against the cell's own `factorizations` counter. Run under the same flock, solo (callgrind
is single-process).

## 7. The routes census and the pre-registered disposition (brief PF-3)

- Binary: `hven_sqp_ssn_safeguard_probe` built from this tree at the pin (Release, `-DHVEN_BUILD_BENCH=ON`, clang 22.1.8, LTO off) in
  `.scratch/build-bench`; invocation `MKL_NUM_THREADS=1 ./hven_sqp_ssn_safeguard_probe routes` with its default QP count and default
  seed (taken once, existing binary, no flag changes), stdout kept verbatim in `out/routes-census.txt`.
- The attribution column is built ONLY IF any census, TRAIN or suite-v1 row at this pin shows `escapes > 0`. "Rows" examined: the routes
  census output; the 90 cells of section 2 (`side.ssn.ssn_escapes`, `side.ipqp.ipqp_escapes`, `side.elastic_from_ipqp_escape` and the
  per-reason `*_escape_*` fields); and the committed harness rows at this pin (`runs/2026-11-m8-pin-48e1/hs/counters-*.csv`, the HS
  TRAIN rows taken at 48e1eeae). If none shows escapes > 0, the escape-path target is retired with that evidence, and the next
  escape seen re-opens it. If any does, the per-escape factorization attribution is built from the call-chain shares (escape ladder
  frames `SsnEngine`/`Ipqp*Escape*`) plus the escape counters, and reported as a column.

## 8. Deliverable

`out/` (per-cell JSON, aggregate CSVs, the routes census, the callgrind extracts), `RESULTS-PF-3.md` (attribution tables suite /
per lane / per set, the ranked behaviour-neutral opportunity list with instruction share, dominating cells, fix idea and neutrality
class, the escape disposition, open items), `SOLO-LEG.md` (pending wall leg, exact command).

## Addendum 1 (committed before the small-dense leg; amends sections 2, 3 and 4; nothing was measured differently on the 90 cells)

**What happened on the 90-cell leg, and the amendments it forced** (the first four legs are retained, not discarded):

- Leg 1 ran with `perf record` per-CPU buffers: five trunc30 cells and every `full` cell lost their pass B (the sample buffers do not
  fit 6 concurrent recorders; zero samples), while pass A (the instruction totals) was intact. Kept as `out/leg1-passB-failed/`.
  Amendments: `--per-thread -m 4M`; a pass-B failure is now an error row, not a silent zero.
- Leg 2 (`out/run1-cells`) and legs 4-5 (`out/run2-cells`, `out/run3-cells`) lost samples to the ring buffer at sample periods below
  ~250 000 instructions (est/total 0.06-0.99). Amendment: `P = max(500 000, total / 12 000)`. The final 90-cell leg is `out/cells`.
  The pass-A instruction totals of all four runs are compared in `out/determinism.txt` (the cross-run check of "instructions are
  scheduling-invariant", with every status/iteration/factorization counter equal across runs).
- Exit code 3 of `hven_bench_cell` is the harness's `--assert-reference` verdict (the three known REFERENCE FAILURE cells hs025 on both
  lanes and hs033 hven-sqp, identical to the pin-48e1 run); it does not invalidate the sample and is accepted.
- **Sample-count criterion (amends section 3):** `est_over_total` within 1 % is required of cells with at least 200 samples; for the
  tiny HS cells (13-17 samples each at the 500 000 floor) it is bounded by one period over the total, and their shares are reported
  only as aggregates over the 22 cells of a lane.
- **Truncated unwinds (amends section 4).** DWARF unwinding stops inside MKL assembly BLAS kernels (no CFI): 7.3 % of all instructions
  are samples with a chain of at most 3 frames whose leaf is a BLAS-2/3 kernel (`dger`, `dgemm`, `dgemv`, `dsyr`). They are
  redistributed, per cell, over components in proportion to the same cell's fully-unwound samples whose chain holds a frame of the
  same BLAS family (`components.blas_family`); a family with no such evidence stays `unattributed.unresolved`. Raw figures are kept in
  every JSON (`components_raw` is recomputable) and the aggregate reports raw and unresolved shares.
- Two further per-sample records were added before the final leg and change no attribution: the first hven frame above the
  linear/KKT glue for each backend sample (`backend_callers`), and the non-hven leaf by nearest hven frame (`leaf_by_hven_caller`).
- The routes-census attribution run is repeated with the amended tool (`out/routes-attrib.json`).

**The small-dense set joins the profile.** The owner's population "CUTEst TRAIN" includes `cutest-small-dense v1` TRAIN (97
problems; its roster is in the binary and its decoded instances are those of the hven-bench-sd checkout, linked into
`.scratch/h3/decoded` next to the 35 of `cutest-nlp-35`): `cells-sd.csv`, 97 x 2 lanes = 194 `full` cells, all `optimal` or a
terminating status inside 21 s at the frozen baseline (`runs/2026-11-m8-corun-cutest/sd97/serial-A`), same instrument, same
co-run protocol (width 6, cpus 0-5, one flock acquisition, longest-first order from that baseline). The dense arm lives in this
set, so the "fixed overhead on small problems" and the factorization pathologies (SPANHYD, CRESC100, MSS1 are all small-dense TRAIN
cells) are read from it. HOLDOUT problems of the set (42) are not named anywhere. Pass A and pass B each run under a `timeout`
(600 s and 900 s); a cell that trips it is an error row, to be re-run as `trunc30`, never a silent zero.

## Addendum 2 (committed before the re-run of both legs; amends section 4)

**A classification bug found in the first complete tables, and its correction.** The table's regexes were applied to the full
demangled signature, parameter list included, so a frame such as `QpEngine::ratio_test(..., hven::QpCounters&)` matched the
instrumentation rule through its argument type, and `solve_bordered_eqp(..., KktAssembly const&)` matched `kkt.assembly` before the
QP rule. Rules are now matched against the function name alone (`components.func_name`: `(anonymous namespace)` normalised to
`ANON`, the name cut at the first `(`). No rule text changed except `ANON::` for the harness-report override. Sampling, cells,
protocol, periods and the redistribution rule are unchanged. Both legs (90 + 194 cells) and the routes attribution are re-taken
with the corrected table; the tables produced by the faulty classifier are retained under `out/superseded-signature-match/`
(their pass-A instruction totals are unaffected and join the cross-run determinism check).
