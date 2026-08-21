# M3 Gate-B verification battery — fast half

**Tree:** `/home/ghecht/Projects/hven`, branch `m3`, HEAD **6535566** (clean at
start and at finish; `git status --short` empty, no tracked file modified, no
commit made).
**Date:** 2026-08-14. **Host:** fedora, clang 22.1.8, MKL oneAPI (LP64,
`libmkl_intel_thread` + `libiomp5`), SNOPT 7 present.
**Parallelism:** `-j6` for all builds. **`MKL_NUM_THREADS=1` exported for every
solve-running step** (see §7).
**Log root:**
`/tmp/claude-1000/-home-ghecht-Projects-hven/befb7a32-f021-43a2-b05c-b078913d7b67/scratchpad/`
(all gate-B files prefixed `gateB-`).

Result: **all seven steps pass.** No real regression found. Two bookkeeping
anomalies are recorded in §8; neither is a behavioral delta.

---

## Step 1 — CLEAN configure + build, both configs — **PASS**

The pre-existing cache files were saved BEFORE deletion so the options were
recoverable:

- `gateB-CMakeCache-release-saved.txt` (copy of the old `build/CMakeCache.txt`)
- `gateB-CMakeCache-debug-saved.txt` (copy of the old `build-debug/CMakeCache.txt`)

Both recovered caches showed a plain CMakePresets configure, so both configs
were reconfigured from the presets that produced them:

| Config | Command | Log | RC |
| --- | --- | --- | --- |
| Release | `rm -rf build && cmake --preset linux-clang-release` | `gateB-configure-release.log` | 0 |
| Debug | `rm -rf build-debug && cmake --preset linux-clang-debug` | `gateB-configure-debug.log` | 0 |
| Release build | `cmake --build build -j6` | `gateB-build-release.log` | 0 |
| Debug build | `cmake --build build-debug -j6` | `gateB-build-debug.log` | 0 |

Both build dirs were deleted entirely (`rm -rf`) and regenerated from scratch;
neither reused any object or cache from the previous tree state.

**Warnings:** zero errors both configs. Release carries only the two
pre-existing warning families (1 gtest-internal `-Wcharacter-conversion` in
`gtest-printers.h`; 6 `-Wpotentially-evaluated-expression` in
`tests/interior/test_solver_interface_adapter.cpp`). Nothing new.

Binaries produced in `build/`: `libhven.a`, `hven_tests`,
`hven_fault_injection_tests`, `hven_interior_tests`, `hven_sqp_tests`,
`hven_golden_rig{,_audit,_report}`, `hven_align_probe`, and the six bench
binaries (`hven_sqp_bench`, `hven_sqp_corpus`, `hven_sqp_f7_cold`,
`hven_sqp_snopt_f7`, `hven_sqp_tau_bar_sweep_probe`,
`hven_sqp_ssn_safeguard_probe`). Note `HVEN_BUILD_BENCH` is `OFF` in both
caches and the bench binaries still build — that is by design (root
`CMakeLists.txt` L536-543: bench builds whenever the suite does, because the
suite's `CorpusRunnerProcess.WallDeadline*` subprocess tests invoke
`hven_sqp_corpus`).

### Provenance stamp — the gate's own requirement

Gate A's census stamp read `cf65a03a77eb-dirty`. The stamp is resolved at
CONFIGURE time from `git describe --always --dirty --abbrev=12`
(`bench/CMakeLists.txt` L70-85) and compiled into `hven_sqp_corpus` as
`HVEN_SQP_CORPUS_GIT_DESCRIBE`.

**Observed stamp string: `6535566ce749`** — 12-hex abbreviation of 6535566,
**no `-dirty` suffix**. Verified two ways:

1. Statically, in the binary: `strings build/bench/hven_sqp_corpus` contains
   the literal `6535566ce749`.
