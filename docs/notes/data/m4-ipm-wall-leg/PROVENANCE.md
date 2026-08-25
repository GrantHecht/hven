# How the arms in `runs/` were produced

Both legs compare two arms, `base` and `head`, and each arm is one static
binary. What varies between them is the hven headers and the `libhven.a` they
compile and link against; the probe SOURCE compiled into both is always this
directory's copy, because two arms running different programs are not
comparable. `build_ipm_time.sh` and `build_layout_time.sh` both take
`<source root> <build dir> <output binary>` and take the probe source from
their own directory.

That is true of both scripts as they now stand, and it was NOT true of
`build_ipm_time.sh` until this branch: it compiled
`/home/ghecht/Projects/hven/.scratch/task-3/ab/ipm_time.cpp`, an out-of-tree
path in a different checkout, left over from the run it was first written for.
That stale copy is the pre-fix probe -- it prints the convergence flag under
the name `iters` -- so any IPM leg log produced before this branch carries no
real iteration count whatever the source beside it says. The script now
resolves its source relative to itself, as the layout script always did.

Every log also opens with a BUILD HEADER: the probe source and its sha256, and
each arm's binary with its sha256 and mtime. A log without that header cannot
be told apart from one a stale probe produced, which is the failure above.

## `runs/2026-08-22-layout-cost-fix-ipm.log` and `runs/2026-08-22-layout-cost-fix-layout.log`

The header at the top of each log is authoritative for that run; what follows
is the standing description of how the two arms are constructed.

| arm | hven source | `libhven.a` |
|---|---|---|
| `base` | `777e1a7` | built from that source, preset `linux-clang-release` |
| `head` | the layout-cost branch, rebased onto `24f3258` | `hven-fix/build`, same preset |

**THE BASE ARM IS NOT THIS BRANCH'S BASE, and these two rows therefore report more
than this branch.** The branch's parent is `24f3258`; `777e1a7` is 22 commits earlier.
The arms were built before the rebase, when `777e1a7` was the branch point, and were
not rebuilt after it. The 22 commits in between touch
`src/drivers/aggregate_eval_seam.cpp` and `src/model/non_linear_program.cpp` and
include a performance commit (`b1b2c4d`, hand-ordering the three claim blocks in place
of a library sort) and two fixes (`15147eb`, `53fad01`), so their effect is inside
these deltas too. The span is covered separately by
`docs/notes/data/2026-08-22-m4-task6-wall-leg/` and `…-task7-wall-leg/`; the clean
per-branch acceptance is the tycho-shaped one recorded in
`docs/notes/data/2026-08-22-m4-transcribe-attribution/`, whose arms are built at the
rebased head.

**No `777e1a7` checkout survives on this box**, and the base arm was not built
from one. It was built from a source tree extracted with
`git archive 777e1a7 | tar -x` into a scratch directory, with `dep/eigen` and
`dep/fmt` symlinked to the working tree's submodules (a `git archive` of a
gitlink produces an empty directory), then configured with the same preset and
built with the same compiler. That reproduces the commit's sources exactly; it
does not reproduce a checkout, and nothing here needs one.

Conditions: Fedora, AMD Ryzen 7 5800X3D 8C/16T, 31 GiB, Linux 7.1.9-200.fc44,
`/usr/bin/clang++` 22.1.8, MKL LP64 static, `HVEN_FP_MODE=SAFER_FAST`,
`-DHVEN_DEFAULT_QP_THREADS=8`, Release. Every run held
`flock /tmp/box-build.lock` and started with the box's one-minute load average
below 0.6; the box state at each run's start and end is in the report that
cites the run.

## `runs/2026-08-24-task9-ipm.log`, `runs/2026-08-24-task9-ipm-2.log`

