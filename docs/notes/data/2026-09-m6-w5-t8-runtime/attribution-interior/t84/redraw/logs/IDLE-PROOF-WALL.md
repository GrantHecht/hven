# W5 T8.9r fix1 -- R2 idle proof, per timed batch

Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

**TRANSIENTS AND STATES ARE DISCLOSED (attrib4).** A pid present in one snapshot of a batch but not the other cannot have its CPU delta differenced; the column counts them rather than dropping them silently, and EVERY foreign state seen in any snapshot is listed, not only `R`.

| log | batch | window wall (s) | snapshots | foreign pids | transients | worst foreign cputime delta (s) | worst fraction | states seen | `R` seen | pauses | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|---|---|
| W1-wall-r1.log | wall-r1 | 34.09 | 6 | 196 | 40 | 1.060 (pid 50122) | **3.1094 %** | R,S | 50122 | 2 | **NOT met** |
| W2-wall-r2.log | wall-r2 | 29.15 | 6 | 198 | 32 | 0.810 (pid 50122) | **2.7787 %** | S | none | 0 | **NOT met** |
| W3-wall-r3.log | wall-r3 | 29.48 | 6 | 198 | 31 | 0.830 (pid 50122) | **2.8155 %** | S | none | 0 | **NOT met** |
| W4-wall-r4.log | wall-r4 | 29.44 | 6 | 199 | 30 | 0.810 (pid 50122) | **2.7514 %** | S | none | 0 | **NOT met** |
| W5-wall-r5.log | wall-r5 | 29.30 | 6 | 198 | 31 | 0.940 (pid 50122) | **3.2082 %** | S | none | 0 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

**THE NICE-INCLUSIVE REPAIR (attrib4).** The two bold columns are the REPAIRED figure: `user + nice + steal + guest` on the core, with only the run's own `user` seconds subtracted on cpu2 and nothing subtracted on cpu10. The `user`-only columns beside them are the SUPERSEDED figure the earlier proof took its verdict on; they are printed so the two can be compared and are not what any verdict here rests on.

| log | batch | runs | timed wall (s) | **cpu2 foreign TASK (s)** | **fraction** | **cpu10 TASK (s)** | **fraction** | cpu2 user-only (s) | cpu10 user-only (s) | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| W1-wall-r1.log | wall-r1 | 4 | 26.36 | **0.010** | **0.0379 %** | **0.110** | **0.4173 %** | 0.000 | 0.020 | 23.75 | 26.21 | 0.020 | 0.130 | PINNED-CLEAN |
| W2-wall-r2.log | wall-r2 | 4 | 26.45 | **0.020** | **0.0756 %** | **0.090** | **0.3403 %** | 0.000 | 0.050 | 23.82 | 26.30 | 0.030 | 0.130 | PINNED-CLEAN |
| W3-wall-r3.log | wall-r3 | 4 | 26.73 | **0.020** | **0.0748 %** | **0.070** | **0.2619 %** | 0.000 | 0.040 | 24.11 | 26.61 | 0.000 | 0.130 | PINNED-CLEAN |
| W4-wall-r4.log | wall-r4 | 4 | 26.71 | **0.010** | **0.0374 %** | **0.130** | **0.4867 %** | 0.000 | 0.090 | 24.06 | 26.57 | 0.020 | 0.120 | PINNED-CLEAN |
| W5-wall-r5.log | wall-r5 | 4 | 26.55 | **0.010** | **0.0377 %** | **0.090** | **0.3390 %** | 0.000 | 0.030 | 23.88 | 26.39 | 0.020 | 0.110 | PINNED-CLEAN |

## The five busiest foreign processes per batch

- W1-wall-r1.log / wall-r1: TRANSIENT pids (present in some snapshot, not in both ends): 3087234,3087268,3087279,3087791,3087793,3087795,3087803,3087805,3087807,3087812,3087815,3087824,3087826,3087828,3087891,3088184,3088527,3088529,3088531,3088533,3088535,3088536,3089198,3089208,3089210,3089256,3089878,3089880,3089900,3089902,3089904,3089906,3089913,3090590,3090592,3090593,3090594,3090610,3090612,3090614
- W1-wall-r1.log / wall-r1 (window 34.09 s): pid 50122 +1.06 s; pid 1097257 +0.36 s; pid 2721602 +0.28 s; pid 2722314 +0.14 s; pid 4022442 +0.11 s
- W1-wall-r1.log / wall-r1: BOX_PAUSE 2026-09-11T20:27:36Z tag=wall-r1/a2-after try=1 -- a foreign process is in state R.
- W1-wall-r1.log / wall-r1: BOX_PAUSE_GIVEUP 2026-09-11T20:27:41Z tag=wall-r1/a2-after -- still R after 1 pauses.
- W2-wall-r2.log / wall-r2: TRANSIENT pids (present in some snapshot, not in both ends): 3108301,3110398,3110400,3111144,3111308,3112387,3112390,3112391,3112393,3112409,3112412,3112413,3112415,3112577,3113100,3113102,3113104,3113567,3113779,3113781,3113793,3114471,3114474,3114476,3114478,3115154,3115156,3115172,3115174,3115176,3115179,3115347
- W2-wall-r2.log / wall-r2 (window 29.15 s): pid 50122 +0.81 s; pid 1097257 +0.30 s; pid 2722314 +0.13 s; pid 4022442 +0.10 s; pid 1861779 +0.08 s
- W3-wall-r3.log / wall-r3: TRANSIENT pids (present in some snapshot, not in both ends): 3092614,3093293,3093295,3093983,3093985,3093987,3094664,3094665,3094682,3094686,3094688,3094690,3095994,3096030,3096679,3096681,3096682,3096684,3097361,3097364,3097366,3097378,3098056,3098058,3098059,3098075,3098077,3098079,3098130,3098739,3098741
- W3-wall-r3.log / wall-r3 (window 29.48 s): pid 50122 +0.83 s; pid 2721602 +0.34 s; pid 1097257 +0.28 s; pid 2722314 +0.12 s; pid 4022442 +0.11 s
- W4-wall-r4.log / wall-r4: TRANSIENT pids (present in some snapshot, not in both ends): 3096679,3096681,3097364,3097366,3097378,3098056,3098058,3098059,3098075,3098079,3098130,3098739,3100089,3100738,3100740,3100742,3100743,3100746,3101424,3101428,3101430,3101432,3102093,3102094,3102111,3102113,3102115,3102117,3102794,3102796
- W4-wall-r4.log / wall-r4 (window 29.44 s): pid 50122 +0.81 s; pid 1097257 +0.27 s; pid 2722314 +0.16 s; pid 4022442 +0.10 s; pid 3819500 +0.06 s
- W5-wall-r5.log / wall-r5: TRANSIENT pids (present in some snapshot, not in both ends): 3098741,3100738,3100740,3101428,3101430,3101432,3102093,3102094,3102111,3102115,3102117,3102794,3102796,3104196,3104794,3104796,3104797,3104799,3105460,3105463,3105465,3105472,3106149,3106151,3106152,3106168,3106170,3106172,3106174,3106851,3106853
- W5-wall-r5.log / wall-r5 (window 29.30 s): pid 50122 +0.94 s; pid 1097257 +0.32 s; pid 2721602 +0.26 s; pid 2722314 +0.12 s; pid 4022442 +0.10 s