2. Live, in an artifact the binary actually wrote — a no-solve `--from-csv`
   pass over the committed baseline (`gateB-stamp-probe.csv`,
   `gateB-stamp-probe.log`):

```
# hven_sqp_corpus provenance
# binary: 6535566ce749
# schema: 37
# budget_table_hash: 0x357aee91dee27391
# MKL_NUM_THREADS: 1
# host: fedora
# generated: 2026-08-14T17:41:57Z
```

The stamp names a real, pushed commit and is clean. Gate-B artifacts produced
from this binary carry a valid provenance. (`schema: 37` and
`budget_table_hash: 0x357aee91dee27391` both match the committed baselines'
headers.)

---

## Step 2 — Full ctest, both configs — **PASS**

Both runs: `ctest --test-dir <dir> --output-on-failure -j6` with
`MKL_NUM_THREADS=1` exported.

| Config | Registered | Executed | Passed | Failed | Skipped | Disabled | Wall | Log | RC |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Release | **1150** | 1148 | **1146** | **0** | **2** | **2** | 21.4 s | `gateB-ctest-release.log` | 0 |
| Debug | **1150** | 1148 | **1146** | **0** | **2** | **2** | 780.3 s | `gateB-ctest-debug.log` | 0 |

`ctest -N` reports **1150 registered in both configs** — exactly the B4
baseline. ctest's own summary line reads `100% tests passed, 0 tests failed
out of 1148` in both; 1148 = 1150 − 2 permanently `DISABLED_`, and the 2
skipped tests count inside the 1148 as not-failed. So the full decomposition
is **1150 = 1146 passed + 2 skipped + 2 disabled + 0 failed**, identical in
both configs.

The did-not-run set is identical in both configs:

- `FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas` (Skipped — no old seam configured in `build/`/`build-debug/`; it runs and passes in the 3-seam build, see §6)
- `JetMklGuard.RestoresThreadDefaultOnExit` (Skipped — see §8 anomaly 2)
- `EqpRefinementAb.FootprintRuleProbe` (Disabled, pre-existing)
- `EqpRefinementAb.FullBattery` (Disabled, pre-existing)

### Count arithmetic

| Point | Registered | Delta | Evidence |
| --- | --- | --- | --- |
| Gate A (phase-A tree, SNOPT-enabled) | 1118 | — | `ctest-gateA-release.log`; gate-A note L85 |
| **M2.5 / `origin/main` merge into `m3`** | **1125** | **+7** | `ctest-postmerge-release.log`: `0 tests failed out of 1123`, 1 skipped + 2 disabled ⇒ 1125 registered |
| after B1 (fault-injection tests) | 1130 | +5 | task-b1-report.md L131 ("1125 baseline + 5 new, exact arithmetic") |
| after B2 (`DenseSymmetricFactorGrowth`) | 1148 | +18 | task-b2-report.md L225/L245; `ctest -N \| grep -c DenseSymmetricFactorGrowth` = 18 |
| after B3 | 1152 | +4 | task-b3-report.md L67 |
| after B4 (`test_kkt_system.cpp` dissolved) | **1150** | **−2** | task-b4-report.md L134 |
| **Gate B observed, both configs** | **1150** | **0** | this run |

`1118 + 7 + 5 + 18 + 4 − 2 = 1150`. **Exact, no unexplained test.** The
briefing's chain (`1118 +5 +18 +4 −2 = 1143`) omits the **+7** the M2.5 merge
brought in from `origin/main` — that is the one reconciling term, and it is
independently witnessed by the post-merge ctest log (1125, taken before B1
started). See §8 anomaly 1.

---

## Step 3 — `bench_scale --self-check` (Release) — **PASS**

`MKL_NUM_THREADS=1 ./build/bench/hven_sqp_bench --self-check`, RC **0**.
Log: `gateB-selfcheck-release.log`.

```
self-check: F3 n=1000 arm=warm (full_step ON) vs tests/test_warm_start_battery.cpp kPins["F3n1000"].arm[kArmWarm]
  reached_p1=true (want true)
  steps=6 (want 6)  majors=30 (want 30)  minors=84 (want 84)
  factorizations=25 (want 25)  fact_saved=0 (want 0)  full_step_majors=22 (want 22)
  n_cold=1 (want 1)  n_seeded=0 (want 0)  n_warm=5 (want 5)  n_hot=0 (want 0)
  predictor_calls=0 (want 0)  border_refine=84 (want 84)  elastic_activations=0 (want 0)
