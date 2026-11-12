# Apple dense-arm thread pin removal: Linux evidence

Date 2026-11-12. Base 9b9be7673 (git archive), head = branch m8-apple-unpin. Linux, clang++,
Release, HVEN_FP_MODE=SAFER_FAST, Ninja, MKL_NUM_THREADS=1, run under `flock /tmp/box-build.lock`
after an empty `pgrep -a hven_bench_cell`.

Commands: `cmake -S <tree> -B <dir> -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST
-DCMAKE_CXX_COMPILER=/usr/bin/clang++`; base built with `--target hven`, head built in full;
`ctest -j4 --output-on-failure` on head.

## Object comparison (library target `hven`, `CMakeFiles/hven.dir/src/linear/dense_session.cpp.o`)

- base: ab67471475addac8bfef476001a26e13b6d4e0bbab12eb5ef98133da6cd3f2a8
- head: ab67471475addac8bfef476001a26e13b6d4e0bbab12eb5ef98133da6cd3f2a8

Byte-identical (the removed code is Apple-only).

## ctest (head, Release)

`100% tests passed, 0 tests failed out of 3606` (1 skipped by design, 2 disabled).
