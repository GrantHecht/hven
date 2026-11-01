# M7 W2 E2 round F8-1 (diagnosis) + fix round 9 (the standstill retreat) — the task record

> Base `bffd2ea2` (its sources equal fix 8's gated head `212886d5`) → head **`1f8d3118`**, one code
> commit (cherry-pick of worktree `8cfc12ed`, source-identical). Declaration **`f74c8872`**; gates
> **GREEN** (this record). Rulings S46–S48 in `.superpowers/m7-w2-e2-brief.md`.

## F8-1, the diagnosis (`REPORT-f81.md`, no commit)

ACOPR118's four (and ACOPP118's four) capped sub-QPs: **2 true cycles** (ACOPR118 qp3/qp5 — a
zero-motion drop, a stalled dependency test admitted at α = 0, an oscillating full step; qp5
revisits a state with period 334) and **6 standstills** (tiny full steps on one working set, the
iterate off its own equality rows so termination can never fire). Shared cause: working sets
EXACTLY rank-deficient (SVD ~1e-16), mostly entered by stall admits (59/92), and on the Schur-border
path step solves and dependency tests on those faces return impossible values (ρ up to 9.4). All
QPs feasible (scipy/HiGHS oracle). Fix 8's at-risk cells checked first: ACOPR57 optimal (+22 %
factorizations, a declared cost), READING6 identical.

## Fix round 9 (`REPORT-fix9.md`)

On the border path with `independent_working_set` on, a pass that stands still (zero motion, or a
second consecutive full step) off a structurally violated equality row while its inertia reads
kWrong takes the existing border retreat and arms the re-check; once per solve.
`QpCounters::standstill_retreats` (72 → 73). Seven capped-QP dumps are fixtures (three
Release-only). `T7Fix2…` 4 → 5 factorizations is a counter pin (status and x unchanged). Riders
R9-1/R9-2 from fix 8's review are in. Measured: ACOPR118 **optimal** on the lane build (21 majors,
f 129660.68370269175), 11 TRAIN cells identical, corpus 0/102 and 0/51, U0 unchanged, E2-off
twins identical. The root cause (F81-1, F81-2) is NOT fixed — registered as the next E2 item.

Fable review (`REVIEW-fable.md`): **CLOSED**, riders R10-1..3 to the next code commit.

## Gates (chain pid 13109, 2026-10-01, `gates/chain-e2f9.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**.

| gate | result |
|---|---|
| G1 Debug suite | 2890 / 2890 (skips asserted by name: baseline 4 + 3 Release-only) |
| G2 long test | 1 / 1 |
| G3 Release suite | 2891 / 2891 |
| G4 U0 replay vs the M6 pin | the declared **43**-difference set; values identical to the fix-8 gate replay |
| G5 P-SYM | Release and Debug DIFFERING **38** = class A 3 + class B 35, UNEXPLAINED **0** |
| G6 | install smoke, export contract, clang-format, `git diff --check` |

`wrappers-gates/` holds the declaration (runner copied from fix 8's, two fail-closed additions:
the by-name baseline pinned to the e2f8 `gates/` record; the skipped set asserted by name).
`gates/` holds the chain's outputs (logs renamed `.txt`; P-SYM object trees not copied).
