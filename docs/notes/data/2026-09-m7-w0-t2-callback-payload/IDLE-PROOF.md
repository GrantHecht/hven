# W5 T8.9r fix1 -- R2 idle proof, per timed batch

Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

**FIX ROUND 3 (settler R13).** `FOREIGN_PS` is read beside `FOREIGN_TICK`, so `ps` states such as `Rsl` are seen; pauses and give-ups are separate columns; every foreign pid and its observed states is listed per batch in the appendix file; and a batch whose EVIDENCE is short of R2' -- an `R` with no re-snapshot, or a missing alternation snapshot -- is UNPROVEN-EVIDENCE, reported apart from UNPROVEN-FRACTION. **No fraction, delta or bar-verdict moved.**

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

**TRANSIENTS AND STATES ARE DISCLOSED (attrib4).** A pid present in one snapshot of a batch but not the other cannot have its CPU delta differenced; the column counts them rather than dropping them silently, and EVERY foreign state seen in any snapshot is listed, not only `R`.

| log | batch | window wall (s) | snapshots | foreign pids | transients | worst foreign cputime delta (s) | worst fraction | states seen | `R` seen | pauses | give-ups | re-snapshots | alternation gaps | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 20-ipmleg-perf.log | ipmleg-perf | 145.13 | 5 | 78 | 5 | 0.270 (pid 13480) | **0.1860 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 9 | **NOT met** |
| 21-ipmleg2-perf.log | ipmleg2-perf | 144.19 | 5 | 79 | 3 | 0.250 (pid 13480) | **0.1734 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 9 | **NOT met** |
| 22-ipmleg3-perf.log | ipmleg3-perf | 144.21 | 5 | 76 | 6 | 0.280 (pid 13480) | **0.1942 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 9 | **NOT met** |
| 23-ipmleg-wall.log | ipmlegwall-wall | 48.41 | 3 | 79 | 0 | 0.100 (pid 13480) | **0.2066 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 3 | met |
| 30-sqpperf-walk-r1-g1-9.log | sqpperf-walk-r1-g1-9 | 4.60 | 11 | 79 | 0 | 0.050 (pid 83618) | **1.0870 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 18 | **NOT met** |
| 31-sqpperf-walk-r1-g10-18.log | sqpperf-walk-r1-g10-18 | 10.63 | 11 | 78 | 2 | 0.050 (pid 83618) | **0.4704 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 18 | **NOT met** |
| 32-sqpperf-walk-r1-g19-22.log | sqpperf-walk-r1-g19-22 | 8.52 | 6 | 82 | 4 | 0.020 (pid 13480) | **0.2347 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 8 | **NOT met** |
| 33-sqpperf-walk-r1-g23-24.log | sqpperf-walk-r1-g23-24 | 6.95 | 4 | 86 | 0 | 0.110 (pid 83618) | **1.5827 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 4 | **NOT met** |
| 34-sqpperf-walk-r1-g25-26.log | sqpperf-walk-r1-g25-26 | 373.26 | 4 | 82 | 8 | 2.130 (pid 83618) | **0.5706 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 4 | **NOT met** |
| 35-sqpperf-walk-r1-g27-27.log | sqpperf-walk-r1-g27-27 | 227.33 | 3 | 82 | 8 | 1.110 (pid 83618) | **0.4883 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 2 | **NOT met** |
| 50-attrib-small.log | attrib-small | 9.23 | 5 | 85 | 2 | 0.090 (pid 83618) | **0.9751 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 6 | **NOT met** |
| 51-attrib-large.log | attrib-large | 63.22 | 5 | 85 | 2 | 0.320 (pid 83618) | **0.5062 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 6 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

**THE NICE-INCLUSIVE REPAIR (attrib4).** The two bold columns are the REPAIRED figure: `user + nice + steal + guest` on the core, with only the run's own `user` seconds subtracted on cpu2 and nothing subtracted on cpu10. The `user`-only columns beside them are the SUPERSEDED figure the earlier proof took its verdict on; they are printed so the two can be compared and are not what any verdict here rests on.

**THE VERDICT HAS TWO PARTS (fix3).** The FRACTION part is R2''s bar and is unchanged. The EVIDENCE part asks whether the log contains what R2' requires: a re-snapshot after every foreign `R`, and a snapshot between consecutive timed runs. A batch is PINNED-CLEAN only when both hold; otherwise the cell names which part failed, and both can fail at once.

