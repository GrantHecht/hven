# M8 PF-4 r4 -- successor of DECLARATION-PF-4-r3.md after a throttle void at P4a (DECLARATION)

Committed on `pf4-r2` before the first r4 leg; never edited afterwards. `DECLARATION-PF-4-r2.md`, `-r3.md` and every
output (`r2/gate/`, `r2/gate-r3/`) are kept. A failed or void r4 leg is answered by `DECLARATION-PF-4-r5.md`.

## 0. Why a successor

r3's P4a (`r2/gate-r3/orchestrate.out`) passed its fixture pre-flight and ran in one box lane (lane 1). In round 2, one
measured process was THROTTLED: arm B, `f7_n10000_bound_corrupted`, 1 throttle event. Under the declared validity check
a throttled process voids the leg; `runtime_leg_r3.sh` exited 5, and the analysis found the 37 missing pairs that the
stop left behind. 88 processes had read 0 throttle events.

`kernel.perf_event_max_sample_rate` read 63000 afterwards. That is a system-wide value which the kernel lowers when perf
interrupts take too long anywhere on the box. r1 ran this leg solo, on the exclusive box, with 0 throttles in 162
processes.

The throttle check is a property of the whole box's interrupt load, not of the lane. P4a therefore runs on the whole box
again. This is not a wall-clock assertion: P4a asserts instruction counts only. The exclusive acquisition restores r1's
solo condition for the throttle check.

## 1. What changes, and what does not

- `r2/legs/orchestrate_r4.sh` = `orchestrate_r3.sh` with P4a under `box-lane --exclusive` and outputs under
  `r2/gate-r4/`. P4b, P5 and P6 each still take one box lane.
- Under `--exclusive`, `BOX_LANE_PHYS` is `0 .. 7`, so the measured processes pin to core 0 (the first listed);
  instruction counts are core-independent.
- r3's partial runtime evidence is kept without its symbol tables (`r2/gate-r3/runtime/walk/SYMS-NOT-KEPT.txt`).
- Unchanged from `DECLARATION-PF-4-r2.md` / `-r3.md`: the code commit (`4a256c7a30c4d1953cc3f84fed0fac7b497436c3`), the
  four binaries and their sha256, every declared mover, every criterion (P4a's THROTTLE 0 included), `analyze_r3.py`, and
  P1-P3's r2 results.

## 2. Order

```
bash docs/notes/data/2026-11-m8-pf4/r2/legs/orchestrate_r4.sh 4a256c7a30c4d1953cc3f84fed0fac7b497436c3 b63d68e8356291dc8db9f7d0c5ab5a06cb29551b019ee33fbdf6eab31111bb8e b55a15090390697dbca09d9a53b931da042ed4450c82aaa93667532231d164d7 9d766ea786fdbffb938120547b629177c5ff8d4309efa144194a5044246c9481 def8b04c10f70ccd5c0f3c6d4bc882409b3e103be77b6dd8a1309168011805ef 3965 > docs/notes/data/2026-11-m8-pf4/r2/gate-r4/orchestrate.out 2>&1
```

Legs in order: P4a (exclusive), P4b, P5, P6. The chain stops at the first failed leg.
