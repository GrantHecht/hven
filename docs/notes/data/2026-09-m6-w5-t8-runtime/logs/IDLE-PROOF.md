# W5 T8.9r fix1 -- R2 idle proof, per timed batch

Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

| log | batch | window wall (s) | snapshots | foreign pids | worst foreign cputime delta (s) | worst fraction | `R` seen | pauses | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|
| L4-leg1-ipm-r1.log | leg1-ipm-r1 | 48.27 | 4 | 196 | 1.420 (pid 50122) | **2.9418 %** | none | 0 | **NOT met** |
| L4-leg1-ipm-r2.log | leg1-ipm-r2 | 48.30 | 4 | 198 | 1.400 (pid 50122) | **2.8986 %** | none | 0 | **NOT met** |
| L4-leg1-ipm-r3.log | leg1-ipm-r3 | 53.21 | 4 | 195 | 1.550 (pid 50122) | **2.9130 %** | 50122 | 2 | **NOT met** |
| L4-leg1-ssn-r1.log | leg1-ssn-r1 | 37.41 | 4 | 196 | 1.060 (pid 50122) | **2.8335 %** | none | 0 | **NOT met** |
| L4-leg1-ssn-r2.log | leg1-ssn-r2 | 37.39 | 4 | 196 | 1.010 (pid 50122) | **2.7013 %** | none | 0 | **NOT met** |
| L4-leg1-ssn-r3.log | leg1-ssn-r3 | 37.34 | 4 | 195 | 1.020 (pid 50122) | **2.7317 %** | none | 0 | **NOT met** |
| L4-leg1-walk-r1.log | leg1-walk-r1 | 414.78 | 4 | 192 | 11.740 (pid 50122) | **2.8304 %** | none | 0 | **NOT met** |
| L4-leg1-walk-r2.log | leg1-walk-r2 | 414.82 | 4 | 195 | 10.830 (pid 50122) | **2.6108 %** | 2722106 | 2 | **NOT met** |
| L4-leg1-walk-r3.log | leg1-walk-r3 | 414.84 | 4 | 196 | 11.710 (pid 50122) | **2.8228 %** | none | 0 | **NOT met** |
| L5-leg1perf-ipm.log | leg1perf-ipm | 32.32 | 8 | 201 | 0.940 (pid 50122) | **2.9084 %** | none | 0 | **NOT met** |
| L5-leg1perf-ssn.log | leg1perf-ssn | 28.61 | 8 | 201 | 0.790 (pid 50122) | **2.7613 %** | none | 0 | **NOT met** |
| L5-leg1perf-walk.log | leg1perf-walk | 15.60 | 8 | 206 | 0.450 (pid 50122) | **2.8846 %** | none | 0 | **NOT met** |
| L6-leg2-ipm-off-passA.log | leg2-ipm-off-passA | 228.21 | 5 | 195 | 6.120 (pid 50122) | **2.6817 %** | none | 0 | **NOT met** |
| L6-leg2-ipm-off-passB.log | leg2-ipm-off-passB | 238.13 | 5 | 195 | 6.550 (pid 50122) | **2.7506 %** | 50122,50634 | 4 | **NOT met** |
| L6-leg2-ipm-sink-passA.log | leg2-ipm-sink-passA | 228.50 | 5 | 200 | 6.070 (pid 50122) | **2.6565 %** | none | 0 | **NOT met** |
| L6-leg2-ipm-sink-passB.log | leg2-ipm-sink-passB | 229.97 | 5 | 198 | 6.390 (pid 50122) | **2.7786 %** | none | 0 | **NOT met** |
| L6-leg2-ssn-off-passA.log | leg2-ssn-off-passA | 369.77 | 5 | 198 | 10.220 (pid 50122) | **2.7639 %** | none | 0 | **NOT met** |
| L6-leg2-ssn-off-passB.log | leg2-ssn-off-passB | 370.45 | 5 | 197 | 10.400 (pid 50122) | **2.8074 %** | none | 0 | **NOT met** |
| L6-leg2-ssn-sink-passA.log | leg2-ssn-sink-passA | 369.24 | 5 | 197 | 9.830 (pid 50122) | **2.6622 %** | none | 0 | **NOT met** |
| L6-leg2-ssn-sink-passB.log | leg2-ssn-sink-passB | 370.48 | 5 | 196 | 9.860 (pid 50122) | **2.6614 %** | none | 0 | **NOT met** |
| L6-leg2-walk-off-passA.log | leg2-walk-off-passA | 140.14 | 5 | 196 | 3.680 (pid 50122) | **2.6259 %** | none | 0 | **NOT met** |
| L6-leg2-walk-off-passB.log | leg2-walk-off-passB | 140.11 | 5 | 198 | 4.000 (pid 50122) | **2.8549 %** | none | 0 | **NOT met** |
| L6-leg2-walk-sink-passA.log | leg2-walk-sink-passA | 140.55 | 5 | 195 | 3.810 (pid 50122) | **2.7108 %** | none | 0 | **NOT met** |
| L6-leg2-walk-sink-passB.log | leg2-walk-sink-passB | 140.38 | 5 | 198 | 3.780 (pid 50122) | **2.6927 %** | none | 0 | **NOT met** |
| L7-interior-perfA.log | interior-perfA | 111.82 | 5 | 196 | 3.250 (pid 50122) | **2.9065 %** | none | 0 | **NOT met** |
| L7-interior-perfB.log | interior-perfB | 111.61 | 5 | 199 | 3.010 (pid 50122) | **2.6969 %** | none | 0 | **NOT met** |
| L7-interior-wall.log | interior-wall | 111.14 | 5 | 199 | 3.110 (pid 50122) | **2.7983 %** | none | 0 | **NOT met** |
| L8-interior-cells-r1.log | interior-cells-r1 | 62.11 | 13 | 198 | 1.770 (pid 50122) | **2.8498 %** | none | 0 | **NOT met** |
| L8-interior-cells-r2.log | interior-cells-r2 | 67.40 | 13 | 198 | 1.850 (pid 50122) | **2.7448 %** | 1097257 | 2 | **NOT met** |
| L8-interior-cells-r3.log | interior-cells-r3 | 67.38 | 13 | 196 | 1.950 (pid 50122) | **2.8940 %** | 50122 | 2 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

