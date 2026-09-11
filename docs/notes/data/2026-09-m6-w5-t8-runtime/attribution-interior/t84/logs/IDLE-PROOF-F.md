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
| F1-wallpf-r1.log | wallpf-r1 | 14.43 | 4 | 203 | 0.390 (pid 50122) | **2.7027 %** | none | 0 | **NOT met** |
| F2-wallpf-r2.log | wallpf-r2 | 14.26 | 4 | 199 | 0.400 (pid 50122) | **2.8050 %** | none | 0 | **NOT met** |
| F3-wallpf-r3.log | wallpf-r3 | 14.55 | 4 | 205 | 0.400 (pid 50122) | **2.7491 %** | none | 0 | **NOT met** |
| F4-wallpf-r4.log | wallpf-r4 | 14.28 | 4 | 202 | 0.400 (pid 50122) | **2.8011 %** | none | 0 | **NOT met** |
| F5-wallpf-r5.log | wallpf-r5 | 14.50 | 4 | 203 | 0.410 (pid 50122) | **2.8276 %** | none | 0 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

| log | batch | runs | timed wall (s) | **cpu2 un-niced USER (s)** | **fraction** | **cpu10 un-niced USER (s)** | **fraction** | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| F1-wallpf-r1.log | wallpf-r1 | 2 | 12.83 | **0.000** | **0.0000 %** | **0.030** | **0.2338 %** | 11.52 | 12.76 | 0.010 | 0.060 | PINNED-CLEAN |
| F2-wallpf-r2.log | wallpf-r2 | 2 | 12.67 | **0.000** | **0.0000 %** | **0.010** | **0.0789 %** | 11.33 | 12.61 | 0.010 | 0.060 | PINNED-CLEAN |
| F3-wallpf-r3.log | wallpf-r3 | 2 | 12.96 | **0.000** | **0.0000 %** | **0.020** | **0.1543 %** | 11.60 | 12.89 | 0.010 | 0.060 | PINNED-CLEAN |
| F4-wallpf-r4.log | wallpf-r4 | 2 | 12.68 | **0.000** | **0.0000 %** | **0.040** | **0.3155 %** | 11.36 | 12.61 | 0.010 | 0.060 | PINNED-CLEAN |
| F5-wallpf-r5.log | wallpf-r5 | 2 | 12.87 | **0.000** | **0.0000 %** | **0.000** | **0.0000 %** | 11.56 | 12.80 | 0.010 | 0.050 | PINNED-CLEAN |

## The five busiest foreign processes per batch

- F1-wallpf-r1.log / wallpf-r1 (window 14.43 s): pid 50122 +0.39 s; pid 1097257 +0.18 s; pid 2722314 +0.06 s; pid 1861779 +0.06 s; pid 4022442 +0.05 s
- F2-wallpf-r2.log / wallpf-r2 (window 14.26 s): pid 50122 +0.40 s; pid 1097257 +0.16 s; pid 3819500 +0.05 s; pid 2722314 +0.05 s; pid 1861779 +0.04 s
- F3-wallpf-r3.log / wallpf-r3 (window 14.55 s): pid 50122 +0.40 s; pid 1097257 +0.17 s; pid 2722314 +0.09 s; pid 4022442 +0.04 s; pid 2722106 +0.04 s
- F4-wallpf-r4.log / wallpf-r4 (window 14.28 s): pid 50122 +0.40 s; pid 2721602 +0.25 s; pid 1097257 +0.16 s; pid 4022442 +0.05 s; pid 2722314 +0.05 s
- F5-wallpf-r5.log / wallpf-r5 (window 14.50 s): pid 50122 +0.41 s; pid 1097257 +0.15 s; pid 2722314 +0.06 s; pid 2722106 +0.05 s; pid 4022442 +0.04 s
