# The standing IPM wall leg

A per-task leg for the remainder of M4: every task touching the interior-point
consumption path re-runs this instrument and records its delta in
`docs/notes/2026-08-m4-ledger.md`, so contract-cost accretion is a visible
running total rather than a per-task judgement.

## Why this instrument and not the standing bench

`hven_sqp_bench` and the `--engine ssn` corpus drive `SqpDriver`, which never
reaches `InteriorPointSolver`. Neither can observe a change to the
interior-point consumption path; both remain worth running, as neutrality
checks. This is the leg that observes it.

## Running it

```
build_ipm_time.sh <base source root> <base build dir> ./ipm_base
build_ipm_time.sh <head source root> <head build dir> ./ipm_head
./ipm_wall_leg.sh 9 15 > runs/<date>-<task>.log
./aggregate.py runs/<date>-<task>.log
```

The protocol — the two arms, the alternation, and how to read the two
estimators — is stated in `ipm_wall_leg.sh`'s own header. Read per-task deltas
off the MINIMUM estimator: the median estimator's rep-to-rep spread on these
problem sizes is 0.5–0.8%, which is the same size as the effects being looked
for.

## Runs

| run | arms | headline |
|---|---|---|
| `runs/2026-08-21-task3.log` | base `f07184b` vs the Task 3 consumption switch | minimum estimator −0.23% … +0.34%, largest cells +0.03% / +0.06%; median estimator inside its own spread |
| `runs/2026-08-21-task4-1.log`, `runs/2026-08-21-task4-2.log` | base `a0dcec2` vs the NlpModel bridge's review fix round | minimum estimator +0.01% … +0.97% over two repetitions, every cell inside the base arm's own rep-spread; largest cells reproducible at +0.378% / +0.380% (serial n=240) |

Two repetitions rather than one, because every cell of the first came out
positive and a shared sign is worth a second look. The second reproduced the
largest serial cell to three decimal places, and rep8 of its threaded arm hit a
machine disturbance (0.444 s against a 0.035 s cell) that inflates that arm's
printed spread while leaving the minimum estimator untouched — which is the
behaviour that estimator is chosen for.

What that run measured, stated because the number alone invites the wrong
reading: 28 of the 30 objects in `libhven.a` were byte-identical between the two
arms, and the two that differed — `aggregate_declaration.cpp.o` and
`nlp_model_aggregate.cpp.o` — contain no code a solve executes
(`AggregateDeclaration::validate()` is layout-time; the bridge is never called
by the interior engine). No machine code on the timed path changed, so the
reproducible sub-half-percent offset is link layout — the changed objects shift
every symbol after them in the archive — and not evaluation cost. `xnorm2` and
`flag` were identical between arms on every cell of both repetitions.

Conditions: Fedora, AMD Ryzen 7 5800X3D 8C/16T, 31 GiB, Linux 7.1.5-201.fc44,
`/usr/bin/clang++` 22.1.8, MKL LP64, Release, `HVEN_FP_MODE=SAFER_FAST`.
