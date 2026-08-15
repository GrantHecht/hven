# B1 — pricing the retarget's extra O(nnz) pattern hash

**Task:** M3 phase C, B1 (`docs/notes/2026-08-15-m3-phase-c-plan.md` §3).
Measurement only — **no source change ships from this task**.

**Verdict (against the pre-stated decision rule): the extra hash costs
6.1 %–7.8 % of SSN-major wall-clock on the SSN-heavy families. That is above
5 %, so B2 is GATE-BLOCKING, not optional.**

---

## 1. Provenance

| | |
|---|---|
| Measured at | **`31e57b0`** (`test(sqp): update the three walk-baseline consumers for the declared gate-B amendment`) — the phase-C base, before any relocation, per the plan's method clause |
| Measurement tree | a detached `git worktree` at `31e57b0`; the `m3` working tree was never modified and was verified clean before and after |
| Date | 2026-08-15 (measurement window 03:51:18 → 04:53:14 −04:00) |
| Host | fedora, AMD Ryzen 7 5800X3D (8C/16T), 31 GiB, Linux 7.1.5-201.fc44, `scaling_governor = powersave` |
| Toolchain | clang 22.1.8 (Fedora 22.1.8-4.fc44), Ninja, `CMAKE_BUILD_TYPE=Release`, `HVEN_BUILD_TESTS=OFF`, `HVEN_BUILD_BENCH=ON` |
| Backend | Intel oneAPI MKL **2026.1**, LP64, `libmkl_intel_thread` + `libiomp5` |
| **`MKL_NUM_THREADS`** | **1**, exported for every solve-running step |
| Serialization | strictly one solve at a time; the machine ran nothing else during the window except a handful of small read-only file reads (well under a second of CPU on one of sixteen cores), and the A and B passes are interleaved so any residual drift is charged to both arms |
| Build parallelism | `-j6`, all builds completed **before** the measurement window opened |

**Claim class (CLAUDE.md §7).** The counters here are asserted: the
hashes-per-major count, and the counter-identity of the two arms. The costs are
**wall-clock and are therefore informational by the project's own rule** — but
this task's *obligation* is a wall-clock obligation (gate-B request note §12
addendum item 2 asks that the retarget's steady-state cost be priced), so the
claim is stated as a wall-clock claim and labelled as one.

## 2. What was measured

The retarget's one disclosed steady-state cost: **steady state computes the KKT
pattern hash three times per SSN major** — `needs_analysis` at the call site,
`factorize_checked`'s own `needs_analysis`, and `SymmetricFactor::factorize`'s
internal `hven::pattern_hash` — where the dissolved `KktSystem` seam computed
two. The quantity priced is **the one extra hash**.

### Method deviation, declared

The plan's step 1 says "build `31e57b0` and **the archived pre-retarget engine at
the pinned tag**". **This repository carries no tags**, and the only in-repo
pre-retarget state is `258c337` (the commit before `ab8aeda`), where the SQP
engine still had `kkt_system.h`. Building *that* would compare two engines
differing in far more than one hash — the whole KKT-layer retarget — and would
not attribute the cost to the hash.

**What was done instead:** arm B is `31e57b0` plus an **uncommitted, local,
measurement-only patch** that threads the already-computed hash through, so
steady state pays **two** — the dissolved seam's exact count — with every other
line of the engine identical. That isolates the one variable the plan's decision
rule is about. The patch is reproduced verbatim in `patch_B.py` beside this
report; it was reverted immediately after the arm-B build linked, and both the
worktree and the `m3` tree were verified clean.

The patch's hash arithmetic, stated so it can be checked:

| Situation | arm A (`31e57b0`) | arm B (patched) | pre-retarget seam |
|---|---|---|---|
| Steady state (analyzed, same pattern) | 3 | **2** | 2 |
| First factorize (nothing analyzed yet) | 3 | 3 | 3 |
| Cross-pattern re-analysis | 5 | 3 | 3 |

`needs_analysis` short-circuits on `!k.analyzed`, which is why the first-factorize
row is already equal; the steady-state row is the one the retarget moved.

### Comparability

`compile-flags-diff.txt`: over all 26 translation units, **arm A, arm B and the
scaffold arm S compile under byte-identical flags** (only each build's own output
path differs). All three were configured from the same worktree with the same
cache options.

### Counter neutrality — asserted

Across all eight A/B passes, **zero** differences in any counter column
(`status`, `factorizations`, `qp_minors`, `escapes`, `qp_subproblems`,
`qp_fact_per_qp`, every `ssn_*` counter, `neg_ineq_duals`, `kkt_verdict`) — see
`ab-analysis.txt` line 1. Arm B changes when a hash is computed and nothing else.
Every cell also reproduced the committed `2026-08-08-gate-battery/ssn_battery.csv`
counters exactly.

