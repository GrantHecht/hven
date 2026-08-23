# How the arms in `runs/` were produced

Both legs compare two arms, `base` and `head`, and each arm is one static
binary. What varies between them is the hven headers and the `libhven.a` they
compile and link against; the probe SOURCE compiled into both is always this
directory's copy, because two arms running different programs are not
comparable. `build_ipm_time.sh` and `build_layout_time.sh` both take
`<source root> <build dir> <output binary>` and take the probe source from
their own directory.

One wrinkle worth stating rather than leaving to be rediscovered: the IPM leg's
committed `build_ipm_time.sh` compiles
`/home/ghecht/Projects/hven/.scratch/task-3/ab/ipm_time.cpp` -- an OUT-OF-TREE
path in a different checkout, left over from the run the script was first
written for. The copy beside it in this directory is the one under version
control and the one this file's runs used. A run that wants the committed
source must point the script at it (or use `build_layout_time.sh`'s shape,
which resolves the source relative to the script).

## `runs/2026-08-22-layout-cost-fix-ipm.log` and `runs/2026-08-22-layout-cost-fix-layout.log`

| arm | hven source | `libhven.a` |
|---|---|---|
| `base` | `777e1a7` (the branch point) | built from that source, preset `linux-clang-release` |
| `head` | `777e1a7` + the layout-cost branch | `hven-fix/build`, same preset |

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
