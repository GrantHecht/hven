# M6 W1 T10 — IPQP acceptance evidence (A4, Q4, A8, A13, A14, A10)

Produced 2026-09-01 on branch `m6`. Provenance and the declared serial
protocol: `PROVENANCE.txt` (read it first — it governs every number here).

The arm is `bench/ipqp_e1_arm.h` (implementation) + `bench/ipqp_e1_arm.cpp`
(CLI), built as `hven_sqp_ipqp_e1_arm`.

**The A4 gate is executable** (fix round 1, item A), and **registered only
behind `-DHVEN_A4_GATE=ON`** (fix round 2, item A): CI's bare `ctest
--timeout 300` runs with no such flag, so the default configure never
registers a RED entry into it.

```
cmake -S . -B build -DHVEN_BUILD_BENCH=ON -DHVEN_A4_GATE=ON  # opt in, once
ctest -j4 -LE a4_gate -E ProbeBudgetBoundsAFailingProposal    # the default run
ctest -L a4_gate --output-on-failure                          # the A4 gate, solo
```

It runs all 29 cells at both sizes under the shipped options, scores E1's four
criteria, and **exits nonzero on any RED criterion — so it FAILS today**, which
is the point: A4's verdict is a test result, not prose. Its fix-round-1 run
printed exactly the verdict below (14/29, 5/29, 1/27, blow-up PASS) and exited
1; its rows agree with the committed `a4-gate-shipped-defaults.csv` on **1711
of 1711** non-timing values, the eight anchor Rule-A cells excepted, which now
carry the `-1` not-scored sentinel instead of 0. Wall 114 s (Release, solo,
informational). The three `IpqpAcceptanceA4.*` tests in
`tests/sqp/test_ipqp_acceptance.cpp` are NOT that gate and no longer read like
it: they pin the taxonomy against E1's sweep scripts, smoke the arm's mechanics
on a 200-node cell that is not in the taxonomy, and recompute this artifact's
own scoring from the committed gate CSV.

---

## 1. Regeneration identity — the cells really are E1's cells

E1's generator (`docs/notes/data/2026-08-m6-e1-acquisition/generator/
e1_generate.cpp`) cannot be rebuilt in this repository: it compiles against
the origin project's headers, and `run_sweep.sh` deleted every `.qp`/`.sol`
dump it wrote. So identity is established against what the artifact *did*
keep — the per-cell quantities its own verifier printed at `%.6e`, and the
contiguous variant's recorded LICQ offsets.

`compare_regen_to_e1.py` performs the comparison mechanically:

```
$ python3 compare_regen_to_e1.py regen-certificate-fpcontract-off.csv
cells compared: 27  reference cells found: 27  values compared: 225  differences: 0
PASS: 27 constructed cells, every recorded value reproduced.
```

It **exits nonzero** on a difference, a missing reference, a duplicate id, or a
row count other than 27, so a truncated certificate cannot pass silently
(fix round 1, item H). On `regen-certificate.csv` it exits 1 with 43
differences — see the qualification below.

**225 of 225 values, 0 differences**, across all 27 constructed cells
(18 scattered + 9 contiguous): stationarity residual and its scale, minimum
inactive relative slack, minimum active multiplier, the 10th-percentile
margin, minimum box slack, the LICQ `min|D|`/`max|D|`, and — for the nine
contiguous cells — the drawn-and-LICQ-shifted `active_offset`
(6571, 4393, 2196, 7088, 8010, 12414, 9176, 1974, 12102, exactly
`variant_contiguous_layout.csv`). The offsets alone certify that the
`mt19937_64` stream is aligned draw for draw.

### How to reproduce both certificates

```
# the shipped uniform regime
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHVEN_BUILD_BENCH=ON
cmake --build build --target hven_sqp_ipqp_e1_arm
build/bench/hven_sqp_ipqp_e1_arm --regen --out regen-certificate.csv

# the MATCHED-ARITHMETIC arm (this option exists only for this certificate)
cmake -S . -B build-gen -DCMAKE_BUILD_TYPE=Release -DHVEN_BUILD_BENCH=ON \
      -DHVEN_BENCH_E1_GENERATOR_ARITHMETIC=ON
