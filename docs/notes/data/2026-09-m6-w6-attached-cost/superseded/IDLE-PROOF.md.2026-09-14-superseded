# W5 T8.9r fix1 -- R2 idle proof, per timed batch

Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

**FIX ROUND 3 (settler R13).** `FOREIGN_PS` is read beside `FOREIGN_TICK`, so `ps` states such as `Rsl` are seen; pauses and give-ups are separate columns; every foreign pid and its observed states is listed per batch in the appendix file; and a batch whose EVIDENCE is short of R2' -- an `R` with no re-snapshot, or a missing alternation snapshot -- is UNPROVEN-EVIDENCE, reported apart from UNPROVEN-FRACTION. **No fraction, delta or bar-verdict moved.**

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

**TRANSIENTS AND STATES ARE DISCLOSED (attrib4).** A pid present in one snapshot of a batch but not the other cannot have its CPU delta differenced; the column counts them rather than dropping them silently, and EVERY foreign state seen in any snapshot is listed, not only `R`.

| log | batch | window wall (s) | snapshots | foreign pids | transients | worst foreign cputime delta (s) | worst fraction | states seen | `R` seen | pauses | give-ups | re-snapshots | alternation gaps | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 10-ipmleg-perf.log | ipmleg-perf | 72.73 | 5 | 69 | 4 | 0.140 (pid 13480) | **0.1925 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 3 | **NOT met** |
| 11-ipmleg-wall.log | ipmleg-wall | 24.10 | 3 | 71 | 0 | 0.040 (pid 13480) | **0.1660 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 1 | met |
| 12-hs-perf.log | hs-perf | 112.85 | 5 | 71 | 0 | 0.170 (pid 13480) | **0.1506 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 15 | met |
| 13-hs-wall.log | hs-wall | 36.98 | 3 | 71 | 0 | 0.060 (pid 13480) | **0.1622 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 5 | met |
| 20-sqpperf-ipm-r1.log | sqpperf-ipm-r1-gall | 101.00 | 29 | 70 | 6 | 0.180 (pid 13480) | **0.1782 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 81 | **NOT met** |
| 20-sqpperf-ipm-r2.log | sqpperf-ipm-r2-gall | 101.37 | 29 | 73 | 0 | 0.200 (pid 13480) | **0.1973 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 81 | met |
| 20-sqpperf-ipm-r3.log | sqpperf-ipm-r3-gall | 101.31 | 29 | 73 | 0 | 0.180 (pid 13480) | **0.1777 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 81 | met |
| 21-sqpperf-ssn-r1.log | sqpperf-ssn-r1-gall | 79.71 | 29 | 70 | 6 | 0.150 (pid 83618) | **0.1882 %** | Rl+,S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | 13480 | 0 | 0 | 0 | 81 | **NOT met** |
| 21-sqpperf-ssn-r2.log | sqpperf-ssn-r2-gall | 79.71 | 29 | 73 | 0 | 0.160 (pid 83618) | **0.2007 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 81 | met |
| 21-sqpperf-ssn-r3.log | sqpperf-ssn-r3-gall | 79.68 | 29 | 73 | 0 | 0.180 (pid 83618) | **0.2259 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 81 | met |
| 22-sqpperf-walk-r1-g1.log | sqpperf-walk-r1-g1 | 5.25 | 11 | 73 | 0 | 0.060 (pid 83618) | **1.1429 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 27 | **NOT met** |
| 22-sqpperf-walk-r1-g2.log | sqpperf-walk-r1-g2 | 13.29 | 11 | 73 | 0 | 0.050 (pid 83618) | **0.3762 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 27 | met |
| 22-sqpperf-walk-r1-g3.log | sqpperf-walk-r1-g3 | 811.52 | 11 | 67 | 16 | 1.210 (pid 13480) | **0.1491 %** | S,S+,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 27 | **NOT met** |
| 23-sqpperf-walk-r2-g1.log | sqpperf-walk-r2-g1 | 5.29 | 11 | 74 | 0 | 0.050 (pid 83618) | **0.9452 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 27 | **NOT met** |
| 23-sqpperf-walk-r2-g19-22.log | sqpperf-walk-r2-g19-22 | 10.85 | 6 | 72 | 5 | 0.030 (pid 13501) | **0.2765 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 12 | **NOT met** |
| 23-sqpperf-walk-r2-g2.log | sqpperf-walk-r2-g2 | 13.25 | 11 | 72 | 5 | 0.050 (pid 83618) | **0.3774 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 27 | **NOT met** |
| 23-sqpperf-walk-r2-g23-27.log | sqpperf-walk-r2-g23-27 | 801.04 | 7 | 69 | 19 | 1.220 (pid 13480) | **0.1523 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 15 | **NOT met** |
| 24-sqpperf-walk-r3-g1.log | sqpperf-walk-r3-g1 | 5.33 | 11 | 76 | 0 | 0.040 (pid 83618) | **0.7505 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 27 | **NOT met** |
| 24-sqpperf-walk-r3-g19-22.log | sqpperf-walk-r3-g19-22 | 10.78 | 6 | 70 | 0 | 0.030 (pid 13501) | **0.2783 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 12 | met |
| 24-sqpperf-walk-r3-g2.log | sqpperf-walk-r3-g2 | 13.29 | 11 | 74 | 5 | 0.050 (pid 83618) | **0.3762 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 27 | **NOT met** |
| 24-sqpperf-walk-r3-g23-27.log | sqpperf-walk-r3-g23-27 | 806.30 | 7 | 66 | 15 | 46.720 (pid 16979) | **5.7944 %** | R,S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | 16979 | 1 | 1 | 0 | 15 | **NOT met** |
| 30-sqpwall-ipm-a00.log | sqpwall-ipm-a00 | 23.49 | 2 | 69 | 0 | 0.040 (pid 13480) | **0.1703 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | met |
| 30-sqpwall-ipm-a01.log | sqpwall-ipm-a01 | 23.49 | 2 | 69 | 0 | 0.050 (pid 13480) | **0.2129 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | met |
| 30-sqpwall-ipm-a10.log | sqpwall-ipm-a10 | 23.50 | 2 | 69 | 0 | 0.050 (pid 13480) | **0.2128 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | met |
| 30-sqpwall-ipm-a11.log | sqpwall-ipm-a11 | 23.50 | 2 | 69 | 0 | 0.040 (pid 13480) | **0.1702 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | met |
| 30-sqpwall-ssn-a00.log | sqpwall-ssn-a00 | 18.13 | 2 | 69 | 0 | 0.040 (pid 13480) | **0.2206 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | met |
| 30-sqpwall-ssn-a01.log | sqpwall-ssn-a01 | 18.14 | 2 | 69 | 0 | 0.040 (pid 13480) | **0.2205 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | met |
| 30-sqpwall-ssn-a10.log | sqpwall-ssn-a10 | 18.10 | 2 | 69 | 0 | 0.040 (pid 13501) | **0.2210 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | met |
| 30-sqpwall-ssn-a11.log | sqpwall-ssn-a11 | 18.14 | 2 | 69 | 0 | 0.050 (pid 943) | **0.2756 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | met |
| 30-sqpwall-walk-a00.log | sqpwall-walk-a00 | 206.85 | 2 | 66 | 6 | 0.330 (pid 13480) | **0.1595 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | **NOT met** |
| 30-sqpwall-walk-a01.log | sqpwall-walk-a01 | 206.98 | 2 | 68 | 2 | 0.330 (pid 13480) | **0.1594 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | **NOT met** |
| 30-sqpwall-walk-a10.log | sqpwall-walk-a10 | 204.76 | 2 | 66 | 6 | 0.310 (pid 13480) | **0.1514 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | **NOT met** |
| 30-sqpwall-walk-a11.log | sqpwall-walk-a11 | 205.33 | 2 | 67 | 4 | 0.310 (pid 13480) | **0.1510 %** | S,S<,S<Ls,S<sl,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl | none | 0 | 0 | 0 | 0 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

