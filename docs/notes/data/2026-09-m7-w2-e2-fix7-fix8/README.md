# M7 W2 E2 fix rounds 7 + 8 — the task record

> Base `2b4c9fa8` (its sources equal E4's gated head `e212a308`) → head **`212886d5`**. Two code
> commits on `m7`: **`b05f75e0`** (fix 7, cherry-pick of `86bd0962`) and **`212886d5`** (fix 8,
> cherry-pick of `35986d1f`), both source-identical to their worktree originals. Declaration
> **`fe94814d`**; gates **GREEN** (this record).

| round | commit | what it is |
|---|---|---|
| fix 7 | `b05f75e0` | **Rule A.** A dependency test that could not run (no usable factor for the pass's working set) is no longer read as a verdict: the integer screen decides alone, and a negative screen is an ordinary add (`DepTest::unrun`, `dep_unrun_tests`). Fixes the honesty of the E4 fix-5 root cause R1; trajectory-neutral on the measured cells. |
| fix 8 | `212886d5` | **Rule B, bounded.** The start-point repair may no longer be aborted by the integer screen; it is bounded instead by a MEASURED pin budget `max(8, ceil(n/2))` (largest in-cell success 118 pins at n = 344 = 0.343n; n/3 cut it; 1.46× headroom; 0 budget stops on 858 instrumented repairs). The probe budget removed as non-binding (probes ≤ 2·pins by construction). **F7-1**: a failed repair that took the pass's usable factor spends the pass. Fixture `QpE2Fix8Budget` pins the budget's declared cost; the two ACOPR118 dumps are fixtures. Option-off arm byte-identical. |

Movers (all attributed): HS 3340 → 3342 (HS15 cold, rule B; the Accelerate pin UNOBSERVED and
untouched); corpus +1 minor on three rows per algebra (F7-1); `AFullyPinnedRow…` minors 1 → 2
(F7-1). No status, objective or certificate re-pinned.

**The ACOPR118 cell still ends `numerical_error`** — four sub-QPs hit the minor cap and still do at
4× the cap, in both arms (rulings S43/S45: "stalls to any cap"; a cycle is inferred, not
measured). That is round F8-1. RE-TAKE 10 is deferred until F8-1 lands (S44).

Rulings S38–S45 live in `.superpowers/m7-w2-e2-brief.md`.

## Files

- `REPORT-fix7.md`, `REPORT-fix8.md`, `REVIEW-fable.md` — verbatim copies (one-line preamble each).
  The review: **CLOSED** with two test-side riders (R9-1, R9-2) carried to the next code commit.
- `wrappers-gates/` — the declaration: `gates-e2f8.sh`, `chain_gates_e2f8.sh` (copied from E4 r2,
  re-pointed, one fail-closed change: the by-name baseline narrowed to the current-run `gates/`
  record), `psym-derivation-e2f8.txt`, `preflight-e2f8.txt`, `rehearsal/`.
- `gates/` — the chain's outputs (logs renamed `.txt`; P-SYM object trees not copied).

## Gates (chain pid 3923995, 2026-09-30 23:44 → 2026-10-01 00:57; `gates/chain-e2f8.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**.

| gate | result |
|---|---|
| G1 Debug suite | 2881 / 2881 |
| G2 long test | 1 / 1 |
| G3 Release suite | 2882 / 2882 |
| G4 U0 replay vs the M6 pin | the declared **43**-difference set (walk 17 / ssn 13 / ipm 13), matched as a set; the replay identical to E4 run 2's |
| G5 P-SYM | Release and Debug DIFFERING **39** = class A 4 + class B 35, UNEXPLAINED **0**; 6 non-debug section movers both configs, DEBUG_ONLY 0 / 6 (the raw "P-SYM: FAIL" lines are the comparator's mover listing; the assertion is `G5_ASSERT_RC=0`) |
| G6 | install smoke PASSED, export contract, clang-format, `git diff --check` |
