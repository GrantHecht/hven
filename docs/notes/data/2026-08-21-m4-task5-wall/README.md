# Task 5 wall leg — the Level 2 consumption switch, base `07d5ee1` vs `d9cc35e`

Two instruments, because the diff under test spans both consumption paths:

| instrument | what it observes | files |
|---|---|---|
| standing bench wall leg | `SqpDriver`, i.e. `AggregateEvalSeam` + `sqp_driver.cpp` | `wall_leg.sh`, `bench_wall.log`, `bench_wall_aggregate.txt` |
| standing IPM wall leg | `InteriorPointSolver`, i.e. the engine TU's refuse arm | `../m4-ipm-wall-leg/runs/2026-08-22-task5-{1,2}.log`, `ipm_wall_{1,2}_aggregate.txt` |

The plan's Step 3 names the bench instrument; the ledger's standing rule
(`../m4-ipm-wall-leg/README.md`) requires the IPM instrument from every task
touching the interior-point consumption path, which the refuse arm in
`src/model/non_linear_program.cpp` does. Both were run.

Conditions: Fedora, AMD Ryzen 7 5800X3D 8C/16T, 31 GiB, Linux 7.1.5-201.fc44,
`/usr/bin/clang++`, MKL LP64, Release, `HVEN_FP_MODE=SAFER_FAST`,
`MKL_NUM_THREADS=1`, serial arms pinned to one physical core, base/head
alternated per rep. `quiet_check.sh`'s output is recorded before each arm
(`pgrep-before-*.txt`): nothing but the `earlyoom` daemon, whose *pattern
arguments* contain the string `clang`, was running.

## Bench leg — IN BAND

```
F3n1000cold    median 0.085540 -> 0.086096 (+0.650%) | minimum 0.085360 -> 0.085953 (+0.695%) | base rep-spread 0.83%
F3n1000warm    median 0.045894 -> 0.046238 (+0.750%) | minimum 0.045824 -> 0.046177 (+0.770%) | base rep-spread 0.33%
F7n200cold     median 17.478507 -> 17.390596 (-0.503%) | minimum 17.476098 -> 17.378185 (-0.560%) | base rep-spread 0.28%
```

Both estimators agree per cell. The two sub-100 ms F3 cells sit at +0.65…+0.77%,
inside the standing precedent (`+0.01%…+0.97%`, minimum estimator, the bridge
task's ledger entry); the 17 s F7 cell — the only cell whose wall is large
against its own spread — is **negative**: head is 0.5% FASTER.

## IPM leg — OUT OF BAND, reproduced, mechanism named

```
run 1                                                                    run 2
serial   n=60  min +0.688% (base spread 1.17%)                           min +0.691% (1.40%)
serial   n=120 min +1.403% (0.91%)                                       min +1.124% (0.41%)
serial   n=240 min +2.439% (1.17%)                                       min +2.202% (0.97%)
threaded n=60  min +0.441% (1.54%)                                       min +0.491% (1.11%)
threaded n=120 min +0.983% (1.64%)                                       min +1.283% (1.12%)
threaded n=240 min +2.365% (1.13%)                                       min +2.341% (1.52%)
```

The `n = 240` cells clear their own base rep-spread on both estimators in both
repetitions, and clear the standing precedent's upper end (+0.97%). Recorded as
a **band trip requiring adjudication**, not waved through.

**Answers are unchanged.** Across both repetitions and both arms there is
exactly one `(flag, xnorm2)` pair per size, identical between base and head:
`n=60 flag=0 xnorm2=1.3750963021836506`, `n=120 … 2.7501926043856093`,
`n=240 … 5.5003852082409646`.

**Mechanism, measured rather than assumed.** Unpacking both `libhven.a`s: 30
objects base, 31 head; four differ (`interior_point_solver.cpp.o`,
`non_linear_program.cpp.o`, `nlp_model_aggregate.cpp.o`, `sqp_driver.cpp.o`)
and one is new (`aggregate_eval_seam.cpp.o`). Unlike the bridge task's leg, two
of the differing objects ARE on the timed path — but no `.cpp` of the interior
engine changed. What reaches them is `model/candidate_point.h`: `is_legal_request`
gained three equality tests, and both `validate_eval_request`'s throw and
`assemble_impl`'s new terminal refuse grew their (never-taken) cold paths.

That added work cannot be the delta. `validate_eval_request` is called once per
`assemble()` (`include/hven/model/nlp_aggregate.h:573`); at a few tens of
assembles per solve, three integer comparisons are nanoseconds against the
0.8 ms the `n = 240` cell moved. The refuse arm itself is a terminal `else`
after seven exact-equality tests and is never entered on any timed cell — no
throw occurred, every `flag` is 0. What is left is code layout: two timed-path
objects were recompiled with grown cold paths (an inlining input), and a new
archive member shifts every symbol after it — the same link-layout mechanism the
bridge task's entry established for its own reproducible sub-half-percent
offset, one size larger because this diff recompiles more of the timed path.
