# M8 DN-0 IPM re-measure -- DECLARATION-r1

This file is the declaration. It is committed, with every instrument under `tools/`, BEFORE the
first measuring leg, and it is never edited afterwards. A failed leg gets a successor
`DECLARATION-r2.md`; the failed record is kept beside it. Results go to separate files
(`results/`, `PROVENANCE.txt`).

It succeeds `docs/notes/data/2026-11-m8-dn0/DECLARATION.md` (DN-0) for the **IPM engine only**.
DN-0's directory is frozen and is not touched. The SQP's DN-0 outcome (DN-1 fires) stands and is not
re-measured. Authority: `docs/notes/2026-11-m8-brief.md`, "### DN" (the pre-registered G-DENSE rule),
and the settler's ruling C259:
- the IPM is UNDECIDED, because DN-0's primary IPM reading measured a process-once MKL `dsecnd()`
  clock-calibration spin (~4e9 instructions against a 2-4e6 solve);
- DN-0's secondary (steady-state) IPM reading was non-evidential: a 1e6 period floor left 1-4 samples
  per cell (DN-0 `results/rule-notes.txt` R2).

F1 (origin/m8 `0a34c8a2`, `docs/notes/data/2026-11-m8-f1-init/`) removed the spin. This round re-runs
DN-0's legs for the IPM at that commit, with a finer sampling period, at least 10 repeats, and a
measurement-only build that carries CFI unwind tables.

## 1. Pin and binaries

- **hven** `0a34c8a2adba9e725351b3c9e23c14471d3fecb1` (origin/m8 head, F1 landed). It was built by the
  harness's unchanged `scripts/build_hven_pin.sh` (Release, clang++, `HVEN_LINK_TIME_OPT=OFF`, tests and
  bench off) into `.hven/install-0a34c8a2adba/`.
  - `libhven.a` sha256 `37d2a2ae...db02`.
  - Against DN-0's pin `58dc14bc`, 55 of 56 archive members are byte-identical; only `solver_init.cpp.o`
    differs (`raw/identity/libhven-58dc14bc-vs-0a34c8a2-members.txt`). The source diff is
    `src/drivers/solver_init.cpp` plus a header comment.
- **hven-bench** worktree `/home/ghecht/Projects/hven-bench-sd`, branch `m8-dn0r1` off main `6f2dfe9`, at
  `38f5b4a`. That commit is an EVIDENCE-ONLY pin move (`cmake/hven_pin.cmake` bdd797e3 -> 0a34c8a2). It is
  reverted in the commit after the run and never lands on main (the PF-1 / DN-0 precedent). Lane sources
  are unchanged.
