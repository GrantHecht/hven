# M8 sparse-equil v4 r3 -- the continuation after r2 run 1's void P4 (DECLARATION-SEQ4-r3)

Successor of `DECLARATION-SEQ4-r2.md` (kept unedited, with every output of its run 1: `r2/orchestrate.out`, `r2/suites/`,
`r2/replay/`, `r2/train/train_check.txt`, commit a79ad27ff). Committed before any r3 leg; never edited afterwards.

## 0. What happened

r2 run 1 (2026-11-19 03:20 -> 03:50, under the box lock) passed P0 (code identity: the declared trees, the frozen binaries),
P1 (Release and Debug "100% tests passed, 0 tests failed out of 3909"), P2 (REPLAY_AS_DECLARED, 18 comparisons, 0 differences)
and P3 (I4_PASS: every B row equals the r2 dev row; the movers are exactly the declared 9; none missing or capped), and
stopped at P4 BEFORE ANY MEASUREMENT: `r2/runtime/analyze_r11.py`, copied from v4's `runtime/` one directory deeper, counted
five levels up to the repo root (`COMPARE_NOT_FOUND .../docs/scripts/compare_replay.py`) and the fixture pre-flight failed
closed; the copy also lacked `analyze_r5.py`, which `runtime_leg.sh` calls per sample. No code is wrong; the instrument is.

## 1. The fix (docs only; the code commit, the binaries and every criterion are r2's)

`r3/runtime/` = r2's runtime runner with the repo root DERIVED (`git rev-parse --show-toplevel`) in `analyze_r11.py` and in
`analyze_r5.py` (now copied from v4's `runtime/`), the evidence directory re-pointed to `r3/runtime/`; nothing else. The
fixture pre-flight reads PASS on the fixed copy (total 38909, excluded 21354, filtered 17555, as expected).

## 2. The legs

`r3/legs/orchestrate.sh`: r2's P0 verbatim (the code commit `4d70afbd0`, the trees of DECLARATION-SEQ4-r2 section 6, the
untouched paths equal 44f74446f, the binaries of `r2/FROZEN.txt`), then:

| leg | instrument | criterion (r2's, unchanged) |
|---|---|---|
| P4 | `r3/runtime/run_walk.sh` | serial and solo under the lock; THROTTLE 0; counter identity on every (round, cell); B/A instructions:u within [0.995, 1.005] |
| P5 | `r2/legs/mutations.sh` -> `r3/mutation/` | control clean; Mt1 Mt2 Mt3 Me1 Me2 Mm1 Mm2 Mm3 Mm4 Mi1 Mr1 Mr2 Mb1 Md1 Md2 each KILLED; Me1 killed by `QpPlatformRetake.LakesQp0EscalatesOnTheEngineRefusal` too; tree restored |
| P6 | `r2/legs/install_smoke.sh` -> `r3/install-smoke/` | both front ends and both export-contract checks rc 0 |

r2 run 1's P1, P2 and P3 STAND: same code commit, same frozen binaries, criteria met and committed; they are not re-run.
Protocols as r2's. The chain stops at the first failed leg; a successor `DECLARATION-SEQ4-r4.md` after a fix.

```
flock /tmp/box-build.lock setsid -w bash docs/notes/data/2026-11-m8-sparse-equil-v4/r3/legs/orchestrate.sh 4d70afbd025889a0b845a2902863edd17a5ecc3f 078fe2984bb101462450ce4888f387a02de9f18fd51a4729d4f90b5780ba19ab 003806283cd4666bc915ea18ecf52d9005b92846c3309852f2653eb969c2d8ed 5e9d20cf4fe693ba53eeb6daa39cf8f4d8e7a25777f846c17e9420f802d4fec6 9533b0dbc2bb2c9b057360dede7fea7f6df8baa14e2fea196e09d53d50d53f5d 3909 > docs/notes/data/2026-11-m8-sparse-equil-v4/r3/orchestrate.out 2>&1
```

Then CI on all four lanes (`gh workflow run ci.yml --ref sparse-equil-v4`).
