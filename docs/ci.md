# CI lanes

`.github/workflows/ci.yml` runs a three-OS matrix on every push to `main`
and every pull request: `linux-clang-release`, `macos-clang-release`, and
`windows-clang-release`. All three are required-green (see "Branch
protection" below). A `concurrency` group (`ci-${{ github.ref }}`,
`cancel-in-progress: true`) cancels a ref's stale in-flight run whenever a
newer commit lands on the same ref.

## Linux lane (`linux-clang-release` job)

Runs on `ubuntu-latest`. Steps:

1. Checkout with `submodules: recursive` (pulls `dep/eigen`, `dep/fmt`).
2. Install `clang` and `ninja-build` via apt.
3. Install Intel MKL (`intel-oneapi-mkl-devel`) via the Intel oneAPI apt
   repository, then export `ONEAPI_ROOT` and `MKLROOT` into
   `$GITHUB_ENV` so `cmake/FindMKL.cmake` is not limited to its three
   hardcoded fallback paths for libiomp5's directory (the piece most prone
   to moving between oneAPI package layouts). MKLROOT itself still assumes
   the mkl/latest symlink apt creates.
4. Configure with the `linux-clang-release` CMake preset.
5. Build (`cmake --build build`).
6. Test (`ctest --test-dir build --output-on-failure`).
7. Accelerate backend syntax check, stubbed
   (`scripts/check_accelerate_syntax_linux.sh`) — cheap and Linux-runnable,
   so it rides in this job rather than a separate one. See that script's own
   header comment and `docs/testing.md` for exactly what it does and does
   not prove; it is not a substitute for the macOS lane below.

This lane is observed, repeatedly, on GitHub's own infrastructure — it is
no longer a claim resting on local verification plus a lint check. At the
time this page was last updated it had run green on every push to `main`
since the repository gained a remote (`gh run list` against
`GrantHecht/hven` shows an unbroken string of `success` runs for the `ci`
workflow). Locally, the same configure/build/ctest sequence (steps 4-6
above) is also re-verified by hand before every push to this lane, on both
the release and debug presets — 116/116 tests passing, 0 failed, at the
commit that added the three-OS matrix.

## macOS lane (`macos-clang-release` job)

Runs on `macos-latest` (Apple Silicon). Steps:

1. Checkout with `submodules: recursive`.
2. Install Ninja (`brew install ninja`) — no other package install: the
   compiler is AppleClang (Xcode Command Line Tools, already on the runner
   image; `macos-clang-release`'s `CMAKE_CXX_COMPILER` cache variable is
   left unset so CMake picks up whatever `clang++` `xcode-select` resolves
   to), and Accelerate is a system framework, found by
   `cmake/FindAccelerateSparse.cmake` via the SDK's default framework
   search path with no install step at all.
3. Configure with the new `macos-clang-release` CMake preset
   (`CMakePresets.json`; `binaryDir` `build-macos/`).
4. Build (`cmake --build build-macos`).
5. Test (`ctest --test-dir build-macos --output-on-failure`).

**Why AppleClang, not Homebrew LLVM.** Homebrew LLVM (`macos-llvm-release`
in tycho's own `CMakePresets.json`, the precedent this repo's Windows preset
was written from) is the more thoroughly precedented choice elsewhere in
the sibling projects, but it is one more brew formula this job does not
need to install, cache, or keep in sync with Xcode's own SDK version.
`cmake/hven_compile_options.cmake` was reviewed for anything that hard-requires
non-Apple clang before choosing AppleClang: everything it does that is
gated on `CMAKE_CXX_COMPILER_ID STREQUAL "Clang"` (the exact string,
which AppleClang's compiler ID does **not** match — AppleClang's ID is
literally `AppleClang`) is an optional nicety that is silently skipped
under AppleClang, never a hard failure: the `-mllvm -inline-threshold=...`
flag pair, the `-Wabsolute-value` warning, and `-ftime-trace` tracing. None
of them are load-bearing for a green build. `LINK_TIME_OPT` (the one thing
gated on compiler ID that select real behavior, not just a nicety) is
`FALSE` by default and this job does not turn it on, so that branch never
runs here either. The one flag applied unconditionally regardless of
compiler ID — `-march=native` / `-mtune=native` (`hven_compile_options()`'s
non-wheel SIMD branch) — is the one open question AppleClang specifically
raises: Homebrew LLVM's AArch64 host-CPU detection is well precedented (it
is what tycho's own macOS preset already uses successfully), but whether
AppleClang's clang driver accepts `-march=native`/`-mtune=native` the same
way on `arm64-apple-darwin` had not been checked against real Apple
hardware anywhere in this repo's history at the time this job was written.
If it does not, that is exactly the kind of first-contact finding this job
exists to surface — see the note below.