SELF-CHECK PASSED
```

Every column sits on its pin; the whole block is bit-exact against
`kF3n1000WarmPin` (`bench/bench_scale.cpp` L620-643).

**`border_refine` observed = 84** (want 84).

---

## Step 4 — `test_schur` float-parity witness — **PASS**

`test_schur.cpp` is a source of `hven_sqp_tests` (`tests/sqp/CMakeLists.txt`
L36), so its "target" at ctest level is the `Schur.*` registered set. Run
explicitly in **both** configs:

| Config | Command | Result | Log |
| --- | --- | --- | --- |
| Release | `ctest --test-dir build -R '^Schur\.'` | **9/9 passed, 0 failed**, 0.09 s, RC 0 | `gateB-schur-release.log` |
| Debug | `ctest --test-dir build-debug -R '^Schur\.'` | **9/9 passed, 0 failed**, 0.11 s, RC 0 | `gateB-schur-debug.log` |

All nine: `BorderedSolveMatchesDirectFactorization`,
`RefactorizationTriggerFires`, `DropBorderMatchesDirectFactorizationOfSurvivor`,
`DropBorderThrowsOnOutOfRange`, `IndefiniteSchurSolveMatchesDirect`,
`AntiDiagonalSchurBlock`, `SingularOneByOneSchurBlockThrowsOnInertiaQuery`,
`InertiaBookkeepingPinnedVariable`, `NearlySingularOneByOneNeedsRefactorization`.

This is the witness the design note names at §8.6 / §10 item 7 and the gate-B
checklist calls "`test_schur.cpp`'s float-parity witness unmoved":
`cond_estimate`, `expected_neg_eigs_delta`, `nearly_singular` and the
exact-singular behavior all come back through `DenseSymmetricFactor`
unchanged. **Nothing moved** — no gate-blocking float delta.

---

## Step 5 — Bench parity quick pass — **PASS (counter-identical)**

**Finding on which baseline this corresponds to.** `bench_scale` has no
zero-flag default scenario: `--family/--n/--arm/--sweep/--csv` are all
required (`bench/bench_scale.cpp` L867). And every artifact committed under
`bench/baselines/` (`2026-08-06-corpus/walk_baseline.csv`,
`2026-08-08-gate-battery/{ssn_battery,walk_reswept}.csv`,
`2026-08-09-task6b-resweep/ssn_resweep.csv`,
`2026-08-09-task6b-resweep-walk/walk_resweep.csv`) is an **`hven_sqp_corpus`
census artifact** — different binary, different harness, different columns, and
its cheapest cell is N=1000 at a 900 s/phase budget, i.e. census territory,
which this battery is explicitly told not to run. **There is no committed
bench_scale baseline row under `bench/baselines/`.**

The bench harness's own docs name the reference it does have: `bench_scale`'s
default scenario is the F3/n=1000/warm sweep, and its committed pinned
reference is `tests/sqp/test_warm_start_battery.cpp`'s
`kPins["F3n1000"].arm[kArmWarm]` = `{6, 30, 84, 25, 0, 22, 1, 0, 5, 0, 0, 84, 0}`
(the file banner, L108-122). So the quick screen was run against that, and
deliberately through the **ordinary CSV path, not `--self-check`** — the point
being to exercise the production sweep plumbing that step 3 bypasses.

Invocation (RC 0, `MKL_NUM_THREADS=1`):

```
./build/bench/hven_sqp_bench --family F3 --n 1000 --arm warm --sweep 6 \
    --csv <scratch>/gateB-bench-parity-F3n1000warm.csv