| log | batch | runs | timed wall (s) | **cpu2 foreign TASK (s)** | **fraction** | **cpu10 TASK (s)** | **fraction** | cpu2 user-only (s) | cpu10 user-only (s) | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | evidence | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 20-ipmleg-perf.log | ipmleg-perf | 12 | 144.10 | **0.090** | **0.0625 %** | **0.100** | **0.0694 %** | 130.450 | 0.100 | 0.00 | 143.38 | 0.080 | 0.600 | 9 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 21-ipmleg2-perf.log | ipmleg2-perf | 12 | 143.18 | **0.080** | **0.0559 %** | **0.030** | **0.0210 %** | 129.290 | 0.030 | 0.00 | 142.46 | 0.080 | 0.580 | 9 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 22-ipmleg3-perf.log | ipmleg3-perf | 12 | 143.21 | **0.080** | **0.0559 %** | **0.030** | **0.0209 %** | 129.160 | 0.030 | 0.00 | 142.47 | 0.070 | 0.600 | 9 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 23-ipmleg-wall.log | ipmlegwall-wall | 4 | 47.95 | **0.030** | **0.0626 %** | **0.040** | **0.0834 %** | 43.140 | 0.040 | 0.00 | 47.70 | 0.020 | 0.200 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 30-sqpperf-walk-r1-g1-9.log | sqpperf-walk-r1-g1-9 | 27 | 2.15 | **0.160** | **7.4419 %** | **0.000** | **0.0000 %** | 1.610 | 0.000 | 0.00 | 1.99 | 0.110 | 0.010 | 18 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 31-sqpperf-walk-r1-g10-18.log | sqpperf-walk-r1-g10-18 | 27 | 8.17 | **0.140** | **1.7136 %** | **0.140** | **1.7136 %** | 6.690 | 0.140 | 0.00 | 8.00 | 0.120 | 0.050 | 18 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 32-sqpperf-walk-r1-g19-22.log | sqpperf-walk-r1-g19-22 | 12 | 7.27 | **0.060** | **0.8253 %** | **0.000** | **0.0000 %** | 5.960 | 0.000 | 0.00 | 7.17 | 0.050 | 0.040 | 8 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 33-sqpperf-walk-r1-g23-24.log | sqpperf-walk-r1-g23-24 | 6 | 6.19 | **0.020** | **0.3231 %** | **0.050** | **0.8078 %** | 5.240 | 0.050 | 0.00 | 6.14 | 0.040 | 0.030 | 4 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 34-sqpperf-walk-r1-g25-26.log | sqpperf-walk-r1-g25-26 | 6 | 372.53 | **0.050** | **0.0134 %** | **0.200** | **0.0537 %** | 362.870 | 0.200 | 0.00 | 370.75 | 0.050 | 1.680 | 4 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 35-sqpperf-walk-r1-g27-27.log | sqpperf-walk-r1-g27-27 | 3 | 226.85 | **0.030** | **0.0132 %** | **0.130** | **0.0573 %** | 220.920 | 0.130 | 0.00 | 225.78 | 0.010 | 1.030 | 2 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 50-attrib-small.log | attrib-small | 9 | 8.23 | **0.050** | **0.6075 %** | **0.010** | **0.1215 %** | 7.640 | 0.010 | 0.00 | 8.11 | 0.040 | 0.040 | 6 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 51-attrib-large.log | attrib-large | 9 | 62.21 | **0.050** | **0.0804 %** | **0.060** | **0.0964 %** | 56.230 | 0.060 | 0.00 | 61.87 | 0.050 | 0.250 | 6 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |

## The five busiest foreign processes per batch

**Every foreign pid of every batch, with every state observed for it, is `IDLE-PIDS.txt`** (settler R13). The five below are a summary of that file, not a substitute for it.

