# Build notes

What a developer needs to know about how this library compiles, beyond
`cmake --preset … && cmake --build …`.

## The precompiled header

`src/hven_pch.h` is a precompiled header used by six of the `hven`
target's translation units. It exists because this library's build time
is dominated by header parsing, not by how much code any one file
contains.

The numbers behind that claim (Linux, clang, Release `-O3`, idle box,
ccache disabled):

- Parsing the header set in `src/hven_pch.h` costs **3.01 s**, and that
  cost is paid once per TU that includes it.
- `src/drivers/interior_point_solver.cpp` is the largest TU at 3722
  lines and 7.02 s. **3.01 s of that is the headers**; only 4.01 s is
  its own body. (The per-TU table in `src/CMakeLists.txt` records 6.99 s
  for the same file. The two figures are separate measurements — this
  one taken from a full clean build, that one from the isolated
  with/without-PCH sweep — and the ~0.03 s between them is run-to-run
  noise, not a discrepancy.)
- `src/core/pattern_hash.cpp` is **39 lines** and still costs 2.98 s.

So file length barely predicts compile cost here, and the lever that
works is amortizing the shared header floor rather than moving code
between files. Building the PCH once costs 3.38 s; each participating TU
then drops 1.5–2.0 s. Measured effect on a clean `-j2` build of the
library target, median of three runs each: **28.16 s → 23.20 s
(−17.6%)**.

The same measurements are why the engine's large TUs are *not* split
into smaller ones — a split multiplies the 3.01 s floor by the number of
pieces instead of amortizing it, and (with `LINK_TIME_OPT` off) turns
helper calls inside the interior-point hot loop into cross-TU references
the compiler can no longer inline.

### Which TUs use it, and the rule for joining

Membership is **opt-in**, listed in `src/CMakeLists.txt`. Everything not
named there is opted out, so a newly added source gets no PCH until
someone measures it.

A TU qualifies only if it clears both bars:

1. it compiles **faster** with the PCH, and
2. its object file stays **byte-identical** to the non-PCH build.

Bar 2 is the one that matters most. The translation-unit section of
`CLAUDE.md` requires runtime neutrality to be proven rather than
presumed for anything touching engine code, and a byte-identical object
is the strongest available proof: identical bytes cannot move a
golden-rig row or a counter. With the current list, every object file the
Linux build produces (28 of them: 21 after M3 phase-C T1/T2 added
`drivers/sqp_print.cpp` and `core/ledger.cpp` and brought the
previously-unmeasured `kkt/kkt_calls.cpp` into the measurement table,
then T3's `drivers/sqp_options.cpp` and `core/enum_names.cpp`, then
T4's `globalization/sqp/funnel.cpp`, then T5's `drivers/sqp_driver.cpp`,
then T6's `globalization/sqp/soc_elastic_restoration.cpp`, then
T7's `warmstart/warm_start.cpp`, then T8's
`warmstart/continuation.cpp`)
*and* `libhven.a` itself
are byte-identical with and without the PCH — which is what
`scripts/check_pch_neutrality.sh` re-proves on demand and in CI.

Several TUs get faster with the PCH but produce a *non*-byte-identical
object, and are excluded for that reason alone. Their code is not wrong
— per-symbol disassembly comparison shows identical instructions, with
only function layout order and local label numbering shifting. It
happens because those TUs include a different header set in a different
order (`interior_point_solver_globalization.cpp`, for example, orders
its includes alphabetically and pulls in two headers outside the shared
set), so prefixing the shared block changes template instantiation order
and therefore emission order. Admitting them would buy roughly 5.4 s
more; doing so means relaxing bar 2, which is a numerics-governance
decision, not a build decision.

### If you edit `src/hven_pch.h`

The header's include list is the include block of
`src/drivers/interior_point_solver.cpp`, verbatim and in the same order.
That ordering is load-bearing — it is what makes the byte-identity
property hold, which is why both lists carry a `// clang-format off`
guard (clang-format would otherwise alphabetize them). After any edit,
run `scripts/check_pch_neutrality.sh` and update the participating-TU
list to whatever still qualifies.

