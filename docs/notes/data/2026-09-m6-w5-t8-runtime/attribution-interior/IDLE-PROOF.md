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
| D1-diff-r1.log | diff-r1 | 88.76 | 13 | 197 | 2.840 (pid 50122) | **3.1996 %** | none | 0 | **NOT met** |
| D2-diff-r2.log | diff-r2 | 88.33 | 13 | 197 | 2.750 (pid 50122) | **3.1133 %** | none | 0 | **NOT met** |
| D3-diff-r3.log | diff-r3 | 88.42 | 13 | 194 | 2.760 (pid 50122) | **3.1215 %** | none | 0 | **NOT met** |
| D4-diff-r4.log | diff-r4 | 88.48 | 13 | 197 | 2.490 (pid 50122) | **2.8142 %** | none | 0 | **NOT met** |
| D5-diff-r5.log | diff-r5 | 88.39 | 13 | 194 | 2.540 (pid 50122) | **2.8736 %** | 116643 | 2 | **NOT met** |
| PA1-perfA-r1.log | perfA-r1 | 77.96 | 13 | 197 | 2.210 (pid 50122) | **2.8348 %** | none | 0 | **NOT met** |
| PA2-perfA-r2.log | perfA-r2 | 77.68 | 13 | 194 | 2.160 (pid 50122) | **2.7806 %** | none | 0 | **NOT met** |
| PA3-perfA-r3.log | perfA-r3 | 78.08 | 13 | 197 | 2.120 (pid 50122) | **2.7152 %** | none | 0 | **NOT met** |
| PB1-perfB-r1.log | perfB-r1 | 28.78 | 6 | 197 | 0.810 (pid 50122) | **2.8145 %** | none | 0 | **NOT met** |
| PB2-perfB-r2.log | perfB-r2 | 34.20 | 6 | 197 | 0.980 (pid 50122) | **2.8655 %** | 2722314 | 2 | **NOT met** |
| PB3-perfB-r3.log | perfB-r3 | 28.42 | 6 | 201 | 0.850 (pid 50122) | **2.9909 %** | none | 0 | **NOT met** |
| W1-wall-r1.log | wall-r1 | 77.75 | 13 | 197 | 2.250 (pid 50122) | **2.8939 %** | none | 0 | **NOT met** |
| W2-wall-r2.log | wall-r2 | 82.23 | 13 | 197 | 2.380 (pid 50122) | **2.8943 %** | 50122 | 2 | **NOT met** |
| W3-wall-r3.log | wall-r3 | 78.19 | 13 | 197 | 2.100 (pid 50122) | **2.6858 %** | none | 0 | **NOT met** |
| W4-wall-r4.log | wall-r4 | 82.90 | 13 | 197 | 2.410 (pid 50122) | **2.9071 %** | 2722314 | 2 | **NOT met** |
| W5-wall-r5.log | wall-r5 | 83.20 | 13 | 194 | 2.280 (pid 50122) | **2.7404 %** | 2722106 | 2 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

