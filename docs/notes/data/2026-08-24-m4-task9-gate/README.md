# Task 9 — the epoch gate: identity evidence and the defect it closes

Two arms, differing only in the Task 9 head batch: base `42091c6`, head the
batch's tip. The IPM wall leg beside this directory
(`../m4-ipm-wall-leg/runs/2026-08-24-task9-ipm*.log`) carries the TIMING; this
directory carries the two things a timing cannot say — that the gated and
ungated runs compute the same numbers, and that the defect the gate closes is
real in the base arm.

## The probe

`identity_probe.cpp`, compiled into BOTH arms from this directory's copy (only
the headers and `libhven.a` come from the arm), the same rule the wall-leg
scripts follow. Three problems through `NLPSolver` at the default objective
scale:

| tag | shape |
|---|---|
| `wide40` | dense-Jacobian quadratic constraints, n = 40 — the wall leg's own family |
| `hs071` | the canonical HS071: a box on every variable, one range row, one equality row |
| `boxed` | an equality-constrained box whose lower bounds are active at the solution |

Each problem is solved, then **the partition count is renegotiated and the
problem is solved again on the same instance** — the sequence the gate exists
for. Everything reported is printed in `%a`: the objective, the iteration
count, the flag, every primal, every equality / inequality / bound multiplier,
and every constraint residual.

Build (both arms), and the exact flags the wall leg's own arms use:

```
clang++ -O3 -DNDEBUG -std=c++20 -march=native -ffast-math -fno-finite-math-only \
        -I <arm>/include -isystem <arm>/dep/eigen -isystem <arm>/dep/fmt/include \
        identity_probe.cpp <arm-build>/libhven.a  <MKL LP64 static group>
```

Run: `MKL_NUM_THREADS=1 ./ident_<arm>`.

| arm | source | probe binary sha256 |
|---|---|---|
| base | `42091c6` | `3fbd5690277e1aeec6e67de3c7b15641edd2417acf07eae19d855747d3739bec` |
| head | Task 9 head batch | `021de56d6f3a941226edff495898f3f728509f695ec5b245baa273bb2859200b` |

## Result 1 — the gated and ungated first solves are byte-identical

`identity_base.txt` and `identity_head.txt` hold the two arms' output. Over the
first solve of all three problems — **149 lines of `%a` values**, covering the
objective, iterations, flag, primals, all three multiplier blocks and both
residual blocks — the two files are byte-identical.

```
grep -v '/re ' identity_base.txt > a; grep -v '/re ' identity_head.txt > b; diff a b
```

That is the substance of the pattern-guard half of the change: the guard reads
the assembled matrix and decides whether to throw. It feeds nothing into the
factorization, so skipping it cannot move a number — and here it does not, on
three problems, to the last bit.

## Result 2 — the defect is real in the base arm, on all three problems

The re-solve after the partition renegotiation is where the two arms part:

```
wide40/re THREW assemble: this provider's KKT location table has not been laid against any destination -- ...
hs071/re  THREW assemble: this provider's KKT location table has not been laid against any destination -- ...
boxed/re  THREW assemble: this provider's KKT location table has not been laid against any destination -- ...
```

against the head arm's

```
wide40/re flag=0 iters=8  obj=0x1.d55dbf94d73e6p-2
hs071/re  flag=0 iters=10 obj=0x1.10396a2dda8bp+4
boxed/re  flag=0 iters=22 obj=0x1.8p-1
```

The renegotiation re-lays, which resets the program's KKT location table to -1
and drops its analyzed-destination capture; it leaves treatment, relax factor
and bounds revision alone, so the treatment call the old gate read reports no
change and no re-analysis ran. **What the base arm hits first is the
destination sentinel at the assemble entry**, not the unchecked scatter — a
named `std::invalid_argument` escaping the solve rather than a wild write. The
raw sites (`fill_solver_coeffs`, `perturb_kkt_p_diags`, `perturb_kkt_c_diags`)
read the table without a `-1` guard and would be reached if a route ever
arrived there without a KKT-bearing `assemble()` first; on the routes this
engine takes, the sentinel is what fires.

Conditions: Fedora, AMD Ryzen 7 5800X3D 8C/16T, 31 GiB, Linux
7.1.9-200.fc44, `/usr/bin/clang++` 22.1.8, MKL LP64 static, Release,
`HVEN_FP_MODE=SAFER_FAST`, `MKL_NUM_THREADS=1`. This probe asserts only
deterministic per-process columns, so it is not a timing artifact and carries
no timing claim.