```

`--sweep 6` is exactly the flag that reproduces the pinned scenario's
`ContinuationOptions{}`: for F3, `dp_init = |p1−p0|/(sweep−1) = 0.5/5 = 0.1`,
the default (`bench_scale.cpp` L554-561), and `dp_max = max(0.5, 0.1) = 0.5`,
also the default (`continuation.h` L310). No other flag was passed, so every
diagnostic knob sits at its documented default.

CSV: `gateB-bench-parity-F3n1000warm.csv`. Log: `gateB-bench-parity.log`.

| Column (ledger-summed over the 6 rows) | Observed | Pinned | Δ |
| --- | --- | --- | --- |
| steps (rows) | 6 | 6 | **0** |
| major_iters | 8+7+7+6+1+1 = **30** | 30 | **0** |
| qp_minor_iters | 23+20+20+18+2+1 = **84** | 84 | **0** |
| factorizations | 8+6+6+5+0+0 = **25** | 25 | **0** |
| factorizations_saved | **0** | 0 | **0** |
| full_step_majors | 0+7+7+6+1+1 = **22** | 22 | **0** |
| start_level n_cold / n_seeded / n_warm / n_hot | **1 / 0 / 5 / 0** | 1 / 0 / 5 / 0 | **0** |
| border_refine_steps | 23+20+20+18+2+1 = **84** | 84 | **0** |
| predictor_calls | **0** | 0 | **0** |
| watchdog_restores / soc_steps / soc_applied / eqp_refine_steps | **0 / 0 / 0 / 0** | 0 | **0** |
| status, every row | **Optimal** | Optimal | **0** |
| p grid visited | 0.25, 0.35, 0.45, 0.55, 0.65, 0.75 | same | **0** |

**Counter-identical. Zero deltas on every counter column.** Wall-clock and
`peak_rss_mib` were not compared, per instruction (informational only; recorded
in the CSV for the record: 0.0006–0.019 s/solve, 12.504 MiB flat).

This is the quick screen only — the full envelope sweep and the 57-cell census
belong to later gate steps.

---

## Step 6 — Three-seam golden rig, Linux legs — **PASS (exactly the designed failure)**

`build-3seam/` was deleted and reconfigured from scratch with the same
invocation as the M2.5-merge run (recovered from
`configure-3seam-postmerge-unpinned.log` + `build-3seam/CMakeCache.txt`):

```
cmake -S . -B build-3seam -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DHVEN_FP_MODE=SAFER_FAST \
  -DHVEN_RIG_PSIOPT_SEAM=/home/ghecht/Projects/tycho \
  -DHVEN_RIG_SQP_SEAM=<origin seam checkout path — full line preserved in the scratchpad log> \
  -DHVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM=ON
