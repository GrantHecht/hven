# W5 T8.9r fix1 -- R2 idle proof, per timed batch

Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

| log | batch | window wall (s) | snapshots | foreign pids | worst foreign cputime delta (s) | worst fraction | `R` seen | pauses | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|
| Y1-ywall-r1.log | ywall-r1 | 36.05 | 7 | 197 | 0.960 (pid 50122) | **2.6630 %** | none | 0 | **NOT met** |
| Y2-ywall-r2.log | ywall-r2 | 35.38 | 7 | 198 | 0.970 (pid 50122) | **2.7417 %** | none | 0 | **NOT met** |
| Y3-ywall-r3.log | ywall-r3 | 35.53 | 7 | 200 | 0.990 (pid 50122) | **2.7864 %** | none | 0 | **NOT met** |
| Y4-ywall-r4.log | ywall-r4 | 35.25 | 7 | 199 | 0.970 (pid 50122) | **2.7518 %** | none | 0 | **NOT met** |
| Y5-ywall-r5.log | ywall-r5 | 40.40 | 7 | 199 | 1.180 (pid 50122) | **2.9208 %** | 2722314 | 2 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

| log | batch | runs | timed wall (s) | **cpu2 un-niced USER (s)** | **fraction** | **cpu10 un-niced USER (s)** | **fraction** | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Y1-ywall-r1.log | ywall-r1 | 5 | 32.79 | **0.000** | **0.0000 %** | **0.050** | **0.1525 %** | 29.53 | 32.59 | 0.020 | 0.160 | PINNED-CLEAN |
| Y2-ywall-r2.log | ywall-r2 | 5 | 32.13 | **0.000** | **0.0000 %** | **0.090** | **0.2801 %** | 28.91 | 31.92 | 0.040 | 0.150 | PINNED-CLEAN |
| Y3-ywall-r3.log | ywall-r3 | 5 | 32.24 | **0.000** | **0.0000 %** | **0.090** | **0.2792 %** | 29.10 | 32.05 | 0.020 | 0.160 | PINNED-CLEAN |
| Y4-ywall-r4.log | ywall-r4 | 5 | 31.97 | **0.000** | **0.0000 %** | **0.030** | **0.0938 %** | 28.73 | 31.78 | 0.020 | 0.150 | PINNED-CLEAN |
| Y5-ywall-r5.log | ywall-r5 | 5 | 32.12 | **0.000** | **0.0000 %** | **0.070** | **0.2179 %** | 28.97 | 31.93 | 0.020 | 0.130 | PINNED-CLEAN |

## The five busiest foreign processes per batch

- Y1-ywall-r1.log / ywall-r1 (window 36.05 s): pid 50122 +0.96 s; pid 1097257 +0.42 s; pid 2722314 +0.18 s; pid 4022442 +0.13 s; pid 1861779 +0.07 s
- Y2-ywall-r2.log / ywall-r2 (window 35.38 s): pid 50122 +0.97 s; pid 1861779 +0.75 s; pid 1097257 +0.42 s; pid 2721602 +0.27 s; pid 2722314 +0.17 s
- Y3-ywall-r3.log / ywall-r3 (window 35.53 s): pid 50122 +0.99 s; pid 1861779 +0.59 s; pid 1097257 +0.43 s; pid 2722314 +0.14 s; pid 4022442 +0.10 s
- Y4-ywall-r4.log / ywall-r4 (window 35.25 s): pid 50122 +0.97 s; pid 1097257 +0.43 s; pid 1861779 +0.35 s; pid 2721602 +0.27 s; pid 2722314 +0.17 s
- Y5-ywall-r5.log / ywall-r5 (window 40.40 s): pid 50122 +1.18 s; pid 1097257 +0.47 s; pid 1861779 +0.42 s; pid 2721602 +0.34 s; pid 2722314 +0.19 s
- Y5-ywall-r5.log / ywall-r5: BOX_PAUSE 2026-09-11T19:28:51Z tag=ywall-r5/f1-after try=1 -- a foreign process is in state R.
- Y5-ywall-r5.log / ywall-r5: BOX_PAUSE_GIVEUP 2026-09-11T19:28:56Z tag=ywall-r5/f1-after -- still R after 1 pauses.