## 3. Population — the SSN-heavy families

The `kSsn` arm of the gate battery, restricted to the cells whose wall the SSN
kernel actually owns, and **bounded**: every cell in the committed battery that
completed `Optimal` with `ssn_iters >= 12`, minus the three whose committed wall
exceeds 10 s (`f7_n2000_path_activity` 42 s, `f7_n2000_path_physics` 44 s,
`f7_n5000_path_physics` 305 s) and the one that recorded `engine_error` against a
2700 s deadline (`f7_n2000_path_neutral`). **17 cells**, n = 750 … 20000, one
pass ~= 424 s wall. Each cell additionally carries the corpus runner's own
per-phase wall deadline, so no cell can run unbounded.

Thirteen of the seventeen have `qp_minors = 0` — pure SSN. Four
(`f7_n{800,1000}_path_{physics,activity}`) spend most of their wall in the
active-set walk's minor iterations; they are kept and reported separately,
because they show what the same change is worth where SSN is *not* the
bottleneck.

Second vehicle: `hven_sqp_ssn_safeguard_probe census 20000` — 517 301 SSN majors
over ~20 000 tiny synthetic QPs (nnz ~= 24 per KKT), fixed count and seed.

**Reps:** 4 per arm for the corpus (interleaved A,B,A,B,…), 7 per arm for the
probe. Run-to-run spread on the corpus pass total is 0.06 % (arm A) and 0.3 %
(arm B) — an order of magnitude below the effect.

## 4. Result 1 — instrument-free A/B (plan step 1)

Per-cell reported solve wall, median of 4 reps. Full table in `ab-analysis.txt`.

| Cell group | arm A | arm B | delta |
|---|---|---|---|
| 13 pure-SSN cells, pooled | 11.2952 s | 10.6653 s | **−5.58 %** |
| per-cell range over those 13 | | | **−4.85 % … −7.45 %** (median −6.34 %) |
| 4 walk-dominated cells, pooled | 24.6523 s | 24.5483 s | −0.42 % |
| all 17, pooled | 35.955 s | 35.214 s | −2.06 % |
| probe, whole-process wall (median of 7) | 23.542 s | 23.363 s | −0.76 % |

Removing **one** hash per major removes 4.85 %–7.45 % of the **entire reported
solve wall** on the pure-SSN cells — and the reported solve wall includes the
driver's NLP evaluations and everything else outside the SSN major loop, so the
share of *SSN-major* wall is necessarily larger. The effect does not decay with
size: −5.88 % at n = 5000, −5.14 % at n = 10000, −4.85 % at n = 20000.

The probe's 0.76 % is not a contradiction: only 26 % of that process's wall is
inside the SSN major loop (6.17 s of 23.5 s; the rest is fixture generation), and
its per-major figure from the scaffold is 3.24 %.

## 5. Result 2 — direct attribution (plan step 2)

An uncommitted scaffold (`patch_S.py`) times `hven::pattern_hash`, the numeric
factorization, and the SSN major loop. Full per-cell table in
`scaffold-attribution.txt`.

Over the 11 setup-clean pure-SSN cells:

| Quantity | min | median | max |
|---|---|---|---|
| **hashes per SSN major** | 3.27 | **3.35** | 3.41 |
| **one extra hash as % of SSN-major wall** | **6.12 %** | **7.34 %** | **7.76 %** |
| one extra hash as % of one numeric factorization | 17.9 % | 19.9 % | 20.2 % |
| all pattern hashing as % of SSN-major wall | 20.5 % | 24.7 % | 26.2 % |
| `pattern_hash` ns per stored entry | 12.53 | **13.04** | 13.06 |
| numeric factorization ns per stored entry | 64.68 | 65.77 | 74.06 |

The measured 3.27–3.41 hashes per major **confirms the gate-B disclosure's
"three"** by counter (the excess over 3.00 is the analysis-branch and border
sites, which are not per-major).

**Why it is this expensive, and why it does not shrink with n.**
`Fnv1a::feed_index` is a strictly serial XOR-then-multiply chain, eight steps per
index (`pattern_hash.h:60-64`), so `hven::pattern_hash` costs ~13 ns per stored
entry regardless of size. A Pardiso numeric factorization of these
path-collocation KKT systems costs ~66 ns per stored entry, also near-linear. The
ratio is therefore roughly **1 : 5, constant in n** — which is exactly what the
n = 750 → 20000 sweep shows. "Against a numeric factorization this is expected to
be noise" was the presumption gate B recorded; the measurement says one hash is a
fifth of a factorization, and the engine pays three of them per major.