cmake --build build-gen --target hven_sqp_ipqp_e1_arm
build-gen/bench/hven_sqp_ipqp_e1_arm --regen --out regen-certificate-fpcontract-off.csv
```

`HVEN_BENCH_E1_GENERATOR_ARITHMETIC` (`bench/CMakeLists.txt`) puts **this one
target** back on E1's generator's own arithmetic — `-ffp-contract=off
-fno-fast-math -march=x86-64 -mtune=generic`, i.e. the plain `-O3` of
`build_e1.sh` — defaults OFF, and is documented there as never valid for a
measurement CSV other than the certificate.

**A correction found by doing this** (fix round 1): round 1 described its
matched-arithmetic file as "an arm TU compiled with `-ffp-contract=off`". That
was imprecise — the hand invocation also dropped `-march=native` and
`-ffast-math`, and **all three matter**. Rebuilding with `-ffp-contract=off`
alone, on top of the uniform regime, leaves **14 `licq_d_min` cells still
differing** (measured); Eigen's `SimplicialLDLT` responds to the ISA and
reassociation flags as well as to contraction. Only the full generator regime
reproduces 225/225. The file keeps its round-1 name, which is therefore
narrower than what it records.

Both certificates are now regenerated from a committed tree (`61b59fc`) and
stamp a real commit; the shipped-regime file's data is byte-for-byte what
round 1 recorded, so nothing moved but the stamp (fix round 1, items B and D).

### The qualification, stated rather than buried

`regen-certificate.csv` is the same arm under the **shipped uniform flag
regime** (CLAUDE.md §7: `-march=native -ffast-math -fno-finite-math-only`,
i.e. FMA contraction on). Against the artifact it differs in **43 of 225
values, and only in two columns**:

| column | differs | why |
|---|---|---|
| `stat_inf` | 27/27 | the verifier's own cancellation residual (~4e-16 on a scale of ~2); an FMA changes its last bits by construction |
| `licq_d_min` | 16/27 | smallest `\|D\|` of an `LDL^T` of `J J'`, 1e-8..1e-10; last-digit on 13 of them, ~4x on the three `af30` contiguous cells |
| everything else | 0/225 | exact |

**What this does and does not establish** (corrected in fix round 1; the
round-1 wording, "nothing that describes a cell moves", was too strong):

- **Established.** At matched arithmetic the cells are bit-identical to E1's:
  all 225 recorded values reproduce to every printed digit. The *taxonomy*,
  the *ground truth* (`x*`, the active set, the multipliers) and the *chosen
  offsets* are integer- or draw-determined and are identical under **both**
  builds, as are `stat_scale`, the slack and multiplier extrema, the margin
  decile, the box slack and `licq_d_max`.
- **NOT established.** Full cell-byte identity under the shipped regime. The
  constructed `g` is assembled by the same summation the differing `stat_inf`
  measures, so `g` moves by ~1 ulp — and **`g` is a QP-defining coefficient,
  not a diagnostic**. The A4 gate, both counterfactuals and the Q4 sweep all
  ran on those shipped-regime cells. No gate leg was run at
  `-ffp-contract=off`, so **it is untested whether the four criteria score
  identically under the two builds**; nothing here should be read as claiming
  they do. The `licq_d_min` spread on the three `af30` contiguous cells
  (9.61e-10 → 5.56e-9) is likewise larger than one ulp; the LICQ certificate
  holds either way, and the offset it selects is identical.

The generator compiled at plain `-O3`; hven compiles under one uniform regime
(CLAUDE.md §7) and this arm is not exempted from it. So: **the recipe is
bit-exact at matched arithmetic, and the shipped regime perturbs `g` at the
last ulp** — a §7 flag-regime consequence, not a recipe drift.

---

## 2. A4 — the gate, at the shipped defaults

`a4-gate-shipped-defaults.csv`. All 29 cells, both sizes (`nx = 2e4` and
`1e5`), `IpqpOptions{}` verbatim, asserted rather than spot-checked.

**VERDICT: RED on THREE of E1's four pre-registered criteria.** (Round 1 said
"all four" here, contradicting its own table; corrected in fix round 1.)