**This is the milestone's first real Apple-hardware run.** The Accelerate
session, shim, and Apple-gated test halves
(`src/linear/accelerate_session.cpp`, `symmetric_factor_accelerate.cpp`,
the three `#if defined(__APPLE__)` test blocks under `tests/linear/`) have,
up to this job's first green run, only ever been syntax-checked against a
hand-written stub of `<Accelerate/Accelerate.h>` on Linux
(`scripts/check_accelerate_syntax_linux.sh` — see that script's own header
comment and `docs/testing.md` for the exact claim ceiling that check
supports and does not exceed). First-contact compile, link, or runtime
failures in this job are expected and are treated as real findings about
the Accelerate backend or the stub it was checked against, not as
embarrassments to explain away — each one gets fixed, documented in the
task report that closes this work, and folded into `docs/testing.md`'s
claim-ceiling bookkeeping and the project's Mac docket.

## Windows lane (`windows-clang-release` job)

Runs on `windows-latest`. Steps:

1. Checkout with `submodules: recursive`.
2. Install Ninja (`choco install ninja`) — LLVM/clang-cl is preinstalled on
   the runner image at `C:\Program Files\LLVM\bin\clang-cl.exe`, the same
   path tycho's own `x64-Clang-Release` preset (`CMakePresets.json`, read
   read-only as this job's clang-cl precedent) hard-codes.
3. Install Intel MKL, cached (see below).
4. Configure with the new `windows-clang-release` CMake preset
   (`CMakePresets.json`; `binaryDir` `build-windows/`) — its
   `CMAKE_C(XX)_COMPILER*`/`CMAKE_LINKER`/architecture/toolset block is
   tycho's `x64-Clang-Release` preset verbatim.
5. Build (`cmake --build build-windows`).
6. Test (`ctest --test-dir build-windows --output-on-failure`).

