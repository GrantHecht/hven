# M6 W1 T10c comment-trim sidecar

Facts that lived ONLY in a trimmed test comment -- not in any `.superpowers/`
task report, the M6 ledger, the W1 spec, or the W1 implementation plan -- moved
here VERBATIM before the comment was compressed. Append-only; each agent owns
its own sections.

## tests/sqp/test_ipqp_dispatch.cpp:TheRoutingTableIsExercisedAndItsIdentitiesHoldOnEverySolve

> NOT COMPARED TO `f_star` HERE, deliberately: two HS members
> (25, 33) are ones the WALK itself does not drive to the published
> optimum from their shipped start point, so an f_star assertion in
> this test would be asserting something about the battery rather
> than about the routing.

(The trimmed comment keeps the rule -- the walk does not reach every published
optimum, so `f_star` would pin the battery -- but drops the identity of the two
members, which is recorded nowhere else.)

## tests/sqp/test_ipqp_engine.cpp:TheFactorizationCapIsCheckedBeforeEVERYFactorizationLadderRungsIncluded

> The cap used to be consulted once per iteration, so a
> single ladder could outrun it: measured at cap 1 on a strongly
> indefinite Hessian, the first ladder paid THREE factorizations before
> anything stopped it.

(`.superpowers/w1-t4-report.md` records I6 and this fixture, but not the
measured "three factorizations before anything stopped it".)

## tests/sqp/test_ipqp_engine.cpp:AnIndefiniteSubproblemArmsTheLadderAndWalksToTheBound

> Measured:
> `ipqp_pivot_reroute_primal == 1` under Debug and `== 0` under Release,
> with an IDENTICAL trajectory either way (29 iterations, 44
> factorizations), because the re-route rung simply replaces a rung the
> wrong-inertia path would have taken.

(`.superpowers/w1-t4b-report.md` records the Debug-1 / Release-0 measurement and
the 29 iterations; the factorization count 44 appears only here.)

## tests/sqp/test_ipqp_options.cpp:file banner

>   (1) IpqpOptions' fields are boundary-validated in validate_sqp_options,
>       one pin per predicate class, following test_problem_scaling.cpp's
>       ProblemScalingOptions.TheRuleIsValidatedAtTheBoundary pattern.

(The trimmed banner keeps the rule -- one pin per predicate class, validated at
every mode -- but drops the named precedent this file's arrangement follows.
`.superpowers/w1-t1-report.md` lists the predicates and their test names but
does not name `test_problem_scaling.cpp` as the pattern.)

## src/qp/ipqp_engine.cpp:file banner (the condensed bound algebra)

Verbatim from the T10c-trimmed banner. The step-by-step elimination appears in no
report, ledger entry, spec section or plan note; the spec states the Newton system
(§3.1) but not the bound elimination that produces `Sigma_b` and the mu-form RHS
identity the code relies on.

```
// THE CONDENSED BOUND ALGEBRA, likewise once. With dL = x - l, dU = u - x and
// per-index complementarity targets (tl, tu),
//
//     dzl_i = [ tl_i - dL_i zl_i - zl_i dx_i ] / dL_i
//     dzu_i = [ tu_i - dU_i zu_i + zu_i dx_i ] / dU_i
//
// so eliminating them from stationarity contributes
// Sigma_b_i = zl_i/dL_i + zu_i/dU_i to the (1,1) diagonal (which is exactly
// ipqp_accumulate_bound_sigma) and moves `tl/dL - tu/dU` to the right-hand
// side. For a UNIFORM target tl = tu = mu that right-hand-side term is
// precisely the NEGATIVE of ipqp_math.h's mu-form bound gradient, which is
// why the RHS below is assembled by calling that kernel rather than by a
// hand-written loop -- and why the AFFINE (mu = 0) right-hand side needs no
// special case: every term of the mu-form gradient carries a factor mu.
//
// ONE DEVIATION FROM "UNIFORM": the Mehrotra corrector's target is
// tl_i = sigma*mu - dx_aff_i * dzl_aff_i, tu_i = sigma*mu + dx_aff_i *
// dzu_aff_i. The uniform part goes through the kernel; the second-order
// correction is a separate, explicitly written term. That is not a duplicate
// of the kernel's loop -- it is a term the kernel does not model.
```

Also verbatim, the sign derivation the trimmed banner compresses to one rule:

```
// THE SLACK BLOCK'S UNKNOWN IS -ds, NOT ds, and that is forced by the matrix
// rather than chosen: the (iq, s) coupling is -I, so the inequality row reads
// `Ai dx - v_s - delta dyi`, while the linearized constraint
// `Ai x + s - bi - delta(yi - lambda_est) = 0` reads `Ai dx + ds - delta dyi`.
// Hence v_s = -ds.
```