The two instruments are independent (one changes the code and times the product;
the other times the function in place) and they agree: 6.1 %–7.8 % per-major from
the scaffold, 4.9 %–7.5 % of whole-solve wall from the A/B.

## 6. The decision rule

Pre-stated in the plan before any number existed: *<= 1 % of SSN-major wall-clock
=> noise, B2 does not run; above 1 % => B2 runs; above 5 % => B2 is gate-blocking
rather than optional.*

Measured: **6.12 %–7.76 %** of SSN-major wall-clock on the SSN-heavy families
(median 7.34 %), corroborated by a 5.58 % pooled reduction in whole-solve wall on
the same cells.

> **VERDICT: above 5 % => B2 is GATE-BLOCKING.**

Two qualifications, stated rather than buried:

1. On families where the active-set walk owns the wall, the same removal is
   worth 0.42 %. The verdict is scoped to the families the gate-B verdict named,
   which is where the per-major cost lands.
2. Arm B's patch also changes code layout, and layout noise is worth a percent or
   so on its own. That is why the scaffold exists: the two instruments disagree
   by about a point, and the *smaller* of the two readings is still above 5 % on
   every clean cell but two (`f7_n10000` 6.38 %, `f7_n20000` 6.12 %).

## 7. Hash-count-per-major end-state row (phase-C ratification A3)

Steady-state `hven::pattern_hash` calls per SSN major:

| Stage | Count | Who computes them | Priced here at |
|---|---|---|---|
| Pre-retarget (dissolved `KktSystem` seam) | 2 | call-site `pattern_matches`; `KktSystem::factorize`'s own | — |
| **Phase-C base `31e57b0`** | **3** (measured 3.27–3.41 incl. non-per-major sites) | call-site `needs_analysis`; `factorize_checked`'s `needs_analysis`; `SymmetricFactor::factorize`'s internal guard | 24.7 % of SSN-major wall |
| **Post-B2** | **2** | the threaded decision's one hash; `SymmetricFactor::factorize`'s internal guard | ~17.4 % of SSN-major wall (one hash saved, 7.3 %) |
| Floor without changing `hven::linear`'s contract | 2 | `factorize()`'s pattern guard is `SymmetricFactor`'s own published contract (`FactorizeRejectsAForeignPattern`); B2 does not touch it, so 2 is B2's floor. Going to 1 would be a `hven::linear` contract change, out of scope for B2 and not proposed here | — |
| **Post-H3 (the composite re-key)** | **2 — H3 must not move this number** | H3 re-keys the *SQP-side* structure hashes (`qp_engine.h`'s `structural_hash`, `ssn_engine.h`'s `structure_hash`), not the KKT-level `hven::pattern_hash`; it changes the digest, not the per-major count | H1's Q3 constraint priced: an iteration/compressed-copy path that added **one** O(nnz) pass per major would cost 7.3 % of SSN-major wall on these families — the same unit price measured above |

So the B-series and the H-series agree on the arithmetic: **3 -> 2 -> 2.**

A finding H1/H3 should carry, discovered while establishing the above:
`SsnEngine::structure_hash` is an O(nnz) hash reached from `sync_matrix`, which
runs once per SSN solve (`ssn_engine.h:1688`) **and again on every prox update**
(`ssn_engine.h:3259`, via `set_prox_sigma`). On these cells `ssn_prox_updates`
often exceeds `ssn_iters` (e.g. 144 updates against 28 majors on
`f7_n1000_path_activity`), so that hash is already a per-major-or-worse O(nnz)
cost — on the same 13 ns/entry cost curve measured here. It is **not** counted in
this report's figures (which instrument `hven::pattern_hash` only). H3's design
should be priced against it.

## 8. Files

| File | What |
|---|---|
| `report.md` | this |
| `ab-analysis.txt` | the counter-identity check and the full per-cell A/B table |
| `ab-run.log` | the measurement window's own timing log |
| `corpus_{A,B}_r{1..4}.csv` | the eight raw corpus artifacts, with their own provenance headers |
| `scaffold-attribution.txt` | the derived per-cell direct-attribution table |
| `scaffold-raw-{corpus,probe}.txt` | the scaffold's raw accumulator dumps |
| `compile-flags-diff.txt` | the three arms' compile-flag identity |
| `patch_B.py`, `patch_S.py` | the two **measurement-only, never-committed-to-the-engine** patches, kept here so the measurement reproduces |
| `run_ab.sh` | the runner |

Reproduce: `git worktree add --detach <wt> 31e57b0`, configure three Release
builds, apply `patch_B.py` / `patch_S.py` to their respective trees before
configuring, revert, then `run_ab.sh`.
