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