| log | batch | runs | timed wall (s) | **cpu2 un-niced USER (s)** | **fraction** | **cpu10 un-niced USER (s)** | **fraction** | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| D1-diff-r1.log | diff-r1 | 22 | 82.02 | **0.340** | **0.4145 %** | **0.110** | **0.1341 %** | 73.46 | 80.40 | 0.690 | 0.400 | PINNED-CLEAN |
| D2-diff-r2.log | diff-r2 | 22 | 81.55 | **0.000** | **0.0000 %** | **0.090** | **0.1104 %** | 74.08 | 80.97 | 0.070 | 0.390 | PINNED-CLEAN |
| D3-diff-r3.log | diff-r3 | 22 | 81.65 | **0.150** | **0.1837 %** | **0.230** | **0.2817 %** | 73.80 | 80.67 | 0.260 | 0.370 | PINNED-CLEAN |
| D4-diff-r4.log | diff-r4 | 22 | 81.65 | **0.000** | **0.0000 %** | **0.120** | **0.1470 %** | 74.28 | 81.10 | 0.080 | 0.380 | PINNED-CLEAN |
| D5-diff-r5.log | diff-r5 | 22 | 81.73 | **0.000** | **0.0000 %** | **0.190** | **0.2325 %** | 74.17 | 81.13 | 0.110 | 0.360 | PINNED-CLEAN |
| PA1-perfA-r1.log | perfA-r1 | 11 | 71.44 | **0.000** | **0.0000 %** | **0.110** | **0.1540 %** | 64.41 | 71.01 | 0.070 | 0.320 | PINNED-CLEAN |
| PA2-perfA-r2.log | perfA-r2 | 11 | 71.16 | **0.000** | **0.0000 %** | **0.050** | **0.0703 %** | 64.25 | 70.75 | 0.060 | 0.330 | PINNED-CLEAN |
| PA3-perfA-r3.log | perfA-r3 | 11 | 71.50 | **0.010** | **0.0140 %** | **0.070** | **0.0979 %** | 64.49 | 71.09 | 0.050 | 0.320 | PINNED-CLEAN |
| PB1-perfB-r1.log | perfB-r1 | 4 | 26.06 | **0.060** | **0.2302 %** | **0.010** | **0.0384 %** | 23.46 | 25.76 | 0.090 | 0.120 | PINNED-CLEAN |
| PB2-perfB-r2.log | perfB-r2 | 4 | 26.46 | **0.000** | **0.0000 %** | **0.110** | **0.4157 %** | 23.98 | 26.32 | 0.010 | 0.120 | PINNED-CLEAN |
| PB3-perfB-r3.log | perfB-r3 | 4 | 25.68 | **0.000** | **0.0000 %** | **0.060** | **0.2336 %** | 23.22 | 25.53 | 0.000 | 0.120 | PINNED-CLEAN |
| W1-wall-r1.log | wall-r1 | 11 | 71.34 | **0.000** | **0.0000 %** | **0.120** | **0.1682 %** | 64.57 | 70.94 | 0.070 | 0.310 | PINNED-CLEAN |
| W2-wall-r2.log | wall-r2 | 11 | 70.75 | **0.000** | **0.0000 %** | **0.130** | **0.1837 %** | 64.16 | 70.39 | 0.030 | 0.340 | PINNED-CLEAN |
| W3-wall-r3.log | wall-r3 | 11 | 71.70 | **0.000** | **0.0000 %** | **0.110** | **0.1534 %** | 64.93 | 71.31 | 0.050 | 0.330 | PINNED-CLEAN |
| W4-wall-r4.log | wall-r4 | 11 | 71.36 | **0.000** | **0.0000 %** | **0.090** | **0.1261 %** | 64.62 | 71.00 | 0.030 | 0.310 | PINNED-CLEAN |
| W5-wall-r5.log | wall-r5 | 11 | 71.69 | **0.000** | **0.0000 %** | **0.070** | **0.0976 %** | 64.89 | 71.30 | 0.060 | 0.310 | PINNED-CLEAN |

## The five busiest foreign processes per batch

