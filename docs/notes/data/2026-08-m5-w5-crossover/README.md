# M5 W5 — the IPM → SQP crossover, measured on the replay corpus

Date: 2026-08-25. Branch: `m5`. Runner commit: `8bd4b9d62ef0`
(`bench/bench_crossover.cpp`, `bench/crossover_legs.h`; correctness gate
`tests/sqp/test_crossover_legs.cpp`).

This directory is the W5 deliverable: what the interior-point → SQP hand-off is
worth, in counters, on real corpus problems.

---

## 1. The protocol, verbatim

Declared before the runs, in `docs/notes/2026-08-m5-ledger.md` at commit
`cf198da`. Quoted here in full and unedited, because the artifact must carry the
protocol it was produced under:

> **W5 LEG PROTOCOL, declared before the runs** (§7):
> PROBLEM SET: every replay-corpus cell expressible through the
> dual-bind path (the NlpProblemModel conversion the W3 crossover
> pin uses — one declaration, both engines); cells that cannot
> dual-bind are LISTED in the artifact with the reason, never
> silently dropped. LEGS per cell: (a) IPM-only (baseline +
> exporter), (b) SQP cold, (c) SQP warm core-only (a's export, tag
> stripped), (d) SQP warm with polish. ASSERTED CURRENCY: counters
> only — SQP majors / qp minors / factorizations for legs b/c/d,
> with c-vs-b and d-vs-b margins recorded per cell; statuses and
> the per-cell values are deterministic per-process columns at
> MKL_NUM_THREADS=1. WALL: informational only, recorded but never
> quoted as measurement unless re-run solo under the serial rule.
> EXECUTION: one solve at a time, MKL_NUM_THREADS=1, serialized —
> no co-run for this artifact (first derivation; co-run terms would
> need declaring and buy little at this corpus size). ARTIFACT:
> docs/notes/data/2026-08-m5-w5-crossover/ — per-leg CSVs with §7
> provenance stamps (toolchain, hardware, date, commit), a README
> declaring this protocol verbatim, and the aggregate table
> (per-cell counter margins). The known census-cell residual
> variance and the slow-Continuation box-health carry are noted in
> the README as environment caveats; nothing here asserts wall or
> residual byte-equality across processes.

### 1.1 The one addition to it, declared

