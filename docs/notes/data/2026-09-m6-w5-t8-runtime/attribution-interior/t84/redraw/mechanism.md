# W5 T8.9r-attrib4 — what E1 and E2 change, what they bought, and what E3 found

`steps.md` has the numbers. This file is what the two patches do with file and
line, the two facts that bound what they could ever have bought, the page-fault
instrument's answer — which is this leg's one quantitative result — and E3's
site list.

**The honest summary first.** Neither experiment recovers the step: **E2 +4.5 %
of the corpus, E1 −16.5 % (a regression)**, both inside or about one
round-to-round spread. But the fault counter moves hard and in both directions,
and it moves the wall almost not at all: **E2 removes 57.3 % of the head's
excess minor page faults and recovers 4.5 % of the wall step; E1 ADDS 55.6 % on
top of the excess and costs 16.5 %.** The page-fault count is therefore **not
what carries `9cebbbe`'s +2.6…3.2 %** — which is a bound on attrib3's §E
reading, measured rather than argued.

---

## A. What E1 changes

`experiment-E1.patch`, against `e51a7e0`, three files.

* **`include/hven/drivers/solve_result.h:183`** — `SolveResult::reset_for_call()`
  is added beside `export_warm_start()`. It assigns every base field the
  initialiser its own declaration gives it: `status`, the four shared
  diagnostics, `f`, `iterations`, `wall_seconds`, the six `Vec` blocks and
  `export_snapshot_`.
* **`include/hven/drivers/ipm_solver_types.h:661`** — `IpmResult::reset_for_call()`
  is added after `eval_error_log`. It calls the base's, then assigns the
  engine's own thirty-six fields their declared defaults: the two payload
  counters, the treatment, the two internal-fixing blocks, the four terminal
  residuals to NaN, the six timings, the factorization stats, the SOC/watchdog
  counters, `recovery_depth_histogram.fill(0)`, the nine `last_*` sentinels,
  `last_eval_exception.clear()`, `last_kkt_info`, `phases.clear()`, the two
  analysis snapshots, `kkt_factor_counters` and `eval_error_log.reset()`.
* **`src/drivers/interior_point_solver.cpp:5075`** — `this->result_ = IpmResult{};`
  becomes `this->result_.reset_for_call();`.
* **`src/drivers/interior_point_solver.cpp:6086`** — `return std::move(this->result_);`
  becomes `IpmResult out = this->result_; return out;`.

The post-condition at entry is identical to the assignment's — a default
`IpmResult`, every sentinel written, nothing of a previous call reachable — and
the gate proves it on 2 550 cells. What changes is which side of the call keeps
the buffers.

**TWO FACTS BOUND WHAT E1 COULD EVER HAVE BOUGHT, and both were established by
reading before the arms were built.**

1. **`Vec` is `Eigen::Matrix`, which has no capacity distinct from its size.**
   `resize(0)` on an Eigen dense block **frees** its storage
   (`DenseStorage::resize` deletes and re-allocates whenever the element count
   changes), where `std::vector::clear()` keeps it. And an EMPTY block is the
   declared meaning of "unmeasured" for `ce`, `ci` and the multiplier blocks
   (`solve_result.h:150`, `:153`), so emptying them is what the contract
   requires whatever it costs. E1's capacity-preservation therefore reaches
   `phases`, the two message strings and the object itself — **not** the six
   `Vec` blocks, which are where the bytes are.
