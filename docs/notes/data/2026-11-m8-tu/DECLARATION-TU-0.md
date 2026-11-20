# M8 TU-0 -- tuning levers as options, and validate() gaps (DECLARATION)

Committed before the first gate leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is answered by `DECLARATION-TU-0-r2.md`, with this file
and every output kept.

## 0. What this is

Spec: the TU-0 brief (nine options and the `SqpOptions::qp` validate fixes), implemented on `origin/m8` `331b31837` (I17 landed; the branch was rebased onto it, conflicts in
`sqp_options.cpp` and the options-dump pins only). Every default reproduces today's constant or branch: **no behaviour change at the default configuration.**

Options added (all `SqpOptions` unless stated; each has a NaN-rejecting `validate()` predicate, a terse doc comment, an options-dump line, and the consumers named):

| option | type, default | range | consumer |
|---|---|---|---|
| `row_scaling_power_of_two` | bool, false | -- | `compute_problem_scaling`: each row factor rounded to the nearest power of two after the clamp (column rule's rounding; objective factor not rounded) |
| `advanced.trust_region_growth` | `SqpTrustRegionGrowth`, `kRatio` | enumerators | `detail::trust_region_grows` at the accepted-step growth site |
| `advanced.ssn_farkas_residual_tol` | double, 1e-6 | (0,1) | `SsnOptions::farkas_residual_tol`: `farkas_certificate` and its two messages (IPQP's gate keeps its constant) |
| `advanced.ssn_uncertain_tol` | double, 0.1 | [0,1) | existing `SsnOptions::uncertain_tol` |
| `advanced.ssn_lm_sigma_c` | double, 1.0 | finite, > 0 | `SsnOptions::lm_sigma_c` |
| `advanced.ssn_lm_sigma_exponent` | double, 2.0 | [1,2] | `SsnOptions::lm_sigma_exponent`; `p == 2.0 ? r*r : std::pow(r, p)` |
| `advanced.restoration_elastic_seed` | bool, true | -- | the elastic exhaustion site offers no candidate at false (trial-site offer unchanged) |
| `advanced.elastic_rho0_headroom_rungs` | Index, 1 | [1,6] | `elastic_initial_rho`: the cap is `kElasticRhoMax` divided by `kElasticRhoFactor` k times |
| `advanced.adaptive_dual_regularization_max` | double, 1e-8 | finite, [1e-12, 1] | the schedule's iteration-0 value and its clamp ceiling |

`GL-R1` arms: `kRatio` is today's test; `kRatioOrFeasibilityStep` = ratio, or an accepted h-type step; `kAnyAccepted` = any accepted step; all keep the guards (no growth after a
failed-subproblem step; none unless the radius is active and finite).

validate() fixes (`SqpOptions::qp`): `dual_regularization`, `primal_regularization` finite >= 0; `feasibility_tol`, `optimality_tol` finite > 0; `schur_condition_max` finite >= 1;
`max_schur_border_dimension` >= 0 (**0 accepted**: `needs_refactorization()` refactorizes whenever the border dimension exceeds 0 and `take_latch` latches on the cap at the first pinned
variable, so a border is never used; `Tu0SchurBorder` solves Hs76 and Hs15 at 0); `working_set_algebra` an enumerator; `trust_region_radius` >= 0 or +inf, never NaN.
IpmOptions / IpqpOptions / CommonOptions: no gap found.

Also in this round (settler ruling): `scripts/check_bench_stamp_selftest.sh`'s no-git cells run under `GIT_CEILING_DIRECTORIES` so a TMPDIR inside a checkout cannot find a repository
(`BenchStampGuard.SelfTest` failed under the rule "TMPDIR in the worktree").

**Retirements (no code):** A1-R4, A1-R5, A1-R6, A7-R1, A7-R2, A7-R3, A7-R4 (pre-registered negatives C128/C134), A2-F1 (new design), A2-R1, A2-R3, A3-R2 (so A3-R5 stays conditional),
A3-R3, M7-13, M7-41.

## 1. The code and the binaries

- Base = arm A = `origin/m8` `331b3183747b5c1849651706f648de2c337ddf75` (detached worktree `../tu0-base`, `build/`). Arm B = this worktree (branch `tu0`), `build/`, `build-debug/`.
- **Code commit `0947374fef07c7085f982ecbdcf1aaaa45b6d00f`.** `src include tests bench examples bindings CMakeLists.txt cmake scripts` of the leg HEAD must equal it
  (`legs/orchestrate_tu0.sh`); commits after it add only evidence, runners and docs.
- clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST`, bench + examples ON, MKL 2026.2, AMD Ryzen 7 5800X3D, Linux x86-64. Apple/Accelerate and Windows: CI only.
- Frozen binaries (sha256, checked before the first leg): corpus A `92e3f2939970b372b3400683ddc2b2b66f06cc1346e8ecbc56e892fcd39a563f`, corpus B `2bfd9f6bdc3a12c2864105a689cc78b815b2fdb4e130550516e4eeadebf7e963`;
  lane A `9d766ea786fdbffb938120547b629177c5ff8d4309efa144194a5044246c9481` (pin = arm A), lane B `3e14366fa060be9dced8f97ddb078b045551eb10c1be1ec950985efe6fb3d10a` (pin = the code commit).
  Lanes: the private clone `.scratch/hb` (never pushed) at hven-bench `main` `e03cbad456d0c93fa543f5575bead45977101277` + `legs/hb_patch_never.py` (the `never-` label prefix), branches `tu0-A` / `tu0-B`
  (`legs/train_setup.sh`).
- Instruments: copies of I17's `legs/{build,ctest,replay,train_enum,cell,install_smoke}.sh` (env-driven; `env.sh` re-pointed), `train_setup.sh` (re-based on hven-bench main), `train_check.py`
  (the nine option keys in place of I17's four; no ledger movers), `mutate_tu0.py`, `mutations_tu0.sh`, `orchestrate_tu0.sh`.

## 2. Disclosures: what ran before this file (none is a leg)

- Dev runs at the pre-rebase code (base afc9744bf): Release ctest 3953 tests, one failure (`BenchStampGuard.SelfTest`, TMPDIR-in-repo; fixed above); the new tests pass.
- Builds of `build`, `build-debug`, `../tu0-base/build` and the lane setups ran under the box lock, except one final `ninja` in `build-debug` run once outside the lock to check the tree was current
  (it linked three bench executables, about a minute; the Debug tree's test executables were built under the lock).
- The Release tree on this code commit: a full build ("100%" reached); not yet run under ctest at this code commit.

## 3. Expected movers and pins (declared)

**Behaviour under the default configuration: none.** Replay: 0 differences. TRAIN (238 cells x {D, K}, `movers-train.txt` is empty): every row equals arm A in every key except
`options_label` and `hven_pin_sha`; the `.optdump` of a hven-sqp row gains exactly nine lines (the nine options at their defaults: `row_scaling_power_of_two = false`,
`advanced.trust_region_growth = kRatio`, `advanced.ssn_farkas_residual_tol = 1e-06`, `advanced.ssn_uncertain_tol = 0.1`, `advanced.ssn_lm_sigma_c = 1`, `advanced.ssn_lm_sigma_exponent = 2`,
`advanced.restoration_elastic_seed = true`, `advanced.elastic_rho0_headroom_rungs = 1`, `advanced.adaptive_dual_regularization_max = 1e-08`), all else byte-equal; an ipm row's optdump is byte-equal.

P4 (the library-behaviour proof) is carried by P2 and P3: at defaults the counters, statuses and objectives are byte-identical, and the replay and TRAIN rows read exactly those.

**ctest: 3982** (Release and Debug on the code commit, `ctest -N`; the base reads 3956).

**Test pins re-derived (declared changes):** `SqpOptions` arity 18 -> 19; `SqpAdvancedOptions` arity 29 -> 37; `kSqpDumpLines` 90 -> 99; `sizeof(SqpOptions)` 664 -> 728; `sizeof(SqpAdvancedOptions)` 368 -> 432.

## 4. The legs and their criteria (fixed here, before data)

All under `flock /tmp/box-build.lock`, serial, one process at a time; `pgrep -a hven_bench_cell` empty first; `MKL_NUM_THREADS=1`; wall-clock is not asserted (CLAUDE.md section 7); HOLDOUT is never run.
`legs/orchestrate_tu0.sh <code> <base> <corpus A> <corpus B> <lane A> <lane B> 3982` runs them in order and stops at the first failure; outputs under `gate/`.

| leg | instrument | criterion |
|---|---|---|
| P0 | orchestrate_tu0.sh | code paths of HEAD == the code commit and clean; base worktree's code paths == `331b31837`; the four binary sha256 match; the three build trees are up to date |
| P1 | `ctest.sh` Release, Debug (arm B) | each log reads "100% tests passed, 0 tests failed out of 3982" |
| P2 | `replay.sh` arm A, arm B | 27 cells x walk, ssn, ipm; `scripts/compare_replay.py` A vs B reads "differences: 0" in each of the three, exact |
| P3 | `train_enum.sh` x2 + `train_check.py` | 238 cells x D, K on each arm; `TU0_TRAIN_PASS`: no missing or capped row, no mover, optdump as section 3 |
| P4 | -- | carried by P2 + P3 (section 3) |
| P5 | `mutations_tu0.sh` | control clean; every mutation of section 5 fails at least one pinning test (`MUTATIONS_AS_DECLARED`); the tree is restored and rebuilt |
| P6 | `install_smoke.sh` | install smoke and export contract rc 0 on the default and the clang++ front ends |
| CI | `gh workflow run ci.yml --ref tu0` | all four lanes green on the pushed head |

## 5. Mutations

`legs/mutate_tu0.py` (each anchor occurs exactly once, checked before the first rebuild). Killed iff at least one pinning test of `hven_sqp_tests` (`Tu0*`, `SqpSolverElasticSeed.*`,
`SqpSolverRestorationSeed.*`), `hven_qp_tests` (`SsnEngineLocal.*`) or `hven_solver_tests` (`Options.*`, `OptionsDump.*`) fails. All must-kill.

| id | what |
|---|---|
| G1-G4 | `kRatioOrFeasibilityStep` loses the h-type clause; `kAnyAccepted` degrades to the ratio; `kRatio` always grows; the failed-step guard dropped |
| G5 | the driver passes `kRatio` regardless of the option |
| F1-F4 | each of the four SSN forwarding lines deleted (`farkas_residual_tol`, `uncertain_tol`, `lm_sigma_c`, `lm_sigma_exponent`) |
| F5-F7 | `farkas_certificate` ignores its tolerance; the sizing ignores `lm_sigma_c`; the sizing ignores the exponent |
| E1 | `restoration_elastic_seed` not read at the elastic site |
| H1, H2 | the headroom loop runs once; the option not passed to `elastic_initial_rho` |
| M1, M2 | the adaptive ceiling constant at iteration 0; in the clamp |
| R1-R3 | power-of-two rounding skipped for equality rows; for inequality rows; the driver does not set the rule's flag |
| V1-V6, V8, V10-V15 | each `validate()` condition replaced by `false`: `qp.dual_regularization`, `qp.primal_regularization`, `qp.feasibility_tol`, `qp.optimality_tol`, `qp.schur_condition_max`, `qp.max_schur_border_dimension`, `qp.trust_region_radius`, `ssn_farkas_residual_tol`, `ssn_uncertain_tol`, `ssn_lm_sigma_c`, `ssn_lm_sigma_exponent`, `elastic_rho0_headroom_rungs`, `adaptive_dual_regularization_max` |
| V7, V9 | the `enum_in_range` calls for `qp.working_set_algebra` and `advanced.trust_region_growth` disabled |

## 6. Order, STOP and ON FAIL

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-tu/legs/orchestrate_tu0.sh 0947374fef07c7085f982ecbdcf1aaaa45b6d00f 331b3183747b5c1849651706f648de2c337ddf75 92e3f2939970b372b3400683ddc2b2b66f06cc1346e8ecbc56e892fcd39a563f 2bfd9f6bdc3a12c2864105a689cc78b815b2fdb4e130550516e4eeadebf7e963 9d766ea786fdbffb938120547b629177c5ff8d4309efa144194a5044246c9481 3e14366fa060be9dced8f97ddb078b045551eb10c1be1ec950985efe6fb3d10a 3982 \
  > docs/notes/data/2026-11-m8-tu/orchestrate.out 2>&1   # leg outputs under gate/
```

A failed leg stops the chain; the fix lands and the declaration is re-derived as `DECLARATION-TU-0-r2.md`. The branch is pushed (never `m8`) after the chain passes; CI on the pushed head
must show all four lanes green, and a red lane is a successor round.
