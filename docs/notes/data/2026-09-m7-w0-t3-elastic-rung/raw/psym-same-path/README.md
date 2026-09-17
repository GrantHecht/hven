# P-SYM, SAME PATH — `60256f50` vs `3d3145a8`, BOTH CONFIGS (M7 W0 T3 fix round 1)

The step-2b round compared the two commits built at DIFFERENT absolute paths
(`.scratch/m7t3/base/...` against `/home/ghecht/Projects/hven/...`), which
`scripts/psym_compare.sh`'s own preamble names as the precondition most easily
broken. That comparison left `src/qp/ipqp_engine.cpp.o` unexplained, and the
report's rationale for it — "it does not include `sqp_solver_types.h` at all" —
was FALSE (sol's Important 1). This is the arm that closes it.

## THE PROTOCOL

ONE worktree, ONE build directory per config, both arms compiled at the same
absolute paths:

    source  /home/ghecht/Projects/hven/.scratch/m7t3-fix1/psym-src   (git worktree)
    build   /home/ghecht/Projects/hven/.scratch/m7t3-fix1/psym-build        (Release)
            /home/ghecht/Projects/hven/.scratch/m7t3-fix1/psym-build-debug  (Debug)

For each config: check the worktree out at `60256f50`, configure, build EVERY
target (`psym_compare.sh capture` refuses a partial build), capture; then check
the SAME worktree out at `3d3145a8`, rebuild IN PLACE, capture, compare.
`CCACHE_DISABLE=1` on every build. clang 22.1.8, Ninja, MKL, uniform flag
regime. Each leg ran solo under `flock /tmp/box-build.lock` with the
`PGREP_EMPTY` / `FOREGROUND_START` / `FOREGROUND_END` markers; the wrapper is
fold-proven (a stubbed failure returns rc 7 through it).

Full transcripts (0.4 MB and 0.7 MB) are retained in the task's scratch at
`.scratch/m7t3-fix1/logs/psym-{release,debug}-compare-full.log`; the library
objects' own sections are copied here verbatim.

## THE RESULT, IN ONE LINE

The build-path confound was REAL but NARROW, and `ipqp_engine.cpp.o` was never
part of it: at the same path the differing-object count falls from 99 to 48, and
the same FOUR library objects differ in BOTH configs.

| | different paths (step 2b) | same path (this arm), Release | same path, Debug |
|---|---|---|---|
| objects differing | 99 | 48 | 48 |
| library objects differing | 4 | 4 | 4 |

The Release and Debug differing-object lists are IDENTICAL as sets
(`differing-objects.txt`).

## EVERY DIFFERING OBJECT, CLASSIFIED

Mechanically, from the build's own dependency database (`ninja -t deps`), not by
eye:

* 142 objects captured. **48 differ, and every one of the 48 either (a) has a
  source file changed between the two commits (6 objects) or (b) includes
  `include/hven/drivers/sqp_solver_types.h` (42 objects), whose `SqpOptions`
  grew by one enum field — `sizeof(SqpOptions)` 376 → 384.**
* **No object that does not include that header differs at all** (74 objects).
* Inclusion alone is not the explanation and the numbers say so: 20 objects
  include the header and are byte-IDENTICAL. What differs is the subset that
  embeds the LAYOUT — a stack local, a member access, a by-value parameter.

The six source-changed objects:

    CMakeFiles/hven.dir/src/drivers/sqp_options.cpp.o          src/drivers/sqp_options.cpp
    CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o           src/drivers/sqp_solver.cpp
    tests/drivers/.../test_options_dump.cpp.o                  tests/drivers/test_options_dump.cpp
    tests/sqp/.../test_elastic_rung_vehicle.cpp.o              tests/sqp/test_elastic_rung_vehicle.cpp
    tests/sqp/.../test_ipqp_options.cpp.o                      tests/sqp/test_ipqp_options.cpp
    tests/sqp/.../test_sqp_solver.cpp.o                        tests/sqp/test_sqp_solver.cpp

## THE FOUR LIBRARY OBJECTS

| object | verdict |
|---|---|
| `src/drivers/sqp_options.cpp.o` | EXPECTED — source changed. Three symbols: `dump_options(SqpOptions)`, `validate(SqpOptions)`, `sqp_preset` |
| `src/drivers/sqp_solver.cpp.o` | EXPECTED — source changed. 40 differing symbols plus the new `SqpSolver::elastic_ipqp_engine()` / `elastic_rung_tier()` and the two signatures that gained an `IpqpEngine*` |
| `src/warmstart/continuation.cpp.o` | EXPLAINED, MINIMALLY — exactly ONE instruction of 40 948 changes, in `run_continuation(..., SqpSolver&, ...)`: `mov 0x168(%rax)` → `mov 0x170(%rax)`, an 8-byte member-offset shift inside `SqpSolver`, whose `opts_` grew. Identical finding in Debug |
| `src/qp/ipqp_engine.cpp.o` | **EXPLAINED — and it is a real codegen difference, not the path.** See below |

### `ipqp_engine.cpp.o`, closed

The include chain the step-2b report denied exists, and it is direct:

    src/qp/ipqp_engine.cpp
      -> include/hven/detail/qp/ipqp_engine.h:36   #include <hven/drivers/sqp_solver_types.h>

and the TU does not merely see `SqpOptions` — it CONSTRUCTS one, on the stack,
inside the one symbol that differs:

    src/qp/ipqp_engine.cpp:763   SqpOptions probe;        // in IpqpEngine::solve

(`IpqpEngine::solve` validates its `IpqpOptions` by calling the driver's own
`validate(const SqpOptions &)` on an otherwise-default probe, so the rules are
not copied twice.) The new field makes that object 8 bytes larger, and that is
the whole of the difference:

* **Release** — 172 of 173 symbols identical; the one that differs is
  `IpqpEngine::solve`. Same instruction COUNT before and after (24 522), same
  frame (`sub $0xca0,%rsp` in both). 17 changed instruction pairs, all inside
  the probe's initialisation window (`0x3c1`–`0x480(%rsp)`): 8 are
  displacement-only and the other 9 are equivalent-store re-selections in the
  same window (`vmovaps`↔`vmovups`, one `movq $0`→`movl $0`, and one
  `data16 … nopw`→`nopw` padding line). Three further instructions appear only in
  the before arm and three only in the after arm, and the fix-round-1 summary
  called that "3 instructions re-ordered", which undersells it — here is the
  whole of it (corrected 2026-09-16, fix round 2):

      REMOVED  vmovups %ymm0,0x43b(%rsp)      ADDED  vmovupd %ymm0,0x443(%rsp)
      REMOVED  vmovupd (%rcx),%ymm0           ADDED  vmovdqu %ymm3,0x430(%rsp)
      REMOVED  xchg %ax,%ax                   ADDED  movq $0x0,0x3d0(%rsp)

  Two of each pair are the SAME stores at a displacement 8 bytes higher
  (`0x43b`→`0x443`, and the `%ymm3` store `0x428`→`0x430` whose partner sits in
  the changed-pair list above) that the sequence alignment put in the
  added/removed buckets rather than in the changed-pair bucket. The two that are
  genuinely asymmetric are the ones the field explains: a two-byte `xchg %ax,%ax`
  PADDING nop dropped, and a `movq $0x0,0x3d0(%rsp)` ADDED — an eight-byte zero
  store, i.e. the new member being zero-initialised in the probe. Every one of
  them is inside the probe window; no call, no branch, and no other symbol moves.
  The Debug arm shows the same event without the scheduling noise (below).
* **Debug** — 2 469 symbols identical, 2 differ, and Debug says it outright
  because it does not inline:
  * `SqpOptions::SqpOptions()` — emitted in THIS object because of the probe —
    gains exactly one instruction, `movl $0x0,0xa8(%rdi)` (the new
    `elastic_rung_engine` member's initialiser), and every later member's offset
    shifts by 8 (`add $0x168,%rdi` → `add $0x170,%rdi`).
  * `IpqpEngine::solve` — same instruction count (8 960), **314 changed pairs,
    every one of them displacement-only** (`-0x208(%rbp)` → `-0x210(%rbp)` and
    so on), plus 18 instructions re-ordered within a block.

So the verdict for `ipqp_engine.cpp.o` is: EXPECTED, fully attributed to the
one field this task added, and it would differ at any path. The step-2b
report's "it does not include `sqp_solver_types.h` at all … the signature of
the BUILD-PATH confound" is corrected on both clauses.

## WHAT P-SYM'S EXIT STATUS SAYS, AND WHY IT IS NOT A GATE FAILURE

`scripts/psym_compare.sh` exits 1 on any difference outside its noise class, and
it did here in both configs. That is the expected status for a commit that ADDS
A PUBLIC FIELD: P-SYM's pass condition is "the code the compiler emits did not
change", which is the right bar for a relocation and the wrong bar for a feature.
The gate this task owes is the one above — every differing object named and
attributed — and it is discharged: 48 of 48, in two configs, with no object
differing that cannot be traced to the field or to a changed source file.
