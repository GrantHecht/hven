# M6 W1 T4b step 0 — the §3.2 gate's regularized residual at the frozen point

Informational evidence, recorded before any T4b code was written, as the T4b
plan of record (`docs/notes/2026-08-31-m6-w1-t4b-ladder-plan.md` §2, gate list
item 5 of the brief) requires. It confirms **mechanism 4** — the plan's claim
that the freeze is exact because the step's right-hand side and the §3.2 gate
are built from two DIFFERENT regularized problems — against a number rather
than against a derivation.

## Method

A scratch-only, environment-gated `fprintf` inside the §3.2 gate block of
`src/qp/ipqp_engine.cpp`, immediately after the gate's relative regularized
residual `R` is formed and BEFORE it is compared against `reg_gate_ref`. Built
in a detached `git worktree` at the T4b BASE commit `5307857`, Release,
`/usr/bin/clang++`, `HVEN_FP_MODE=SAFER_FAST`, with `-DHVEN_IPQP_DIAG` on the
scratch build only. `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`. The working tree was
never touched; the diagnostic exists in no commit and in no shipped
translation unit. The worktree's source was restored (`git checkout --`) and
verified clean immediately afterwards, and the scratch build tree deleted.

Driver: `IpqpA11Test.TheHSIndefiniteRowsArmTheGateAndClimbMonotonelyButNeverReachTheRead`,
which runs the three HS indefinite rows through `IpqpEngine::solve` at the
default `IpqpOptions`.

## The trace — `indefinite_equality_qp` (eig(H) = {1, −1, −2})

```
it=0  rho_sched=8     rho_floor=0   R=1                       ref=nan      res_worst=1
it=1  rho_sched=8     rho_floor=0   R=0.0044737764286153503   ref=1        res_worst=0.92366288952804043
it=2  rho_sched=0.8   rho_floor=80  R=1.6109054603875035      ref=4.47e-3  res_worst=1.6272229070008692
it=3  rho_sched=0.8   rho_floor=80  R=1.6081695041083337      ref=4.47e-3  res_worst=1.6244138678511737
it=4  rho_sched=0.8   rho_floor=80  R=1.6081558859001439      ref=4.47e-3  res_worst=1.6243998858726467
it=5  rho_sched=0.8   rho_floor=80  R=1.6081558181725271      ref=4.47e-3  res_worst=1.6243998163358859
...   (bit-identical through it=59)
it=59 rho_sched=0.8   rho_floor=80  R=1.6081558181725271      ref=4.47e-3  res_worst=1.6243998163358859
```

**THE STEP-0 CONSTANT: `R* = 1.6081558181725271`.**

`|x − zeta| = 0.024525680421265975` at the frozen point (so `zeta != x`, and
the T5 round-2 comment's aside that "at the fixed point `zeta == x`, so that
term is exactly 0" is wrong about THIS quantity — it is the STOPPING rule's
residual that carries no proximal term, not the gate's).

## The prediction, checked

The plan predicts `R* = 0.99 x S0`, from the closed form: at the frozen point
the step's RHS is zero, i.e. `grad Q + 80 (x − zeta) + bounds = 0`, so the
stopping residual is `S0 = |grad Q + bounds| = 80 |x − zeta|` while the gate's
regularized residual is `|grad Q + 0.8 (x − zeta) + bounds| = 79.2 |x − zeta|`
— a ratio of exactly `(rho_total − rho_sched) / rho_total = 79.2/80 = 0.99`.

| row | `rho_sched` | `rho_floor` | frozen `R*` | `res_worst` | `R*/res_worst` |
|---|---|---|---|---|---|
| `indefinite_equality_qp` | 0.8 | 80 | 1.6081558181725270 | 1.6243998163358859 | **0.99** |
| `indefinite_equality_and_row_qp` | 0.08 | 8 | 0.7409506785705873 | 0.7484350288591790 | **0.99** |
| `two_negative_eigenvalue_row_qp` | 0.8 | 80 | 0.4468100728288633 | 0.4513233058877407 | **0.99** |

All three rows sit at the predicted ratio to the digit, on three different
`(rho_sched, rho_floor)` pairs. Mechanism 4 is confirmed as stated.

## Why the gate cannot fire on it

The gate advances when `R <= max(kIpqpRegGateContract * reg_gate_ref,
min(opt_target, feas_target))`. At the frozen point `reg_gate_ref =
4.4737764286153503e-3` (captured at the last advance, iteration 1), so the
contraction clause asks for `R` of order `1e-3`, and the accuracy clause asks
for `R <= 1e-7`. The observed `R* = 1.608` misses both by more than three
orders of magnitude, and — because `x` does not move — it misses them by the
same amount forever. Codex's review of the plan reached the same two clauses
from the algebra; this is the measurement.
