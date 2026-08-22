# The standing wall legs

Two per-task legs for the remainder of M4: every task touching the
interior-point consumption path re-runs them and records the deltas in
`docs/notes/2026-08-m4-ledger.md`, so contract-cost accretion is a visible
running total rather than a per-task judgement.

- **The IPM leg** (`ipm_time.cpp`, `ipm_wall_leg.sh`) times whole solves. It is
  the original leg; everything below headed "Running it" and "Runs" is its.
- **The layout leg** (`layout_time.cpp`, `layout_wall_leg.sh`) times
  TRANSCRIPTION -- see "The layout leg" at the bottom.

Both are read by one aggregator, `aggregate.py`.

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
| `runs/2026-08-22-task5-1.log`, `runs/2026-08-22-task5-2.log` | base `07d5ee1` vs the SQP driver's Level 2 consumption switch | **BAND TRIP**: minimum estimator +0.44% … +2.44% over two repetitions; the `n = 240` cells (+2.44%/+2.20% serial, +2.37%/+2.34% threaded) clear their own base rep-spread and the precedent's +0.97% upper end. Answers identical (`flag`/`xnorm2` bit-equal on every cell of both runs). Mechanism in `../2026-08-21-m4-task5-wall/README.md`: two timed-path objects recompile because `candidate_point.h` grew, but the added work is three integer compares per `assemble()` — layout, not evaluation cost |

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

## The layout leg

The IPM leg's problem has three pieces and one application each, so per-claim
and per-piece LAYOUT work is a rounding error inside its cells: a change to
what a transcription costs is invisible in it. That is not hypothetical -- the
first Level 2 layout implementation moved the front end's transcription cells
by more than half while every whole-solve cell stayed at parity, and no
instrument in this repository could see it.

`layout_time.cpp` is that instrument. It builds a problem shaped like a
collocation transcription -- four constraint pieces plus an objective piece,
`n` applications over four-variable windows that overlap by two (so consecutive
applications share KKT columns, and columns are contested across partitions),
four partitions, bounds on every variable -- and times five cells at three
sizes:

| cell | what it times |
|---|---|
| `construct` | building the pieces and the program, first layout included |
| `transcribe` | re-laying an existing program (`make_nlp` again), layout only |
| `transcribe+key` | the same re-lay plus ONE `model_structure_key()` read |
| `solve` | one partitioned whole solve |
| `solve1` | the same solve at one partition |

`transcribe+key` is INFORMATIONAL and exists so that one number is not hidden.
The structural key's two digests are taken on first read rather than during the
lay, so a consumer that never asks about structural identity pays `transcribe`
and one that asks once per lay pays `transcribe+key`. Recording only the
cheaper of the two would report a saving that some consumer still pays.

Every cell prints an identity column -- the structural key's folded digest and
the claim count for the layout cells, the objective, iteration count and
convergence flag for the solve cells. Those must be EQUAL between arms; a
difference in one is a correctness finding, not a timing one.

With ONE exception, and it is why `solve1` exists. Above one partition the
contested-slot accumulation order is decided by the thread schedule -- two
partitions summing into one KKT column can land in either order -- so `solve`'s
objective moves in its low bits between runs of the same binary. That is a
property of the partitioned scatter and predates the contract layer. `solve`'s
gate is therefore its iteration count and its flag; `solve1` runs the same
problem at one partition, where the fold order is fixed, and ITS objective is
the bit-identity column.

```
build_layout_time.sh <base source root> <base build dir> ./layout_base
build_layout_time.sh <head source root> <head build dir> ./layout_head
./layout_wall_leg.sh 9 15 > runs/<date>-<task>-layout.log
./aggregate.py runs/<date>-<task>-layout.log
```

The probe source compiled into BOTH arms is this directory's copy -- only the
headers and the archive come from the arm -- because two arms running different
programs are not comparable.
