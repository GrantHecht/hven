# The Linux runner divergence register (hven)

A **divergence register entry** here records a place where the GitHub Linux CI
runner pool gives this library measurably different numbers than the pinned
local reference machine — together with what was observed, where, and what is
asserted as a result. The format follows
`2026-08-14-accelerate-divergence-register.md`; the subject differs: these are
divergences *within one platform's lane* across runner hardware, not
between backends. An entry is not a bug report against the library: every
occurrence so far happened on commits whose objects were proven byte-identical
to a previously green state.

Created 2026-08-15, when the phase-C plan's microarch-flake watch rule
(`2026-08-15-m3-phase-c-plan.md` §10 item 10) fired on the third occurrence.

## L-1 — the four-test Linux flake class

**Status: OPEN — root cause narrowed, not closed.** Not attributable to any
library change (see exoneration). Standing lane property.

**The occurrences:**

| # | Run / where recorded | Commit | Thread pin state |
|---|---|---|---|
| 1 | recorded in `2026-08-14-m3-gate-b-review-request.md` | gate-B era | lane UNPINNED |
| 2 | run `31893392460` attempt 1; task R5's report §7–8 | R5 | lane UNPINNED (diagnosis event) |
| 3 | run `31907664085` attempt 1 | `e7f89a7` (S2) | **`MKL_NUM_THREADS=1` PINNED** (`23247e3`, verified in the failing run's `ci.yml`) |
| 4 | run `31921576722` attempt 1 | `dc98d70` (S3) | PINNED |

Occurrence 4 (2026-08-15): same four tests, same assertion sites as
occurrence 3's table below; green on rerun; the commit's P-SYM was 60/60
byte-identical, so the exoneration basis holds at its strongest. Per-cell
values were not re-lifted — occurrence 3's table remains the durable
exemplar of the class; the S3 task report records both attempts. Frequency
note: two pinned-lane occurrences in one day of heavy CI traffic
strengthens the microarch/dispatch-variance hypothesis and the case for
answering the runner-pin/`-march` question at the flag-unification event.

**Why occurrence 3 changes the diagnosis.** R5 root-caused the class to the
lane's missing thread pin (threaded MKL reductions are order-dependent);
`23247e3` pinned it. The same four tests then flaked *with the pin in place*.
Thread-count nondeterminism is therefore **not sufficient** to explain the
class. The residual explanation is runner microarchitecture / MKL kernel
dispatch variance across the runner pool. Two of the failing tests compare two
solves **inside one process** and expect bit-identity, so what varies is the
arithmetic under a paired in-process A/B — consistent with an MKL dispatch that
is not stable across the two arms on some microarchitectures, and not with
process-to-process scheduling noise.

**Class scoping (corrected at S2 fix round 1 — do not quote the earlier
"every difference is last-bits" sentence):** last-bits arithmetic, **two of
whose four cells surface as discrete-state divergences** — a >3-orders
threshold miss and an integer-counter/boolean-flag flip. Last-bits arithmetic
amplified through a discrete tie-break is the plausible mechanism for both;
the per-row table records what was actually observed.

**Exoneration basis (all three occurrences):** Linux-only, never reproduced
locally, objects byte-identical across the flake (occurrence 3: P-SYM 60/60
between `9e90595` and `e7f89a7`, an object set that includes the four flaking
test TUs), and the identical commit green on rerun with only the runner
changed.

### The captured evidence — occurrence 3, run `31907664085` attempt 1

Captured verbatim before CI retention drops it (plan §10 item 10's
requirement). The four `SsnEngineLocal` expected-sides were recovered from
GitHub retention by the S2 reviewer after the local raw log was cleaned up;
this table is the durable copy.

| Test | Site | Observed |
|---|---|---|
| `SqpDriverRadius.FloorRaisesTheRestorationRequest` | `test_sqp_restoration.cpp:1211` | `(run.last_radius * 0.5) < (opts.tr_min)` — actual `4.76837158203125e-07` vs `1e-10` (the solve never reached the radius floor) |
| `B1Gate.EqualityOnlyWarmSolvesAreBitIdenticalAcrossTheRepair` | `test_b1_gate.cpp:783`, HS7 | `3.4794942110750977e-10` vs `3.4794942110708625e-10` |
| " | `test_b1_gate.cpp:782`, HS26 | `2.282201458793485e-12` vs `2.2822014587943094e-12` |
| `CorpusTask6bPhaseB.TheShippedKSsnConfigurationIsUnmovedByTheFourLevers` | `test_corpus_cells.cpp:2562/2563`, `f7_n1000_bound_neutral` | `"6.295821599e-14"` vs `"6.295816157e-14"` |
| " | `:2562/2563/2564`, `f7_n1000_path_neutral` | `"3.053661768e-10"` vs `"3.053662878e-10"`; `"1.045096208e-10"` vs `"1.045096222e-10"` |
| " | `:2565`, `f7_n1000_path_neutral` | `"4.555248530e-07"` vs `"4.555255959e-07"` |
| " | `:2566`, `f7_n1000_path_neutral` | `"1.271442172e-13"` vs `"1.271441918e-13"` |
| `SsnEngineLocal.WeaklyActiveRowFinishesUncertain` | `test_ssn_engine.cpp:1993` | `res.counters.ssn_bulk_flips` — actual `3`, expected `1` |
| " | `test_ssn_engine.cpp:2023` | `bool(bare.ineq_active[1]) != bool(full.ineq_active[1])` — actual `true` vs `true`; the two modes agreed where the fixture needs them to differ (`bare.ineq_active[1] = true`) |
| " | `test_ssn_engine.cpp:2040` | `full.ineq_active[1]` — actual `true`, expected `false` |
| " | `test_ssn_engine.cpp:2041` | `full.ineq_uncertain[1]` — actual `false`, expected `true` |

### What is and is not asserted

Nothing in the suite was weakened in response to this class: the four tests
assert their pinned values unchanged, and a flake is handled by rerun, with
each occurrence checked against the exoneration basis above before it is
called a flake (a deviation that survives that check is a regression, per
CLAUDE.md §7's serial-confirm rule). The `ci.yml` Test-step comment points
here.

**Open question with an owner:** whether the Linux lane should pin a runner
microarchitecture (or the build stop using `-march=native` on CI) so the lane
tests one arithmetic instead of a pool lottery. That interacts with the U0
flag-unification event's re-derivation and belongs to its design note, not to
a fix riding a task commit.

### Relation to the observations in this register's sibling

The Accelerate register records cross-backend divergences observed on the
macOS lane under its current configuration; entries there that re-derive under
U0's flag change are that event's concern. This register's class is orthogonal:
same backend, same platform, different runner silicon.