| arm | hven source | `libhven.a` |
|---|---|---|
| `base` | `42091c6` (the Task 9 token-grant anchor, and this batch's real branch point) | built from a `git archive 42091c6 | tar -x` tree with `dep/eigen` and `dep/fmt` symlinked to the working tree's submodules |
| `head` | the Task 9 head batch at its tip | built from the working tree |

Both libraries were configured identically and independently of the working
build directory, so the two arms differ in source and in nothing else:

```
cmake -S <arm> -B <arm>/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_FP_MODE=SAFER_FAST \
      -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF
ninja -C <arm>/build -j6 hven
```

UNLIKE the layout-cost fix's rows above, **the base arm here IS this batch's
branch point** -- the two rows report this batch and nothing else.

Two repetitions rather than one, for the same reason the Task 4 rows give: every
cell of the first came out with a shared sign. The second reproduced it on every
cell and both estimators.

## `runs/2026-08-24-task9-fixround-ipm.log`

The re-run the standing instrument calls for when a fix round touches the solve
path again -- here the forced pattern verification for calls that hand the KKT
matrix out.

| arm | hven source | `libhven.a` |
|---|---|---|
| `base` | `42091c6` (the same Task 9 anchor the two rows above used) | the same `git archive 42091c6` tree and build directory those rows were taken from, rebuilt from nothing |
| `head` | `ccdd541`, the fix round's tip | built from the working tree into the same independent build directory the earlier head arm used |

Both arms were configured exactly as the rows above state, so the two differ in
source and in nothing else. The probe drives whole solves with NO early callback
installed, which is deliberate: that is the shape the epoch skip still covers,
and it is the shape the earlier rows measured, so the two runs are comparable
cell for cell. A call WITH a callback installed now verifies the pattern at
every factorization by design, which returns it to pre-epoch-gate cost; no
instrument row asserts that arm, because it is a restoration of the previous
behaviour rather than a change to be tracked.

One-minute load average 0.40 at the run's start.

## `runs/2026-08-24-task9-fb4-remeasure.log`

The isolating A/B the ledger registered for Task 9 (`6b5fc12` -> `46d6ff8`,
"fix batch 4"), which the earlier reading left as "mechanism not established"
against a 4.4% spread.

| arm | hven source |
|---|---|
| `base` | `6b5fc12` |
| `head` | `46d6ff8` |

Both extracted and built exactly as the two arms above, on the same box in the
same window, with the one-minute load average below 0.6 at the run's start.
Base rep-spreads came in at 0.60-3.05%, against 4.4% before, which is what makes
the reading assertable this time. **The re-measure is not part of the Task 9
comparison** and shares nothing with it but the instrument and the box.

Conditions for all three runs: Fedora, AMD Ryzen 7 5800X3D 8C/16T, 31 GiB, Linux
7.1.9-200.fc44, `/usr/bin/clang++` 22.1.8, MKL LP64 static,
`HVEN_FP_MODE=SAFER_FAST`, `-DHVEN_DEFAULT_QP_THREADS=8`, Release. Every build
held `flock /tmp/box-build.lock`; every run started with the box's one-minute
load average below 0.6, and each log's own header records the value it started
at.

## `runs/2026-08-25-align-loops-ipm.log`, `runs/2026-08-25-align-loops-ipm-2.log`

The flag flip: `-falign-loops=32` added to the shared Release flag set. Unlike
every pair above, the two arms are not two commits — they are one commit's
sources built twice, differing in one compile flag.

| arm | hven source | `libhven.a` |
|---|---|---|
| `base` | `8fa68c2`, from a `git archive 8fa68c2 \| tar -x` tree with `dep/eigen` and `dep/fmt` symlinked to the working tree's submodules | built from that tree |
| `head` | the SAME extracted tree, plus the one-hunk patch appending `-falign-loops=32` to `RELEASE_FLAGS` in `cmake/hven_compile_options.cmake` | built from that tree |

A recursive diff of the two source trees reports exactly one differing file,
which is that cmake module. Both were configured identically and independently
of the working build directory, as the Task 9 rows above state:

```
cmake -S <arm> -B <arm>/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_FP_MODE=SAFER_FAST \
      -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF
ninja -C <arm>/build -j6 hven
```

The flag lands on 33 compile lines of the head arm's `build.ninja` and on none
of the base arm's. `libhven.a`: 7,494,110 → 7,614,046 bytes, +1.60 %.

Two repetitions for the same reason the Task 4 and Task 9 rows give: every cell
of the first shared a sign. The second reproduced the largest serial cell to
three decimal places.

Both runs started at a one-minute load average above this leg's usual sub-0.6
gate — 1.19 and 2.19 — decaying from the builds that produced the arms rather
than from a competing workload; the box was otherwise idle and the lock was
held throughout. That is recorded rather than smoothed over: the serial cells'
0.29-1.91 % base rep-spreads in run 2 and the two runs' cell-for-cell agreement
are what the reading rests on, and the threaded n=60/n=120 cells (34-37 %
spread in both runs) are explicitly not part of it.

The layout-only evidence these two logs cannot carry — the three-problem `%a`
identity check across the same flag flip, and the PCH byte-identity re-prove —
is in `../2026-08-25-align-loops/`.

Conditions: Fedora, AMD Ryzen 7 5800X3D 8C/16T, 31 GiB, Linux 7.1.9-200.fc44,
`/usr/bin/clang++` 22.1.8, MKL LP64 static, `HVEN_FP_MODE=SAFER_FAST`,
`-DHVEN_DEFAULT_QP_THREADS=8`, Release. Every build and every run held
`flock /tmp/box-build.lock`.