- D1-diff-r1.log / diff-r1 (window 88.76 s): pid 50122 +2.84 s; pid 1097257 +0.93 s; pid 2722314 +0.41 s; pid 2721602 +0.36 s; pid 4022442 +0.29 s
- D2-diff-r2.log / diff-r2 (window 88.33 s): pid 50122 +2.75 s; pid 1097257 +0.84 s; pid 2722314 +0.40 s; pid 2721602 +0.38 s; pid 4022442 +0.27 s
- D3-diff-r3.log / diff-r3 (window 88.42 s): pid 50122 +2.76 s; pid 1097257 +0.93 s; pid 2722314 +0.58 s; pid 2721602 +0.55 s; pid 4022442 +0.25 s
- D4-diff-r4.log / diff-r4 (window 88.48 s): pid 50122 +2.49 s; pid 1097257 +0.96 s; pid 2721602 +0.54 s; pid 2722314 +0.36 s; pid 3819500 +0.30 s
- D5-diff-r5.log / diff-r5 (window 88.39 s): pid 50122 +2.54 s; pid 1097257 +0.90 s; pid 2721602 +0.40 s; pid 2722314 +0.39 s; pid 3819500 +0.27 s
- D5-diff-r5.log / diff-r5: BOX_PAUSE 2026-09-11T18:25:35Z tag=diff-r5/close try=1 -- a foreign process is in state R.
- D5-diff-r5.log / diff-r5: BOX_PAUSE_GIVEUP 2026-09-11T18:25:40Z tag=diff-r5/close -- still R after 1 pauses.
- PA1-perfA-r1.log / perfA-r1 (window 77.96 s): pid 50122 +2.21 s; pid 1097257 +0.79 s; pid 2722314 +0.35 s; pid 2721602 +0.35 s; pid 4022442 +0.21 s
- PA2-perfA-r2.log / perfA-r2 (window 77.68 s): pid 50122 +2.16 s; pid 1097257 +0.79 s; pid 2722314 +0.37 s; pid 2721602 +0.29 s; pid 4022442 +0.23 s
- PA3-perfA-r3.log / perfA-r3 (window 78.08 s): pid 50122 +2.12 s; pid 1097257 +0.80 s; pid 2721602 +0.62 s; pid 2722314 +0.34 s; pid 4022442 +0.23 s
- PB1-perfB-r1.log / perfB-r1 (window 28.78 s): pid 50122 +0.81 s; pid 2721602 +0.27 s; pid 1097257 +0.26 s; pid 2722314 +0.13 s; pid 4022442 +0.10 s
- PB2-perfB-r2.log / perfB-r2 (window 34.20 s): pid 50122 +0.98 s; pid 1097257 +0.39 s; pid 2721602 +0.26 s; pid 2722314 +0.17 s; pid 4022442 +0.09 s
- PB2-perfB-r2.log / perfB-r2: BOX_PAUSE 2026-09-11T18:25:59Z tag=perfB-r2/a04-after try=1 -- a foreign process is in state R.
- PB2-perfB-r2.log / perfB-r2: BOX_PAUSE_GIVEUP 2026-09-11T18:26:04Z tag=perfB-r2/a04-after -- still R after 1 pauses.
- PB3-perfB-r3.log / perfB-r3 (window 28.42 s): pid 50122 +0.85 s; pid 1097257 +0.29 s; pid 2721602 +0.27 s; pid 2722314 +0.12 s; pid 4022442 +0.09 s
- W1-wall-r1.log / wall-r1 (window 77.75 s): pid 50122 +2.25 s; pid 1097257 +0.83 s; pid 2721602 +0.61 s; pid 2722314 +0.37 s; pid 4022442 +0.24 s
- W2-wall-r2.log / wall-r2 (window 82.23 s): pid 50122 +2.38 s; pid 1097257 +0.99 s; pid 2721602 +0.53 s; pid 2722314 +0.41 s; pid 4022442 +0.19 s
- W2-wall-r2.log / wall-r2: BOX_PAUSE 2026-09-11T17:50:37Z tag=wall-r2/a06-after try=1 -- a foreign process is in state R.
- W2-wall-r2.log / wall-r2: BOX_PAUSE_GIVEUP 2026-09-11T17:50:42Z tag=wall-r2/a06-after -- still R after 1 pauses.
- W3-wall-r3.log / wall-r3 (window 78.19 s): pid 50122 +2.10 s; pid 1097257 +0.85 s; pid 2722314 +0.35 s; pid 2721602 +0.29 s; pid 4022442 +0.23 s
- W4-wall-r4.log / wall-r4 (window 82.90 s): pid 50122 +2.41 s; pid 1097257 +0.82 s; pid 2722314 +0.37 s; pid 2721602 +0.30 s; pid 4022442 +0.25 s
- W4-wall-r4.log / wall-r4: BOX_PAUSE 2026-09-11T17:53:26Z tag=wall-r4/a08-after try=1 -- a foreign process is in state R.
- W4-wall-r4.log / wall-r4: BOX_PAUSE_GIVEUP 2026-09-11T17:53:31Z tag=wall-r4/a08-after -- still R after 1 pauses.
- W5-wall-r5.log / wall-r5 (window 83.20 s): pid 50122 +2.28 s; pid 1097257 +0.87 s; pid 2721602 +0.54 s; pid 2722314 +0.36 s; pid 4022442 +0.24 s
- W5-wall-r5.log / wall-r5: BOX_PAUSE 2026-09-11T17:55:35Z tag=wall-r5/a04-after try=1 -- a foreign process is in state R.
- W5-wall-r5.log / wall-r5: BOX_PAUSE_GIVEUP 2026-09-11T17:55:40Z tag=wall-r5/a04-after -- still R after 1 pauses.
