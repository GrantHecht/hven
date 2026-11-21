# QP dump format ("D4QP 1")

`SqpAdvancedOptions::qp_dump_dir` with `qp_dump_policy = QpDumpPolicy::kFailed` or `kAll` writes one text
file per qualifying walk solve of an SQP solve: the major's QP, its SOC re-solve, its cap retry, the
certified fallback's walks and each elastic-ladder rung, the restoration phase's included. `kFailed`
writes the solves that end in any status but optimal. kSsn and kIpqp solves are not walk solves and
are not written.

## File names

```
qp_p<pid>_s<solver>_<seq>_d<depth>_m<major>_<site>.txt
```

- `pid`: the writing process's id.
- `solver`: a process-unique id of the `SqpSolver` (a restoration phase carries its parent's).
- `seq`: a process-wide sequence number, five digits zero-padded, in solve order.
- `depth`: 0 the primary driver, 1 the restoration sub-driver.
- `major`: the driver's major iteration (`-1` for a solve no site tagged).
- `site`: `main`, `soc`, `retry`, `fallback` (the certified fallback's cold walks), `rung<k>`
  (elastic-ladder rung `k`, 0 the first), or `qp` (untagged).

Each file is written under a temporary name (`<name>.tmp`) and hard-linked into place, so a reader
never sees a partial file. A file of the final name that already exists is never replaced: the
solve throws `std::runtime_error`. So does a directory that cannot be created or a file that cannot
be written.

## Contents

Text, one item per line, in this order:

```
D4QP 1
dims n me mi
infinite_bound v
opts primal_delta dual_mu feas_tol opt_tol max_iter schur_cap schur_cond_max ws_algebra tr_radius independent threads
overrides tr_radius primal_delta dual_mu
warm W seed S hot H k0_reused K
result status=S minors=M facts=F hkc=C maxiter=I
H rows cols nnz        then nnz lines "i j v"
g n                    then n lines
Ae rows cols nnz       then nnz lines "i j v"
be me                  then me lines
Ai rows cols nnz       then nnz lines "i j v"
bi mi                  then mi lines
lower n                then n lines
upper n                then n lines
seed_x n               then n lines        (only when S = 1)
seed_bound_state n     then n integers     (only when S = 1)
seed_ineq_active mi    then mi integers    (only when S = 1)
x_final n              then n lines
```

- Real numbers are written `%.17g` (`inf`, `-inf` and `nan` as such).
- `infinite_bound`: the QP's `infinite_bound`, at or beyond which a bound is absent.
- `opts`: the engine's `QpOptions` as constructed; `max_iter` is the cap the solve ran at;
  `ws_algebra` 0 = kRefactorize, 1 = kSchurBorder; `independent` 0/1.
- `overrides`: the solve's `QpSolveOverrides` (+inf or negative = use the options).
- `warm`/`seed`/`hot`: 0/1 flags for a warm solve, a seed given, a hot start offered; `k0_reused` the
  solve's counter.
- `result`: QpStatus `status` (0 optimal, 1 iteration limit, 2 infeasible, 3 numerical error),
  `minors` and `facts` (the solve's minor iterations and factorizations), `hkc`
  (`QpOptions::hessian_known_convex`, 0/1) and `maxiter` (the cap again).
- `H` is the stored upper triangle; `Ae`, `Ai` are row-major.
- `seed_bound_state`: 0 free, 1 at lower, 2 at upper, 3 fixed. `seed_ineq_active`: 0/1 per row.

The QP: minimize `g'x + x'Hx/2` subject to `Ae x = be`, `Ai x <= bi`, `lower <= x <= upper`, and,
when the effective trust-region radius is finite, `|x - x0|_inf <= tr` about
`x0 = clamp(seed_x or 0, lower, upper)`.

## Not recorded

Tokens of the scratch dumps in `tests/qp/fixtures/e2-d4/` that this writer does not write: the
`result` line's `blk_term`, `cert`, `cert_rej`, `zmcap`, `zmexh`, `retreats`, `exch`, `deferred`,
`undec`, `site` and `solve`, and the `final_bound_state` / `final_active_ineq` integer vectors. The
hot-start cache a hot solve read is not recorded, so a replay of an `H = 1` solve starts warm, not
hot. Older dumps have no `infinite_bound` line; a reader then uses the library default (1e20).