**THE NICE-INCLUSIVE REPAIR (attrib4).** The two bold columns are the REPAIRED figure: `user + nice + steal + guest` on the core, with only the run's own `user` seconds subtracted on cpu2 and nothing subtracted on cpu10. The `user`-only columns beside them are the SUPERSEDED figure the earlier proof took its verdict on; they are printed so the two can be compared and are not what any verdict here rests on.

**THE VERDICT HAS TWO PARTS (fix3).** The FRACTION part is R2''s bar and is unchanged. The EVIDENCE part asks whether the log contains what R2' requires: a re-snapshot after every foreign `R`, and a snapshot between consecutive timed runs. A batch is PINNED-CLEAN only when both hold; otherwise the cell names which part failed, and both can fail at once.

| log | batch | runs | timed wall (s) | **cpu2 foreign TASK (s)** | **fraction** | **cpu10 TASK (s)** | **fraction** | cpu2 user-only (s) | cpu10 user-only (s) | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | evidence | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 10-ipmleg-perf.log | ipmleg-perf | 6 | 71.92 | **0.030** | **0.0417 %** | **0.070** | **0.0973 %** | 65.190 | 0.070 | 0.00 | 71.58 | 0.060 | 0.260 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 11-ipmleg-wall.log | ipmleg-wall | 2 | 23.71 | **0.000** | **0.0000 %** | **0.030** | **0.1265 %** | 21.510 | 0.030 | 0.00 | 23.61 | 0.010 | 0.080 | 1 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 12-hs-perf.log | hs-perf | 18 | 111.83 | **0.080** | **0.0715 %** | **1.090** | **0.9747 %** | 110.230 | 1.090 | 0.00 | 111.05 | 0.090 | 0.690 | 15 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 13-hs-wall.log | hs-wall | 6 | 36.51 | **0.040** | **0.1096 %** | **0.020** | **0.0548 %** | 36.060 | 0.020 | 0.00 | 36.25 | 0.020 | 0.220 | 5 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 20-sqpperf-ipm-r1.log | sqpperf-ipm-r1-gall | 108 | 94.15 | **0.600** | **0.6373 %** | **0.150** | **0.1593 %** | 83.820 | 0.150 | 0.00 | 92.98 | 0.460 | 0.410 | 81 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 20-sqpperf-ipm-r2.log | sqpperf-ipm-r2-gall | 108 | 94.48 | **0.570** | **0.6033 %** | **0.180** | **0.1905 %** | 84.330 | 0.180 | 0.00 | 93.36 | 0.430 | 0.420 | 81 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 20-sqpperf-ipm-r3.log | sqpperf-ipm-r3-gall | 108 | 94.49 | **0.560** | **0.5927 %** | **0.130** | **0.1376 %** | 84.170 | 0.130 | 0.00 | 93.32 | 0.450 | 0.420 | 81 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 21-sqpperf-ssn-r1.log | sqpperf-ssn-r1-gall | 108 | 72.75 | **0.590** | **0.8110 %** | **0.090** | **0.1237 %** | 61.360 | 0.090 | 0.00 | 71.78 | 0.470 | 0.330 | `R` at `sqpperf-ssn-r1-gall/f7_n2000_bound_neutral` with NO re-snapshot; 81 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 21-sqpperf-ssn-r2.log | sqpperf-ssn-r2-gall | 108 | 72.73 | **0.520** | **0.7150 %** | **0.140** | **0.1925 %** | 61.420 | 0.140 | 0.00 | 71.87 | 0.450 | 0.310 | 81 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 21-sqpperf-ssn-r3.log | sqpperf-ssn-r3-gall | 108 | 72.74 | **0.500** | **0.6874 %** | **0.110** | **0.1512 %** | 61.320 | 0.110 | 0.00 | 71.84 | 0.470 | 0.320 | 81 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 22-sqpperf-walk-r1-g1.log | sqpperf-walk-r1-g1 | 36 | 2.79 | **0.150** | **5.3763 %** | **0.000** | **0.0000 %** | 2.100 | 0.000 | 0.00 | 2.68 | 0.120 | 0.020 | 27 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 22-sqpperf-walk-r1-g2.log | sqpperf-walk-r1-g2 | 36 | 10.79 | **0.180** | **1.6682 %** | **0.050** | **0.4634 %** | 8.800 | 0.050 | 0.00 | 10.60 | 0.150 | 0.050 | 27 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 22-sqpperf-walk-r1-g3.log | sqpperf-walk-r1-g3 | 36 | 809.09 | **0.260** | **0.0321 %** | **0.250** | **0.0309 %** | 786.790 | 0.250 | 0.00 | 805.22 | 0.200 | 3.550 | 27 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 23-sqpperf-walk-r2-g1.log | sqpperf-walk-r2-g1 | 36 | 2.78 | **0.180** | **6.4748 %** | **0.000** | **0.0000 %** | 2.110 | 0.000 | 0.00 | 2.59 | 0.190 | 0.020 | 27 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 23-sqpperf-walk-r2-g19-22.log | sqpperf-walk-r2-g19-22 | 16 | 9.63 | **0.080** | **0.8307 %** | **0.030** | **0.3115 %** | 7.930 | 0.030 | 0.00 | 9.53 | 0.060 | 0.050 | 12 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 23-sqpperf-walk-r2-g2.log | sqpperf-walk-r2-g2 | 36 | 10.78 | **0.180** | **1.6698 %** | **0.010** | **0.0928 %** | 8.780 | 0.010 | 0.00 | 10.57 | 0.140 | 0.050 | 27 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 23-sqpperf-walk-r2-g23-27.log | sqpperf-walk-r2-g23-27 | 20 | 799.60 | **0.160** | **0.0200 %** | **0.610** | **0.0763 %** | 778.900 | 0.610 | 0.00 | 795.91 | 0.100 | 3.500 | 15 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 24-sqpperf-walk-r3-g1.log | sqpperf-walk-r3-g1 | 36 | 2.80 | **0.180** | **6.4286 %** | **0.010** | **0.3571 %** | 2.120 | 0.010 | 0.00 | 2.61 | 0.170 | 0.020 | 27 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 24-sqpperf-walk-r3-g19-22.log | sqpperf-walk-r3-g19-22 | 16 | 9.62 | **0.070** | **0.7277 %** | **0.020** | **0.2079 %** | 7.920 | 0.020 | 0.00 | 9.54 | 0.040 | 0.050 | 12 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 24-sqpperf-walk-r3-g2.log | sqpperf-walk-r3-g2 | 36 | 10.78 | **0.180** | **1.6698 %** | **0.020** | **0.1855 %** | 8.810 | 0.020 | 0.00 | 10.52 | 0.170 | 0.040 | 27 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| 24-sqpperf-walk-r3-g23-27.log | sqpperf-walk-r3-g23-27 | 20 | 799.87 | **0.200** | **0.0250 %** | **0.900** | **0.1125 %** | 779.110 | 0.900 | 0.00 | 796.02 | 0.130 | 3.600 | `R` at `sqpperf-walk-r3-g23-27/f7_n20000_bound_corrupted` with NO re-snapshot; 15 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| 30-sqpwall-ipm-a00.log | sqpwall-ipm-a00 | 1 | 23.30 | **0.000** | **0.0000 %** | **0.030** | **0.1288 %** | 20.750 | 0.030 | 0.00 | 22.93 | 0.000 | 0.110 | complete | PINNED-CLEAN |
| 30-sqpwall-ipm-a01.log | sqpwall-ipm-a01 | 1 | 23.30 | **0.010** | **0.0429 %** | **0.010** | **0.0429 %** | 20.740 | 0.010 | 0.00 | 22.91 | 0.000 | 0.100 | complete | PINNED-CLEAN |
| 30-sqpwall-ipm-a10.log | sqpwall-ipm-a10 | 1 | 23.32 | **0.000** | **0.0000 %** | **0.010** | **0.0429 %** | 20.750 | 0.010 | 0.00 | 22.92 | 0.000 | 0.100 | complete | PINNED-CLEAN |
| 30-sqpwall-ipm-a11.log | sqpwall-ipm-a11 | 1 | 23.32 | **0.000** | **0.0000 %** | **0.030** | **0.1286 %** | 20.720 | 0.030 | 0.00 | 22.96 | 0.010 | 0.100 | complete | PINNED-CLEAN |
| 30-sqpwall-ssn-a00.log | sqpwall-ssn-a00 | 1 | 17.95 | **0.010** | **0.0557 %** | **0.130** | **0.7242 %** | 15.080 | 0.130 | 0.00 | 17.58 | 0.000 | 0.080 | complete | **UNPROVEN-FRACTION** |
| 30-sqpwall-ssn-a01.log | sqpwall-ssn-a01 | 1 | 17.94 | **0.000** | **0.0000 %** | **0.030** | **0.1672 %** | 15.020 | 0.030 | 0.00 | 17.58 | 0.000 | 0.080 | complete | PINNED-CLEAN |
| 30-sqpwall-ssn-a10.log | sqpwall-ssn-a10 | 1 | 17.91 | **0.010** | **0.0558 %** | **0.010** | **0.0558 %** | 15.020 | 0.010 | 0.00 | 17.52 | 0.000 | 0.080 | complete | PINNED-CLEAN |
| 30-sqpwall-ssn-a11.log | sqpwall-ssn-a11 | 1 | 17.94 | **0.000** | **0.0000 %** | **0.020** | **0.1115 %** | 15.030 | 0.020 | 0.00 | 17.59 | 0.000 | 0.080 | complete | PINNED-CLEAN |
| 30-sqpwall-walk-a00.log | sqpwall-walk-a00 | 1 | 206.66 | **0.000** | **0.0000 %** | **0.130** | **0.0629 %** | 199.520 | 0.130 | 0.00 | 205.39 | 0.070 | 0.940 | complete | PINNED-CLEAN |
| 30-sqpwall-walk-a01.log | sqpwall-walk-a01 | 1 | 206.79 | **0.000** | **0.0000 %** | **0.100** | **0.0484 %** | 199.450 | 0.100 | 0.00 | 205.40 | 0.060 | 1.010 | complete | PINNED-CLEAN |
| 30-sqpwall-walk-a10.log | sqpwall-walk-a10 | 1 | 204.57 | **0.000** | **0.0000 %** | **0.080** | **0.0391 %** | 198.860 | 0.080 | 0.00 | 203.36 | 0.100 | 0.870 | complete | PINNED-CLEAN |
| 30-sqpwall-walk-a11.log | sqpwall-walk-a11 | 1 | 205.14 | **0.000** | **0.0000 %** | **0.040** | **0.0195 %** | 199.170 | 0.040 | 0.00 | 203.79 | 0.100 | 0.990 | complete | PINNED-CLEAN |

