# FIX ROUND 1's SECOND GATE CHAIN — REFUSED, and why

*Written BEFORE the repaired runner was changed again, so that this run is on the
record as a refused run and not as a draft of the one that follows it.*

Chain `wrappers/chain_gates_m4fix1.sh`, detached with `setsid` as pid `2950851`
at 2026-09-22T09:56:14-04:00 against HEAD `2a148849`, tree clean.
`CHAIN_END 10:22:07`. **`GATES_M4FIX1_RC=2`, `CHAIN_SUMMARY gates=2 fold_proofs=0`.**

## The cause: the guard fired on a COMMENT

Run 1 was refused because six gate segments carried the previous round's scratch
path. The repair in `2a148849` made the segments read `${GATES_SCRATCH:?}` and
added `check_segment`, which refused any segment that *named a scratch path of
its own*. `check_segment` implemented that as one line:

```sh
hits=$(grep -n 'scratch' "$f")
```

`g5.sh` line 4 is a **comment**:

```
# The section digest lives beside the checker in the scratch copy, for the same
```

It contains the word `scratch`, so the guard refused the segment and the chain
stopped. **G5 and G6 never ran.** The refusal is on `gates.txt` line 64, and the
segment it judged is committed here verbatim as `g5-as-generated.sh` — its line 2
is `ROOT=/home/ghecht/Projects/hven; S=${GATES_SCRATCH:?}; G=$S/gates`, which is
correct. The segment was right; the check was wrong.

Two things went wrong at once, and they are worth separating:

1. **The test was textual where the property is a value.** "Where does this
   segment write?" is a question about the directory the segment resolves, not
   about which words appear in its prose. A `grep` for a word cannot tell a path
   from a sentence about a path.
2. **The test was negative where the property is positive.** `check_segment`
   could only say "I found nothing bad". A segment that named no path *and*
   resolved to the wrong tree — an unset `GATES_SCRATCH`, a typo in the export —
   would have passed it.

This was a known weakness before the run: it was recorded in
`.superpowers/m7-w2-m4arm-fix1-STATE.md` at 09:58 ("It is also a *grep for the
literal `scratch`*, not a positive assertion that the segment's output dir
equals `$S`") while the chain was still in its first leg. It is recorded here as
what it is — a guard that fired on the wrong evidence — and not as a surprise.

## What the refused run nevertheless measured

**These are raw readings taken by hand from the leg transcripts committed here.
They are NOT a gate result.** The chain did not judge; it refused after G4.

| | declared in `psym-derivation-m4fix1.txt` | read by hand from the leg transcripts; not a gate result |
|---|---|---|
| Debug suite | 2714 tests, 0 failed | `100% tests passed, 0 tests failed out of 2714` (`suite-debug.txt`) |
| the >600 s Debug test, alone | 1 test, 0 failed | `1/1 Test #1523: Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 673.78 sec` (`suite-debug-long.txt`) |
| Release suite | 2715 tests, 0 failed | `100% tests passed, 0 tests failed out of 2715` (`suite-release.txt`) |
| U0 replay | 27 cells × 75 columns × 3 engines, 0 differences | three lines, each `cells compared: 27  columns compared: 75  differences: 0` (`replay/comparison.txt`) |
| the 16 `QpAntiCyclingArm` fixtures | pass by name in both configurations | 16 `Passed` by name in each of `suite-debug.txt` and `suite-release.txt` |
| fold proofs | `FOLD_PROOFS_FAILED=0` | `FOLD_PROOFS_TOTAL=46  FOLD_PROOFS_FAILED=0` (`fold-proofs.txt`) |
| P-SYM + sections (G5) | DIFFERING 0; DEBUG_ONLY Release 0 / Debug 1 | **NOT RUN** — the segment was refused |
| G6 | smoke, export contract, clang-format, `DIRTY_FILES=0` | **NOT RUN** |

Unlike run 1, G1–G4 here were genuinely **asserted**, not merely executed:
`gates.txt` carries `ASSERT OK G1 Debug suite total = 2714`,
`ASSERT OK G2 long test total = 1`, `ASSERT OK G3 Release suite total = 2715`,
`ASSERT OK replay lines at 27/75/0 = 3`. The `S` repair from `2a148849` is
therefore *confirmed working* — `suite-debug-long.txt` names
`.scratch/m7w2m4fix1gates/gates-dbg`, this round's own tree, where run 1's named
M4's. The fold proofs ran for the same reason: the Debug tree existed under the
declared path this time, so the chain's fold-proof segment found its
`CTestTestfile.cmake` instead of refusing.

**None of that makes this chain pass.** Two of six gates never ran, so the chain
has no verdict to give, and the four numbers above are readings, not a gate.

## What is repaired next, and why it is not "re-running until it agrees"

The declaration is **not touched**: `gates-fix1/psym-derivation-m4fix1.txt` was
committed at `90a31da4`, before run 1, and governs run 3 unchanged. Nothing about
what the gate WANTS has moved across all three runs. What moves is the runner:

* the segment check becomes a **positive assertion on the value the segment
  actually uses** — the segment's own prologue is evaluated with `GATES_SCRATCH`
  set exactly as the segment will see it, and the resolved `$S` and `$G` must
  equal the declared tree and its `gates/` subdirectory;
* the residual textual check keeps only the part that is genuinely about paths —
  a literal `.scratch/` in the segment's **code** — and **strips comments before
  looking**, which is the defect above;
* **all six segments are written and checked in a pre-flight, before the first
  leg.** Run 2 spent 26 minutes on G1–G4 before discovering that G5 would be
  refused; there was no reason for that to cost a Debug suite and a 673 s test;
* `chain_gates_m4fix1.sh` stops naming the scratch tree at all. It asks the gate
  runner for it (`gates-m4fix1.sh --print-scratch`), so the round has **one
  literal** rather than two that happen to agree — the carried-path class one
  level up, which is what run 1 died of.

Run 3 is recorded as a third chain, beside these two, with all three outcomes
printed.
