# M7 W2 A2 — the complementarity-derived `mu_0`: the measurements

The two A2 toggles `IpqpOptions::ipqp_mu0_rule` (`kFixed` / `kComplementarity`) and
`IpqpOptions::ipqp_mu_residual_floor` (A1-R1), built per the design page
`docs/notes/2026-10-m7-w2-a2-mu0-design.md` and measured per its PRE-REGISTERED section 6. The
page landed at `0000a52f` before any measured row existed; no criterion was edited after.

## Provenance

* hven: branch `w2-a2` (off `m7` `f424c8db`). Pre-registration `0000a52f`; implementation
  `3e0c6afc` (library + tests), the E1-arm flags (bench commit after it), `e924bbe2` (two tests
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
  the T10 families (`fam-*.csv`), and `scored.txt`.
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
| **G5** mutants | **18 of 20 killed** (`out/mutants/`). M5 (strict-positivity check dropped) SURVIVES as an EQUIVALENT mutant: after the `1.5 · min` shift every multiplier is `>= 0.5 |min z| > 0` or the band test already refuses `mu = 0`. M7 (the ladder's high-water mark not restored) SURVIVES: the estimate is taken at the unit-scaled point, whose bound curvature (`Sigma = 1` per side) needs no more modification than the loop's own first iteration, so the restored value is never the larger one on any fixture built; the restore stays as a defence. Round 1 left M8 and M14 alive too; `e924bbe2` added the fixtures that kill them |

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

PENDING — filled when the TRAIN chain closes.

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