**MKL install, cached.** Intel MKL has no Windows apt-equivalent one-liner,
and its offline installer is large (~1.9 GB for the combined oneAPI
Base+HPC installer this job downloads, even though it only extracts two
components from it), so the install is cached with `actions/cache`, keyed
on the installer version plus the exact component set
(`windows-oneapi-2026.1.0.191-mkl.devel-openmp-v1` — bump this key whenever
either changes). On a cache miss, `scripts/install_windows_mkl.bat` runs
the same two-step pattern Intel's own CI reference
([`oneapi-src/oneapi-ci`](https://github.com/oneapi-src/oneapi-ci),
`scripts/install_windows.bat`) and other public
oneAPI-on-GitHub-Actions setups use: download the offline installer `.exe`
(a self-extracting archive holding every toolkit component), extract it
locally, then run its embedded `bootstrapper.exe` with `--components=` to
install only `intel.oneapi.win.mkl.devel` (MKL's headers and import libs —
confirmed as a real, currently-published component id via
[`oneapi-src.github.io/oneapi-ci`](https://oneapi-src.github.io/oneapi-ci/))
and `intel.oneapi.win.openmp` (the OpenMP runtime `cmake/FindMKL.cmake`'s
default, unset-`MKL_USE_SEQUENTIAL` threading layer links against —
`libiomp5md.lib` — chosen by inference, **not** independently confirmed the
same way the MKL component id was). After install, `MKLROOT` and
`ONEAPI_ROOT` are exported into `$GITHUB_ENV`, mirroring the Linux job's own
step, so `cmake/FindMKL.cmake` looks under the actual Windows install
location rather than depending on one of its Linux-shaped hardcoded
fallback paths matching by accident. An alternative considered and set
aside: the NuGet packages `intelmkl.devel.win-x64` /
`intelmkl.redist.win-x64` / `intelopenmp.devel.win` — downloaded and
inspected directly (`nuget.org/api/v2/package/intelmkl.devel.win-x64/2026.1.0.226`)
while writing this job. Their internal layout
(`<pkg>/build/native/include`, `<pkg>/build/native/win-x64`) does not
match any hint path `cmake/FindMKL.cmake` already knows, which would have
meant either patching that Find module or re-staging files into a
synthetic `MKLROOT` tree; the offline-installer route needs neither, since
its install layout is the same `<oneAPI root>/mkl/<version>/...` shape
`cmake/FindMKL.cmake`'s existing (Linux-derived) hints already anticipate.

**The Windows/clang-cl flag set has not run here before either.**
`cmake/hven_compile_options.cmake`'s `WIN32` branches were migrated from
tycho verbatim — a direct diff against tycho's own
`cmake/tycho_compile_options.cmake` at the time this job was written showed
no functional difference, only comment wording — so the flags themselves
carry tycho's own Windows track record. What has never run before is
everything hven-specific wrapped around them on this exact runner image:
`HVEN_FP_MODE=SAFER_FAST`'s clang-cl `/clang:-fno-finite-math-only`
feature-detection check (`CMakeLists.txt`), `cmake/FindMKL.cmake`'s Windows
hint paths against a freshly-installed oneAPI, and the combination of all
of the above with `cmake/hven_sparse_backend.cmake`'s non-Apple branch.
First-contact failures here get the same treatment as the macOS lane above:
fixed, documented, not treated as an indictment of the workflow.

## Branch protection (human step, GitHub settings)

Not automatable by this repository's own files — a human with admin access
to `GrantHecht/hven` needs to open **Settings → Branches → Branch protection
rules** for `main` and set **Require status checks to pass before merging**
to include all three job names:

- `linux-clang-release`
- `macos-clang-release`
- `windows-clang-release`

Do this once all three have recorded at least one successful run on a real
pull request against `main` — requiring a check that has never gone green
locks out every PR until it does.

## UNOBSERVED discipline: Apple hardware

CLAUDE.md's rule stands regardless of what this page says: **no
Apple/Accelerate value may be fabricated, zero-filled, or interpolated —
absent evidence is reported as absent.** What has changed with this task is
*where* that evidence now comes from.

Before the `macos-clang-release` CI job existed, this repository's only
possible source of real Apple-hardware evidence was a human running the
manual routine below by hand and recording the row in the table. That table
is kept below as the historical record of every hand-run observation to
date; do not delete or backfill it.

```bash
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST
ninja
ctest --output-on-failure
```

| Date | Machine | macOS | Commit | Result |
| ---- | ------- | ----- | ------ | ------ |
| —    | —       | —     | —      | UNOBSERVED |

**That condition is now closed.** `macos-clang-release` has executed on
GitHub's own Apple Silicon runners and passed: run
[31287205323](https://github.com/GrantHecht/hven/actions/runs/31287205323),
commit `ca2744c1ba41bf111a71900ba3034897fd314144`, runner image
`macos-26-arm64`, **51 tests, 51 passed, 1 skipped**. The Accelerate
session, the Accelerate adapter and every `#if defined(__APPLE__)` test
half compiled, linked and ran on real hardware for the first time there,
and that whole lane re-runs on every push to `main` and every pull
request. It is the ongoing execution record for this backend now; the
hand-run table above stays as history and is not backfilled from it.

### What a green lane does and does not fill in

Green means the Apple *code paths* execute. It does **not** by itself put a
number in an expected table — the ctest step asserts, it does not report,
and a passing assertion against an `UNOBSERVED` slot is recorded as an
unfilled slot rather than as an observation.

The route from this lane to a filled Accelerate row is the
`Emit the Accelerate observations for derivation` step: it runs
`hven_golden_rig_report` in report mode after the gate and uploads
`rig-report-accelerate.txt` as a build artifact. That file's observations
block is the same paste-ready CSV the local three-seam derivation uses, so
an Accelerate row gets filled by copying it out of a named artifact from a
named run — never by typing what a Mac is assumed to produce. Whoever
fills one records the run id and the commit beside the row's own
provenance columns, so the artifact can be fetched again and the row
re-checked against it.

Two limits on what that artifact can ever fill:

- **Native arms only.** Neither old seam builds on a runner (there are no
  sibling checkouts there, and the SQP seam does not compile against
  Accelerate at all), so `psiopt-old@accelerate` stays `UNOBSERVED` until
  a Mac with both checkouts runs the three-seam configuration by hand.
- **A row still has to reproduce before it is committed.** The two gates
  the Linux derivation applied hold here too: the value must reproduce
  across runs, and it must not move with the build configuration. One
  artifact from one run demonstrates neither, so filling Accelerate rows
  means at least two runs, and a check against a differently-configured
  build, before anything is written down.

The manual trigger (`workflow_dispatch`) exists for exactly this: asking
for a derivation artifact should not require inventing a commit to push.
