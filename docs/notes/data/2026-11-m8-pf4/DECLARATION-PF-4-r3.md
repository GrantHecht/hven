# M8 PF-4 r3 -- successor of DECLARATION-PF-4-r2.md after an instrument defect at P4a (DECLARATION)

Committed on `pf4-r2` before the first r3 leg; never edited afterwards. `DECLARATION-PF-4-r2.md` and every r2 output
(`r2/gate/`, committed `0b9870d52`) are kept. A failed or void r3 leg is answered by `DECLARATION-PF-4-r4.md`.

## 0. Why a successor

The r2 chain (`r2/gate/orchestrate.out`) passed three legs:
- P1: ctest Release and Debug, "100% tests passed, 0 tests failed out of 3965" each.
- P2: replay x3, `REPLAY_AS_DECLARED`.
- P3: `I4_PASS`, the 33 movers exactly as declared, 0 WORSE; `P3_INSTRUCTIONS` hven-ipm B/A 1.001183, hven-sqp B/A
  0.591169.

It STOPPED at P4a on an instrument defect, before any process ran. `r2/runtime/analyze.py` was copied from r1's
`runtime/`, which sits five levels below the repository root, and it finds `scripts/compare_replay.py` at
`here/../../../../..`. From `r2/runtime/` (six levels down) that is `docs/scripts/compare_replay.py`. The fixture
pre-flight therefore printed `COMPARE_NOT_FOUND` and `FIXTURE_PREFLIGHT_FAIL` (exit 7). Nothing about the code or the
binaries is in question.

## 1. What changes, and what does not

- `r2/runtime/analyze_r3.py` = `analyze.py` with the one root line six levels up. Its fixture pre-flight on
  `r2/runtime/fixture/` reads total 38909, excluded 21354, filtered 17555 -> PASS.
- `r2/runtime/runtime_leg_r3.sh` and `run_walk_r3.sh` are the r1-named copies calling it.
- `r2/legs/orchestrate_r3.sh` = `orchestrate.sh` with P1-P3 commented out (their r2 results stand: same code commit and
  binaries, checked again by P0), with P4a on `run_walk_r3.sh`, and with outputs under `r2/gate-r3/`.
- Unchanged from `DECLARATION-PF-4-r2.md`: the code commit (`4a256c7a30c4d1953cc3f84fed0fac7b497436c3`), the four frozen
  binaries and their sha256, every declared mover, the criteria of P4a, P4b, P5 (R1 and R2 must-kill) and P6
  (section 4 there), and the box-lane protocol.

## 2. Order

```
bash docs/notes/data/2026-11-m8-pf4/r2/legs/orchestrate_r3.sh 4a256c7a30c4d1953cc3f84fed0fac7b497436c3 b63d68e8356291dc8db9f7d0c5ab5a06cb29551b019ee33fbdf6eab31111bb8e b55a15090390697dbca09d9a53b931da042ed4450c82aaa93667532231d164d7 9d766ea786fdbffb938120547b629177c5ff8d4309efa144194a5044246c9481 def8b04c10f70ccd5c0f3c6d4bc882409b3e103be77b6dd8a1309168011805ef 3965 > docs/notes/data/2026-11-m8-pf4/r2/gate-r3/orchestrate.out 2>&1
```

Legs in order: P4a, P4b, P5, P6, each in one box lane. The chain stops at the first failed leg.

## 3. Disclosure

CI run 44761741351 on the r2 declaration head `18d15c8d0` (code = the code commit) finished before this file:
- linux-clang-release, linux-clang-coverage and macos-clang-release: green.
- windows-clang-release: one failure, `SqpElasticWitnessStart.SawpathMajor0RungSolvesFromItsWitness`. Its off-reference
  portable contract was not met: a kOptimal answer with stationarity 8.38e-6 against 1e-6. The test was green on
  Windows in r1's CI.
A rerun of the failed job (attempt 2) was dispatched before this file and had not finished. Either way it is a CI
result, not a leg; RESULTS reports it.