```

Configure RC 0 (`gateB-configure-3seam.log`). Seam provenance as expected:

- **SQP old seam: `verified`.** `golden rig: SQP old-seam arm pinned to
  phase-7-close (4faa1df116da53c9dc68f36635c118f52d39d2b9), tree state
  verified`. The archived engine tree's HEAD has moved on
  (`4304bb8bbdc06639…`) but the rig pins to the TAG and separately verifies
  the origin seam checkout is byte-identical to it (`tests/golden_rig/CMakeLists.txt`
  L284-329), which it is. **So the sqp-old arm compiles the archived tag's
  headers against the RETARGETED engine — this is the M3 golden-rig gate
  proper on Linux.**
- **psiopt old seam: `unverified`,** by design of the escape hatch. The
  expected `CMake Warning` fired: tycho HEAD is `e19f9b87bec3…`, not the pinned
  `5b5dc736c238…`. `HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM=ON` downgrades it to a
  warning — the same posture as the M2.5-merge run. No expected table was
  derived from this configure.

Build: `cmake --build build-3seam -j6 --target hven_golden_rig`, RC 0, 0 errors
(`gateB-build-3seam.log`).

### P-series (`--gtest_filter='*InteriorPointTrace*'`)

`gateB-rig-pseries.log`. **30 tests: 28 passed / 1 skipped / 1 failed**, RC 1
(the RC is the designed failure). Exactly the expected shape.

| Trace | native | native_sqp_parity | native_psiopt_parity | psiopt_old | sqp_old |
| --- | --- | --- | --- | --- | --- |
| P1 IterateLoopLifecycle | OK | OK | OK | OK | OK |
| P2 InertiaCorrectionLadderReplay | OK | OK | OK | OK | OK |
| P3 SingularVerdictAndControl | OK | OK | OK | OK | OK |
| P4 PerturbationEvidencePresenceIsBackendHonest | OK | OK | OK | OK | OK |
| P5 InertiaBeforeFactorizationIsAnExplicitState | OK | OK | OK | OK | **FAILED (by design)** |
| P6 RefinementStepEvidence | OK | OK | OK | OK | **SKIPPED (by design)** |

- The one designed **skip** is P6/`sqp_old`:
  `traces_interior_point.cpp:288: Skipped — sqp-old@mkl does not have a
  refinement-step readback -- trace P6 needs it`.
- The one designed **failure** is P5/`sqp_old` (`GetParam() = sqp-old@mkl`) and
  it is the FABRICATION finding, unchanged in content:

```
traces_interior_point.cpp:266: Failure
Expected: (e.state) != (hl::InertiaEvidence::State::kObserved)
nothing has been factorized, so there is nothing to have observed

traces_interior_point.cpp:268: Failure
Value of: e.n_pos == 0 && e.n_neg == 0 && e.n_zero == 0
  Actual: true   Expected: false
counts with nothing behind them must stay invalid, never be zero-filled --
a zero-filled triple reads exactly like a real one
```

**No other failure. No real regression.**

### Whole-rig confirmation (extra, cheap)

The full `hven_golden_rig` binary was also run (`gateB-rig-full.log`, 110 ms):
**83 tests = 76 passed + 6 designed skips + 1 designed failure**, RC 1. The
extra 5 skips are all pre-existing old-seam capability skips in the T-series
(`T2b/psiopt_old`, `T4/psiopt_old`, `T4/sqp_old`, `T4b/psiopt_old`,
`T4b/sqp_old`). The failure is the same single P5/`sqp_old`.

`FailByDesignControl` — the inverted guard that would catch an adapter
regressing to smoothing and silently turning the designed failure green — ran
and passed **3/3** in this build (it is the one Skipped in `build/`, which has
no old seam):

- `SqpSeamStillZeroFillsItsPreFactorizationInertia` — OK
- `SqpSeamDeclaresTheInertiaSurfaceTheFailingTraceNeeds` — OK
- `ControlsArePresentForWhicheverOldSeamsThisBuildHas` — OK

So the P5 failure is confirmed live-and-genuine, not an artifact of a stale or
smoothed adapter.

---

## Step 7 — `MKL_NUM_THREADS=1` — **CONFIRMED**

`export MKL_NUM_THREADS=1` was in effect for **every** solve-running step:

| Step | Solve-running | `MKL_NUM_THREADS=1` |
| --- | --- | --- |
| 1 stamp probe (`--from-csv`, no solve) | no | yes (recorded in the CSV header: `# MKL_NUM_THREADS: 1`) |
| 2 ctest Release | yes | **yes** |
| 2 ctest Debug | yes | **yes** |
| 3 `bench_scale --self-check` | yes | **yes** |
| 4 `Schur.*` Release + Debug | yes | **yes** |
| 5 bench parity run | yes | **yes** |
| 6 golden rig P-series + full rig | yes | **yes** |

