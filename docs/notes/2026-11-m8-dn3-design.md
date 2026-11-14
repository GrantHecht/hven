# M8 DN-3: measured dense auto-selection (design and pre-registration)

Date: 2026-11-12. Author: the DN-3 design lane. Status: DRAFT r0. This round writes no code and runs
no measurement leg. Base: `m8` at `b70e45cfb`.

Authority:
- the M8 brief, `docs/notes/2026-11-m8-brief.md`: DN-3 (`:286-295`), the scope valve (`:103-106`,
  item 1: DN-2/DN-3 may be cut and stay registered), Q4 (`:488-491`: DN-3 sits behind the valve);
- the DN-1 design, `docs/notes/2026-11-m8-dn1-design.md`: §4 (selection and the size gate), §10
  (Accelerate), §14 (the proof pattern), §15 R2/R10, §18 (DN-1d);
- the evidence: DN-0 (`docs/notes/data/2026-11-m8-dn0/`, `-dn0-ipm-r1/`), the DN-1 design analysis
  (`docs/notes/data/2026-11-m8-dn1-design/`), DN-1 (`-dn1-impl/`), DN-1b (`-dn1b-guard/`),
  DN-1c (`-dn1c-sqp-check/`), DN-1d (`-dn1d-singular/`);
- the settler's binding inputs for this round (from the ledger, which this lane cannot read; quoted
  as relayed), numbered I1-I9 below where they are answered;
- the owner's 2026-11-08 ruling that a Linux-adopted default ships on every platform given CI
  correctness (I6).

The declaration is `docs/notes/data/2026-11-m8-dn3/DECLARATION.md`. Every number here is Linux
x86-64 with MKL 2026.2. Apple/Accelerate and Windows values are UNOBSERVED throughout.

---

## 0. Decisions at a glance

| topic | proposal | section | needs a ruling |
|---|---|---|---|
| pre-registration | **No DN-1 close declaration exists.** The brief's DN-3 clause is the only pre-registration of the threshold rule. Its density clause therefore does not fire: the threshold is `n_kkt` alone | §2 | no (a fact) |
| threshold reading | per engine, `rho` = DN-0's rule definition (fresh analyze+factorize+solve), read on DN-0's frozen replay: **T_SQP = 105, T_IPM = 167**. Values already published (DN-1 design §4); disclosed as known | §2 | **yes, Q1** |
| what `kAuto` selects | per analyzed pattern at `analyze()`: dense iff n ≤ T for the engine. Every factor that today receives the engine's policy receives it; the predictor and the equality basis stay `kNever` | §3 | no |
| singular policy | SQP `kReport` (the DN-1d path, unchanged); IPM `kSparseFallback` + the DN-1b step guard, the IPM's only built path, disclosed as interim | §3.3 | **yes, Q3** |
| threads | the dense arm runs at 1 thread under every dense policy. **Not implemented today** (`dense_session.cpp:184, 285, 372, 398` use `opts_.threads`) | §3.4 | **yes, Q4** |
| adoption | per engine; the RETAKE21 per-cell rule on instructions (init and process overhead excluded); families = HS TRAIN and small-dense TRAIN | §5 | **yes, Q2** (per engine vs joint) |
| measurement | serial, alone, `MKL_NUM_THREADS=1`, perf stat ×3 per arm; init and B_proc shares from perf record on a CFI build, period ≥ 400,000 | §4 | no |
| identity above T | measured, not argued: 27-cell replay × 3 and the 57-cell census at the flipped default, 0 differences. A small side factor (the rank-repair Gram) can fall below T inside an F7 cell | §6 | STOP routes to the settler |
| SQP solve check (I4) | every shifted answer is ρ-gated; unshifted dense answers carry a per-solve check only on the walk. SSN step, IPQP and the rank-repair Gram have none | §7.1 | **yes, Q5** |
| platforms | adopted on Linux TRAIN ⇒ the default on all platforms, gated on each CI lane's correctness; Apple/Windows performance UNOBSERVED. Supersedes DN-1 design §10's "platform-scoped" plan | §8 | no (owner ruling) |
| public names | none required. Optional, for owner approval: a per-solve count of dense-taken factorizations on both engines | §9 | **yes, Q6** |

---

## 1. What DN-3 is for, and what it is not

DN-1 built the dense arm and proved it forced (`kAlways`) on DN-0's 238 TRAIN cells. The SQP saving
is large: init-excluded 0.54 (hs043) and 0.70 (HS84) of solver instructions at `MKL_NUM_THREADS=1`
(`dn1-impl/dn1legs/PROVENANCE.txt`, M; DN-1d r3 re-reading 0.544 / 0.705). DN-3 decides two things:
1. the size at which the default (`kAuto`) stops using the dense arm (§2);
2. whether `kAuto` becomes each engine's default (§5).

Out of scope, unchanged from the brief: dense updates inside the QP engines (walk Schur updates,
IPQP), DN-2 (dense assembly), the HOLDOUT (I9).

## 2. The threshold rule

### 2.1 What was pre-registered, and where

The brief, `docs/notes/2026-11-m8-brief.md:286-290` (lines 288-290 from `d0f8cd99e`, "brief review
fixes per settler ruling C193", 2026-11-01; the surrounding clause from `bd3d234a2`), verbatim:

> **DN-3 (conditional on DN-1): measured auto-selection.** A size/density threshold
> `(n_kkt, nnz / n_kkt^2)` is chosen on TRAIN by a rule pre-registered at DN-1's close:
> - the threshold is a single `n_kkt` value, the largest `n_kkt` at which every replayed matrix of
>   that size or smaller has `rho < 1`; the density coordinate is recorded and used only if DN-1's
>   close declaration pre-registers a two-coordinate rule with its fit stated;

**No DN-1 close declaration exists on `m8` at `b70e45cfb`.** Searched: every `DECLARATION*` under
`docs/notes/data/2026-11-m8-dn*` (DN-1 impl, -dn1legs, -r2; DN-1b r1/r2; DN-1c r1-r3; DN-1d r1-r3),
the DN-0 and DN-0 r1 declarations, and the commit log since DN-1. None mentions a DN-3 threshold
rule, a two-coordinate fit, or a DN-1 close. The one other text is the DN-1 design §4
(`2026-11-m8-dn1-design.md:345-349`, `a8c540cf7`), which is a design statement, not a declaration:

> DN-3 reads this page's cumulative tables (`results.txt` [B], [C2b], [E3c]) for its threshold rule.
> The brief's DN-3 rule (the largest n at which every replayed matrix of that size or smaller has
> `rho < 1`) gives n = 105 for the SQP and 167 for the IPM on DN-0's replay. [...] That `rho` counts
> a fresh analysis per matrix. Whether DN-3 should read the engine-weighted ratio instead is DN-3's
> to declare.