- **Binaries** (`tools/expected.sha256`). `tools/preflight.sh` refuses any leg if a binary differs, if the
  harness HEAD is not `38f5b4a`, if its tree is dirty, or if the pin stamp is not 0a34c8a2.

  | binary | role | sha256 |
  |---|---|---|
  | `build/lanes/hven/hven_bench_cell` | MEASURED (S legs): `configure.sh` + `cmake --build build`, unchanged | `48902c5c...fd1e` |
  | `.scratch/dn0r1/build-cfi/hven_bench_cell` | RECORD (P leg, SC round): see below | `5145a913...be17` |
  | `.scratch/dn0r1/build-dump/lanes/hven/hven_bench_cell` | DUMP: link-closure objects identical to MEASURED, plus `-Wl,--wrap=pardiso` + `pardiso_wrap.c` | `4c2513ce...3020` |
  | `.scratch/dn0r1/build-tools/dn0_replay` | replay, measuring | `80a21da9...00af` (byte-identical to DN-0's) |
  | `.scratch/dn0r1/build-tools/dn0_replay_verify` | replay, iparm-identity check | `1ad55842...eded` (byte-identical to DN-0's) |
  | `.scratch/dn0r1/hven-cfi/install/lib64/libhven.a` | the CFI archive the RECORD binary links | `6f24b159...0f3b` |

- **The RECORD binary (CFI, measurement-only)** (`tools/build_instruments.sh`).
  - **Build.** The pinned source is built with the pin build's exact cmake arguments plus
    `-DCCACHE_PROGRAM=tools/cfi_launcher.sh`. hven's `CMakeLists.txt` installs `CCACHE_PROGRAM` as the
    compiler launcher. The launcher runs each compile unchanged and appends `-fasynchronous-unwind-tables`
    last, which overrides the library's `-fno-asynchronous-unwind-tables`. All 57 compile commands are
    wrapped. The pin build itself ran through ccache, which does not change object bytes.
  - **Link.** The RECORD binary is the MEASURED binary's own link command (`ninja -t commands`), run from
    `build/` on the same lane objects. Only the `libhven.a` path and the output path are swapped (link.diff
    in PROVENANCE).
  - **Code bytes unchanged -- proven before this declaration:**
    - **archive level** (`tools/code_identity.py`, `raw/identity/libhven-code-identity.txt`): 56 members;
      all 6756 executable sections (6,891,718 bytes) and their relocations are byte-identical. The only
      differences are `.eh_frame` and `.rela.eh_frame` (51 members differ, 2 gain one). The installed
      header trees are identical.
    - **binary level** (`tools/binary_text_identity.py`, `raw/identity/binary-text-identity.txt`): `.text`
      and `.rodata` have the same address and size. The RW segment moves by 0x3000 (a page multiple, so
      alignment is preserved). Of 21,455,966 instructions, 21,422,822 are identical text. The other 33,144
      differ only by an RIP-relative displacement of exactly +0x3000. No other difference.
    - **counter level**: the SC round (§3) measures every cell's instructions on the RECORD binary and
      reports SC / S-median per cell (informational; not a gate).
- **D/R and F1.**
  - F1 changed one archive member, `solver_init.cpp.o`. The replay binaries are byte-identical to DN-0's.
  - D and R are nevertheless **re-run** for the 119 IPM cells at the new pin, and the rule reads the
    re-run.
  - `tools/dump_identity.py` compares every r1 dump with DN-0's on matrix bytes, `index.tsv` (excluding
    the session-handle address) and cell outcome. This is reported in results and is not a gate.

## 2. Population and terms

- **HS TRAIN (22)** = the 21 TRAIN members of `sets/hs-27.json` plus `hs071`. **Small-dense CUTEst TRAIN
  (97)** = the TRAIN members of `sets/cutest-small-dense.json`. Lane **hven-ipm only: 119 cells**.
- `tools/roster.py` is DN-0's with `LANES = ("hven-ipm",)`. It re-checks split-v1 and the HOLDOUT guard,
  and any HOLDOUT name aborts. **HOLDOUT is never run.**
- Cell argv, cwd, environment (`MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`), shipped defaults, the 300 s wall
  cap and `dnf_wall` are DN-0's (DN-0 §2).
- Serial: one cell process at a time, alone under `/tmp/box-build.lock`, the lane process pinned to core 2
  (SMT sibling 10 idle).

## 3. Instruments (`tools/`; changes from DN-0 named)

- **S -- totals** (`legs.sh S1/S2/S3`, `cell.sh stat`, MEASURED binary): DN-0's, unchanged.
  - `taskset -c 2 perf stat -x, -e instructions:u,cycles:u,branches:u`, one process per cell, one untimed
    warm-up process first.
  - Round 1 covers all 119 cells; rounds 2 and 3 cover the cells not capped in round 1. Cell total = the
    median of its rounds. The round-1 CELL row is the cell's status row.
- **SC -- counter identity** (new, informational; `legs.sh SC`, `cell.sh statrec`): one perf stat round on
  the RECORD binary, same form as S, cells not capped in S1.
- **P -- buckets** (`legs.sh P`, `cell.sh record`, `attribute.py` and `bucket_map.tsv` byte-identical to
  DN-0's). Changes from DN-0:
  - **binary**: the RECORD binary (CFI), not the MEASURED one;
  - **form**: `taskset -c 4 perf record -q -m 1M -e instructions:u -c PERIOD --call-graph dwarf,16384
    --user-callchains -- taskset -c 2 <lane>`. perf's reader runs on core 4 (SMT sibling 12 idle) and the
    lane process runs alone on core 2. DN-0 ran both on core 2. Only the lane process (comm
    `hven_bench_cell`) is attributed, as in DN-0;
  - **period**: base `= max(250007, T/8000)` (odd), where `T` = the S median (DN-0: floor 1000003);
  - **repeats**: `R = min(100, max(10, ceil(8000 * base / T)))` (DN-0: `min(40, ceil(4000*base/T))`).
    Repeat `k` runs at period `base*(1000+7k)/1000` (odd). A capped cell gets one 60 s repeat
    (informational, as in DN-0);
  - **defect logging**: each repeat's `perf report --stats` LOST, LOST_SAMPLES and THROTTLE counts are
    written to the cell's `.runs` file;
  - attribution, weighting by period, B_proc exclusion and the fallback rules are DN-0's.
- **D -- KKT dumps** and **R -- dense replay**: DN-0's instruments (`pardiso_wrap.c` and `dn0_replay.cpp`
  are byte copies), re-built against the 0a34c8a2 install and re-run over the 119 IPM cells. R now also
  checks the iparm identity per cell (DN-0 §7 named it a failure condition; `legs.sh` enforces it).

## 4. Definitions (read by `tools/rule.py`, the rule's only implementation)

DN-0 §4, verbatim in substance: the buckets, `total` (B_proc excluded), share, `rho` (and the two
reported-only variants), negative-eigenvalue agreement (a matrix where either arm yields no count does NOT
agree), and the median cell per population. The median cell is ranked ascending by the S-median total
instructions; an S1-capped cell ranks above every finished cell; ties go by name; the median is rank
`ceil(N/2)` (22 -> 11, 97 -> 49). A median cell with no replayed matrix fails the DN-1 condition.

## 5. The rule (brief, verbatim; scope as DN-0 declared it)

| fires | condition | then |
|---|---|---|
| **DN-1** (dense arm behind the factor layer) | `(B_sym + B_fac) / total >= 0.40` **and** `rho <= 0.5` on that cell, **and** negative-eigenvalue counts agree on >= 99 % of replayed matrices | build DN-1 |
| **DN-2** (dense KKT assembly) | `B_kkt / total >= 0.25`, measured after DN-1 if DN-1 was built | build DN-2 |
| **no dense build** | neither fires | record which bucket dominates. If it is B_setup or B_eval, that bucket is registered as the small-problem lever with its numbers, and no dense code is written in M8 |

- **The outcome read here is the IPM engine's.** DN-1 fires for the IPM if its condition holds on BOTH IPM
  median cells (HS TRAIN, small-dense TRAIN). The milestone outcome (DN-1, by the SQP) is DN-0's and is
  unchanged whatever this reads.
- **Agreement clause, read exactly as DN-0 declared it**: over the median cell's own replayed matrices
  ("on that cell"). Criteria are never changed after data (C259). Beside it, `rule.py` REPORTS:
  - the population's pooled agreement over every replayed matrix of every cell;
  - the DN-1 condition evaluated with the pooled figure in place of the per-cell one.

  Both are labelled "reported, not the rule".
- DN-2 is read with the same scope. If DN-1 fires for the IPM, the IPM's DN-2 reading is provisional.
- "Dominates" = the largest share on each median cell, plus the population-wide picture (`summary.py`).
- **Primary reading** = the definitions above (process total, B_proc excluded, the process-once init
  included): the rule outcome.
- **Secondary reading** (informational, DN-0's mechanics): samples whose chain contains
  `hven::detail::ensure_solver_initialized` are removed. After F1 that function calls
  `mkl_get_max_threads()`.

## 6. What was seen before this declaration (disclosure)

No population cell was run before this commit. Pre-declaration work:

1. **Builds and identity proofs** (§1): `tools/build_pin.sh`, `tools/build_instruments.sh`, and the
   `raw/identity/` files.
2. **Sampling calibration** (`tools/calibrate.sh`, `tools/calib_summary.py`,
   `raw/calibration/calibration-summary.txt`).
   - Cells OUTSIDE the population: cutest-nlp-35 TRAIN SMBANK, READING6, ZAMB2-8, DALLASM and ACOPP57, on
     the hven-ipm lane with the RECORD binary, run from the main harness's decoded directory. Each name
     was checked TRAIN and absent from both population sets.
   - This boot's kernel lowered `perf_event_max_sample_rate` to 79000 (journal 2026-11-07 18:46:34;
     F1 DECLARATION-r2). That allows 79 samples per 1 ms tick. The sysctl is not this agent's to change.
   - `-m` above 1M is refused for this user (`perf_event_mlock_kb` 516).
   - **DN-0's form** (perf and the lane process sharing core 2), 16 KB stacks: LOST samples in at least
     one run at every period from 25013 to 300007 (sporadically, 1-5 records, at 200003-300007), and
     none at 400009. It THROTTLEd at <= 100003. A smaller
     8 KB stack cut chain depth: chains reaching `main` fell from 0.3-0.6 to 0.06-0.2.
   - **The perf reader on core 4** removed LOST at every period (100003-250007, 15 runs each). THROTTLE
     remained only at 100003 (peak 83-88 samples/ms). At the declared 250007 the result was: LOST 0,
     THROTTLE 0, peak 32-42 samples/ms (at most 53 % of the limit), sampled/perf-stat instructions
     0.993-1.000, unmatched 0.4-2.4 %, about 0.3 s per repeat.
   - The calibration fixed the period floor (250007), the stack copy (16384, as DN-0), the perf-reader
     core (4) and the repeat rule (target 8000 samples, 10..100 repeats).
3. **Not a rule input**: sample counts per population cell are expected near 10^3 solver samples for
   ~6e6-instruction cells (100 repeats x ~24 samples, B_proc included). The rule reads shares,
   not counts.

## 7. Failure and successor

A leg FAILS if any of the following happens; it then gets a successor `DECLARATION-r2.md`, and the failed
record is kept:
- the roster or pre-flight refuses;
- a roster cell has no `.result` for its leg;
- an S1 or SC `.stat` lacks an instructions count;
- a cell has no attribution JSON after P;
- any replayed matrix fails the iparm identity check;
- **any P repeat of any population cell records a LOST, LOST_SAMPLES or THROTTLE event** (new; DN-0
  only reported lost samples).

`legs.sh` exits non-zero on each of these and records the reason. A cell's solver outcome (error,
infeasible, capped) is a measurement, never a failure. A dump-identity difference against DN-0 is a
reported finding, not a failure (the rule reads the r1 re-run).

## 8. Carried caveats

DN-0 §8, unchanged: the CUTEst derivative-check caveats (`cells.csv` `caveat`). Every figure is MKL
Pardiso on Linux x86-64. Apple/Accelerate is **UNOBSERVED**. Wall-clock is informational and never quoted
as a measurement.

## 9. Order and outputs

- **Order**: S1, S2, S3, SC, P, D, R. Each leg runs as `flock /tmp/box-build.lock bash legs.sh LEG
  HARNESS_ROOT RUNDIR`, with RUNDIR = `hven-bench-sd/.scratch/dn0r1/run`. The chain stops at the first
  failing leg.
- **Outputs**:
  - `results/cells.csv`, `results/rule.json`, `results/rule.txt`, `results/summary.txt`,
    `results/dump-identity.txt`, `results/dump-fidelity.txt`;
  - per-cell raw summaries under `raw/` (stat files, `.runs`, attribution JSON, replay and verify CSVs,
    dump `index.tsv`);
  - `PROVENANCE.txt`.
- perf data, perf script text and binary matrices stay in the harness's git-ignored `.scratch/dn0r1/` and
  are not committed.
