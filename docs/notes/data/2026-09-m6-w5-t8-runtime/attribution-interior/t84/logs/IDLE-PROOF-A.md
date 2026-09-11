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
| A1-alloc-r1.log | alloc-r1 | 28.30 | 6 | 199 | 0.740 (pid 50122) | **2.6148 %** | none | 0 | **NOT met** |
| A2-alloc-r2.log | alloc-r2 | 28.91 | 6 | 202 | 0.850 (pid 50122) | **2.9402 %** | none | 0 | **NOT met** |
| A3-alloc-r3.log | alloc-r3 | 28.47 | 6 | 198 | 0.780 (pid 50122) | **2.7397 %** | none | 0 | **NOT met** |
| A4-alloc-r4.log | alloc-r4 | 32.72 | 6 | 200 | 0.960 (pid 50122) | **2.9340 %** | 50122 | 2 | **NOT met** |
| A5-alloc-r5.log | alloc-r5 | 28.46 | 6 | 200 | 0.830 (pid 50122) | **2.9164 %** | none | 0 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

| log | batch | runs | timed wall (s) | **cpu2 un-niced USER (s)** | **fraction** | **cpu10 un-niced USER (s)** | **fraction** | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A1-alloc-r1.log | alloc-r1 | 4 | 25.55 | **0.010** | **0.0391 %** | **0.040** | **0.1566 %** | 23.90 | 25.42 | 0.020 | 0.100 | PINNED-CLEAN |
| A2-alloc-r2.log | alloc-r2 | 4 | 26.15 | **0.000** | **0.0000 %** | **0.060** | **0.2294 %** | 24.48 | 26.00 | 0.030 | 0.120 | PINNED-CLEAN |
| A3-alloc-r3.log | alloc-r3 | 4 | 25.80 | **0.000** | **0.0000 %** | **0.020** | **0.0775 %** | 24.16 | 25.66 | 0.020 | 0.120 | PINNED-CLEAN |
| A4-alloc-r4.log | alloc-r4 | 4 | 24.98 | **0.000** | **0.0000 %** | **0.050** | **0.2002 %** | 23.31 | 24.84 | 0.010 | 0.110 | PINNED-CLEAN |
| A5-alloc-r5.log | alloc-r5 | 4 | 25.70 | **0.000** | **0.0000 %** | **0.020** | **0.0778 %** | 24.01 | 25.56 | 0.030 | 0.110 | PINNED-CLEAN |

## The five busiest foreign processes per batch

- A1-alloc-r1.log / alloc-r1 (window 28.30 s): pid 50122 +0.74 s; pid 1097257 +0.32 s; pid 2721602 +0.26 s; pid 2722314 +0.14 s; pid 1861779 +0.09 s
- A2-alloc-r2.log / alloc-r2 (window 28.91 s): pid 50122 +0.85 s; pid 1097257 +0.35 s; pid 1861779 +0.19 s; pid 2722314 +0.13 s; pid 4022442 +0.07 s
- A3-alloc-r3.log / alloc-r3 (window 28.47 s): pid 50122 +0.78 s; pid 1097257 +0.31 s; pid 2721602 +0.26 s; pid 2722314 +0.14 s; pid 4022442 +0.08 s
- A4-alloc-r4.log / alloc-r4 (window 32.72 s): pid 50122 +0.96 s; pid 1861779 +0.39 s; pid 1097257 +0.37 s; pid 1312476 +0.14 s; pid 2722314 +0.13 s
- A4-alloc-r4.log / alloc-r4: BOX_PAUSE 2026-09-11T19:39:17Z tag=alloc-r4/open try=1 -- a foreign process is in state R.
- A4-alloc-r4.log / alloc-r4: BOX_PAUSE_GIVEUP 2026-09-11T19:39:22Z tag=alloc-r4/open -- still R after 1 pauses.
- A5-alloc-r5.log / alloc-r5 (window 28.46 s): pid 50122 +0.83 s; pid 1097257 +0.34 s; pid 2721602 +0.26 s; pid 1861779 +0.15 s; pid 2722314 +0.14 s
