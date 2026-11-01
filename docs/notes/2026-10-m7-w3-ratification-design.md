# M7 W3 — the globalization ratification ladder: the design page and its pre-registered protocol

Date: 2026-10-28. Author: the hven W3 lane. Status: a DESIGN PAGE, **committed before any
measurement**. Branch `w3` off `m7` `bb512d69`; every `file:line` below was located at that commit.

Authority: the M7 brief `docs/notes/2026-09-m7-brief.md:303-318` (W3) — "What EXISTS is ratified:
R1 (`ssn_sigma_rule = kResidualArmed`), R4 (the Farkas-gated infeasibility exit, with the ordered
1e-4 re-derivation of the false-positive axis), R5 (`ssn_certify_from_face`, its default flip
preconditioned on the weak-active-indefinite fixture) — each against its OFF state on TRAIN; a
lever that cannot show a win stays at the shipped default. The τ item: `kFunnelTauBar = 100` is a
`constexpr`; the re-derivation on an ill-conditioned population runs through the existing sweep
probe (patched-source recompile), and becomes a setting only if the sweep says the published value
is wrong for this population. R3, R7, R9 are NOT built in M7 (H-11)"; the roadmap's M7 exit
criterion (`docs/notes/2026-08-26-roadmap-m6-m8-draft.md`: "every shipped heuristic has a measured
win over its own OFF state on the suite's TRAIN split … the holdout stays untouched"); the A4
decision rule (`hven-bench/docs/w2-a4-decision-rule.md` and `scripts/w2_a4_decide.py`, unchanged
since `cbd26ed`); settler rulings C129 (A2-F4: **a lost fixture cell vetoes a default move**), C133
(the toolchain event) and C140 (the A3 continuation). Precedent pages: A2, A3, A7
(`docs/notes/2026-10-m7-w2-a{2,3,7}-*.md`). The ladder's research record is the SQP lane's Phase-7
results note and its Task-6b Phase-B data (cited below as **P7** and **P7-B**); the `kFunnelTauBar`
record is the same lane's elastic-τ tuning note §4 (cited as **TAU**). Their criteria are restated
here; nothing in this page depends on reading them.

## 0. The answer in one paragraph

Every lever W3 ratifies already exists, ships at its OFF state, and is reachable only at
`qp_mode = kSsn` — except `kFunnelTauBar`, which every mode reads. So W3 adds **no library code**.
The one code change is in the bench: `hven_sqp_corpus --hs` today silently ignores the SSN
measurement levers the corpus arms accept (`bench/bench_corpus.cpp:2080-2120` builds a bare
`SqpOptions` with only `qp_mode` set), and the 1e-4 re-derivation's ordered "HS arm" needs them, so
`--hs` gains lever forwarding and ten trailing SSN columns (§3.1). The two `constexpr` constants a
measurement must move — `kSsnFarkasResidualTol` (R4's 1e-4 point) and `kFunnelTauBar` (the τ item)
— are moved by **patched-source recompiles of scratch commits that never merge** (§3.2), the
established technique (`bench/tau_bar_sweep_probe.cpp:28-55`, `bench/ssn_safeguard_probe.cpp:52-120`).
TRAIN runs under the A4 rule verbatim, per family: R1/R4/R5 each as `sqp-ssn` plus one field
against `D = sqp-ssn`; τ as the `default` label at a patched pin against `D = default` at the
measured head. A lever is RECOMMENDED only if it WINS a family, loses none, passes every
veto-class fixture clause (C129), and is deterministic (§5.6). **R5 cannot be recommended in this
round whatever TRAIN says**: its flip carries two preconditions (the weak-active-indefinite fixture
and the §9.1 `factorize_count` pin of `docs/notes/2026-09-m6-w5-t6-ownership.md:885-912`) and
neither exists at `bb512d69` (§3.4). **No default moves in this round** in any case: a move is the
settler's declared event.

## 1. Where the levers are, located

| lever | declared (default) | read | notes |
|---|---|---|---|
| R1 `SqpOptions::ssn_sigma_rule` | `include/hven/drivers/sqp_solver_types.h:703`, `= SsnSigmaRule::kLadder`; alphabet `:61-75` | forwarded `src/drivers/sqp_solver.cpp:8533` onto `SsnOptions::sigma_rule` (`include/hven/detail/qp/ssn_engine.h:531`); sizing law `ssn_engine.h:208-217` (`kSsnLmSigmaC = 1.0`) | ARM value `kResidualArmed`; inert until the ladder arms (`sqp_solver_types.h:65-67`) |
| R4 `SqpOptions::ssn_infeasibility_rule` | `sqp_solver_types.h:709`, `= SsnInfeasibilityRule::kSymptoms`; alphabet `:89-99` | forwarded `sqp_solver.cpp:8535` onto `SsnOptions::infeasibility_rule` (`ssn_engine.h:551`); gate `src/qp/ssn_engine.cpp:186-197`, standing route `:303-350`, exhaustion route `:758-774` | ARM value `kFarkasGated`; can only make the exit MORE reluctant (`ssn_engine.h:544-551`) |
| R4's operative tolerance `detail::kSsnFarkasResidualTol` | `ssn_engine.h:241`, `inline constexpr double = 1e-6` (gap tol `:242` = 1e-8) | read only under `kFarkasGated` (`ssn_engine.cpp:192-193`) | the 1e-4 re-derivation's point |
| R5 `SqpOptions::ssn_certify_from_face` | `sqp_solver_types.h:696`, `= false` | `sqp_solver.cpp:8532` onto `SsnOptions::defer_certification` (`ssn_engine.h:516`); FORCED OFF on the IPQP→SSN warm-grade route `sqp_solver.cpp:8648` | the saving is one factorization per accepted refinement, pinned at `tests/sqp/test_sqp_solver.cpp:8764` |
| τ `detail::kFunnelTauBar` | `include/hven/detail/globalization/sqp/globalization.h:88`, `inline constexpr double = 100.0` | its ONLY reader is `FunnelStrategy::reset`, `src/globalization/sqp/funnel.cpp:25`: `width_ = max(kFunnelTauBar, kFunnelKappaBar * h0)` (`kFunnelKappaBar = 1.25`, `globalization.h:91`) | read by every `qp_mode`, by the main loop's funnel and by the restoration sub-solve's own funnel (QC-14) |
| the mode gate | `SqpOptions::qp_mode = QpMode::kWalk`, `sqp_solver_types.h:603` | — | R1/R4/R5 are inert at kWalk (`sqp_solver_types.h:700-709`), so the shipped configuration reads none of them |
| harness label `sqp-ssn` | `hven-bench/lanes/hven/hven_bench_cell.cpp:588-589` | `qp_mode = kSsn` and nothing else (NOT `column_scaling = kAuto`, which `default` carries, `:578-583`) | `D` for R1/R4/R5 |
| the `--hs` path | `bench/bench_corpus.cpp:2675-2770` (arg checks), `:2080-2140` (`run_hs_cell`) | builds `SqpOptions opts; opts.qp_mode = mode;` and nothing else (`:2088-2089`, `:2103-2104`) | refuses corpus flags and hidden levers by name but NOT the SSN measurement levers, which it then ignores |

**What each default IS today: OFF.** `kLadder`, `kSymptoms`, `false` — the shipped iteration bit for
bit (`sqp_solver_types.h:57-59`, "every default below is the shipped iteration bit for bit"); the
three were ratified OPT-IN on 2026-08-08 (P7 §6; `docs/notes/2026-08-26-m6-register-sqp-lane-submission.md:80-87`).
`kFunnelTauBar = 100` is KLV Table 1's value.

## 2. The record

* **R1** (P7 §6.2): `kResidualArmed` ADOPTED as a bounded-downside OPTION; its benefit is confined
  to the probe's family-E ill-conditioning and reads ≈ null under combined scaling; corpus cost +5
  factorizations on one already-failing cell, 44/48 cells byte-identical; one untuned point
  (c = 1). `kResidualAlways` REJECTED. Carry: the (c, exponent) sensitivity "to the first population
  that exhibits the geometry" (P7 §11.6 item 3).
* **R4** (P7 §6.4): ADOPTED on the FALSE-POSITIVE axis — 295 → 0 false `kInfeasible` on the probe's
  three badly-scaled feasible populations; recall moved the wrong way (56.5 % → 55.2 %); the corpus
  evidence is one budget-pinned cell; the `<b, y> < 0` conjunct is an unkilled mutant (M11) kept on
  Farkas' lemma. **The 1e-4 re-derivation was ORDERED before any default change**: P7-B's nine-point
  tolerance sweep (probe only, one seed, 10 000 QPs per population) found the false-positive
  elimination insensitive across ten decades and the recall loss zero at 1e-4; "nothing at 1e-4 has
  been run on the corpus or HS arms".
* **R5** (P7 §6.1): ADOPTED opt-in; −1 factorization per accepted refinement (HS battery −10.68 %,
  corpus −66 exactly), certificate outcomes identical on every shipped corpus. The one reachable
  divergence is **indefinite H plus a weakly active row**, where R5 certifies a point the shipped
  path escapes `kIndefinite`. "The weak-active indefinite fixture is a registered carry AND a
  PRECONDITION for any future default flip." hven added a SECOND precondition at M6 W5 T6
  (`docs/notes/2026-09-m6-w5-t6-ownership.md:885-912`, ledger `docs/notes/2026-08-m6-ledger.md:3338-3344`):
  the deferral path has no cumulative observer, so a `SymmetricFactor::factorize_count` pin on the
  accepted-deferred path must exist before the flip.
* **τ** (TAU §4): `kFunnelTauBar` swept over {1, 10, 100, 1e3, 1e5} on the `hs_sweeps.h` cold arm
  (KD and SOC off) through `bench/tau_bar_sweep_probe.cpp`: every major count identical over
  {10 … 1e5}; τ̄ = 1 cost HS26 +9 majors (absent SOC). Ruled: keep 100. The population was the
  six well-conditioned sweep problems; no ill-conditioned population was ever swept.
  **A naming fact the reader needs:** P7 §11.5's own "τ re-derivation on an ill-conditioned
  population" (owner ruling D4) concerns a DIFFERENT τ — the SSN uncertain band's
  `SsnOptions::uncertain_tol = detail::kSsnUncertainEnter = 0.1` (`ssn_engine.h:112-120`, `:489`), a
  runtime option. The brief names `kFunnelTauBar`, and this page measures `kFunnelTauBar`; the band
  τ is registered as W3-R3 (§6), not measured.

## 3. The decisions this page takes

### 3.1 The one code change: `--hs` carries the SSN levers (bench only)

The 1e-4 re-derivation's ordered "HS arm" and every HS fixture clause below need an HS instrument
that can run an SSN lever. `hven_sqp_corpus --hs` cannot: it parses the five SSN measurement flags
(`--ssn-prox-carry`, `--ssn-certify-from-face`, `--ssn-sigma-rule`, `--ssn-hint-rule`,
`--ssn-infeasibility-rule`) and drops them. The change, bench-only:

* `run_hs_cell` applies the same `EngineLevers` the corpus arms apply (both the warm-up and the
  timed solves), and the HS provenance stamps every non-default lever on the corpus provenance's
  own `# lever:` lines;
* the HS CSV appends TEN trailing columns, after `ev_ssn_warm_grade`: `ssn_iters, ssn_escapes,
  esc_budget, esc_singular, esc_no_contraction, esc_infeasible_suspect, esc_indefinite,
  ssn_prox_updates, ssn_refinements, ssn_refine_refused` (the `SqpCounters::ssn` fields of the same
  names, `include/hven/core/solver_counters.h:601-729`);
* nothing in `include/` or `src/` changes: **`git diff bb512d69 <head> -- include src` is empty**,
  which is the library-identity proof (§5.1 G0), stronger than a P-SYM.

This is a switch a measurement needs and that does not exist (brief, method item 2). Its OFF
identity is §5.1's G-H1; its non-vacuity is G-H2.

### 3.2 Patched-source recompiles for the two constants

Neither constant becomes a field to be measured. Each measured point is a SCRATCH COMMIT on a local
branch that is never merged and never pushed: the measured head plus ONE line, committed with the
data as a patch file.

| scratch | the one line | built for |
|---|---|---|
| `w3-scratch-farkas1e-4` | `ssn_engine.h:241` `kSsnFarkasResidualTol = 1e-4` | the R4 re-derivation's probe, corpus and HS arms (§5.3) |
| `w3-scratch-tau10` | `globalization.h:88` `kFunnelTauBar = 10.0` | τ arm T10 (in-tree clauses and TRAIN pin) |
| `w3-scratch-tau1e3` | `globalization.h:88` `kFunnelTauBar = 1000.0` | τ arm T1e3 (in-tree clauses and TRAIN pin) |

Each is built as a whole tree (CMake, Release, the head's configure line), so every TU that
includes the header sees the patched value — no partial-link argument is needed. The τ probe's own
five-point reproduction (τ̄ ∈ {1, 10, 100, 1e3, 1e5}) uses the probe's documented recipe
(`tau_bar_sweep_probe.cpp:37-55`, compile the probe and a patched `funnel.cpp` against the head's
`libhven.a`) with the flag block of `ssn_safeguard_probe.cpp:86-99` (ABI and numerics flags); its
**self-test** is the recipe built with the copy UNPATCHED, which must print the in-tree probe's
output byte for byte.

### 3.3 The τ population

TAU swept a well-conditioned population; the brief asks for an ill-conditioned one. The
ill-conditioned NLPs this box has are TRAIN's: the `cutest` family carries the rank-deficient
network cells whose equality Jacobians E1b certified as numerically rank-deficient (8 cells, ledger
`docs/notes/2026-09-m7-ledger.md:929`), the TAX and MSS cells, and the ACOP cells; `hs` carries the
badly scaled HS problems. TRAIN is also the M7 instrument the roadmap names for this ladder. So:

* **the population is TRAIN, both families**, reached by the probe's technique (the patched-source
  recompile, §3.2) applied to the harness pin rather than to the probe binary;
* **the existing sweep probe runs too**, at its own population, as the record's reproduction on the
  new toolchain (descriptive, and one veto clause, §5.4).

This reading of "through the existing sweep probe" is the page's own; if the settler meant the
probe binary alone, the τ result below is reported as TRAIN's and the probe's separately, so either
reading can be taken from the record (ruling Q2, §6).

**Arms:** T10 (τ̄ = 10) and T1e3 (τ̄ = 1000): one decade either side of the published value, the
smallest move TAU did not already price (TAU priced 1 and found 1e5 inert on its population). Only
two arms because each costs two full TRAIN runs on both families.

### 3.4 R5's preconditions are not built here

Neither precondition exists at `bb512d69`: no test drives the SSN kernel at a weakly active row of
an indefinite subproblem with `defer_certification` on, and no test reads `factorize_count` on the
accepted-deferred path (searched: `tests/` has `defer_certification = true` only in
`tests/sqp/test_ssn_engine.cpp:3165-3263`, none weak-active; the walk's
`QpEngineIndefinite.WeaklyActiveBoundIsNotCertifiedOptimal`, `tests/sqp/test_qp_engine_indefinite.cpp:2214`,
is the walk's, not the SSN kernel's). Building either is not a switch a measurement needs (brief,
method item 2), and the first is a constructed-fixture design task of its own. So R5 is MEASURED on
TRAIN and on the in-tree instruments like R1 and R4, and its recommendation clause F-c (§5.2) is
**pre-registered to FAIL** while the preconditions are absent. A TRAIN WIN for R5 is reported as
such and is the input to W3-R1 (§6), not a recommendation.

### 3.5 What stays out

R3, R7, R9 (H-11: not built). R2 (P7 §6.3: REJECT ratified). `ssn_hint_rule` and `ssn_prox_carry`
(not in W3's list). The SSN band τ (W3-R3). R1's (c, exponent) sensitivity (W3-R4, measured only if
TRAIN shows the geometry).

## 4. Toolchain (C133), binding on every leg

The box moved to `intel-oneapi-mkl` 2026.2.0 on 2026-10-27; `clang++` 22.1.8 (Fedora 22.1.8-4.fc44).
The new-toolchain baseline is `docs/notes/data/2026-10-m7-toolchain-2026-10-27/` (U0
`replay/head-{walk,ssn,ipm}.csv`, E1 `e1/e1-off.csv`). Every comparison in this page is between
binaries built in THIS round on THIS toolchain, or against that baseline record. No old-toolchain
number is used as a same-run comparison; RETAKE19 (hven-bench `b5423f6`, MKL 2026.2.0-881, pin
`6a2a58c7`) is new-toolchain and is used only descriptively (C-2). Every artifact is stamped with
the compiler and the MKL directory its build resolved (read from its `CMakeCache.txt`).

## 5. The measurement protocol — PRE-REGISTERED

Nothing below is edited once a measured row exists. A criterion found ill-posed is reported as
ill-posed and THAT ITEM STOPS (brief, method item 1); it is not amended, and the other items
continue only where they do not depend on it. Box terms: every leg its own `flock
/tmp/box-build.lock` hold, `pgrep -a hven_bench_cell` as a separate command first,
PGREP_EMPTY / FOREGROUND_START / FOREGROUND_END markers, `set -o pipefail`, no `timeout`, one solver
at a time, `MKL_NUM_THREADS=1`, ctest at most `-j4` under `setsid`, cell legs under 600 s; counters
only, wall informational; Accelerate UNOBSERVED. HOLDOUT is never run.

**Arms.** OFF (every lever at its default); **R1** = OFF + `ssn_sigma_rule = kResidualArmed`; **R4** =
OFF + `ssn_infeasibility_rule = kFarkasGated` (at the shipped 1e-6); **R5** = OFF +
`ssn_certify_from_face = true`; **R4@1e-4** = R4 on the `w3-scratch-farkas1e-4` build; **T10**, **T1e3**
= OFF on the τ scratch builds. In-tree SSN instruments run every R arm at `--engine ssn`; the τ arms
run at `--engine walk` (the shipped mode) on the in-tree instruments.

### 5.1 Gates (the switch's OFF proof and the builds)

* **G0** library identity: `git diff bb512d69 <measured head> -- include src` is EMPTY.
* **G1** U0: `hven_sqp_corpus --engine {walk,ssn,ipm}` over
  `docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt` at the head, `scripts/compare_replay.py`
  (exact) against the toolchain record's `replay/head-*.csv`: **0 / 0 / 0**.
* **G2** E1 OFF: `hven_sqp_ipqp_e1_arm --gate` at the head vs the toolchain record's `e1/e1-off.csv`,
  every column the RECORD carries except `wall_s`: **0 differences**. (The head's E1 CSV has columns
  the record predates — A2's and A3's counters, landed after `a869e7f3` — and those are not compared.)
* **G3** suites: Release full suite at the head green (ctest `-j4`); the Debug build of the bench
  targets compiles (the bench is "built in both configurations", `bench/CMakeLists.txt`).
* **G-H1** the switch's OFF identity: `--hs --engine walk` and `--hs --engine ssn` at default levers,
  head vs a Release build of `bb512d69`'s `hven_sqp_corpus` built in this round: on the base's 28
  columns, EXCLUDING the five wall columns (`wall_median_s, wall_min_s, wall_max_s, spread_pct,
  median_se_pct`), **0 differences on 27 rows each**. The ten new columns have no base counterpart and
  are not compared.
* **G-H2** the switch's non-vacuity (the mutant "levers parsed but not applied" — the base binary's
  own behaviour): under `--hs --engine ssn --ssn-certify-from-face`, at least one row's
  `factorizations` differs from the OFF row's. Predicted: more than 20 rows, each by exactly that
  row's OFF `ssn_refinements` (the in-tree claim of `test_sqp_solver.cpp:8764-8822`).
* **G-S** each scratch build's self-check: its patched line is the only `git diff` against the
  measured head. For `farkas1e-4` additionally: its OFF rows on corpus-44 and HS-27 equal the head's
  OFF rows on every column but the wall columns (the constant is read only under `kFarkasGated`).
  The τ probe recipe's unpatched self-test (§3.2) prints the in-tree probe's output byte for byte.

### 5.2 The in-tree instruments and the R fixture clauses — VETO-CLASS (C129)

Instruments, each at `--engine ssn`, each arm in the same build and session as its OFF:

* **corpus-44**: `hven_sqp_corpus --cells <docs/notes/data/2026-10-m7-w3/corpus44-cells.txt>`: the
  44 census cells whose committed SSN row (`bench/baselines/2026-08-09-task6b-resweep/ssn_resweep.csv`)
  took under 300 s (the cut is half the 600 s leg bound; the list is committed with this page, the 13
  excluded cells named in its header). It does NOT contain P7's two corpus suspects
  (`f7_n2000_path_neutral` 852 s, `f7_n5000_path_warm` 2 121 s on that record), and this page says
  so rather than run a leg over the bound.
* **HS-27**: `hven_sqp_corpus --hs` (the 27 problems of `tests/sqp/support/hs_problems.h`), default
  `--repeat 1 --hs-warmup 1`.

| clause | statement (vs OFF, same build) | R1 | R4 | R5 |
|---|---|---|---|---|
| **F-a** corpus | no corpus-44 cell whose `status` is `Optimal` (the corpus CSV's spelling) at OFF has any other status under the arm | PASS | PASS | PASS |
| **F-b** HS | no HS-27 problem whose `status` is `optimal` (the HS CSV's spelling, `to_string(SolveStatus)`) at OFF has any other status under the arm | PASS | PASS | PASS |
| **F-c** R5's preconditions | the weak-active-indefinite SSN fixture AND the §9.1 accepted-deferred `factorize_count` pin exist in-tree and pass | — | — | **FAIL** (not built, §3.4) |

DESCRIPTIVE beside them (no veto): per arm Σ `factorizations` on each instrument; per cell the
escape census (`esc_*`), `ssn_prox_updates`, `ssn_refinements`. Predicted: R1 identical to OFF on
every row where OFF's `ssn_prox_updates` is 0 (inert until the ladder arms), elsewhere free;
R4 identical to OFF on every row where OFF's `esc_infeasible_suspect` is 0 (the gate is reached only
by an armed symptom pair, and a refused pair falls through to the routes OFF would also reach later
— so a row with no OFF suspect exit is unchanged), elsewhere free; R5 identical to OFF on every
column except `factorizations`, which drops by exactly OFF's `ssn_refinements` per row (HS-27 and
corpus-44 alike).

### 5.3 The R4 1e-4 re-derivation — ordered; its own clauses

Three instruments, R4 (1e-6, head) against R4@1e-4 (scratch), the OFF symptoms arm as the control:

* **probe**: `hven_sqp_ssn_safeguard_probe lever-farkas 10000 20260807` (P7-B's count and seed), at the
  head and at `farkas1e-4`; plus `lever-farkas-m11 10000 20260807` at both (P7's unkilled mutant,
  re-read at the new tolerance: DESCRIPTIVE).
* **corpus-44** and **HS-27** at `--ssn-infeasibility-rule farkas`, head and `farkas1e-4`.

**1e-4 is RE-DERIVED SAFE on the false-positive axis iff all of:**

* **E-a** probe: false `kInfeasible` = 0 on each of the three badly-scaled feasible populations at
  1e-4;
* **E-b** probe: recall at 1e-4 ≥ recall at 1e-6;
* **E-c** corpus-44 and HS-27: no cell whose status is the instrument's optimal spelling
  (`Optimal` / `optimal`) under R4 at 1e-6 has another status under R4@1e-4.

Predicted: E-a PASS (0 / 0 / 0), E-b PASS (1e-4 recall = the symptoms arm's, P7-B), E-c PASS;
descriptively, `esc_infeasible_suspect` 0 on every corpus-44 row under all three arms (the record's
two suspects are excluded cells) and on every HS-27 row. **What it decides:** only which tolerance a
future default would carry. It flips nothing; a TRAIN arm at 1e-4 is not run here (W3-R2).

### 5.4 The τ fixture clauses — VETO-CLASS (C129)

Instruments at `--engine walk`, each τ arm against the head (τ̄ = 100), same session:

| clause | statement | T10 | T1e3 |
|---|---|---|---|
| **T-a** U0 walk | no U0 cell (`u0-replay-cells.txt`) whose `status` is `Optimal` at the head has another status at the arm | PASS | PASS |
| **T-b** HS-27 walk | no HS-27 problem whose `status` is `optimal` at the head has another status at the arm | PASS | PASS |
| **T-c** the probe | no `hs_sweeps` problem whose `all_optimal` is 1 at τ̄ = 100 reads 0 at the arm's τ̄ | PASS | PASS |

DESCRIPTIVE: the probe's full five-point table (τ̄ ∈ {1, 10, 100, 1e3, 1e5}) against TAU §4.4's
— predicted: majors identical over {10, 100, 1e3, 1e5}; τ̄ = 1 moves HS26 and/or HS40 and fails no
problem. The hven probe has evolved since TAU, so a different baseline at 100 is a finding about
the probe's population, not a failure.

### 5.5 TRAIN — the A4 rule verbatim, per family

`hven-bench/docs/w2-a4-decision-rule.md` §2 and `scripts/w2_a4_decide.py`, unmodified, run A scored,
the arm's `D` placed in the tree's `default` slot exactly as A3 did. Scratch clones of `hven-bench`
at `b5423f6` (local commits only, never pushed; CUTEst SIFs and the decoded instances symlinked
read-only from the harness):

| clone | pin | lane edit | labels run |
|---|---|---|---|
| `bench` | the measured head | three labels, each `sqp-ssn` plus ONE field | `sqp-ssn` (D for R), `sqp-ssn-sigma-armed`, `sqp-ssn-farkas`, `sqp-ssn-certify-face`, `default` (D for τ) |
| `bench-base` | `bb512d69` | none | `sqp-ssn` (C-1) |
| `bench-tau10` | `w3-scratch-tau10` | none | `default` (arm T10) |
| `bench-tau1e3` | `w3-scratch-tau1e3` | none | `default` (arm T1e3) |

Each R label is proved one-field by the lane binary's own option dumps (exactly one line differs
from `sqp-ssn`'s). A τ arm's options dump equals `default`'s byte for byte (τ̄ is not an option);
the arm is proved by its pin stamp (`hven.pin.sha` in every CSV header) and its scratch commit's
one-line diff. Every label runs TWICE (A, B) on TRAIN only, serially, one cell per leg, through
`scripts/w2_a4_sweep.sh` → `runners/run_serial.py`, the sets' own ceilings (`hs` 600 s, `cutest`
300 s), the lane's 300-iteration budget. **Determinism** (A4 rule §4): `N / N / 0` per label and
family. **HOLDOUT is never run.**

**Controls.** **C-1 (gate)**: `bench-base`'s `sqp-ssn` run A reproduces `bench`'s `sqp-ssn` run A
cell for cell on `outcome`, and on `iterations`, `factorizations`, `objective` and
`side.major_iters` wherever both rows carry them (a cell flipping `dnf_wall` either way within a
few seconds of its ceiling is a wall event, reported, not a difference). C-1 failing STOPS the R
decisions (their measured head would not be the base's library). **C-2 (descriptive)**: `bench`'s
`default` run A on `cutest` vs RETAKE19's `run-A-default/cutest` (`hven-bench
runs/2026-10-m7-w2-retake19/`), same toolchain, pin `6a2a58c7`: differences reported as code
movement since that pin, never as a gate.

**Predictions (the script's verdicts):**

| label vs D | `hs` | `cutest` |
|---|---|---|
| `sqp-ssn-sigma-armed` vs `sqp-ssn` | TIE | TIE |
| `sqp-ssn-farkas` vs `sqp-ssn` | TIE | TIE or lose (cost) |
| `sqp-ssn-certify-face` vs `sqp-ssn` | **WIN** | **WIN** |
| T10 `default` vs `default` | TIE or lose | TIE or lose |
| T1e3 `default` vs `default` | TIE | TIE |

### 5.6 What decides

**An R arm `X` is RECOMMENDED as the default iff all of:**

1. its TRAIN verdict is WIN on at least one family and `lose` on none (TIE or no-decision allowed);
2. it passes every VETO-CLASS clause of §5.2 that applies to it (F-a, F-b; and for R5, F-c);
3. its two TRAIN runs pass determinism `N / N / 0` on both families;
4. C-1 passes.

For R4 the reading additionally states §5.3's outcome: if 1e-4 is re-derived safe, a recommended R4
carries "at 1e-6 as measured; 1e-4 re-derived safe, its own TRAIN arm W3-R2"; if not, "at 1e-6".

**τ: the published value is WRONG for this population iff** some arm `T` ∈ {T10, T1e3} meets the
same four conditions with §5.4's clauses in place of §5.2's (T-a, T-b, T-c; C-1 replaced by the τ
arm's own pin proof). Then — and only then — `kFunnelTauBar` becomes a setting,
`SqpOptions::funnel_tau_bar` at the published default 100.0, validated finite and > 0, dumped, and
proved identity at its default (G0's replacement: U0 exact 0/0/0, E1 OFF 0 differences, P-SYM, and
mutants on the read and the validation), its DEFAULT NOT MOVED in this round. Otherwise the
`constexpr` stands and no code lands, in those words.

If two arms of one item qualify, the reading names both with their `m(X)` and recommends neither
(the settler's call). **No default moves in this round** in any case.

**Predicted from §5.2-§5.5:** R1 not recommended (TIE/TIE); R4 not recommended (no WIN); R5 WIN on
both families but **not recommended (F-c)**; τ: neither arm qualifies; the `constexpr` stands.

### 5.7 Self-consistency, checked before commit

* No clause asserts a counter is zero at OFF. The R4 prediction "identical where OFF's
  `esc_infeasible_suspect` is 0" is DESCRIPTIVE, not a clause; OFF there is a different rule
  (`kSymptoms`), not a disabled one (lesson A3-R4).
* G-H1 compares only columns the base binary has (lesson A3 G2b).
* F-c is a precondition clause, not a measurement, and is declared to FAIL in advance; §5.6's
  prediction for R5 already reads it.
* The τ arms' D is `default`, not `sqp-ssn`: τ̄ is read at every mode, and `default` is the shipped
  configuration. The R arms' D is `sqp-ssn`: R1/R4/R5 are inert at `default`'s kWalk.
* C-1 compares two pins whose library sources are identical (G0); it is a control on the harness
  and the build, which is what A3's C-1 also was at its core.

## 6. Out of scope, registered here

* **W3-R1** — R5's two preconditions (§3.4): the weak-active-indefinite SSN fixture (indefinite H,
  a weakly active row, the shipped verification escaping `kIndefinite` while the face certificate
  would pass) and the §9.1 accepted-deferred `factorize_count` pin. Owner: whoever takes R5's flip.
* **W3-R2** — R4 at 1e-4 on TRAIN, if §5.3 re-derives 1e-4 safe and R4 at 1e-6 WINS; otherwise moot.
* **W3-R3** — the SSN uncertain band's τ (`uncertain_tol`, owner ruling D4, P7 §11.5): a runtime
  option, its own re-derivation, not this page's τ. **Ruling Q1**: which τ the brief meant.
* **W3-R4** — R1's (c, exponent) sensitivity, carried by P7 §11.6 to "the first population that
  exhibits the geometry"; measured only if TRAIN shows R1 armed on some cell.
* **Ruling Q2** — §3.3's reading of "through the existing sweep probe".

## 7. What is here

This page and `docs/notes/data/2026-10-m7-w3/corpus44-cells.txt` (the corpus-44 list, part of the
pre-registration). The bench switch, the scratch commits' patches, the measurements and the README
follow on `w3`, data in `docs/notes/data/2026-10-m7-w3/`.
