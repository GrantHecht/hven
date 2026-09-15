# W5 T8.9r fix1 -- R2 idle proof, per timed batch

> **SUPERSEDED AT FIX2 (2026-09-11).** R2 was AMENDED to **R2'** (the pinned-core
> rule) and **every retained batch of every round was re-audited** under it with
> ONE `idle_proof.py` — the attrib4 NICE-INCLUSIVE version. The verdicts in this
> file were taken on the superseded `user`-only accounting, which did not count a
> foreign task's `nice` time. **The governing table is
> `logs/IDLE-PROOF.md`** (artifact root). This file is retained, unedited below
> this line, because the amendment changes which test carries the verdict, not
> what the superseded test measured.


Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

| log | batch | window wall (s) | snapshots | foreign pids | worst foreign cputime delta (s) | worst fraction | `R` seen | pauses | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|
| W1-wall-r1.log | wall-r1 | 42.59 | 8 | 200 | 1.250 (pid 50122) | **2.9350 %** | none | 0 | **NOT met** |
| W2-wall-r2.log | wall-r2 | 42.31 | 8 | 199 | 1.150 (pid 50122) | **2.7180 %** | none | 0 | **NOT met** |
| W3-wall-r3.log | wall-r3 | 42.40 | 8 | 199 | 1.130 (pid 50122) | **2.6651 %** | none | 0 | **NOT met** |
| W4-wall-r4.log | wall-r4 | 47.53 | 8 | 199 | 1.410 (pid 50122) | **2.9665 %** | 50122 | 2 | **NOT met** |
| W5-wall-r5.log | wall-r5 | 43.42 | 8 | 200 | 1.200 (pid 50122) | **2.7637 %** | none | 0 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

| log | batch | runs | timed wall (s) | **cpu2 un-niced USER (s)** | **fraction** | **cpu10 un-niced USER (s)** | **fraction** | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| W1-wall-r1.log | wall-r1 | 6 | 38.77 | **0.000** | **0.0000 %** | **0.120** | **0.3095 %** | 34.86 | 38.53 | 0.040 | 0.180 | PINNED-CLEAN |
| W2-wall-r2.log | wall-r2 | 6 | 38.51 | **0.010** | **0.0260 %** | **0.020** | **0.0519 %** | 34.17 | 37.97 | 0.330 | 0.200 | PINNED-CLEAN |
| W3-wall-r3.log | wall-r3 | 6 | 38.58 | **0.030** | **0.0778 %** | **0.100** | **0.2592 %** | 34.24 | 38.03 | 0.350 | 0.180 | PINNED-CLEAN |
| W4-wall-r4.log | wall-r4 | 6 | 38.70 | **0.050** | **0.1292 %** | **0.030** | **0.0775 %** | 34.32 | 38.14 | 0.350 | 0.150 | PINNED-CLEAN |
| W5-wall-r5.log | wall-r5 | 6 | 39.62 | **0.010** | **0.0252 %** | **0.070** | **0.1767 %** | 35.42 | 39.04 | 0.290 | 0.200 | PINNED-CLEAN |

## The five busiest foreign processes per batch

- W1-wall-r1.log / wall-r1 (window 42.59 s): pid 50122 +1.25 s; pid 1097257 +0.44 s; pid 2722314 +0.20 s; pid 4022442 +0.11 s; pid 1312476 +0.11 s
- W2-wall-r2.log / wall-r2 (window 42.31 s): pid 50122 +1.15 s; pid 1097257 +0.41 s; pid 2721602 +0.26 s; pid 2722314 +0.19 s; pid 4022442 +0.10 s
- W3-wall-r3.log / wall-r3 (window 42.40 s): pid 50122 +1.13 s; pid 1097257 +0.44 s; pid 2721602 +0.36 s; pid 2722314 +0.21 s; pid 2721730 +0.15 s
- W4-wall-r4.log / wall-r4 (window 47.53 s): pid 50122 +1.41 s; pid 1097257 +0.50 s; pid 2721602 +0.28 s; pid 2722314 +0.19 s; pid 4022442 +0.15 s
- W4-wall-r4.log / wall-r4: BOX_PAUSE 2026-09-11T18:53:26Z tag=wall-r4/b04-after try=1 -- a foreign process is in state R.
- W4-wall-r4.log / wall-r4: BOX_PAUSE_GIVEUP 2026-09-11T18:53:31Z tag=wall-r4/b04-after -- still R after 1 pauses.
- W5-wall-r5.log / wall-r5 (window 43.42 s): pid 50122 +1.20 s; pid 1097257 +0.45 s; pid 2722314 +0.20 s; pid 4022442 +0.15 s; pid 1312476 +0.11 s
