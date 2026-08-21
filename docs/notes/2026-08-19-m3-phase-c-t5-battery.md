# M3 phase-C task T5 — the major-loop carve: battery record

The proof battery for `de97f5d`
(`refactor(drivers): give SqpDriver's major loop a TU (phase-C T5)`), which
moved every member function of `SqpDriver` except its two constructors —
`solve_impl` above all — out of `include/hven/drivers/sqp_driver.h` and into
`src/drivers/sqp_driver.cpp`.

**Why this file is tracked.** Four of the six legs can only be measured
*against* the commit they judge, so their numbers cannot appear in its message.
T4's equivalent numbers lived only in an untracked workspace file and its review
called that out; this note is the fix. It is a record, not a pin: nothing here
is asserted by a test, and re-deriving any figure means re-running the leg.

**Conditions for every figure below.** Fedora, AMD Ryzen 7 5800X3D 8C/16T,
31 GiB, Linux 7.1.5-201.fc44; `/usr/bin/clang++` 22.1.8; MKL LP64; Release,
`HVEN_FP_MODE=SAFER_FAST`, `CCACHE_DISABLE=1`; the same host every phase-C
measurement from U0 onward was taken on. Base arm `718eef4`, new arm `de97f5d`.

## P-CENSUS — the bit-identity bar

57 cells of the walk corpus replayed on a fresh `de97f5d` corpus binary
(provenance stamp `de97f5d0a87d`, clean) and compared against the baseline of
record, `bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv`, on its 13
asserted counter/status columns via `scripts/run_walk_census.sh` under the
ratified parallel-tiered protocol (T1 = 44 cells 6-wide, T2 = 3 solo,
T3 = 10 deep-DNF cells 5-wide at full budget; `MKL_NUM_THREADS=1` per process;
one process per pinned PHYSICAL core, SMT siblings idle; ALONE on the machine).

**Result: 57/57 rows, merge exit 0, comparator exit 0, ZERO mismatches, empty
serial-confirm list.** Run 2026-08-19 16:33:49Z → 22:30:49Z (5 h 57 m): tier 1
36 m, tier 2 2 h 41 m, tier 3 2 h 40 m.

This is the leg that matters most for T5. The carve moved the entire major
loop's floating-point arithmetic — the trust-region update, the adaptive-mu
rule, every convergence and acceptance test — into a different translation
unit. Under U0's one uniform flag regime the expressions must compile as they
compiled inline, and this is the proof rather than the argument. Any counter
movement would have been a FAILED carve to be reverted or redrawn, never a
re-derivation.

## P-BENCH — runtime neutrality, under the owner ruling as amended 2026-08-18

B2's committed 17-cell `--engine ssn` wall set, 4 reps per arm, `taskset -c 2`,
`MKL_NUM_THREADS=1`, strictly serial, machine otherwise idle. The old arm was
measured **before the commit existed**. `bench_scale --self-check` passed on
both arms.

- **Counters bit-identical across arms on all 36 asserted columns × 17 cells ×
  8 reps**, `kkt_residual` included bit-for-bit. This is the ruling's hard,
  unconditional bar and it is met.
- Pooled median, all 17 cells: 35.0268 s → 34.9878 s, **−0.111 %**.
- Engine-heavy limb (7 cells ≥ 1 s): pooled **−0.082 %**; worst single cell
  **+0.16 %** (`f7_n1000_path_activity`), under a third of the amended ruling's
  +0.5 % reproducible-slowdown threshold.
- Sub-second limb (10 cells): pooled **−0.418 %**, and **all ten are faster**
  (−0.21 % … −0.64 %), so the one-sided +1.5 % cost gate is not approached.

Recorded, not claimed: the favourable direction is consistent with the ~10 MB
of object code the carve removes from the build, but a −0.1 % pooled move sits
inside the same layout/emission-order mechanism the ruling adopted (measured
cross-session floor +0.03 %), and the amended text says favourable moves are
recorded rather than adjudicated.

## P-BUILD — the T-series' build-time case

Three fresh clean builds per arm, identical configure line, `-j16` over all
targets, `/usr/bin/time -v`, median reported, machine otherwise idle.

| | before (`718eef4`) | after (`de97f5d`) | Δ |
|---|---|---|---|
| parallel wall-clock (median of 3) | 79.95 s | 67.57 s | **−12.38 s (−15.5 %)** |
| peak RSS (median of 3) | 669 016 KiB | 668 752 KiB | −264 KiB (−0.04 %) |

The arms' full ranges are **disjoint** — 79.56–80.35 s before, 66.83–67.65 s
after — so this is not a median artefact. T1+T2, T3 and T4 were four
consecutive null results and T4's report concluded that the series' build-time
case rested entirely on T5. It rests there and it holds.