Consequences, stated plainly:
- **The density coordinate is not used.** Its condition (a two-coordinate rule pre-registered at
  DN-1's close, with its fit) did not happen. Density (nnz / n²) is recorded per matrix, as the
  brief asks, and read by nothing.
- **The brief's sentence leaves three choices open** that a "rule pre-registered at DN-1's close"
  would have fixed: which `rho`, which replay, and one value or one per engine. This page does not
  invent answers to them. It proposes one reading (§2.2), lists the alternatives (§2.3), and holds
  every leg until the settler rules (Q1, DECLARATION §1).

### 2.2 The proposed reading (P-A), for the settler's ruling

For each engine E ∈ {IPM, SQP}:

> **T_E** = the largest integer N such that every matrix m in DN-0's replay of engine E with
> n(m) ≤ N has rho(m) < 1, where rho(m) = `dense_total(m) / sparse_total(m)` as recorded in
> `docs/notes/data/2026-11-m8-dn1-design/matrices.csv.gz` (sha256 `f204be2c...b078c`, one row per
> replayed matrix of DN-0's R leg). A matrix with a missing or non-finite value counts as
> rho ≥ 1. If no matrix has rho ≥ 1, T_E is the largest replayed n.

The terms, from DN-0's declaration (`dn0/DECLARATION.md` §3 R, §4):
- **dense** = `DenseSymmetricFactor` construct + `try_factorize` + `solve` (one RHS), MKL `dsytrf`,
  densification excluded;
- **sparse** = `SymmetricFactor` construct + `analyze` + `factorize` + `solve`, options rebuilt from
  the captured iparm;
- both in user-space instructions, the median of three repetitions; the population is the first 50
  numeric factorizations of every one of the 238 TRAIN cells (5501 matrices).

This is the `rho` DN-0's rule reads ("`rho` (the rule's)"), and the brief's DN-3 clause uses the same
symbol. **Known values** (DN-1 design §4, reproduced from the same file for this page, no new data):
- **T_SQP = 105.** The first SQP matrix with rho ≥ 1 is `sd-hven-sqp-CRESC50 mat_0049` (n = 106,
  rho 1.58). The next are LAKES at n = 156 (1.03, 1.003). The SQP value rests on one matrix.
- **T_IPM = 167.** The first IPM matrices with rho ≥ 1 are `sd-hven-ipm-LAKES` at n = 168 (1.04-1.11).
- Pooled over both engines: 105.

These values are therefore known before the declaration. The pre-registration still has work to
do: it fixes the rule against later re-cutting, and the adoption criterion (§5) reads rows nobody
has seen.

Why P-A over the alternatives:
1. It is the brief's literal rule on the quantity the brief's rule system already defined.
2. It reads only frozen data. No new replay leg sits between the ruling and the implementation.
3. **The adoption test (§5) guards against a threshold that is too high.** P-A's rho charges the
   sparse arm a fresh analysis on every matrix. The engine reuses the analysis, so P-A favours the
   dense arm near T. A cell where that bias matters loses on instructions and counts against
   adoption.

The risk, stated: excluding the analysis, the factor+solve ratio `rho_fs` crosses 1 much earlier
(per bin: IPM 1.05 at n 33-64, SQP 1.35 at n 65-128; `dn1-design/results.txt` [A]). One solve per
factorization understates the engine's ~28 solves per factorization (DN-1 design §1.1), where the
dense solve is 6-10× cheaper, so `rho_fs` is not the engine's ratio either. Mixed cells near T are
where P-A could cost instructions. §5's per-cell report shows them.

### 2.3 Alternatives the settler may choose instead

| | reading | value | cost |
|---|---|---|---|
| P-A (proposed) | per engine, fresh rho, DN-0 replay | 105 / 167 | none: frozen data |
| P-B | one value pooled over both engines, fresh rho | 105 / 105 | none |
| P-C | per engine, per-matrix **engine-weighted** rho: the analysis is charged only where the engine analyzed, and each solve is weighted by the RHS columns the engine solved on that factorization (DN-0 `index.tsv`; DN-0 §4's `rho_engine_weighted`, per matrix rather than per cell) | not computed; DN-0's per-cell weighted rho is < 1 on 95/95 SQP and 82/95 IPM small-dense cells (DN-0 REPORT §4) | a script over committed data; the value is unknown until run |
| P-D | re-replay the 5501 matrices through the **shipped** arm (SQP `kReport`: `dsytrf_rook` + detector; IPM `kSparseFallback`: `dsytrf`) and read fresh or weighted rho | unknown | a new replay leg, its own declaration |

P-D is the most faithful to what ships: the SQP's arm is now rook plus a detector, not DN-0's
`dsytrf` (DN-1's A6 column: rook at a median 4,943 instructions per SQP factorization and 4,848 per
solve, isolated; no `dsytrf` figure was taken on the same footing). It is the right choice if the settler
wants the threshold to describe the shipped arm rather than DN-0's. It costs a leg and a second
declaration round.

## 3. What `kAuto` selects, and where

### 3.1 The mechanism (already built at the factor layer)

`SymmetricFactor::Options::dense_factorization = kAuto` selects the dense arm for an analyzed pattern
iff n ≤ `max_dense_dimension` (`src/linear/dense_session.cpp:546-555`). `kAuto` without a threshold
≥ 1 throws at construction (`:510-515`). The decision is per pattern at `analyze()` and lasts the
pattern's life (DN-1 design §4). No factor-layer change is needed.

What DN-3 adds at the engines:
- `IpmOptions::linear` and `SqpOptions::linear` accept `kAuto` (today refused:
  `src/drivers/ipm_options.cpp:262-279`, `src/drivers/sqp_options.cpp:462-476`).
- `max_dense_dimension` is accepted only under `kAuto`, engaged ≥ 1. Empty under `kAuto` means
  the engine's library threshold (T_IPM, T_SQP) — the meaning the public doc comment already
  states ("empty uses the library's rule", `include/hven/drivers/ipm_options.h:79-82`,
  `sqp_options.h:685-688`).
- The threshold reaches every factor the policy reaches. Today only the `DensePolicy` is threaded
  (`sqp_kkt_options(threads, dense)`, `include/hven/detail/kkt/kkt_calls.h:41`; `KktFactor`,
  `BorderState`, `SsnEngine`, `QpEngine`, `IpqpEngine`, the rank-repair Gram; the IPM at
  `src/drivers/ipm_solver.cpp:199`). The implementation threads the threshold beside it, through
  one internal value so no site can drop it. The `QpEngine` fingerprint (`qp_engine.h:1759`,
  `engine_fingerprint(opts, threads, dense)`) must include it, or hot-start reuse could cross two
  thresholds.

### 3.2 Per engine, per consumer

| engine | consumer | receives `kAuto`? | n it decides on |
|---|---|---|---|
| IPM | the step KKT (`KktFactorization`, one pattern per solve) | yes | n_kkt, fixed per solve: a cell is all-dense or all-sparse |
| SQP | walk K0 / `BorderState::kkt` (`rebuild_k0`, `qp_engine.cpp:5674`) | yes | K0's order |
| SQP | walk eliminated EQP factors: `kkt`, `fresh`, the three `local`, `own`, `shifted`, `saddle_kkt`, `diag_kkt` (`qp_engine.cpp:210, 551, 1519, 1865, 3582, 4270, 4353, 4560, 5448, 6266`) | yes | n_free + rows |
| SQP | SSN `kkt_` (`ssn_engine.h:729`) | yes | the augmented KKT |
| SQP | IPQP `KktFactorization` (`ipqp_engine.cpp:719-727`) | yes | the IPQP KKT |
| SQP | rank-repair Gram (`ws_independence.cpp:935-936`) | yes | mr (working rows); mr ≤ `kRankDenseRows` already uses Eigen's eigensolver |
| SQP | warm-start predictor (`predictor.h:628`) | **no**, `kNever` always | — |
| SQP | equality basis Gram and certificate (`equality_basis.cpp:211`) | **no**, `kNever` always | — |

The SQP analyzes once per pattern (HS84: 55 patterns over 85 factorizations, DN-1 design §4), so an
SQP cell can be mixed: patterns at or below T dense, the rest sparse.

### 3.3 Singular policy per engine (I5)

- **SQP: `DenseSingularPolicy::kReport`, the DN-1d path.** `sqp_kkt_options` already sets it for
  any policy other than `kNever` (`kkt_calls.h:47-49`), so `kAuto` inherits it: rook pivoting, the
  two-stage detector, the Ruiz + σ-shift refactor (σ = 2^-30, one retry at 256σ), the platform
  retake as last resort (`src/kkt/kkt_calls.cpp:112-152`), every shifted answer ρ-gated (§7.1). The
  IPQP sets `kReport` itself (`ipqp_engine.cpp:723-725`). The rank-repair Gram keeps
  `kSparseFallback` by the DN-1c record (`ws_independence.cpp:933-936`: G + tI is SPD with the
  shift bounding its condition, and the inverse iteration solves near-singular systems on purpose).
- **IPM: `kSparseFallback` plus the DN-1b step guard.** I5 calls the sparse fallback INTERIM and
  names the DN-1d path for auto-selection. For the IPM that path does not exist: `kReport` needs a
  consumer response, and the IPM has none (DN-1 design §15 R10, §18: "The IPM keeps
  `kSparseFallback` and DN-1b's guard (IPM `kReport` registered, R10)"). Three options, **Q3**:
  (i) the IPM's `kAuto` uses `kSparseFallback` + guard, disclosed as interim (proposed);
  (ii) build an IPM `kReport` response before IPM adoption (new design and proof);
  (iii) DN-3 adopts for the SQP only and the IPM's `kAuto` stays refused.

### 3.4 Threads (I3)

I3 states the dense arm is pinned to 1 thread, because threaded `dsytrf` inertia was unreliable on
undecidable matrices (DN-1 A-thr: 9 IPM matrices differ at the default threads, all undecidable;
MSS1 mat_0000 dense-taken at 1 thread, exactly singular at the default). **On `b70e45cfb` it is not
pinned.** Every LAPACK call in the dense session runs under `MklThreadScope(opts_.threads)`
(`src/linear/dense_session.cpp:184, 285, 372, 398`), so at `common.threads = 0` MKL may thread
`dsytrf` at n ≈ 100-400.

Proposal: the dense session runs at 1 thread under every dense policy; `Options::threads` and
`set_threads()` keep configuring the platform session (fallback, retake, sparse-selected patterns).
At n ≤ 167 a threaded `dsytrf` has nothing to gain (DN-1 M: at the default threads the dense arm's
samples were 0 % on OpenMP worker threads). The pin moves `kAlways` at the default thread setting,
which is a declared event in the implementation round, and moves nothing at `MKL_NUM_THREADS=1`.
**Q4**: pin under every dense policy (proposed), or under `kAuto` only.

*Note, 2026-11-12 (owner ruling):* the Apple arm of the pin (`BLASSetThreading` around each dense
call, macOS 15+) was removed. The pin is `MklThreadScope(1)` on MKL and a no-op on Accelerate, which
runs at its default threading. Accelerate dense-arm threading is UNOBSERVED.

## 4. The measurement protocol

The full criteria are in the DECLARATION. In summary:

- **Population.** DN-0's 238 TRAIN cells: HS TRAIN (22: the 21 TRAIN members of `sets/hs-27.json`
  plus `hs071`) and small-dense CUTEst TRAIN (97), on both hven lanes. The roster is re-checked
  against hven-bench's split and holdout guard; any HOLDOUT name aborts before a cell runs (I9).
- **Arms.** One binary: the implementation head, with `kAuto` accepted and the default still
  `kNever`. The lane runs two labels, `default` (`kNever`) and `auto-default` (`kAuto`, empty
  threshold, so the library value). Only the option differs between the arms.
- **Counter = instructions** (I1). The criterion reads **solver instructions** per cell:
  `perf stat -e instructions:u` over the lane process (all threads), three rounds per arm in the
  order N A A N N A after one untimed warm-up per arm, median, multiplied by `(1 − s_init − s_proc)`.
  - `s_init` = the period-weighted share of perf-record samples whose call chain holds any frame
    matching the declared process-once init set: hven's `ensure_solver_initialized`,
    `mkl_get_max_threads`, `MKL_Read_ThreadENV_Count_Cores`, the OpenMP runtime's
    `__kmp_(do_)?(serial|middle|parallel)_initialize` and `__kmp_internal_begin`, and MKL's clock
    calibration (`dsecnd`, `mkl_serv_dsecnd`, `mkl_serv_get_clocks_frequency`). hven itself no longer
    calls `dsecnd` (`src/drivers/solver_init.cpp:26-31`, F1 `d546afd1b`); its symbols stay in the
    set so a spin MKL makes internally is still excluded.
  - `s_proc` = DN-0's B_proc: process start/exit and harness-only work, by DN-0's frozen
    `bucket_map.tsv`. DN-0 put it at 13-27 % of an SQP median cell. Left in, it dilutes the
    10 % bar.
  - Both are read on raw perf-record samples, which is why §4's next point matters.
- **Attribution through statically linked MKL (I2).** MKL is linked statically and carries no
  unwind tables. hven's own TUs are compiled with `-fno-asynchronous-unwind-tables`. So the records
  are taken on a **CFI record build**, DN-0 r1's method (`dn0-ipm-r1/tools/cfi_launcher.sh`): the
  same compile commands plus `-fasynchronous-unwind-tables`, with every executable section proven
  byte-identical to the measured binary (`code_identity.py`). An init sample with its leaf inside
  MKL or libiomp5 keeps at least the leaf and its first CFI-less caller, which is what the symbol
  set matches. What symbol matching cannot claim is reported, not subtracted:
  - the main-thread samples with a truncated chain (no hven frame) and a leaf in libiomp5, libc or
    ld.so;
  - a STOP if that share exceeds 0.01 of solver instructions on any of the four median cells;
  - a gdb call-count probe on the four median cells, both arms (DN-0 r1's
    `init_probe_gdb_count.sh`), confirming each init symbol is entered at most once per process.
    Informational.
- **perf record.** `-e instructions:u -c PERIOD --call-graph dwarf,16384 --user-callchains -m 1M`,
  base `PERIOD = max(400009, ⌊T/8000⌋) | 1` (always ≥ 400,000), repeats `R = min(40, ⌈8000·base/T⌉)`,
  repeat k at `(base·(1000+7k)/1000) | 1`. Any throttled sample voids the cell's record. The base
  is lower than DN-0's 1,000,003 because the IPM's steady state is ~2-4e6 instructions per solve
  (DN-0 REPORT §3). At DN-0's period that is a handful of samples. The sampling standard error of
  `s_init + s_proc` is reported per cell.
- **Threads and placement.** `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, `common.threads = 1`, pinned
  `taskset -c 2`. The engines' default thread setting (`common.threads = 0`, MKL variables unset) is
  read on the four median cells only (hs026, HS67 IPM; hs043, HS84 SQP; DN-0 REPORT §1),
  informational.
- **Serial (CLAUDE.md §7).** Every instruction-count leg runs serially: one cell process at a time,
  alone on the box, under `/tmp/box-build.lock`, wall cap 300 s per cell (DN-0's). Instruction
  counts are not the deterministic per-process columns §7 lets co-run, and DN-0/DN-1 took them
  serially. The counter-only identity legs (§6) reuse DN-1d's instruments and their declared
  protocol unchanged.

## 5. The adoption criterion (I1)

Per engine (**Q2**: per engine proposed; joint over both engines is the alternative), over the
engine's two families F ∈ {HS TRAIN, small-dense TRAIN}:
- per cell: **converged** = status optimal or acceptable;
- **win**: kAuto converges and kNever does not, or both converge and kAuto's solver instructions
  ≤ 0.90 × kNever's;
- **loss**: the mirror (kNever converges and kAuto does not, or both converge and kNever ≤ 0.90 ×
  kAuto);
- otherwise a **tie**. A cell that crashes, throws out of the lane, or hits the 300 s cap is not
  converged;
- the family's sign = sign(wins − losses);
- **`kAuto` becomes the engine's default iff at least one family has sign +1 and none has −1**, and
  §6's identity holds.

This is the RETAKE21 rule (M7 close §4.2; brief `:374-377`) with the counter set to instructions
(I1) and the arms set to `kNever` / `kAuto`. Applied mechanically by a committed script
(`sl/adopt_verdict.py`, planned).

Reported beside the verdict, not gating:
- per cell: both arms' status, objective (and the relative difference, flagged above 1e-8 ·
  max(1, |f|)), iterations, factorizations, raw / init / B_proc instructions, the margin to the 10 %
  boundary in sampling standard errors, and the dense counters (SQP `dense_*` fields; IPM
  `dense_residual_ratio_triggers`);
- the cells DN-1 already showed moving under `kAlways` that fall at or below T: IPM ALLINITC
  (acceptable → optimal) and SQP HS91 (objective rel 1.30e-8). Both are ties or wins under the rule,
  so they are listed for the settler by name;
- the per-cell dense share (cell all-dense / mixed / all-sparse), **if** Q6's counter is approved;
- the four median cells at the default thread setting.

Not adopted for an engine ⇒ that engine's default stays `kNever`. Whether `kAuto` still ships as an
accepted opt-in is **Q7** (proposed: yes, since it is measured and proven either way).

## 6. Byte identity above the threshold

The brief requires every cell above the threshold to stay byte-identical, so that the 27-cell replay
and the census do not move.

The argument why it should hold:
- a sparse-selected pattern takes the platform session's path, exactly as under `kNever`
  (`dense_selected`, `dense_session.cpp:546-555`);
- the SQP's `kReport` setting reaches sparse-selected patterns, but its two observable effects (the
  flag and `kDenseSingular`) are produced only by the dense arm (`symmetric_factor_mkl.cpp:809-819`;
  `numerically_singular` is set nowhere else in `src/linear/`), so `flagged()`
  (`kkt_calls.cpp:54-58`) is false on every platform outcome;
- the IPQP's `reconfigure()` under any non-`kNever` policy (`ipqp_engine.cpp:719-727`) must leave the
  platform session's configuration as the default build has it (the iparm dump leg shows it).

**Why it must be measured.** "Above the threshold" is a property of each analyzed pattern, not of
the cell. Every census and replay cell is F7 with n_kkt ≥ 2,250 (DN-1 design §14 S4), so the KKT
factors stay sparse. But the SQP's rank-repair Gram is mr × mr, and any small K0 or eliminated face
is its own pattern. If such a factor has n ≤ 105 in an F7 cell, that cell takes a dense
factorization and can move. Neither this lane nor the code can rule that out without running it.

The proof (DECLARATION I-set): at the flipped default, against the implementation head's `kNever`
default:
- I1: the 27-cell replay × walk/ssn/ipm, 0 differences ×3 (`scripts/compare_replay.py`);
- I2: the 57-cell census, every non-deadline-bound row exact (DN-1d P5's instrument and criterion);
- I3: the interior leg and the iparm dump byte-identical.

A move is a STOP to the settler, with the cell, the factor and its n. The options then are to keep
the side factor at `kNever` under `kAuto`, or to declare the mover. This page does not pre-decide.

## 7. Prerequisites and their status

### 7.1 The SQP dense solve check (I4)

I4: a dense solve check is a prerequisite before SQP auto-selection; 16 of 119 SQP cells earlier
accepted dense solves with residual ratio > 1e-5. (DN-1c's own probe, `dn1c-sqp-check/DECLARATION.txt`,
"THE MEASUREMENT", counts LARGE solves above 1e-5 in 14 cells, plus SMALL refinement-like solves in
others.) What DN-1c/DN-1d built, per consumer, read from the code at `b70e45cfb`:

| SQP dense consumer | shifted numerics possible? | answer through shifted numerics | answer through unshifted, unflagged dense numerics |
|---|---|---|---|
| walk EQP, eliminated (`solve_eqp`, `include/hven/detail/qp/eqp_solve.h:451-455`) | yes | `refine_shifted_solve` then ρ ≤ 1e-8 (`kkt_calls.h:242-280`) | **checked**: the row-residual refusal, both arms (`eqp_solve.h:134-143`, `:581-583`) |
| walk EQP, border via K0 (`rebuild_k0`, `qp_engine.cpp:5674`; `solve_bordered_eqp`) | yes | per-block ρ on the first Schur solve (`bordered_eqp.h:668`) | **checked**: the bordered residual-ratio refusal (`bordered_eqp.h:734`) |
| verdict refinement twins (`qp_engine.cpp:3456`, `:3598-3608`) | yes | ρ (border twin, eliminated twin) | the walk's own working-face keep rule |
| `own`, `shifted`, `saddle_kkt`, `diag_kkt` faces (`qp_engine.cpp:5448, 6266, 1519, 1865`) | yes | through `solve_eqp` (gated) | through `solve_eqp` (checked); the directions they then give are certified from the data (DN-1 design §18.1, `ws_independence` row) |
| SSN Newton step (`ssn_engine.cpp:583-590`) | yes | `refine_shifted_solve` then ρ | **not checked per solve**: the detector and the SSN inertia gate only |
| SSN deferred certification (`ssn_engine.cpp:920`) | yes | no solve (inertia only) | — |
| IPQP (`ipqp_engine.cpp:723-725`; solve at `:1980`) | **no**: a flagged or exactly singular read is `kPerturbed`, the tier's own remedy (`:82`, `:1707`) | — | **not checked per solve**: the detector only |
| rank-repair Gram (`ws_independence.cpp:935-936`, `:974`) | no (`kSparseFallback`, no detector) | — | **not checked**; refuses only on `perturbed_pivots`, which the dense arm reports ABSENT, so on the dense arm the refusal can never fire (`:975-977`) |
| predictor, equality basis | `kNever` | — | — |

Reading:
- **Under the criterion the settler ruled for DN-1d (C283: every answer computed through shifted
  numerics is verified against the unshifted system before it is used), coverage is complete.**
  Every consumer that can hold shifted numerics gates them. The IPQP and the Gram never hold
  them, and the no-site `factorize_checked` overload (`kkt_calls.cpp:62-80`) retakes a flagged
  factorization on the platform instead of shifting (its only caller is the predictor, `kNever`).
- **Under a per-solve residual reading of I4, three consumers are uncovered**:
  1. **SSN Newton step, unshifted**: `src/qp/ssn_engine.cpp:586-590` (the gate is a no-op unless
     `dense_shifted`);
  2. **IPQP step solves**: `src/qp/ipqp_engine.cpp:1980`;
  3. **rank-repair Gram**: `src/qp/ws_independence.cpp:935-936, 974-977` (no detector, by the DN-1c
     record).

  The sparse arm has no per-solve check at these sites either. DN-1c's probe found the
  catastrophic class (ratio > 0.1) only at KktFactor sites (MESH, MSS1, HS90), all flagged by the
  condition test, and "None in the SSN, the IPQP tiers or the Gram factor". It rejected a 1e-5
  per-solve test as unsupported for the SQP (14 % of factorizations, in cells that solve). DN-1d's
  detector inherits that class (DETECTOR_PASS: CAT 106/106).
- **Q5**: is I4 discharged by C283's criterion plus the detector (proposed), or does it require a
  per-solve check at the three sites above? If the latter, that is a blocker for SQP adoption. It
  is named here, not fixed here.

### 7.2 The IPM step guard (I3)

**Landed.** `IpmSolverImpl::dense_step_singularity_guard` (`src/drivers/ipm_solver.cpp:2137`, called
at `:3985`): a residual ratio > 1e-5 on a dense-taken step solve removes the ladder's shifts and
re-runs it singular, at most 3 attempts. The least-squares multiplier init carries the same guard
(DN-1b DECLARATION, "THE CHANGE"). Counted in `IpmResult::dense_residual_ratio_triggers`. The DN-1c
riders F1/F2 and the DN-1d rider F8 are landed with it. The IPM is eligible for `kAuto` on this
prerequisite. Its singular policy is Q3.

### 7.3 Implementation prerequisites (the next round, not this one)

| # | item | status at `b70e45cfb` |
|---|---|---|
| R1 | dense arm at 1 thread (§3.4) | **not implemented**: `dense_session.cpp:184, 285, 372, 398` |
| R2 | threshold threaded to every consumer, and into the `QpEngine` fingerprint (§3.1) | not implemented: only `DensePolicy` is threaded |
| R3 | engines accept `kAuto` and `max_dense_dimension` under it | refused today (`ipm_options.cpp:262-279`, `sqp_options.cpp:462-476`, `tests/drivers/test_limits.cpp:140-148`) |
| R4 | `BorderState`, `SsnEngine` and `QpEngine` default the policy to `kNever` (`qp_engine.h:1569-1570`, `:1756-1757`; `ssn_engine.h:727-728`) | registered (I8). Every library construction passes the engine's policy today (`qp_engine.h:1760`; `qp_engine.cpp:631, 914, 1673, 2919`; `src/drivers/sqp_solver.cpp:2145, 2209, 2214, 8969-8970, 8993, 9019, 9046`). Proposed: remove the defaults in the implementation round, so a forgotten site fails to compile, as `KktFactor` already does |
| R5 | walk border resets construct `BorderState(0, dense_policy_)`, threads 0 (`qp_engine.cpp:631, 914, 1673, 2919`) | registered (I8). Byte-identical at `kNever` today. Moot for the dense arm once R1 pins it; for sparse-selected patterns it is today's behaviour |
| R6 | inertia trust between the bands (an unflagged rook D-ratio ~1e-14 is trusted where Pardiso would perturb; DN-1 design §18 "Bands", §18.1 open item 2) | registered (I8). SQP adoption makes it a default-path exposure; reported, not gated |

## 8. Per-platform plan (I6, I7)

- **Linux/MKL**: all measurement (§4, §5) and the identity proof (§6).
- **The default, if adopted on Linux TRAIN, ships on every platform** (owner ruling 2026-11-08), on
  the condition that each platform's CI lane passes its correctness gates with the flipped default:
  - macOS/Accelerate: the full suite including the dense-arm engine tests, on Accelerate's LAPACK
    through `lapacke_shim.h` (rook, `dsycon_rook`; compiled by that lane, never run on a Mac by
    this project);
  - Windows/MKL: the full suite.

  A lane red on the flip commit blocks the flip on that platform; a lane already red on the pre-flip
  head is a STOP to the settler (the condition cannot be read).
- **UNOBSERVED, disclosed in the result**: every Apple and Windows instruction count, timing, rho
  and saving; whether Accelerate's `dsytrf_rook` pivots as MKL's does; Accelerate's thread behaviour.
  No Mac timing session (I6).
- This supersedes DN-1 design §10's "DN-3's default is platform-scoped and stays `kNever` on Apple"
  and the brief change 6 it proposed.
- Size evidence for the gate (I7): on the SQP, Pardiso's unperturbed inertia errors are 135 of 140
  at n ≥ 33 (DN-1 design §1.2), and the dense count matches the reference on 100 % of decidable
  matrices at every size (A1, DN-1 set). The proposed thresholds (105 / 167) sit inside the band
  where the dense arm is both cheaper on the replay and right on inertia. The IPM's n = 168 LAKES
  matrices, where the old e1/e2 decode miscounted, stay sparse under T_IPM = 167.

## 9. Names

Reused, no approval needed: `DensePolicy::kAuto`; `IpmLinearOptions::max_dense_dimension`,
`SqpLinearOptions::max_dense_dimension`; `SymmetricFactor::Options::max_dense_dimension`; the
factor counters `dense_factorize_count` and the rest.

Internal, listed for completeness: the two library thresholds as `detail` constants (one per engine;
spelled in the implementation round under the B8 rules), and an internal value carrying policy plus
threshold to the factor sites (R2). The public doc comments state both values.

**Proposed, owner approval needed (Q6):** a per-solve count of factorizations the dense arm took,
on `SqpCounters` and on `IpmResult` (proposed spelling `dense_factorizations`, after the existing
`factorizations`). Under `kAuto` a user cannot otherwise tell whether a solve used the dense arm at
all. DN-3's report would use it to classify cells as all-dense, mixed or all-sparse. If declined,
that classification is not reported. No criterion depends on it.

No other public name.

## 10. Open questions for the settler

- **Q1, the threshold reading.** P-A (per engine, fresh rho, DN-0's replay: 105 / 167), P-B
  (pooled: 105), P-C (engine-weighted rho per matrix), or P-D (re-replay through the shipped arm).
  *Recommendation:* P-A. No leg runs before this ruling.
- **Q2, adoption scope.** Per engine (proposed) or joint over both engines' four families.
- **Q3, the IPM's singular policy under `kAuto`.** (i) `kSparseFallback` + DN-1b guard, disclosed as
  interim (proposed); (ii) build IPM `kReport` first; (iii) SQP-only DN-3.
- **Q4, the thread pin.** 1 thread under every dense policy (proposed; moves `kAlways` at default
  threads, declared) or under `kAuto` only.
- **Q5, I4's discharge.** C283's criterion plus the detector (proposed), or a per-solve check at the
  SSN step, the IPQP solve and the rank-repair Gram first (§7.1, a named blocker).
- **Q6, the new counter.** Approve `dense_factorizations` on `SqpCounters` and `IpmResult`.
- **Q7, `kAuto` as opt-in when not adopted.** Ship it accepted with the default unchanged
  (proposed), or keep it refused.
- **Q8, a census mover from a small side factor (§6), if one occurs.** Pre-decided by nothing here.
  Raised now so the settler knows the STOP may fire.
- **Q9, the scope valve.** DN-3 sits behind valve item 1 (brief `:103-106`, Q4). This page assumes
  the schedule allows it.

---

## §DN-3b. The IPM's answer to a singular dense KKT factorization (2026-11-12)

Settler ruling on Q3 (owner-aligned): option (ii). Under the dense arm the IPM answers a singular or flagged
dense KKT factorization with its own regularization (Wächter & Biegler 2006, Algorithm IC: δ_c on a singular
Jacobian, a larger δ_w) and a DENSE refactorization, never with the sparse library. `kSparseFallback` was
interim. Base `m8` at `ccd2e951f`; code head `1101cc38b`; declaration and evidence
`docs/notes/data/2026-11-m8-dn3b/`. Linux x86-64, MKL 2026.2; Apple/Accelerate and Windows UNOBSERVED.

### 1. What the ladder sees, per case

The ladder (`factor_impl`, `src/drivers/ipm_solver.cpp:1929`) reads three things after each factorization:
`Singular()` (`:1941`, observed counts not summing to n), `Inertia()` (negatives minus m), and
`SingularitySignal()` (`:2027`, `Singular() || Inertia() < 0`), which engages δ_c once per call and latches it
for the phase. No new constant: the δ schedule is the ladder's own (`inertia_correction_initial` 1e-5,
`_increase` 8, `_decrease` 1/3, 15 attempts; δ_c = `dual_regularization(μ)` = 1e-8 μ^0.25).

| case | factor outcome under `kReport` | what the ladder reads | answer |
|---|---|---|---|
| (a) exactly singular (rook `info > 0`, a zero 2×2 det, non-finite D) | `kBackendError`, `kDenseSingular`, arm `kDense`, no numerics, `numerically_singular` | `computation_info_of` → `NumericalIssue` (`src/interior/kkt_factorization.cpp:32, 57`); the failed projection's counts (MKL: −1/−1 passed through; Accelerate: zeroed) never sum to n, so `Singular()` and `SingularitySignal()` hold | δ_c engages, δ_w climbs, the next rung refactors on the dense arm: the singular branch Pardiso's refusals already take. **No ladder code changes** |
| (b) flagged (rook D-ratio < 1e-15 and rcond < 2^-53) | `kOk`, numerics kept, observed inertia, `numerically_singular` | the observed counts, trusted | accepted if the inertia is right. Whether the system is singular in the sense that matters (the step is wrong) is decided by the step solve's residual ratio: the DN-1b guard (`dense_step_singularity_guard`, `:2148`; `kDenseResidualRatioSingular` = 1e-5, Ipopt's `residual_ratio_singular`) |
| (c) guard residual-ratio failure | — | the guard strips the ladder's shifts and re-runs `factor_impl` declared singular (`base_singular`): δ_c engaged, δ_w at max(previous, initial, 8·cumulative), ≤ 3 attempts | unchanged from DN-1b; every re-run factorization is now dense (a singular one is case (a) inside the re-run) |

Why (b) is not mapped to `Singular()`, measured: barrier scaling makes IPM KKT matrices ill-conditioned by
construction while a dense solve on them stays accurate (DN-1 design §1.4: 611 of 2669 dense-taken IPM
matrices, 517 of them decidable, have a pivot ≤ 1e-14·max|a|). On the DN-0 population under `kAlways` the
detector flags **1903 of 5831** IPM factorizations (dev probe, DECLARATION disclosures). A dev variant that read a
flag as singular (probe commit `c168f86`, never merged) turned **36 of 119** IPM cells into status/objective
movers against today's `kAlways`, 29 of them a success → `numerical_error`, and S1 against kNever fell to
80/112 (from 109/112): the flag persists under every δ_w
(the ill-conditioning is the barrier's, not the Hessian's), the ladder exhausts, and the iteration ends
"persistently singular". Ipopt's own singularity evidence is the factorization's zero pivots plus the solve's
residual ratio, not a condition estimate; this design matches that. A flag is counted
(`SymmetricFactor::Counters::dense_flagged_count`) and acts on nothing.

The least-squares multiplier estimate (`[I Jᵀ; J 0]`, `:5089`) had the same gap: an exactly singular dense
report there used to end the phase ("the initial KKT factorization failed"). It now takes that system's one
singular rule, the guard's (δ_c, refactor); a refactor that still leaves no numerics, or a solve still above the
ratio, discards the estimate (Wächter & Biegler §3.6) and the phase proceeds. Unreachable under `kNever` and
`kSparseFallback` (`arm == kDense && info == NumericalIssue` needs `kReport`).

### 2. The dense session's σ-shift does not run under the IPM

`kReport` means "report; the engine regularizes". The session never shifts on its own: the equilibrated σ-shift
(`factorize_equilibrated`) and the platform retake (`retake_on_platform`) are consumer responses that the SQP's
`factorize_checked` calls. The IPM calls neither:
- **Two regularizers would fight.** σ on D K D (+σ on primal rows, −σ on dual rows, fixed 2^-30) is itself a
  δ_w/δ_c-type perturbation, but in equilibrated coordinates and invisible to the ladder's accounting (the Hpert0
  warm start, `rho_k` decay, the δ_c latch). The ladder would then tune δ_w against a matrix it did not build.
- **A shifted factor reports `perturbed_pivots = dim`.** The IPM ladder reads only counts (`ppivs()` is unused),
  so the shifted inertia would be read as exact.
- **Shifted numerics need the C283 discipline** (refine against the unshifted K, ρ-gate). The IPM's guard measures
  the residual against `kkt_sol_.matrix()`, which carries exactly the IPM's own shifts, so it is consistent only
  while the IPM owns every shift.
- **Algorithm IC already is the singular response**, with a convergent schedule and a defined exhaustion. The SQP
  needed σ because the walk's EQP faces have no regularization ladder of their own.

Counted: `dense_shift_refactor_count` and `dense_retake_count` read 0 on every IPM row (FA leg; P4 tests).

### 3. Exhaustion

A ladder that exhausts on an exactly singular rung leaves no numerics with `info == NumericalIssue`
(`kkt_no_numerics`, `:3819`; `kkt_no_numerics_numeric`, `:4031`), which takes the existing exhausted-inertia
dispatch (the no-numerics routing round): acceptable-tier stop outside restoration, then the elastic re-centre,
then restoration entry, then `kNumericalError` with stop reason `kNone`. Never a throw (no solve runs on no
numerics), never the sparse path (the policy has no fallback). A ladder that exhausts WITH numerics (a flagged or
wrong-inertia last rung) takes the existing exhausted-with-numerics path, unchanged. Pinned by
`IpmDenseReportSeam.ALadderOfSingularReportsEndsInAStatusNeverAThrowNorTheSparsePath` and
`BothStrategies/IpmDenseReportRestorationSeam.*`; falsified by `dense-singular-hard`.

### 4. Option wiring

No public field. `IpmSolverImpl::set_qp_params` (`:199-210`) sets `dense_singular_policy = kReport` whenever
`linear.dense_factorization != kNever`, as the SQP's `sqp_kkt_options` does (`kkt_calls.h:47-49`). Under
`kNever` the policy stays at the factor layer's default (`kReport` is refused under `kNever` at construction;
falsifier `report-under-never`), so the default path is byte-identical (P2, P3, P5). `ipm_options.cpp`'s
validation needs no change: `kAlways` was already the only accepted dense policy, and `kAuto` stays refused until
DN-3 proper. The public doc comment on `IpmLinearOptions::dense_factorization` states the behaviour. A public
`IpmLinearOptions::dense_singular_policy` was considered and is not proposed: one policy per engine, and
`kSparseFallback` is retired for the IPM rather than offered.

`kSparseFallback` stays reachable for tests only, through one HVEN_TESTING seam
(`IpmDenseSparseFallbackOverride`, `include/hven/detail/drivers/ipm_dense_testing.h`; docs/testing.md "A seventh
seam"). It keeps the faithful vehicle of the hard-backend-error route and of the DN-1b/DN-1d platform-route
fixtures, which only the platform session's injector reaches.

### 5. What moves under `kAlways` (a declared event)

`kAlways` changes three ways for the IPM: the dense kernel is rook (`dsytrf_rook`/`dsytrs_rook`, kReport's), exact
singularity is answered by the ladder instead of Pardiso's perturbation, and the multiplier estimate survives a
singular report. Dev probe on the DN-0 population (238 cells), declared in advance (DECLARATION "FA MOVERS"):
- 0 sparse fallbacks (today's `kAlways`: 15, in BT8 10, HS61 2, LEWISPOL 2, LSNNODOC 1); 4 singular reports
  (BT8 1, HS61 2, LSNNODOC 1), each answered by the ladder on the dense arm.
- 4 status/objective movers against today's `kAlways`: **BT8** (optimal → optimal, f 1 + 9.5e-7 → 1 − 2.8e-15,
  11 → 40 iterations: the dense arm reads the true inertia where Pardiso's perturbation fabricated the expected
  one, and the ladder corrects it with δ_w up to 2.5e1 — Algorithm IC's answer, paid in iterations);
  **CRESC100** (acceptable → optimal; no singular report: the rook kernel's rounding moves a path DN-1d already
  showed sensitive); **LEWISPOL** (optimal → optimal, f 3.0000293 → 2.9999965; its two singular factorizations are
  now the ladder's; kNever ends max_iter, so no reference); **MESH** (diverging → numerical_error at |f| ≈ 3e28:
  unbounded under every arm, kNever diverging; the ladder exhausts with numerics, inertia off by one).
- S1 against kNever: 109/112, misses ALLINITC and CRESC100 (DN-1d's declared pair) and BT8 (above). ACOPP30
  keeps status and objective.

### 6. Deltas for DN-3 proper

- Q3 resolves to (ii): the IPM's `kAuto` inherits `kReport` with no further change (the condition is
  "policy ≠ kNever").
- The IPM adoption measurement (§4-§5) must run on this head: the rook kernel and the singular response move IPM
  `kAlways` rows (BT8's iterations especially), so DN-1's and DN-3a's IPM `kAlways` rows are not the arm.
- The §3.3 IPM bullet and §7.2's "Its singular policy is Q3" are superseded by this section.
- R6 (inertia trust between the bands) now reaches the IPM: a flagged factorization's inertia is trusted. The FA
  leg's flags with no loss beyond the declared movers are the evidence; R6 stays registered.
- No new public name. Q6's proposed `dense_factorizations` counter is unaffected; the factor counters already on
  `IpmResult::kkt_factor_counters` report singular reports, flags and the (zero) fallbacks.

---

## §DN-3. The implementation round (2026-11-12)

Rulings applied: Q1 P-A (T_SQP = 105, T_IPM = 167), Q2 per engine, Q3 settled by DN-3b, Q4 and Q5 landed in DN-3a, Q6
`dense_factorizations` approved, Q7 kAuto ships opt-in, Q8 side factors selected by their own n with an enumeration leg
before adoption. Code head `96ac1a27d`; declaration and evidence `docs/notes/data/2026-11-m8-dn3/` (`DECLARATION-r1.md`
supersedes the held r0). Linux x86-64, MKL 2026.2; Apple/Accelerate and Windows UNOBSERVED.

What the implementation answers, against §3 and §7.3:
- **The threshold reaches every factor through one place per engine** (R2): `sqp_kkt_options()` sets
  `max_dense_dimension` under kAuto, so every walk and SSN `KktFactor` and the rank repair's Gram take it without a
  per-site change; the IPQP sets it beside its policy in its one reconfigure; the IPM in `set_qp_params()`. The constants
  are `detail` (`include/hven/detail/kkt/dense_selection.h`). The design's "an internal value carrying policy plus
  threshold" reduced to this: within an engine the threshold is a function of the policy, so the policy is the value
  threaded and the factory derives the threshold; no site can drop it.
- **`max_dense_dimension` stays reserved** at the engines (the round's ruling: no public knob). §3.1's "accepted only
  under kAuto" is NOT implemented; it is proposed for owner approval in the round's report.
- **The hot-handle key** folds kAuto's threshold (kNever's and kAlways' keys unchanged), so two engines that could select
  different arms for one pattern never share a K0.
- **R4 done**: `BorderState`, `SsnEngine`, `QpEngine` have no defaults; `IpqpEngine` and `rank_repair` keep theirs
  (registered; every library call passes the policy).
- **The rank-repair Gram cannot go dense under kAuto**: its factor path runs only past `kRankDenseRows` = 1000 working
  rows (fewer use Eigen's eigensolver), far above 105. Its count exists for kAlways.
- **`dense_factorizations`** counts the factor layer's `dense_factorize_count + dense_singular_count` deltas
  (factorizations whose arm was dense, shifted refactors included; under kReport no dense attempt goes to the platform,
  so a positive count is exactly "a pattern of order <= T was factorized"). SQP: `DenseTallyScope` (every walk/SSN
  factorization site) plus the IPQP and the Gram; IPM: a monotone lifetime reading like `work.factorizations`.
- R5 (border resets at threads 0) and R6 (inertia trust between the bands) stay registered.

---

## §DN-3 flip. The default event (2026-11-13)

Owner ruling 2026-11-13: DN-3's adoption tallies (`docs/notes/data/2026-11-m8-dn3/RESULTS.txt`; IPM HS 22/0/0, SD
64/11/22; SQP HS 21/0/1, SD 81/0/16) are applied and the S-A4 STOP records are accepted as immaterial. So
`IpmLinearOptions::dense_factorization` and `SqpLinearOptions::dense_factorization` default to `kAuto` (thresholds
unchanged: T_IPM = 167, T_SQP = 105). No public `max_dense_dimension` (owner: not now; it stays reserved).

- One consequence by construction: `IpmLinearOptions::conditional_reproducibility = true` is refused at the default
  (it requires `kNever`, `src/drivers/ipm_options.cpp`), so a CNR caller now sets both.
- Test fixtures that read the platform backend on purpose (fault injectors, Pardiso's own pivot count, the backend
  option sweep), and scenario fixtures whose routes were derived on the sparse arm, set `kNever` explicitly; corpus
  pins of the shipped default are re-derived. Every re-pin and every kNever fixture is listed, with its kAuto
  reading, in `docs/notes/data/2026-11-m8-dn3/DECLARATION-r4.md`, which also carries the I set (r0 §2) on the flip
  commit.
- Registered with the flip, not fixed: T_IPM = 167 is a tuning candidate (the 11 IPM small-dense losses, solver A/N
  1.13-2.83, are mid-size KKT systems near T); the kAuto fixture outcomes r4 lists (robustness readings worse than the
  sparse arm's on a few constructed SQP fixtures); F3n50's `border_refine` not being stable across processes on the
  dense arm.
