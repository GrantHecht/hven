# FIX ROUND 1's FIRST GATE CHAIN — REFUSED, and why

*Written BEFORE the repaired chain was started, so that the failed run is on
the record as a failed run and not as a draft of the one that followed.*

Chain `wrappers/chain_gates_m4fix1.sh`, detached with `setsid` as pid
`2911680` at 2026-09-22T09:21:24-04:00 against HEAD `90a31da4`, tree clean.
`CHAIN_END 09:52:34`. **`GATES_SUMMARY G1=1 G2=1 G3=1 G4=1 G5=0 G6=0`,
`FOLD_PROOFS_AGGREGATE_RC=3`.**

## The cause, which is one line of shell repeated six times

`gates-m4fix1.sh` is M4's `gates-m4.sh`, copied and re-pointed. Each gate
segment is written as a **single-quoted heredoc**, and every one of them opens
with its own `ROOT=...; S=$ROOT/.scratch/<round>; G=$S/gates`. The copy
re-pointed the OUTER `S=` and left the **six inner literals naming M4's scratch
tree**.

So the segments configured, built, ran and wrote **into the previous round's
tree**, while the outer asserts read **this round's**. `assert_ctest` and
`assert_replay` found nothing to read and failed — correctly, and for the right
reason: *"is missing or empty — no ctest summary to check"*. The fold-proof
segment then refused on a `gates-dbg/CTestTestfile.cmake` that had never been
created under the new path. G5 and G6 passed because their asserts run INSIDE
the same segment and were therefore self-consistent.

**This is the FOURTH instance of the error class this very round exists to
close** — a declaration or a path CARRIED from the previous task instead of
derived (M7 W2 M3a fix1, M3b fix1, M4's G5 classifier, and now M4 fix1's own
runner). It was committed in the act of repairing the third.

## What the failed run nevertheless measured

The segments ran at HEAD `90a31da4` from freshly `rm -rf`'d, freshly configured
trees, so their transcripts are real readings of this round's tree. They are
committed here, unedited, and every one of them met the declared number:

| | declared in `psym-derivation-m4fix1.txt` | measured (raw transcript here) |
|---|---|---|
| Debug suite | 2714 tests, 0 failed | `100% tests passed, 0 tests failed out of 2714` (`suite-debug.txt`) |
| the >600 s Debug test, alone | 1 test, 0 failed | `1/1 Test #1523: Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 675.70 sec` (`suite-debug-long.txt`) |
| Release suite | 2715 tests, 0 failed | `100% tests passed, 0 tests failed out of 2715` (`suite-release.txt`) |
| U0 replay | 27 cells × 75 columns × 3 engines, 0 differences | three lines, each `cells compared: 27  columns compared: 75  differences: 0` (`replay-comparison.txt`) |
| the 16 `QpAntiCyclingArm` fixtures | pass by name in both configurations | 16 `Passed` by name in each of `suite-debug.txt` and `suite-release.txt` |
| P-SYM + sections | DIFFERING 0; DEBUG_ONLY Release 0 / Debug 1 | measured and ASSERTED OK in `gates.txt` (G5 rc 0) |
| G6 | smoke, export contract align 32, clang-format, diff --check, DIRTY_FILES=0 | all green in `gates.txt` (G6 rc 0) |

**None of that makes this chain pass.** A gate is judged by whether it JUDGED,
and four of its six asserts had nothing to read. The chain is recorded FAILED
with the cause established, exactly as M4's G5 was.

## What happens next, and why it is not "re-running until it agrees"

The declaration is **not touched**: `psym-derivation-m4fix1.txt` was committed
at `90a31da4`, before this run, and governs the next one unchanged. What is
repaired is the RUNNER, and the repair is structural rather than textual:

* no segment names a path at all — they read `${GATES_SCRATCH:?}`, which the
  outer script exports and an unset value refuses;
* `check_segment` REFUSES any segment that names a scratch path again, and it
  runs before that segment's build does.

The second chain is recorded as a second chain, beside this one, with both
outcomes printed. The difference from "re-running to green" is that nothing
about what the gate WANTS moved between the two runs — only whether the runner
could hand its own asserts the files it had just written.
