# M7 W2 A2 — the complementarity-derived `mu_0`: the measurements

The two A2 toggles `IpqpOptions::ipqp_mu0_rule` (`kFixed` / `kComplementarity`) and
`IpqpOptions::ipqp_mu_residual_floor` (A1-R1), built per the design page
`docs/notes/2026-10-m7-w2-a2-mu0-design.md` and measured per its PRE-REGISTERED section 6. The
page landed at `0000a52f` before any measured row existed; no criterion was edited after.

## Provenance

* hven: branch `w2-a2` (off `m7` `f424c8db`). Pre-registration `0000a52f`; implementation
  `3e0c6afc` (library + tests), the E1-arm flags `f0e9c596`, `e924bbe2` (two tests
  added from the mutation run). **Every measurement leg and both suites ran at `e924bbe2`**
  (`out/suites/chain-final.txt`: `HEAD e924bbe2… DIRTY=0`). Release and Debug, the uniform flag
  regime.
* Toolchain: clang 22.1.8 (Fedora 22.1.8-4.fc44); Intel oneAPI MKL. Hardware: AMD Ryzen 7
  5800X3D (8 physical cores). Date: 2026-10-23.
* Terms (CLAUDE.md §7/§8): every leg its own `flock /tmp/box-build.lock` hold, `pgrep -a
  hven_bench_cell` as a separate prior command, one solver process at a time,
  `MKL_NUM_THREADS=1`, `OMP_NUM_THREADS=1`, no `timeout`. Counters only: no wall-clock figure is
  quoted; `wall_s` columns are informational. Accelerate: UNOBSERVED (no Mac leg was run).
* HOLDOUT was never run.

## What is here

* `scripts/` — every driver as it ran: `build.sh`, `leg.sh`, `suite.sh`, `chain_suites.sh`,
  `chain_build_suites.sh`, `chain_meas.sh`, `chain_final.sh` (suites + measurement chain),
  `mutants.py` / `run_mutants.sh`, `score_meas.py` (re-derives `out/meas/scored.txt` from the
  CSVs), `psym.sh` / `psym_classify.py`, `train_chain.sh` / `train_close.py` (TRAIN), and
  `hven-bench-scratch-clone.diff` (the two local edits to the scratch clone of hven-bench).
* `out/meas/` — G1 (`u0-head-*.csv`, `u0-exact-*.txt`), G2 and the E1 arms (`e1-{off,m0,mf,m0mf}.csv`),
  the T10 families (`fam-*.csv`), and `scored.txt`. The family instrument writes only the CSV, so there are no `fam-*.log` transcripts.
* `out/psym/` — the P-SYM compares (both configs) and `classification.txt`.
* `out/mutants/`, `out/suites/`, `logs/` — transcripts.
* `out/train/` — the TRAIN tree, determinism compares, decisions and controls (section TRAIN).

## The gates (page §6.1)