2. **THE BENCHMARK CONSTRUCTS A FRESH SOLVER FOR EVERY MEASURED ROW.**
   `bench/ipm_corpus_leg.cpp:620` — *"THE MEASURED SOLVE'S SOLVER, fresh, so its
   counters start at zero whatever ran above"* — `ConfiguredSolve ipm_owner =
   make_configured();` at `:622`, one `ipm.solve(program, x0)` at `:654`, and
   the owner dies with the row. So `result_` is default-constructed at the entry
   of the ONLY call that solver ever takes, and there is no previous call's
   buffer for a persistent member to keep. **A cross-call persistence mechanism
   has nothing to persist into in this harness** — and neither did the parent
   `102f729`, whose `result()`-by-reference was equally per-row here.

   That is not a defect in the brief's hypothesis; it is a MEASUREMENT of its
   reach. Whatever carries the step, it is reached inside ONE solve on a
   freshly-constructed solver, which is the same condition at both arms.

E1's remaining effect is therefore the exit copy, and the fault counter prices
it exactly: **+23 143 minor faults per process**, a second set of result blocks
allocated for the returned value while the member keeps its own. **E1 is a
regression and is reported as one.**

## B. What E2 changes

`experiment-E2.patch` is E1 plus two more sites, against `e51a7e0`.

* **`include/hven/drivers/interior_point_solver.h:941`** — a member
  `std::vector<IterateInfo> iters_scratch_;` beside `best_rhs_scratch_`.
* **`src/drivers/interior_point_solver.cpp:2580`** — `alg_impl`'s local
  `std::vector<IterateInfo> iters;` becomes
  `std::vector<IterateInfo> &iters = this->iters_scratch_;` with a `clear()`
  before the existing `reserve(this->effective_max_iters_)`. Every use below is
  unchanged.

Safe as shared state because `alg_impl` is called from exactly one site — the
phase loop at `:5714` — and never re-enters itself: restoration runs IN-PHASE on
this object and builds no second solver. `IterateInfo`
(`include/hven/detail/interior/iterate_info.h:16`) owns no heap storage, so what
survives is the row array itself.

**And this is a PER-PHASE, not a per-call, effect** — the one kind of
persistence this harness can express. It is the only reason E2 differs from E1
at all.

## C. The page-fault instrument — the leg's one quantitative result

Minor faults, whole process, on the wall pin's own four-cell invocation with
**no perf attached** (`/usr/bin/time`'s `%R`, whose format string is
`/usr/bin/time`'s argv and not the child's), three rounds, arm order rotated,
declared co-run tolerant. `faults.csv`, `logs/P*-pf-r*.log`.

| arm | the three rounds | median | vs base | vs head | sys (s) | user (s) |
|---|---|---|---|---|---|---|
| base `102f729` | 299 656 / 299 658 / 299 697 | **299 658** | — | −41 627 (−12.20 %) | 0.56 | 6.34 |
| head `e51a7e0` | 341 285 / 341 285 / 341 286 | **341 285** | +41 627 (+13.89 %) | — | 0.65 | 6.26 |
| E1 | 364 426 / 364 428 / 364 429 | **364 428** | +64 770 (+21.61 %) | **+23 143 (+6.78 %)** | 0.68 | 6.21 |
| E2 | 317 429 / 317 430 / 317 432 | **317 430** | +17 772 (+5.93 %) | **−23 855 (−6.99 %)** | 0.60 | 6.30 |

Under `perf stat -e page-faults,minor-faults` the same four read 299 628 /
341 256 / 364 401 / 317 404 — the same counts to within 30, on a counter whose
own within-arm spread is 3 counts in 300 000. **Major faults are 0 at every arm
in every round.**

**PUT THE TWO INSTRUMENTS BESIDE EACH OTHER AND THE READING IS FORCED:**

| | excess faults over base removed | corpus wall step recovered |
|---|---|---|
| E2 (a source change) | **57.3 %** | **4.5 %** |
| attrib3's experiment 4 (an allocator env intervention, both arms) | 99.7 % | 32.8 % |

A source change that removes more than half of the head's excess faults buys
**one twelfth** of what attrib3's allocator intervention bought. **So the
allocator intervention's 32.8 % cannot be attributed to the fault count alone**
— attrib3 already recorded that the same intervention made BOTH arms 6–7 %
faster, a global effect on the allocator's behaviour that is not the fault
count, and this leg's pair of numbers is the measurement that separates them.
attrib3's §E sentence — *"that last sentence is the reading of the evidence, not
a pinned measurement"* — is the sentence this bounds.

`sys` tracks the faults as it should (0.56 → 0.65 → 0.68 → 0.60 s), and `user`
does not move outside 6.21–6.34 s at any arm. The step is in neither column at
a resolution this instrument can see.

## D. E3, triggered and run

The pre-declared trigger is "the better of E1 and E2 recovers under 80 %". It
recovered 4.5 %, so E3 ran. Both halves are co-run tolerant; no wall figure from
either is quoted. `logs/E3-perf.log`.

### D.1 The fault SITES — and none of them is new at the head

`perf record -e page-faults -c 200 -g --call-graph dwarf,8192` on one large cell
(`f7_n10000_bound_neutral`), at the head, the base and E2. Share of the
process's faults, self, ≥ 1 %:

| site | base | head | E2 |
|---|---|---|---|
| `__memset_avx2_unaligned_erms` (libc) | 27.64 % | 27.63 % | 26.04 % |
| `Eigen::SparseMatrix<double,1,int>::operator=<SparseMatrix<double,0,int>>` | 13.29 % | 14.25 % | 14.22 % |
| `mkl_serv_memcpy_unbounded_s` | 7.17 % | 7.68 % | 7.88 % |
| `Eigen::internal::set_from_triplets` (both instantiations) | 12.02 % | 10.52 % | 10.50 % |
| `mkl_pds_lp64_mps_pardiso` | 5.27 % | 5.26 % | 5.47 % |
| `mkl_pds_lp64_metis_*` (three) | 7.80 % | 8.11 % | 8.54 % |
| `__memmove_avx_unaligned_erms` | 2.53 % | 2.85 % | 3.50 % |
| `hven::…::build_jacobian_pattern`'s lambda | 1.69 % | 2.19 % | 2.19 % |

**THE DISTRIBUTION IS THE SAME AT ALL THREE ARMS.** The largest share delta
between base and head is **+0.96 points** (Eigen's sparse `operator=`); nothing
moves by more than a point. If the head's extra 41 627 faults were a NEW
allocation, the site owning them would stand at ten-odd percent at the head and
zero at the base. **No site does.** The extra faults are the SAME sites faulting
more — the signature of pages that arrive cold, not of a new buffer.

**AND THE LARGEST SITE IS NOT hven's.** The caller chain under `__memset_avx2`
at the head is

```
main -> corpus::run_interior_cell -> corpus::run_interior_problem
     -> InteriorPointSolver::solve -> run_phase_sequence -> init_impl
     -> KktFactorization::compute -> linear::SymmetricFactor::analyze
     -> linear::detail::FactorSession::analyze -> run_phase
     -> mkl_pds_lp64_pardiso -> mkl_pds_lp64_pardiso_c
     -> mkl_serv_calloc -> __memset_avx2_unaligned_erms        (5.70 % of all faults)
