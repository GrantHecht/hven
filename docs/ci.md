# CI lanes

## Linux lane (`.github/workflows/ci.yml`)

Automated, runs on `ubuntu-latest` for every push to `main` and every pull
request. Steps:

1. Checkout with `submodules: recursive` (pulls `dep/eigen`, `dep/fmt`).
2. Install `clang` and `ninja-build` via apt.
3. Install Intel MKL (`intel-oneapi-mkl-devel`) via the Intel oneAPI apt
   repository.
4. Configure with the `linux-clang-release` CMake preset.
5. Build (`cmake --build build`).
6. Test (`ctest --test-dir build --output-on-failure`).

This lane has not yet run on GitHub's infrastructure (no remote configured
for this repository at time of writing); it has been lint-checked
(`python -c "import yaml,sys; yaml.safe_load(open('.github/workflows/ci.yml'))"`)
and the same configure/build/ctest sequence it runs (steps 4-6 above) has
been verified manually, end to end, on a local Linux machine with MKL
already installed under `/opt/intel/oneapi` — configure, build, and ctest
all passed.

## Mac lane (manual hardware routine)

hven has no macOS CI runner yet. Until one exists, macOS coverage (the
Accelerate sparse backend, `cmake/FindAccelerateSparse.cmake`) is a manual
routine run by hand on Apple Silicon hardware. There is no `macos-*`
entry in `CMakePresets.json` yet — one is added once real Mac hardware has
run the routine below and confirmed the cache variables it needs (mirroring
how tycho's `macos-llvm-release` preset was authored). Until then, run
manually:

```bash
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST
ninja
ctest --output-on-failure
```

Record, for every run: the machine (model, chip), macOS version, Xcode/LLVM
version, date, commit hash, and the configure/build/ctest result.

| Date | Machine | macOS | Commit | Result |
| ---- | ------- | ----- | ------ | ------ |
| —    | —       | —     | —      | UNOBSERVED |

No Mac-observed value may be fabricated, zero-filled, or interpolated from
the Linux lane. A row with no real hardware run stays `UNOBSERVED` — it is
not populated with a guess, and it is not silently dropped from this table.
A `macos-llvm-release` CMake preset is added once a Mac has verified it end
to end (configure + build + ctest); until then, no such preset exists in
`CMakePresets.json`.
