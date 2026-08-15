# B1 — pricing the retarget's extra O(nnz) pattern hash

**Task:** M3 phase C, B1 (`docs/notes/2026-08-15-m3-phase-c-plan.md` §3).
Measurement only — **no source change ships from this task**.

**Verdict (against the pre-stated decision rule): the extra hash costs
6.12 %–7.76 % of SSN-major wall-clock on the SSN-heavy families. That is above
5 %, so B2 is GATE-BLOCKING, not optional.**

**Amended 2026-08-15, review fix round 1** (`task-c-b1-review.md`: SPEC PASS,
deviation ruled VALID INSTRUMENT, 1 Critical + 5 Important + 4 Minor, all
documentation). No measurement was re-run, and no measured figure or verdict
moved. The substantive
correction is **C-1**: the forward finding in §7 misread `ssn_backtracks` as
`ssn_prox_updates` — the mechanism it reports is real, its stated frequency was
wrong by roughly two orders of magnitude, and §7 now carries the recomputed
figure. Also amended: the exclusion rule behind §5's range (**I-1**), the
denominator discipline in §5/§6 (**I-2**, **M-2**), arm S's single-rep status
(**I-3**), the `-dirty` provenance stamp (**I-5**), and §2/§8 (**M-1**, **M-4**).
**I-4** landed in the plan's §7 H3 section rather than here.

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

**Why every committed CSV stamps `31e57b02dc9b-dirty`, on both arms (review fix
round 1, I-5).** `bench/CMakeLists.txt:70-76` resolves that stamp at **configure**
time from `git describe --always --dirty`, and deliberately keeps `--dirty`. The
measurement worktree was dirty at every configure, for two different reasons:

- **Arm A** — a linked worktree does **not** populate submodules, so `dep/eigen`
  and `dep/fmt` came up as empty directories; they were replaced with symlinks to
  the main tree's checked-out copies. That typechange on two gitlinks is the
  *whole* of arm A's dirtiness. **No engine source was modified at arm A's
  configure**: `patch_B.py` was applied only after arm A had finished building,
  and `git status --porcelain -- include src` was empty across that boundary.
- **Arms B and S** — dirty for the honest reason as well: their patch was applied
  before their configure. `31e57b0-dirty` is the correct stamp for them.

So the stamp is right on all three arms and cannot distinguish them. What
distinguishes the arms' CSVs is the `# invocation:` path (`build-A` vs `build-B`),
and what *proves* they are what they claim is `compile-flags-diff.txt` (identical
flags, 26 TUs) plus the ~6 % per-cell delta against a 0.06 % run-to-run spread.
The stamp records honestly that the tree was not pristine; it does not record
*why*, so it is recorded here.

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
Every cell also reproduced the committed
`bench/baselines/2026-08-08-gate-battery/ssn_battery.csv` counters exactly — over
the **31 columns the two schemas share**, the B1 CSVs additionally carrying six
`esc_*` columns the 2026-08-08 baseline predates (schema growth, not drift; review
fix round 1, M-1).

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
probe. Run-to-run spread on the corpus per-rep **solve-wall** totals is 0.06 %
(arm A) and 0.3 % (arm B) — an order of magnitude below the effect. **These
spreads belong to the A/B (§4) only. Arm S, the scaffold the verdict's own metric
is read off, ran once per cell and has no spread of its own** — see §5's closing
paragraph for what carries its confidence instead (review fix round 1, I-3).

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

Over **11 of the 13 pure-SSN cells** — the exclusion rule, and why the excluded
pair does not flatter the result, are stated immediately below the table:

| Quantity | min | median | max |
|---|---|---|---|
| **hashes per SSN major** | 3.27 | **3.35** | 3.41 |
| **one extra hash as % of SSN-major wall** | **6.12 %** | **7.34 %** | **7.76 %** |
| one extra hash as % of one numeric factorization | 17.9 % | 19.9 % | 20.2 % |
| all pattern hashing as % of SSN-major wall | 20.5 % | 24.7 % | 26.2 % |
| `pattern_hash` ns per stored entry | 12.53 | **13.04** | 13.06 |
| numeric factorization ns per stored entry | 64.68 | 65.77 | 74.06 |

**The exclusion rule, stated (review fix round 1, I-1).** Two of the thirteen
pure-SSN cells — `f7_n2000_path_corrupted` and `f7_n2000_path_warm` — are excluded
from this table by one criterion, applied for one reason:

> **Rule.** Drop a cell from the per-call statistics when its process
> accumulators are dominated by a foreign in-process workload, detected as
> `hash_calls / major_calls` implausible against the three-per-major the call
> sites predict.