Mechanism, measured rather than inferred: each of the eighteen translation
units that instantiated the driver loses 350–677 KB of object (≈10.2 MB across
the build) because ~2 330 lines of driver body, and the template instantiation
tree hanging off it, stopped being front-ended and code-generated eighteen
times over. The new TU pays that cost once. Peak RSS is flat because the
build's high-water process merely moved from "a test object compiling the
driver inline" to "the driver's own TU"; the saving is in total work, not in
peak memory.

## P-SUITE

`MKL_NUM_THREADS=1 ctest -j1`, fresh configures.

| config | registered | run | failed |
|---|---|---|---|
| Release (SNOPT found) | 1155 | 1153 | 0 |
| Debug (SNOPT found) | 1155 | 1153 | 0 |
| Release, no-SNOPT | 1151 | 1149 | 0 |

No test added or retired. The two not-run per config are the by-design skips
plus the two disabled `EqpRefinementAb` cases. `scripts/check_install_smoke.sh`
also passes on the post-carve tree — the new TU is a library object, so the
install/export/consume round-trip that CI's install lane exercises is worth
proving locally; it ran after the census finished, alone.

## P-SYM (scoped per phase-C ratification A1)

Both arms built sequentially into one and the same absolute build path.

```
65 objects — 46 byte-identical, 0 differing within the accepted noise class,
18 with unclassified differences, 0 missing
```

The 18 are exactly the declared blast radius, 1:1: every object that emitted a
weak copy of a moved `SqpDriver` member at base. Twenty such objects were
declared; the comparison script's capture set covers 18 of them (it omits the
`hven_sqp_f7_cold` and `hven_sqp_tau_bar_sweep_probe` executables). **Every
pre-existing library object is byte-identical, 0 of 24 differ** — including the
three that include the edited header without instantiating the driver
(`sqp_print.cpp.o`, `sqp_options.cpp.o`, `funnel.cpp.o`), which were the
falsifiable part of the declaration. Zero `__LINE__`-class rows, as predicted.
The script's nonzero exit is expected for a T-task: out-lining 2 330 lines
changes its includers' codegen by construction, which is why A1 scopes P-SYM
for these tasks.

## P-PCH (task T9's obligations)

`_hven_expected_source_count` 24 → 25; `docs/build.md`'s object arithmetic
carried. Per-TU measurement (two configures differing only in
`-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON`, compile command replayed 3× per arm,
minimum reported, objects byte-compared), with the two opted-in controls
re-measured in the same pass:

| TU | no-PCH | with-PCH | Δ | object |
|---|---|---|---|---|
| `drivers/interior_point_solver.cpp` (control) | 6.96 | 4.31 | −2.65 | identical |
| `model/nlp_solver.cpp` (control) | 4.07 | 1.84 | −2.23 | identical |
| `drivers/sqp_driver.cpp` (new) | 10.46 | 8.56 | −1.90 | DIFFERS |

The new TU does **not** join `_hven_pch_sources`: bar 1 cleared, bar 2 failed.
Fifth consecutive SQP-side TU in the "faster but not byte-identical" group.
It is also the most expensive TU in the library, 1.5× the next, with the
smallest PCH leverage per second of compile in the table — `src/CMakeLists.txt`'s
addendum records why. `scripts/check_pch_neutrality.sh`: PASS (PCH engaged on
6 TUs, 25/25 objects, `libhven.a` identical, 26 artifacts, 0 differing).

## The call-site inlining verification

The plan's T5 row predicted the loop "currently inlines `eval_nlp`,
`evaluate_kkt`, `build_subproblem`", which is what made this the series' highest
bench risk. Read out of the base commit's own object files, it does not.
`solve_impl`'s relocation set in `bench_corpus.cpp.o` at `718eef4` holds exactly
one `R_X86_64_PLT32` call per SOURCE call site for each of them — `eval_nlp`
3/3, `eval_nlp_values` 3/3, `evaluate_kkt` 3/3, `build_subproblem` 2/2,
`predicted_decrease` 2/2, `crash_basis_seed` 1/1 — plus `QpEngine::run` (5) and
`SsnEngine::solve` (1). **Zero of those fourteen sites was inlined at `-O3`
before the carve**, so the boundary cannot cost an inlining that never happened.

The callees the base build *did* inline into the loop — `map_status`, the three
trust-region update functions, `ssn_engine()`, and 3 of `finish`'s 10 sites —
all move into the same TU as the loop, so the inliner sees the same input;
`constraint_violation_l1` stays `inline` in the header the TU includes. No call
site anywhere loses a caller/callee pair to the boundary. The only new cross-TU
calls are into `solve()` ×3 and `attach_ledger`, each O(1) per solve.
