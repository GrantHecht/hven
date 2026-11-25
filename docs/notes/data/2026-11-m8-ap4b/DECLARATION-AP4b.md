# M8 AP-4b (groups G3-G7: API friction fixes before the API lock) -- DECLARATION (committed before any leg; never edited afterwards -- a failure is DECLARATION-AP4b-r2.md)

Design: the AP-4 design page (friction dispositions 1-9, groups G3-G7), settled by the settler. Base arm A = origin/m8 0cecdc6d0.
Groups are separate commit series on branch ap4b, in order G3, G4, G5, G6, G7; each is independently revertible. Code heads are
recorded in RESULTS.txt beside this file (written after the legs; this file is not edited).

## Changes
- G3 (docs/comments only): cross-reference lines between `WorkCounters::hessian_evals` and `SqpCounters::eval_hess_calls`; the shared
  `SolveResult` measures versus the engine-specific `*Convergence`; the `Vec` brace note; a no-print-helpers note in examples/README.md;
  one appended erratum line on the AP-1 page (approved text unedited). Item 6 (no print helpers) is KEEP with that note only.
- G4: `template <class B> requires same_as<B, SolveBudget> solve(Problem&, ConstVecRef, const B&)` on IpmSolver and SqpSolver, forwarding
  to `solve(p, x0, WarmStart{}, budget)`. A bare `{}` still selects WarmStart. Test: tests/solver/test_solve_overloads.cpp. Example 09 uses it.
- G5: `dense_jacobian_nonzeros(m,n)`, `dense_lower_hessian_nonzeros(n)`, `dense_jacobian_structure(m,n,rows,cols)` (row-major),
  `dense_lower_hessian_structure(n,rows,cols)` (lower triangle, row by row) in model/nlp.h + src/model/nlp.cpp; std::invalid_argument on
  bad sizes. Examples 01/06/08/10 switch to them.
- G6: `IterationEvent::event_index` (0-based count of events delivered earlier in the solve; monotone across IPM phases and restoration
  and SQP depth-1 rows) and the matching `IterationSnapshot` member; `iteration` keeps its meaning.
- G7 (LAST; the one source break): `IterationEvent::violation` -> `row_violation` (also `IterationSnapshot`, the kConstraints doc,
  engines, tests, examples); the migration-0.2 G5 section shows before and after.

## Declared movers
- Corpus: NONE for G3, G4, G5, G7 (no numerics touched). G6 adds one store per delivered event, only on the event-building path (a callback
  installed); the unarmed solve builds no event. No counter, status or residual column can move; the replay is the check (0 differences,
  EXACT, x3), run unarmed.
- Public ABI/API: G4 adds an overload; G5 adds free functions and a member-order-neutral nothing; G6 adds a member (event/snapshot
  member order: appended after `iteration`'s block -- designated initialisers by name keep working; positional aggregate init of the
  event is not a supported spelling, the event is engine-built); G7 renames a member (break; consumers: examples, tests, tycho catch-up).
- Size pins that count bytes of IterationEvent/IterationSnapshot, if any, are re-derived with the group that moves them.
- Trace and options dump: unchanged (the trace reads the engines' own rows, not the event member names). Confirmed by ctest.

## Arms and binaries
Arm A = git archive of 0cecdc6d0 (legs/prebuild.sh), Release, build-ap4b/armA, corpus binary frozen at build-ap4b/armAbin. Arm B = the
group head's Release corpus binary, frozen at build-ap4b/armBbin. Trees build-ap4b/rel (Release) and build-ap4b/dbg (Debug), all targets,
clang++, SAFER_FAST. Every step under `~/.local/bin/box-lane -- ...` (counters only; no wall-clock is asserted or quoted anywhere).

## Proofs and pass criteria (per group head, G3 covered by G4's run because it is comment-only and is in G4's ancestry)
P1  ctest -j4 Release and Debug: 100% passed; the test count is base + the group's new TESTs (counts in RESULTS.txt).
P3  replay 27 cells x (walk, ssn, ipm): arm A vs arm B, scripts/compare_replay.py EXACT, "differences: 0" x3.
P5  scripts/check_install_smoke.sh on the default and clang front ends, each followed by scripts/check_export_contract.sh: pass x2.
    The export contract script asserts macros/properties of the exported target, not symbols, so it has no symbol list to update; this is
    recorded as such (no new exported macro or property in any group).
M   docs/migration-0.2.md: one subsection per user-visible change (G4, G5, G6, G7), each its own heading, separate from the options-registry
    sibling's sections.
Failure: any leg failing is reported, fixed, and re-run as a successor declaration; nothing here is relaxed after the fact.