## The five busiest foreign processes per batch

- 10-ipmleg-perf.log / ipmleg-perf: TRANSIENT pids (present in some snapshot, not in both ends): 448796,448797,454244,454245
- 10-ipmleg-perf.log / ipmleg-perf (window 72.73 s): pid 13480 +0.14 s; pid 13501 +0.10 s; pid 83618 +0.02 s; pid 1289 +0.02 s; pid 669 +0.01 s
- 11-ipmleg-wall.log / ipmleg-wall (window 24.10 s): pid 13501 +0.04 s; pid 13480 +0.04 s; pid 924 +0.01 s; pid 83618 +0.01 s; pid 1289 +0.01 s
- 12-hs-perf.log / hs-perf (window 112.85 s): pid 13480 +0.17 s; pid 13501 +0.12 s; pid 1289 +0.03 s; pid 83618 +0.02 s; pid 926 +0.01 s
- 13-hs-wall.log / hs-wall (window 36.98 s): pid 13480 +0.06 s; pid 13501 +0.04 s; pid 83618 +0.02 s; pid 911 +0.01 s; pid 1289 +0.01 s
- 20-sqpperf-ipm-r1.log / sqpperf-ipm-r1-gall: TRANSIENT pids (present in some snapshot, not in both ends): 453855,454244,454245,462148,464071,464072
- 20-sqpperf-ipm-r1.log / sqpperf-ipm-r1-gall (window 101.00 s): pid 13480 +0.18 s; pid 83618 +0.16 s; pid 13501 +0.16 s; pid 1289 +0.03 s; pid 924 +0.01 s
- 20-sqpperf-ipm-r2.log / sqpperf-ipm-r2-gall (window 101.37 s): pid 13480 +0.20 s; pid 13501 +0.16 s; pid 83618 +0.15 s; pid 1289 +0.03 s; pid 926 +0.01 s
- 20-sqpperf-ipm-r3.log / sqpperf-ipm-r3-gall (window 101.31 s): pid 83618 +0.18 s; pid 13480 +0.18 s; pid 13501 +0.14 s; pid 943 +0.04 s; pid 1289 +0.04 s
- 21-sqpperf-ssn-r1.log / sqpperf-ssn-r1-gall: TRANSIENT pids (present in some snapshot, not in both ends): 462148,464071,464072,485097,487851,487852
- 21-sqpperf-ssn-r1.log / sqpperf-ssn-r1-gall (window 79.71 s): pid 83618 +0.15 s; pid 13480 +0.14 s; pid 13501 +0.12 s; pid 1289 +0.03 s; pid 924 +0.02 s
- 21-sqpperf-ssn-r2.log / sqpperf-ssn-r2-gall (window 79.71 s): pid 83618 +0.16 s; pid 13480 +0.15 s; pid 13501 +0.13 s; pid 1289 +0.02 s; pid 4117 +0.01 s
- 21-sqpperf-ssn-r3.log / sqpperf-ssn-r3-gall (window 79.68 s): pid 83618 +0.18 s; pid 13480 +0.16 s; pid 13501 +0.12 s; pid 1289 +0.03 s; pid 924 +0.01 s
- 22-sqpperf-walk-r1-g1.log / sqpperf-walk-r1-g1 (window 5.25 s): pid 83618 +0.06 s; pid 13480 +0.02 s; pid 13501 +0.01 s; pid 997 +0.00 s; pid 989 +0.00 s
- 22-sqpperf-walk-r1-g2.log / sqpperf-walk-r1-g2 (window 13.29 s): pid 83618 +0.05 s; pid 13501 +0.02 s; pid 13480 +0.02 s; pid 1289 +0.01 s; pid 997 +0.00 s
- 22-sqpperf-walk-r1-g3.log / sqpperf-walk-r1-g3: TRANSIENT pids (present in some snapshot, not in both ends): 445221,445228,445371,485097,487851,487852,514080,514083,514084,514366,514378,514399,514400,514456,514565,514599
- 22-sqpperf-walk-r1-g3.log / sqpperf-walk-r1-g3 (window 811.52 s): pid 13480 +1.21 s; pid 13501 +0.84 s; pid 1289 +0.22 s; pid 83618 +0.14 s; pid 924 +0.08 s
- 23-sqpperf-walk-r2-g1.log / sqpperf-walk-r2-g1 (window 5.29 s): pid 83618 +0.05 s; pid 13501 +0.01 s; pid 13480 +0.01 s; pid 1289 +0.01 s; pid 997 +0.00 s
- 23-sqpperf-walk-r2-g19-22.log / sqpperf-walk-r2-g19-22: TRANSIENT pids (present in some snapshot, not in both ends): 519327,520464,521462,522006,522558
- 23-sqpperf-walk-r2-g19-22.log / sqpperf-walk-r2-g19-22 (window 10.85 s): pid 13501 +0.03 s; pid 83618 +0.02 s; pid 13480 +0.01 s; pid 997 +0.00 s; pid 989 +0.00 s
- 23-sqpperf-walk-r2-g2.log / sqpperf-walk-r2-g2: TRANSIENT pids (present in some snapshot, not in both ends): 515076,515087,518420,519327,520464
- 23-sqpperf-walk-r2-g2.log / sqpperf-walk-r2-g2 (window 13.25 s): pid 83618 +0.05 s; pid 13480 +0.03 s; pid 13501 +0.02 s; pid 997 +0.00 s; pid 989 +0.00 s
- 23-sqpperf-walk-r2-g23-27.log / sqpperf-walk-r2-g23-27: TRANSIENT pids (present in some snapshot, not in both ends): 514378,514399,514400,522006,522558,523134,523414,524095,524119,524120,524173,524175,524474,524599,524668,524669,524834,524840,524852
- 23-sqpperf-walk-r2-g23-27.log / sqpperf-walk-r2-g23-27 (window 801.04 s): pid 13480 +1.22 s; pid 13501 +0.88 s; pid 83618 +0.14 s; pid 1289 +0.13 s; pid 943 +0.08 s
- 24-sqpperf-walk-r3-g1.log / sqpperf-walk-r3-g1 (window 5.33 s): pid 83618 +0.04 s; pid 13480 +0.02 s; pid 13501 +0.01 s; pid 997 +0.00 s; pid 989 +0.00 s
- 24-sqpperf-walk-r3-g19-22.log / sqpperf-walk-r3-g19-22 (window 10.78 s): pid 83618 +0.03 s; pid 13501 +0.03 s; pid 13480 +0.02 s; pid 924 +0.01 s; pid 912 +0.01 s
- 24-sqpperf-walk-r3-g2.log / sqpperf-walk-r3-g2: TRANSIENT pids (present in some snapshot, not in both ends): 525346,525348,528803,531033,531059
- 24-sqpperf-walk-r3-g2.log / sqpperf-walk-r3-g2 (window 13.29 s): pid 83618 +0.05 s; pid 13501 +0.02 s; pid 13480 +0.01 s; pid 997 +0.00 s; pid 989 +0.00 s
- 24-sqpperf-walk-r3-g23-27.log / sqpperf-walk-r3-g23-27: TRANSIENT pids (present in some snapshot, not in both ends): 400249,524599,524668,524669,533443,533445,534021,534870,534917,534918,535003,535548,535605,535606,535691
- 24-sqpperf-walk-r3-g23-27.log / sqpperf-walk-r3-g23-27 (window 806.30 s): pid 16979 +46.72 s; pid 13480 +1.25 s; pid 13501 +0.94 s; pid 83618 +0.14 s; pid 1289 +0.14 s
- 24-sqpperf-walk-r3-g23-27.log / sqpperf-walk-r3-g23-27: BOX_PAUSE 2026-09-14T12:08:00Z tag=sqpperf-walk-r3-g23-27/f7_n20000_bound_corrupted try=1 -- a foreign process is in state R.
- 24-sqpperf-walk-r3-g23-27.log / sqpperf-walk-r3-g23-27: BOX_PAUSE_GIVEUP 2026-09-14T12:08:05Z tag=sqpperf-walk-r3-g23-27/f7_n20000_bound_corrupted -- still R after 1 pauses.
- 30-sqpwall-ipm-a00.log / sqpwall-ipm-a00 (window 23.49 s): pid 13480 +0.04 s; pid 13501 +0.03 s; pid 83618 +0.01 s; pid 1369 +0.01 s; pid 997 +0.00 s
- 30-sqpwall-ipm-a01.log / sqpwall-ipm-a01 (window 23.49 s): pid 13480 +0.05 s; pid 13501 +0.04 s; pid 1289 +0.01 s; pid 997 +0.00 s; pid 989 +0.00 s
- 30-sqpwall-ipm-a10.log / sqpwall-ipm-a10 (window 23.50 s): pid 13480 +0.05 s; pid 13501 +0.04 s; pid 912 +0.01 s; pid 83618 +0.01 s; pid 1291 +0.01 s
- 30-sqpwall-ipm-a11.log / sqpwall-ipm-a11 (window 23.50 s): pid 13480 +0.04 s; pid 13501 +0.03 s; pid 924 +0.02 s; pid 83618 +0.02 s; pid 916 +0.01 s
- 30-sqpwall-ssn-a00.log / sqpwall-ssn-a00 (window 18.13 s): pid 13480 +0.04 s; pid 13501 +0.03 s; pid 83618 +0.02 s; pid 924 +0.01 s; pid 1289 +0.01 s
- 30-sqpwall-ssn-a01.log / sqpwall-ssn-a01 (window 18.14 s): pid 13480 +0.04 s; pid 13501 +0.02 s; pid 1289 +0.01 s; pid 997 +0.00 s; pid 989 +0.00 s
- 30-sqpwall-ssn-a10.log / sqpwall-ssn-a10 (window 18.10 s): pid 13501 +0.04 s; pid 13480 +0.03 s; pid 83618 +0.01 s; pid 997 +0.00 s; pid 989 +0.00 s
- 30-sqpwall-ssn-a11.log / sqpwall-ssn-a11 (window 18.14 s): pid 943 +0.05 s; pid 13501 +0.03 s; pid 13480 +0.03 s; pid 911 +0.01 s; pid 83618 +0.01 s
- 30-sqpwall-walk-a00.log / sqpwall-walk-a00: TRANSIENT pids (present in some snapshot, not in both ends): 535548,535605,535606,540762,540769,540770
- 30-sqpwall-walk-a00.log / sqpwall-walk-a00 (window 206.85 s): pid 13480 +0.33 s; pid 13501 +0.24 s; pid 83618 +0.03 s; pid 1289 +0.03 s; pid 924 +0.01 s
- 30-sqpwall-walk-a01.log / sqpwall-walk-a01: TRANSIENT pids (present in some snapshot, not in both ends): 541326,541896
- 30-sqpwall-walk-a01.log / sqpwall-walk-a01 (window 206.98 s): pid 13480 +0.33 s; pid 13501 +0.22 s; pid 943 +0.05 s; pid 83618 +0.03 s; pid 1289 +0.03 s
- 30-sqpwall-walk-a10.log / sqpwall-walk-a10: TRANSIENT pids (present in some snapshot, not in both ends): 540762,540769,540770,541326,541328,541329
- 30-sqpwall-walk-a10.log / sqpwall-walk-a10 (window 204.76 s): pid 13480 +0.31 s; pid 13501 +0.22 s; pid 924 +0.04 s; pid 1289 +0.03 s; pid 926 +0.01 s
- 30-sqpwall-walk-a11.log / sqpwall-walk-a11: TRANSIENT pids (present in some snapshot, not in both ends): 541328,541329,542434,542435
- 30-sqpwall-walk-a11.log / sqpwall-walk-a11 (window 205.33 s): pid 13480 +0.31 s; pid 13501 +0.22 s; pid 1289 +0.03 s; pid 924 +0.02 s; pid 83618 +0.02 s

## The two kinds of UNPROVEN, counted (fix3)

| batches | PINNED-CLEAN | UNPROVEN-FRACTION | UNPROVEN-EVIDENCE |
|---:|---:|---:|---:|
| 33 | 11 | 16 | 21 |

A batch failing both is counted in both unproven columns, so the three need not sum to the first.
