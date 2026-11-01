# M7 W2 round C10 (the SNOPT-only failure census) + round P1 (the incremental Schur complement) — the task record

> P1: base `baddb728` (its sources equal fix 9's gated head `1f8d3118`) → head **`1f46e34d`**, one
> code commit (cherry-pick of worktree `904350e2`, same tree). Declaration **`96012fdc`**; gates
> **GREEN** (this record). Settler rulings C1–C4 at the end of `REPORT-c10.md`.

## C10, the census (`REPORT-c10.md`, no commit)

The 8 TRAIN cells SNOPT solves and hven-sqp did not at RE-TAKE 10 (ACOPP118, BRITGAS, READING8,
SAWPATH, STEENBRB, STEENBRD, STEENBRE, YORKNET), traced and profiled.
- **Common opening:** 6/8 start with a certified TR-infeasible QP at major 0 and enter restoration.
- **Every non-optimal sub-QP is feasible** (oracle), so every failure is a walk failure.
- **Mechanisms by cell count:**
  - the saddle dead end (NC3; a true saddle on STEENBRB/D/E): 4 cells;
  - R2, the driver answering with TR shrink only: 6 cells;
  - the restoration landing problem: 2 cells;
  - F81-1: ACOPP118.
- **R3 retired:** dependency tests are ~0.5 % of samples.
- **The wall cells' time is two efficiency defects:**
  - (a) `SchurComplement::rebuild_schur()` recomputing all of C per border add/drop (91 / 96 / 74 % of YORKNET / SAWPATH / BRITGAS samples);
  - (b) READING8's start repair re-running Pardiso symbolic analysis on each pin probe.

## P1 (`REPORT-p1.md`)

**(a) landed.** The assembled C is kept as a member:
- An add computes only its new row and column, by the old expression in the old order.
- A drop deletes a row and column.
- `sync_borders` factorizes C once per batch (`begin_batch`/`end_batch`). A throw replays per add, so it leaves the old exception and the old stack.

C and the factor input are bit-identical. The measured surfaces confirm it: corpus 0 rows, U0 block byte-identical, 13 TRAIN cells 0/132 columns, nothing re-pinned.

**(b) not implemented, correctly.** Each pin changes the eliminated KKT's dimension, so reusing the symbolic analysis needs a user permutation (an iparm change) or changed numerics.

Payoff (wall-clock informational, single serial runs):

| cell | before | after | status |
|---|---|---|---|
| YORKNET | > 580 s | 49 s | numerical_error at major 1 |
| SAWPATH | > 580 s | 25 s | numerical_error at major 1 |
| BRITGAS | 497 s | 106 s | numerical_error at major 1 |
| READING8 | > 580 s | > 580 s | unchanged, inside the start repair |

These cells FINISH; none converts. Elsewhere ACOPR118 runs 1.76× faster and the long test drops 626 s → 165 s.

Fable review (`REVIEW-fable.md`): **CLOSED**, riders R1–R3 (non-blocking).

## Gates (chain pid 314584, 2026-10-01, `gates/chain-p1.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**.

| gate | result |
|---|---|
| G1 Debug suite | 2893 / 2893 (skipped set = baseline by name) |
| G2 long test | 1 / 1 |
| G3 Release suite | 2894 / 2894 |
| G4 U0 replay vs the M6 pin | the declared **43**-difference set; exact-identical to the fix-9 gate replay |
| G5 P-SYM | DIFFERING **25** = class A 2 + class B 23 both configs, UNEXPLAINED **0**; the 94 `schur_complement.h` dependents fully classified |
| G6 | install smoke, export contract, clang-format, `git diff --check` |

`wrappers-gates/` holds the declaration. Its runner is copied from fix 9's, with one touch: an empty added-fixture list is no longer counted as one file.
