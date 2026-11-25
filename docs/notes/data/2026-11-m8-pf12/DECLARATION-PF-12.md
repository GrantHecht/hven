# DECLARATION-PF-12 -- PF-1 (CEN-R1) and PF-2 (CEN-R2), measurement phase

Written before any measured leg. MEASUREMENT ONLY: no library change; the brief is
docs/notes/2026-11-m8-brief.md section PF.

## Pins
- m8 arm ("m8"): commit `331b31837` (branch m8 head at task start) plus ONE scratch-only lever
  (`scripts/pf12-lever.patch`, bench/corpus_cells.h, 5 lines): env `HVEN_PF12_QP_FAILURE_BUDGET`
  sets `SqpAdvancedOptions::max_consecutive_qp_failures` (the renamed `qp_failure_budget`; the
  corpus binary has no flag for it). The patch lives in a detached scratch worktree
  (`.scratch/m8-331b31837`); the pf2 branch itself carries no code change. The configure-time stamp
  of that binary therefore reads `-dirty`; this is declared, and the identity proof below replaces
  a clean stamp: with the env unset the binary must reproduce the close-census row of a cell that
  does not route a QP failure.
- U0 arm ("u0"): the M6 tree `19f7674f6` (C179: reproduces U0 byte-for-byte on today's toolchain).
  Detached scratch worktree `.scratch/m6-19f7674f`, unmodified. It hard-wires the failure gate at
  1, so the m8 arm runs with `HVEN_PF12_QP_FAILURE_BUDGET=1` throughout PF-2 (like for like).
- Submodules identical in both trees (eigen bc3b3987, fmt 407c905e).
- Toolchain: clang 22.1.8 (Fedora 4.fc44), cmake 4.3.0, Linux 7.2.12-200.fc44, AMD Ryzen 7
  5800X3D, perf 7.2.12. Release, uniform regime: Ninja, `HVEN_FP_MODE=SAFER_FAST`, `/usr/bin/clang++`
  (CMakePresets linux-clang-release spelled out; binaryDir overridden), ccache disabled.
  Recipe: `scripts/build.sh <src> <build>` (target `hven_sqp_corpus`, -j8), driven by
  `scripts/build_both.sh`.

## Cells (from bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv vs 2026-08-16-u0-corpus)
Both baselines terminate Optimal on all of these, so they carry a defined qp_minors in each tree.
Tier A, the minor-heavy path cells (close-census qp_minors 10.6k-30k):
`f7_n1000_path_corrupted`, `f7_n1000_path_neutral`, `f7_n800_path_neutral`,
`f7_n825_path_neutral_control`, `f7_n750_path_neutral_control`.
Tier B, mid-weight path cells (context, not the discharge set): `f7_n800_path_corrupted`,
`f7_n2000_path_activity`, `f7_n2000_path_physics`.
Not asserted: `f7_n10000_path_physics` (deadline-dependent, brief section PF; not run).
HOLDOUT is never run.

## Instrument (docs/measurement.md section 4)
`perf stat -e instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u`
(`:u`; perf_event_paranoid = 2 on this box). One process per cell (`--internal-run-one <cell>
--engine walk`). Three rounds; arms ALTERNATED within a round (order swapped each round); per-arm
per-cell MEDIAN of the three; one untimed warm-up per worker per arm. The asserted quantity is
`instructions:u / qp_minors` (qp_minors from the row the same process wrote). Cycles, wall and
cache columns are informational.

## Co-run terms (CLAUDE.md section 7)
Counters/instructions only; nothing here is a quotable timing. Four workers, each pinned
(`taskset`) to one logical CPU of a distinct physical core (the 4 physical CPUs of the box-lane the leg is granted (BOX_LANE_PHYS; siblings idle)
by this leg), `MKL_NUM_THREADS=OMP_NUM_THREADS=1` in every process. The box is shared with other
agents' work; that cannot move instructions:u or counters (only a budget outcome, which these
no-deadline `--internal-run-one` processes do not have). Wall columns stay informational.
The whole leg runs under `box-lane` (shared lane, owner rule of 2026-11-20 replacing the flock).

## STOP conditions (determinism checks)
1. Per arm and cell the three rounds must agree on every row column except wall_s and residual-class
   last digits (counters and statuses exactly); otherwise the cell is not an instruction measurement
   of a fixed computation and is reported as such.
2. u0 rows must equal the U0 baseline row on the 13 asserted columns (re-confirms C179).
3. m8 rows with the lever at 1 are compared with the close-census rows: equality where no QP
   failure is routed; the PF-1 cell is expected to differ and is reported on its own.

## PF-1 procedure
The m8 arm of `f7_n1000_path_corrupted` (budget = 1) IS the PF-1 run. Discharge bar: status =
U0 status (Optimal) and factorizations <= 5952 (2x U0's 2976). One extra run at the shipped budget 3
(close-census condition) is recorded for the contrast. If the hit persists, locate it with the
per-QP-solve counters of the row (qp_subproblems, qp_fact_per_qp, ssn_*/ipqp_*/dep_* columns) and,
if needed, a debug-counter read of the ledger.

## PF-2 procedure
Ratio per cell = (m8 instructions/qp_minors) / (u0 instructions/qp_minors), medians. The bar is
<= 1.15 on tier A. Attribution of the excess by symbol: `perf record -e instructions:u` on the m8 and
u0 binaries for two representative tier-A cells (one pinned core each, serial with respect to the
instruction leg), symbols binned into E2 detector/screen, E4 in-EQP polish, BPS refinement, and the
rest; the excess is `(m8 per-minor - u0 per-minor)` split by bin.
