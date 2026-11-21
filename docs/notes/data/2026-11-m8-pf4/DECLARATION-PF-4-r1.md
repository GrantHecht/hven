# M8 PF-4 r1 -- successor of DECLARATION-PF-4.md after an instrument defect at P4a (DECLARATION)

Committed before the first r1 leg; never edited afterwards. `DECLARATION-PF-4.md` and every run-1 output (`gate/`) are kept.
A failed or void r1 leg is answered by `DECLARATION-PF-4-r2.md`.

## 0. Why a successor

Run 1 (`gate/orchestrate.out`, committed `413a9db60`) passed P1 (ctest Release and Debug, 3937 each), P2 (replay x3,
`REPLAY_AS_DECLARED`) and P3 (`I4_PASS`, 33 movers exactly as declared, 0 WORSE; `P3_INSTRUCTIONS` hven-ipm B/A 1.000024,
hven-sqp B/A 0.591225), and STOPPED at P4a on an instrument defect: `runtime/runtime_leg.sh` (WA-1's, re-pointed) calls
`$HERE/analyze_r5.py --excluded-count` after each measured process, and only WA-1's `analyze_r11.py` was copied here (as
`runtime/analyze.py`). The first measured process's call failed, the leg declared itself VOID (`PARSE_ERROR ... LEG VOID`,
exit 6), and the analysis found 3 pairs missing. No measurement was taken; nothing about the code or the binaries is in
question.

## 1. What changes, and what does not

- `runtime/runtime_leg_r1.sh` = `runtime_leg.sh` with the one call renamed to this directory's `analyze.py`, which carries
  the same `--excluded-count` mode (checked on run 1's one symbol table, `gate/runtime/walk/syms/`: prints 0, rc 0).
  `runtime/run_walk_r1.sh` = `run_walk.sh` calling it. `legs/orchestrate_r1.sh` = `orchestrate.sh` without P1-P3 (their
  run-1 results stand: same code commit, same binaries, checked again by P0) and with P4a on `run_walk_r1.sh`; outputs
  under `gate-r1/`.
- Unchanged from `DECLARATION-PF-4.md`: the code commit (`ea71502aa7789f27931e63d1ab6d10fa02b20e8b`), the four frozen
  binaries and their sha256, every declared mover, and the criteria of P4a, P4b, P5 and P6 (section 4 there).

## 2. Order

```
setsid -w flock /tmp/box-build.lock bash docs/notes/data/2026-11-m8-pf4/legs/orchestrate_r1.sh ea71502aa7789f27931e63d1ab6d10fa02b20e8b 20a9fa884605451e10bc81c6f64d5f4a12d27b56fa377eda3a9e62143570cb2c 2b7d173b6959e0372ac9a4c5da51410f049b06aa94e6a4dddce20d202708da18 684379f7a1831ae8529f9da6d338baa9332787b66f206720e61c54c2892f2916 28377676aeef8ffef9b93be3501bce9fa341793843d7a786ac5ad02d72b3bea1 3937 > docs/notes/data/2026-11-m8-pf4/gate-r1/orchestrate.out 2>&1
```

Legs in order: P4a, P4b, P5, P6; the chain stops at the first failed leg.

## 3. Disclosure

CI run 44712385076 on the code commit finished green on all four lanes (linux-clang-release, linux-clang-coverage,
macos-clang-release, windows-clang-release) before this file.
