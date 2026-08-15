# M3 Gate B — execution review request (phase B: retarget onto `hven::linear`)

**From:** the M3 implementer (hven). **To:** the SQP instance (mandatory
execution reviewer), via Grant. **Plan of record:** the reviewer-side
`docs/notes/2026-08-14-hven-m3-plan-revB.md` §3 (phase B) and §9, and — as
folded and approved — this repository's
[`docs/retarget-design-sqp.md`](../retarget-design-sqp.md), whose §11 ledger and
§11.1 gate checklist this note ticks item by item.

**Branch:** `m3` (PR #5, draft). **Phase-B range:** `b12b72b..6535566`
(7 commits; base `e6e2399`, the design-note fold that closed gate A's review).
Gate A's package is `docs/notes/2026-08-14-m3-gate-a-review-request.md`; its
review outcome (PASS, with the census adjudication accepted and the baseline
amendment deferred **to this commit**) is the postscript on that note.

---

## 1. Evidence base and commit topology

| Commit | Task | Content |
| --- | --- | --- |
| `b12b72b` | B1 | `Options` don't-write states (`std::optional<int>`), Pardiso adapter honors `nullopt`, `pardisoinit` canary, Accelerate documented-default factory, the ORDERED failed-factorize pin in `tests/linear` (same commit, per review §2.3) |
| `95bfb16` | B1 fix | propagate the canary's `iparm[9]=8` correction to four stale sites (comment/prose only) |
| `92212b9` | B2 | `DenseSymmetricFactor` grows `Triangle`, `[[nodiscard]] try_factorize`, block evidence |
| `d64f140` | B3 | the `KktFactor` lifecycle helper and `sqp_kkt_options()` factory |
| `258c337` | B3 fix | declare the `src/sqp` flag-regime boundary and the `KktFactor` analysis invariant (comments only) |
| `9d68e1a` | B4 | move the Schur border's dense factorization onto `DenseSymmetricFactor` (deletes `lapacke_shim.h`) |
| `ab8aeda` | B4 | retarget the KKT layer onto `hven::linear` through `KktFactor` (deletes `kkt_system.h`, `kkt_system_accelerate.h`, `test_kkt_system.cpp`) |
| `6535566` | B4 fix | give B4's two residual deltas their own named §11 ledger rows (docs only) |

Each task was implemented, task-reviewed, and fix-rounded before the next began;
the per-task briefs, reports and reviews are the SDD record
(`task-b{1..4}-{brief,report,review}.md`).

**Commit topology of this gate, stated plainly.** All gate-B *evidence* — the
verification battery and the 57-cell census — was produced from a **clean
configure at `6535566`**, and the census binary's compiled-in provenance stamp
reads **`6535566ce749`, with no `-dirty` suffix**. That is the gate-A hygiene
item discharged (gate A's census binary stamped `cf65a03a77eb-dirty`).

The commit that *closes* this gate necessarily lands **after** `6535566`, because
it carries this note, the census artifacts, the declared baseline amendment, the
census runner script, and the `CLAUDE.md` §7 amendment — none of which can exist
before the evidence they describe. So the stamp on every artifact names
`6535566` while the close commit is its child. This is not a stale stamp: the
close commit changes no source, no header, no test and no build input that the
census binary was built from. Its content is documentation, evidence, one shell
script under `scripts/`, one governance edit, and one declared baseline row.

**Two further commits landed between `6535566` and this close commit, and they
DO touch tests and CI.** They are the macOS lane's adjudication (§9), and they
are named here so the reader is not surprised by them:

| Commit | Content |
| --- | --- |
| `55f6cc4` | `test(sqp)`: per-backend arms and infra fixes for the five import-era macOS divergences, plus the new `docs/notes/2026-08-14-accelerate-divergence-register.md` and the workflow's step-conditioning fix |
| `0b526ea` | `fix(ci)`: capture the report-don't-assert observations, gate every lane on the Build step's outcome rather than on "nothing has failed yet", and correct the M3-4 register claim |

Their relationship to the Linux evidence in §§2-4 is stated plainly: **the Linux
battery and the census were produced at `6535566`, before both.** `55f6cc4`
changes test files, so the Linux battery was re-run on it and is unchanged
(1150 registered / 0 failed, Release and Debug — recorded in that commit's own
message); `0b526ea` touches only `.github/workflows/ci.yml` and the register
note, so it cannot move a local count at all. Neither touches a `src/` or
`include/` file, so the census binary's `6535566ce749` stamp remains an accurate
description of the engine the census measured. If the reviewer wants the census
itself re-run at `0b526ea`, that is a six-hour ask and we would rather be told
than assume.

Verification: `git status --short` was empty at the start and at the finish of
the battery; no tracked file was created, modified or deleted while evidence was
being produced.

---

## 2. Verification battery (fast half) — all seven steps PASS

Host: fedora (5800X3D), clang 22.1.8, MKL oneAPI (LP64,
`libmkl_intel_thread` + `libiomp5`), SNOPT 7 present. `-j6` for all builds.
`MKL_NUM_THREADS=1` exported for **every** solve-running step, corroborated
independently two ways (the corpus binary stamped `# MKL_NUM_THREADS: 1` into
its own provenance header, and `JetMklGuard.RestoresThreadDefaultOnExit` skipped
with "host resolves to 1 MKL thread" — a skip that only fires when MKL really
resolves to one thread).

Both `build/` and `build-debug/` were deleted entirely (`rm -rf`) and
reconfigured from their presets; neither reused an object or a cache entry.
Zero errors both configs; only the two pre-existing warning families.

### 2.1 Full ctest, both configs

| Config | Registered | Executed | Passed | Failed | Skipped | Disabled | Wall |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Release | **1150** | 1148 | **1146** | **0** | **2** | **2** | 21.4 s |
| Debug | **1150** | 1148 | **1146** | **0** | **2** | **2** | 780.3 s |

`ctest -N` reports 1150 registered in both configs. The decomposition is
identical in both: **1150 = 1146 passed + 2 skipped + 2 disabled + 0 failed**,
and ctest's own summary reads `100% tests passed, 0 tests failed out of 1148`
(1148 = 1150 − 2 permanently `DISABLED_`).

### 2.2 Count arithmetic — exact, with the reconciling term named

| Point | Registered | Δ | Evidence |
| --- | --- | --- | --- |
| Gate A (phase-A tree, SNOPT-enabled) | 1118 | — | gate-A note §"Gate A evidence" |
| M2.5 / `origin/main` merge into `m3` | **1125** | **+7** | post-merge ctest (0 failed out of 1123; 1 skipped + 2 disabled ⇒ 1125 registered), taken after the merge and before B1 |
| after B1 (fault-injection pins) | 1130 | +5 | task-b1-report.md |
| after B2 (`DenseSymmetricFactorGrowth`) | 1148 | +18 | task-b2-report.md; `ctest -N \| grep -c DenseSymmetricFactorGrowth` = 18 |
| after B3 (`KktFactor` helper tests) | 1152 | +4 | task-b3-report.md |
| after B4 (`test_kkt_system.cpp` dissolved) | **1150** | **−2** | task-b4-report.md §2 |
| **Gate B observed, both configs** | **1150** | **0** | this battery |

**1118 + 7 + 5 + 18 + 4 − 2 = 1150.** Exact; no unexplained test. We flag one
bookkeeping correction against our own brief: the brief's chain
(`1118 +5 +18 +4 −2 = 1143`) **omitted the +7** the M2.5 merge brought in from
`origin/main`. That is the single reconciling term, independently witnessed by
the post-merge ctest log and by B1's own report (which correctly states its
baseline as 1125, not 1118). The gap was in the brief's summary, not in the
tree.

Every disposition behind the −2 is enumerated in task-b4-report.md §2, including
§10 item 7's explicit **zero-delta rows** for the retargeted-in-place files
(`test_schur.cpp`, `test_eqp_solve.cpp`, `test_eqp_refine_ab.cpp`,
`test_border_ops.cpp`, `test_qp_engine_indefinite.cpp`, `test_qp_warm_start.cpp`,
`test_qp_engine.cpp`, `test_ssn_engine.cpp`). The B4 task review verified the
−2 independently: every assertion of the dissolved `test_kkt_system.cpp` has a
named successor pin (ruling (a)), with one disclosed assertion-strength residual
(M-1, §7 below).

### 2.3 The two did-not-run skips

The did-not-run set is identical in both configs and fully accounted:

- `FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas` —
  skipped in `build/`/`build-debug/` because no old seam is configured there; it
  **runs and passes** in the three-seam build (§4).
- `JetMklGuard.RestoresThreadDefaultOnExit` — skipped **precisely because
  `MKL_NUM_THREADS=1` is exported**, which this battery's own protocol mandates
  ("host resolves to 1 MKL thread; pin and restore are indistinguishable"). Gate
  A's Release run shows the same two skips for the same reason. B4's report
  recorded one skip because B4's runs did not export the variable —
  environment-conditional, not a roster change; the registered count is
  unaffected.
- The 2 disabled are the pre-existing `EqpRefinementAb.FootprintRuleProbe` /
  `.FullBattery`.

### 2.4 `ScaleF7Slow` — the phase-gate-mandated exclusion, run explicitly

`ScaleF7Slow.*` is excluded from ctest **registration**
(`tests/sqp/CMakeLists.txt`, `TEST_FILTER "-ScaleF7Slow.*"`), whose own banner
requires it to run "in the phase-gate Debug sweep and in Release CI runs of the
M3 gate". Run explicitly for this gate, both configs:

| Config | Result | Wall |
| --- | --- | --- |
| Release | **6/6 passed** | 55.0 s |
| Debug | **6/6 passed** | 1013.0 s |

(Gate A's figures for comparison: Release 6/6, 52.6 s; Debug 6/6, 915 s.
Wall-clock informational only.)

### 2.5 Self-check, float witness, bench parity

- **`hven_sqp_bench --self-check` (Release): PASSED.** Every column of the
  pinned block sits on its pin, bit-exact against `kF3n1000WarmPin`:
  `steps=6 majors=30 minors=84 factorizations=25 fact_saved=0
  full_step_majors=22 n_cold=1 n_seeded=0 n_warm=5 n_hot=0 predictor_calls=0
  border_refine=84 elastic_activations=0`. **`border_refine` = 84**, unmoved.
- **`test_schur.cpp` float-parity witness: 9/9 passed in BOTH configs**, 0
  failed (`ctest -R '^Schur\.'`). This is the witness §11.1 names:
  `cond_estimate`, `expected_neg_eigs_delta`, `nearly_singular` and the
  exact-singular behavior all come back through `DenseSymmetricFactor`
  unchanged. Nothing moved.
- **Bench parity quick pass: counter-identical, zero deltas on every counter
  column.** Run through the ordinary CSV path (not `--self-check`), so it
  exercises the production sweep plumbing the self-check bypasses:
  `--family F3 --n 1000 --arm warm --sweep 6`. All six rows `Optimal`; ledger
  sums `majors 30 / minors 84 / factorizations 25 / full_step_majors 22 /
  border_refine 84 / start-level 1-0-5-0 / predictor_calls 0`, every one on its
  pin.

  **Disclosure on what that parity is against.** There is **no committed
  `bench_scale` baseline** under `bench/baselines/` — every artifact there is an
  `hven_sqp_corpus` census CSV (different binary, different harness, different
  columns), and the cheapest of those cells is census territory. The reference
  used is therefore the harness's own documented one:
  `tests/sqp/test_warm_start_battery.cpp`'s `kPins["F3n1000"].arm[kArmWarm]`.
  We state this rather than let "bench parity" imply a baseline file that does
  not exist.

---

## 3. The census — parallel-tiered protocol

### 3.1 What changed and why

Gate A's census ran serially and took ~14.7 h. Gate B's runs under a
**parallel-tiered protocol**, approved by Grant on 2026-08-14 and declared here
because a protocol change that is not declared is exactly the kind of silent
break §7 forbids.

The argument is narrow and rests on §7's own first premise — *counters are the
asserted currency; wall-clock is informational*:

1. **The census asserts counters, not time.** The comparator compares the 13
   counter/status columns byte-for-byte and **excludes `wall_s`** by
   construction. No timing from this run is quoted anywhere as a measurement.
2. **Counters are scheduling-invariant at `MKL_NUM_THREADS=1`.** With one MKL
   thread per process and no shared mutable state between cell processes, the
   iteration sequence a cell produces does not depend on what else is on the
   machine.
3. **Contention biases in one direction only.** A co-running cell can be pushed
   *toward* a budget/deadline outcome (`dnf_budget`) — it can never be pushed
   toward a false `Optimal` or a false counter value. Every mode of failure this
   protocol introduces is therefore *conservative*: it can cost us a pass, not
   buy us one.
4. **§7's serialization clause is about wall-clock validity**, which is why the
   close commit amends it to say so explicitly rather than quietly ignoring it
   (§8.2 below).

### 3.2 The protocol

Tiers are assigned from the **frozen baseline**, not from this run's outcomes,
so the whole schedule is fixed before a single solve starts and cannot be tuned
by what the fresh run does. Every cell process is `MKL_NUM_THREADS=1` and
`taskset`-pinned to one logical CPU of a **distinct physical core** — SMT
siblings are never both used.

| Tier | Rule | Cells | Width |
| --- | --- | --- | --- |
| **T1** | everything else: solved, comfortably sub-deadline | **44** | 6-wide, core-pinned |
| **T2** | wall-sensitive: any terminating baseline cell with `wall_s ≥ 1800 s`, plus `f7_n10000_path_physics` regardless | **3** | **solo** |
| **T3** | deep-DNF: every other baseline `dnf_*` cell, each at its full 3600 s/phase budget | **10** | 5-wide |

T2's three cells, with their baseline rows: `f7_n10000_path_physics`
(`dnf_budget`, 3600.000 s — the gate-A flip cell, solo regardless of tier rule,
because an `Optimal` from it is only meaningful if it was alone on the machine);
`f7_n5000_path_neutral` (`NumericalError`, 2399.984 s); `f7_n5000_path_warm`
(`NumericalError`, 2231.393 s). Order is T1 → T2 → T3, and tier 2 starts only
after every tier-1 worker has been reaped, with tier 3 not yet started — so the
solo tier really is solo.

**The safety valve.** Any cell whose 13 asserted columns disagree with the
baseline is written to `serial_confirm_list.txt` and is **not a regression until
it has been re-run alone and still disagrees**. The runner emits that list
mechanically and prints the exact serial re-run command line for each entry.
The merged CSV carries the protocol, the topology, the tier table and the
baseline path in its provenance header, so the artifact declares the conditions
its counters were taken under.

The runner is committed as `scripts/run_walk_census.sh` in this gate's close
commit (§8.1) — the protocol stops being a scratchpad artifact and becomes
reproducible.

### 3.3 Result

**57/57 cells ran; zero runner failures; exactly ONE mismatch.** The comparator's
verdict, verbatim from
[`data/2026-08-14-m3-gate-b/compare.txt`](data/2026-08-14-m3-gate-b/compare.txt):
`57 baseline cells, 57 fresh cells, 1 mismatches`, the single entry being
`f7_n10000_path_physics` — the pre-adjudicated gate-A flip cell and nothing else.
**56 of 57 cells are byte-identical across all 13 asserted columns.** No cell
errored, no cell failed to produce a row, and the `failed/` directory the runner
writes on a nonzero exit is empty.

The one mismatch is the flip, in the expected direction:

```
base : f7_n10000_path_physics,…,dnf_budget,-1,-1,-1,-1,,-1.0
fresh: f7_n10000_path_physics,…,Optimal,8798,19311,0,2,8797;1,1.399679852e-10
```

**The licensing condition of §8.1 is SATISFIED.** Gate B's 13 asserted columns
for this cell are **byte-identical to gate A's**: `Optimal, 8798, 19311, 0, 2,
8797;1, 1.399679852e-10`. The only difference between the two reproductions is
`wall_s` — 2868.3 s here against gate A's 2813.1 s — which the comparator
excludes by construction and which is informational in both directions. Two
independent uncontended reproductions, agreeing on everything that is asserted.

**Serial-confirm list: one entry, and no serial re-run is owed.**
[`serial_confirm_list.txt`](data/2026-08-14-m3-gate-b/serial_confirm_list.txt)
contains `f7_n10000_path_physics` alone, annotated by the runner as
`[pre-adjudicated at gate A — expected flip, not a regression]`. The safety valve
exists to stop a co-run confound being called a regression; this cell was never a
regression candidate, and it was run in the **solo** tier, so there is no
contention to re-confirm away. Every other cell agreed with the baseline, so
nothing else entered the list.

Wall: **6 h 01 m 03 s** (2026-08-14T18:28:30Z → 2026-08-15T00:29:33Z), split
T1 36 m 44 s / T2 2 h 43 m 41 s / T3 2 h 40 m 38 s — the two solo/full-budget
tiers dominate, as the tier design intends. Informational only, recorded for
scheduling purposes and **not** compared against gate A's ~14.7 h serial figure
(a co-run wall and a serial wall are not comparable quantities, per the amended
§7). The full timeline is
[`run.log`](data/2026-08-14-m3-gate-b/run.log).

The `f7_n10000_path_physics` flip is **pre-adjudicated**: gate A ruled it a
wall-clock boundary flip, not a behavioral delta, and the reviewer accepted that
after independently re-deriving all four grounds. It was expected to recur here,
and the runner marks it as such in the serial-confirm list rather than treating
it as a finding.

The `pardisoinit`-defaults canary (§2.4 hazard 1) runs green in the suite in
both configs — the census's "at the inherited refinement cap" clause is carried
by the canary plus B1's don't-write states, not by assertion.

---

## 4. Golden rig — the M3 gate proper on Linux

`build-3seam/` was deleted and reconfigured from scratch (`Release`,
`HVEN_FP_MODE=SAFER_FAST`, both old-seam paths, plus
`HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM=ON`). Configure RC 0; build RC 0, 0 errors.

**Seam provenance:**

- **SQP old seam: `verified`.** The rig pins that arm to the archived
  `phase-7-close` tag (`4faa1df116da53c9dc68f36635c118f52d39d2b9`) and separately
  verifies the checkout's headers are byte-identical to the tag, which they are —
  the checkout's own HEAD has moved on, and the rig does not care, because it
  compiles the **tag's** headers. So the sqp-old arm compiles the archived
  engine's headers against the **retargeted** engine. **This is the M3
  golden-rig gate proper on Linux**, and it is the strongest single piece of
  evidence in this package: the old seam and the new engine are held against the
  same traces in the same binary.
- **psiopt old seam: `unverified`, by declared escape.** The tycho checkout's
  HEAD (`e19f9b87bec3…`) is not the pinned commit (`5b5dc736c238…`), so the
  expected `CMake Warning` fired and the escape hatch downgraded it — the same
  posture as the M2.5 merge run, where the `psiopt/` tree diff across that range
  was verified **empty**. **Declared, not hidden:** no expected table was derived
  from this configure, and the pin ruling is owed on the tycho lane before the
  rig's Mac legs (§9.5, §10).

**P-series** (`--gtest_filter='*InteriorPointTrace*'`): **30 tests = 28 passed /
1 skipped / 1 failed**, RC 1 — exactly the designed shape.

| Trace | native | native_sqp_parity | native_psiopt_parity | psiopt_old | sqp_old |
| --- | --- | --- | --- | --- | --- |
| P1 IterateLoopLifecycle | OK | OK | OK | OK | OK |
| P2 InertiaCorrectionLadderReplay | OK | OK | OK | OK | OK |
| P3 SingularVerdictAndControl | OK | OK | OK | OK | OK |
| P4 PerturbationEvidencePresenceIsBackendHonest | OK | OK | OK | OK | OK |
| P5 InertiaBeforeFactorizationIsAnExplicitState | OK | OK | OK | OK | **FAILED (by design)** |
| P6 RefinementStepEvidence | OK | OK | OK | OK | **SKIPPED (by design)** |

- The designed **skip** is P6/`sqp_old`: the old seam has no refinement-step
  readback, which trace P6 needs.
- The designed **failure** is P5/`sqp_old` — the FABRICATION finding, unchanged
  in content: the old seam zero-fills a pre-factorization inertia triple, and the
  trace refuses to accept counts with nothing behind them.

**No other failure. No real regression.** The whole rig binary was also run:
**83 tests = 76 passed + 6 designed skips + 1 designed failure** (the extra 5
skips are pre-existing old-seam capability skips in the T-series).

**`FailByDesignControl` ran and passed 3/3** in this build — the inverted guard
that would catch an adapter regressing to smoothing and silently turning the
designed failure green:
`SqpSeamStillZeroFillsItsPreFactorizationInertia`,
`SqpSeamDeclaresTheInertiaSurfaceTheFailingTraceNeeds`,
`ControlsArePresentForWhicheverOldSeamsThisBuildHas`. So the P5 failure is
confirmed live-and-genuine, not an artifact of a stale or smoothed adapter.

**Owed:** the Mac legs of the three-seam rig, which need the tycho pin ruling
first (§10). The macOS *suite* lane is green (§9); the *rig* lane on macOS is a
separate configure and is not.

---

## 5. The §11 ledger — declared at SIX rows, and why it grew from four

**This is a mandatory declaration, not a footnote.** The execution review's §2.6
checklist — folded into `docs/retarget-design-sqp.md` §11.1 — was written when
the ledger had **four** rows, and its text read "the ledger's four rows and
nothing else". **The ledger now has six.** The gate must not tick that item
as-is, and this section is the declaration of the difference.

**Provenance of the growth: the B4 task review, finding I-1.** During B4 the
implementer found two residual old-vs-new differences and reported them, by
name, as sitting inside row 4's "mechanical" license (row 4 covers *namespace and
include paths* and *backend-error message text*). The task reviewer **rejected
that framing** — "a throw type is not message text and a validated diagonal is
not an include path" — and required each to be promoted to its own named ledger
row before gate B closes. That fix landed as `6535566` (docs only), taking §11
from four rows to six and updating §11.1's own wording to "six rows".

So the growth is **reviewed, not silent**: it was found by a reviewer, ruled by a
reviewer, and both rows are named in `docs/retarget-design-sqp.md` §11 with their
mitigations. Nothing was discovered after review and slipped in.

| # | Row | Status at gate B |
| --- | --- | --- |
| 1 | Accelerate perturbed-pivots: twin's zero-pivot-count reading (and degraded `n_`) → absent evidence, consumed through the native zero class | IMPLEMENTED on the code path (§4.1 helpers, §4.2 rebuild gate's `n_zero` qualifier, retargeted Accelerate test arms). the macOS lane now executes those arms green (§9.3); the **deliberate reading of the `n_zero` branch's evidence** and the new docket entry remain owed (§9.5) |
| 2 | Pre-factorization / post-release inertia: fabricated zero/stale triple → explicit `kUnavailable` routed as `kSuspect` | IMPLEMENTED (rule 1 in both verdict helpers). Golden P5 rows and the rig adapter untouched — §4 above shows the old seam still failing exactly as designed |
| 3 | kHot reuse key gains the session/epoch conjunct (+ §7.2 usable-numerics), drops `generation` | IMPLEMENTED; **no counter moved** — `k0_reused`, `factorizations`, `symbolic_analyses` and the warm-start battery pins all pass unmodified, so §7's declared-pin-review clause was never triggered |
| 4 | Namespace/include paths; backend-error MESSAGE TEXT reaching `escape_detail` (backend code preserved) | IMPLEMENTED; no pin asserted the old text (re-verified: the suite's `escape_detail` assertions match engine-authored substrings only) |
| **5** | **Dense-border can't-happen throw types**: `std::invalid_argument` → `std::runtime_error` for dsytrs `info != 0` and dsytrf illegal-argument | **NEW at `6535566` per I-1.** Both paths unreachable with valid state (LAPACK contract); no test pins the type; the one consumer that ever cared (the predictor's degradation net) catches `std::exception` by phase for exactly this reason |
| **6** | **`SymmetricFactor::analyze()` validates the structural diagonal** the dissolved seam forwarded unvalidated | **NEW at `6535566` per I-1.** Unreachable from engine assemblies (§3.1 — they emit the diagonal unconditionally); reachable only from hand-built matrices, which is why five test fixtures gained an explicit zero diagonal. Same values, same analytic inertia, same assertions; no counter moves |

**Explicit zero-delta rows hold**: the pinned counters (`suspect_escalations`,
`symbolic_analyses`, `factorizations`, `eqp_refine_steps`, `border_refine_steps`),
the SSN structure key and `structural_hash`/`values_hash`, the dense border's
floats, Accelerate ordering (`kBackendDefault` = `SparseOrderDefault`), and the
refinement-cap effect (don't-write states + canary, landed at B1). **Nothing
outside the six rows moved.**

---

## 6. §11.1 checklist, ticked item by item

| §11.1 item | Verdict | Evidence |
| --- | --- | --- |
| **Count arithmetic with every disposition enumerated**, incl. §10 item 7's explicit zero-delta rows | ✅ **TICKED** | §2.2 above (1118 +7 +5 +18 +4 −2 = 1150, both configs); dispositions in task-b4-report.md §2; the −2 independently verified by the B4 review's ruling (a) |
| **The 57-cell census byte-identical at the inherited refinement cap**, with the `pardisoinit`-defaults canary green | ✅ **TICKED** — 56/57 byte-identical, the 57th the pre-adjudicated flip now folded into the baseline (§8.1); canary green in-suite in both configs | §3.3 |
| **`test_schur.cpp`'s float-parity witness unmoved** (`cond_estimate`, `expected_neg_eigs_delta`, `nearly_singular`, exact-singular behavior bit-identical through `DenseSymmetricFactor`) | ✅ **TICKED** | 9/9 both configs, assertion content untouched by the diff (§2.5) |
| **`suspect_escalations` pins unmoved on BOTH backends** — gate-blocking | ✅ **TICKED** — Linux verified by assertion-reading; Accelerate now **observed** on the macOS lane, green in runs 31856281528 and 31857420601 | §7 below |
| **The §11 ledger's six rows and nothing else** | ✅ **TICKED, with the 4→6 growth declared** | §5 above; the checklist's own "four rows" wording is superseded at `6535566` |
| **Clean-configure provenance stamps** on every gate-B artifact | ✅ **TICKED** | stamp `6535566ce749`, no `-dirty`; verified statically (`strings`) and live (a `--from-csv` pass wrote the stamp into an artifact header) — §1 |
| **The ordered failed-factorize pin landed in the linear suite** before §7.2's usable-numerics conjunct ships | ✅ **TICKED** | `FailedFactorizeEvidencePin.InertiaReportsNonObservedAfterAFailedFactorize` landed in B1 (`b12b72b`), in the same commit as the `Options` change, per review §2.3's ordering; the conjunct shipped later at B4 (`ab8aeda`). Ordering satisfied, and the B4 review traced the conjunct in code (ruling (e)) |

Every item is now ticked. The two that were open when this note was drafted —
the **census** (§3.3) and the **Mac half of `suspect_escalations`** (§7) — both
closed with live evidence: the census with 56/57 byte-identity plus the licensed
amendment, and the Mac half with an all-lanes-green macOS CI run. What remains
open is not a checklist item but a **ruling** (§9.4, the M3-4 question), and the
three-seam rig's Mac legs, which still need the tycho pin ruling (§10).

---

## 7. `suspect_escalations` — the gate-blocking pin

The execution review made this explicitly gate-blocking (§2.2's fold): the
section-4b suspect-stall gate **does** act on `kSuspect`, so a moved
`suspect_escalations` is a behavioral change, not a diagnostic one.

**Linux: VERIFIED ASSERTED, not merely claimed.** The B4 task review checked
that the pins are hard assertions and that the diff does not touch them:
`test_warm_start.cpp` (seven `== 0` pins — file untouched by the diff),
`test_sqp_driver.cpp` (`== 10`, `>= 1` — untouched), and
`test_qp_engine_indefinite.cpp` (`== 1` / `== 0` per arm — pins unchanged). All
pass in both configs in this gate's battery. The neighbouring pins are equally
unmoved: `symbolic_analyses`
(`WarmStart.NeverSharedSolvePaysOneSymbolicAnalysis == 1`, untouched),
`factorizations`/`k0_reused` (`test_qp_warm_start.cpp`, unchanged),
`border_refine` (`test_hs_battery.cpp` `== 3` / `== 10`), `eqp_refine`
(`test_eqp_refine_ab.cpp` `== 0`).

**Mac: OBSERVED, and green.** This item was `UNOBSERVED` when the note was
drafted; it is no longer. The macOS CI lane (`macos-clang-release`, runner class
`github-macos26-arm64`) now runs the full migrated suite to completion, and every
`suspect_escalations` pin above passes there — including the per-arm
`test_qp_engine_indefinite.cpp` pins (`== 1` / `== 0`), which are exactly the
per-backend arms this item asked for. Evidence: CI runs
[31856281528](https://github.com/GrantHecht/hven/actions/runs/31856281528)
(`55f6cc4`) and
[31857420601](https://github.com/GrantHecht/hven/actions/runs/31857420601)
(`0b526ea`), **all three lanes green in both**. The gate-blocking pin is
satisfied on both backends. §9 records what the greening cost and what it left
open.

---

## 8. The declared close commit

### 8.1 The structured baseline amendment (per gate A's ruling)

Gate A's review deferred the amendment **to this commit, structured**:
`bench/baselines/2026-08-06-corpus/walk_baseline.csv` stayed unchanged through
gate B — so that the flip was *predicted and then observed*, not observed and
then explained — and the amendment now folds into the declared gate-B close
commit **citing BOTH fresh rows as re-derivation evidence**. The amended pin
therefore arrives with two independent reproductions on the retargeted engine,
not one.

**What is amended.** Exactly one row of one file: the
`f7_n10000_path_physics` row of
`bench/baselines/2026-08-06-corpus/walk_baseline.csv`.

**From** (baseline as committed — a `dnf_budget` row whose every counter column
is a `-1` sentinel, i.e. a row that asserts nothing numerical):

```
f7_n10000_path_physics,F7,10000,path,physics,0,dnf_budget,-1,-1,-1,-1,,-1.0,3600.000000000
```

**To** (gate B's fresh row, truncated from the census CSV's 37 columns to this
file's 14-column schema — the baseline file is **not** widened):

```
f7_n10000_path_physics,F7,10000,path,physics,0,Optimal,8798,19311,0,2,8797;1,1.399679852e-10,2868.340825348
```

> Gate A's reproduction, for the record:
> `f7_n10000_path_physics,F7,10000,path,physics,0,Optimal,8798,19311,0,2,8797;1,1.399679852e-10,2813.139359661`.
> The two rows differ in `wall_s` and in nothing else.

**The two citations.**

1. **Gate A (2026-08-14, serial, alone, `MKL_NUM_THREADS=1`):** `Optimal`,
   8798 majors, 19311 minors, KKT residual 1.400e-10, wall 2813.1 s.
   Committed at `docs/notes/data/2026-08-14-m3-gate-a/walk_census_gateA.csv`.
2. **Gate B (this gate, T2 **solo**, `MKL_NUM_THREADS=1`, core-pinned):**
   `Optimal`, 8798 majors, 19311 minors, KKT residual 1.400e-10, wall 2868.3 s.
   Committed at
   `docs/notes/data/2026-08-14-m3-gate-b/`. Note the cell was assigned to the
   **solo** tier precisely so this row would be produced under the same
   uncontended conditions as gate A's — the amendment is not resting on a
   co-run row.

**The licensing condition, stated so it can fail loudly.** This amendment is
licensed **only if gate B's 13 asserted columns for this cell are identical to
gate A's**. Two independent reproductions that disagree with each other are not
a re-derivation — they are a finding, and the amendment would be withdrawn and
the cell escalated instead. **§3.3 records which of those two worlds we are in:
the columns are identical, so the amendment is licensed and lands in this
commit.**

**Declared, per CLAUDE.md §7** ("intentional breaks of a pinned/reproduced value
are declared and re-derived explicitly — never silent"): this is an intentional
break of a pinned artifact row, declared in this note, argued in gate A's note
and its review outcome, re-derived twice, and carried in a commit whose message
says so.

**Untouched by the amendment:** the baseline file's own frozen provenance header,
which carries the origin-era stamps every pinned artifact keeps (the M2
arm-label/schema-key ruling class), and all 56 other rows.

### 8.2 The `CLAUDE.md` §7 amendment

The close commit amends §7 to say what it has always meant: serialization is
mandatory for any **wall-clock-asserted** measurement; a **counter-asserting**
replay may co-run under fixed terms (one process per pinned physical core,
`MKL_NUM_THREADS=1`, wall-clock stays informational); any counter deviation under
a co-run must be **serially re-confirmed** before it is called a regression; and
the protocol must be **declared in the evidence artifact**. Grant approved the
protocol change on 2026-08-14. The exact old/new text is in the close-commit
package; `scripts/run_walk_census.sh` implements all three obligations.

We flag this as a **governance edit** for the reviewer's explicit attention. It
is the only rule in this repository the census protocol needed changed, and we
would rather have it argued than assumed.

---

## 9. The Mac leg — landed, green, with one ruling owed

This section was drafted as a list of `UNOBSERVED` slots. It is no longer that.
**The macOS lane is green**, and the greening is itself a gate-B result, so what
follows is the record of it rather than a promise.

### 9.1 What the lane actually was

The macOS lane had been red on `m3` since the SQP suite landed, with **the same
five tests failing in every completed run**. A delegated CI-forensics
investigation (read-only, evidence from GitHub Actions plus local file reads;
copied into this gate's data directory as
[`macos-ci-investigation.md`](data/2026-08-14-m3-gate-b/macos-ci-investigation.md))
established the attribution that matters for this gate:

**All five are import-era.** The failing set is byte-stable from the first run
that contains the suite (`0ba1b9f`, the phase-A review round) through `6535566`
— same five tests, same assertion lines, same observed-vs-expected values. **No
phase-B commit added, removed or moved a macOS failure.** This was the migrated
suite meeting real Apple hardware for the first time: the documented context-pin
bill coming due, not a retarget regression. The gate-A note had said the
Accelerate halves "compile and run" in CI and the gate-A review approved
"macOS watch-only until gate B" — "compile and run" was never a claim of green,
and gate B is where the watched divergence had to be adjudicated.

### 9.2 The five dispositions, and where they landed

Landed in `55f6cc4` (arms + infra) with the follow-up fix `0b526ea`:

| # | Test | Class | Disposition |
| --- | --- | --- | --- |
| 1 | `HsBattery.CrashBasisIsANullResultOnTheBattery…` | counter divergence | per-backend arm: `minors_off`/`minors_on` = **861/858** on Accelerate against MKL's 862/859. `facts_off`/`facts_on` (320/323), the seeded counts and both problem lists reproduce exactly and stay asserted on both backends. Register **M3-1** |
| 2 | `Continuation.ProbeBudgetBoundsAFailingProposal` | counter divergence, pre-declared | the per-backend arm the pin's own comment was written to receive: Accelerate **304**, MKL 303 — origin entry `D19` re-adjudicated as *a number*, not a widened range. Register **M3-2** |
| 3 | `CorpusRunnerProcess.WallDeadlineEmitsADnfBudgetRow…` | infrastructure | **not an arm.** The Apple runner solved the old fixture cell in 83.7 ms, inside the parent's 20 ms poll gap under CI jitter, so the child finished before it could be killed. Cell moved to `f7_n20000_bound_neutral` (~0.35 s of SOLVE, **4× margin**), race mechanism written down. No platform conditional, no Apple value |
| 4 | `CorpusTask6bPhaseB.TheShippedKSsnConfiguration…` | float, unpinnable | the five `{:.9e}` residual byte-compares stay asserted on MKL and become **report-don't-assert** on Accelerate. Every integer column, the status and the per-QP shape reproduce the MKL artifact exactly on Apple. **No Accelerate byte value is committed** — three CI runs disagree on `f7_n1000_path_neutral` in the 7th significant digit, so it fails the two-run bar. Register **M3-3** |
| 5 | `SsnEngineLocal.WeaklyActiveRowFinishesUncertain` | genuine behavioral divergence | portable properties asserted on both backends; the coin directions and `ssn_bulk_flips == 1` stay MKL-asserted and are held `UNOBSERVED` on Accelerate. Register **M3-4**, and **OPEN** — see §9.4 |

Two supporting facts about the evidence base. First, CI runs on real Apple
hardware **are** citable observations — the never-fabricate rule bars invented
values, not measured ones — but this lane's Accelerate session stores
`num_threads` without applying it, so nothing pins the reduction order: counters
that reproduce across runs are committable, floats need two-run byte agreement
and item 4's do not have it. Second, the lane's PCH-engagement gate, install
smoke, export contract and rig-report emit had been **silently skipped on every
`m3` run** — ordinary step short-circuiting behind the red Test step, which also
made the `if: always()` upload fail its `if-no-files-found: error`. Each now
conditions on the step it actually depends on. That is why this gate can point at
JUnit and rig artifacts at all.

### 9.3 The result

CI runs
[31856281528](https://github.com/GrantHecht/hven/actions/runs/31856281528)
(`55f6cc4`) and
[31857420601](https://github.com/GrantHecht/hven/actions/runs/31857420601)
(`0b526ea`): **all three lanes — `linux-clang-release`, `macos-clang-release`,
`windows-clang-release` — green in both runs**, with the JUnit and rig-report
artifacts now actually produced. Linux is unchanged by the macOS work: 1150
registered / 0 failed, Release and Debug.

The register itself is new and committed:
[`docs/notes/2026-08-14-accelerate-divergence-register.md`](2026-08-14-accelerate-divergence-register.md).
It numbers its own entries `M3-n` rather than continuing the origin's `D`-series,
because the origin's highest number is unknown from inside this repository and a
guessed `D20` could silently collide.

### 9.4 The one ruling owed — routed to the execution reviewer

**Register entry M3-4 is OPEN, deliberately.** On Accelerate the weakly-active
tie lands the opposite way from MKL on every coin, and two things follow that a
value cannot settle:

1. **The tied row leaves the uncertain set before the solve ends.** On MKL the
   tie ends `UNCERTAIN`; on Accelerate the default run's end state reports
   `ineq_uncertain[1] == false`. The test as rewritten asserts three portable
   properties (the weakly-active set fires; the tie is never
   inactive-and-certain; bare and safeguarded modes disagree about it) which are
   strictly weaker than "a tie must end uncertain". Is that the right final
   shape, or does the export-honesty property this leg defends need a portable
   substitute for end-state uncertainty?
2. **`ssn_bulk_flips` reads 4 on Accelerate against MKL's 1.** That assertion is
   a *claim* — "no oscillation" — not a coin call. Four flips says the Accelerate
   trajectory oscillates three extra times before settling. **Is that legitimate
   for this trajectory, or is it evidence that the oscillation guard under-damps
   on Accelerate and wants algorithmic attention?**

Both are held `UNOBSERVED` in the test until ruled on; the CI observations
(stable across all ten suite-carrying runs) are citable input. This is the only
one of the five where a design-level ruling, not just a value, is owed, and it is
the one Mac-leg item this gate hands to the execution reviewer rather than
closing.

### 9.5 Still owed on the Mac leg

Green CI does not discharge everything:

1. **The §11 row-1 verdict-consumer half**: the Accelerate arms of the
   retargeted `test_kkt_partial_solve.cpp` / `test_kkt_inertia_probe.cpp`, and
   the §4.2 rebuild gate's natively-observed `n_zero` branch — **inert on MKL by
   construction** (`zero_is_derived` is true there) and therefore with *no Linux
   coverage at all*. The lane now executes those tests; what is still owed is the
   deliberate reading of that branch's evidence rather than its incidental pass.
2. **The new docket entry** that §11 row 1 requires (precedent:
   `psiopt-accelerate-perturbed-pivots`) — named in B4's report §6 as a gate-B
   obligation. The divergence register is not that docket entry; it is the
   register the docket entry will cite.
3. **The three-seam rig's Mac legs**, which need the tycho pin ruling (§10).
4. **The retargeted `test_accelerate_probes.cpp` shim probes**, `-fsyntax-only`-checked
   on Linux against the repo stub but never *executed*.

---

## 10. Open carries

- **tycho rig-pin ruling — needed BEFORE the Mac session.** The psiopt-old arm's
  pin keeps drifting: the M2.5 re-pin named `5b5dc736`, tycho main has advanced
  to `e19f9b87`, and this gate again ran under the sanctioned
  `HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM=ON` escape. The M2.5 merge verified the
  `psiopt/` tree diff across that range is **empty**, so nothing is being papered
  over today — but a HEAD pin against a live main will keep drifting. The
  options on the tycho lane are a pinned worktree, re-pointing at HEAD, or
  relaxing the check to a tree-hash comparison. Gate B's Mac legs need the
  ruling.
- **Two uninventoried stale-comment files.** `tests/sqp/test_scale_smoke.cpp`
  (~L715-717) and `tests/sqp/test_warm_start.cpp` (L934, L944) still name the
  dissolved KKT seam in comments. Their code needed no change, and the B4 brief
  makes an out-of-inventory test edit a **spec violation**, so they were
  correctly left — the task reviewer confirmed this was "the only reading
  consistent with the brief's own enforcement clause" (ruling (g)). They are
  §8-inventory omissions, comment-only, and should be swept once the inventory
  rule no longer bars the edit. The sweep should also take
  `SchurComplement::solve`'s singular-throw message, which still names
  `LAPACKE_dsytrf` — accurate, but now describing another class's internals
  (review M-4) — and `kkt_calls.h`'s B3-era historical references.
- **Deferred minors.** 4 from the B4 task review, 7+2 from B3's, 5 from B1's,
  plus B2's non-blocking note — all recorded in the SDD ledger and the per-task
  review files, triaged at the final whole-branch review, none gate-blocking.
  The one worth the reviewer's eye is **B4 M-1**: the dissolved
  `PatternHashDetectsChange`'s final assertion re-read inertia (`n_neg == 1`)
  after a cross-pattern re-factorize; its successor
  (`KktFactor.NeedsAnalysisPreservesCallSiteCounting`) asserts the outcome
  status across the pattern change but not the inertia value. A small
  assertion-strength regression, disclosed here rather than left to be
  rediscovered.
- **Pre-existing quirk, recorded so it is not rediscovered as a regression:**
  running the whole `hven_sqp_tests` binary as ONE process fails
  `CorpusTask6bPhaseB.TheShippedKSsnConfigurationIsUnmovedByTheFourLevers` —
  cross-test in-process interaction, verified present at the B4 base
  (`258c337`) by stash-and-rerun, and out-of-contract usage: ctest runs one
  process per test, which is the asserted currency, and it is green there before
  and after.
- **New carries from the macOS work (§9), all phase-C or follow-up, none
  gate-blocking:**
  - **The origin divergence notes did not migrate.** `D14`-`D19` and `D22` are
    cited from test comments pointing at four `docs/notes/2026-07-*` /
    `2026-08-01` files that do not exist in this repository — roughly **a dozen
    dangling references**. The observations themselves are quoted in full in the
    comments that cite them, so nothing is lost to a reader; the notes are not.
    Phase-C cleanup: either migrate/recreate them or repoint every reference.
  - **The `WallDeadline` fix buys margin, not enforcement.** Moving the fixture
    to a cell with ~4× the poll interval makes the race unlikely on today's
    Apple runner; it does not make it impossible. The durable fix is to assert
    the deadline **in-child** rather than by parent poll, and that is a
    follow-up, stated so the next fast runner is not a surprise.
  - **`docs/testing.md` and `docs/ci.md` owe entries** for the per-backend arm
    convention, the divergence register's role and evidence bar, and the JUnit
    artifact the lanes now produce. Documentation debt, recorded rather than
    quietly skipped.
  - **A Linux GitHub-runner microarch flake.** Run 31803082323 (`e6e2399`, a
    docs-only commit) failed four Linux tests that pass on the same source
    immediately before and after; a second occurrence has since been seen. **Two
    occurrences on GitHub runners, zero locally.** GitHub's Linux fleet is
    CPU-heterogeneous and MKL dispatches by ISA, so this is environment variance
    — but it is also evidence that a couple of pins sit close enough to a
    numerical edge that microarchitecture alone can flip them, which is worth the
    reviewer's eye even though it is not a phase-B event.
- **Carries still open from gate A**, unchanged: configure-time `git describe`
  provenance would walk to the consumer's `.git` under `add_subdirectory`
  consumption (bench-side, low priority); the SNOPT detection block runs on
  library-only configures (cosmetic); `docs/testing.md` has no entry for the
  migrated suite yet (documentation debt, natural home once phase C settles TU
  structure). The duplicate global-scope `lapacke_shim.h` carry is **CLOSED** —
  B4 deleted the SQP copy.

---

## 11. What this gate asks for

A verdict on **Gate B**, and specifically on:

1. The **census result and its protocol** (§3) — including whether the reviewer
   accepts the co-run argument, or wants the census re-run serially.
2. The **§11 ledger at six rows** (§5), and the two new rows' mitigations.
3. The **declared baseline amendment** (§8.1), whose structure the gate-A review
   specified and whose licensing condition is stated to fail loudly.
4. The **`CLAUDE.md` §7 amendment** (§8.2) as a governance change.
5. **The M3-4 ruling** (§9.4) — the one item this gate hands over rather than
   closes. The Accelerate tie leaves the uncertain set before end state, and
   `ssn_bulk_flips` reads 4 there against MKL's 1: **legitimate trajectory
   difference, or an under-damped oscillation guard that wants algorithmic
   attention?** Every pin on that leg is `UNOBSERVED`-held until this is ruled.
6. The **remaining Mac-leg items** (§9.5) as the honest `UNOBSERVED` remainder,
   and the **tycho pin ruling** (§10) that gates the rig's Mac legs.