| criterion | result |
|---|---|
| every cell converges | **14 / 29** (14 `kOptimal`; 14 `kMaxIter` at the 60-iteration hard cap; 1 `kNumericalError`) |
| `< 40` iterations | **5 / 29** |
| exact active-set recovery (E1's own Rule A) | **1 / 27 constructed** (the 2 anchors are NOT SCORED — see the correction below) |
| no monotone blow-up across the active fraction | **holds** — see below |

Iterations run 10–60; the two equality-only anchors converge in 10 and 11.

The blow-up criterion is the one that passes, and it is read **across the
active-fraction axis at a fixed size, layout and margin** — that is what "no
monotone blow-up across the active-fraction sweep" asks. At `nx = 1e5`
scattered, iterations at (1 %, 10 %, 30 %) are:

| margin class | 1 % | 10 % | 30 % | monotone up? |
|---|---|---|---|---|
| 1e-2 | 35 | 56 | 49 | no |
| 1e-4 | 51 | 47 | 58 | no |
| 1e-6 | 60 | 60 | 60 | no (all at the cap) |

None of the nine (size, layout, margin) tracks is strictly increasing in the
active fraction. The cap, not the fraction, is what binds. (Round 1 printed
these triples grouped by fraction and with wrong values; corrected in fix
round 1 — both reviewers caught it.)

### Dated correction (fix round 1, 2026-09-01): the anchors were never scored

`solve()` skipped the Rule-A block for the two anchors — they carry no
constructed ground truth — leaving `rule_a_missed`/`rule_a_false_positive` at
0, which the round-1 scoring then counted as "recovered exactly". **The
committed CSVs' bytes are unchanged** (they are the measurement); the arm now
writes `-1` as an explicit not-scored sentinel, and the corrected counts are
over CONSTRUCTED cells only:

| configuration | round-1 (wrong) | corrected |
|---|---|---|
| shipped | 3 / 29 | **1 / 27** (`e1_f7_n20000_af10_m1e-4` alone) |
| `ipqp_init_mu = 1e-2` | 11 / 29 | **9 / 27** |
| `1e-2`, `slack = 1` | 25 / 29 | **23 / 27** |

`IpqpAcceptanceA4.TheCommittedGateCsvScoresRedOnThreeOfFourAtTheShippedDefaults`
recomputes all four criteria from this CSV's raw columns and pins them, so the
scoring above is executable rather than prose. The `rule_b` column is E1's
dual-side rule, carried for comparability with the oracle artifact's own
`active_set_size_found_dual` column; no criterion is scored on it.

**The brief's proxy caveat therefore does NOT retire.** PIQP solved these
same cells in 9–18 iterations, recovering the active set exactly on 28 of 29
(it missed one row on `e1_f7_n20000_af01_m1e-2`); the shipped tier does not.

### Where the failure lives — two shipped defaults, measured separately

| configuration | converges | `< 40` iters | Rule-A exact | total iters |
|---|---|---|---|---|
| **shipped** (`ipqp_init_mu = 0.1`, `ipqp_converge_slack = 1e2`) | 14/29 | 5/29 | 1/27 | 1471 |
| `ipqp_init_mu = 1e-2`, slack `1e2` | **28/29** | **28/29** | 9/27 | 690 |
| `ipqp_init_mu = 1e-2`, slack `1` | **28/29** | **28/29** | **23/27** | 737 |

(`a4-counterfactual-mu1e-2.csv`, `a4-counterfactual-mu1e-2-slack1.csv`.)

The decomposition is clean and each half is a *different* default:

- **`ipqp_init_mu` owns the iteration/convergence half.** At 1e-2 the gate
  goes from 5/29 to 28/29 under 40 iterations, on the same cells, same cap.
- **`ipqp_converge_slack` owns the recovery half.** The tier stops at
  `slack × QP tolerance = 1e-7` relative, which leaves active-row slacks
  above Rule A's `1e-8 × row_scale` threshold — so rows that ARE active are
  scored missed. At slack `1` (target the QP tolerance itself) Rule-A
  recovery goes 9/27 → 23/27, at a cost of 47 iterations across 29 cells.

At (`1e-2`, `1`) exactly **one** cell fails anything:
`e1_f7_n20000_af30_m1e-6` — `nx = 1e5`, 30 % active, the tightest margin
class — which reaches the 60-iteration cap. Two further cells miss exactly
one row each, which is the same isolated single miss E1's own artifact
recorded for PIQP on `e1_f7_n20000_af01_m1e-2`.

Only the `ipqp_init_mu` half is sanctioned by this task (spec §10 Q4). The
`ipqp_converge_slack` finding is reported, not acted on — see §7 below.

---

## 3. Q4 — the measured `ipqp_init_mu` sweep

`q4-mu-sweep.csv`. Four levels `{1e-3, 1e-2, 1e-1, 1}` over **37 cells in
four families**: E1's 29, HS10/24/33 under `SqpDriver` at `QpMode::kIpm`,
three indefinite QP fixtures tier-direct, and the two `path_warm` corpus
cells. Decision on **counters**; wall informational.

**All three non-shipped budget values, disclosed** (fix round 1, item D — round
1 named only the first, and the CSV header recorded none of them; the producer
now stamps all five option values into every mode's header):

| option | shipped | this sweep |
|---|---|---|
| `ipqp_hard_iter_cap` | 60 | **200** |
| `ipqp_max_iter` | 0 (size-derived sentinel) | **200** |
| `ipqp_max_factorizations` | 0 (sentinel = 3 x effective cap = 180) | **800** (4 x) |

The cap was raised deliberately: at 60, 35 of the 148 rows would have
truncated and the comparison would measure the cap rather than the level.
**Did the extra headroom decide the winner? No.** Checked against the
committed CSV: no cell at any level reached 600 factorizations; the maximum
over all 148 rows is **225** — `hs33` at `mu = 1e-2`, 152 iterations. That is
the *only* row anywhere in the sweep that exceeds the shipped 180 sentinel,
so the 4x headroom changed one cell's fate and not the ranking. It must be
stated the other way too: **that cell WOULD have hit the shipped sentinel**,
so a sweep re-run at shipped budgets will not reproduce `hs33`'s 214-iteration
HS total exactly. The 1052-vs-2362 gap between 1e-2 and 0.1 is two orders
larger than anything one truncated cell can explain.

| `ipqp_init_mu` | E1 iters | HS iters | indef iters | corpus iters | **total iters** | **total facts** | optimal |
|---|---|---|---|---|---|---|---|
| 1e-3 | 795 | 158 | 96 | 110 | 1159 | 1283 | 37/37 |
| **1e-2** | **715** | 214 | **55** | **68** | **1052** | **1191** | **37/37** |
| 1e-1 *(shipped placeholder)* | 1914 | 246 | 96 | 106 | 2362 | 2511 | 36/37 |
| 1 | 1757 | 213 | 56 | 151 | 2177 | 2277 | 35/37 |

**WINNER: `ipqp_init_mu = 1e-2`.** It is best on total iterations, best on
total factorizations, and one of the two levels that solves every cell. It
wins three of the four families outright; HS prefers 1e-3 (158 vs 214), the
only family that does, and 1e-3 is second overall. The shipped 0.1 is the
**worst** level on iterations — 2.2× the winner — and fails one cell.

The three non-optimal rows in the whole sweep are all at the two high levels:
`e1_f7_n4000_af01_m1e-6` (`kNumericalError`, mu = 1),
`e1_f7_n20000_af01_m1e-2` (`kNumericalError` at mu = 0.1 **and** at mu = 1).

**The 0.1 placeholder does not survive its own sweep.** Adopting the measured
default is a §7 DECLARED break; it is priced but NOT taken in this task —
see §7.

---

## 4. A8 — determinism

Two independent full A4 sweeps at the shipped defaults
(`a4-gate-shipped-defaults.csv` vs `...-repeat.csv`), separate processes,
same protocol:

```
A8: cells compared: 29  columns compared: 59  values: 1711  differences: 0
```

Every non-timing column — status, both active-set rules, the ratio-rule face
verdict, all four residual columns and all 39 `IpqpCounters` fields — is
bit-identical across the two runs. **A8 GREEN.**

---

## 5. A13 / A14 — the envelope legs

**A13 as written is not runnable in this repository, and this is a
pre-existing finding, not a new one.** Spec §8.3 asks for a *PSIOPT-envelope*
re-read; the psiopt engine is not in hven (CLAUDE.md §1: neither engine has
migrated yet — the SQP driver has, the interior-point engine has not), and
`docs/notes/2026-08-15-m3-phase-c-plan.md` §12 **Q6a** already ruled on
exactly this: "Plan §6 clause 3 names a psiopt-envelope sweep vehicle that
does not exist here … substitute the SQP vehicles that exist and say so in
the gate package." The 30.15× figure it would be re-read against is a
tycho_sqp P7 number and has no in-tree source.

**Substituted, under the Q6a precedent and declared as a substitution**: the
hven-side walk-vs-tier wall envelope at `nx = 1e5`, one occasion, both
engines. This is a *contemporary in-tree envelope*, not a PSIOPT re-read, and
no number here may be compared against the 30.15x figure. If the settler wants
the literal A13, it needs the tycho lane and a built psiopt at the pinned
commit.

`envelope-a13-substitute-five-cells.csv` carries the rows behind the table
below (fix round 1, item F: round 1 quoted these numbers in prose with no
committed CSV). Produced by `hven_sqp_corpus --engine walk|ipm` on the five
`f7_n20000_*` cells at `nx = 1e5` — **SOLO, serialized, `taskset -c 2`,
`MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, one process at a time, nothing else on
the machine**, so this wall IS asserting:

| cell (`nx = 1e5`) | walk s | tier s | ratio | walk facts | tier facts |
|---|---|---|---|---|---|
| `f7_n20000_bound_activity` | 0.644 | 1.667 | 2.59 | 1 | 11 |
| `f7_n20000_bound_corrupted` | 0.612 | 1.716 | 2.80 | 1 | 13 |
| `f7_n20000_bound_neutral` | 0.646 | 1.812 | 2.81 | 1 | 13 |
| `f7_n20000_bound_physics` | 0.651 | 1.669 | 2.56 | 1 | 11 |
| `f7_n20000_bound_warm` | 0.611 | 1.635 | 2.67 | 1 | 12 |
| **total** | **3.165** | **8.499** | **2.69** | 5 | 60 |

**Read this with its caveat.** The walk finishes every one of these cells in
ONE factorization — its active set is essentially right at the start — so this
population is not the one the tier exists to help, and 2.69x is the cost of
the tier on cells that never needed it, not a statement about acquisition. The
tier's own hard population is the A4 taxonomy (§2), where the walk has no
measured arm at all. `envelope-a13-substitute-solo-1thread.csv` carries a
second, single-cell reading of the same shape (walk 0.646 s, tier 1.747 s).

**A14** (LTO-on / threads-on context leg) is **INFORMATIONAL and is not a
gate** — spec §8.4 says so, and nothing in this artifact depends on it.
`envelope-a14-threads8-informational.csv`: the same cell at
`MKL_NUM_THREADS=8` runs walk 0.388 s / tier 0.836 s against the pinned
single-thread 0.646 / 1.747 — the ratio is unchanged at 2.15 vs 2.70, i.e.
both engines take the same multi-thread benefit and no conclusion here turns
on it. **The LTO-on leg was NOT run**: `HVEN_LINK_TIME_OPT` is reachable only
through `BUILD_HVEN_WHEEL` in `cmake/hven_compile_options.cmake:122`, a
registered-buggy path, and A14 is by its own definition never a gate — so it
is recorded as NOT RUN rather than run badly.

The W0.1b scatter-inlining CARRY (plan §6 R6) is **not re-examined here** and
is carried forward: the tier's own bench is this arm, and this arm's finding
is that the tier's defaults, not its inlining, are what bound its cost. See
`.superpowers/w1-t10-report.md`.

---

## 6. A10 — Accelerate, UNOBSERVED

No Apple hardware ran any leg of this task. Recorded as **UNOBSERVED**, never
zero-filled (CLAUDE.md §6, absolute):

| leg | Linux (MKL) | Apple (Accelerate) |
|---|---|---|
| A4 gate, 29 cells, both sizes | measured, this artifact | **UNOBSERVED** |
| A8 two-run determinism | 1711 values, 0 diffs | **UNOBSERVED** |
| Q4 `ipqp_init_mu` sweep, 37 cells | measured, this artifact | **UNOBSERVED** |
| A9 symbolic-analysis pin | T9 | **UNOBSERVED** |
| A2 real-hop subproblem | T9 | **UNOBSERVED** |
| A13 envelope | substituted (§5) | **UNOBSERVED** |

The standing mechanism for clearing these is the O12 Mac session
(`docs/notes/2026-08-22-o12-mac-session-runsheet.md`).

---

## 7. What this artifact does NOT do, and why

1. **It does not move `ipqp_init_mu`.** The sweep is decisive (§3) and the
   task text authorises adopting the winner, but the change was applied,
   measured and reverted: it turns **17 tests red**, and at least one of them
   is not a numeric pin. `IpqpCertificationTest.TheFinalReadVerifiesDirections
   OffAWeaklyActiveBoundInsteadOfMasking` — T4b's gate-8 fixture — stops
   producing a weakly active bound at all (`sigma` 0.01 vs the required
   `> 1`; `x(1) - lower(1) = 5e-4` vs `weak_scale = 3.54e-4`), because the
   fixture's mechanism is a function of where the barrier stops. Re-deriving
   it is re-DESIGNING it, in T4b/T5's territory, on a gate that already
   carries an owner-pending residual (plan §7 note (r) item 1, "RESIDUAL C1").
   `IpqpTrace.EscapeEventCarriesTheFullEvidenceBlockVerbatim` likewise changes
   which escape reason fires (1 → 2), which is a census question. The full
   list of 17 is in `.superpowers/w1-t10-report.md`.
2. **It does not move `ipqp_converge_slack`.** Q4 authorises one sweep, over
   `ipqp_init_mu`. The slack finding (§2) is measured and reported so the
   settler can decide whether A4's recovery arm is a second declared break or
   a statement about what the tier's stopping rule is *for*.
3. **It does not retire the proxy caveat.** A4 is red at the shipped
   defaults (§2). Under (`1e-2`, `1`) it is one cell short of green, and that
   cell is the hardest corner of the taxonomy.

---

## 8. A6 — replay and suite

BASE `a142a13` (a detached worktree) vs HEAD `7378325`, two independent
Release builds, same flags. `libhven.a` **byte-identical** between them
(`cmp`) — no `include/` or `src/` file changed — so nothing the library
computes could have moved, and the replay is a check on that rather than a
hope.

The 27-cell U0 replay set read verbatim from `.scratch/w05/cells.txt`. THREE
arms against both binaries: 6 runs, each **SOLO** — one at a time, alone on
the machine, `taskset -c 2`, `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`. The SMT
sibling of core 2 is core 10; `/proc/stat`'s `cpu10` row read before and after
the whole sequence: busy **0.31 %** across the window, i.e. measured idle.
Binary stamp `73783258a030` on the HEAD side.

```
=== base-walk vs head-walk === cells compared: 27  columns compared: 75  differences: 0
=== base-ssn  vs head-ssn  === cells compared: 27  columns compared: 75  differences: 0
=== base-ipm  vs head-ipm  === cells compared: 27  columns compared: 75  differences: 0
=== COMMITTED ipm baseline vs head-ipm === cells compared: 27  columns compared: 75  differences: 0
```

All four at 0 differences on all 75 asserted columns (`wall_s` excluded as
informational). The fourth is the DECLARED schema-76 kIpm baseline
`bench/baselines/2026-09-01-t9-ipm/ipm_baseline.csv`, re-run independently.

**Suite**, split per the standing rule (`ctest -j4 -E
ProbeBudgetBoundsAFailingProposal`, then that test alone):

| build | main set | solo | total |
|---|---|---|---|
| Debug (`build-debug`) | 2174 / 2174 (363.01 s) | passed, 733.28 s | **2175 / 2175** |
| Release | 2174 / 2174 (40.32 s) | passed, 12.87 s | **2175 / 2175** |

BASE carried 2172; this task adds the two `IpqpAcceptanceA4.*` gate tests.
Pre-existing non-failures in both: 1 skipped (`FailByDesignControl.*`), 2
disabled (`EqpRefinementAb.*`).