The protocol names no wall budget. CLAUDE.md §8 requires one ("no unbounded
cells that can run indefinitely"), so the runner enforces a **per-cell wall
deadline of 1200 s**, recorded in every CSV's provenance header. A cell that
outlives it is SIGKILLed and its unfinished legs are written as `dnf_budget`
rows with `-1` in every counter column.

Two consequences a reader must hold onto:

- **A `dnf_budget` row is a budget outcome, not a solver verdict.** It says the
  cell did not finish inside this artifact's budget; it says nothing about
  whether the engine would have converged.
- **A deadline outcome is wall-DEPENDENT**, and therefore not one of the
  scheduling-invariant columns §7 permits to co-run. This is the second reason
  the sweep is serial, beside the protocol's own instruction.

### 1.2 One discarded run, on the record

A first sweep was launched and **stopped after three cells, and its rows
discarded**, when a concurrent build of another project was found competing for
the machine. Counters at `MKL_NUM_THREADS=1` are scheduling-invariant and would
have survived it, but this artifact's deadline outcomes are wall-DEPENDENT, and
the protocol's own instruction is "one solve at a time" — a partial run under
contention is not that. The rows in this directory come from a single sweep
begun only after the box went quiet, and `box_witness.log` records, once a
minute for the whole sweep, how many competing build or compiler processes were
running. Every sample in it must read `competing=0`; a nonzero sample means the
cells overlapping it need re-running solo before their rows are trusted.

The legs run in the order **(a), (c), (d), (b)** — the exporter first because
the warm legs stage its output, and the cold leg LAST because it is the
expensive one. Each leg is an independent solve on a fresh driver over a fresh
bridge, so the order moves no counter; what it buys is that a cell killed at the
deadline still contributes legs (a), (c) and (d), with the margins then honestly
ABSENT rather than fabricated.

---

## 2. What "dual-bind" means here, and why 24 of 57 cells

The W3 crossover pin
(`tests/sqp/test_sqp_warm_currency.cpp::InteriorPointExportCrossesOverIntoTheSqpEngine`)
established the shape the protocol names: **one declared `NLPProblem`**, bound
to the interior-point engine through `NLPSolver`'s own transcription and to the
SQP engine through `NlpProblemModel`. Both engines then compute the same
`DeclarationKey` — the content of the 2026-08-25 declaration-identity ruling —
and an exported value stages across with no conversion and no re-stamp.

The corpus's cells are `NlpModel`s (`F7CollocationChain`), not `NLPProblem`s, so
the half of that path that did not exist was an `NlpModel` stated as an
`NLPProblem`. That is `ModelAsNlpProblem` (`bench/crossover_legs.h`). It is
**not** a second conversion running alongside `NlpProblemModel`: the declared
`NLPProblem` is *the* declaration, and neither engine sees the F7 model
directly. `CrossoverLegs.BothEnginesKeyOneDeclarationTheSameWay` pins the key
agreement; `CrossoverAdapter.StatesTheSameProblemAsTheModelItWraps` pins that
the declaration reads back through `NlpProblemModel` as the original model's
`f`, `cE`, `cI`, both Jacobians and the Lagrangian Hessian.

**Every cell's PROBLEM dual-binds.** What does not always dual-bind is the
cell's declared START.

### 2.1 Measured: 24 cells

The two taxonomies whose start is a bare primal `x0`, which both engines take:

| taxonomy | start | cells |
|---|---|---|
| `kNeutralCold` | `model.start_point()` | 13 |
| `kPhysicsInformed` | `x*(p)` + a deterministic 1e-3 sin-index displacement | 11 |

(13 rather than 12 because the census's two healthy controls, N = 750 and
N = 825, are `kNeutralCold` only.)

Across N ∈ {750, 800, 825, 1000, 2000, 5000, 10000, 20000} and both constraint
windows, as the census lays them out.

### 2.2 Not measured: 33 cells, each with its reason

Listed here in full, never silently dropped. The full machine-readable listing
is `hven_sqp_crossover --list`.

| taxonomy | cells | why leg (a) cannot run from this cell's own start |
|---|---|---|
| `kCorrupted` | 11 | The declared start is a damaged SQP `WarmStart` — the value a prior SQP solve at `p0` produced, then displaced. It carries a hot factorization handle and an activity encoding with **no interior-point counterpart**: the M5 currency's neutral core is not a `WarmStart`, and the IPM's `stage_warm_start` takes a `WarmStartData`. A leg (a) run from somewhere else would not be this cell's baseline. |
| `kFullWarm` | 11 | The same, undamaged: an SQP `WarmStart` carried from a prior solve at `p0`. Same refusal, same reason. |
| `kActivityOnly` | 11 | The declared start is a **synthesized** interior-point iterate — `corpus_cells.h`'s `f7_ip_iterate`, a hand-built central-path point pushed through `from_interior_point`. Leg (a) would have to be the synthesis rather than a solve. **Superseded here by construction**: legs (c) and (d) measure the same hand-off with a *real* interior-point export in place of the synthetic one, which is the measurement this taxonomy was standing in for. |

The 24/33 split is pinned by
`CrossoverLegs.TheDualBindPartitionIsTheOneTheArtifactDocuments`, so a census
change that moves it fails a test rather than silently making this prose wrong.

---

## 3. Files

| file | rows |
|---|---|
| `leg_a_ipm_only.csv` | the interior-point baseline and exporter: convergence flag, iterations, KKT analyses/factorizations/solves, objective, the four terminal residuals, whether the export carried the polish extension |
| `leg_b_sqp_cold.csv` | SQP from cold, same declaration, same `x0`, nothing staged |
| `leg_c_sqp_warm_core.csv` | SQP with (a)'s export staged, `extensions_` cleared — the R3 core-only shape |
| `leg_d_sqp_warm_polish.csv` | SQP with (a)'s export staged exactly as the other engine handed it over |
| `margins.csv` | the aggregate: cold's three counters, and the c-vs-b and d-vs-b margins per cell |
| `sweep.log` | the runner's console transcript of the sweep |
| `box_witness.log` | one line a minute for the duration of the sweep: how many competing build/compiler processes were on the machine. The evidence behind the "alone on the machine" claim, rather than an assertion of it |
| `cells_not_measured.txt` | the 33 refused cells, one per line, each with its reason — `hven_sqp_crossover --list` output for the refused half |

Margin sign convention: **positive means saved** (`cold − warm`). An undefined
margin — either leg missing a counter — is written `absent`, never `0`: a zero
would read as "the crossover saved nothing here", which is a measurement, and
absence is the truth when a leg never ran. `MarginsAreColdMinusWarmAndAbsentWhenUndefined`
pins both halves.

### 3.1 Provenance (CLAUDE.md §7: toolchain, hardware, date, commit)

Each CSV's own header carries the commit, the exact invocation, `MKL_NUM_THREADS`,
the host, the generation timestamp, the deadline, and the serial/asserted-currency
declarations. The two §7 items the header names only by hostname are recorded here
in full:

| | |
|---|---|
| commit | `8bd4b9d62ef0` (branch `m5`), stamped into every CSV as `# binary:` |
| toolchain | clang 22.1.8 (Fedora 22.1.8-4.fc44), C++20, CMake Release, `HVEN_FP_MODE=SAFER_FAST`, build tree `build-m5-release` |
| hardware | AMD Ryzen 7 5800X3D, 8 physical cores / 16 logical; Linux 7.1.9-200.fc44.x86_64 (host `fedora`) |
| linear algebra | Intel MKL Pardiso, `MKL_NUM_THREADS=1` in the sweep process |
| date | sweep began 2026-08-26T02:35:05Z |

---

## 4. Results

### 4.1 Per-cell counter margins

> Rows for `f7_n10000_path_neutral`, `f7_n10000_path_physics`, `f7_n20000_path_neutral`, `f7_n20000_path_physics` come from **pass 2**, the leg-order-fix re-run (`pass2-legorder-fix/`, binary `575331d10fa5`); every other row is from the pass-1 sweep (binary `8bd4b9d62ef0`). See section 5.1.

Cold leg (b) counters are `majors / QP minors / factorizations`; the two
margin columns are **cold minus warm, so positive means saved**.

| cell | N | window | start | cold b | core c saved | polish d saved | IPM |
|---|---|---|---|---|---|---|---|
| `f7_n1000_bound_neutral` | 1000 | bound | neutral | 1/2/1 | 1/2/1 | 1/2/1 | CONVERGED (7 it) |
| `f7_n1000_bound_physics` | 1000 | bound | physics | 1/2/1 | 1/2/1 | 1/2/1 | CONVERGED (5 it) |
| `f7_n2000_bound_neutral` | 2000 | bound | neutral | 1/2/1 | 1/2/1 | 1/2/1 | CONVERGED (8 it) |
| `f7_n2000_bound_physics` | 2000 | bound | physics | 1/2/1 | 1/2/1 | 1/2/1 | CONVERGED (5 it) |
| `f7_n5000_bound_neutral` | 5000 | bound | neutral | 1/2/1 | 1/2/1 | 1/2/1 | CONVERGED (8 it) |
| `f7_n5000_bound_physics` | 5000 | bound | physics | 1/2/1 | 1/2/1 | 1/2/1 | CONVERGED (5 it) |
| `f7_n10000_bound_neutral` | 10000 | bound | neutral | 1/2/1 | 1/2/1 | 1/2/1 | CONVERGED (9 it) |
| `f7_n10000_bound_physics` | 10000 | bound | physics | 1/2/1 | 1/2/1 | 1/2/1 | CONVERGED (5 it) |
| `f7_n20000_bound_neutral` | 20000 | bound | neutral | 1/2/1 | 1/2/1 | 1/2/1 | CONVERGED (9 it) |
| `f7_n20000_bound_physics` | 20000 | bound | physics | 1/2/1 | 1/2/1 | 1/2/1 | CONVERGED (5 it) |
| `f7_n800_path_physics` | 800 | path | physics | 2/481/6 | 1/-228/0 | 1/479/5 | CONVERGED (18 it) |
| `f7_n1000_path_physics` | 1000 | path | physics | 2/623/5 | 1/-264/-2 | 1/621/4 | CONVERGED (18 it) |
| `f7_n2000_path_physics` | 2000 | path | physics | 2/1396/9 | 1/-379/-5 | 1/1394/8 | CONVERGED (21 it) |
| `f7_n750_path_neutral_control` | 750 | path | neutral | 4/9297/7865 | 3/8632/7859 | 3/9295/7864 | CONVERGED (15 it) |
| `f7_n825_path_neutral_control` | 825 | path | neutral | 4/9561/7605 | 3/8829/7599 | 3/9559/7604 | CONVERGED (15 it) |
| `f7_n1000_path_neutral` | 1000 | path | neutral | 4/11043/9951 | 3/10156/9944 | 3/11041/9950 | CONVERGED (15 it) |
| `f7_n5000_path_physics` | 5000 | path | physics | 2/3656/19 | 1/-779/-16 | 1/3654/18 | CONVERGED (18 it) |
| `f7_n800_path_neutral` | 800 | path | neutral | 5/30165/29222 | 4/29456/29216 | 4/30163/29221 | CONVERGED (15 it) |
| `f7_n2000_path_neutral` | 2000 | path | neutral | 3/40004/39744 | 2/38229/39730 | 2/40002/39743 | CONVERGED (16 it) |
| `f7_n5000_path_neutral` | 5000 | path | neutral | **dnf_budget** | absent/absent/absent | absent/absent/absent | CONVERGED (17 it) |
| `f7_n10000_path_physics` | 10000 | path | physics | **dnf_budget** | absent/absent/absent | absent/absent/absent | CONVERGED (22 it) |
| `f7_n10000_path_neutral` | 10000 | path | neutral | **dnf_budget** | absent/absent/absent | absent/absent/absent | CONVERGED (16 it) |
| `f7_n20000_path_physics` | 20000 | path | physics | **dnf_budget** | absent/absent/absent | absent/absent/absent | CONVERGED (21 it) |
| `f7_n20000_path_neutral` | 20000 | path | neutral | **dnf_budget** | absent/absent/absent | absent/absent/absent | CONVERGED (16 it) |

**19 of 24 cells have a defined margin** (a margin needs a cold baseline; where the cold leg did not finish inside its budget the margin is `absent`, never `0`).

**Cold baseline reached `Optimal` — work SAVED reaching the same answer** (18 cells)

| | majors saved | QP minors saved | factorizations saved |
|---|---|---|---|
| **core-only (c)** | 27 (77% of 35), per-cell 1..4 | 55443, per-cell -779..29456 | 54605, per-cell -16..29216 |
| **with polish (d)** | 27 (77% of 35), per-cell 1..4 | 66226, per-cell 2..30163 | 54684, per-cell 1..29221 |

**Cold baseline did NOT reach `Optimal` — these are NOT averaged with the above.** A margin against a failed cold solve measures work the cold leg burned before giving up, not work saved reaching the same answer. Reported per cell, never pooled:

| cell | cold status | cold cost | core c | polish d | warm outcome |
|---|---|---|---|---|---|
| `f7_n2000_path_neutral` | **NumericalError** | 3/40004/39744 | 2/38229/39730 | 2/40002/39743 | core Optimal, polish Optimal |

Cells where **core-only cost MORE QP minors than cold**: `f7_n800_path_physics`, `f7_n1000_path_physics`, `f7_n2000_path_physics`, `f7_n5000_path_physics`.
Cells where the polish route cost more QP minors than cold: **none**.

**The activity-inference identity**, checked on every cell with both legs measured:

| cell | core-only `qp_minors` | polish `ip_activity_inferred` | identity holds |
|---|---|---|---|
| `f7_n1000_bound_neutral` | 0 | 0 | yes |
| `f7_n1000_bound_physics` | 0 | 0 | yes |
| `f7_n2000_bound_neutral` | 0 | 0 | yes |
| `f7_n2000_bound_physics` | 0 | 0 | yes |
| `f7_n5000_bound_neutral` | 0 | 0 | yes |
| `f7_n5000_bound_physics` | 0 | 0 | yes |
| `f7_n10000_bound_neutral` | 0 | 0 | yes |
| `f7_n10000_bound_physics` | 0 | 0 | yes |
| `f7_n20000_bound_neutral` | 0 | 0 | yes |
| `f7_n20000_bound_physics` | 0 | 0 | yes |
| `f7_n800_path_physics` | 709 | 708 | yes |
| `f7_n1000_path_physics` | 887 | 886 | yes |
| `f7_n2000_path_physics` | 1775 | 1774 | yes |
| `f7_n750_path_neutral_control` | 665 | 664 | yes |
| `f7_n825_path_neutral_control` | 732 | 731 | yes |
| `f7_n1000_path_neutral` | 887 | 886 | yes |
| `f7_n5000_path_physics` | 4435 | 4434 | yes |
| `f7_n800_path_neutral` | 709 | 708 | yes |
| `f7_n2000_path_neutral` | 1775 | 1774 | yes |
| `f7_n5000_path_neutral` | 4435 | 4434 | yes |

Holds on 20 cell(s), fails on 0. `ip_activity_inferred` is identically 0 on every core-only leg.

### 4.1b Cells whose COLD leg exceeded the budget

These have no margin -- a margin needs a cold baseline. They are still the most informative cells in the artifact, because **the polish leg finished on every one of them, and it is the only route that did**. On the four largest, neither the cold baseline nor the core-only warm start reached an answer inside its budget:

| cell | N | IPM (a) | core-only (c) | with polish (d) | cold (b) |
|---|---|---|---|---|---|
| `f7_n5000_path_neutral` | 5000 | CONVERGED (17 it, 17 fac) | Optimal 1/4435/35 | Optimal 1/2/1 | **dnf_budget** |
| `f7_n10000_path_physics` | 10000 | CONVERGED (22 it, 22 fac) | **dnf_budget** | Optimal 1/2/1 | **dnf_budget** |
| `f7_n10000_path_neutral` | 10000 | CONVERGED (16 it, 16 fac) | **dnf_budget** | Optimal 1/2/1 | **dnf_budget** |
| `f7_n20000_path_physics` | 20000 | CONVERGED (21 it, 21 fac) | **dnf_budget** | Optimal 1/39/2 | **dnf_budget** |
| `f7_n20000_path_neutral` | 20000 | CONVERGED (16 it, 16 fac) | **dnf_budget** | Optimal 1/2/1 | **dnf_budget** |

The frozen walk baseline, measured SOLO under `scripts/run_walk_census.sh`'s ratified protocol, records the cold solve of these same cells at 2363.9 s (N = 5000 neutral), 2801.6 s (N = 10000 physics) and its own 3600 s budget exhaustion (N = 10000 neutral, N = 20000 both). Every one of those exceeds this artifact's 1200 s per-cell budget outright. **The deadline outcomes here are therefore over-determined**: they are what a solo run would have produced too, and the contention recorded in section 4.3 is not the discriminating factor. That corroboration is independent evidence, not a solo re-run, and is labelled as such.

### 4.2 What the legs say

**The polish extension, not the neutral core, is what makes the crossover pay.**

Leg (d) — the export staged exactly as the interior-point engine handed it over
— costs **1 major on every path cell and 0 on every bound-arc cell, at every
problem size measured**, and its QP work stays in single or low double digits
while the problem grows by a factor of 27:

| window | N | leg (d) cost, majors / QP minors / factorizations |
|---|---|---|
| bound-arc | 1000 … 20000 | `0 / 0 / 0` on all ten cells |
| path-interface | 750 … 10000 | `1 / 2 / 1` on all but one |
| path-interface | 20000 (physics) | `1 / 39 / 2` — the one cell that is not `1 / 2 / 1` |

It is **near-constant, not exactly constant**, and the N = 20000 physics cell is
the honest exception: 39 QP minors rather than 2. Its N = 20000 *neutral*
counterpart is back at `1 / 2 / 1`, so the outlier does not track problem size
alone. What does hold everywhere is the shape: one major, a handful of minors,
one or two factorizations, against a core-only route whose minors reach 17703 at
the same size and a cold route that does not finish at all.

On the bound-arc window the SQP certifies the handed-over point without building
a subproblem at all; on the path window it infers the active set from the
`(z_lower, z_upper)` pair the polish extension carries, builds one subproblem,
and certifies. That is the whole solve.

That the interior-point leg **converged on all 24 cells** (5 to 22 iterations,
N = 750 through N = 20000, up to 100000 variables) is what makes those two rows
mean anything: the point being handed over is a real solved point in every
case, not a lucky one.

Leg (c) — the same value with `extensions_` cleared, which is the core-only
shape R3 promises any engine can read — saves **the same majors** as leg (d) and
then pays for the missing activity information in QP minors. Its minor count
scales with the problem: 887 at N = 1000, 4435 at N = 5000 on the
path-interface window, against leg (d)'s constant 2.

#### The mechanism, in counters rather than in prose

The two legs differ in exactly one counter besides the QP work, and it is the
one that explains the rest. `ip_activity_inferred` is **identically 0 on every
core-only leg** — the neutral core carries no activity information, and the
engine says so — and nonzero on every polish leg. Across all measured cells the
relation is exact:

> **polish leg's `ip_activity_inferred` == core-only leg's `qp_minors` − 1**

on every path-window cell (708/709, 886/887, 1774/1775, 4434/4435, 664/665,
731/732), and both are 0 on every bound-arc cell. The QP minors the core-only
leg spends identifying the active set are, one for one, the activity facts the
polish extension already knew. The extension is not a speed-up applied to the
same work; it is that work, done once by the other engine and handed over.

That the bound-arc cells sit at 0 on both sides is the control: with no active
path row there is nothing to infer, the two warm routes become the same run, and
the margin is simply the whole of the cold solve.

#### Where core-only costs more than cold

On the physics-informed path cells the missing activity information is sharp
enough to make the core-only margin **negative in QP minors**: leg (c) costs
*more* minors than a cold solve (887 against 623 at N = 1000; 4435 against 3656
at N = 5000). Those cells start near the optimum by construction, so the
core-only seed trades an already-good primal for a marginally better one and
gives up the active-set identification that proximity was supplying for free.
The majors margin stays positive throughout; it is the minors that invert.

#### The crossover rescues a solve the cold leg loses

On `f7_n2000_path_neutral` the comparison stops being quantitative. The **cold
leg fails** — `NumericalError` after 3 majors, 40004 QP minors and 39744
factorizations — while **both warm legs converge**, the polish route in
1 major / 2 minors / 1 factorization. The frozen walk baseline records
`NumericalError` for this same cell, so the cold failure is a known property of
the problem and not an artifact of the dual-bind path.

A margin against a failed baseline is a different claim from a margin against a
successful one — it measures work burned before giving up, not work saved
reaching the same answer — so the aggregate below reports the two populations
separately and never averages across them.

#### The strongest cells are the ones with no margin at all

On the four largest path cells — N = 10000 and N = 20000, i.e. 50000 and 100000
variables — the comparison stops being a margin and becomes a difference in
kind:

- the **cold** baseline does not reach an answer inside its budget;
- the **core-only** warm start does not either;
- the **polish** route solves all four, at 1 major and 2 QP minors (39 on one).

Section 4.1b is that table. Those cells carry `absent` in every margin column,
because a margin needs a cold baseline and there is none — which means the
aggregate in section 4.1 systematically **understates** the hand-off: the cells
where the crossover matters most are exactly the ones it cannot score. The
frozen walk baseline, measured solo, puts the cold solve of those same cells at
2802 s or past its own 3600 s budget, so this is a property of the problems and
not of this artifact's deadline.

#### The reading for the currency's design

**R3's core-only guarantee is real and worth having**: every core-only leg
converged, every one saved majors, and one of them converged where cold did not.
But it is not where the value of this hand-off is. A consumer that strips
extensions to cross an engine boundary keeps the major-iteration saving and
gives up the part that made the crossover nearly free — and on a problem whose
start is already good, may pay more QP work than it would have cold.

### 4.3 Contention during the sweep, and what it does and does not touch

`box_witness.log` records that **the machine was not alone for most of this
sweep**, and the artifact states that plainly rather than burying it.

| | |
|---|---|
| samples (one a minute, whole sweep) | 139 |
| `competing=0` | 18 |
| `competing=1` | 1 |
| `competing=2` | 120 |
| first non-zero sample | 02:37:06Z, about two minutes into a sweep that ran until 04:54Z |

A build belonging to another agent working in this same repository started
shortly after the sweep began and held roughly one core for most of its
duration. The quiet gate did its job — it opened on five consecutive clean
samples — but a gate can only check the machine at the moment it starts, not
keep it clear afterwards.

**No per-cell clean/contended split is claimed.** The witness samples once a
minute; most bound-arc cells finish in under a second, so their windows contain
no sample at all and their cleanliness is simply not observable at this
resolution. Only 3 of the 24 cells have a window containing at least one sample
with every sample in it at `competing=0`. An earlier draft of this section
asserted a 12-cell clean window; that was derived by assuming contention, once
begun, was continuous, which the log disproves — 16 clean samples occur after
the first dirty one. The assumption was wrong and the split it produced is not
reported.

**What contention does not touch.** Counters at `MKL_NUM_THREADS=1` are
scheduling-invariant — exactly the property §7 relies on when it permits
counter-asserting replays to co-run at all. Every counter column in these CSVs
therefore stands as measured, and the counters are this artifact's whole
asserted currency.

**What it does touch.** A `dnf_budget` outcome is wall-DEPENDENT: contention can
push a cell toward its deadline and never away from it. Under §7 such an outcome
is a CANDIDATE, not a result, until reproduced alone. Five cells carry one:
`f7_n5000_path_neutral`, `f7_n10000_path_physics`, `f7_n10000_path_neutral`,
`f7_n20000_path_physics`, `f7_n20000_path_neutral`.

**Those five are not re-run here, and the reason is evidence rather than
convenience.** The frozen walk baseline — measured SOLO under
`scripts/run_walk_census.sh`'s ratified protocol — records the cold solve of
these same cells at 2363.9 s, 2801.6 s, and its own 3600 s budget exhaustion for
the remaining three. Every one of those exceeds this artifact's 1200 s per-cell
budget outright, so the deadline outcomes are **over-determined**: a solo run
produces them too, and contention is not the discriminating factor. That is
independent corroboration, not a solo re-run, and is labelled as such. A reader
who wants the stronger claim should re-run those five alone; nothing in
section 4's conclusions depends on it, because those cells contribute no margin
either way.

---

## 5. How this artifact was produced

### 5.1 Two passes, and why there are two

**Pass 1** (binary `8bd4b9d62ef0`) ran all 24 dual-bindable cells with the legs
in the order (a), (c), (d), (b). That order was chosen on the reasoning that
only the cold baseline was expensive enough to starve the legs behind it under
the per-cell wall deadline. **On four large path cells that reasoning was
wrong**: leg (c), whose QP minors scale with the problem, exhausted the whole
1200 s budget on its own, and legs (d) and (b) never ran. The cells it cost are
precisely the ones with the most to say — leg (d) is *constant cost*, so the
largest problems are where the contrast between the two warm routes is sharpest.

**Pass 2** (binary `575331d10fa5`, commit `575331d`, in `pass2-legorder-fix/`)
re-runs exactly those four cells with the corrected order (a), (d), (c), (b):
exporter first because the warm legs stage its output, then
cheapest-and-most-informative, then the scaling leg, then the cold baseline.

Each leg is an independent solve on a fresh driver over a fresh bridge, so the
order **moves no counter** — it decides only which legs exist when a cell runs
out of budget. A pass-2 row is therefore the same measurement pass 1 would have
taken had it reached that leg.

The merge rule used in section 4 is deliberately narrow: a pass-2 row replaces a
pass-1 row **only where pass 1 recorded an absent (`dnf_budget`) status and pass
2 recorded a real one**. A completed pass-1 measurement is never overwritten.
Both passes stay on disk under their own provenance headers, so the substitution
is auditable rather than asserted.

---

## 6. Caveats on the record

- **Leg (b) is not the committed walk-corpus baseline row.** It is a cold solve
  of the same mathematics reached through the dual-bind conversion, so its
  counters may differ from `bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv`.
  What the protocol's margins are taken over is b, c and d *against each other*:
  one declaration, one conversion, one start point, one options object, three
  solves that differ only in what was staged.
- **The minor-iteration budget the walk corpus uses is not applied here**, and
  deliberately. `SqpDriver`'s budget-carrying overload takes an explicit
  `WarmStart` argument, which `refuse_two_warm_sources` refuses against a staged
  currency value — so that budget is unavailable to legs (c) and (d), and taking
  it on leg (b) alone would make the cold leg the only bounded one, which is
  exactly the asymmetry the margins must not have. All three legs are bounded by
  the same thing: the corpus's own `SqpOptions::max_iter` and
  `SqpOptions::qp.max_iter` caps. This is a real difference from the walk
  baseline's conditions and is part of why the previous bullet holds.
- **Nothing here asserts wall.** Every `wall_s` column is informational. The
  per-cell wall deadline is a budget guard, not a measurement.
- **Nothing here asserts residual byte-equality across processes.** The
  registered census-cell residual variance carry (the `ScoreModelSurface` pin's
  premise — process-invariant residual last digits — fails on at least one box
  under MKL address sensitivity) applies to residual columns in these CSVs as
  much as anywhere else. The asserted currency is counters; residuals are
  recorded for orientation.
- **The slow-`Continuation` box-health carry** stands unchanged; nothing in this
  artifact depends on it.
- **`kkt_inf_` and the other three IPM residuals are on the SOLVER's scale**,
  not the caller's, and `NaN` means UNMEASURED rather than zero — the IPM's own
  `SolveResult` convention.
- **Leg (a)'s counters are NOT commensurable with legs (b)/(d)'s.** An
  interior-point iteration and an SQP major are different units of work, and an
  IPM KKT factorization and an SQP factorization factor different matrices of
  different dimension. Leg (a) is recorded so a reader can see what the export
  cost and whether the point handed over was converged; it is not a term in any
  margin, and the protocol's asserted currency is deliberately the b/c/d
  margins alone. A cost comparison between the two engines is a different
  measurement than this one.
- **Legs (c) and (d) are the same run when the export carried no polish
  extension.** The `legs_cd_identical` column in `margins.csv` says so per cell
  rather than leaving a reader to infer it from two equal margins.

---

## 7. Reproducing

```
cmake --build build-m5-release --target hven_sqp_crossover
scripts/run_crossover_legs.sh
```

The script's defaults are the ones these rows were produced under. It waits for
the machine to go quiet before the first solve, runs the sweep as one process on
one pinned physical core with its SMT sibling idle and `MKL_NUM_THREADS=1`,
writes `box_witness.log` beside the rows, and emits `cells_not_measured.txt` at
the end. It warns on stderr if the machine was ever not alone.

The runner offers no co-run arm and no width flag: the W5 protocol declares no
co-run terms, so producing rows under conditions this README does not describe
is not something the instrument can do.
