# M8 DN-3 -- the flip and its I set (DECLARATION-r5)

Successor of `DECLARATION-r4.md` (commit `b5af742de`), which stays frozen beside this file with every r4 output under
`r4/` (record commit `63bf818c5`). Committed before r5's first leg; never edited afterwards. Criteria are never changed
after data. A failed or void leg is answered by `DECLARATION-r6.md`, with this record kept.

## 0. What happened under r4 (the record, kept)

`r4/orchestrate.out`, 06:33-06:58, on the flip commit `2d6d89cd0`:
- **F1 MET**: Release and Debug "100% tests passed, 0 tests failed out of 3633" (`r4/suites/`).
- **I1 MET**: 27 cells x walk / ssn / ipm, three times: 0 differences in all 9 comparisons, 75 columns (`r4/replay/`).
- **I3a MET**: the kNever dumper's iparm dump byte-identical to the B8-riders pin (sha256 `34bb45c0...`, `r4/iparm/`).
- **I3b as stated** (reported): 12 configurations `status=optimal iterations=10 calls=0`; `parallel_solve=2`, `cnr=on`,
  `cnr=on+threads=2` REFUSED, 0 calls.
- **I3c FAILED, the orchestration stopped** (`r4/interior/`): "unmoved" and "promoted" each read 26 differences, every
  one a residual column of one of three small IPM fixtures -- `hs071_x1_fixed` 23 (seven treatments/variants),
  `infeas2_spike` 2 (`MakeParameter/stalled`: kkt_inf 3.7e-9 vs 0, econ_inf 0.0625 vs 0.125), `infeas2_stationary` 1
  (`MakeParameter/resto_infeasible` econ_inf 4.707e-14 vs 4.730e-14). No status or iteration column differs, and no
  row of any `f7_*` cell differs. r4 (and design §6) assumed every interior-leg cell is F7, above T; these three are
  below T_IPM, so the flip moves them by construction -- the same class of mover I4 asserts on TRAIN, not the
  above-threshold side-factor mover r0's S-I guards against. **F3, I4, I2 did not run.**

## 1. What r5 changes (nothing else)

- **Code head `297649961a3b794dab7d7d38e993a1475559823d`**: the flip commit `2d6d89cd0` + one bench-only commit,
  `bench/ipm_corpus_leg.cpp`'s pre-flip dense pin `HVEN_LEG_IPM_PRE_DN3_DEFAULT` (writes `linear.dense_factorization
  = kNever` in the interior leg; the shape of M7 W2 D1's `HVEN_LEG_IPM_PRE_D1_DEFAULTS`; stamped in the CSV's levers
  comment line as `pre_dn3_dense_pin=on|off`). `src/`, `include/`, `tests/` are byte-identical to the flip commit
  (checked by the orchestrator), so the library, the test suites and the lane binary are the flip commit's.
- Corpus binary rebuilt and frozen (`r5/legs/prebuild.sh`, bench stamp `OK (297649961a3b)`):
  `build-flip/flipbin/hven_sqp_corpus` sha256 `12cd2b9f95074d73e55e3af279b1c77e6e21ff34683a534c2cc698ea52e6e03c`
  (r4's `7e41142f...` kept at `build-flip/flipbin-r4/`). Lane binary: r4's, sha256 `7ee7b5e1...` (pin `2d6d89cd0`).
- **I3c is re-cut** (`r5/legs/interior.sh`; compare_replay.py byte-exact, no `--residual-gate`):

| id | comparison | criterion |
|---|---|---|
| I3c-N1 | the pinned (kNever) 27-cell run vs `interior-r2/interior-B.csv` (the head `96ac1a27d`) | 0 differences |
| I3c-N2 | the pinned baseline-invocation run vs `bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv` | 0 differences |
| I3c-A1 | the default (kAuto) 27-cell run vs r4's `r4/interior/interior-flip.csv` | 0 differences (the movers reproduce) |
| I3c-A2 | the default 27-cell run vs `interior-r2/interior-B.csv` | every difference on a cell of {hs071_x1_fixed, infeas2_spike, infeas2_stationary} (`r5/legs/mover_check.py` MOVERS_AS_DECLARED): no f7 row moves |
| I3c-A3 | the default baseline-invocation run vs r4's `r4/interior/interior-flip-baseline.csv` | 0 differences |

  The mover set and r4's flip rows were SEEN before this file (section 0); that is why A1 and A3 bind r5's rows to
  r4's recorded ones rather than to anything new, and N1 / N2 prove the pre-flip rows still reachable byte for byte.
- **The interior baseline is re-derived** (declared, CLAUDE.md section 7): if every r5 leg passes, r5's default
  baseline-invocation CSV (`r5/interior/interior-default-baseline.csv`) is promoted, byte-copied, to
  `bench/baselines/2026-11-m8-dn3-flip-ipm-leg/interior_baseline.csv`, with a README naming this declaration and the
  three moved cells. `bench/baselines/2026-11-m8-b7-ipm-leg/` stays frozen (it is the kNever pin's reference).
- Every other leg is r4's instrument and criterion, unchanged, re-run in full on the r5 head (outputs under `r5/`):
  F1 (`r4/legs/ctest.sh`), I1 (`r4/legs/replay.sh`, now on the r5 corpus binary), I3a/I3b (`r4/legs/iparm.sh`), F3
  (`r4/legs/install_smoke.sh`), I4 (`r4/sl/flip_enum.sh` + `flip_identity.py`, the lane binary above), I2
  (`r4/legs/census.sh`, the r5 corpus binary). r4's protocols, STOP / ON FAIL (section 5), NOT RUN (section 6) and
  registered items (section 7) stand, plus: the interior leg's three small fixtures are flip movers (section 0).

## 2. Disclosures

No leg, cell or run of the r5 binaries was taken before this file, other than `r5/legs/prebuild.sh` (builds and the
stamp check). `r5/legs/mover_check.py` was checked against r4's recorded report (MOVERS_AS_DECLARED with the three
cells; UNDECLARED_MOVER with one of them dropped) and against r2's 0-difference report.

## 3. Order

```
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c/docs/notes/data/2026-11-m8-dn3/r5/legs/orchestrate-r5.sh 297649961a3b794dab7d7d38e993a1475559823d 12cd2b9f95074d73e55e3af279b1c77e6e21ff34683a534c2cc698ea52e6e03c 7ee7b5e1cd8567f19ff0048e4e6ff523b5beb03d16a13dfbe5de891eaf27d6ab > /home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c/docs/notes/data/2026-11-m8-dn3/r5/orchestrate.out 2>&1
```

Legs in order: F1 (rel, dbg), I1, I3a/I3b, I3c, F3, I4, I2. ON FAIL: stop, commit the evidence, report; no re-run,
no criterion change after data.