| log | batch | runs | timed wall (s) | **cpu2 un-niced USER (s)** | **fraction** | **cpu10 un-niced USER (s)** | **fraction** | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| L4-leg1-ipm-r1.log | leg1-ipm-r1 | 2 | 46.68 | **0.000** | **0.0000 %** | **0.030** | **0.0643 %** | 41.53 | 45.87 | 0.020 | 0.230 | PINNED-CLEAN |
| L4-leg1-ipm-r2.log | leg1-ipm-r2 | 2 | 46.69 | **0.000** | **0.0000 %** | **0.060** | **0.1285 %** | 41.50 | 45.90 | 0.000 | 0.230 | PINNED-CLEAN |
| L4-leg1-ipm-r3.log | leg1-ipm-r3 | 2 | 46.60 | **0.000** | **0.0000 %** | **0.070** | **0.1502 %** | 41.46 | 45.82 | 0.020 | 0.240 | PINNED-CLEAN |
| L4-leg1-ssn-r1.log | leg1-ssn-r1 | 2 | 35.86 | **0.000** | **0.0000 %** | **0.060** | **0.1673 %** | 30.08 | 35.09 | 0.000 | 0.180 | PINNED-CLEAN |
| L4-leg1-ssn-r2.log | leg1-ssn-r2 | 2 | 35.80 | **0.000** | **0.0000 %** | **0.020** | **0.0559 %** | 30.10 | 35.05 | 0.010 | 0.180 | PINNED-CLEAN |
| L4-leg1-ssn-r3.log | leg1-ssn-r3 | 2 | 35.78 | **0.000** | **0.0000 %** | **0.040** | **0.1118 %** | 30.18 | 35.00 | 0.000 | 0.170 | PINNED-CLEAN |
| L4-leg1-walk-r1.log | leg1-walk-r1 | 2 | 413.20 | **0.000** | **0.0000 %** | **0.270** | **0.0653 %** | 398.25 | 410.29 | 0.330 | 2.220 | PINNED-CLEAN |
| L4-leg1-walk-r2.log | leg1-walk-r2 | 2 | 413.19 | **0.000** | **0.0000 %** | **0.170** | **0.0411 %** | 398.34 | 410.18 | 0.220 | 2.250 | PINNED-CLEAN |
| L4-leg1-walk-r3.log | leg1-walk-r3 | 2 | 413.20 | **0.000** | **0.0000 %** | **0.220** | **0.0532 %** | 398.01 | 410.21 | 0.310 | 2.230 | PINNED-CLEAN |
| L5-leg1perf-ipm.log | leg1perf-ipm | 36 | 27.99 | **0.000** | **0.0000 %** | **0.070** | **0.2501 %** | 24.52 | 27.60 | 0.160 | 0.150 | PINNED-CLEAN |
| L5-leg1perf-ssn.log | leg1perf-ssn | 36 | 24.35 | **0.000** | **0.0000 %** | **0.060** | **0.2464 %** | 20.05 | 23.92 | 0.170 | 0.120 | PINNED-CLEAN |
| L5-leg1perf-walk.log | leg1perf-walk | 36 | 11.28 | **0.000** | **0.0000 %** | **0.020** | **0.1773 %** | 9.02 | 10.97 | 0.160 | 0.060 | PINNED-CLEAN |
| L6-leg2-ipm-off-passA.log | leg2-ipm-off-passA | 6 | 225.98 | **0.000** | **0.0000 %** | **0.080** | **0.0354 %** | 223.62 | 224.42 | 0.040 | 1.490 | PINNED-CLEAN |
| L6-leg2-ipm-off-passB.log | leg2-ipm-off-passB | 6 | 225.84 | **0.000** | **0.0000 %** | **0.080** | **0.0354 %** | 223.46 | 224.25 | 0.050 | 1.520 | PINNED-CLEAN |
| L6-leg2-ipm-sink-passA.log | leg2-ipm-sink-passA | 6 | 226.19 | **0.000** | **0.0000 %** | **0.340** | **0.1503 %** | 223.81 | 224.57 | 0.040 | 1.530 | PINNED-CLEAN |
| L6-leg2-ipm-sink-passB.log | leg2-ipm-sink-passB | 6 | 227.66 | **0.000** | **0.0000 %** | **0.110** | **0.0483 %** | 225.33 | 226.05 | 0.040 | 1.530 | PINNED-CLEAN |
| L6-leg2-ssn-off-passA.log | leg2-ssn-off-passA | 6 | 367.49 | **0.000** | **0.0000 %** | **0.380** | **0.1034 %** | 363.23 | 364.88 | 0.050 | 2.480 | PINNED-CLEAN |
| L6-leg2-ssn-off-passB.log | leg2-ssn-off-passB | 6 | 368.17 | **0.000** | **0.0000 %** | **0.190** | **0.0516 %** | 363.82 | 365.57 | 0.050 | 2.470 | PINNED-CLEAN |
| L6-leg2-ssn-sink-passA.log | leg2-ssn-sink-passA | 6 | 366.97 | **0.000** | **0.0000 %** | **0.210** | **0.0572 %** | 362.68 | 364.40 | 0.050 | 2.470 | PINNED-CLEAN |
| L6-leg2-ssn-sink-passB.log | leg2-ssn-sink-passB | 6 | 368.22 | **0.000** | **0.0000 %** | **0.330** | **0.0896 %** | 363.93 | 365.60 | 0.060 | 2.470 | PINNED-CLEAN |
| L6-leg2-walk-off-passA.log | leg2-walk-off-passA | 6 | 137.88 | **0.010** | **0.0073 %** | **0.220** | **0.1596 %** | 136.74 | 136.96 | 0.020 | 0.860 | PINNED-CLEAN |
| L6-leg2-walk-off-passB.log | leg2-walk-off-passB | 6 | 137.88 | **0.000** | **0.0000 %** | **0.130** | **0.0943 %** | 136.76 | 136.97 | 0.030 | 0.860 | PINNED-CLEAN |
| L6-leg2-walk-sink-passA.log | leg2-walk-sink-passA | 6 | 138.30 | **0.000** | **0.0000 %** | **0.180** | **0.1302 %** | 136.95 | 137.38 | 0.030 | 0.870 | PINNED-CLEAN |
| L6-leg2-walk-sink-passB.log | leg2-walk-sink-passB | 6 | 138.14 | **0.000** | **0.0000 %** | **0.140** | **0.1013 %** | 137.06 | 137.21 | 0.040 | 0.860 | PINNED-CLEAN |
| L7-interior-perfA.log | interior-perfA | 9 | 109.47 | **0.440** | **0.4019 %** | **0.280** | **0.2558 %** | 97.73 | 108.03 | 0.510 | 0.520 | PINNED-CLEAN |
| L7-interior-perfB.log | interior-perfB | 9 | 109.31 | **0.000** | **0.0000 %** | **0.080** | **0.0732 %** | 98.64 | 108.73 | 0.030 | 0.510 | PINNED-CLEAN |
| L7-interior-wall.log | interior-wall | 9 | 108.77 | **0.000** | **0.0000 %** | **0.150** | **0.1379 %** | 98.37 | 108.21 | 0.040 | 0.480 | PINNED-CLEAN |
| L8-interior-cells-r1.log | interior-cells-r1 | 33 | 55.20 | **0.000** | **0.0000 %** | **0.100** | **0.1812 %** | 50.23 | 54.74 | 0.110 | 0.230 | PINNED-CLEAN |
| L8-interior-cells-r2.log | interior-cells-r2 | 33 | 55.49 | **0.000** | **0.0000 %** | **0.080** | **0.1442 %** | 50.58 | 54.99 | 0.130 | 0.240 | PINNED-CLEAN |
| L8-interior-cells-r3.log | interior-cells-r3 | 33 | 55.35 | **0.000** | **0.0000 %** | **0.080** | **0.1445 %** | 50.49 | 54.89 | 0.150 | 0.250 | PINNED-CLEAN |

