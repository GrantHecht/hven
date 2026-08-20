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

**Status: OPEN — root cause narrowed twice, not closed.** Not attributable to
any library change (see exoneration). Standing lane property. **Read the
occurrence-5 AND occurrence-6 sections below before using anything above
them**: under the U0 CI flag posture the fourth member stopped being a flake at
all — it is now a stable, reproducible lane-vs-local divergence, which changes
how it must be handled — while the other three were read as retired on a single
green observation. Occurrence 6 (2026-08-19) falsifies that reading for
`SsnEngineLocal.WeaklyActiveRowFinishesUncertain`, which flaked again under the
pinned ISA and went green on a rerun of identical source. Treat the flake half
of this class as OPEN.

**The occurrences:**

| # | Run / where recorded | Commit | Thread pin state |
|---|---|---|---|
| 1 | recorded in `2026-08-14-m3-gate-b-review-request.md` | gate-B era | lane UNPINNED |
| 2 | run `31893392460` attempt 1; task R5's report §7–8 | R5 | lane UNPINNED (diagnosis event) |
| 3 | run `31907664085` attempt 1 | `e7f89a7` (S2) | **`MKL_NUM_THREADS=1` PINNED** (`23247e3`, verified in the failing run's `ci.yml`) |
| 4 | run `31921576722` attempt 1 | `dc98d70` (S3) | PINNED |
| 5 | run `31985550447` attempts 1 **and 2** | `305e5a1` (U0) | PINNED, **and the lane ISA pinned to `x86-64-v3`** |
| 6 | run `32277243150` attempt 1 (green on the next run of identical source, `32309301233`) | `772c25c` (docs-only, atop T5's `de97f5d`) | PINNED + `x86-64-v3` |
| 7 | run `32347392685` attempt 1 (**green on attempt 2, the `--failed` rerun of the same run**) | `222ed68` (docs-only, atop T6/T7/T8) | PINNED + `x86-64-v3` |

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

**ANSWERED at U0 (2026-08-16):** the design note
(`2026-08-16-m3-u0-design.md` §2) rules a fixed per-lane ISA baseline —
`-march=x86-64-v3` on the Linux/Windows lanes, `-mcpu=apple-m1` on the macOS
lane, via `HVEN_SIMD_ARCH` — with `native` remaining the local/derivation
regime. Runner-class pinning was rejected as not purchasable on hosted
runners. This removes the per-runner-codegen half of the exposure; the MKL
kernel-dispatch half (this class, L-1) remains OPEN and is unchanged by the
posture.

### What the U0 push actually showed — occurrence 5, and a narrowing

**The posture worked on three of the four tests, and turned the fourth from a
flake into a fact.**

CI run [31985550447](https://github.com/GrantHecht/hven/actions/runs/31985550447)
(commit `305e5a1`, the first push carrying `HVEN_SIMD_ARCH=x86-64-v3` on this
lane) is the first observation under the pinned ISA. Compare it against the
base commit's run
[31925472238](https://github.com/GrantHecht/hven/actions/runs/31925472238)
(`8e2cbd0`, `native`), which failed with **all four** class members:

| test | at `8e2cbd0`, `native` | at `305e5a1`, `x86-64-v3` |
| --- | --- | --- |
| `SqpDriverRadius.FloorRaisesTheRestorationRequest` | FAIL | **pass** |
| `B1Gate.EqualityOnlyWarmSolvesAreBitIdenticalAcrossTheRepair` | FAIL | **pass** |
| `SsnEngineLocal.WeaklyActiveRowFinishesUncertain` | FAIL | **pass** |
| `CorpusTask6bPhaseB.TheShippedKSsnConfigurationIsUnmovedByTheFourLevers` | FAIL | FAIL (occurrence 5) |

Three of those passes are partly re-derivation rather than posture — U0 moved
those three sites' Release pins — so the honest reading is that the two changes
together retired three of the four, not that the ISA pin alone did.

**Occurrence 5 is different in kind from occurrences 1–4, and that is the
finding.** It did **not** go green on rerun. Attempts 1 and 2 of run
31985550447 produced **bit-identical values on all ten compared columns**, so
the standing rerun-and-exonerate handling does not apply to it: this lane now
has *one* arithmetic, reproducibly, and it is not the derivation machine's.

| cell / column | derivation machine | this lane, both attempts |
| --- | --- | --- |
| `f7_n1000_bound_neutral` kkt_residual, stationarity | `6.295832335e-14` | `6.295832674e-14` |
| `f7_n1000_path_neutral` kkt_residual, primal | `3.053665099e-10` | `3.053661768e-10` |
| `f7_n1000_path_neutral` stationarity | `1.045096220e-10` | `1.045096199e-10` |
| `f7_n1000_path_neutral` dual_sign | `4.555244335e-07` | `4.555252106e-07` |
| `f7_n1000_path_neutral` complementarity | `1.271441541e-13` | `1.271441316e-13` |

Largest relative difference: **1.7e-6**. No counter, status, escape census
column or per-QP shape moved on either attempt — those stayed byte-strict
against the frozen artifact, on the lane, as they always have.

**It is not the ISA flag.** Rebuilding locally with this lane's exact
`-march=x86-64-v3` reproduces the LOCAL values — the whole 695-test SQP binary
passes unchanged under the lane's declared arithmetic. So the pinned baseline
did what §2 of the U0 design note claimed for it (one defined codegen per lane)
and the residue is the half that note said it would not close: **MKL kernel
dispatch on the runner's silicon.** Corroborating detail: `3.053661768e-10` is
*exactly* the value occurrence 3's table above captured from a diverging runner
under `native` — the same arithmetic, now arrived at deterministically.

**Class status: NARROWED, still OPEN.** The per-runner-codegen half is closed
by the posture. The MKL-dispatch half is now a stable, reproducible lane
property rather than an intermittent one — which is an improvement in
diagnosability, not a fix.

Disposition taken at U0 (a **reviewed finding**, reversible on the reviewer's
word; see the U0 delta report): the five float residual columns in
`CorpusTask6bPhaseB` are compared **relatively at 1e-5** rather than as exact
9-digit strings, with the derivation-machine strings recorded via
`RecordProperty` so nothing is lost. The justification is in the test's own
comment: these are residual norms near 1e-10 and 1e-13 formed by cancellation
from O(1) data, so their intrinsic relative accuracy is about `eps/magnitude`
— of order 1e-6 here — and the 9th significant digit was never a portable
quantity. Every integer column, the status, and the per-QP shape, which are
where this test's subject lives, remain **byte-strict on every lane**. The
sibling precedent is this same test's existing Accelerate arm, which has
reported-not-asserted those five columns since M3-3 for the same reason.

### Occurrence 6 (2026-08-19, phase-C T5) — the flake half is NOT retired

CI run [32277243150](https://github.com/GrantHecht/hven/actions/runs/32277243150)
on `772c25c` (a docs-only commit sitting on top of T5's carve `de97f5d`) failed
`linux-clang-release` on **one** class member,
`SsnEngineLocal.WeaklyActiveRowFinishesUncertain`, at the class's own
registered assertion site and with the class's own registered values:

```
test_ssn_engine.cpp:1995  res.counters.ssn_bulk_flips   Which is: 3   expected: 1
test_ssn_engine.cpp:2025  bool(bare.ineq_active[1]) != bool(full.ineq_active[1])
                          actual: true vs true
test_ssn_engine.cpp:2042  full.ineq_active[1]  Actual: true  Expected: false
```

That is occurrence 3's table row verbatim (`ssn_bulk_flips` — actual 3,
expected 1). Windows and macOS were green; the other three class members
passed.

**It went green on the next run of the same source.** Run
[32309301233](https://github.com/GrantHecht/hven/actions/runs/32309301233)
(`beac64b`, which adds only a tracked docs note on top of `772c25c`) is
**success on all three lanes**, this test included. So the standing
rerun-and-exonerate handling DOES apply to this member — unlike occurrence 5's
`CorpusTask6bPhaseB`, which reproduced across attempts and was dispositioned
separately.

**Correction to the occurrence-5 reading, which is why this is worth
recording.** That section's table shows this test passing under the pinned ISA
and concludes the posture plus U0's re-derivation "retired three of the four".
Occurrence 6 shows that for this member the retirement is **not** established:
it is still an intermittent lane flake, seen once in two runs of identical
source three days later. The narrowing stands (per-runner *codegen* is closed
by `HVEN_SIMD_ARCH`); what does not stand is treating a single green
observation as retirement of a member whose mechanism — MKL kernel dispatch on
runner silicon — the posture never claimed to close.

**T5's carve is exonerated on the strongest available basis, and it is not a
rerun argument.** `tests/sqp/test_ssn_engine.cpp.o` is OUTSIDE T5's declared
P-SYM blast radius and was proven **byte-identical** across the carve, as was
every one of the 24 pre-existing library objects it links against; the TU
defines no `SqpDriver` member and the carve moved no code it compiles. T5's own
P-CENSUS (57/57 cells, zero mismatches) and P-BENCH (36 asserted columns × 17
cells × 8 reps, all identical) show no counter moving anywhere, and the test
passes in all three local P-SUITE configurations. See
`docs/notes/2026-08-19-m3-phase-c-t5-battery.md`.

### Occurrence 7 (2026-08-20, phase-C T6+T7+T8) — same member, same values, rerun green

CI run [32347392685](https://github.com/GrantHecht/hven/actions/runs/32347392685)
on `222ed68` (the tracked battery note sitting on top of the three carves
`0a76fe0`, `a4db5f0`, `8400424`) failed `linux-clang-release` on the **same
single member** as occurrence 6, `SsnEngineLocal.WeaklyActiveRowFinishesUncertain`,
at the same three assertion sites and with the class's registered values:

```
test_ssn_engine.cpp:1995  res.counters.ssn_bulk_flips   Which is: 3   expected: 1
test_ssn_engine.cpp:2025  bool(bare.ineq_active[1]) != bool(full.ineq_active[1])
                          actual: true vs true
test_ssn_engine.cpp:2042  full.ineq_active[1]  Actual: true  Expected: false
```

Windows and macOS were green on attempt 1; the other three class members passed.
**Attempt 2 — a `--failed` rerun of the same run, i.e. the identical commit and
the identical artifacts — is success on all three lanes.**

This is the third data point on the flake half and it does not change the class
status, but it does sharpen one thing. Occurrence 6's rerun was a *different
run of different (docs-only) source*; occurrence 7's is a rerun of the **same
run**, so the only variable between red and green is the runner instance. That
is as clean a demonstration as this register is likely to get that the mechanism
is a property of the runner rather than of anything in the tree.

**Class status: unchanged — NARROWED, still OPEN.** Two consecutive T-series
pushes have now hit this member once each, which suggests a per-push hit rate
worth quantifying rather than continuing to absorb one rerun at a time. Flagged
for the orchestrator as a candidate for its own task; not fixed here.

**T6, T7 and T8 are exonerated on the strongest available basis, and it is not
the rerun.** `tests/sqp/test_ssn_engine.cpp.o` is OUTSIDE all three commits'
declared P-SYM blast radii, and it is **byte-identical across all four P-SYM
captures of this unit** — base, T6, T7 and T8 — one md5 for all four. It defines
none of the symbols any of the three carves moved. The shared P-CENSUS at the
combined head (57/57 cells, **zero mismatches**, zero cells needing serial
confirmation) and P-BENCH (36 asserted columns x 17 cells x 8 reps, all
identical) show no counter moving anywhere, and the test passes in all nine
local P-SUITE runs across the three commits — Release, Debug and no-SNOPT. See
`docs/notes/2026-08-20-m3-phase-c-t678-battery.md`.

### Relation to the observations in this register's sibling

The Accelerate register records cross-backend divergences observed on the
macOS lane under its current configuration; entries there that re-derive under
U0's flag change are that event's concern. This register's class is orthogonal:
same backend, same platform, different runner silicon.