```

— **MKL Pardiso's own workspace, `calloc`'d and zeroed inside the symbolic
analysis**, reached once per solve through `init_impl`. `9cebbbe` did not touch
`init_impl`, `KktFactorization::compute`, `SymmetricFactor::analyze` or anything
below them, and the harness builds a fresh solver per row at both arms, so this
allocation happens once per row on both sides. **No site is obvious in the sense
the brief's cap required, so the one permitted further experiment was NOT
taken.**

### D.2 dTLB and L1-D — no verdict, and the reason is measured

`perf stat`, whole process, median of three rounds, the four-cell invocation.
**Only `head` vs `E2` is like-for-like**: those two run the same 25 rows, where
`102f729` runs 15 in its 19-column schema, so its column is context, not a
comparison.

| event | base (15 rows) | head (25 rows) | E2 (25 rows) | E2/head |
|---|---|---|---|---|
| `dTLB-load-misses:u` | 5 690 786 | 5 874 701 | 5 800 785 | 0.9874 |
| `dTLB-loads:u` | 12 944 411 | 13 568 182 | 13 451 158 | 0.9914 |
| `L1-dcache-load-misses:u` | 757 074 585 | 782 871 927 | 783 933 309 | 1.0014 |
| `L1-dcache-loads:u` | 23 791 559 226 | 24 859 617 281 | 24 597 358 577 | 0.9895 |
| `instructions:u` | 58 196 079 989 | 60 959 150 154 | 60 301 837 460 | 0.9892 |
| `cycles:u` | 20 605 005 018 | 21 964 570 336 | 21 047 904 487 | 0.9583 |

`dTLB-store-misses` and `dTLB-stores` read **`<not supported>`** on this PMU in
every round at every arm. They are reported ABSENT, never zero-filled.

**NO RATE IS DERIVED FROM `dTLB-loads`.** It reads 13.5 million against 24.9
BILLION `L1-dcache-loads` in the same process, so whatever this Zen 3 event
counts, it is not the load stream; a "miss rate" built on it would be a number
about the PMU's event mapping and not about the program.

**AND THE WHOLE-PROCESS COUNTS RETURN NO VERDICT, on the floor attrib3
measured.** attrib3's byte-identical control pair moved `instructions` +1.02 %
and `cycles` +1.81 % (`../mechanism-tables.txt` §B1). E2 moves them −1.08 % and
−4.17 % while its WALL moves −0.11 %; a −4.17 % cycle reading beside a −0.11 %
wall reading is not a result, it is the instrument. The counters are recorded
and the verdict is refused, exactly as §11 and attrib3 refused it.

## E. What this leg establishes, and what it does not

**Establishes.**

1. The step is there and is the same size on a fourth independent round set:
   **1.0261** on the eleven scored rows.
2. **Persistent per-call result storage does not recover it**: E1 −16.5 %, E2
   +4.5 %, both inside or about one round-to-round spread, with every counter
   and diagnostic identical on 2 550 gate cells per arm.
3. **The fault count is not the carrier.** Removing 57.3 % of the head's excess
   faults by a source change buys 4.5 % of the step; attrib3's allocator
   intervention removed 99.7 % and bought 32.8 %, so most of that 32.8 % belongs
   to something the intervention did BESIDES removing faults.
4. **No fault site is new at the head**, and the largest is MKL Pardiso's own
   symbolic-analysis workspace, reached through code `9cebbbe` did not touch.
5. A persistence mechanism has **no cross-call surface in this benchmark**: the
   harness builds a fresh solver for every measured row at every arm.

**Does not establish.** Where the remaining +2.6 % is. It is not added work
(counters identical end to end), not a new fault site, not the fault COUNT, not
the solver object's data layout, not code placement, and not
`compute_declared_diagnostics` — six mechanisms now refuted by measurement
across attrib3 and this leg. **No disposition is offered — §11.1 and the owner
have it.**

**No committed source changed. E1 and E2 are candidate fixes and evidence; they
are not adopted here**, and E1 should not be: it is a regression on both
instruments.

APPLE / ACCELERATE: UNOBSERVED.  WINDOWS: UNOBSERVED.