| gate | result |
|---|---|
| **G1** U0 three-arm replay, both fields OFF, exact vs `docs/notes/data/2026-10-m7-scw-gates/gates/replay/` | walk / ssn / ipm **27 × 75, 0 / 0 / 0** |
| **G2** E1 `--gate` at OFF vs `a4-gate-mu1e-2.csv` (65 columns, `wall_s` excluded) | **29 × 65, 0 differences** |
| **G3** suites | Release **3220/3220**, Debug **3219/3219** (long test excluded) + the long test alone **1/1** |
| **G3** P-SYM `f424c8db` vs `e924bbe2`, same build path, both configs | exit 1 (expected: a public options field); classified **Release 65 differing = 10 source-changed + 55 includers of a changed header, 0 unexplained; Debug 66 = 10 + 56, 0 unexplained**; negative control 107 / 106 includers byte-identical; one added object (the new test TU) |
| **G4** fire counters | 0 on every OFF row (E1 29 cells, families 8 cells); under M0 `derived + fallbacks = 29` on E1 = one per cold start; under MF the two `ipqp_mu0_*` are 0 |
| **G5** mutants | **18 of 20 killed at round 1, 19 of 20 after the riders** (`out/mutants/`). M5 (strict-positivity check dropped) SURVIVES as an EQUIVALENT mutant: after the `1.5 · min` shift every multiplier is `>= 0.5 |min z| > 0` or the band test already refuses `mu = 0`. M7 (the ladder's high-water mark not restored) survived the round-1 fixtures, and the original argument for it was WRONG: at a box half-width `d < 0.1` the unit-scaled cold `Sigma = 1e-2/d^2` exceeds 1, so the restore IS load-bearing. The review's fixture (`scalar_box_qp(-50, 1)`, box `[-0.01, 0.01]`, `g *= 1e4`) now kills M7 (see `riders.md`). Round 1 left M8 and M14 alive too; `e924bbe2` added the fixtures that kill them |

## The E1 population (page §6.2) — predicted → measured

OFF in the same build: 28/29 converge, RED on `e1_f7_n20000_af30_m1e-6` alone; end-to-end exact
recovery 12/27; tier-contract 13/27; tier + polish factorizations 746.

| clause | M0 predicted → measured | MF predicted → measured |
|---|---|---|
| **E1-a** no new RED | PASS → **PASS** | PASS → **PASS** (29/29 converge, all under 40) |
| **E1-b** Σ tier+polish factorizations ≤ 746 | FAIL → **FAIL** (775: +29, one estimate per cell, none recouped) | PASS → **PASS** (656) |
| **E1-c** e2e ≥ 12 and tier-contract ≥ 13 | PASS → **PASS** (12, 13; no cell's reading moved) | PASS → **FAIL** (e2e **8**, tier-contract **12**) |
| **E1-d** the RED cell optimal inside 60 | FAIL → **FAIL** (`max_iter`, 60) | PASS → **PASS** (`optimal`, 19 iterations) |

**What M0 did on E1: nothing but pay.** `ipqp_mu0_derived = 0`, `ipqp_mu0_fallbacks = 29`: on
every cell the derived `mu_0` was out of `[1e-12, 1e-2]`, the start fell back to `kFixed`, and the
only trace of the rule is the extra factorization. The trajectories are otherwise OFF's (no
cell's end-to-end reading moved).

**What MF did on E1: it fixed A1-R1 and broke the face.** The RED cell converges in 19 iterations
instead of hitting the 60 cap, and the population's cost falls 746 → 656. But 15 cells' end-to-end
readings move, almost all for the worse (`out/meas/scored.txt`): the floor stops the tier at a
complementarity two to three decades higher (e.g. `n20000_af30_m1e-4` res_comp 2.4e-10 → 1.2e-8),
and at that barrier level the ratio classifier over-commits — exactly the A1 page's §2 mechanism
(`d^2 < kappa * mu_stop`). OFF's collapse to the `1e-12` floor was, on this population, what made
the tight-margin faces readable. One cell improves (`n20000_af01_m1e-6` 129 → 3 missed); the RED
cell's reading changes kind (178 missed / 5124 false → 1630 missed / 0 false); the other thirteen
get worse.

## T10's other families (page §6.3) — predicted → measured

| clause | M0 | MF |
|---|---|---|
| **T-a** no OFF-optimal cell lost | PASS → **PASS** | PASS → **FAIL**: `two_negative_eigenvalue_row` `optimal` (27 it) → `max_iter` (60) |
| **T-b** Σ HS `ipqp_factorizations` ≤ OFF's 183 | PASS → **FAIL** (191: every HS cold start fell back, +1 each) | PASS → **PASS** (172) |

Under MF the two `path_warm` corpus cells fall 37 → 14 and 31 → 15 iterations and HS 33 80 → 61;
HS 10 rises 25 → 41. (Counters per cell: `scored.txt`, FAMILIES.)

## TRAIN (page §6.4)

**How it ran.** A scratch `git clone` of `hven-bench` at `40bb726` (RETAKE18's close) with two
local commits, never pushed (`scripts/hven-bench-scratch-clone.diff`): `cmake/hven_pin.cmake`
moved to hven `e924bbe2` (`w2-a2`), and the lane gained the three labels `sqp-ipm-mu0c`,
`sqp-ipm-mufloor`, `sqp-ipm-mu1e-3`, each `sqp-ipm` plus ONE field — proved from the lane binary's
own dumps (`out/train/dumps/*-vs-sqp-ipm.diff`: exactly one line each). The CUTEst instances and
their decode are the harness's own files, linked read-only. The pin was built by the clone's
`scripts/build_hven_pin.sh` (its `HVEN_PIN.txt` stamps `e924bbe2`), the harness by its own
configure. Cells ran through the harness's documented runner (`scripts/w2_a4_sweep.sh` →
`runners/run_serial.py`, TRAIN split, the sets' own ceilings: `hs` 600 s, `cutest` 300 s; the
lane's 300-iteration budget), **one cell per leg**, `pgrep -a hven_bench_cell` in its own command
first and the clone's `scripts/leg.sh` gates around the lock: **352 legs, every one
`PGREP_EMPTY`, `PGREP_NO_UNCOVERED_CELL` and `IN_LOCK_PGREP_EMPTY`** (`out/train/leg-logs/`,
`chain-summary.txt`). 344 legs exit 0; the 8 exit-1 legs are `hs025` under every label and run —
the runner's REFERENCE FAILURE on the battery's documented trap (32.835 vs the published 0), the
same row every label returns. A first attempt was aborted within a minute (the clone lacked the
`.SIF` links; no cutest row was produced) and restarted from empty; nothing from it is used.

**Determinism (§4 of the A4 rule): every label, both families, run A vs run B — `N / N / 0`**
(`out/train/determinism-*.txt`: 21/21/0 on `hs`, 23/23/0 on `cutest`).

**Controls.** `default` and `sqp-qn` on `cutest`, once, against RETAKE18's chain rows: **0 of 23
cells differ** on outcome, iterations, factorizations or objective for either label
(`out/train/control-*-vs-retake18.txt`) — the A2 fields are unread under `kWalk`.

**The decision** — `scripts/w2_a4_decide.py` from the clone, unmodified, run A scored, with `D =
sqp-ipm` sitting in the tree's `default` slot as the page pre-registered (`out/train/decisions.txt`):

| family | label | optimal | dnf_wall | error | \|B\| | median cost (D) | verdict | predicted |
|---|---|---:|---:|---:|---:|---:|---|---|
| `hs` | `sqp-ipm` (D) | 21 | 0 | 0 | 21 | 10.6667 | — | — |
| `hs` | `sqp-ipm-mu0c` | 21 | 0 | 0 | 21 | 11 | **lose: cost** | WIN |
| `hs` | `sqp-ipm-mufloor` | 21 | 0 | 0 | 21 | **9.22222** | **WIN** | TIE |
| `hs` | `sqp-ipm-mu1e-3` | 21 | 0 | 0 | 21 | 10.5556 | **WIN** | WIN |
| `cutest` | `sqp-ipm` (D) | 16 | 5 | 2 | 16 | 207.696 | — | — |
| `cutest` | `sqp-ipm-mu0c` | 16 | 5 | 2 | 16 | 207.747 | **lose: cost** | ≤ TIE |
| `cutest` | `sqp-ipm-mufloor` | 16 | 6 | 1 | 16 | **207.332** | **WIN** | TIE |

The script's own pick: `sqp-ipm-mufloor` on both families. Per cell (`out/train/per-cell-table.md`):

* **`mu0c` is the E1 result again.** No outcome moves on either family, and every cell whose
  cost moves at all is dearer (the estimate's factorization, then the `kFixed` start). The rule falls back on TRAIN too; the side file does not carry the new counters (the
  lane's 153 columns predate them — no schema event was taken), so that is inferred from the costs
  and from E1/families, not read off a TRAIN row.
* **`mufloor` wins `hs` broadly but not uniformly** — 13 of 21 cells cheaper, 2 equal, 6 dearer
  (worst `hs011` 11.57 → 23.71, `hs033` 19.67 → 26.29, `hs027` 27.5 → 33) — and **wins `cutest` narrowly**: 207.332 vs 207.696 over
  16 cells most of which the tier never changes (`BIGBANK`, `NET3`, `SAWPATH`, `STEENBRG`,
  `ZAMB2-8` identical), and one `error` (`TAX53322`) becomes `dnf_wall`, which clause 2 scores in
  its favour (errors 2 → 1). No objective moved by more than 1e-6 relative on any cell both
  solve, under any label.
* **`mu1e-3` wins `hs` by a hair** (10.5556 vs 10.6667) with one large loss (`hs045` 17 → 51.5).

**What the pre-registered reading decides** (page §6.4, verbatim rule: "RECOMMENDED ON iff its
label WINS on at least one family and loses on none"):

* `ipqp_mu0_rule`: loses on both families → **stays `kFixed`**. Not recommended.
* `ipqp_mu_residual_floor`: WINS on both → **RECOMMENDED ON by the TRAIN rule.** The default is
  not moved in this round (the page: a move is the settler's declared event). **Stated beside it,
  not folded into it:** the same toggle FAILED two pre-registered fixture clauses — E1-c (face
  recovery 12 → 8 end-to-end) and T-a (`two_negative_eigenvalue_row` lost). The page did not
  pre-register how a fixture-clause failure bears on a TRAIN recommendation; this README reports
  both and does not resolve it.
* `ipqp_init_mu = 1e-3` on `hs`: WIN → the brief's "HS 1e-3" table entry is **confirmed on TRAIN
  as configuration guidance**, by a margin of 0.11 factorizations per major and with one large
  per-cell loss. hven ships one `ipqp_init_mu`; nothing moves.

## Verdict, per clause

| clause | `ipqp_mu0_rule = kComplementarity` | `ipqp_mu_residual_floor = true` |
|---|---|---|
| G1–G5 (OFF identity, suites, P-SYM, counters, mutants) | PASS (both fields; mutants 19/20 after the rider round, M5 the one equivalent survivor) | ← |
| E1-a no new RED | PASS → PASS | PASS → PASS |
| E1-b cost | FAIL → FAIL | PASS → PASS |
| E1-c recovery | PASS → PASS | PASS → **FAIL** |
| E1-d A1-R1 | FAIL → FAIL | PASS → PASS |
| T-a no loss | PASS → PASS | PASS → **FAIL** |
| T-b HS cost | PASS → **FAIL** | PASS → PASS |
| TRAIN `hs` | WIN → **lose (cost)** | TIE → **WIN** |
| TRAIN `cutest` | ≤ TIE → **lose (cost)** | TIE → **WIN** |
| pre-registered reading | stays `kFixed` | RECOMMENDED ON by TRAIN; two fixture clauses failed beside it |

`ipqp_init_mu = 1e-3` on `hs`: WIN → WIN.

## Findings to register

* **A2-F1 — the ceiling makes `kComplementarity` inert.** The Mehrotra/PIQP estimate's
  complementarity is set by the start's distances times its estimated prices, and with
  trust-region-scale boxes and O(1) prices that is O(1)–O(10) (the unit fixtures measured 0.4–14;
  E1 and every family cell measured above 1e-2). The page pre-registered `ipqp_init_mu` as the
  ceiling (to match the corrector's target clamp and §5.3's warm clamp), so the rule falls back
  almost always. A rule that keeps its derived value needs the corrector's ceiling lifted with
  it; that is a different design, not a tuning of this one.
* **A2-F2 — A1-R1's floor and A1's face are one dial.** The residual floor removes the μ collapse
  (A1-R1 closed on the RED cell) but raises `mu_stop`, and the face recovery on the tight-margin
  class falls with it. A μ update that tracks infeasibility and a stop that resolves `d^2` against
  `kappa * mu` cannot be chosen separately on E1.
* **A2-F3 — the harness cannot see the A2 counters.** The lane's side file (153 columns) predates
  `ipqp_mu0_derived` / `_fallbacks` / `ipqp_mu_floor_binds`; reading "did the rule fire on TRAIN"
  needs a declared harness schema event.
* **A2-F4 — the pre-registration left one combination open:** a TRAIN WIN beside fixture-clause
  FAILs. Reported, not resolved here.
* Carried from the page: **A2-R1** (PIQP Algorithm 2's complementarity-coupled proximal decrease),
  **A2-R2** (the NLP IPM driver's `init_mu = 1e-3`, same circular placement, never measured),
  **A2-R3** (the warm-kill's cold restart under `kComplementarity`).