Independent corroboration that the variable really reached the process: the
corpus binary stamped `# MKL_NUM_THREADS: 1` into its own provenance header,
and `JetMklGuard.RestoresThreadDefaultOnExit` skipped with
`host resolves to 1 MKL thread` — a skip that only fires when MKL actually
resolves to one thread.

---

## §8 Anomalies (both bookkeeping, neither a regression)

**1. The briefing's count chain is missing the M2.5-merge term (+7).** The
brief gives `1118 → +5 (B1) → +18 (B2) → +4 (B3) → −2 (B4)`, which sums to
1143, not 1150. The missing term is the **+7 that the `origin/main` (M2.5/PCH)
merge brought into `m3` between gate A and B1**, independently witnessed by
`ctest-postmerge-release.log` (1125 registered, taken after the merge and
before B1) and by B1's own report, which states its baseline as 1125 rather
than 1118. With that term the arithmetic is exact: `1118 + 7 + 5 + 18 + 4 − 2
= 1150`, and 1150 is what both configs register. No test is unaccounted for.
This is a gap in the brief's summary, not in the tree.

**2. Two skips observed where B4's report recorded one.** The extra skip is
`JetMklGuard.RestoresThreadDefaultOnExit`, which calls `GTEST_SKIP()` with
"host resolves to 1 MKL thread; pin and restore are indistinguishable"
(`tests/interior/test_jet_mkl_guard.cpp:19`). It is **skipped precisely
because `MKL_NUM_THREADS=1` was exported**, which step 7 mandates. Gate A's own
Release run shows the same two skips for the same reason; B4's runs evidently
did not export the variable. Environment-conditional, expected under this
battery's own protocol, and it does not change the registered count. Decomposed
correctly the two configs are **1150 = 1146 passed + 2 skipped + 2 disabled +
0 failed**; B4's "1147 passed + 1 skipped" is the same 1148 executed tests
under a different environment.

## §9 Owed / not covered by this battery

- **57-cell census** — deliberately not run here; it runs alone after this
  battery per the gate-B sequence.
- **`ScaleF7Slow.*`** — excluded from ctest REGISTRATION by
  `tests/sqp/CMakeLists.txt` L136 (`TEST_FILTER "-ScaleF7Slow.*"`), whose own
  banner says it must run "in the phase-gate Debug sweep and in Release CI runs
  of the M3 gate". It is not part of this fast half and was not run **in this
  battery**. Gate A ran it (Release 6/6, 52.6 s; Debug 6/6, 915 s). Flagging it
  so it is not lost. **[gate-B fix round]** It was subsequently run for this
  gate by the orchestrator — separately from the battery, post-battery and
  pre-census, `MKL_NUM_THREADS=1`, machine otherwise idle, same commit
  `6535566` — and passed **6/6 in both configs** (Release 55.0 s, Debug
  1013.0 s). The captured output is committed alongside this report as
  `scalef7slow-both-configs.log`; the gate note §2.4 quotes it.
- **Mac leg** — every Accelerate-arm item in the gate-B checklist
  (perturbed-pivots verdict consumer, `suspect_escalations` per-backend arms,
  the retargeted `test_kkt_partial_solve`/`test_kkt_inertia_probe` Accelerate
  arms) is a macOS observation and remains owed.
- **tycho-side psiopt seam re-pin** — the rig's psiopt-old arm ran unpinned
  under the sanctioned escape hatch, as at the M2.5 merge. The pin ruling is
  still owed on the tycho lane.

## §10 Tree hygiene

`git status --short` empty at start and at finish. No tracked file was created,
modified, or deleted; nothing was staged or committed. The only writes were to
the scratchpad, to the untracked build directories (`build/`, `build-debug/`,
`build-3seam/` — all gitignored), and to this report under `.superpowers/`
(gitignored, `.gitignore:4`).