Those two read `h/maj` of **322.38** and **458.46** against 3.27–3.70 everywhere
else, because their `kCorrupted`/`kFullWarm` setup hop runs a full walk-engine
cold solve in the same process — **8131 and 8104 factorizations and ~28 000
hashes** against **87 and 61** SSN major-loop iterations
(`scaffold-raw-corpus.txt`). Their per-call averages therefore describe a mixture
of two solves, not the cell named on the row, and their X%maj (3.55 %, 3.53 %) is
a ratio across that mixture rather than a reading for the measured solve.

**The exclusion is conservative, not convenient.** Those two cells' *A/B* deltas —
which are unaffected by the contamination, because the A/B compares only the
reported solve's wall — are **7.45 %** and **6.62 %**, among the **highest** of
all seventeen. Including a valid per-major figure for them would raise the range,
not lower it.

**Label correction.** The first version called this subset "the 11 setup-clean
pure-SSN cells". That was wrong: **five** of the eleven carry `CONTAM=setup` in
`scaffold-attribution.txt` (`f7_n800_path_{corrupted,warm}`,
`f7_n825_path_neutral_control`, `f7_n1000_path_{corrupted,warm}`). `CONTAM=setup`
merely flags that a setup hop shares the process; it is the *magnitude* of the
contamination, caught by the `h/maj` rule above, that disqualifies a cell.

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

### How far the two instruments agree — like for like

The first version of this section compared the scaffold's 6.1 %–7.8 % (denominator:
**SSN-major wall**) against the A/B's 4.9 %–7.5 % (denominator: **whole-solve
wall**) and called the result agreement. Those are percentages of different
denominators and are not comparable (review fix round 1, M-2/I-2). The
like-for-like check is **in absolute seconds**, and it is available from what is
already committed:

> predicted saving = `factorizations` × (ns per hash)  — arm B removes one hash
> per `factorize_checked` call — against the **measured** A/B delta.

| cell | predicted Δs | measured Δs | ratio |
|---|---|---|---|
| `f7_n750_path_neutral_control` | 0.0181 | 0.0180 | 1.01 |
| `f7_n800_path_neutral` | 0.0220 | 0.0214 | 1.03 |
| `f7_n800_path_corrupted` | 0.0142 | 0.0138 | 1.03 |
| `f7_n800_path_warm` | 0.0122 | 0.0122 | 1.00 |
| `f7_n825_path_neutral_control` | 0.0221 | 0.0216 | 1.02 |
| `f7_n1000_path_neutral` | 0.0208 | 0.0199 | 1.05 |
| `f7_n1000_path_corrupted` | 0.0191 | 0.0194 | 0.98 |
| `f7_n1000_path_warm` | 0.0199 | 0.0204 | 0.97 |
| `f7_n5000_path_activity` | 0.0770 | 0.0757 | 1.02 |
| `f7_n10000_path_activity` | 0.1140 | 0.1082 | 1.05 |
| `f7_n20000_path_activity` | 0.2412 | 0.2242 | 1.08 |

Over all 11 cells the two instruments agree to within **0.97–1.08**, i.e. **≤ 8 %
of the effect**. This construction is the reviewer's (task review §1); the table
is recomputed here from the committed artifacts and reproduces it. On the four
walk-dominated cells the ratio scatters 0.58–1.31, which is expected: there the
effect is 0.3 % of the cell's wall and the comparison is noise-limited, not
instrument-limited.

**Arm S is single-rep, and this is what carries its confidence (review fix round
1, I-3).** The scaffold ran **once per cell** — the 0.06 % / 0.3 % spreads quoted
in §3 belong to the **A/B**, which is the corroborating instrument, not to arm S.
Arm S has no run-to-run spread of its own. Three things stand in for one:
its ratio is taken over whole-cell accumulator totals (tens to tens of thousands
of calls, not single timings); the 11 independent cells agree with each other to
±0.8 points around a 7.34 % median across a 27× range in problem size; and the
absolute-seconds reconciliation above ties it to a 4-rep instrument to within 8 %.
No re-run was performed for this, and none is owed — but a single-rep instrument
is what the verdict is read off, and that is stated rather than left to be
discovered.

## 6. The decision rule

Pre-stated in the plan before any number existed: *<= 1 % of SSN-major wall-clock
=> noise, B2 does not run; above 1 % => B2 runs; above 5 % => B2 is gate-blocking
rather than optional.*

**Which number the rule is applied to (review fix round 1, I-2).** The rule's
denominator is **SSN-major wall-clock**, and **arm S is the only instrument that
measures that denominator** — `patch_S`'s timer brackets the whole
`for (Index it = 0;; ++it)` body, so its `ns per hash ÷ ns per major` *is* the
rule's metric. The A/B percentages in §4 are on **whole-solve wall**, a strictly
larger denominator; they corroborate, and they are **not admissible against a bar
defined on SSN-major wall**. In particular `f7_n20000`'s A/B figure of 4.85 % is
not a sub-threshold reading of this rule — it is a reading of a different ratio.
The two are reconciled in absolute seconds in §5, which is the comparison that
does not mix denominators.

