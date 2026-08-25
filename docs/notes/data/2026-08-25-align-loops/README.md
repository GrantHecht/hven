# `-falign-loops=32` — the layout-only evidence

Two arms differing in **one compile flag and nothing else**: both are the
sources of `8fa68c2`, extracted with `git archive 8fa68c2 | tar -x` into two
scratch trees with `dep/eigen` and `dep/fmt` symlinked to the working tree's
submodules, and the head tree carries the one-hunk patch that appends
`-falign-loops=32` to `RELEASE_FLAGS` in `cmake/hven_compile_options.cmake`.
A recursive diff of the two trees reports exactly that one file.

Both libraries were configured identically and independently of the working
build directory:

```
cmake -S <arm> -B <arm>/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_FP_MODE=SAFER_FAST \
      -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF
ninja -C <arm>/build -j6 hven
```

The flag appears on 33 compile lines of the head arm's `build.ninja` and on
none of the base arm's. `libhven.a`: 7,494,110 → 7,614,046 bytes, **+1.60 %**.

The point of this directory is the thing a timing cannot say: an alignment
change moves *when* instructions issue, never *what* they compute. Both checks
below are the standing ones, run across the flag flip.

## Check 1 — the three-problem `%a` identity check

Probe: `../2026-08-24-m4-task9-gate/identity_probe.cpp`, compiled into BOTH
arms from that directory's single copy (only the headers and `libhven.a` come
from the arm), with the wall leg's own flags. Three problems through
`NLPSolver` — `wide40`, `hs071`, `boxed` — each solved, then re-solved after
the partition count is renegotiated. Everything reported is printed in `%a`:
objective, iteration count, flag, every primal, all three multiplier blocks,
every constraint residual. Run `MKL_NUM_THREADS=1`.

**Result: `identity_base.txt` and `identity_head.txt` are byte-identical over
all 298 lines** — both solves of all three problems, not just the first.

```
diff identity_base.txt identity_head.txt     # empty
```

That is a strictly wider comparison than the Task 9 run this probe was written
for, which could only assert the 149 first-solve lines: there the base arm
carried the defect the gate closes, so its re-solve lines were expected to
differ. Here the two arms are the same program, so the whole file has to match,
and it does.

| arm | probe binary sha256 |
|---|---|
| base (no flag) | `de817a1fe46fa76b13e274f128bd693d0f799599753170e6dee6d5a0a38acf58` |
| head (`-falign-loops=32`) | `62b6c1cef193f1354ebafe860c232af453de13e5fbf37460a27487fc72c4fe3d` |

## Check 2 — the wall leg's 54 answer pairs

`../m4-ipm-wall-leg/runs/2026-08-25-align-loops-ipm.log` and `…-ipm-2.log`,
each `./ipm_wall_leg.sh 9 15`: 2 arms × 9 reps × 3 sizes = **54 base/head
pairs per run**, each pair carrying `iters`, `flag` and `xnorm2`.

**All 54 pairs equal in each run**, and across the 108 data lines of a run
there are exactly three distinct `(iters, flag, xnorm2)` triples — one per
problem size, the same three on both sides:

```
n=60   iters=7 flag=0 xnorm2=1.3750963021836506
n=120  iters=7 flag=0 xnorm2=2.7501926043856093
n=240  iters=8 flag=0 xnorm2=5.5003852082409646
```

The timing those two logs carry, and how to read it, is in
`../m4-ipm-wall-leg/README.md`.

## The PCH byte-identity re-prove

`pch-neutrality.log` — `scripts/check_pch_neutrality.sh` run against the
working tree with the flag in it. Both of its builds carry the flag (the
PCH-on `build.ninja` has it on 33 compile lines, the PCH-off one on 32; the
difference is the PCH's own compile). PASS: the PCH is engaged on 6
translation units and all 33 emitted artifacts — 32 objects and `libhven.a` —
are byte-identical with and without it.

The prose record of that re-prove, appended to the baseline it refreshes, is
the 2026-08-25 addendum in `../../2026-08-m3-phase-c-tu-carve-addenda.md`.

## Conditions

Fedora, AMD Ryzen 7 5800X3D 8C/16T, 31 GiB, Linux 7.1.9-200.fc44,
`/usr/bin/clang++` 22.1.8, MKL LP64 static, Release, `HVEN_FP_MODE=SAFER_FAST`,
`-DHVEN_DEFAULT_QP_THREADS=8`. Every build and every timed run held
`flock /tmp/box-build.lock`.
