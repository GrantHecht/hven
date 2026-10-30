# P-SYM, SAME PATH — `0be12ec3` (the T3 close) vs `39746705` (this task's HEAD)

## The protocol

T3 fix round 1's, verbatim. ONE worktree, ONE build directory per config, both arms
compiled at the SAME absolute paths — the precondition `scripts/psym_compare.sh`'s own
preamble names as the one most easily broken:

    source  /home/ghecht/Projects/hven/.scratch/m7t4/psym-src          (git worktree)
    build   /home/ghecht/Projects/hven/.scratch/m7t4/psym-build-release
            /home/ghecht/Projects/hven/.scratch/m7t4/psym-build-debug

For each config: check the worktree out at `0be12ec3`, configure, build EVERY target
(`capture` refuses a partial build), capture; then check the SAME worktree out at
`39746705`, rebuild IN PLACE, capture, compare. `CCACHE_DISABLE=1` on every build.
clang 22.1.8, Ninja, MKL, uniform flag regime. Each leg ran solo inside its own hold of
`flock /tmp/box-build.lock` with the `PGREP_EMPTY` / `FOREGROUND_START` /
`FOREGROUND_END` markers. `psym_compare` tool sha256
`2dd3717c2dddc2c8e287876f282d242acaa9696d03e4859728918002e18b002a`.

## The result

| | Debug | Release |
|---|---|---|
| objects captured | 143 | 143 |
| byte-identical | 87 | 95 |
| within the accepted noise class | 8 | 0 |
| with differences | **47** | **47** |
| only-after (no before-arm counterpart) | 1 — the new TU `test_disproved_entry.cpp.o` | 1 — the same |

**The two differing-object lists are IDENTICAL AS SETS** (`differing-objects.txt`), which
is the cross-check T3's arm used and the reason the classification below is stated once
for both configs. What is NOT identical is which SYMBOL carries the difference: at `-O3`
`SqpOptions::SqpOptions()` is inlined away and has no symbol of its own, so the same one
instruction appears inside each caller instead. That is why Release lists 542 distinct
differing symbols against Debug's 111 while the object set does not move.

**EXACTLY THREE LIBRARY OBJECTS DIFFER**, and each for a stated reason:

* `src/drivers/sqp_solver.cpp.o` — Debug: 6 differing symbols and 9 only-after, i.e. the
  reseed's own new free function and the routing-chain functions the change sits in.
  **Its source changed.**
* `src/drivers/sqp_options.cpp.o` — `dump_options(const SqpOptions&, …)` and
  `sqp_preset(…)`. **Its source changed** (the new dump line).
* `src/qp/ipqp_engine.cpp.o` — ONE differing symbol, `SqpOptions::SqpOptions()`,
  because that TU constructs an `SqpOptions` on the stack. This is the SAME object and
  the SAME reason M7 W0 T3 recorded one task earlier; nothing in the tier's own code
  moved.

Every other object in `libhven.a` is BYTE-IDENTICAL.

## Every differing object, classified

Mechanically, in `differing-objects-debug.txt`: 47 objects, of which

* **3 have a source file changed between the two commits** — `sqp_solver.cpp`,
  `sqp_options.cpp`, `test_options_dump.cpp`;
* **44 do not, and in every one of them the differing symbols are
  `SqpOptions::SqpOptions()` and/or functions that construct an `SqpOptions` on the
  stack** (`TestBody()`, `sqp_default()`, `solve_fixture_cold(…)`, …). Each of those 44
  TUs names `SqpOptions` in its own source or in a header it includes — checked
  mechanically, including the two that need the indirection said out loud:
  `bench_crossover.cpp.o` through `bench/crossover_legs.h`, and the interior suite's
  CMake unity TU `Unity/unity_1_cxx.cxx.o` through `tests/interior/test_ipm_trace.cpp`.

**And the difference is ONE INSTRUCTION.** The comparator's own line for
`SqpOptions::SqpOptions()` in every one of those objects reads *"normalized listing is
60 lines vs 61 — instructions were added or removed"*: the single store that
initialises `ipqp_reseed_after_disproval = false`. `sizeof(SqpOptions)` did NOT move
(384 before and after — the bool fits the padding `elastic_rung_engine` left), which is
why this task's differing-object count is smaller than T3's 48 and why no object that
merely *includes* the header, without embedding its constructor, differs at all.

**Nothing is unexplained.** `P-SYM: FAIL — differences outside the accepted noise class`
is the tool reporting that a behaviour-changing commit changed behaviour; the classification
above is the reading that the brief asks for, and it is complete.