- 20-ipmleg-perf.log / ipmleg-perf: TRANSIENT pids (present in some snapshot, not in both ends): 743909,746537,746539,746540,747343
- 20-ipmleg-perf.log / ipmleg-perf (window 145.13 s): pid 13480 +0.27 s; pid 13501 +0.24 s; pid 716398 +0.15 s; pid 710795 +0.14 s; pid 672469 +0.13 s
- 21-ipmleg2-perf.log / ipmleg2-perf: TRANSIENT pids (present in some snapshot, not in both ends): 748457,748460,748461
- 21-ipmleg2-perf.log / ipmleg2-perf (window 144.19 s): pid 13480 +0.25 s; pid 13501 +0.24 s; pid 716398 +0.13 s; pid 672469 +0.12 s; pid 710795 +0.10 s
- 22-ipmleg3-perf.log / ipmleg3-perf: TRANSIENT pids (present in some snapshot, not in both ends): 747343,748042,748070,750248,750642,750702
- 22-ipmleg3-perf.log / ipmleg3-perf (window 144.21 s): pid 13480 +0.28 s; pid 13501 +0.24 s; pid 710795 +0.12 s; pid 672469 +0.12 s; pid 716398 +0.10 s
- 23-ipmleg-wall.log / ipmlegwall-wall (window 48.41 s): pid 13480 +0.10 s; pid 13501 +0.08 s; pid 716398 +0.05 s; pid 943 +0.04 s; pid 672469 +0.04 s
- 30-sqpperf-walk-r1-g1-9.log / sqpperf-walk-r1-g1-9 (window 4.60 s): pid 83618 +0.05 s; pid 716398 +0.01 s; pid 710795 +0.01 s; pid 672469 +0.01 s; pid 13480 +0.01 s
- 31-sqpperf-walk-r1-g10-18.log / sqpperf-walk-r1-g10-18: TRANSIENT pids (present in some snapshot, not in both ends): 750248,756840
- 31-sqpperf-walk-r1-g10-18.log / sqpperf-walk-r1-g10-18 (window 10.63 s): pid 83618 +0.05 s; pid 716398 +0.02 s; pid 13501 +0.02 s; pid 13480 +0.02 s; pid 926 +0.01 s
- 32-sqpperf-walk-r1-g19-22.log / sqpperf-walk-r1-g19-22: TRANSIENT pids (present in some snapshot, not in both ends): 760320,760322,760323,760324
- 32-sqpperf-walk-r1-g19-22.log / sqpperf-walk-r1-g19-22 (window 8.52 s): pid 83618 +0.02 s; pid 13501 +0.02 s; pid 13480 +0.02 s; pid 710795 +0.01 s; pid 997 +0.00 s
- 33-sqpperf-walk-r1-g23-24.log / sqpperf-walk-r1-g23-24 (window 6.95 s): pid 83618 +0.11 s; pid 764151 +0.02 s; pid 13501 +0.02 s; pid 13480 +0.02 s; pid 84052 +0.01 s
- 34-sqpperf-walk-r1-g25-26.log / sqpperf-walk-r1-g25-26: TRANSIENT pids (present in some snapshot, not in both ends): 764013,764034,764072,764629,767408,767573,767868,768145
- 34-sqpperf-walk-r1-g25-26.log / sqpperf-walk-r1-g25-26 (window 373.26 s): pid 83618 +2.13 s; pid 13480 +0.60 s; pid 13501 +0.43 s; pid 710795 +0.36 s; pid 760324 +0.35 s
- 35-sqpperf-walk-r1-g27-27.log / sqpperf-walk-r1-g27-27: TRANSIENT pids (present in some snapshot, not in both ends): 767408,767573,767868,768894,769924,770124,770441,771007
- 35-sqpperf-walk-r1-g27-27.log / sqpperf-walk-r1-g27-27 (window 227.33 s): pid 83618 +1.11 s; pid 13480 +0.36 s; pid 13501 +0.26 s; pid 672469 +0.22 s; pid 716398 +0.21 s
- 50-attrib-small.log / attrib-small: TRANSIENT pids (present in some snapshot, not in both ends): 775489,776773
- 50-attrib-small.log / attrib-small (window 9.23 s): pid 83618 +0.09 s; pid 13480 +0.02 s; pid 710795 +0.01 s; pid 672469 +0.01 s; pid 13501 +0.01 s
- 51-attrib-large.log / attrib-large: TRANSIENT pids (present in some snapshot, not in both ends): 776773,778162
- 51-attrib-large.log / attrib-large (window 63.22 s): pid 83618 +0.32 s; pid 13480 +0.12 s; pid 13501 +0.10 s; pid 710795 +0.06 s; pid 672469 +0.06 s

## The two kinds of UNPROVEN, counted (fix3)

| batches | PINNED-CLEAN | UNPROVEN-FRACTION | UNPROVEN-EVIDENCE |
|---:|---:|---:|---:|
| 12 | 0 | 5 | 12 |

A batch failing both is counted in both unproven columns, so the three need not sum to the first.