Measured, on the rule's own metric: **6.12 %–7.76 %** of SSN-major wall-clock on
the SSN-heavy families, median **7.34 %**, minimum **6.12 %** — all 11 cells clear
5 %.

> **VERDICT: above 5 % => B2 is GATE-BLOCKING.**

Three qualifications, stated rather than buried:

1. On families where the active-set walk owns the wall, the same removal is
   worth 0.42 % of whole-solve wall. The verdict is scoped to the families the
   gate-B verdict named, which is where the per-major cost lands.
2. **Layout noise, bounded.** Arm B's patch also changes code layout, and layout
   noise is worth a point or so on its own. The absolute-seconds reconciliation
   in §5 bounds layout noise *plus* scaffold timer overhead at **≤ 8 % of the
   effect** — i.e. ≤ 0.6 points on a 7.3 % figure. Applying that correction to
   the **weakest** cell gives 6.12 / 1.08 = **5.7 %**, and to the next weakest
   6.38 / 1.05 = **6.1 %**. The verdict survives its own worst case with roughly a
   0.7-point margin, and much more elsewhere. (This bound is the reviewer's
   construction, adopted here; the earlier "the smaller of the two readings"
   sentence was self-contradictory and mixed denominators, and is withdrawn.)
3. Arm S is single-rep; see §5's closing paragraph for what carries its
   confidence.

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
`SsnEngine::structure_hash` is a **second, separately-counted O(nnz) hash** that
this report's figures do not include (the scaffold instruments
`hven::pattern_hash` only). `sync_matrix` computes it **unconditionally**, before
any decision (`ssn_engine.h:3034`), and is reached from two places: once per SSN
solve (`ssn_engine.h:1688`) **and again on every prox update** (`ssn_engine.h:3259`,
via `set_prox_sigma`).

**Frequency, corrected — review fix round 1, C-1.** The first version of this
paragraph claimed `ssn_prox_updates` "often exceeds `ssn_iters` (144 against 28 on
`f7_n1000_path_activity`)". **That was a counter misread**: 144 is that cell's
`ssn_backtracks`; its `ssn_prox_updates` is **2**. Across all 17 cells
`ssn_prox_updates` ranges **0–3** and **never** exceeds `ssn_iters` (worst case 3
against 32 majors). The mechanism stands — the reviewer verified the two call
sites and the unconditional hash independently — but the frequency does not.

Recomputed from `corpus_A_r1.csv`, structure hashes per solve = QP subproblems +
prox updates:

| | min | median | max |
|---|---|---|---|
| `structure_hash` calls per SSN major | 0.097 | **0.148** | 0.219 |
| as a share of the 3 `hven::pattern_hash`/major priced above | 3.2 % | 4.9 % | 7.3 % |
| implied cost, at this report's ≈7.3 %-per-hash-per-major unit price | 0.7 % | **1.1 %** | 1.6 % of SSN-major wall |

The last row is an **upper bound**: `structure_hash` feeds H/Ae/Ai plus the
bound-row list, whose combined nnz is at most the assembled KKT's, so it cannot
cost more per call than the `hven::pattern_hash` the unit price was measured on.

**Corrected hand-off to H1/H3.** Worth **inventorying** at H1 — the hash is
unconditional, its second call site is frequency-dependent on the family, and a
prox-heavy family would pay more than this corpus does. It is **not** a hot path
on these families: about one seventh of what the extra pattern hash costs, on the
order of 1 % of SSN-major wall. It changes **no** B-series conclusion, and H3
should price its design against it as a ~1 % item, not a "several percent" one.

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
| `cells.txt` | the 17-cell list the runner consumes |

Reproduce (review fix round 1, M-4 — the runner hard-codes a session scratchpad
path, so the first step is not optional):

1. `git worktree add --detach <wt> 31e57b0`; populate `dep/eigen` and `dep/fmt`
   (a linked worktree leaves them empty).
2. **Repoint `SP` at the top of `run_ab.sh`** to a directory holding `build-A`,
   `build-B` and a copy of `cells.txt`. Both are read from `$SP`.
3. Configure three Release builds (`HVEN_BUILD_TESTS=OFF`, `HVEN_BUILD_BENCH=ON`),
   applying `patch_B.py <wt>` / `patch_S.py <wt>` **before** the arm's configure
   and reverting with `git checkout -- include src` after it links.
4. `MKL_NUM_THREADS=1`, machine idle, then `run_ab.sh`.

The cell list is also recoverable independently from any committed CSV's
`# invocation:` header.