`src/CMakeLists.txt` also refuses to configure if the opt-in list stops
describing the build: it fails if a listed name matches no real source,
if the opt-out fails to take effect for any non-participating source, or
if the target's source count changes without the list being revisited.

## ccache

The build uses ccache automatically when it is on `PATH`. Two settings
matter on a PCH-using tree.

Put this in `~/.config/ccache/ccache.conf`:

```ini
max_size = 40G
sloppiness = pch_defines,time_macros
```

- **`max_size`** — the default (5G) is too small for a tree of this
  shape; a full build writes several GiB and the cache thrashes rather
  than helping.
- **`sloppiness = pch_defines,time_macros`** — this is what makes
  PCH-consuming compiles cacheable at all. Without it ccache refuses
  them, which on this tree means refusing most of the expensive ones.

Two clarifications, because the folklore around this is wrong often
enough to be worth stating precisely:

- **`-Xclang -fno-pch-timestamp` is a requirement, not an
  optimization.** ccache's manual is explicit that Clang embeds a
  timestamp in the PCH, and that using ccache with a Clang PCH requires
  compiling it with this flag. `src/CMakeLists.txt` passes it under
  Clang for exactly that reason. It is not a tuning knob that buys a few
  extra cache hits — without it the arrangement does not work.
- **`pch_external_checksum` is inert here, and is deliberately not in
  the config above.** It does not "make consumers hash the PCH" —
  hashing the PCH is already what ccache does by default. The setting
  tells ccache to hash a `<pch>.sum` file *instead of* the PCH itself
  when one exists, as a performance workaround for very large
  precompiled headers. Nothing in this build emits a `.sum` file, so the
  setting would do nothing. It was recommended here in an earlier
  revision of this page; that was a misreading of the manual.

The safeguard that actually protects the verification path is neither of
these: `scripts/check_pch_neutrality.sh` exports `CCACHE_DISABLE=1` for
both of its builds, so the byte-identity comparison always compiles the
sources in front of it rather than replaying whatever a cache happened
to hold.

## Disclosed limitations

Three things this setup does not prove, stated so nobody has to infer
them:

- **The membership bars are Linux-and-clang measurements.** Both the
  timing table and the byte-identity results were produced on Linux with
  clang. They are assumed, not verified, to transfer to the macOS and
  Windows lanes; those lanes assert only that the PCH is *engaged* on
  the expected six TUs, not that it is byte-neutral there. A full
  neutrality run on macOS is Mac-leg work.
- **Byte-identity holds for clang, and does not hold for GCC.** Measured
  on GCC 16.1.1, three of the six opted-in TUs
  (`interior_point_solver_settings.cpp`, `non_linear_program.cpp`,
  `nlp_adapter.cpp`) emit a different object with the PCH than without
  it. Clang is the toolchain `CLAUDE.md` documents, every preset selects
  and all three CI lanes use, so this is outside the supported
  configuration — but it is a real boundary on the claim, not a
  rounding error. `scripts/check_pch_neutrality.sh` defaults to clang
  for this reason and prints the compiler it used.
- **tycho-embedded composition is build-verified, not
  identity-verified.** When tycho consumes hven via `add_subdirectory`,
  the two projects' precompiled headers are independent per-target
  objects and were confirmed to build cleanly together. That check did
  not compare object bytes in the embedded configuration. If that proof
  is ever needed, `scripts/check_pch_neutrality.sh` is runnable
  standalone against any pair of build directories.

## A note on diagnostics

CMake compiles its generated PCH wrapper with system-header semantics.
A practical consequence: warnings originating inside the shared header
set are suppressed in the six TUs that consume the PCH. This is not a
loss of coverage in practice — the test suite compiles those same
headers without the PCH — but it does mean a new warning introduced in
one of those headers will surface from the tests rather than from the
library build.
