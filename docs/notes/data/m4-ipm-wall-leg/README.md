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

Conditions: Fedora, AMD Ryzen 7 5800X3D 8C/16T, 31 GiB, Linux 7.1.5-201.fc44,
`/usr/bin/clang++` 22.1.8, MKL LP64, Release, `HVEN_FP_MODE=SAFER_FAST`.
