# W5 T8.9r fix1 -- R2 idle proof, per timed batch

Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

| log | batch | window wall (s) | snapshots | foreign pids | worst foreign cputime delta (s) | worst fraction | `R` seen | pauses | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|
| X1-xwall-r1.log | xwall-r1 | 28.20 | 6 | 199 | 0.810 (pid 50122) | **2.8723 %** | none | 0 | **NOT met** |
| X2-xwall-r2.log | xwall-r2 | 33.39 | 6 | 200 | 0.940 (pid 50122) | **2.8152 %** | 4022442 | 2 | **NOT met** |
| X3-xwall-r3.log | xwall-r3 | 28.53 | 6 | 199 | 0.850 (pid 50122) | **2.9793 %** | none | 0 | **NOT met** |
| X4-xwall-r4.log | xwall-r4 | 28.28 | 6 | 200 | 0.820 (pid 50122) | **2.8996 %** | none | 0 | **NOT met** |
| X5-xwall-r5.log | xwall-r5 | 29.21 | 6 | 199 | 0.800 (pid 50122) | **2.7388 %** | none | 0 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

| log | batch | runs | timed wall (s) | **cpu2 un-niced USER (s)** | **fraction** | **cpu10 un-niced USER (s)** | **fraction** | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| X1-xwall-r1.log | xwall-r1 | 4 | 25.51 | **0.000** | **0.0000 %** | **0.010** | **0.0392 %** | 22.94 | 25.37 | 0.010 | 0.130 | PINNED-CLEAN |
| X2-xwall-r2.log | xwall-r2 | 4 | 25.68 | **0.000** | **0.0000 %** | **0.030** | **0.1168 %** | 23.09 | 25.53 | 0.030 | 0.130 | PINNED-CLEAN |
| X3-xwall-r3.log | xwall-r3 | 4 | 25.85 | **0.000** | **0.0000 %** | **0.040** | **0.1547 %** | 23.28 | 25.68 | 0.030 | 0.120 | PINNED-CLEAN |
| X4-xwall-r4.log | xwall-r4 | 4 | 25.59 | **0.000** | **0.0000 %** | **0.040** | **0.1563 %** | 23.01 | 25.45 | 0.030 | 0.110 | PINNED-CLEAN |
| X5-xwall-r5.log | xwall-r5 | 4 | 26.46 | **0.000** | **0.0000 %** | **0.010** | **0.0378 %** | 23.86 | 26.32 | 0.020 | 0.120 | PINNED-CLEAN |

## The five busiest foreign processes per batch

- X1-xwall-r1.log / xwall-r1 (window 28.20 s): pid 50122 +0.81 s; pid 2721602 +0.35 s; pid 1097257 +0.29 s; pid 2721730 +0.12 s; pid 2722314 +0.11 s
- X2-xwall-r2.log / xwall-r2 (window 33.39 s): pid 50122 +0.94 s; pid 1097257 +0.33 s; pid 2722314 +0.15 s; pid 4022442 +0.08 s; pid 1861779 +0.07 s
- X2-xwall-r2.log / xwall-r2: BOX_PAUSE 2026-09-11T19:17:08Z tag=xwall-r2/open try=1 -- a foreign process is in state R.
- X2-xwall-r2.log / xwall-r2: BOX_PAUSE_GIVEUP 2026-09-11T19:17:13Z tag=xwall-r2/open -- still R after 1 pauses.
- X3-xwall-r3.log / xwall-r3 (window 28.53 s): pid 50122 +0.85 s; pid 1097257 +0.30 s; pid 2721602 +0.26 s; pid 2722314 +0.11 s; pid 4022442 +0.08 s
- X4-xwall-r4.log / xwall-r4 (window 28.28 s): pid 50122 +0.82 s; pid 1097257 +0.31 s; pid 2722314 +0.13 s; pid 1312476 +0.09 s; pid 4022442 +0.07 s
- X5-xwall-r5.log / xwall-r5 (window 29.21 s): pid 50122 +0.80 s; pid 1097257 +0.30 s; pid 2721602 +0.27 s; pid 2722314 +0.14 s; pid 4022442 +0.07 s