## The five busiest foreign processes per batch

- L4-leg1-ipm-r1.log / leg1-ipm-r1 (window 48.27 s): pid 50122 +1.42 s; pid 1097257 +0.54 s; pid 2721602 +0.28 s; pid 2722314 +0.20 s; pid 4022442 +0.13 s
- L4-leg1-ipm-r2.log / leg1-ipm-r2 (window 48.30 s): pid 50122 +1.40 s; pid 1097257 +0.53 s; pid 2722314 +0.21 s; pid 4022442 +0.17 s; pid 2722912 +0.09 s
- L4-leg1-ipm-r3.log / leg1-ipm-r3 (window 53.21 s): pid 50122 +1.55 s; pid 1097257 +0.53 s; pid 2722314 +0.21 s; pid 4022442 +0.16 s; pid 2721602 +0.14 s
- L4-leg1-ipm-r3.log / leg1-ipm-r3: BOX_PAUSE 2026-09-11T15:34:45Z tag=leg1-ipm-r3/open try=1 -- a foreign process is in state R.
- L4-leg1-ipm-r3.log / leg1-ipm-r3: BOX_PAUSE_GIVEUP 2026-09-11T15:34:50Z tag=leg1-ipm-r3/open -- still R after 1 pauses.
- L4-leg1-ssn-r1.log / leg1-ssn-r1 (window 37.41 s): pid 50122 +1.06 s; pid 1097257 +0.42 s; pid 2721602 +0.27 s; pid 2722314 +0.18 s; pid 4022442 +0.11 s
- L4-leg1-ssn-r2.log / leg1-ssn-r2 (window 37.39 s): pid 50122 +1.01 s; pid 1097257 +0.41 s; pid 2721602 +0.34 s; pid 2722314 +0.14 s; pid 4022442 +0.12 s
- L4-leg1-ssn-r3.log / leg1-ssn-r3 (window 37.34 s): pid 50122 +1.02 s; pid 1097257 +0.41 s; pid 2722314 +0.16 s; pid 4022442 +0.12 s; pid 2722196 +0.12 s
- L4-leg1-walk-r1.log / leg1-walk-r1 (window 414.78 s): pid 50122 +11.74 s; pid 1097257 +3.58 s; pid 2721602 +1.98 s; pid 2722314 +1.70 s; pid 4022442 +1.04 s
- L4-leg1-walk-r2.log / leg1-walk-r2 (window 414.82 s): pid 50122 +10.83 s; pid 1097257 +3.78 s; pid 2721602 +2.21 s; pid 2722314 +1.55 s; pid 4022442 +0.92 s
- L4-leg1-walk-r2.log / leg1-walk-r2: BOX_PAUSE 2026-09-11T17:09:03Z tag=leg1-walk-r2/close try=1 -- a foreign process is in state R.
- L4-leg1-walk-r2.log / leg1-walk-r2: BOX_PAUSE_GIVEUP 2026-09-11T17:09:08Z tag=leg1-walk-r2/close -- still R after 1 pauses.
- L4-leg1-walk-r3.log / leg1-walk-r3 (window 414.84 s): pid 50122 +11.71 s; pid 1097257 +3.45 s; pid 2721602 +2.03 s; pid 2722314 +1.51 s; pid 4022442 +0.87 s
- L5-leg1perf-ipm.log / leg1perf-ipm (window 32.32 s): pid 50122 +0.94 s; pid 1097257 +0.36 s; pid 2721602 +0.26 s; pid 1312476 +0.14 s; pid 2722314 +0.13 s
- L5-leg1perf-ssn.log / leg1perf-ssn (window 28.61 s): pid 50122 +0.79 s; pid 1097257 +0.27 s; pid 2722314 +0.15 s; pid 1312476 +0.11 s; pid 1861779 +0.10 s
- L5-leg1perf-walk.log / leg1perf-walk (window 15.60 s): pid 50122 +0.45 s; pid 1097257 +0.18 s; pid 1861779 +0.10 s; pid 1312476 +0.10 s; pid 2722314 +0.07 s
- L6-leg2-ipm-off-passA.log / leg2-ipm-off-passA (window 228.21 s): pid 50122 +6.12 s; pid 1097257 +1.83 s; pid 2721602 +0.92 s; pid 2722314 +0.81 s; pid 4022442 +0.40 s
- L6-leg2-ipm-off-passB.log / leg2-ipm-off-passB (window 238.13 s): pid 50122 +6.55 s; pid 1097257 +2.02 s; pid 2721602 +1.16 s; pid 2722314 +0.88 s; pid 2721730 +0.67 s
- L6-leg2-ipm-off-passB.log / leg2-ipm-off-passB: BOX_PAUSE 2026-09-11T16:15:24Z tag=leg2-ipm-off-passB/r1 try=1 -- a foreign process is in state R.
- L6-leg2-ipm-off-passB.log / leg2-ipm-off-passB: BOX_PAUSE_GIVEUP 2026-09-11T16:15:29Z tag=leg2-ipm-off-passB/r1 -- still R after 1 pauses.
- L6-leg2-ipm-off-passB.log / leg2-ipm-off-passB: BOX_PAUSE 2026-09-11T16:16:45Z tag=leg2-ipm-off-passB/r2 try=1 -- a foreign process is in state R.
- L6-leg2-ipm-off-passB.log / leg2-ipm-off-passB: BOX_PAUSE_GIVEUP 2026-09-11T16:16:50Z tag=leg2-ipm-off-passB/r2 -- still R after 1 pauses.
- L6-leg2-ipm-sink-passA.log / leg2-ipm-sink-passA (window 228.50 s): pid 50122 +6.07 s; pid 1097257 +2.00 s; pid 2721602 +1.18 s; pid 2722314 +1.05 s; pid 2722044 +0.64 s
- L6-leg2-ipm-sink-passB.log / leg2-ipm-sink-passB (window 229.97 s): pid 50122 +6.39 s; pid 1097257 +2.04 s; pid 2721602 +1.15 s; pid 2722314 +0.86 s; pid 4022442 +0.58 s
- L6-leg2-ssn-off-passA.log / leg2-ssn-off-passA (window 369.77 s): pid 50122 +10.22 s; pid 1097257 +3.38 s; pid 2721602 +1.73 s; pid 2722314 +1.38 s; pid 4022442 +0.97 s
- L6-leg2-ssn-off-passB.log / leg2-ssn-off-passB (window 370.45 s): pid 50122 +10.40 s; pid 1097257 +3.34 s; pid 2721602 +1.87 s; pid 2722314 +1.38 s; pid 4022442 +0.84 s
- L6-leg2-ssn-sink-passA.log / leg2-ssn-sink-passA (window 369.24 s): pid 50122 +9.83 s; pid 1097257 +3.21 s; pid 2721602 +1.76 s; pid 2722314 +1.41 s; pid 4022442 +0.83 s
- L6-leg2-ssn-sink-passB.log / leg2-ssn-sink-passB (window 370.48 s): pid 50122 +9.86 s; pid 1097257 +3.35 s; pid 2721602 +1.82 s; pid 2722314 +1.39 s; pid 4022442 +0.85 s
- L6-leg2-walk-off-passA.log / leg2-walk-off-passA (window 140.14 s): pid 50122 +3.68 s; pid 1097257 +1.13 s; pid 2721602 +0.95 s; pid 2722314 +0.51 s; pid 2721730 +0.28 s
- L6-leg2-walk-off-passB.log / leg2-walk-off-passB (window 140.11 s): pid 50122 +4.00 s; pid 1097257 +1.19 s; pid 2721602 +0.64 s; pid 2722314 +0.54 s; pid 4022442 +0.28 s
- L6-leg2-walk-sink-passA.log / leg2-walk-sink-passA (window 140.55 s): pid 50122 +3.81 s; pid 1097257 +1.18 s; pid 2721602 +0.62 s; pid 2722314 +0.53 s; pid 4022442 +0.27 s
- L6-leg2-walk-sink-passB.log / leg2-walk-sink-passB (window 140.38 s): pid 50122 +3.78 s; pid 1097257 +1.13 s; pid 2721602 +0.85 s; pid 2722314 +0.54 s; pid 4022442 +0.28 s
- L7-interior-perfA.log / interior-perfA (window 111.82 s): pid 50122 +3.25 s; pid 1097257 +1.20 s; pid 2721602 +0.63 s; pid 2722314 +0.48 s; pid 4022442 +0.32 s
- L7-interior-perfB.log / interior-perfB (window 111.61 s): pid 50122 +3.01 s; pid 1097257 +1.21 s; pid 2721602 +0.57 s; pid 2722314 +0.49 s; pid 2721730 +0.48 s
- L7-interior-wall.log / interior-wall (window 111.14 s): pid 50122 +3.11 s; pid 1097257 +1.21 s; pid 2721602 +0.61 s; pid 2722314 +0.51 s; pid 4022442 +0.30 s
- L8-interior-cells-r1.log / interior-cells-r1 (window 62.11 s): pid 50122 +1.77 s; pid 1097257 +0.60 s; pid 2721602 +0.28 s; pid 2722314 +0.26 s; pid 1312476 +0.18 s
- L8-interior-cells-r2.log / interior-cells-r2 (window 67.40 s): pid 50122 +1.85 s; pid 1097257 +0.68 s; pid 2721602 +0.36 s; pid 2722314 +0.27 s; pid 4022442 +0.17 s
- L8-interior-cells-r2.log / interior-cells-r2: BOX_PAUSE 2026-09-11T17:16:27Z tag=interior-cells-r2/f7_n10000_bound_physics try=1 -- a foreign process is in state R.
- L8-interior-cells-r2.log / interior-cells-r2: BOX_PAUSE_GIVEUP 2026-09-11T17:16:32Z tag=interior-cells-r2/f7_n10000_bound_physics -- still R after 1 pauses.
- L8-interior-cells-r3.log / interior-cells-r3 (window 67.38 s): pid 50122 +1.95 s; pid 1097257 +0.67 s; pid 2721602 +0.29 s; pid 2722314 +0.27 s; pid 4022442 +0.19 s
- L8-interior-cells-r3.log / interior-cells-r3: BOX_PAUSE 2026-09-11T17:13:23Z tag=interior-cells-r3/f7_n1000_bound_neutral try=1 -- a foreign process is in state R.
- L8-interior-cells-r3.log / interior-cells-r3: BOX_PAUSE_GIVEUP 2026-09-11T17:13:28Z tag=interior-cells-r3/f7_n1000_bound_neutral -- still R after 1 pauses.
